/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2019,  The University of Memphis
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

#include <ndn-cxx/util/logger.hpp>
#include "PSync/detail/user-prefixes.hpp"

namespace psync {

NDN_LOG_INIT(psync.UserPrefixes);

bool
UserPrefixes::addUserNode(const ndn::Name& prefix)
{
  if (!isUserNode(prefix)) {
    m_prefixes[prefix] = 0;
    return true;
  }
  else {
    return false;
  }
}

void
UserPrefixes::removeUserNode(const ndn::Name& prefix)
{
  auto it = m_prefixes.find(prefix);
  if (it != m_prefixes.end()) {
    m_prefixes.erase(it);
  }
}

bool
UserPrefixes::updateSeqNo
  (const ndn::Name& prefix, uint64_t seqNo, uint64_t& oldSeqNo)
{
  oldSeqNo = 0;
  NDN_LOG_DEBUG("UpdateSeq: " << prefix << " " << seqNo);

  auto it = m_prefixes.find(prefix);
  if (it != m_prefixes.end()) {
    oldSeqNo = it->second;
  }
  else {
    NDN_LOG_WARN("Prefix not found in m_prefixes");
    return false;
  }

  if (oldSeqNo >= seqNo) {
    NDN_LOG_WARN("Update has lower/equal seq no for prefix, doing nothing!");
    return false;
  }

  // Insert the new sequence number
  it->second = seqNo;
  return true;
}

} // namespace psync
