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

#include "PSync/full-producer-arbitrary.hpp"

#include <ndn-cxx/util/logger.hpp>
#include <ndn-cxx/util/segment-fetcher.hpp>
#include <ndn-cxx/security/validator-null.hpp>

#include <cstring>
#include <limits>
#include <functional>

namespace psync {

NDN_LOG_INIT(psync.FullProducerArbitrary);

FullProducerArbitrary::FullProducerArbitrary(const size_t expectedNumEntries,
                           ndn::Face& face,
                           const ndn::Name& syncPrefix,
                           const ArbitraryUpdateCallback& onArbitraryUpdateCallback,
                           ndn::time::milliseconds syncInterestLifetime,
                           ndn::time::milliseconds syncReplyFreshness,
                           const ShouldAddToSyncDataCallback& onShouldAddToSyncDataCallback,
                           const CanAddName& onCanAddName)
  : ProducerBase(expectedNumEntries, syncPrefix, syncReplyFreshness)
  , m_face(face)
  , m_scheduler(m_face.getIoService())
  , m_segmentPublisher(m_face, m_keyChain)
  , m_syncInterestLifetime(syncInterestLifetime)
  , m_onArbitraryUpdateCallback(onArbitraryUpdateCallback)
  , m_onShouldAddToSyncDataCallback(onShouldAddToSyncDataCallback)
  , m_onCanAddName(onCanAddName)
{
  int jitter = m_syncInterestLifetime.count() * .20;
  m_jitter = std::uniform_int_distribution<>(-jitter, jitter);

  m_registeredPrefix = m_face.setInterestFilter(
                         ndn::InterestFilter(m_syncPrefix).allowLoopback(false),
                         std::bind(&FullProducerArbitrary::onSyncInterest, this, _1, _2),
                         std::bind(&FullProducerArbitrary::onRegisterFailed, this, _1, _2));

  // Should we do this after setInterestFilter success call back
  // (Currently following ChronoSync's way)
  sendSyncInterest();
}

FullProducerArbitrary::~FullProducerArbitrary()
{
  if (m_fetcher) {
    m_fetcher->stop();
  }
}

void
FullProducerArbitrary::publishName(const ndn::Name& name)
{
  if (m_name2hash.find(name) != m_name2hash.end()) {
    NDN_LOG_DEBUG("Already published, ignoring: " << name);
    return;
  }

  NDN_LOG_INFO("Publish: " << name);

  insertToIBF(name);

  satisfyPendingInterests();
}

void
FullProducerArbitrary::sendSyncInterest()
{
  // If we send two sync interest one after the other
  // since there is no new data in the network yet,
  // when data is available it may satisfy both of them
  if (m_fetcher) {
    m_fetcher->stop();
  }

  // Sync Interest format for full sync: /<sync-prefix>/<ourLatestIBF>
  ndn::Name syncInterestName = m_syncPrefix;

  // Append our latest IBF
  m_iblt.appendToName(syncInterestName);

  m_outstandingInterestName = syncInterestName;

  m_scheduledSyncInterestId =
    m_scheduler.schedule(m_syncInterestLifetime / 2 + ndn::time::milliseconds(m_jitter(m_rng)),
                         [this] { sendSyncInterest(); });

  ndn::Interest syncInterest(syncInterestName);

  ndn::util::SegmentFetcher::Options options;
  options.interestLifetime = m_syncInterestLifetime;
  options.maxTimeout = m_syncInterestLifetime;

  m_fetcher = ndn::util::SegmentFetcher::start(m_face,
                                               syncInterest,
                                               ndn::security::v2::getAcceptAllValidator(),
                                               options);

  m_fetcher->onComplete.connect([this, syncInterest] (ndn::ConstBufferPtr bufferPtr) {
                                  onSyncData(syncInterest, bufferPtr);
                                });

  m_fetcher->onError.connect([] (uint32_t errorCode, const std::string& msg) {
                               NDN_LOG_ERROR("Cannot fetch sync data, error: " <<
                                              errorCode << " message: " << msg);
                             });

  NDN_LOG_DEBUG("sendFullSyncInterest, nonce: " << syncInterest.getNonce() <<
                ", hash: " << std::hash<ndn::Name>{}(syncInterestName));
}

void
FullProducerArbitrary::onSyncInterest(const ndn::Name& prefixName, const ndn::Interest& interest)
{
  if (m_segmentPublisher.replyFromStore(interest.getName())) {
    return;
  }

  ndn::Name nameWithoutSyncPrefix = interest.getName().getSubName(prefixName.size());
  ndn::Name interestName;

  if (nameWithoutSyncPrefix.size() == 1) {
    // Get /<prefix>/IBF from /<prefix>/IBF
    interestName = interest.getName();
  }
  else if (nameWithoutSyncPrefix.size() == 3) {
    // Get /<prefix>/IBF from /<prefix>/IBF/<version>/<segment-no>
    interestName = interest.getName().getPrefix(-2);
  }
  else {
    return;
  }

  ndn::name::Component ibltName = interestName.get(-1);

  NDN_LOG_DEBUG("Full Sync Interest Received, nonce: " << interest.getNonce() <<
                ", hash: " << std::hash<ndn::Name>{}(interestName));

  IBLT iblt(m_expectedNumEntries);
  try {
    iblt.initialize(ibltName);
  }
  catch (const std::exception& e) {
    NDN_LOG_WARN(e.what());
    return;
  }

  IBLT diff = m_iblt - iblt;

  std::set<uint32_t> positive;
  std::set<uint32_t> negative;

  if (!diff.listEntries(positive, negative)) {
    NDN_LOG_TRACE("Cannot decode differences, positive: " << positive.size()
                   << " negative: " << negative.size() << " m_threshold: "
                   << m_threshold);

    // Send all data if greater then threshold, else send positive below as usual
    // Or send if we can't get neither positive nor negative differences
    if (positive.size() + negative.size() >= m_threshold ||
        (positive.size() == 0 && negative.size() == 0)) {
      State state;
      for (const auto& it : m_name2hash) {
        state.addContent(it.first);
      }

      if (!state.getContent().empty()) {
        m_segmentPublisher.publish(interest.getName(), interest.getName(),
                                   state.wireEncode(), m_syncReplyFreshness);
      }

      return;
    }
  }

  State state;
  for (const auto& hash : positive) {
    ndn::Name name = m_hash2name[hash];
    ndn::Name prefix = name.getPrefix(-1);

    if (m_name2hash.find(name) != m_name2hash.end()) {
      if (!m_onShouldAddToSyncDataCallback ||
          m_onShouldAddToSyncDataCallback(prefix.toUri(), negative)) {
        state.addContent(name);
      }
    }
  }

  if (!state.getContent().empty()) {
    NDN_LOG_DEBUG("Sending sync content: " << state);
    sendSyncData(interestName, state.wireEncode());
    return;
  }
  else {
    NDN_LOG_WARN("State is empty");
  }

  auto& entry = m_pendingEntries.emplace(interestName, PendingEntryInfoFull{iblt, {}}).first->second;
  entry.expirationEvent = m_scheduler.schedule(interest.getInterestLifetime(),
                          [this, interest] {
                            NDN_LOG_TRACE("Erase Pending Interest " << interest.getNonce());
                            m_pendingEntries.erase(interest.getName());
                          });
}

void
FullProducerArbitrary::sendSyncData(const ndn::Name& name, const ndn::Block& block)
{
  NDN_LOG_DEBUG("Checking if data will satisfy our own pending interest");

  ndn::Name nameWithIblt;
  m_iblt.appendToName(nameWithIblt);

  // Append hash of our IBF so that data name maybe different for each node answering
  ndn::Name dataName(ndn::Name(name).appendNumber(std::hash<ndn::Name>{}(nameWithIblt)));

  // checking if our own interest got satisfied
  if (m_outstandingInterestName == name) {
    NDN_LOG_DEBUG("Satisfied our own pending interest");
    // remove outstanding interest
    if (m_fetcher) {
      NDN_LOG_DEBUG("Removing our pending interest from face (stop fetcher)");
      m_fetcher->stop();
      m_outstandingInterestName = ndn::Name("");
    }

    NDN_LOG_DEBUG("Sending Sync Data");

    // Send data after removing pending sync interest on face
    m_segmentPublisher.publish(name, dataName, block, m_syncReplyFreshness);

    NDN_LOG_TRACE("Renewing sync interest");
    sendSyncInterest();
  }
  else {
    NDN_LOG_DEBUG("Sending Sync Data");
    m_segmentPublisher.publish(name, dataName, block, m_syncReplyFreshness);
  }
}

void
FullProducerArbitrary::onSyncData(const ndn::Interest& interest, const ndn::ConstBufferPtr& bufferPtr)
{
  deletePendingInterests(interest.getName());

  State state(ndn::Block(std::move(bufferPtr)));
  std::vector<ndn::Name> updates;

  NDN_LOG_DEBUG("Sync Data Received:  " << state);

  for (const auto& name : state.getContent()) {
    if (m_name2hash.find(name) == m_name2hash.end()) {
      NDN_LOG_DEBUG("Checking whether to add");
      if (!m_onCanAddName || m_onCanAddName(name)) {
        NDN_LOG_DEBUG("Adding...");
        updates.push_back(name);
        insertToIBF(name);
      }
      // We should not call satisfyPendingSyncInterests here because we just
      // got data and deleted pending interest by calling deletePendingFullSyncInterests
      // But we might have interests not matching to this interest that might not have deleted
      // from pending sync interest
    }
  }

  // We just got the data, so send a new sync interest
  if (!updates.empty()) {
    m_onArbitraryUpdateCallback(updates);
    NDN_LOG_TRACE("Renewing sync interest");
    sendSyncInterest();
  }
  else {
    NDN_LOG_TRACE("No new update, interest nonce: " << interest.getNonce() <<
                  " , hash: " << std::hash<ndn::Name>{}(interest.getName()));
  }
}

void
FullProducerArbitrary::satisfyPendingInterests()
{
  NDN_LOG_DEBUG("Satisfying full sync interest: " << m_pendingEntries.size());

  for (auto it = m_pendingEntries.begin(); it != m_pendingEntries.end();) {
    const PendingEntryInfoFull& entry = it->second;
    IBLT diff = m_iblt - entry.iblt;
    std::set<uint32_t> positive;
    std::set<uint32_t> negative;

    if (!diff.listEntries(positive, negative)) {
      NDN_LOG_TRACE("Decode failed for pending interest");
      if (positive.size() + negative.size() >= m_threshold ||
          (positive.size() == 0 && negative.size() == 0)) {
        NDN_LOG_TRACE("pos + neg > threshold or no diff can be found, erase pending interest");
        m_pendingEntries.erase(it++);
        continue;
      }
    }

    State state;
    for (const auto& hash : positive) {
      ndn::Name name = m_hash2name[hash];

      if (m_name2hash.find(name) != m_name2hash.end()) {
        state.addContent(name);
      }
    }

    if (!state.getContent().empty()) {
      NDN_LOG_DEBUG("Satisfying sync content: " << state);
      sendSyncData(it->first, state.wireEncode());
      m_pendingEntries.erase(it++);
    }
    else {
      ++it;
    }
  }
}

void
FullProducerArbitrary::deletePendingInterests(const ndn::Name& interestName) {
  for (auto it = m_pendingEntries.begin(); it != m_pendingEntries.end();) {
    if (it->first == interestName) {
      NDN_LOG_TRACE("Delete pending interest: " << interestName);
      m_pendingEntries.erase(it++);
    }
    else {
      ++it;
    }
  }
}

} // namespace psync
