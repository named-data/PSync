/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2023,  The University of Memphis
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
#include "PSync/detail/util.hpp"

#include "tests/boost-test.hpp"
#include "tests/io-fixture.hpp"
#include "tests/key-chain-fixture.hpp"

#include <ndn-cxx/util/dummy-client-face.hpp>

namespace psync {

using ndn::Interest;
using ndn::Name;

class FullProducerFixture : public tests::IoFixture, public tests::KeyChainFixture
{
protected:
  ndn::DummyClientFace m_face{m_io, m_keyChain, {true, true}};
};

BOOST_FIXTURE_TEST_SUITE(TestFullProducer, FullProducerFixture)

BOOST_AUTO_TEST_CASE(OnInterest)
{
  Name syncPrefix("/psync"), userNode("/testUser");
  FullProducer node(m_face, m_keyChain, 40, syncPrefix, userNode, nullptr);

  Name syncInterestName(syncPrefix);
  syncInterestName.append("malicious-IBF");

  BOOST_CHECK_NO_THROW(node.onSyncInterest(syncPrefix, Interest(syncInterestName)));
}

BOOST_AUTO_TEST_CASE(ConstantTimeoutForFirstSegment)
{
  Name syncPrefix("/psync"), userNode("/testUser");
  FullProducer node(m_face, m_keyChain, 40, syncPrefix, userNode, nullptr, 8_s);

  advanceClocks(10_ms);
  m_face.sentInterests.clear();

  // full sync sends the next one in interest lifetime / 2 +- jitter
  advanceClocks(6_s);
  BOOST_CHECK_EQUAL(m_face.sentInterests.size(), 1);
}

BOOST_AUTO_TEST_CASE(OnSyncDataDecodeFailure)
{
  Name syncPrefix("/psync"), userNode("/testUser");
  FullProducer node(m_face, m_keyChain, 40, syncPrefix, userNode, nullptr);

  Name syncInterestName(syncPrefix);
  node.m_iblt.appendToName(syncInterestName);
  Interest syncInterest(syncInterestName);

  auto badCompress = std::make_shared<const ndn::Buffer>(5);
  BOOST_CHECK_NO_THROW(node.onSyncData(syncInterest, badCompress));

  const uint8_t test[] = {'t', 'e', 's', 't'};
  auto goodCompressBadBlock = detail::compress(node.m_contentCompression, test);
  BOOST_CHECK_NO_THROW(node.onSyncData(syncInterest, goodCompressBadBlock));
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace psync
