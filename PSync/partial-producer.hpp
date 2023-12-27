/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2023,  The University of Memphis
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

#ifndef PSYNC_PARTIAL_PRODUCER_HPP
#define PSYNC_PARTIAL_PRODUCER_HPP

#include "PSync/producer-base.hpp"
#include "PSync/detail/bloom-filter.hpp"

namespace psync {

/**
 * @brief Partial sync logic to publish data names
 *
 * Application should call publishName whenever it wants to
 * let consumers know that new data is available.
 * Additional userPrefix should be added via addUserNode before calling publishName
 * Currently, publishing of data needs to be handled by the application.
 */
class PartialProducer : public ProducerBase
{
public:
  /**
   * @brief Constructor options.
   */
  struct Options
  {
    /// Expected number of entries in IBF.
    uint32_t ibfCount = 40;
    /// Compression scheme to use for IBF.
    CompressionScheme ibfCompression = CompressionScheme::NONE;
    /// FreshnessPeriod of hello Data.
    ndn::time::milliseconds helloDataFreshness = HELLO_REPLY_FRESHNESS;
    /// FreshnessPeriod of sync Data.
    ndn::time::milliseconds syncDataFreshness = SYNC_REPLY_FRESHNESS;
  };

  /**
   * @brief Constructor.
   *
   * @param face Application face.
   * @param keyChain KeyChain instance to use for signing.
   * @param syncPrefix The prefix of the sync group.
   * @param opts Options.
   */
  PartialProducer(ndn::Face& face,
                  ndn::KeyChain& keyChain,
                  const ndn::Name& syncPrefix,
                  const Options& opts);

  [[deprecated]]
  PartialProducer(ndn::Face& face,
                  ndn::KeyChain& keyChain,
                  size_t expectedNumEntries,
                  const ndn::Name& syncPrefix,
                  const ndn::Name& userPrefix,
                  ndn::time::milliseconds helloReplyFreshness = HELLO_REPLY_FRESHNESS,
                  ndn::time::milliseconds syncReplyFreshness = SYNC_REPLY_FRESHNESS,
                  CompressionScheme ibltCompression = CompressionScheme::NONE);

  /**
   * @brief Publish name to let subscribed consumers know
   *
   * If seq is null then the seq of prefix is incremented by 1 else
   * the supplied sequence is set in the IBF.
   * Upon updating the sequence in the IBF satisfyPendingSyncInterests
   * is called to let subscribed consumers know.
   *
   * @param prefix the prefix to be updated
   * @param seq the sequence number of the prefix
   */
  void
  publishName(const ndn::Name& prefix, std::optional<uint64_t> seq = std::nullopt);

private:
  /**
   * @brief Satisfy any pending interest that have subscription for prefix
   *
   * @param prefix the prefix that was updated in publishName
   */
  void
  satisfyPendingSyncInterests(const ndn::Name& prefix);

PSYNC_PUBLIC_WITH_TESTS_ELSE_PRIVATE:
  /**
   * @brief Receive hello interest from consumer and respond with hello data
   *
   * Hello data's name format is: /\<sync-prefix\>/hello/\<current-IBF\>
   *
   * @param prefix the hello interest prefix
   * @param interest the hello interest received
   */
  void
  onHelloInterest(const ndn::Name& prefix, const ndn::Interest& interest);

  /**
   * @brief Receive sync interest from consumer
   *
   * Either respond with sync data if consumer is behind or
   * store sync interest in m_pendingEntries
   *
   * Sync data's name format is: /\<syncPrefix\>/sync/\<BF\>/\<old-IBF\>/\<current-IBF\>
   * (BF has 3 components).
   */
  void
  onSyncInterest(const ndn::Name& prefix, const ndn::Interest& interest);

PSYNC_PUBLIC_WITH_TESTS_ELSE_PRIVATE:
  struct PendingEntryInfo
  {
    detail::BloomFilter bf;
    detail::IBLT iblt;
    ndn::scheduler::ScopedEventId expirationEvent;
  };

  std::map<ndn::Name, PendingEntryInfo> m_pendingEntries;
  ndn::ScopedRegisteredPrefixHandle m_registeredPrefix;
  ndn::time::milliseconds m_helloReplyFreshness;
};

} // namespace psync

#endif // PSYNC_PARTIAL_PRODUCER_HPP
