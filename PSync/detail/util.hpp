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

#ifndef PSYNC_DETAIL_UTIL_HPP
#define PSYNC_DETAIL_UTIL_HPP

#include "PSync/common.hpp"

#include <ndn-cxx/encoding/buffer.hpp>

#include <string>

namespace psync {
namespace detail {

uint32_t
murmurHash3(const void* key, size_t len, uint32_t seed);

/**
 * @brief Compute 32-bit MurmurHash3 of Name TLV-VALUE.
 */
uint32_t
murmurHash3(uint32_t seed, const ndn::Name& name);

inline uint32_t
murmurHash3(uint32_t seed, uint32_t value)
{
  return murmurHash3(&value, sizeof(value), seed);
}

std::shared_ptr<ndn::Buffer>
compress(CompressionScheme scheme, const uint8_t* buffer, size_t bufferSize);

std::shared_ptr<ndn::Buffer>
decompress(CompressionScheme scheme, const uint8_t* buffer, size_t bufferSize);

} // namespace detail
} // namespace psync

#endif // PSYNC_DETAIL_UTIL_HPP
