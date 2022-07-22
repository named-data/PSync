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
 *
 * murmurHash3 was written by Austin Appleby, and is placed in the public
 * domain. The author hereby disclaims copyright to this source code.
 * https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
 */

#include "PSync/detail/util.hpp"

#include <ndn-cxx/encoding/buffer-stream.hpp>
#include <ndn-cxx/util/backports.hpp>
#include <ndn-cxx/util/exception.hpp>

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#ifdef PSYNC_HAVE_ZLIB
  #include <boost/iostreams/filter/zlib.hpp>
#endif
#ifdef PSYNC_HAVE_GZIP
  #include <boost/iostreams/filter/gzip.hpp>
#endif
#ifdef PSYNC_HAVE_BZIP2
  #include <boost/iostreams/filter/bzip2.hpp>
#endif
#ifdef PSYNC_HAVE_LZMA
  #include <boost/iostreams/filter/lzma.hpp>
#endif
#ifdef PSYNC_HAVE_ZSTD
  #include <boost/iostreams/filter/zstd.hpp>
#endif

namespace psync::detail {

namespace bio = boost::iostreams;

static inline uint32_t
ROTL32(uint32_t x, int8_t r)
{
  return (x << r) | (x >> (32 - r));
}

uint32_t
murmurHash3(const void* key, size_t len, uint32_t seed)
{
  const uint8_t * data = (const uint8_t*)key;
  const int nblocks = len / 4;

  uint32_t h1 = seed;

  const uint32_t c1 = 0xcc9e2d51;
  const uint32_t c2 = 0x1b873593;

  //----------
  // body

  const uint32_t * blocks = (const uint32_t *)(data + nblocks*4);

  for (int i = -nblocks; i; i++)
  {
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

  const uint8_t * tail = (const uint8_t*)(data + nblocks*4);

  uint32_t k1 = 0;

  switch (len & 3)
  {
    case 3: k1 ^= tail[2] << 16;
            [[fallthrough]];
    case 2: k1 ^= tail[1] << 8;
            [[fallthrough]];
    case 1: k1 ^= tail[0];
            k1 *= c1; k1 = ROTL32(k1,15); k1 *= c2; h1 ^= k1;
  }

  //----------
  // finalization

  h1 ^= len;
  h1 ^= h1 >> 16;
  h1 *= 0x85ebca6b;
  h1 ^= h1 >> 13;
  h1 *= 0xc2b2ae35;
  h1 ^= h1 >> 16;

  return h1;
}

uint32_t
murmurHash3(uint32_t seed, const ndn::Name& name)
{
  const auto& wire = name.wireEncode();
  return murmurHash3(wire.value(), wire.value_size(), seed);
}

std::shared_ptr<ndn::Buffer>
compress(CompressionScheme scheme, ndn::span<const uint8_t> buffer)
{
  ndn::OBufferStream out;
  bio::filtering_streambuf<bio::input> in;

  switch (scheme) {
    case CompressionScheme::ZLIB:
#ifdef PSYNC_HAVE_ZLIB
      in.push(bio::zlib_compressor(bio::zlib::best_compression));
      break;
#else
      NDN_THROW(CompressionError("ZLIB compression not supported!"));
#endif

    case CompressionScheme::GZIP:
#ifdef PSYNC_HAVE_GZIP
      in.push(bio::gzip_compressor(bio::gzip::best_compression));
      break;
#else
      NDN_THROW(CompressionError("GZIP compression not supported!"));
#endif

    case CompressionScheme::BZIP2:
#ifdef PSYNC_HAVE_BZIP2
      in.push(bio::bzip2_compressor());
      break;
#else
      NDN_THROW(CompressionError("BZIP2 compression not supported!"));
#endif

    case CompressionScheme::LZMA:
#ifdef PSYNC_HAVE_LZMA
      in.push(bio::lzma_compressor(bio::lzma::best_compression));
      break;
#else
      NDN_THROW(CompressionError("LZMA compression not supported!"));
#endif

    case CompressionScheme::ZSTD:
#ifdef PSYNC_HAVE_ZSTD
      in.push(bio::zstd_compressor(bio::zstd::best_compression));
      break;
#else
      NDN_THROW(CompressionError("ZSTD compression not supported!"));
#endif

    case CompressionScheme::NONE:
      break;
  }
  in.push(bio::array_source(reinterpret_cast<const char*>(buffer.data()), buffer.size()));
  bio::copy(in, out);

  return out.buf();
}

std::shared_ptr<ndn::Buffer>
decompress(CompressionScheme scheme, ndn::span<const uint8_t> buffer)
{
  ndn::OBufferStream out;
  bio::filtering_streambuf<bio::input> in;

  switch (scheme) {
    case CompressionScheme::ZLIB:
#ifdef PSYNC_HAVE_ZLIB
      in.push(bio::zlib_decompressor());
      break;
#else
      NDN_THROW(CompressionError("ZLIB decompression not supported!"));
#endif

    case CompressionScheme::GZIP:
#ifdef PSYNC_HAVE_GZIP
      in.push(bio::gzip_decompressor());
      break;
#else
      NDN_THROW(CompressionError("GZIP compression not supported!"));
#endif

    case CompressionScheme::BZIP2:
#ifdef PSYNC_HAVE_BZIP2
      in.push(bio::bzip2_decompressor());
      break;
#else
      NDN_THROW(CompressionError("BZIP2 compression not supported!"));
#endif

    case CompressionScheme::LZMA:
#ifdef PSYNC_HAVE_LZMA
      in.push(bio::lzma_decompressor());
      break;
#else
      NDN_THROW(CompressionError("LZMA compression not supported!"));
#endif

    case CompressionScheme::ZSTD:
#ifdef PSYNC_HAVE_ZSTD
      in.push(bio::zstd_decompressor());
      break;
#else
      NDN_THROW(CompressionError("ZSTD compression not supported!"));
#endif

    case CompressionScheme::NONE:
      break;
  }
  in.push(bio::array_source(reinterpret_cast<const char*>(buffer.data()), buffer.size()));
  bio::copy(in, out);

  return out.buf();
}

} // namespace psync::detail
