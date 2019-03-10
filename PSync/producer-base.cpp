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
                           ndn::time::milliseconds syncReplyFreshness)
  : m_iblt(expectedNumEntries)
  , m_expectedNumEntries(expectedNumEntries)
  , m_threshold(expectedNumEntries/2)
  , m_syncPrefix(syncPrefix)
  , m_syncReplyFreshness(syncReplyFreshness)
  , m_rng(ndn::random::getRandomNumberEngine())
{
}

void
ProducerBase::insertToIBF(const ndn::Name& name)
{
  uint32_t newHash = murmurHash3(N_HASHCHECK, name.toUri());
  m_name2hash[name] = newHash;
  m_hash2name[newHash] = name;
  m_iblt.insert(newHash);
}

void
ProducerBase::removeFromIBF(const ndn::Name& name)
{
  auto hashIt = m_name2hash.find(name);
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
