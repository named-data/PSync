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
 **/

#include "PSync/detail/util.hpp"
#include "PSync/detail/config.hpp"
#include "unit-test-time-fixture.hpp"

#include <boost/test/unit_test.hpp>
#include <iostream>

namespace psync {

using namespace ndn;
using namespace std;

BOOST_AUTO_TEST_SUITE(TestUtil)

BOOST_AUTO_TEST_CASE(Basic)
{
  std::vector<CompressionScheme> available = {CompressionScheme::ZLIB, CompressionScheme::GZIP,
                                              CompressionScheme::BZIP2, CompressionScheme::LZMA,
                                              CompressionScheme::ZSTD};
  std::vector<CompressionScheme> supported;
  std::vector<CompressionScheme> notSupported;

#ifdef PSYNC_HAVE_ZLIB
  supported.push_back(CompressionScheme::ZLIB);
#endif
#ifdef PSYNC_HAVE_GZIP
  supported.push_back(CompressionScheme::GZIP);
#endif
#ifdef PSYNC_HAVE_BZIP2
  supported.push_back(CompressionScheme::BZIP2);
#endif
#ifdef PSYNC_HAVE_LZMA
  supported.push_back(CompressionScheme::LZMA);
#endif
#ifdef PSYNC_HAVE_ZSTD
  supported.push_back(CompressionScheme::ZSTD);
#endif

  const uint8_t uncompressed[] = {'t', 'e', 's', 't'};

  for (const auto& s : supported) {
    BOOST_CHECK_NO_THROW(compress(s, uncompressed, sizeof(uncompressed)));
    auto compressed = compress(s, uncompressed, sizeof(uncompressed));
    BOOST_CHECK_NO_THROW(decompress(s, compressed->data(), compressed->size()));
  }

  std::set_difference(available.begin(), available.end(), supported.begin(), supported.end(),
                      std::inserter(notSupported, notSupported.begin()));

  for (const auto& s : notSupported) {
    BOOST_CHECK_THROW(compress(s, uncompressed, sizeof(uncompressed)),
                      std::runtime_error);
    BOOST_CHECK_THROW(decompress(s, uncompressed, sizeof(uncompressed)),
                      std::runtime_error);
  }
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace psync