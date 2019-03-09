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

#include "PSync/producer-base.hpp"

#include <ndn-cxx/util/logger.hpp>
#include <boost/algorithm/string.hpp>

#include <cstring>
#include <limits>
#include <functional>

namespace psync {

NDN_LOG_INIT(psync.ProducerBase);

ProducerBase::ProducerBase(size_t expectedNumEntries,
                           const ndn::Name& syncPrefix,
                           const ndn::Name& userPrefix,
                           ndn::time::milliseconds syncReplyFreshness,
                           ndn::time::milliseconds helloReplyFreshness)
  : m_iblt(expectedNumEntries)
  , m_expectedNumEntries(expectedNumEntries)
  , m_threshold(expectedNumEntries/2)
  , m_syncPrefix(syncPrefix)
  , m_syncReplyFreshness(syncReplyFreshness)
  , m_helloReplyFreshness(helloReplyFreshness)
  , m_rng(ndn::random::getRandomNumberEngine())
{
  addUserNode(userPrefix);
}

bool
ProducerBase::addUserNode(const ndn::Name& prefix)
{
  if (m_prefixes.find(prefix) == m_prefixes.end()) {
    m_prefixes[prefix] = 0;
    return true;
  }
  else {
    return false;
  }
}

void
ProducerBase::removeUserNode(const ndn::Name& prefix)
{
  auto it = m_prefixes.find(prefix);
  if (it != m_prefixes.end()) {
    uint64_t seqNo = it->second;
    m_prefixes.erase(it);

    removeFromIBF(ndn::Name(prefix).appendNumber(seqNo));
  }
}

void
ProducerBase::updateSeqNo(const ndn::Name& prefix, uint64_t seq)
{
  NDN_LOG_DEBUG("UpdateSeq: " << prefix << " " << seq);

  uint64_t oldSeq;
  auto it = m_prefixes.find(prefix);
  if (it != m_prefixes.end()) {
    oldSeq = it->second;
  }
  else {
    NDN_LOG_WARN("Prefix not found in m_prefixes");
    return;
  }

  if (oldSeq >= seq) {
    NDN_LOG_WARN("Update has lower/equal seq no for prefix, doing nothing!");
    return;
  }

  // Delete the last sequence prefix from the iblt
  // Because we don't insert zeroth prefix in IBF so no need to delete that
  if (oldSeq != 0) {
    removeFromIBF(ndn::Name(prefix).appendNumber(oldSeq));
  }

  // Insert the new seq no
  it->second = seq;
  ndn::Name prefixWithSeq = ndn::Name(prefix).appendNumber(seq);
  insertToIBF(prefixWithSeq);
}

void
ProducerBase::insertToIBF(const ndn::Name& prefix)
{
  uint32_t newHash = murmurHash3(N_HASHCHECK, prefix.toUri());
  m_name2hash[prefix] = newHash;
  m_hash2name[newHash] = prefix;
  m_iblt.insert(newHash);
}

void
ProducerBase::removeFromIBF(const ndn::Name& prefix)
{
  auto hashIt = m_name2hash.find(prefix);
  if (hashIt != m_name2hash.end()) {
    uint32_t hash = hashIt->second;
    m_name2hash.erase(hashIt);
    m_hash2name.erase(hash);
    m_iblt.erase(hash);
  }
}

void
ProducerBase::onRegisterFailed(const ndn::Name& prefix, const std::string& msg) const
{
  NDN_LOG_ERROR("ProduerBase::onRegisterFailed " << prefix << " " << msg);
  BOOST_THROW_EXCEPTION(Error(msg));
}

} // namespace psync
