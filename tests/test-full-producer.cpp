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
 */

#include "PSync/full-producer.hpp"
#include "PSync/detail/util.hpp"


#include "tests/boost-test.hpp"
#include "tests/io-fixture.hpp"
#include "tests/key-chain-fixture.hpp"

#include <ndn-cxx/util/dummy-client-face.hpp>

namespace psync::tests {

using ndn::Interest;
using ndn::Name;

class FullProducerFixture : public IoFixture, public KeyChainFixture
{
protected:
  ndn::DummyClientFace m_face{m_io, m_keyChain, {true, true}};
};

BOOST_FIXTURE_TEST_SUITE(TestFullProducer, FullProducerFixture)

BOOST_AUTO_TEST_CASE(OnInterest)
{
  Name syncPrefix("/psync");
  FullProducer::Options opts;
  opts.ibfCount = 40;
  FullProducer node(m_face, m_keyChain, syncPrefix, opts);

  Name syncInterestName(syncPrefix);
  syncInterestName.append("malicious-IBF");

  BOOST_CHECK_NO_THROW(node.onSyncInterest(syncPrefix, Interest(syncInterestName)));
}

BOOST_AUTO_TEST_CASE(ConstantTimeoutForFirstSegment)
{
  Name syncPrefix("/psync");
  FullProducer::Options opts;
  opts.ibfCount = 40;
  opts.syncInterestLifetime = 8_s;
  FullProducer node(m_face, m_keyChain, syncPrefix, opts);

  advanceClocks(10_ms);
  m_face.sentInterests.clear();

  // full sync sends the next one in interest lifetime / 2 +- jitter
  advanceClocks(6_s);
  BOOST_CHECK_EQUAL(m_face.sentInterests.size(), 1);
}

BOOST_AUTO_TEST_CASE(OnSyncDataDecodeFailure)
{
  Name syncPrefix("/psync");
  FullProducer::Options opts;
  opts.ibfCount = 40;
  FullProducer node(m_face, m_keyChain, syncPrefix, opts);

  Name syncInterestName(syncPrefix);
  node.m_iblt.appendToName(syncInterestName);
  Interest syncInterest(syncInterestName);

  auto badCompress = std::make_shared<const ndn::Buffer>(5);
  BOOST_CHECK_NO_THROW(node.onSyncData(syncInterest, badCompress));

  const uint8_t test[] = {'t', 'e', 's', 't'};
  auto goodCompressBadBlock = detail::compress(node.m_contentCompression, test);
  BOOST_CHECK_NO_THROW(node.onSyncData(syncInterest, goodCompressBadBlock));
}

BOOST_AUTO_TEST_CASE(SatisfyPendingInterestsBehavior)
{
  Name syncPrefix("/psync");
  FullProducer::Options opts;
  opts.ibfCount = 6;
  FullProducer node(m_face, m_keyChain, syncPrefix, opts);

  Name syncInterestName(syncPrefix);
  node.m_iblt.appendToName(syncInterestName);
  syncInterestName.appendNumber(1);
  Interest syncInterest(syncInterestName);

  node.addUserNode(syncPrefix);

  node.onSyncInterest(syncPrefix, syncInterest);

  BOOST_CHECK_EQUAL(node.m_pendingEntries.size(), 1);

  // Test whether data is still sent if IBF diff is greater than default threshhold.
  auto prefix1 = Name("/test/alice").appendNumber(1);
  uint32_t newHash1 = psync::detail::murmurHash3(42, prefix1);
  node.m_iblt.insert(newHash1);

  auto prefix2 = Name("/test/bob").appendNumber(1);
  uint32_t newHash2 = psync::detail::murmurHash3(42, prefix2);
  node.m_iblt.insert(newHash2);

  auto prefix3 = Name("/test/carol").appendNumber(1);
  uint32_t newHash3 = psync::detail::murmurHash3(42, prefix3);
  node.m_iblt.insert(newHash3);

  auto prefix4 = Name("/test/david").appendNumber(1);
  uint32_t newHash4 = psync::detail::murmurHash3(42, prefix4);
  node.m_iblt.insert(newHash4);

  auto prefix5 = Name("/test/erin").appendNumber(1);
  uint32_t newHash5 = psync::detail::murmurHash3(42, prefix5);
  node.m_iblt.insert(newHash5);

  node.publishName(syncPrefix);

  advanceClocks(10_ms);

  BOOST_CHECK_EQUAL(m_face.sentData.size(), 1);

  BOOST_CHECK_EQUAL(node.m_pendingEntries.empty(), true);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace psync::tests
