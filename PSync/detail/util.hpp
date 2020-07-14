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
 * murmurHash3 was written by Austin Appleby, and is placed in the public
 * domain. The author hereby disclaims copyright to this source code.
 * https://github.com/aappleby/smhasher/blob/master/src/murmurHash3.cpp
 */

#ifndef PSYNC_UTIL_HPP
#define PSYNC_UTIL_HPP

#include "PSync/detail/config.hpp"

#include <ndn-cxx/name.hpp>

#include <inttypes.h>
#include <vector>
#include <string>

namespace psync {

uint32_t
murmurHash3(uint32_t nHashSeed, const std::vector<unsigned char>& vDataToHash);

uint32_t
murmurHash3(uint32_t nHashSeed, const std::string& str);

uint32_t
murmurHash3(uint32_t nHashSeed, uint32_t value);

struct MissingDataInfo
{
  MissingDataInfo(const ndn::Name& prefix, uint64_t lowSeq, uint64_t highSeq)
    : prefix(prefix)
    , lowSeq(lowSeq)
    , highSeq(highSeq)
  {}

  ndn::Name prefix;
  uint64_t lowSeq;
  uint64_t highSeq;
};

enum class CompressionScheme {
  NONE,
  ZLIB,
  GZIP,
  BZIP2,
  LZMA,
  ZSTD,
#ifdef PSYNC_HAVE_ZLIB
  DEFAULT = ZLIB
#else
  DEFAULT = NONE
#endif
};

std::shared_ptr<ndn::Buffer>
compress(CompressionScheme scheme, const uint8_t* buffer, size_t bufferSize);

std::shared_ptr<ndn::Buffer>
decompress(CompressionScheme scheme, const uint8_t* buffer, size_t bufferSize);

class Error : public std::runtime_error
{
public:
  using std::runtime_error::runtime_error;
};

} // namespace psync

#endif // PSYNC_UTIL_HPP
