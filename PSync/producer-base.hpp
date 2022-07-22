/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2022,  The University of Memphis
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

#ifndef PSYNC_PRODUCER_BASE_HPP
#define PSYNC_PRODUCER_BASE_HPP

#include "PSync/common.hpp"
#include "PSync/detail/access-specifiers.hpp"
#include "PSync/detail/iblt.hpp"
#include "PSync/segment-publisher.hpp"

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/security/validator-config.hpp>
#include <ndn-cxx/util/random.hpp>
#include <ndn-cxx/util/scheduler.hpp>

#include <boost/bimap/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>

#include <map>

namespace psync {

namespace bm = boost::bimaps;

/**
 * @brief Base class for PartialProducer and FullProducer
 *
 * Contains code common to both
 */
class ProducerBase
{
public:
  class Error : public std::runtime_error
  {
  public:
    using std::runtime_error::runtime_error;
  };

PSYNC_PUBLIC_WITH_TESTS_ELSE_PROTECTED:
  /**
   * @brief constructor
   *
   * @param expectedNumEntries expected number entries in IBF
   * @param face application's face
   * @param syncPrefix The prefix of the sync group
   * @param userPrefix The prefix of the first user in the group
   * @param syncReplyFreshness freshness of sync data
   * @param ibltCompression Compression scheme to use for IBF
   * @param contentCompression Compression scheme to use for Data content
   */
  ProducerBase(size_t expectedNumEntries,
               ndn::Face& face,
               const ndn::Name& syncPrefix,
               const ndn::Name& userPrefix,
               ndn::time::milliseconds syncReplyFreshness = SYNC_REPLY_FRESHNESS,
               CompressionScheme ibltCompression = CompressionScheme::NONE,
               CompressionScheme contentCompression = CompressionScheme::NONE);
public:
  /**
   * @brief Returns the current sequence number of the given prefix
   *
   * @param prefix prefix to get the sequence number of
   */
  std::optional<uint64_t>
  getSeqNo(const ndn::Name& prefix) const
  {
    auto it = m_prefixes.find(prefix);
    if (it == m_prefixes.end()) {
      return std::nullopt;
    }
    return it->second;
  }

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
  addUserNode(const ndn::Name& prefix);

  /**
   * @brief Remove the user node from synchronization
   *
   * Erases prefix from IBF and other maps
   *
   * @param prefix the user node to be removed
   */
  void
  removeUserNode(const ndn::Name& prefix);

PSYNC_PUBLIC_WITH_TESTS_ELSE_PROTECTED:
  /**
   * @brief Update m_prefixes and IBF with the given prefix and seq
   *
   * Whoever calls this needs to make sure that prefix is in m_prefixes
   * We remove already existing prefix/seq from IBF
   * (unless seq is zero because we don't insert zero seq into IBF)
   * Then we update m_prefixes, m_biMap, and IBF
   *
   * @param prefix prefix of the update
   * @param seq sequence number of the update
   */
  void
  updateSeqNo(const ndn::Name& prefix, uint64_t seq);

  bool
  isUserNode(const ndn::Name& prefix) const
  {
    return m_prefixes.find(prefix) != m_prefixes.end();
  }

  /**
   * @brief Sends a data packet with content type nack
   *
   * Producer sends a nack to consumer if consumer has very old IBF
   * whose differences with latest IBF can't be decoded successfully
   *
   * @param name send application nack with this name
   */
  void
  sendApplicationNack(const ndn::Name& name);

  /**
   * @brief Logs a message if setting an interest filter fails
   */
  void
  onRegisterFailed(const ndn::Name& prefix, const std::string& msg) const;

PSYNC_PUBLIC_WITH_TESTS_ELSE_PROTECTED:
  detail::IBLT m_iblt;
  uint32_t m_expectedNumEntries;
  // Threshold is used check if the differences are greater
  // than it and whether we need to update the other side.
  uint32_t m_threshold;

  // prefix and sequence number
  std::map<ndn::Name, uint64_t> m_prefixes;

  using HashNameBiMap = bm::bimap<bm::unordered_set_of<uint32_t>,
                                  bm::unordered_set_of<ndn::Name, std::hash<ndn::Name>>>;
  HashNameBiMap m_biMap;

  ndn::Face& m_face;
  ndn::KeyChain m_keyChain;
  ndn::Scheduler m_scheduler;

  ndn::Name m_syncPrefix;
  ndn::Name m_userPrefix;

  ndn::time::milliseconds m_syncReplyFreshness;

  SegmentPublisher m_segmentPublisher;

  ndn::random::RandomNumberEngine& m_rng;
  CompressionScheme m_ibltCompression;
  CompressionScheme m_contentCompression;
};

} // namespace psync

#endif // PSYNC_PRODUCER_BASE_HPP
