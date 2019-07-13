/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2019,  The University of Memphis
 *
 * This file is part of PSync.
 * See AUTHORS.md for complete list of PSync authors and contributors.
 *
 * PSync is free software: you can redistribute it and/or modify it under the terms
 * of the GNU Lesser General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * PSync is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along with
 * PSync, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "PSync/partial-producer.hpp"
#include "PSync/detail/state.hpp"

#include <ndn-cxx/util/logger.hpp>

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
 : ProducerBase(expectedNumEntries, syncPrefix, syncReplyFreshness)
 , m_face(face)
 , m_scheduler(m_face.getIoService())
 , m_segmentPublisher(m_face, m_keyChain)
 , m_helloReplyFreshness(helloReplyFreshness)
{
  addUserNode(userPrefix);

  m_registeredPrefix = m_face.registerPrefix(m_syncPrefix,
    [this] (const ndn::Name& syncPrefix) {
      m_face.setInterestFilter(ndn::Name(m_syncPrefix).append("hello"),
                               std::bind(&PartialProducer::onHelloInterest, this, _1, _2));
      m_face.setInterestFilter(ndn::Name(m_syncPrefix).append("sync"),
                               std::bind(&PartialProducer::onSyncInterest, this, _1, _2));
    },
    std::bind(&PartialProducer::onRegisterFailed, this, _1, _2));
}

void
PartialProducer::publishName(const ndn::Name& prefix, ndn::optional<uint64_t> seq)
{
  if (!m_prefixes.isUserNode(prefix)) {
    return;
  }

  uint64_t newSeq = seq.value_or(m_prefixes.prefixes[prefix] + 1);

  NDN_LOG_INFO("Publish: " << prefix << "/" << newSeq);

  updateSeqNo(prefix, newSeq);

  satisfyPendingSyncInterests(prefix);
}

void
PartialProducer::updateSeqNo(const ndn::Name& prefix, uint64_t seq)
{
  uint64_t oldSeq;
  if (!m_prefixes.updateSeqNo(prefix, seq, oldSeq))
    return;  // Delete the last sequence prefix from the iblt
  // Because we don't insert zeroth prefix in IBF so no need to delete that
  if (oldSeq != 0) {
    removeFromIBF(ndn::Name(prefix).appendNumber(oldSeq));
  }  // Insert the new seq no
  ndn::Name prefixWithSeq = ndn::Name(prefix).appendNumber(seq);
  insertToIBF(prefixWithSeq);
}

void
PartialProducer::onHelloInterest(const ndn::Name& prefix, const ndn::Interest& interest)
{
  ndn::Name interestName = interest.getName();
  if (m_segmentPublisher.replyFromStore(interestName)) {
    return;
  }

  // Last component or fourth last component (in case of interest with version and segment)
  // needs to be hello
  if (interestName.get(-1).toUri() != "hello" &&
      interestName.get(-4).toUri() != "hello") {
    return;
  }

  NDN_LOG_DEBUG("Hello Interest Received, nonce: " << interest);

  State state;

  for (const auto& prefix : m_prefixes.prefixes) {
    state.addContent(ndn::Name(prefix.first).appendNumber(prefix.second));
  }
  NDN_LOG_DEBUG("sending content p: " << state);

  ndn::Name helloDataName = prefix;
  m_iblt.appendToName(helloDataName);

  m_segmentPublisher.publish(interestName, helloDataName,
                             state.wireEncode(), m_helloReplyFreshness);
}

