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
 * @brief UserPrefixes holds the prefixes map from prefix to sequence number,
 * used by PartialProducer and FullProducer.
 *
 * Contains code common to both
 */
class UserPrefixes
{
public:
  /**
   * @brief Check if the prefix is in prefixes.
   *
   * @param prefix The prefix to check.
   * @return True if the prefix is in prefixes.
   */
  bool
  isUserNode(const ndn::Name& prefix) const
  {
    return prefixes.find(prefix) != prefixes.end();
  }

  /**
   * @brief Returns the current sequence number of the given prefix
   *
   * @param prefix prefix to get the sequence number of
   */
  ndn::optional<uint64_t>
  getSeqNo(const ndn::Name& prefix) const
  {
    auto it = prefixes.find(prefix);
    if (it == prefixes.end()) {
      return ndn::nullopt;
    }
    return it->second;
  }

  /**
   * @brief Adds a user node for synchronization
   *
   * Initializes prefixes[prefix] to zero
   *
   * @param prefix the user node to be added
   * @return true if the prefix was added, false if the prefix was already in
   * prefixes.
   */
  bool
  addUserNode(const ndn::Name& prefix);

  /**
   * @brief Remove the user node from synchronization. If the prefix is not in
   * prefixes, then do nothing.
   *
   * The caller should first check isUserNode(prefix) and erase the prefix from
   * the IBLT and other maps if needed.
   *
   * @param prefix the user node to be removed
   */
  void
  removeUserNode(const ndn::Name& prefix);

  /**
   * @brief Update prefixes with the given prefix and sequence number. This
   * does not update the IBLT. This logs a message for the update.
   *
   * Whoever calls this needs to make sure that isUserNode(prefix) is true.
   *
   * @param prefix the prefix of the update
   * @param seqNo the sequence number of the update
   * @param oldSeqNo This sets oldSeqNo to the old sequence number for the
   * prefix. If this method returns true and oldSeqNo is not zero, the caller
   * can remove the old prefix from the IBLT.
   * @return True if the sequence number was updated, false if the prefix was
   * not in prefixes, or if the seqNo is less than or equal to the old
   * sequence number. If this returns false, the caller should not update the
   * IBLT.
   */
  bool
  updateSeqNo(const ndn::Name& prefix, uint64_t seqNo, uint64_t& oldSeqNo);

  // prefix and sequence number
  std::map <ndn::Name, uint64_t> prefixes;
};

} // namespace psync

#endif // PSYNC_USER_PREFIXES_HPP
