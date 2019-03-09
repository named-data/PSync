/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2019,  The University of Memphis
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
#include "PSync/detail/util.hpp"

#include <boost/test/unit_test.hpp>
#include <ndn-cxx/name.hpp>
#include <ndn-cxx/data.hpp>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/util/dummy-client-face.hpp>

namespace psync {

using namespace ndn;

BOOST_AUTO_TEST_SUITE(TestProducerBase)

BOOST_AUTO_TEST_CASE(Ctor)
{
  util::DummyClientFace face;
  BOOST_REQUIRE_NO_THROW(ProducerBase(40, Name("/psync"), Name("/testUser")));
}

BOOST_AUTO_TEST_CASE(Basic)
{
  util::DummyClientFace face;
  Name userNode("/testUser");
  ProducerBase producerBase(40, Name("/psync"), userNode);
  // Hash table size should be 40 + 40/2 = 60 (which is perfectly divisible by N_HASH = 3)
  BOOST_CHECK_EQUAL(producerBase.m_iblt.getHashTable().size(), 60);
  BOOST_CHECK_EQUAL(producerBase.getSeqNo(userNode).value(), 0);

  producerBase.updateSeqNo(userNode, 1);
  BOOST_CHECK(producerBase.getSeqNo(userNode.toUri()).value() == 1);

  std::string prefixWithSeq = Name(userNode).appendNumber(1).toUri();
  uint32_t hash = producerBase.m_name2hash[prefixWithSeq];
  ndn::Name prefix = producerBase.m_hash2name[hash];
  BOOST_CHECK_EQUAL(prefix.getPrefix(prefix.size() - 1), userNode.toUri());

  producerBase.removeUserNode(userNode);
  BOOST_CHECK(producerBase.getSeqNo(userNode.toUri()) == ndn::nullopt);
  BOOST_CHECK(producerBase.m_name2hash.find(prefixWithSeq) == producerBase.m_name2hash.end());
  BOOST_CHECK(producerBase.m_hash2name.find(hash) == producerBase.m_hash2name.end());

  Name nonExistentUserNode("/notAUser");
  producerBase.updateSeqNo(nonExistentUserNode, 1);
  BOOST_CHECK(producerBase.m_name2hash.find(Name(nonExistentUserNode).appendNumber(1).toUri()) ==
              producerBase.m_name2hash.end());
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace psync