void
PartialProducer::onSyncInterest(const ndn::Name& prefix, const ndn::Interest& interest)
{
  if (m_segmentPublisher.replyFromStore(interest.getName())) {
    return;
  }

  NDN_LOG_DEBUG("Sync Interest Received, nonce: " << interest.getNonce() <<
                " hash: " << std::hash<std::string>{}(interest.getName().toUri()));

  ndn::Name nameWithoutSyncPrefix = interest.getName().getSubName(prefix.size());
  ndn::Name interestName;

  if (nameWithoutSyncPrefix.size() == 4) {
    // Get /<prefix>/BF/IBF/ from /<prefix>/BF/IBF (3 components of BF + 1 for IBF)
    interestName = interest.getName();
  }
  else if (nameWithoutSyncPrefix.size() == 6) {
    // Get <prefix>/BF/IBF/ from /<prefix>/BF/IBF/<version>/<segment-no>
    interestName = interest.getName().getPrefix(-2);
  }
  else {
    return;
  }

  ndn::name::Component bfName, ibltName;
  unsigned int projectedCount;
  double falsePositiveProb;
  try {
    projectedCount = interestName.get(-4).toNumber();
    falsePositiveProb = interestName.get(-3).toNumber()/1000.;
    bfName = interestName.get(-2);

    ibltName = interestName.get(-1);
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

  NDN_LOG_TRACE("Number elements in IBF: " << m_prefixes.prefixes.size());

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
    ndn::Name name = m_hash2name[hash];
    ndn::Name prefix = name.getPrefix(-1);

    if (bf.contains(prefix.toUri())) {
      // generate data
      state.addContent(name);
      NDN_LOG_DEBUG("Content: " << prefix << " " << std::to_string(m_prefixes.prefixes[prefix]));
    }
  }

  NDN_LOG_TRACE("m_threshold: " << m_threshold << " Total: " << positive.size() + negative.size());

  if (positive.size() + negative.size() >= m_threshold || !state.getContent().empty()) {

    // send back data
    ndn::Name syncDataName = interestName;
    m_iblt.appendToName(syncDataName);

    m_segmentPublisher.publish(interest.getName(), syncDataName,
                               state.wireEncode(), m_syncReplyFreshness);
    return;
  }

  auto& entry = m_pendingEntries.emplace(interestName, PendingEntryInfo{bf, iblt, {}}).first->second;
  entry.expirationEvent = m_scheduler.schedule(interest.getInterestLifetime(),
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

    NDN_LOG_TRACE("Number elements in IBF: " << m_prefixes.prefixes.size());
    NDN_LOG_TRACE("m_threshold: " << m_threshold << " Total: " << positive.size() + negative.size());

    if (!peel) {
      NDN_LOG_TRACE("Decoding of differences with stored IBF unsuccessful, deleting pending interest");
      m_pendingEntries.erase(it++);
      continue;
    }

    State state;
    if (entry.bf.contains(prefix.toUri()) || positive.size() + negative.size() >= m_threshold) {
      if (entry.bf.contains(prefix.toUri())) {
        state.addContent(ndn::Name(prefix).appendNumber(m_prefixes.prefixes[prefix]));
        NDN_LOG_DEBUG("sending sync content " << prefix << " " << std::to_string(m_prefixes.prefixes[prefix]));
      }
      else {
        NDN_LOG_DEBUG("Sending with empty content to send latest IBF to consumer");
      }

      // generate sync data and cancel the event
      ndn::Name syncDataName = it->first;
      m_iblt.appendToName(syncDataName);

      m_segmentPublisher.publish(it->first, syncDataName,
                                 state.wireEncode(), m_syncReplyFreshness);

      m_pendingEntries.erase(it++);
    }
    else {
      ++it;
    }
  }
}

void
PartialProducer::sendApplicationNack(const ndn::Name& name)
{
  NDN_LOG_DEBUG("Sending application nack");
  ndn::Name dataName(name);
  m_iblt.appendToName(dataName);

  dataName.appendSegment(0);
  ndn::Data data(dataName);
  data.setFreshnessPeriod(m_syncReplyFreshness);
  data.setContentType(ndn::tlv::ContentType_Nack);
  data.setFinalBlock(dataName[-1]);
  m_keyChain.sign(data);
  m_face.put(data);
}

} // namespace psync
