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

#ifndef PSYNC_FULL_PRODUCER_ARBITRARY_HPP
#define PSYNC_FULL_PRODUCER_ARBITRARY_HPP

#include "PSync/producer-base.hpp"
#include "PSync/detail/state.hpp"

#include <map>
#include <unordered_set>
#include <random>

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <ndn-cxx/util/segment-fetcher.hpp>
#include <ndn-cxx/util/time.hpp>

namespace psync {

// Name has to be different than PendingEntryInfo
// used in partial-producer otherwise get strange segmentation-faults
// when partial producer is destructed
struct PendingEntryInfoFull
{
  IBLT iblt;
  ndn::scheduler::ScopedEventId expirationEvent;
};

typedef std::function<void(const std::vector<ndn::Name>&)> ArbitraryUpdateCallback;
typedef std::function<bool(const ndn::Name&)> CanAddName;
typedef std::function<bool(const ndn::Name&,
                           const std::set<uint32_t>&)> ShouldAddToSyncDataCallback;

const ndn::time::milliseconds SYNC_INTEREST_LIFTIME = 1_s;

/**
 * @brief Full sync logic to synchronize with other nodes
 *        where all nodes wants to get all names prefixes synced.
 *
 * Application should call publishName whenever it wants to
 * let consumers know that new data is available for the userPrefix.
 * Multiple userPrefixes can be added by using addUserNode.
 * Currently, fetching and publishing of data needs to be handled by the application.
 */
class FullProducerArbitrary : public ProducerBase
{
public:
  /**
   * @brief constructor
   *
   * Registers syncPrefix in NFD and sends a sync interest
   *
   * @param expectedNumEntries expected entries in IBF
   * @param face application's face
   * @param syncPrefix The prefix of the sync group
   * @param onArbitraryUpdateCallback The call back to be called when there is new data
   * @param syncInterestLifetime lifetime of the sync interest
   * @param syncReplyFreshness freshness of sync data
   * @param onShouldAddToSyncDataCallback whether to add sync data to content being sent (FullProducer future hash)
   * @param onCanAddName whether to add name to IBF
   */
  FullProducerArbitrary(size_t expectedNumEntries,
                        ndn::Face& face,
                        const ndn::Name& syncPrefix,
                        const ArbitraryUpdateCallback& onArbitraryUpdateCallback,
                        ndn::time::milliseconds syncInterestLifetime = SYNC_INTEREST_LIFTIME,
                        ndn::time::milliseconds syncReplyFreshness = SYNC_REPLY_FRESHNESS,
                        const ShouldAddToSyncDataCallback& onShouldAddToSyncDataCallback = ShouldAddToSyncDataCallback(),
                        const CanAddName& onCanAddName = CanAddName());

  ~FullProducerArbitrary();

  void
  removeName(const ndn::Name& name) {
    removeFromIBF(name);
  }

  void
  addName(const ndn::Name& name) {
    insertToIBF(name);
  }

  /**
   * @brief Publish name to let others know
   *
   * However, if the name has already been published, do nothing.
   * @param name the Name to be updated
   */
  void
  publishName(const ndn::Name& name);

private:
  /**
   * @brief Send sync interest for full synchronization
   *
   * Forms the interest name: /<sync-prefix>/<own-IBF>
   * Cancels any pending sync interest we sent earlier on the face
   * Sends the sync interest
   */
  void
  sendSyncInterest();

PSYNC_PUBLIC_WITH_TESTS_ELSE_PRIVATE:
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
   */
  void
  onSyncInterest(const ndn::Name& prefixName, const ndn::Interest& interest);

private:
  /**
   * @brief Send sync data
   *
   * Check if the data will satisfy our own pending interest,
   * remove it first if it does, and then renew the sync interest
   * Otherwise just send the data
   *
   * @param name name to be set as data name
   * @param block the content of the data
   */
  void
  sendSyncData(const ndn::Name& name, const ndn::Block& block);

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

public:
  /**
   * @brief Satisfy pending sync interests
   *
   * For pending sync interests SI, if IBF of SI has any difference from our own IBF:
   * send data back.
   * If we can't decode differences from the stored IBF, then delete it.
   */
  void
  satisfyPendingInterests();

PSYNC_PUBLIC_WITH_TESTS_ELSE_PRIVATE:
  /**
   * @brief Delete pending sync interests that match given name
   */
  void
  deletePendingInterests(const ndn::Name& interestName);

private:
  ndn::Face& m_face;
  ndn::KeyChain m_keyChain;
  ndn::Scheduler m_scheduler;

PSYNC_PUBLIC_WITH_TESTS_ELSE_PROTECTED:
  SegmentPublisher m_segmentPublisher;

  std::map<ndn::Name, PendingEntryInfoFull> m_pendingEntries;
  ndn::time::milliseconds m_syncInterestLifetime;
  ArbitraryUpdateCallback m_onArbitraryUpdateCallback;
  ShouldAddToSyncDataCallback m_onShouldAddToSyncDataCallback;
  CanAddName m_onCanAddName;
  ndn::scheduler::ScopedEventId m_scheduledSyncInterestId;
  std::uniform_int_distribution<> m_jitter;
  ndn::Name m_outstandingInterestName;
  ndn::ScopedRegisteredPrefixHandle m_registeredPrefix;
  std::shared_ptr<ndn::util::SegmentFetcher> m_fetcher;
};

} // namespace psync

#endif // PSYNC_FULL_PRODUCER_ARBITRARY_HPP
