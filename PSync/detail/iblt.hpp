/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2020,  The University of Memphis
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

#ifndef PSYNC_IBLT_HPP
#define PSYNC_IBLT_HPP

#include "PSync/detail/util.hpp"

#include <ndn-cxx/name.hpp>

#include <inttypes.h>
#include <set>
#include <vector>
#include <string>

namespace psync {

class HashTableEntry
{
public:
  int32_t count;
  uint32_t keySum;
  uint32_t keyCheck;

  bool
  isPure() const;

  bool
  isEmpty() const;
};

extern const size_t N_HASH;
extern const size_t N_HASHCHECK;

/**
 * @brief Invertible Bloom Lookup Table (Invertible Bloom Filter)
 *
 * Used by Partial Sync (PartialProducer) and Full Sync (Full Producer)
 */
class IBLT
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
  IBLT(size_t expectedNumEntries, CompressionScheme scheme = CompressionScheme::ZLIB);

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
   * @param positive
   * @param negative
   * @return true if decoding is complete successfully
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
   * @brief Appends self to name
   *
   * Encodes our hash table from uint32_t vector to uint8_t vector
   * We create a uin8_t vector 12 times the size of uint32_t vector
   * We put the first count in first 4 cells, keySum in next 4, and keyCheck in next 4.
   * Repeat for all the other cells of the hash table.
   * Then we append this uint8_t vector to the name.
   *
   * @param name
   */
  void
  appendToName(ndn::Name& name) const;

  /**
   * @brief Extracts IBLT from name component
   *
   * Converts the name into a uint8_t vector which is then decoded to a
   * a uint32_t vector.
   *
   * @param ibltName IBLT represented as a Name Component
   * @return a uint32_t vector representing the hash table of the IBLT
   */
  std::vector<uint32_t>
  extractValueFromName(const ndn::name::Component& ibltName) const;

private:
  void
  update(int plusOrMinus, uint32_t key);

private:
  std::vector<HashTableEntry> m_hashTable;
  static const int INSERT = 1;
  static const int ERASE = -1;
  CompressionScheme m_compressionScheme;
};

bool
operator==(const IBLT& iblt1, const IBLT& iblt2);

bool
operator!=(const IBLT& iblt1, const IBLT& iblt2);

std::ostream&
operator<<(std::ostream& out, const IBLT& iblt);

} // namespace psync

#endif // PSYNC_IBLT_HPP
