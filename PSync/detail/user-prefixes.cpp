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

#include "PSync/detail/user-prefixes.hpp"

namespace psync {

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
    uint64_t seqNo = it->second;
    m_prefixes.erase(it);
  }
}

} // namespace psync
