/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2018,  The University of Memphis
 *
 * This file is part of PSync.
 * See AUTHORS.md for complete list of PSync authors and contributors.
 *
 * PSync is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * PSync is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * PSync, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "partial-producer.hpp"
#include "detail/state.hpp"

#include <ndn-cxx/util/logger.hpp>

#include <iostream>
#include <cstring>
#include <limits>

namespace psync {

NDN_LOG_INIT(psync.PartialProducer);

PartialProducer::PartialProducer(size_t expectedNumEntries,
                                 ndn::Face& face,
                                 const ndn::Name& syncPrefix,
                                 const ndn::Name& userPrefix,
                                 ndn::time::milliseconds syncReplyFreshness,
                                 ndn::time::milliseconds helloReplyFreshness)
 : ProducerBase(expectedNumEntries, face, syncPrefix,
                userPrefix, syncReplyFreshness, helloReplyFreshness)
{
  m_registerPrefixId =
    m_face.registerPrefix(m_syncPrefix,
      [this] (const ndn::Name& syncPrefix) {
        m_face.setInterestFilter(ndn::Name(m_syncPrefix).append("hello"),
                                 std::bind(&PartialProducer::onHelloInterest, this, _1, _2));

        m_face.setInterestFilter(ndn::Name(m_syncPrefix).append("sync"),
                                 std::bind(&PartialProducer::onSyncInterest, this, _1, _2));
      },
      std::bind(&PartialProducer::onRegisterFailed, this, _1, _2));
}

PartialProducer::~PartialProducer()
{
  m_face.unregisterPrefix(m_registerPrefixId, nullptr, nullptr);
}

void
PartialProducer::publishName(const ndn::Name& prefix, ndn::optional<uint64_t> seq)
{
  if (m_prefixes.find(prefix) == m_prefixes.end()) {
    return;
  }

  uint64_t newSeq = seq.value_or(m_prefixes[prefix] + 1);

  NDN_LOG_INFO("Publish: " << prefix << "/" << newSeq);

  updateSeqNo(prefix, newSeq);

  satisfyPendingSyncInterests(prefix);
}

void
PartialProducer::onHelloInterest(const ndn::Name& prefix, const ndn::Interest& interest)
{
  NDN_LOG_DEBUG("Hello Interest Received, nonce: " << interest.getNonce());

  State state;

  for (const auto& p : m_prefixes) {
    state.addContent(p.first);
  }
  NDN_LOG_DEBUG("sending content p: " << state);

  ndn::Name helloDataName = prefix;
  m_iblt.appendToName(helloDataName);

  ndn::Data data;
  data.setName(helloDataName);
  data.setFreshnessPeriod(m_helloReplyFreshness);
  data.setContent(state.wireEncode());

  m_keyChain.sign(data);
  m_face.put(data);
}

void
PartialProducer::onSyncInterest(const ndn::Name& prefix, const ndn::Interest& interest)
{
  NDN_LOG_DEBUG("Sync Interest Received, nonce: " << interest.getNonce() <<
                " hash: " << std::hash<std::string>{}(interest.getName().toUri()));

  ndn::Name interestName = interest.getName();

  ndn::name::Component bfName, ibltName;
  unsigned int projectedCount;
  double falsePositiveProb;
  try {
    projectedCount = interestName.get(interestName.size()-4).toNumber();
    falsePositiveProb = interestName.get(interestName.size()-3).toNumber()/1000.;
    bfName = interestName.get(interestName.size()-2);

    ibltName = interestName.get(interestName.size()-1);
  }
  catch (const std::exception& e) {
    NDN_LOG_ERROR("Cannot extract bloom filter and IBF from sync interest: " << e.what());
    NDN_LOG_ERROR("Format: /<syncPrefix>/sync/<BF-count>/<BF-false-positive-probability>/<BF>/<IBF>");
    return;
  }

  BloomFilter bf;
  IBLT iblt(m_expectedNumEntries);

  try {
    bf = BloomFilter(projectedCount, falsePositiveProb, bfName);
    iblt.initialize(ibltName);
  }
  catch (const std::exception& e) {
    NDN_LOG_WARN(e.what());
    return;
  }

  // get the difference
  IBLT diff = m_iblt - iblt;

  // non-empty positive means we have some elements that the others don't
  std::set<uint32_t> positive;
  std::set<uint32_t> negative;

  NDN_LOG_TRACE("Number elements in IBF: " << m_prefixes.size());

  bool peel = diff.listEntries(positive, negative);

  NDN_LOG_TRACE("Result of listEntries on the difference: " << peel);

  if (!peel) {
    NDN_LOG_DEBUG("Can't decode the difference, sending application Nack");
    sendApplicationNack(interestName);
    return;
  }

  // generate content for Sync reply
  State state;
  NDN_LOG_TRACE("Size of positive set " << positive.size());
  NDN_LOG_TRACE("Size of negative set " << negative.size());
  for (const auto& hash : positive) {
    ndn::Name prefix = m_hash2prefix[hash];
    if (bf.contains(prefix.toUri())) {
      // generate data
      state.addContent(ndn::Name(prefix).appendNumber(m_prefixes[prefix]));
      NDN_LOG_DEBUG("Content: " << prefix << " " << std::to_string(m_prefixes[prefix]));
    }
  }

  NDN_LOG_TRACE("m_threshold: " << m_threshold << " Total: " << positive.size() + negative.size());

  if (positive.size() + negative.size() >= m_threshold || !state.getContent().empty()) {

    // send back data
    ndn::Name syncDataName = interestName;
    m_iblt.appendToName(syncDataName);
    ndn::Data data;
    data.setName(syncDataName);
    data.setFreshnessPeriod(m_syncReplyFreshness);
    data.setContent(state.wireEncode());

    m_keyChain.sign(data);
    NDN_LOG_DEBUG("Sending sync data");
    m_face.put(data);

    return;
  }

  ndn::util::scheduler::ScopedEventId scopedEventId(m_scheduler);
  auto it = m_pendingEntries.emplace(interestName,
                                     PendingEntryInfo{bf, iblt, std::move(scopedEventId)});

  it.first->second.expirationEvent =
    m_scheduler.scheduleEvent(interest.getInterestLifetime(),
                              [this, interest] {
                                NDN_LOG_TRACE("Erase Pending Interest " << interest.getNonce());
                                m_pendingEntries.erase(interest.getName());
                              });
}

void
PartialProducer::satisfyPendingSyncInterests(const ndn::Name& prefix) {
  NDN_LOG_TRACE("size of pending interest: " << m_pendingEntries.size());

  for (auto it = m_pendingEntries.begin(); it != m_pendingEntries.end();) {
    const PendingEntryInfo& entry = it->second;

    IBLT diff = m_iblt - entry.iblt;
    std::set<uint32_t> positive;
    std::set<uint32_t> negative;

    bool peel = diff.listEntries(positive, negative);

    NDN_LOG_TRACE("Result of listEntries on the difference: " << peel);

    NDN_LOG_TRACE("Number elements in IBF: " << m_prefixes.size());
    NDN_LOG_TRACE("m_threshold: " << m_threshold << " Total: " << positive.size() + negative.size());

    if (!peel) {
      NDN_LOG_TRACE("Decoding of differences with stored IBF unsuccessful, deleting pending interest");
      m_pendingEntries.erase(it++);
      continue;
    }

    State state;
    if (entry.bf.contains(prefix.toUri()) || positive.size() + negative.size() >= m_threshold) {
      if (entry.bf.contains(prefix.toUri())) {
         state.addContent(ndn::Name(prefix).appendNumber(m_prefixes[prefix]));
         NDN_LOG_DEBUG("sending sync content " << prefix << " " << std::to_string(m_prefixes[prefix]));
      }
      else {
        NDN_LOG_DEBUG("Sending with empty content to send latest IBF to consumer");
      }

      // generate sync data and cancel the event
      ndn::Name syncDataName = it->first;
      m_iblt.appendToName(syncDataName);
      ndn::Data data;
      data.setName(syncDataName);
      data.setFreshnessPeriod(m_syncReplyFreshness);
      data.setContent(state.wireEncode());

      m_keyChain.sign(data);
      NDN_LOG_DEBUG("Sending sync data");
      m_face.put(data);

      m_pendingEntries.erase(it++);
    }
    else {
      ++it;
    }
  }
}

} // namespace psync
