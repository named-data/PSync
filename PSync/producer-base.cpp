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

#include "PSync/producer-base.hpp"
#include "PSync/detail/util.hpp"

#include <ndn-cxx/util/exception.hpp>
#include <ndn-cxx/util/logger.hpp>

#include <cstring>
#include <limits>

namespace psync {

NDN_LOG_INIT(psync.ProducerBase);

ProducerBase::ProducerBase(ndn::Face& face,
                           ndn::KeyChain& keyChain,
                           size_t expectedNumEntries,
                           const ndn::Name& syncPrefix,
                           const ndn::Name& userPrefix,
                           ndn::time::milliseconds syncReplyFreshness,
                           CompressionScheme ibltCompression,
                           CompressionScheme contentCompression)
  : m_face(face)
  , m_keyChain(keyChain)
  , m_scheduler(m_face.getIoService())
  , m_rng(ndn::random::getRandomNumberEngine())
  , m_iblt(expectedNumEntries, ibltCompression)
  , m_segmentPublisher(m_face, m_keyChain)
  , m_expectedNumEntries(expectedNumEntries)
  , m_threshold(expectedNumEntries / 2)
  , m_syncPrefix(syncPrefix)
  , m_userPrefix(userPrefix)
  , m_syncReplyFreshness(syncReplyFreshness)
  , m_ibltCompression(ibltCompression)
  , m_contentCompression(contentCompression)
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

    ndn::Name prefixWithSeq = ndn::Name(prefix).appendNumber(seqNo);
    auto hashIt = m_biMap.right.find(prefixWithSeq);
    if (hashIt != m_biMap.right.end()) {
      m_iblt.erase(hashIt->second);
      m_biMap.right.erase(hashIt);
    }
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
    ndn::Name prefixWithSeq = ndn::Name(prefix).appendNumber(oldSeq);
    auto hashIt = m_biMap.right.find(prefixWithSeq);
    if (hashIt != m_biMap.right.end()) {
      m_iblt.erase(hashIt->second);
      m_biMap.right.erase(hashIt);
    }
  }

  // Insert the new seq no in m_prefixes, m_biMap, and m_iblt
  it->second = seq;
  ndn::Name prefixWithSeq = ndn::Name(prefix).appendNumber(seq);
  auto newHash = detail::murmurHash3(detail::N_HASHCHECK, prefixWithSeq);
  m_biMap.insert({newHash, prefixWithSeq});
  m_iblt.insert(newHash);
}

void
ProducerBase::sendApplicationNack(const ndn::Name& name)
{
  NDN_LOG_DEBUG("Sending application nack");
  ndn::Name dataName(name);
  m_iblt.appendToName(dataName);

  dataName.appendSegment(0);
  ndn::Data data(dataName);
  data.setFreshnessPeriod(m_syncReplyFreshness);
  data.setContentType(ndn::tlv::ContentType_Nack);
  data.setFinalBlock(dataName[-1]);
  m_keyChain.sign(data);
  m_face.put(data);
}

void
ProducerBase::onRegisterFailed(const ndn::Name& prefix, const std::string& msg) const
{
  NDN_LOG_ERROR("ProducerBase::onRegisterFailed(" << prefix << "): " << msg);
  NDN_THROW(Error(msg));
}

} // namespace psync
