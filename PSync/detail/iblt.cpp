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

#include "PSync/detail/iblt.hpp"
#include "PSync/detail/util.hpp"

#include <ndn-cxx/util/exception.hpp>

namespace psync::detail {

namespace be = boost::endian;

constexpr size_t ENTRY_SIZE = sizeof(HashTableEntry::count) + sizeof(HashTableEntry::keySum) +
                              sizeof(HashTableEntry::keyCheck);

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
  auto decompressed = decompress(m_compressionScheme, ibltName.value_bytes());
  if (decompressed->size() != ENTRY_SIZE * m_hashTable.size()) {
    NDN_THROW(Error("Received IBF cannot be decoded!"));
  }

  const uint8_t* input = decompressed->data();
  for (auto& entry : m_hashTable) {
    entry.count = be::endian_load<int32_t, sizeof(int32_t), be::order::big>(input);
    input += sizeof(entry.count);

    entry.keySum = be::endian_load<uint32_t, sizeof(uint32_t), be::order::big>(input);
    input += sizeof(entry.keySum);

    entry.keyCheck = be::endian_load<uint32_t, sizeof(uint32_t), be::order::big>(input);
    input += sizeof(entry.keyCheck);
  }
}

static inline void
ibltUpdate(std::vector<HashTableEntry>& ht, int32_t plusOrMinus, uint32_t key)
{
  size_t bucketsPerHash = ht.size() / N_HASH;

  for (size_t i = 0; i < N_HASH; i++) {
    size_t startEntry = i * bucketsPerHash;
    uint32_t h = murmurHash3(i, key);
    HashTableEntry& entry = ht.at(startEntry + (h % bucketsPerHash));
    entry.count += plusOrMinus;
    entry.keySum ^= key;
    entry.keyCheck ^= murmurHash3(N_HASHCHECK, key);
  }
}

void
IBLT::insert(uint32_t key)
{
  ibltUpdate(m_hashTable, 1, key);
}

void
IBLT::erase(uint32_t key)
{
  ibltUpdate(m_hashTable, -1, key);
}

void
IBLT::appendToName(ndn::Name& name) const
{
  std::vector<uint8_t> buffer(ENTRY_SIZE * m_hashTable.size());
  uint8_t* output = buffer.data();
  for (const auto& entry : m_hashTable) {
    be::endian_store<int32_t, sizeof(int32_t), be::order::big>(output, entry.count);
    output += sizeof(entry.count);

    be::endian_store<uint32_t, sizeof(uint32_t), be::order::big>(output, entry.keySum);
    output += sizeof(entry.keySum);

    be::endian_store<uint32_t, sizeof(uint32_t), be::order::big>(output, entry.keyCheck);
    output += sizeof(entry.keyCheck);
  }

  auto compressed = compress(m_compressionScheme, buffer);
  name.append(ndn::name::Component(std::move(compressed)));
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

IBLTDiff
operator-(const IBLT& lhs, const IBLT& rhs)
{
  const auto& lht = lhs.getHashTable();
  const auto& rht = rhs.getHashTable();
  BOOST_ASSERT(lht.size() == rht.size());

  std::vector<HashTableEntry> peeled(lht.size());
  std::transform(lht.begin(), lht.end(), rht.begin(), peeled.begin(),
    [] (const HashTableEntry& lhe, const HashTableEntry& rhe) {
      HashTableEntry diff;
      diff.count = lhe.count - rhe.count;
      diff.keySum = lhe.keySum ^ rhe.keySum;
      diff.keyCheck = lhe.keyCheck ^ rhe.keyCheck;
      return diff;
    }
  );

  std::set<uint32_t> positive;
  std::set<uint32_t> negative;

  size_t nErased = 0;
  do {
    nErased = 0;
    for (const auto& entry : peeled) {
      if (entry.isPure()) {
        if (entry.count == 1) {
          positive.insert(entry.keySum);
        }
        else {
          negative.insert(entry.keySum);
        }
        ibltUpdate(peeled, -entry.count, entry.keySum);
        ++nErased;
      }
    }
  } while (nErased > 0);

  // If any buckets for one of the hash functions is not empty,
  // then we didn't peel them all:
  bool canDecode = std::all_of(peeled.begin(), peeled.end(),
                               [] (const HashTableEntry& entry) { return entry.isEmpty(); });
  return {canDecode, std::move(positive), std::move(negative)};
}

} // namespace psync::detail
