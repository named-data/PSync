/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2018,  The University of Memphis
 *
 * This file is part of NLSR (Named-data Link State Routing).
 * See AUTHORS.md for complete list of NLSR authors and contributors.
 *
 * NLSR is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NLSR is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NLSR, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "full-producer.hpp"

#include <ndn-cxx/util/logger.hpp>
#include <ndn-cxx/util/segment-fetcher.hpp>
#include <ndn-cxx/security/validator-null.hpp>

#include <iostream>
#include <cstring>
#include <limits>
#include <functional>

namespace psync {

NDN_LOG_INIT(psync.FullProducer);

FullProducer::FullProducer(const size_t expectedNumEntries,
                           ndn::Face& face,
                           const ndn::Name& syncPrefix,
                           const ndn::Name& userPrefix,
                           const UpdateCallback& onUpdateCallBack,
                           ndn::time::milliseconds syncInterestLifetime,
                           ndn::time::milliseconds syncReplyFreshness)
  : ProducerBase(expectedNumEntries, face, syncPrefix, userPrefix, syncReplyFreshness)
  , m_syncInterestLifetime(syncInterestLifetime)
  , m_onUpdate(onUpdateCallBack)
  , m_outstandingInterestId(nullptr)
  , m_scheduledSyncInterestId(m_scheduler)
{
  int jitter = m_syncInterestLifetime.count() * .20;
  m_jitter = std::uniform_int_distribution<>(-jitter, jitter);

  m_registerPrefixId =
    m_face.setInterestFilter(ndn::InterestFilter(m_syncPrefix).allowLoopback(false),
                             std::bind(&FullProducer::onInterest, this, _1, _2),
                             std::bind(&FullProducer::onRegisterFailed, this, _1, _2));

  // Should we do this after setInterestFilter success call back
  // (Currently following ChronoSync's way)
  sendSyncInterest();
}

FullProducer::~FullProducer()
{
  if (m_outstandingInterestId != nullptr) {
    m_face.removePendingInterest(m_outstandingInterestId);
  }

  m_face.unsetInterestFilter(m_registerPrefixId);
}

void
FullProducer::publishName(const ndn::Name& prefix, ndn::optional<uint64_t> seq)
{
  if (m_prefixes.find(prefix) == m_prefixes.end()) {
    NDN_LOG_WARN("Prefix not added: " << prefix);
    return;
  }

  uint64_t newSeq = seq.value_or(m_prefixes[prefix] + 1);

  NDN_LOG_INFO("Publish: "<< prefix << "/" << newSeq);

  updateSeqNo(prefix, newSeq);

  satisfyPendingInterests();
}

void
FullProducer::sendSyncInterest()
{
  // If we send two sync interest one after the other
  // since there is no new data in the network yet,
  // when data is available it may satisfy both of them
  if (m_outstandingInterestId != nullptr) {
    m_face.removePendingInterest(m_outstandingInterestId);
  }

  // Sync Interest format for full sync: /<sync-prefix>/<ourLatestIBF>
  ndn::Name syncInterestName = m_syncPrefix;

  // Append our latest IBF
  m_iblt.appendToName(syncInterestName);

  m_outstandingInterestName = syncInterestName;

  m_scheduledSyncInterestId =
    m_scheduler.scheduleEvent(m_syncInterestLifetime / 2 + ndn::time::milliseconds(m_jitter(m_rng)),
                              [this] { sendSyncInterest(); });

  ndn::Interest syncInterest(syncInterestName);
  syncInterest.setInterestLifetime(m_syncInterestLifetime);
  // Other side appends hash of IBF to sync data name
  syncInterest.setCanBePrefix(true);
  syncInterest.setMustBeFresh(true);

  syncInterest.setNonce(1);
  syncInterest.refreshNonce();

  m_outstandingInterestId = m_face.expressInterest(syncInterest,
                              std::bind(&FullProducer::onSyncData, this, _1, _2),
                              [] (const ndn::Interest& interest, const ndn::lp::Nack& nack) {
                                NDN_LOG_TRACE("received Nack with reason " << nack.getReason() <<
                                              " for Interest with Nonce: " << interest.getNonce());
                              },
                              [] (const ndn::Interest& interest) {
                                NDN_LOG_DEBUG("On full sync timeout " << interest.getNonce());
                              });

  NDN_LOG_DEBUG("sendFullSyncInterest, nonce: " << syncInterest.getNonce() <<
                ", hash: " << std::hash<ndn::Name>{}(syncInterestName));
}

void
FullProducer::onInterest(const ndn::Name& prefixName, const ndn::Interest& interest)
{
  ndn::Name nameWithoutSyncPrefix = interest.getName().getSubName(prefixName.size());
  if (nameWithoutSyncPrefix.size() == 2 &&
      nameWithoutSyncPrefix.get(nameWithoutSyncPrefix.size() - 1) == RECOVERY_PREFIX.get(0)) {
      onRecoveryInterest(interest);
  }
  else if (nameWithoutSyncPrefix.size() == 1) {
    onSyncInterest(interest);
  }
}

void
FullProducer::onSyncInterest(const ndn::Interest& interest)
{
  ndn::Name interestName = interest.getName();
  ndn::name::Component ibltName = interestName.get(interestName.size()-1);

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

    // Send nack if greater then threshold, else send positive below as usual
    // Or send if we can't get neither positive nor negative differences
    if (positive.size() + negative.size() >= m_threshold ||
        (positive.size() == 0 && negative.size() == 0)) {

      // If we don't have anything to offer means that
      // we are behind and should not mislead other nodes.
      bool haveSomethingToOffer = false;
      for (const auto& content : m_prefixes) {
        if (content.second != 0) {
          haveSomethingToOffer = true;
        }
      }

      if (haveSomethingToOffer) {
        sendApplicationNack(interestName);
      }
      return;
    }
  }

