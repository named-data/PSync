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

#include "PSync/full-producer.hpp"

#include <ndn-cxx/util/logger.hpp>
#include <ndn-cxx/util/segment-fetcher.hpp>
#include <ndn-cxx/security/validator-null.hpp>

#include <cstring>
#include <limits>
#include <functional>

namespace psync {

NDN_LOG_INIT(psync.FullProducer);

FullProducer::FullProducer(const size_t expectedNumEntries,
                           ndn::Face& face,
                           const ndn::Name& syncPrefix,
                           const ndn::Name& userPrefix,
                           const UpdateCallback& onUpdateCallBack,
                           ndn::time::milliseconds syncInterestLifetime,
                           ndn::time::milliseconds syncReplyFreshness)
  : m_producerArbitrary(expectedNumEntries, face, syncPrefix,
                        [this] (const std::vector<ndn::Name>& names) {
                          arbitraryUpdateCallBack(names);
                        },
                        syncInterestLifetime, syncReplyFreshness,
                        [this] (const ndn::Name& prefix, const std::set<uint32_t>& negative) {
                          return isNotFutureHash(prefix, negative);
                        },
                        [this] (const ndn::Name& name) {
                          ndn::Name prefix = name.getPrefix(-1);
                          uint64_t seq = name.get(-1).toNumber();

                          if (m_prefixes.m_prefixes.find(prefix) == m_prefixes.m_prefixes.end() ||
                              m_prefixes.m_prefixes[prefix] < seq) {
                            updateSeqNo(prefix, seq);
                          }
                        })
  , m_onUpdateCallback(onUpdateCallBack)
{
  addUserNode(userPrefix);
}

void
FullProducer::publishName(const ndn::Name& prefix, ndn::optional<uint64_t> seq)
{
  if (!m_prefixes.isUserNode(prefix)) {
    return;
  }

  uint64_t newSeq = seq.value_or(m_prefixes.m_prefixes[prefix] + 1);

  NDN_LOG_INFO("Publish: " << prefix << "/" << newSeq);

  updateSeqNo(prefix, newSeq);

  m_producerArbitrary.satisfyPendingInterests();
}

void
FullProducer::updateSeqNo(const ndn::Name& prefix, uint64_t seq)
{
  uint64_t oldSeq;
  if (!m_prefixes.updateSeqNo(prefix, seq, oldSeq))
    return;  // Delete the last sequence prefix from the iblt
  // Because we don't insert zeroth prefix in IBF so no need to delete that
  if (oldSeq != 0) {
    m_producerArbitrary.removeName(ndn::Name(prefix).appendNumber(oldSeq));
  }  // Insert the new seq no
  ndn::Name prefixWithSeq = ndn::Name(prefix).appendNumber(seq);
  m_producerArbitrary.addName(prefixWithSeq);
}

bool
FullProducer::isNotFutureHash(const ndn::Name& prefix, const std::set<uint32_t>& negative)
{
  uint32_t nextHash = murmurHash3(N_HASHCHECK,
                                  ndn::Name(prefix).appendNumber(m_prefixes.m_prefixes[prefix] + 1).toUri());
  for (const auto& nHash : negative) {
    if (nHash == nextHash) {
      return false;
      break;
    }
  }
  return true;
}

void
FullProducer::arbitraryUpdateCallBack(const std::vector<ndn::Name>& names)
{
  std::vector<MissingDataInfo> updates;

  for (const auto& name : names) {
    ndn::Name prefix = name.getPrefix(-1);
    uint64_t seq = name.get(-1).toNumber();

    if (m_prefixes.m_prefixes.find(prefix) == m_prefixes.m_prefixes.end() ||
        m_prefixes.m_prefixes[prefix] < seq) {
      updates.push_back(MissingDataInfo{prefix, m_prefixes.m_prefixes[prefix] + 1, seq});
    }
  }

  m_onUpdateCallback(updates);
}

} // namespace psync
