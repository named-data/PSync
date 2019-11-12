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

#include "PSync/full-producer.hpp"
#include "unit-test-time-fixture.hpp"

#include <boost/test/unit_test.hpp>
#include <ndn-cxx/name.hpp>
#include <ndn-cxx/util/dummy-client-face.hpp>
#include <ndn-cxx/mgmt/nfd/control-parameters.hpp>

namespace psync {

using namespace ndn;
using namespace std;

BOOST_AUTO_TEST_SUITE(TestFullProducer)

BOOST_AUTO_TEST_CASE(Constructor)
{
  util::DummyClientFace face({true, true});
  BOOST_REQUIRE_NO_THROW(FullProducer(40, face, Name("/psync"), Name("/testUser"), nullptr));
}

BOOST_AUTO_TEST_CASE(OnInterest)
{
  Name syncPrefix("/psync"), userNode("/testUser");
  util::DummyClientFace face({true, true});

  FullProducer node(40, face, syncPrefix, userNode, nullptr);

  Name syncInterestName(syncPrefix);
  syncInterestName.append("malicious-IBF");

  BOOST_REQUIRE_NO_THROW(node.onSyncInterest(syncPrefix, Interest(syncInterestName)));
}

BOOST_FIXTURE_TEST_CASE(ConstantTimeoutForFirstSegment, ndn::tests::UnitTestTimeFixture)
{
  Name syncPrefix("/psync"), userNode("/testUser");
  util::DummyClientFace face(io, {true, true});

  FullProducer node(40, face, syncPrefix, userNode, nullptr, ndn::time::milliseconds(8000));
  advanceClocks(ndn::time::milliseconds(10));
  face.sentInterests.clear();

  // full sync sends the next one in interest lifetime / 2 +- jitter
  advanceClocks(ndn::time::milliseconds(6000));
  BOOST_CHECK_EQUAL(face.sentInterests.size(), 1);
}

BOOST_AUTO_TEST_CASE(OnSyncDataDecodeFailure)
{
  Name syncPrefix("/psync"), userNode("/testUser");
  util::DummyClientFace face({true, true});

  FullProducer node(40, face, syncPrefix, userNode, nullptr);

  ndn::Name syncInterestName(syncPrefix);
  node.m_iblt.appendToName(syncInterestName);
  ndn::Interest syncInterest(syncInterestName);

  auto badCompress = std::make_shared<const ndn::Buffer>(5);

  BOOST_REQUIRE_NO_THROW(node.onSyncData(syncInterest, badCompress));

  const uint8_t test[] = {'t', 'e', 's', 't'};
  auto goodCompressBadBlock = compress(node.m_contentCompression, &test[0], sizeof(test));
  BOOST_REQUIRE_NO_THROW(node.onSyncData(syncInterest, goodCompressBadBlock));
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace psync