  State state;
  for (const auto& hash : positive) {
    ndn::Name prefix = m_hash2prefix[hash];
    // Don't sync up sequence number zero
    if (m_prefixes[prefix] != 0 && !isFutureHash(prefix.toUri(), negative)) {
      state.addContent(ndn::Name(prefix).appendNumber(m_prefixes[prefix]));
    }
  }

  if (!state.getContent().empty()) {
    NDN_LOG_DEBUG("Sending sync content: " << state);
    sendSyncData(interestName, state.wireEncode());
    return;
  }

  ndn::util::scheduler::ScopedEventId scopedEventId(m_scheduler);
  auto it = m_pendingEntries.emplace(interestName,
                                     PendingEntryInfoFull{iblt, std::move(scopedEventId)});

  it.first->second.expirationEvent =
    m_scheduler.scheduleEvent(interest.getInterestLifetime(),
                              [this, interest] {
                                NDN_LOG_TRACE("Erase Pending Interest " << interest.getNonce());
                                m_pendingEntries.erase(interest.getName());
                              });
}

void
FullProducer::onRecoveryInterest(const ndn::Interest& interest)
{
  NDN_LOG_DEBUG("Recovery interest received");

  State state;
  for (const auto& content : m_prefixes) {
    if (content.second != 0) {
      state.addContent(ndn::Name(content.first).appendNumber(content.second));
    }
  }

  // Send even if state is empty to let other side know that we are behind
  sendRecoveryData(interest.getName(), state);
}

void
FullProducer::sendSyncData(const ndn::Name& name, const ndn::Block& block)
{
  NDN_LOG_DEBUG("Checking if data will satisfy our own pending interest");

  ndn::Name nameWithIblt;
  m_iblt.appendToName(nameWithIblt);

  // Append hash of our IBF so that data name maybe different for each node answering
  ndn::Data data(ndn::Name(name).appendNumber(std::hash<ndn::Name>{}(nameWithIblt)));
  data.setFreshnessPeriod(m_syncReplyFreshness);
  data.setContent(block);
  m_keyChain.sign(data);

  // checking if our own interest got satisfied
  if (m_outstandingInterestName == name) {
    NDN_LOG_DEBUG("Satisfied our own pending interest");
    // remove outstanding interest
    if (m_outstandingInterestId != nullptr) {
      NDN_LOG_DEBUG("Removing our pending interest from face");
      m_face.removePendingInterest(m_outstandingInterestId);
      m_outstandingInterestId = nullptr;
      m_outstandingInterestName = ndn::Name("");
    }

    NDN_LOG_DEBUG("Sending Sync Data");

    // Send data after removing pending sync interest on face
    m_face.put(data);

    NDN_LOG_TRACE("Renewing sync interest");
    sendSyncInterest();
  }
  else {
    NDN_LOG_DEBUG("Sending Sync Data");
    m_face.put(data);
  }
}

