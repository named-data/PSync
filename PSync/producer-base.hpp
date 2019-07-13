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

#ifndef PSYNC_PRODUCER_BASE_HPP
#define PSYNC_PRODUCER_BASE_HPP

#include "PSync/detail/access-specifiers.hpp"
#include "PSync/detail/bloom-filter.hpp"
#include "PSync/detail/iblt.hpp"
#include "PSync/detail/util.hpp"
#include "PSync/segment-publisher.hpp"

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/random.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <ndn-cxx/util/time.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/security/validator-config.hpp>

#include <map>
#include <unordered_set>

namespace psync {

using namespace ndn::time_literals;

const ndn::time::milliseconds SYNC_REPLY_FRESHNESS = 1_s;

/**
 * @brief Base class for PartialProducer and FullProducer
 *
 * Contains code common to both
 */
class ProducerBase
{
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
   * @param syncPrefix The prefix of the sync group
   * @param syncReplyFreshness freshness of sync data
   */
  ProducerBase(size_t expectedNumEntries,
               const ndn::Name& syncPrefix,
               ndn::time::milliseconds syncReplyFreshness = SYNC_REPLY_FRESHNESS);

PSYNC_PUBLIC_WITH_TESTS_ELSE_PROTECTED:
  /**
   * @brief Update m_prefixes and IBF with the given prefix and seq
   *
   * Whoever calls this needs to make sure that prefix is in m_prefixes
   * We remove already existing prefix/seq from IBF
   * (unless seq is zero because we don't insert zero seq into IBF)
   * Then we update m_prefix, m_prefix2hash, m_hash2prefix, and IBF
   *
   * @param name prefix of the update
   */
  void
  insertToIBF(const ndn::Name& name);

  void
  removeFromIBF(const ndn::Name& name);

  /**
   * @brief Logs a message if setting an interest filter fails
   *
   * @param prefix
   * @param msg
   */
  void
  onRegisterFailed(const ndn::Name& prefix, const std::string& msg) const;

PSYNC_PUBLIC_WITH_TESTS_ELSE_PROTECTED:
  IBLT m_iblt;
  uint32_t m_expectedNumEntries;
  // Threshold is used check if the differences are greater
  // than it and whether we need to update the other side.
  uint32_t m_threshold;

  // Just for looking up hash faster (instead of calculating it again)
  // Only used in updateSeqNo, name (arbitrary or /prefix/seq) is the key
  std::map <ndn::Name, uint32_t> m_name2hash;
  // Value is name
  std::map <uint32_t, ndn::Name> m_hash2name;

  ndn::Name m_syncPrefix;

  ndn::time::milliseconds m_syncReplyFreshness;

  ndn::random::RandomNumberEngine& m_rng;
};

} // namespace psync

#endif // PSYNC_PRODUCER_BASE_HPP
