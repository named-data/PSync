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

#include "PSync/detail/iblt.hpp"
#include "PSync/detail/util.hpp"

#include <ndn-cxx/util/exception.hpp>

namespace psync {
namespace detail {

namespace be = boost::endian;

const size_t N_HASH(3);
const size_t N_HASHCHECK(11);

bool
HashTableEntry::isPure() const
{
  if (count == 1 || count == -1) {
    uint32_t check = murmurHash3(N_HASHCHECK, keySum);
    return keyCheck == check;
  }

  return false;
}

bool
HashTableEntry::isEmpty() const
{
  return count == 0 && keySum == 0 && keyCheck == 0;
}

IBLT::IBLT(size_t expectedNumEntries, CompressionScheme scheme)
  : m_compressionScheme(scheme)
{
  // 1.5x expectedNumEntries gives very low probability of decoding failure
  size_t nEntries = expectedNumEntries + expectedNumEntries / 2;
  // make nEntries exactly divisible by N_HASH
  size_t remainder = nEntries % N_HASH;
  if (remainder != 0) {
    nEntries += (N_HASH - remainder);
  }

  m_hashTable.resize(nEntries);
}

void
IBLT::initialize(const ndn::name::Component& ibltName)
{
  const auto& values = extractValueFromName(ibltName);

  if (3 * m_hashTable.size() != values.size()) {
    NDN_THROW(Error("Received IBF cannot be decoded!"));
  }

  for (size_t i = 0; i < m_hashTable.size(); i++) {
    HashTableEntry& entry = m_hashTable.at(i);
    if (values[i * 3] != 0) {
      entry.count = values[i * 3];
      entry.keySum = values[(i * 3) + 1];
      entry.keyCheck = values[(i * 3) + 2];
    }
  }
}

void
IBLT::update(int plusOrMinus, uint32_t key)
{
  size_t bucketsPerHash = m_hashTable.size() / N_HASH;

  for (size_t i = 0; i < N_HASH; i++) {
    size_t startEntry = i * bucketsPerHash;
    uint32_t h = murmurHash3(i, key);
    HashTableEntry& entry = m_hashTable.at(startEntry + (h % bucketsPerHash));
    entry.count += plusOrMinus;
    entry.keySum ^= key;
    entry.keyCheck ^= murmurHash3(N_HASHCHECK, key);
  }
}

void
IBLT::insert(uint32_t key)
{
  update(INSERT, key);
}

void
IBLT::erase(uint32_t key)
{
  update(ERASE, key);
}

bool
IBLT::listEntries(std::set<uint32_t>& positive, std::set<uint32_t>& negative) const
{
  IBLT peeled = *this;

  size_t nErased = 0;
  do {
    nErased = 0;
    for (const auto& entry : peeled.m_hashTable) {
      if (entry.isPure()) {
        if (entry.count == 1) {
          positive.insert(entry.keySum);
        }
        else {
          negative.insert(entry.keySum);
        }
        peeled.update(-entry.count, entry.keySum);
        ++nErased;
      }
    }
  } while (nErased > 0);

  // If any buckets for one of the hash functions is not empty,
  // then we didn't peel them all:
  for (const auto& entry : peeled.m_hashTable) {
    if (entry.isEmpty() != true) {
      return false;
    }
  }

  return true;
}

IBLT
IBLT::operator-(const IBLT& other) const
{
  BOOST_ASSERT(m_hashTable.size() == other.m_hashTable.size());

  IBLT result(*this);
  for (size_t i = 0; i < m_hashTable.size(); i++) {
    HashTableEntry& e1 = result.m_hashTable.at(i);
    const HashTableEntry& e2 = other.m_hashTable.at(i);
    e1.count -= e2.count;
    e1.keySum ^= e2.keySum;
    e1.keyCheck ^= e2.keyCheck;
  }

  return result;
}

void
IBLT::appendToName(ndn::Name& name) const
{
  constexpr size_t unitSize = sizeof(m_hashTable[0].count) +
                              sizeof(m_hashTable[0].keySum) +
                              sizeof(m_hashTable[0].keyCheck);

  size_t tableSize = unitSize * m_hashTable.size();
  std::vector<uint8_t> table(tableSize);

  for (size_t i = 0; i < m_hashTable.size(); i++) {
    uint32_t count    = be::native_to_big(static_cast<uint32_t>(m_hashTable[i].count));
    uint32_t keySum   = be::native_to_big(static_cast<uint32_t>(m_hashTable[i].keySum));
    uint32_t keyCheck = be::native_to_big(static_cast<uint32_t>(m_hashTable[i].keyCheck));

    std::memcpy(&table[i * unitSize], &count, sizeof(count));
    std::memcpy(&table[(i * unitSize) + 4], &keySum, sizeof(keySum));
    std::memcpy(&table[(i * unitSize) + 8], &keyCheck, sizeof(keyCheck));
  }

  auto compressed = compress(m_compressionScheme, table.data(), table.size());
  name.append(ndn::name::Component(std::move(compressed)));
}

std::vector<uint32_t>
IBLT::extractValueFromName(const ndn::name::Component& ibltName) const
{
  auto decompressedBuf = decompress(m_compressionScheme, ibltName.value(), ibltName.value_size());

  if (decompressedBuf->size() % 4 != 0) {
    NDN_THROW(Error("Received IBF cannot be decompressed correctly!"));
  }

  size_t n = decompressedBuf->size() / 4;
  std::vector<uint32_t> values(n, 0);

  for (size_t i = 0; i < n; i++) {
    uint32_t t;
    std::memcpy(&t, &(*decompressedBuf)[i * 4], sizeof(t));
    values[i] = be::big_to_native(t);
  }

  return values;
}

bool
operator==(const IBLT& iblt1, const IBLT& iblt2)
{
  auto iblt1HashTable = iblt1.getHashTable();
  auto iblt2HashTable = iblt2.getHashTable();
  if (iblt1HashTable.size() != iblt2HashTable.size()) {
    return false;
  }

  size_t N = iblt1HashTable.size();

  for (size_t i = 0; i < N; i++) {
    if (iblt1HashTable[i].count != iblt2HashTable[i].count ||
        iblt1HashTable[i].keySum != iblt2HashTable[i].keySum ||
        iblt1HashTable[i].keyCheck != iblt2HashTable[i].keyCheck)
      return false;
  }

  return true;
}

bool
operator!=(const IBLT& iblt1, const IBLT& iblt2)
{
  return !(iblt1 == iblt2);
}

std::ostream&
operator<<(std::ostream& os, const IBLT& iblt)
{
  os << "count keySum keyCheckMatch\n";
  for (const auto& entry : iblt.getHashTable()) {
    os << entry.count << " " << entry.keySum << " "
       << ((entry.isEmpty() || murmurHash3(N_HASHCHECK, entry.keySum) == entry.keyCheck) ? "true" : "false")
       << "\n";
  }
  return os;
}

} // namespace detail
} // namespace psync
