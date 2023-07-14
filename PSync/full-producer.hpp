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

#ifndef PSYNC_FULL_PRODUCER_HPP
#define PSYNC_FULL_PRODUCER_HPP

#include "PSync/producer-base.hpp"

#include <random>
#include <set>

#include <ndn-cxx/util/segment-fetcher.hpp>

namespace psync {

/**
 * @brief Full sync logic to synchronize with other nodes
 *        where all nodes wants to get all names prefixes synced.
 *
 * Application should call publishName whenever it wants to
 * let consumers know that new data is available for the userPrefix.
 * Multiple userPrefixes can be added by using addUserNode.
 * Currently, fetching and publishing of data needs to be handled by the application.
 */
class FullProducer : public ProducerBase
{
public:
  /**
   * @brief Constructor options.
   */
  struct Options
  {
    /// Callback to be invoked when there is new data.
    UpdateCallback onUpdate = [] (const auto&) {};
    /// Expected number of entries in IBF.
    uint32_t ibfCount = 80;
    /// Compression scheme to use for IBF.
    CompressionScheme ibfCompression = CompressionScheme::DEFAULT;
    /// Lifetime of sync Interest.
    ndn::time::milliseconds syncInterestLifetime = SYNC_INTEREST_LIFETIME;
    /// FreshnessPeriod of sync Data.
    ndn::time::milliseconds syncDataFreshness = SYNC_REPLY_FRESHNESS;
    /// Compression scheme to use for Data content.
    CompressionScheme contentCompression = CompressionScheme::DEFAULT;
  };

  /**
   * @brief Constructor.
   *
   * @param face Application face.
   * @param keyChain KeyChain instance to use for signing.
   * @param syncPrefix The prefix of the sync group.
   * @param opts Options.
   */
  FullProducer(ndn::Face& face,
               ndn::KeyChain& keyChain,
               const ndn::Name& syncPrefix,
               const Options& opts);

  [[deprecated]]
  FullProducer(ndn::Face& face,
               ndn::KeyChain& keyChain,
               size_t expectedNumEntries,
               const ndn::Name& syncPrefix,
               const ndn::Name& userPrefix,
               UpdateCallback onUpdateCallBack,
               ndn::time::milliseconds syncInterestLifetime = SYNC_INTEREST_LIFETIME,
               ndn::time::milliseconds syncReplyFreshness = SYNC_REPLY_FRESHNESS,
               CompressionScheme ibltCompression = CompressionScheme::DEFAULT,
               CompressionScheme contentCompression = CompressionScheme::DEFAULT);

  ~FullProducer();

  /**
   * @brief Publish name to let others know
   *
   * addUserNode needs to be called before this to add the prefix
   * if not already added via the constructor.
   * If seq is null then the seq of prefix is incremented by 1 else
   * the supplied sequence is set in the IBF.
   *
   * @param prefix the prefix to be updated
   * @param seq the sequence number of the prefix
   */
  void
  publishName(const ndn::Name& prefix, std::optional<uint64_t> seq = std::nullopt);

PSYNC_PUBLIC_WITH_TESTS_ELSE_PRIVATE:
  /**
   * @brief Send sync interest for full synchronization
   *
   * Forms the interest name: /<sync-prefix>/<own-IBF>
   * Cancels any pending sync interest we sent earlier on the face
   * Sends the sync interest
   */
  void
  sendSyncInterest();

  void
  processWaitingInterests();

  void
  scheduleProcessWaitingInterests();

  /**
   * @brief Process sync interest from other parties
   *
   * Get differences b/w our IBF and IBF in the sync interest.
   * If we cannot get the differences successfully then send an application nack.
   *
   * If we have some things in our IBF that the other side does not have, reply with the content or
   * If no. of different items is greater than threshold or equals zero then send a nack.
   * Otherwise add the sync interest into a map with interest name as key and PendingEntryInfoFull
   * as value.
   *
   * @param prefixName prefix for sync group which we registered
   * @param interest the interest we got
   * @param isTimedProcessing is this interest from the waiting interests list
   */
  void
  onSyncInterest(const ndn::Name& prefixName, const ndn::Interest& interest,
                 bool isTimedProcessing = false);

