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

#include "PSync/partial-producer.hpp"

#include <boost/test/unit_test.hpp>
#include <ndn-cxx/name.hpp>
#include <ndn-cxx/util/dummy-client-face.hpp>
#include <ndn-cxx/mgmt/nfd/control-parameters.hpp>

namespace psync {

using namespace ndn;
using namespace std;

BOOST_AUTO_TEST_SUITE(TestPartialProducer)

BOOST_AUTO_TEST_CASE(Constructor)
{
  util::DummyClientFace face({true, true});
  BOOST_REQUIRE_NO_THROW(PartialProducer(40, face, Name("/psync"), Name("/testUser")));
}

BOOST_AUTO_TEST_CASE(RegisterPrefix)
{
  Name syncPrefix("/psync"), userNode("/testUser");
  util::DummyClientFace face({true, true});
  PartialProducer producer(40, face, syncPrefix, userNode);

  face.processEvents(time::milliseconds(-1));

  BOOST_REQUIRE_EQUAL(face.sentInterests.size(), 1);

  face.sentInterests.back();
  Interest interest = *face.sentInterests.begin();
  BOOST_CHECK_EQUAL(interest.getName().get(3), name::Component("register"));
  name::Component test = interest.getName().get(4);
  nfd::ControlParameters params(test.blockFromValue());
  BOOST_CHECK_EQUAL(params.getName(), syncPrefix);
}

BOOST_AUTO_TEST_CASE(PublishName)
{
  Name syncPrefix("/psync"), userNode("/testUser"), nonUser("/testUser2");
  util::DummyClientFace face({true, true});
  PartialProducer producer(40, face, syncPrefix, userNode);

  BOOST_CHECK_EQUAL(producer.getSeqNo(userNode).value_or(-1), 0);
  producer.publishName(userNode);
  BOOST_CHECK_EQUAL(producer.getSeqNo(userNode).value_or(-1), 1);

  producer.publishName(userNode);
  BOOST_CHECK_EQUAL(producer.getSeqNo(userNode).value_or(-1), 2);

  producer.publishName(userNode, 10);
  BOOST_CHECK_EQUAL(producer.getSeqNo(userNode).value_or(-1), 10);

  producer.publishName(nonUser);
  BOOST_CHECK_EQUAL(producer.getSeqNo(nonUser).value_or(-1), -1);
}

BOOST_AUTO_TEST_CASE(SameSyncInterest)
{
  Name syncPrefix("/psync"), userNode("/testUser");
  util::DummyClientFace face({true, true});
  PartialProducer producer(40, face, syncPrefix, userNode);

  Name syncInterestName(syncPrefix);
  syncInterestName.append("sync");
  Name syncInterestPrefix = syncInterestName;

  BloomFilter bf(20, 0.001);
  bf.appendToName(syncInterestName);

  producer.m_iblt.appendToName(syncInterestName);

  Interest syncInterest(syncInterestName);
  syncInterest.setInterestLifetime(time::milliseconds(1000));
  syncInterest.setNonce(1);
  BOOST_REQUIRE_NO_THROW(producer.onSyncInterest(syncInterestPrefix, syncInterest));
  face.processEvents(time::milliseconds(10));
  BOOST_CHECK_EQUAL(producer.m_pendingEntries.size(), 1);

  face.processEvents(time::milliseconds(500));

  // Same interest again - size of pending interest should remain same, but expirationEvent should change
  syncInterest.setNonce(2);
  BOOST_REQUIRE_NO_THROW(producer.onSyncInterest(syncInterestPrefix, syncInterest));
  face.processEvents(time::milliseconds(10));
  BOOST_CHECK_EQUAL(producer.m_pendingEntries.size(), 1);

  face.processEvents(time::milliseconds(500));
  BOOST_CHECK_EQUAL(producer.m_pendingEntries.size(), 1);

  face.processEvents(time::milliseconds(500));
  BOOST_CHECK_EQUAL(producer.m_pendingEntries.size(), 0);
}

BOOST_AUTO_TEST_CASE(OnSyncInterest)
{
  Name syncPrefix("/psync"), userNode("/testUser");
  util::DummyClientFace face({true, true});
  PartialProducer producer(40, face, syncPrefix, userNode);

  // Sync interest with no bloom filter attached
  Name syncInterestName(syncPrefix);
  syncInterestName.append("sync");
  producer.m_iblt.appendToName(syncInterestName);
  BOOST_REQUIRE_NO_THROW(producer.onSyncInterest(syncInterestName, Interest(syncInterestName)));

  // Sync interest with malicious bloom filter
  syncInterestName = syncPrefix;
  syncInterestName.append("sync");
  syncInterestName.appendNumber(20); // count of bloom filter
  syncInterestName.appendNumber(1);  // false positive probability * 1000 of bloom filter
  syncInterestName.append("fake-name");
  producer.m_iblt.appendToName(syncInterestName);
  BOOST_REQUIRE_NO_THROW(producer.onSyncInterest(syncInterestName, Interest(syncInterestName)));

  // Sync interest with malicious IBF
  syncInterestName = syncPrefix;
  syncInterestName.append("sync");
  BloomFilter bf(20, 0.001);
  bf.appendToName(syncInterestName);
  syncInterestName.append("fake-name");
  BOOST_REQUIRE_NO_THROW(producer.onSyncInterest(syncInterestName, Interest(syncInterestName)));
}

BOOST_AUTO_TEST_CASE(ApplicationNack)
{
  util::DummyClientFace face;
  PartialProducer producer(40, face, Name("/psync"), Name("/testUser"));

  BOOST_CHECK_EQUAL(face.sentData.size(), 0);
  producer.m_syncReplyFreshness = time::milliseconds(1000);
  producer.sendApplicationNack(Name("test"));
  face.processEvents(time::milliseconds(10));
  BOOST_CHECK_EQUAL(face.sentData.size(), 1);

  Data data = *face.sentData.begin();
  BOOST_CHECK_EQUAL(data.getContentType(), ndn::tlv::ContentType_Nack);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace psync