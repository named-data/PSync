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

#ifndef PSYNC_FULL_PRODUCER_HPP
#define PSYNC_FULL_PRODUCER_HPP

#include "PSync/producer-base.hpp"
#include "PSync/full-producer-arbitrary.hpp"
#include "PSync/detail/state.hpp"
#include "PSync/detail/user-prefixes.hpp"

#include <map>
#include <random>
#include <set>

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <ndn-cxx/util/segment-fetcher.hpp>
#include <ndn-cxx/util/time.hpp>

namespace psync {

typedef std::function<void(const std::vector<MissingDataInfo>&)> UpdateCallback;

/**
 * @brief Full sync logic to synchronize with other nodes
 *        where all nodes wants to get all names prefixes synced.
 *
 * Application should call publishName whenever it wants to
 * let consumers know that new data is available for the userPrefix.
 * Multiple userPrefixes can be added by using addUserNode.
 * Currently, fetching and publishing of data needs to be handled by the application.
 */
class FullProducer
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
   * @param userPrefix The prefix of the first user in the group
   * @param onUpdateCallBack The call back to be called when there is new data
   * @param syncInterestLifetime lifetime of the sync interest
   * @param syncReplyFreshness freshness of sync data
   */
  FullProducer(size_t expectedNumEntries,
               ndn::Face& face,
               const ndn::Name& syncPrefix,
               const ndn::Name& userPrefix,
               const UpdateCallback& onUpdateCallback,
               ndn::time::milliseconds syncInterestLifetime = SYNC_INTEREST_LIFTIME,
               ndn::time::milliseconds syncReplyFreshness = SYNC_REPLY_FRESHNESS);

  /**
   * @brief Returns the current sequence number of the given prefix
   *
   * @param prefix prefix to get the sequence number of
   */
  ndn::optional<uint64_t>
  getSeqNo(const ndn::Name& prefix) const
  {
    return m_prefixes.getSeqNo(prefix);
  }

PSYNC_PUBLIC_WITH_TESTS_ELSE_PRIVATE:
  /**
   * @brief Adds a user node for synchronization
   *
   * Initializes m_prefixes[prefix] to zero
   * Does not add zero-th sequence number to IBF
   * because if a large number of user nodes are added
   * then decoding of the difference between own IBF and
   * other IBF will not be possible
   *
   * @param prefix the user node to be added
   */
  bool
  addUserNode(const ndn::Name& prefix)
  {
    return m_prefixes.addUserNode(prefix);
  }

  /**
   * @brief Remove the user node from synchronization
   *
   * Erases prefix from IBF and other maps
   *
   * @param prefix the user node to be removed
   */
  void
  removeUserNode(const ndn::Name& prefix)
  {
    if (m_prefixes.isUserNode(prefix)) {
      uint64_t seqNo = m_prefixes.prefixes[prefix];
      m_prefixes.removeUserNode(prefix);
      m_producerArbitrary.removeName(ndn::Name(prefix).appendNumber(seqNo));
    }
  }

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
  publishName(const ndn::Name& prefix, ndn::optional<uint64_t> seq = ndn::nullopt);

private:
   /**
   * @brief Check if hash(prefix + 1) is in negative
   *
   * Sometimes what happens is that interest from other side
   * gets to us before the data
   */
  bool
  isNotFutureHash(const ndn::Name& prefix, const std::set<uint32_t>& negative);

PSYNC_PUBLIC_WITH_TESTS_ELSE_PROTECTED:
  void
  updateSeqNo(const ndn::Name& prefix, uint64_t seq);

private:
  void
  arbitraryUpdateCallBack(const std::vector<ndn::Name>& names);

PSYNC_PUBLIC_WITH_TESTS_ELSE_PROTECTED:
  FullProducerArbitrary m_producerArbitrary;
  UpdateCallback m_onUpdateCallback;

  UserPrefixes m_prefixes;
};

} // namespace psync

#endif // PSYNC_FULL_PRODUCER_HPP