void
FullProducer::onSyncData(const ndn::Interest& interest, const ndn::Data& data)
{
  ndn::Name interestName = interest.getName();
  deletePendingInterests(interest.getName());

  if (data.getContentType() == ndn::tlv::ContentType_Nack) {
    NDN_LOG_DEBUG("Got application nack, sending recovery interest");
    sendRecoveryInterest(interest);
    return;
  }

  State state(data.getContent());
  std::vector<MissingDataInfo> updates;

  if (interestName.get(interestName.size()-1) == RECOVERY_PREFIX.get(0) &&
      state.getContent().empty()) {
    NDN_LOG_TRACE("Recovery data is empty, other side is behind");
    return;
  }

  NDN_LOG_DEBUG("Sync Data Received:  " << state);

  for (const auto& content : state.getContent()) {
    ndn::Name prefix = content.getPrefix(-1);
    uint64_t seq = content.get(content.size()-1).toNumber();

    if (m_prefixes.find(prefix) == m_prefixes.end() || m_prefixes[prefix] < seq) {
      updates.push_back(MissingDataInfo{prefix, m_prefixes[prefix] + 1, seq});
      updateSeqNo(prefix, seq);
      // We should not call satisfyPendingSyncInterests here because we just
      // got data and deleted pending interest by calling deletePendingFullSyncInterests
      // But we might have interests not matching to this interest that might not have deleted
      // from pending sync interest
    }
  }

  // We just got the data, so send a new sync interest
  if (!updates.empty()) {
    m_onUpdate(updates);
    NDN_LOG_TRACE("Renewing sync interest");
    sendSyncInterest();
  }
  else {
    NDN_LOG_TRACE("No new update, interest nonce: " << interest.getNonce()  <<
                  " , hash: " << std::hash<ndn::Name>{}(interestName));
  }
}

void
FullProducer::satisfyPendingInterests()
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
      ndn::Name prefix = m_hash2prefix[hash];

      if (m_prefixes[prefix] != 0) {
        state.addContent(ndn::Name(prefix).appendNumber(m_prefixes[prefix]));
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

bool
FullProducer::isFutureHash(const ndn::Name& prefix, const std::set<uint32_t>& negative)
{
  uint32_t nextHash = murmurHash3(N_HASHCHECK,
                                  ndn::Name(prefix).appendNumber(m_prefixes[prefix] + 1).toUri());
  for (const auto& nHash : negative) {
    if (nHash == nextHash) {
      return true;
      break;
    }
  }
  return false;
}

void
FullProducer::deletePendingInterests(const ndn::Name& interestName) {
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

void
FullProducer::sendRecoveryData(const ndn::Name& prefix, const State& state)
{
  ndn::EncodingBuffer buffer;
  buffer.prependBlock(state.wireEncode());

  const uint8_t* rawBuffer = buffer.buf();
  const uint8_t* segmentBegin = rawBuffer;
  const uint8_t* end = rawBuffer + buffer.size();

  uint64_t segmentNo = 0;
  do {
    const uint8_t* segmentEnd = segmentBegin + (ndn::MAX_NDN_PACKET_SIZE >> 1);
    if (segmentEnd > end) {
      segmentEnd = end;
    }

    ndn::Name segmentName(prefix);
    segmentName.appendSegment(segmentNo);

    std::shared_ptr<ndn::Data> data = std::make_shared<ndn::Data>(segmentName);
    data->setContent(segmentBegin, segmentEnd - segmentBegin);
    data->setFreshnessPeriod(m_syncReplyFreshness);

    segmentBegin = segmentEnd;
    if (segmentBegin >= end) {
      data->setFinalBlock(segmentName[-1]);
    }

    m_keyChain.sign(*data);
    m_face.put(*data);

    NDN_LOG_DEBUG("Sending recovery data, seq: " << segmentNo);

    ++segmentNo;
  } while (segmentBegin < end);
}

void
FullProducer::sendRecoveryInterest(const ndn::Interest& interest)
{
  if (m_outstandingInterestId != nullptr) {
    m_face.removePendingInterest(m_outstandingInterestId);
    m_outstandingInterestId = nullptr;
  }

  ndn::Name ibltName;
  m_iblt.appendToName(ibltName);

  ndn::Name recoveryInterestName(m_syncPrefix);
  recoveryInterestName.appendNumber(std::hash<ndn::Name>{}(ibltName));
  recoveryInterestName.append(RECOVERY_PREFIX);

  ndn::Interest recoveryInterest(recoveryInterestName);
  recoveryInterest.setInterestLifetime(m_syncInterestLifetime);

  auto fetcher = ndn::util::SegmentFetcher::start(m_face,
                                                  recoveryInterest,
                                                  ndn::security::v2::getAcceptAllValidator());

  fetcher->onComplete.connect([this, recoveryInterest] (ndn::ConstBufferPtr bufferPtr) {
                                NDN_LOG_TRACE("Segment fetcher got data");
                                ndn::Data data;
                                data.setContent(std::move(bufferPtr));
                                onSyncData(recoveryInterest, data);
                              });

  fetcher->onError.connect([] (uint32_t errorCode, const std::string& msg) {
                             NDN_LOG_ERROR("Cannot recover, error: " << errorCode <<
                                           " message: " << msg);
                           });
}

} // namespace psync