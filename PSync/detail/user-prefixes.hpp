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

#ifndef PSYNC_USER_PREFIXES_HPP
#define PSYNC_USER_PREFIXES_HPP

#include <map>
#include <ndn-cxx/name.hpp>

namespace psync {

/**
 * @brief UserPrefixes holds the m_prefixes map from prefix to sequence number,
 * used by PartialProducer and FullProducer.
 *
 * Contains code common to both
 */
class UserPrefixes
{
public:
  /**
   * @brief Check if the prefix is in m_prefixes.
   *
   * @param prefix The prefix to check.
   * @return True if the prefix is in m_prefixes.
   */
  bool
  isUserNode(const ndn::Name& prefix) const
  {
    return m_prefixes.find(prefix) != m_prefixes.end();
  }

  /**
   * @brief Returns the current sequence number of the given prefix
   *
   * @param prefix prefix to get the sequence number of
   */
  ndn::optional<uint64_t>
  getSeqNo(const ndn::Name& prefix) const
  {
    auto it = m_prefixes.find(prefix);
    if (it == m_prefixes.end()) {
      return ndn::nullopt;
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
   * @return True if the prefix was added, false if the prefix was already in
   * m_prefixes.
   */
  bool
  addUserNode(const ndn::Name& prefix);

  /**
   * @brief Remove the user node from synchronization. If the prefix is not in
   * m_prefixes, then do nothing.
   *
   * The caller should first check isUserNode(prefix) and erase the prefix from
   * the IBF and other maps if needed.
   *
   * @param prefix the user node to be removed
   */
  void
  removeUserNode(const ndn::Name& prefix);

  // prefix and sequence number
  std::map <ndn::Name, uint64_t> m_prefixes;
};

} // namespace psync

#endif // PSYNC_USER_PREFIXES_HPP