  /**
   * @brief Send sync data
   *
   * Check if the data will satisfy our own pending interest,
   * remove it first if it does, and then renew the sync interest
   * Otherwise just send the data
   *
   * @param name name to be set as data name
   * @param block the content of the data
   * @param syncReplyFreshness the freshness to use for the sync data; defaults to @p SYNC_REPLY_FRESHNESS
   */
  void
  sendSyncData(const ndn::Name& name, const ndn::Block& block,
               ndn::time::milliseconds syncReplyFreshness);

  /**
   * @brief Process sync data
   *
   * Call deletePendingInterests to delete any pending sync interest with
   * interest name would have been satisfied once NFD got the data.
   *
   * For each prefix/seq in data content check that we don't already have the
   * prefix/seq and updateSeq(prefix, seq)
   *
   * Notify the application about the updates
   * sendSyncInterest because the last one was satisfied by the incoming data
   *
   * @param interest interest for which we got the data
   * @param bufferPtr sync data content
   */
  void
  onSyncData(const ndn::Interest& interest, const ndn::ConstBufferPtr& bufferPtr);

private:
  /**
   * @brief Satisfy pending sync interests
   *
   * For pending sync interests do a difference with current IBF to find out missing prefixes.
   * Send [Missing Prefixes] union @p updatedPrefixWithSeq
   *
   * This is because it is called from publish, so the @p updatedPrefixWithSeq must be missing
   * from other nodes regardless of IBF difference failure.
   */
  void
  satisfyPendingInterests(const ndn::Name& updatedPrefixWithSeq);

  /**
   * @brief Delete pending sync interests that match given name
   */
  void
  deletePendingInterests(const ndn::Name& interestName);

  /**
   * @brief Check if hash(prefix + 1) is in negative
   *
   * Sometimes what happens is that interest from other side
   * gets to us before the data
   */
  bool
  isFutureHash(const ndn::Name& prefix, const std::set<uint32_t>& negative);

#ifdef PSYNC_WITH_TESTS
public:
  size_t nIbfDecodeFailuresAboveThreshold = 0;
  size_t nIbfDecodeFailuresBelowThreshold = 0;
#endif // PSYNC_WITH_TESTS

private:
  struct PendingEntryInfo
  {
    detail::IBLT iblt;
    ndn::scheduler::ScopedEventId expirationEvent;
  };

  struct WaitingEntryInfo
  {
    uint16_t numTries = 0;
    ndn::Interest::Nonce nonce;
  };

  ndn::time::milliseconds m_syncInterestLifetime;
  UpdateCallback m_onUpdate;
  ndn::scheduler::ScopedEventId m_scheduledSyncInterestId;
  static constexpr int MIN_JITTER = 100;
  static constexpr int MAX_JITTER = 500;
  std::uniform_int_distribution<> m_jitter{MIN_JITTER, MAX_JITTER};
  ndn::time::system_clock::time_point m_lastInterestSentTime;
  ndn::Name m_outstandingInterestName;
  ndn::ScopedRegisteredPrefixHandle m_registeredPrefix;
  std::shared_ptr<ndn::SegmentFetcher> m_fetcher;
  uint64_t m_incomingFace = 0;
  std::map<ndn::Name, WaitingEntryInfo> m_waitingForProcessing;
  bool m_inNoNewDataWaitOutPeriod = false;
  ndn::scheduler::ScopedEventId m_interestDelayTimerId;

PSYNC_PUBLIC_WITH_TESTS_ELSE_PRIVATE:
  std::map<ndn::Name, PendingEntryInfo> m_pendingEntries;
};

} // namespace psync

#endif // PSYNC_FULL_PRODUCER_HPP
