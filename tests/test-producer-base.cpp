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

#include "PSync/producer-base.hpp"

#include "tests/boost-test.hpp"
#include "tests/key-chain-fixture.hpp"

#include <ndn-cxx/util/dummy-client-face.hpp>

namespace psync {

using ndn::Name;

class ProducerBaseFixture : public tests::KeyChainFixture
{
protected:
  ndn::DummyClientFace m_face;
};

BOOST_FIXTURE_TEST_SUITE(TestProducerBase, ProducerBaseFixture)

BOOST_AUTO_TEST_CASE(Basic)
{
  Name userNode("/testUser");
  ProducerBase producerBase(m_face, m_keyChain, 40, Name("/psync"), userNode);

  // Hash table size should be 40 + 40/2 = 60 (which is perfectly divisible by N_HASH = 3)
  BOOST_CHECK_EQUAL(producerBase.m_iblt.getHashTable().size(), 60);
  BOOST_CHECK_EQUAL(producerBase.getSeqNo(userNode).value(), 0);

  producerBase.updateSeqNo(userNode, 1);
  BOOST_CHECK(producerBase.getSeqNo(userNode).value() == 1);

  auto prefixWithSeq = Name(userNode).appendNumber(1);
  uint32_t hash = producerBase.m_biMap.right.find(prefixWithSeq)->second;
  Name prefix(producerBase.m_biMap.left.find(hash)->second);
  BOOST_CHECK_EQUAL(prefix.getPrefix(-1), userNode);

  producerBase.removeUserNode(userNode);
  BOOST_CHECK(producerBase.getSeqNo(userNode) == std::nullopt);
  BOOST_CHECK(producerBase.m_biMap.right.find(prefixWithSeq) == producerBase.m_biMap.right.end());
  BOOST_CHECK(producerBase.m_biMap.left.find(hash) == producerBase.m_biMap.left.end());

  Name nonExistentUserNode("/notAUser");
  producerBase.updateSeqNo(nonExistentUserNode, 1);
  BOOST_CHECK(producerBase.m_biMap.right.find(Name(nonExistentUserNode).appendNumber(1)) ==
              producerBase.m_biMap.right.end());
}

BOOST_AUTO_TEST_CASE(ApplicationNack)
{
  ProducerBase producerBase(m_face, m_keyChain, 40, Name("/psync"), Name("/testUser"));

  BOOST_CHECK_EQUAL(m_face.sentData.size(), 0);
  producerBase.sendApplicationNack(Name("test"));
  m_face.processEvents(10_ms);

  BOOST_REQUIRE_EQUAL(m_face.sentData.size(), 1);
  BOOST_CHECK_EQUAL(m_face.sentData.front().getContentType(), ndn::tlv::ContentType_Nack);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace psync
