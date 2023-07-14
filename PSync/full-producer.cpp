/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2024,  The University of Memphis
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
 */

#include "PSync/full-producer.hpp"
#include "PSync/detail/state.hpp"
#include "PSync/detail/util.hpp"

#include <ndn-cxx/lp/tags.hpp>
#include <ndn-cxx/security/validator-null.hpp>
#include <ndn-cxx/util/logger.hpp>

#include <cstring>

namespace psync {

NDN_LOG_INIT(psync.FullProducer);

FullProducer::FullProducer(ndn::Face& face,
                           ndn::KeyChain& keyChain,
                           const ndn::Name& syncPrefix,
                           const Options& opts)
  : ProducerBase(face, keyChain, opts.ibfCount, syncPrefix, opts.syncDataFreshness,
                 opts.ibfCompression, opts.contentCompression)
  , m_syncInterestLifetime(opts.syncInterestLifetime)
  , m_onUpdate(opts.onUpdate)
{
  m_registeredPrefix = m_face.setInterestFilter(ndn::InterestFilter(m_syncPrefix).allowLoopback(false),
    [this] (auto&&... args) { onSyncInterest(std::forward<decltype(args)>(args)...); },
    [] (auto&&... args) { onRegisterFailed(std::forward<decltype(args)>(args)...); });

  // Should we do this after setInterestFilter success call back
  // (Currently following ChronoSync's way)
  sendSyncInterest();
}

FullProducer::FullProducer(ndn::Face& face,
                           ndn::KeyChain& keyChain,
                           size_t expectedNumEntries,
                           const ndn::Name& syncPrefix,
                           const ndn::Name& userPrefix,
                           UpdateCallback onUpdateCb,
                           ndn::time::milliseconds syncInterestLifetime,
                           ndn::time::milliseconds syncReplyFreshness,
                           CompressionScheme ibltCompression,
                           CompressionScheme contentCompression)
  : FullProducer(face, keyChain, syncPrefix,
                 Options{std::move(onUpdateCb), static_cast<uint32_t>(expectedNumEntries), ibltCompression,
                         syncInterestLifetime, syncReplyFreshness, contentCompression})
{
  addUserNode(userPrefix);
}

FullProducer::~FullProducer()
{
  if (m_fetcher) {
    m_fetcher->stop();
  }
}

void
FullProducer::publishName(const ndn::Name& prefix, std::optional<uint64_t> seq)
{
  if (m_prefixes.find(prefix) == m_prefixes.end()) {
    NDN_LOG_WARN("Prefix not added: " << prefix);
    return;
  }

  uint64_t newSeq = seq.value_or(m_prefixes[prefix] + 1);
  NDN_LOG_INFO("Publish: " << prefix << "/" << newSeq);
  updateSeqNo(prefix, newSeq);

  m_inNoNewDataWaitOutPeriod = false;

  satisfyPendingInterests(ndn::Name(prefix).appendNumber(newSeq));
}

void
FullProducer::sendSyncInterest()
{
  if (m_inNoNewDataWaitOutPeriod) {
    NDN_LOG_TRACE("Cannot send sync Interest as Data is expected from CS");
    return;
  }

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
  // Append cumulative updates that has been inserted into this IBF
  syncInterestName.appendNumber(m_numOwnElements);

  auto currentTime = ndn::time::system_clock::now();
  if ((currentTime - m_lastInterestSentTime < ndn::time::milliseconds(MIN_JITTER)) &&
      (m_outstandingInterestName == syncInterestName)) {
    NDN_LOG_TRACE("Suppressing Interest: " << std::hash<ndn::Name>{}(syncInterestName));
    return;
  }

  m_outstandingInterestName = syncInterestName;

  m_scheduledSyncInterestId =
    m_scheduler.schedule(m_syncInterestLifetime / 2 + ndn::time::milliseconds(m_jitter(m_rng)),
                         [this] { sendSyncInterest(); });

  ndn::Interest syncInterest(syncInterestName);

  using ndn::SegmentFetcher;
  SegmentFetcher::Options options;
  options.interestLifetime = m_syncInterestLifetime;
  options.maxTimeout = m_syncInterestLifetime;
  options.rttOptions.initialRto = m_syncInterestLifetime;

  // This log message must be before sending the Interest through SegmentFetcher
  // because getNonce generates a Nonce for this Interest.
  // SegmentFetcher makes a copy of this Interest, so if we print the Nonce
  // after, that Nonce will be different than the one seen in tshark!
  NDN_LOG_DEBUG("sendFullSyncInterest, nonce: " << syncInterest.getNonce() <<
                ", hash: " << std::hash<ndn::Name>{}(syncInterestName));

  m_lastInterestSentTime = currentTime;
  m_fetcher = SegmentFetcher::start(m_face, syncInterest,
                                    ndn::security::getAcceptAllValidator(), options);

  m_fetcher->onComplete.connect([this, syncInterest] (const ndn::ConstBufferPtr& bufferPtr) {
    onSyncData(syncInterest, bufferPtr);
  });

  m_fetcher->afterSegmentValidated.connect([this] (const ndn::Data& data) {
      auto tag = data.getTag<ndn::lp::IncomingFaceIdTag>();
      if (tag) {
        m_incomingFace = *tag;
      }
      else {
        m_incomingFace = 0;
      }
    });

  m_fetcher->onError.connect([this] (uint32_t errorCode, const std::string& msg) {
    NDN_LOG_ERROR("Cannot fetch sync data, error: " << errorCode << ", message: " << msg);
    // We would like to recover from errors like NoRoute NACK quicker than sync Interest timeout.
    // We don't react to Interest timeout here as we have scheduled the next sync Interest
    // to be sent in half the sync Interest lifetime + jitter above. So we would react to
    // timeout before it happens.
    if (errorCode != SegmentFetcher::ErrorCode::INTEREST_TIMEOUT) {
      auto after = ndn::time::milliseconds(m_jitter(m_rng));
      NDN_LOG_DEBUG("Schedule sync Interest after: " << after);
      m_scheduledSyncInterestId = m_scheduler.schedule(after, [this] { sendSyncInterest(); });
    }
  });
}

void
FullProducer::processWaitingInterests()
{
  NDN_LOG_TRACE("Processing waiting Interest list, size: " << m_waitingForProcessing.size());
  if (m_waitingForProcessing.size() == 0) {
    return;
  }

  for (auto it = m_waitingForProcessing.begin(); it != m_waitingForProcessing.end();) {
    if (it->second.numTries == std::numeric_limits<uint16_t>::max()) {
      NDN_LOG_TRACE("Interest with hash already marked for deletion, removing now: " <<
                     std::hash<ndn::Name>{}(it->first));
      it = m_waitingForProcessing.erase(it);
      continue;
    }

    it->second.numTries += 1;
    ndn::Interest interest(it->first);
    interest.setNonce(it->second.nonce);
    onSyncInterest(m_syncPrefix, interest, true);
    if (it->second.numTries == std::numeric_limits<uint16_t>::max()) {
      NDN_LOG_TRACE("Removing Interest with hash: " << std::hash<ndn::Name>{}(it->first));
      it = m_waitingForProcessing.erase(it);
    }
    else {
      ++it;
    }
  }
  NDN_LOG_TRACE("Done processing waiting Interest list, size: " << m_waitingForProcessing.size());
}

void
FullProducer::scheduleProcessWaitingInterests()
{
  // If nothing waiting, no need to schedule
  if (m_waitingForProcessing.size() == 0) {
    return;
  }

  if (!m_interestDelayTimerId) {
    auto after = ndn::time::milliseconds(m_jitter(m_rng));
    NDN_LOG_TRACE("Setting a timer to processes waiting Interest(s) in: " << after);

    m_interestDelayTimerId = m_scheduler.schedule(after, [=] {
      NDN_LOG_TRACE("Timer has expired, trying to process waiting Interest(s)");
      processWaitingInterests();
      scheduleProcessWaitingInterests();
    });
  }
}

void
FullProducer::onSyncInterest(const ndn::Name& prefixName, const ndn::Interest& interest,
                             bool isTimedProcessing)
{
  ndn::Name interestName = interest.getName();
  auto interestNameHash = std::hash<ndn::Name>{}(interestName);
  NDN_LOG_DEBUG("Full sync Interest received, nonce: " << interest.getNonce() <<
                ", hash: " << interestNameHash);

  if (isTimedProcessing) {
    NDN_LOG_TRACE("Delayed Interest being processed now");
  }

  if (m_segmentPublisher.replyFromStore(interestName)) {
    NDN_LOG_DEBUG("Answer from memory");
    return;
  }

  ndn::Name nameWithoutSyncPrefix = interestName.getSubName(prefixName.size());

  if (nameWithoutSyncPrefix.size() == 4) {
    // /<IBF>/<numCumulativeElements>/<version>/<segment>
    NDN_LOG_DEBUG("Segment not found in memory. Other side will have to restart");
    // This should have been answered from publisher Cache!
    sendApplicationNack(prefixName);
    return;
  }

  if (nameWithoutSyncPrefix.size() != 2) {
    NDN_LOG_WARN("Two components required after sync prefix: /<IBF>/<numCumulativeElements>; received: " << interestName);
    return;
  }

  ndn::name::Component ibltName = interestName[-2];
  uint64_t numRcvdElements = interestName[-1].toNumber();

  detail::IBLT iblt(m_expectedNumEntries, m_ibltCompression);
  try {
    iblt.initialize(ibltName);
  }
  catch (const std::exception& e) {
    NDN_LOG_WARN(e.what());
    return;
  }

  auto diff = m_iblt - iblt;

  NDN_LOG_TRACE("Decode, positive: " << diff.positive.size()
                << " negative: " << diff.negative.size() << " m_threshold: "
                << m_threshold);

  auto waitingIt = m_waitingForProcessing.find(interestName);

  if (!diff.canDecode) {
    NDN_LOG_DEBUG("Cannot decode differences!");

    if (numRcvdElements > m_numOwnElements) {
      if (!isTimedProcessing && waitingIt == m_waitingForProcessing.end()) {
        NDN_LOG_TRACE("Decode failure, adding to waiting Interest list " << interestNameHash);
        m_waitingForProcessing.emplace(interestName, WaitingEntryInfo{0, interest.getNonce()});
        scheduleProcessWaitingInterests();
      }
      else if (isTimedProcessing && waitingIt != m_waitingForProcessing.end()) {
        if (waitingIt->second.numTries > 1) {
          NDN_LOG_TRACE("Decode failure, still behind. Erasing waiting Interest as we have tried twice");
          waitingIt->second.numTries = std::numeric_limits<uint16_t>::max(); // markWaitingInterestForDeletion
          NDN_LOG_DEBUG("Waiting Interest has been deleted. Sending new sync interest");
          sendSyncInterest();
        }
        else {
          NDN_LOG_TRACE("Decode failure, still behind, waiting more till the next timer");
        }
      }
      else {
        NDN_LOG_TRACE("Decode failure, still behind");
      }
    }
    else {
      if (m_numOwnElements == numRcvdElements && diff.positive.size() == 0 && diff.negative.size() > 0) {
        NDN_LOG_TRACE("We have nothing to offer and are actually behind");
#ifdef PSYNC_WITH_TESTS
            ++nIbfDecodeFailuresBelowThreshold;
#endif // PSYNC_WITH_TESTS
        return;
      }

      detail::State state;
      for (const auto& content : m_prefixes) {
        if (content.second != 0) {
          state.addContent(ndn::Name(content.first).appendNumber(content.second));
        }
      }
#ifdef PSYNC_WITH_TESTS
            ++nIbfDecodeFailuresAboveThreshold;
#endif // PSYNC_WITH_TESTS

      if (!state.getContent().empty()) {
        NDN_LOG_DEBUG("Sending entire state: " << state);
        // Want low freshness when potentially sending large content to clear it quickly from the network
        sendSyncData(interestName, state.wireEncode(), 10_ms);
        // Since we're directly sending the data, we need to clear pending interests here
        deletePendingInterests(interestName);
      }
      // We seem to be ahead, delete the Interest from waiting list
      if (waitingIt != m_waitingForProcessing.end()) {
        waitingIt->second.numTries = std::numeric_limits<uint16_t>::max();
      }
    }
    return;
  }

  if (diff.positive.size() == 0 && diff.negative.size() == 0) {
    NDN_LOG_TRACE("Saving positive: " << diff.positive.size() << " negative: " << diff.negative.size());

    auto& entry = m_pendingEntries.emplace(interestName, PendingEntryInfo{iblt, {}}).first->second;
    entry.expirationEvent = m_scheduler.schedule(interest.getInterestLifetime(),
                            [this, interest] {
                              NDN_LOG_TRACE("Erase pending Interest " << interest.getNonce());
                              m_pendingEntries.erase(interest.getName());
                            });

    // Can't delete directly in this case as it will cause
    // memory access errors with the for loop in processWaitingInterests
    if (isTimedProcessing) {
      if (waitingIt != m_waitingForProcessing.end()) {
        waitingIt->second.numTries = std::numeric_limits<uint16_t>::max();
      }
    }
    return;
  }

  // Only add to waiting list if we don't have anything to send (positive = 0)
  if (diff.positive.size() == 0 && diff.negative.size() > 0) {
    if (!isTimedProcessing && waitingIt == m_waitingForProcessing.end()) {
      NDN_LOG_TRACE("Adding Interest to waiting list: " << interestNameHash);
      m_waitingForProcessing.emplace(interestName, WaitingEntryInfo{0, interest.getNonce()});
      scheduleProcessWaitingInterests();
    }
    else if (isTimedProcessing && waitingIt != m_waitingForProcessing.end()) {
      if (waitingIt->second.numTries > 1) {
        NDN_LOG_TRACE("Still behind after waiting for Interest " << interestNameHash <<
                      ". Erasing waiting Interest as we have tried twice");
        waitingIt->second.numTries = std::numeric_limits<uint16_t>::max(); // markWaitingInterestForDeletion
      }
      else {
        NDN_LOG_TRACE("Still behind after waiting for Interest " << interestNameHash <<
                      ". Keep waiting for Interest as number of tries is not exhausted");
      }
    }
    else {
      NDN_LOG_TRACE("Still behind after waiting for Interest " << interestNameHash);
    }
    return;
  }

  if (diff.positive.size() > 0) {
    detail::State state;
    for (const auto& hash : diff.positive) {
      auto nameIt = m_biMap.left.find(hash);
      if (nameIt != m_biMap.left.end()) {
        ndn::Name nameWithoutSeq = nameIt->second.getPrefix(-1);
        // Don't sync up sequence number zero
        if (m_prefixes[nameWithoutSeq] != 0 &&
            !isFutureHash(nameWithoutSeq.toUri(), diff.negative)) {
          state.addContent(nameIt->second);
        }
      }
    }

    if (!state.getContent().empty()) {
      NDN_LOG_DEBUG("Sending sync content: " << state);
      sendSyncData(interestName, state.wireEncode(), m_syncReplyFreshness);

      // Timed processing or not - if we are answering it, it should not go in waiting Interests
      if (waitingIt != m_waitingForProcessing.end()) {
        waitingIt->second.numTries = std::numeric_limits<uint16_t>::max();
      }
    }
  }
}

void
FullProducer::sendSyncData(const ndn::Name& name, const ndn::Block& block,
                           ndn::time::milliseconds syncReplyFreshness)
{
  bool isSatisfyingOwnInterest = m_outstandingInterestName == name;
  if (isSatisfyingOwnInterest && m_fetcher) {
    NDN_LOG_DEBUG("Removing our pending Interest from face (stop fetcher)");
    m_fetcher->stop();
    m_outstandingInterestName.clear();
  }

  NDN_LOG_DEBUG("Sending sync Data");
  auto content = detail::compress(m_contentCompression, block);
  m_segmentPublisher.publish(name, name, *content, syncReplyFreshness);
  if (isSatisfyingOwnInterest) {
    NDN_LOG_DEBUG("Renewing sync interest");
    sendSyncInterest();
  }
}

void
FullProducer::onSyncData(const ndn::Interest& interest, const ndn::ConstBufferPtr& bufferPtr)
{
  deletePendingInterests(interest.getName());

  detail::State state;
  try {
    auto decompressed = detail::decompress(m_contentCompression, *bufferPtr);
    state.wireDecode(ndn::Block(std::move(decompressed)));
  }
  catch (const std::exception& e) {
    NDN_LOG_ERROR("Cannot parse received sync Data: " << e.what());
    return;
  }
  NDN_LOG_DEBUG("Sync Data received: " << state);

  std::vector<MissingDataInfo> updates;

  for (const auto& content : state) {
    ndn::Name prefix = content.getPrefix(-1);
    uint64_t seq = content.get(content.size() - 1).toNumber();

    if (m_prefixes.find(prefix) == m_prefixes.end() || m_prefixes[prefix] < seq) {
      updates.push_back({prefix, m_prefixes[prefix] + 1, seq, m_incomingFace});
      updateSeqNo(prefix, seq);
      // We should not call satisfyPendingSyncInterests here because we just
      // got data and deleted pending interest by calling deletePendingFullSyncInterests
      // But we might have interests not matching to this interest that might not have deleted
      // from pending sync interest
    }
  }

  if (!updates.empty()) {
    m_onUpdate(updates);
    // Wait a bit to let neighbors get the data too
    auto after = ndn::time::milliseconds(m_jitter(m_rng));
    m_scheduledSyncInterestId = m_scheduler.schedule(after, [this] {
      NDN_LOG_DEBUG("Got updates, renewing sync Interest now");
      sendSyncInterest();
    });
    NDN_LOG_DEBUG("Schedule sync Interest after: " << after);
    m_inNoNewDataWaitOutPeriod = false;

    processWaitingInterests();
  }
  else {
    NDN_LOG_TRACE("No new update, Interest nonce: " << interest.getNonce() <<
                  " , hash: " << std::hash<ndn::Name>{}(interest.getName()));
    m_inNoNewDataWaitOutPeriod = true;

    // Have to wait, otherwise will get same data from CS
    auto after = m_syncReplyFreshness + ndn::time::milliseconds(m_jitter(m_rng));
    m_scheduledSyncInterestId = m_scheduler.schedule(after, [this] {
      NDN_LOG_DEBUG("Sending sync Interest after no new update");
      m_inNoNewDataWaitOutPeriod = false;
      sendSyncInterest();
    });
    NDN_LOG_DEBUG("Schedule sync after: " << after);
  }

}

void
FullProducer::satisfyPendingInterests(const ndn::Name& updatedPrefixWithSeq)
{
  NDN_LOG_DEBUG("Satisfying full sync Interest: " << m_pendingEntries.size());

  for (auto it = m_pendingEntries.begin(); it != m_pendingEntries.end();) {
    NDN_LOG_TRACE("Satisfying pending Interest: " << std::hash<ndn::Name>{}(it->first.getPrefix(-1)));
    const auto& entry = it->second;
    auto diff = m_iblt - entry.iblt;
    NDN_LOG_TRACE("Decoded: " << diff.canDecode << " positive: " << diff.positive.size() <<
                  " negative: " << diff.negative.size());

    detail::State state;
    bool publishedPrefixInDiff = false;
    for (const auto& hash : diff.positive) {
      auto nameIt = m_biMap.left.find(hash);
      if (nameIt != m_biMap.left.end()) {
        if (updatedPrefixWithSeq == nameIt->second) {
          publishedPrefixInDiff = true;
        }
        state.addContent(nameIt->second);
      }
    }

    if (!publishedPrefixInDiff) {
      state.addContent(updatedPrefixWithSeq);
    }

    NDN_LOG_DEBUG("Satisfying sync content: " << state);
    sendSyncData(it->first, state.wireEncode(), m_syncReplyFreshness);
    it = m_pendingEntries.erase(it);
  }
}

bool
FullProducer::isFutureHash(const ndn::Name& prefix, const std::set<uint32_t>& negative)
{
  auto nextHash = detail::murmurHash3(detail::N_HASHCHECK,
                                      ndn::Name(prefix).appendNumber(m_prefixes[prefix] + 1));
  return negative.find(nextHash) != negative.end();
}

void
FullProducer::deletePendingInterests(const ndn::Name& interestName)
{
  auto it = m_pendingEntries.find(interestName);
  if (it != m_pendingEntries.end()) {
    NDN_LOG_TRACE("Delete pending Interest: " << std::hash<ndn::Name>{}(interestName));
    it = m_pendingEntries.erase(it);
  }
}

} // namespace psync
