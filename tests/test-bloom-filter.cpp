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
 **/

#include "PSync/detail/bloom-filter.hpp"

#include "tests/boost-test.hpp"

#include <ndn-cxx/name.hpp>

namespace psync::tests {

using detail::BloomFilter;

BOOST_AUTO_TEST_SUITE(TestBloomFilter)

BOOST_AUTO_TEST_CASE(Basic)
{
  BloomFilter bf(100, 0.001);

  std::string insertName("/memphis");
  bf.insert(insertName);
  BOOST_CHECK(bf.contains(insertName));
}

BOOST_AUTO_TEST_CASE(NameAppendAndExtract)
{
  ndn::Name bfName("/test");
  BloomFilter bf(100, 0.001);
  bf.insert("/memphis");
  bf.appendToName(bfName);

  BloomFilter bfFromName(100, 0.001, bfName.at(-1));

  BOOST_CHECK_EQUAL(bfName.at(1).toNumber(), 100);
  BOOST_CHECK_EQUAL(bfName.at(2).toNumber(), 1);
  BOOST_CHECK_EQUAL(bf, bfFromName);

  BOOST_CHECK_THROW(BloomFilter(200, 0.001, bfName.at(-1)), std::runtime_error);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace psync::tests
