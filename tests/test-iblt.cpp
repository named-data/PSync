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

#include "PSync/detail/iblt.hpp"
#include "PSync/detail/util.hpp"

#include <boost/test/unit_test.hpp>
#include <ndn-cxx/name.hpp>
#include <ndn-cxx/interest.hpp>

namespace psync {

using namespace ndn;

BOOST_AUTO_TEST_SUITE(TestIBLT)

BOOST_AUTO_TEST_CASE(Equal)
{
  int size = 10;

  IBLT iblt1(size);
  IBLT iblt2(size);
  BOOST_CHECK_EQUAL(iblt1, iblt2);

  std::string prefix = Name("/test/memphis").appendNumber(1).toUri();
  uint32_t newHash = murmurHash3(11, prefix);
  iblt1.insert(newHash);
  iblt2.insert(newHash);

  BOOST_CHECK_EQUAL(iblt1, iblt2);

  Name ibfName1("sync"), ibfName2("sync");
  iblt1.appendToName(ibfName1);
  iblt2.appendToName(ibfName2);
  BOOST_CHECK_EQUAL(ibfName1, ibfName2);
}

BOOST_AUTO_TEST_CASE(NameAppendAndExtract)
{
  int size = 10;

  IBLT iblt(size);
  std::string prefix = Name("/test/memphis").appendNumber(1).toUri();
  uint32_t newHash = murmurHash3(11, prefix);
  iblt.insert(newHash);

  Name ibltName("sync");
  iblt.appendToName(ibltName);

  IBLT rcvd(size);
  rcvd.initialize(ibltName.get(-1));

  BOOST_CHECK_EQUAL(iblt, rcvd);

  IBLT rcvdDiffSize(20);
  BOOST_CHECK_THROW(rcvdDiffSize.initialize(ibltName.get(-1)), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(CopyInsertErase)
{
  int size = 10;

  IBLT iblt1(size);

  std::string prefix = Name("/test/memphis").appendNumber(1).toUri();
  uint32_t hash1 = murmurHash3(11, prefix);
  iblt1.insert(hash1);

  IBLT iblt2(iblt1);
  iblt2.erase(hash1);
  prefix = Name("/test/memphis").appendNumber(2).toUri();
  uint32_t hash3 = murmurHash3(11, prefix);
  iblt2.insert(hash3);

  iblt1.erase(hash1);
  prefix = Name("/test/memphis").appendNumber(5).toUri();
  uint32_t hash5 = murmurHash3(11, prefix);
  iblt1.insert(hash5);

  iblt2.erase(hash3);
  iblt2.insert(hash5);

  BOOST_CHECK_EQUAL(iblt1, iblt2);
}

BOOST_AUTO_TEST_CASE(HigherSeqTest)
{
  // The case where we can't recognize if the rcvd IBF has higher sequence number
  // Relevant to full sync case
  int size = 10;

  IBLT ownIBF(size);
  IBLT rcvdIBF(size);

  std::string prefix = Name("/test/memphis").appendNumber(3).toUri();
  uint32_t hash1 = murmurHash3(11, prefix);
  ownIBF.insert(hash1);

  std::string prefix2 = Name("/test/memphis").appendNumber(4).toUri();
  uint32_t hash2 = murmurHash3(11, prefix2);
  rcvdIBF.insert(hash2);

  IBLT diff = ownIBF - rcvdIBF;
  std::set<uint32_t> positive;
  std::set<uint32_t> negative;

  BOOST_CHECK(diff.listEntries(positive, negative));
  BOOST_CHECK(*positive.begin() == hash1);
  BOOST_CHECK(*negative.begin() == hash2);
}

BOOST_AUTO_TEST_CASE(Difference)
{
  int size = 10;

  IBLT ownIBF(size);

  IBLT rcvdIBF = ownIBF;

  IBLT diff = ownIBF - rcvdIBF;

  std::set<uint32_t> positive; // non-empty Positive means we have some elements that the others don't
  std::set<uint32_t> negative;

  BOOST_CHECK(diff.listEntries(positive, negative));
  BOOST_CHECK_EQUAL(positive.size(), 0);
  BOOST_CHECK_EQUAL(negative.size(), 0);

  std::string prefix = Name("/test/memphis").appendNumber(1).toUri();
  uint32_t newHash = murmurHash3(11, prefix);
  ownIBF.insert(newHash);

  diff = ownIBF - rcvdIBF;
  BOOST_CHECK(diff.listEntries(positive, negative));
  BOOST_CHECK_EQUAL(positive.size(), 1);
  BOOST_CHECK_EQUAL(negative.size(), 0);

  prefix = Name("/test/csu").appendNumber(1).toUri();
  newHash = murmurHash3(11, prefix);
  rcvdIBF.insert(newHash);

  diff = ownIBF - rcvdIBF;
  BOOST_CHECK(diff.listEntries(positive, negative));
  BOOST_CHECK_EQUAL(positive.size(), 1);
  BOOST_CHECK_EQUAL(negative.size(), 1);
}

BOOST_AUTO_TEST_CASE(DifferenceBwOversizedIBFs)
{
  // Insert 50 elements into IBF of size 10
  // Check that we can still list the difference
  // even though we can't list the IBFs itself

  int size = 10;

  IBLT ownIBF(size);

  for (int i = 0; i < 50; i++) {
    std::string prefix = Name("/test/memphis" + std::to_string(i)).appendNumber(1).toUri();
    uint32_t newHash = murmurHash3(11, prefix);
    ownIBF.insert(newHash);
  }

  IBLT rcvdIBF = ownIBF;

  std::string prefix = Name("/test/ucla").appendNumber(1).toUri();
  uint32_t newHash = murmurHash3(11, prefix);
  ownIBF.insert(newHash);

  IBLT diff = ownIBF - rcvdIBF;

  std::set<uint32_t> positive;
  std::set<uint32_t> negative;
  BOOST_CHECK(diff.listEntries(positive, negative));
  BOOST_CHECK_EQUAL(positive.size(), 1);
  BOOST_CHECK_EQUAL(*positive.begin(), newHash);
  BOOST_CHECK_EQUAL(negative.size(), 0);

  BOOST_CHECK(!ownIBF.listEntries(positive, negative));
  BOOST_CHECK(!rcvdIBF.listEntries(positive, negative));
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace psync