/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2022,  The University of Memphis
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
 */

#ifndef PSYNC_COMMON_HPP
#define PSYNC_COMMON_HPP

#include "PSync/detail/config.hpp"

#include <ndn-cxx/name.hpp>
#include <ndn-cxx/util/time.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>

namespace psync {

using namespace ndn::time_literals;

const ndn::time::milliseconds HELLO_INTEREST_LIFETIME = 1_s;
const ndn::time::milliseconds HELLO_REPLY_FRESHNESS = 1_s;
const ndn::time::milliseconds SYNC_INTEREST_LIFETIME = 1_s;
const ndn::time::milliseconds SYNC_REPLY_FRESHNESS = 1_s;

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

class CompressionError : public std::runtime_error
{
public:
  using std::runtime_error::runtime_error;
};

struct MissingDataInfo
{
  ndn::Name prefix;
  uint64_t lowSeq;
  uint64_t highSeq;
  uint64_t incomingFace;
};

using UpdateCallback = std::function<void(const std::vector<MissingDataInfo>&)>;

} // namespace psync

#endif // PSYNC_COMMON_HPP
