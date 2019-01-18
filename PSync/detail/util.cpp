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
 *
 * murmurHash3 was written by Austin Appleby, and is placed in the public
 * domain. The author hereby disclaims copyright to this source code.
 * https://github.com/aappleby/smhasher/blob/master/src/murmurHash3.cpp
 **/

#include "PSync/detail/util.hpp"

#include <ndn-cxx/util/backports.hpp>

namespace psync {

static uint32_t
ROTL32 ( uint32_t x, int8_t r )
{
  return (x << r) | (x >> (32 - r));
}

uint32_t
murmurHash3(uint32_t nHashSeed, const std::vector<unsigned char>& vDataToHash)
{
  uint32_t h1 = nHashSeed;
  const uint32_t c1 = 0xcc9e2d51;
  const uint32_t c2 = 0x1b873593;

  const size_t nblocks = vDataToHash.size() / 4;

  //----------
  // body
  const uint32_t * blocks = (const uint32_t *)(&vDataToHash[0] + nblocks*4);

  for (size_t i = -nblocks; i; i++) {
    uint32_t k1 = blocks[i];

    k1 *= c1;
    k1 = ROTL32(k1,15);
    k1 *= c2;

    h1 ^= k1;
    h1 = ROTL32(h1,13);
    h1 = h1*5+0xe6546b64;
  }

  //----------
  // tail
  const uint8_t * tail = (const uint8_t*)(&vDataToHash[0] + nblocks*4);

  uint32_t k1 = 0;

  switch (vDataToHash.size() & 3) {
    case 3:
      k1 ^= tail[2] << 16;
      NDN_CXX_FALLTHROUGH;

    case 2:
      k1 ^= tail[1] << 8;
      NDN_CXX_FALLTHROUGH;

    case 1:
      k1 ^= tail[0];
      k1 *= c1; k1 = ROTL32(k1,15); k1 *= c2; h1 ^= k1;
  }

  //----------
  // finalization
  h1 ^= vDataToHash.size();
  h1 ^= h1 >> 16;
  h1 *= 0x85ebca6b;
  h1 ^= h1 >> 13;
  h1 *= 0xc2b2ae35;
  h1 ^= h1 >> 16;

  return h1;
}

uint32_t
murmurHash3(uint32_t nHashSeed, const std::string& str)
{
  return murmurHash3(nHashSeed, std::vector<unsigned char>(str.begin(), str.end()));
}

uint32_t
murmurHash3(uint32_t nHashSeed, uint32_t value)
{
  return murmurHash3(nHashSeed,
                     std::vector<unsigned char>((unsigned char*)&value,
                                                (unsigned char*)&value + sizeof(uint32_t)));
}

} // namespace psync
