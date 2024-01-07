/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2024,  The University of Memphis
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
 *

 * This file incorporates work covered by the following copyright and
 * permission notice:

 * The MIT License (MIT)

 * Copyright (c) 2014 Gavin Andresen

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/

#ifndef PSYNC_DETAIL_IBLT_HPP
#define PSYNC_DETAIL_IBLT_HPP

#include "PSync/common.hpp"

#include <ndn-cxx/name.hpp>

#include <boost/operators.hpp>

#include <set>
#include <string>

namespace psync::detail {

class HashTableEntry : private boost::equality_comparable<HashTableEntry>
{
public:
  bool
  isPure() const;

  bool
  isEmpty() const;

private: // non-member operators
  // NOTE: the following "hidden friend" operators are available via
  //       argument-dependent lookup only and must be defined inline.
  // boost::equality_comparable provides != operator.

  friend bool
  operator==(const HashTableEntry& lhs, const HashTableEntry& rhs) noexcept
  {
    return lhs.count == rhs.count && lhs.keySum == rhs.keySum && lhs.keyCheck == rhs.keyCheck;
  }

public:
  int32_t count = 0;
  uint32_t keySum = 0;
  uint32_t keyCheck = 0;
};

inline constexpr size_t N_HASH = 3;
inline constexpr size_t N_HASHCHECK = 11;

/**
 * @brief Invertible Bloom Lookup Table (Invertible Bloom Filter)
 *
 * Used by Partial Sync (PartialProducer) and Full Sync (Full Producer)
 */
class IBLT : private boost::equality_comparable<IBLT>
{
public:
  class Error : public std::runtime_error
  {
  public:
    using std::runtime_error::runtime_error;
  };

  /**
   * @brief constructor
   *
   * @param expectedNumEntries the expected number of entries in the IBLT
   * @param scheme compression scheme to be used for the IBLT
   */
  explicit
  IBLT(size_t expectedNumEntries, CompressionScheme scheme);

  /**
   * @brief Populate the hash table using the vector representation of IBLT
   *
   * @param ibltName the Component representation of IBLT
   * @throws Error if size of values is not compatible with this IBF
   */
  void
  initialize(const ndn::name::Component& ibltName);

  void
  insert(uint32_t key);

  void
  erase(uint32_t key);

  /**
   * @brief List all the entries in the IBLT
   *
   * This is called on a difference of two IBLTs: ownIBLT - rcvdIBLT
   * Entries listed in positive are in ownIBLT but not in rcvdIBLT
   * Entries listed in negative are in rcvdIBLT but not in ownIBLT
   *
   * @return whether decoding completed successfully
   */
  bool
  listEntries(std::set<uint32_t>& positive, std::set<uint32_t>& negative) const;

  IBLT
  operator-(const IBLT& other) const;

  const std::vector<HashTableEntry>&
  getHashTable() const
  {
    return m_hashTable;
  }

  /**
   * @brief Appends self to @p name
   *
   * Encodes our hash table from uint32_t vector to uint8_t vector
   * We create a uin8_t vector 12 times the size of uint32_t vector
   * We put the first count in first 4 cells, keySum in next 4, and keyCheck in next 4.
   * Repeat for all the other cells of the hash table.
   * Then we append this uint8_t vector to the name.
   */
  void
  appendToName(ndn::Name& name) const;

private:
  void
  update(int plusOrMinus, uint32_t key);

private: // non-member operators
  // NOTE: the following "hidden friend" operators are available via
  //       argument-dependent lookup only and must be defined inline.
  // boost::equality_comparable provides != operator.

  friend bool
  operator==(const IBLT& lhs, const IBLT& rhs)
  {
    return lhs.m_hashTable == rhs.m_hashTable;
  }

private:
  std::vector<HashTableEntry> m_hashTable;
  static constexpr int INSERT = 1;
  static constexpr int ERASE = -1;
  CompressionScheme m_compressionScheme;
};

std::ostream&
operator<<(std::ostream& os, const IBLT& iblt);

} // namespace psync::detail

#endif // PSYNC_DETAIL_IBLT_HPP
