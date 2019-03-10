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
#include "PSync/consumer.hpp"
#include "unit-test-time-fixture.hpp"

#include <boost/test/unit_test.hpp>
#include <ndn-cxx/name.hpp>
#include <ndn-cxx/util/dummy-client-face.hpp>

namespace psync {

using namespace ndn;
using namespace std;

class PartialSyncFixture : public tests::UnitTestTimeFixture
{
public:
  PartialSyncFixture()
   : face(io, {true, true})
   , syncPrefix("psync")
   , userPrefix("testUser-0")
   , numHelloDataRcvd(0)
   , numSyncDataRcvd(0)
  {
    producer = make_shared<PartialProducer>(40, face, syncPrefix, userPrefix);
    addUserNodes("testUser", 10);
  }

  ~PartialSyncFixture()
  {
    for (auto consumer : consumers) {
      if (consumer) {
        consumer->stop();
      }
    }
  }

  void
  addConsumer(int id, const vector<string>& subscribeTo, bool linkToProducer = true)
  {
    consumerFaces[id] = make_shared<util::DummyClientFace>(io, util::DummyClientFace::Options{true, true});

    if (linkToProducer) {
      face.linkTo(*consumerFaces[id]);
    }

    consumers[id] = make_shared<Consumer>(syncPrefix, *consumerFaces[id],
                      [&, id] (const vector<Name>& availableSubs)
                      {
                        numHelloDataRcvd++;
                        BOOST_CHECK(checkSubList(availableSubs));

                        checkIBFUpdated(id);

                        for (const auto& sub : subscribeTo) {
                          consumers[id]->addSubscription(sub);
                        }
                        consumers[id]->sendSyncInterest();
                      },
                      [&, id] (const std::vector<MissingDataInfo>& updates) {
                        numSyncDataRcvd++;

                        checkIBFUpdated(id);

                        for (const auto& update : updates) {
                          BOOST_CHECK(consumers[id]->isSubscribed(update.prefix));
                          BOOST_CHECK_EQUAL(oldSeqMap.at(update.prefix) + 1, update.lowSeq);
                          BOOST_CHECK_EQUAL(producer->m_prefixes.m_prefixes.at(update.prefix), update.highSeq);
                          BOOST_CHECK_EQUAL(consumers[id]->getSeqNo(update.prefix).value(), update.highSeq);
                        }
                      }, 40, 0.001);

    advanceClocks(ndn::time::milliseconds(10));
  }

  void
  checkIBFUpdated(int id)
  {
    Name emptyName;
    producer->m_iblt.appendToName(emptyName);
    BOOST_CHECK_EQUAL(consumers[id]->m_iblt, emptyName);
  }

  bool
  checkSubList(const vector<Name>& availableSubs)
  {
    for (const auto& prefix : producer->m_prefixes.m_prefixes ) {
      if (std::find(availableSubs.begin(), availableSubs.end(), prefix.first) == availableSubs.end()) {
        return false;
      }
    }
    return true;
  }

  void
  addUserNodes(const std::string& prefix, int numOfUserNodes)
  {
    // zeroth is added through constructor
    for (int i = 1; i < numOfUserNodes; i++) {
      producer->addUserNode(prefix + "-" + to_string(i));
    }
  }

  void
  publishUpdateFor(const std::string& prefix)
  {
    oldSeqMap = producer->m_prefixes.m_prefixes;
    producer->publishName(prefix);
    advanceClocks(ndn::time::milliseconds(10));
  }

  void
  updateSeqFor(const std::string& prefix, uint64_t seq)
  {
    oldSeqMap = producer->m_prefixes.m_prefixes;
    producer->updateSeqNo(prefix, seq);
  }

  util::DummyClientFace face;
  Name syncPrefix;
  Name userPrefix;

  shared_ptr<PartialProducer> producer;
  std::map <ndn::Name, uint64_t> oldSeqMap;

  shared_ptr<Consumer> consumers[3];
  shared_ptr<util::DummyClientFace> consumerFaces[3];
  int numHelloDataRcvd;
  int numSyncDataRcvd;
};

BOOST_FIXTURE_TEST_SUITE(PartialSync, PartialSyncFixture)

BOOST_AUTO_TEST_CASE(Simple)
{
  vector<string> subscribeTo{"testUser-2", "testUser-4", "testUser-6"};
  addConsumer(0, subscribeTo);

  consumers[0]->sendHelloInterest();
  advanceClocks(ndn::time::milliseconds(10));
  BOOST_CHECK_EQUAL(numHelloDataRcvd, 1);

  publishUpdateFor("testUser-2");
  BOOST_CHECK_EQUAL(numSyncDataRcvd, 1);
  publishUpdateFor("testUser-3");
  BOOST_CHECK_EQUAL(numSyncDataRcvd, 1);
  publishUpdateFor("testUser-2");
  BOOST_CHECK_EQUAL(numSyncDataRcvd, 2);
}

BOOST_AUTO_TEST_CASE(MissedUpdate)
{
  vector<string> subscribeTo{"testUser-2", "testUser-4", "testUser-6"};
  addConsumer(0, subscribeTo);

  consumers[0]->sendHelloInterest();
  advanceClocks(ndn::time::milliseconds(10));
  BOOST_CHECK_EQUAL(numHelloDataRcvd, 1);

  updateSeqFor("testUser-2", 3);
  BOOST_CHECK_EQUAL(numSyncDataRcvd, 0);

  // The sync interest sent after hello will timeout
  advanceClocks(ndn::time::milliseconds(1000));
  BOOST_CHECK_EQUAL(numSyncDataRcvd, 0);

  // Next sync interest will bring back the sync data
  advanceClocks(ndn::time::milliseconds(1500));
  BOOST_CHECK_EQUAL(numSyncDataRcvd, 1);
}

BOOST_AUTO_TEST_CASE(LateSubscription)
{
  vector<string> subscribeTo{"testUser-2", "testUser-4", "testUser-6"};
  addConsumer(0, subscribeTo);

  consumers[0]->sendHelloInterest();
  advanceClocks(ndn::time::milliseconds(10));

  BOOST_CHECK_EQUAL(numHelloDataRcvd, 1);
  publishUpdateFor("testUser-2");
  BOOST_CHECK_EQUAL(numSyncDataRcvd, 1);

  consumers[0]->addSubscription("testUser-3");
  consumers[0]->sendSyncInterest();
  publishUpdateFor("testUser-3");
  BOOST_CHECK_EQUAL(numSyncDataRcvd, 2);
}

BOOST_AUTO_TEST_CASE(ConsumerSyncTimeout)
{
  vector<string> subscribeTo{"testUser-2", "testUser-4", "testUser-6"};
  addConsumer(0, subscribeTo);

  consumers[0]->sendHelloInterest();
  BOOST_CHECK_EQUAL(producer->m_pendingEntries.size(), 0);
  advanceClocks(ndn::time::milliseconds(10));
  BOOST_CHECK_EQUAL(producer->m_pendingEntries.size(), 1);
  advanceClocks(ndn::time::milliseconds(10), 100);
  BOOST_CHECK_EQUAL(producer->m_pendingEntries.size(), 0);
  advanceClocks(ndn::time::milliseconds(10), 100);

  int numSyncInterests = 0;
  for (const auto& interest : consumerFaces[0]->sentInterests) {
    if (interest.getName().getSubName(0, 2) == Name("/psync/sync")) {
      numSyncInterests++;
    }
  }
  BOOST_CHECK_EQUAL(numSyncInterests, 2);
  BOOST_CHECK_EQUAL(numSyncDataRcvd, 0);
}

BOOST_AUTO_TEST_CASE(MultipleConsumersWithSameSubList)
{
  vector<string> subscribeTo{"testUser-2", "testUser-4", "testUser-6"};
  addConsumer(0, subscribeTo);
  addConsumer(1, subscribeTo);
  addConsumer(2, subscribeTo);

  consumers[0]->sendHelloInterest();
  consumers[1]->sendHelloInterest();
  consumers[2]->sendHelloInterest();
  advanceClocks(ndn::time::milliseconds(10));

  BOOST_CHECK_EQUAL(numHelloDataRcvd, 3);

  publishUpdateFor("testUser-2");
  BOOST_CHECK_EQUAL(numSyncDataRcvd, 3);

  publishUpdateFor("testUser-3");
  BOOST_CHECK_EQUAL(numSyncDataRcvd, 3);
}

BOOST_AUTO_TEST_CASE(MultipleConsumersWithDifferentSubList)
{
  vector<string> subscribeTo{"testUser-2", "testUser-4", "testUser-6"};
  addConsumer(0, subscribeTo);

  vector<string> subscribeTo1{"testUser-1", "testUser-3", "testUser-5"};
  addConsumer(1, subscribeTo1);

  vector<string> subscribeTo2{"testUser-2", "testUser-3"};
  addConsumer(2, subscribeTo2);

  consumers[0]->sendHelloInterest();
  consumers[1]->sendHelloInterest();
  consumers[2]->sendHelloInterest();
  advanceClocks(ndn::time::milliseconds(10));

  BOOST_CHECK_EQUAL(numHelloDataRcvd, 3);

  publishUpdateFor("testUser-2");
  BOOST_CHECK_EQUAL(numSyncDataRcvd, 2);

  numSyncDataRcvd = 0;
  publishUpdateFor("testUser-3");
  BOOST_CHECK_EQUAL(numSyncDataRcvd, 2);
}

BOOST_AUTO_TEST_CASE(ReplicatedProducer)
{
  vector<string> subscribeTo{"testUser-2", "testUser-4", "testUser-6"};
  addConsumer(0, subscribeTo);

  consumers[0]->sendHelloInterest();
  advanceClocks(ndn::time::milliseconds(10));
  BOOST_CHECK_EQUAL(numHelloDataRcvd, 1);

  publishUpdateFor("testUser-2");
  BOOST_CHECK_EQUAL(numSyncDataRcvd, 1);

  // Link to first producer goes down
  face.unlink();

  util::DummyClientFace face2(io, {true, true});
  PartialProducer replicatedProducer(40, face2, syncPrefix, userPrefix);
  for (int i = 1; i < 10; i++) {
      replicatedProducer.addUserNode("testUser-" + to_string(i));
  }
  advanceClocks(ndn::time::milliseconds(10));
  replicatedProducer.publishName("testUser-2");
  // Link to a replicated producer comes up
  face2.linkTo(*consumerFaces[0]);

  BOOST_CHECK_EQUAL(face2.sentData.size(), 0);

  // Update in first producer as well so consumer on sync data
  // callback checks still pass
  publishUpdateFor("testUser-2");
  replicatedProducer.publishName("testUser-2");
  advanceClocks(ndn::time::milliseconds(15), 100);
  BOOST_CHECK_EQUAL(numSyncDataRcvd, 2);
  BOOST_CHECK_EQUAL(face2.sentData.size(), 1);
}

BOOST_AUTO_TEST_CASE(ApplicationNack)
{
  // 50 is more than expected number of entries of 40 in the producer's IBF
  addUserNodes("testUser", 50);

  vector<string> subscribeTo{"testUser-2", "testUser-4", "testUser-6"};
  addConsumer(0, subscribeTo);

  consumers[0]->sendHelloInterest();
  advanceClocks(ndn::time::milliseconds(10));
  BOOST_CHECK_EQUAL(numHelloDataRcvd, 1);

  publishUpdateFor("testUser-2");
  BOOST_CHECK_EQUAL(numSyncDataRcvd, 1);

  oldSeqMap = producer->m_prefixes.m_prefixes;
  for (int i = 0; i < 50; i++) {
    ndn::Name prefix("testUser-" + to_string(i));
    producer->updateSeqNo(prefix, producer->getSeqNo(prefix).value() + 1);
  }
  // Next sync interest should trigger the nack
  advanceClocks(ndn::time::milliseconds(15), 100);

  // Application should have been notified that new data is available
  // from the hello itself.
  BOOST_CHECK_EQUAL(numSyncDataRcvd, 2);

  bool nackRcvd = false;
  for (const auto& data : face.sentData) {
    if (data.getContentType() == ndn::tlv::ContentType_Nack) {
      nackRcvd = true;
      break;
    }
  }
  BOOST_CHECK(nackRcvd);

  publishUpdateFor("testUser-4");
  advanceClocks(ndn::time::milliseconds(10));
  BOOST_CHECK_EQUAL(numSyncDataRcvd, 3);
}

BOOST_AUTO_TEST_CASE(SegmentedHello)
{
  vector<string> subscribeTo{"testUser-2", "testUser-4", "testUser-6"};
  addConsumer(0, subscribeTo);

  addUserNodes("testUser", 400);

  consumers[0]->sendHelloInterest();
  advanceClocks(ndn::time::milliseconds(10));
  BOOST_CHECK_EQUAL(numHelloDataRcvd, 1);

  // Simulate sending delayed interest for second segment
  Name dataName = face.sentData.back().getName();
  face.sentData.clear();
  BOOST_CHECK_EQUAL(producer->m_segmentPublisher.m_ims.size(), 2);

  advanceClocks(ndn::time::milliseconds(1000));
  BOOST_CHECK_EQUAL(producer->m_segmentPublisher.m_ims.size(), 0);

  producer->onHelloInterest(consumers[0]->m_helloInterestPrefix, Interest(dataName));
  advanceClocks(ndn::time::milliseconds(10));
  BOOST_CHECK_EQUAL(producer->m_segmentPublisher.m_ims.size(), 2);
  BOOST_CHECK_EQUAL(face.sentData.front().getName()[-1].toSegment(), 1);
}

BOOST_AUTO_TEST_CASE(SegmentedSync)
{
  ndn::Name longNameToExceedDataSize;
  for (int i = 0; i < 100; i++) {
    longNameToExceedDataSize.append("test-" + std::to_string(i));
  }
  addUserNodes(longNameToExceedDataSize.toUri(), 10);

  vector<string> subscribeTo;
  for (int i = 1; i < 10; i++) {
    subscribeTo.push_back(longNameToExceedDataSize.toUri() + "-" + to_string(i));
  }
  addConsumer(0, subscribeTo);

  consumers[0]->sendHelloInterest();
  advanceClocks(ndn::time::milliseconds(10));
  BOOST_CHECK_EQUAL(numHelloDataRcvd, 1);

  // To be used later to simulate sending delayed segmented interest
  ndn::Name syncInterestName(consumers[0]->m_syncInterestPrefix);
  consumers[0]->m_bloomFilter.appendToName(syncInterestName);
  syncInterestName.append(consumers[0]->m_iblt);
  syncInterestName.appendVersion();
  syncInterestName.appendSegment(1);

  oldSeqMap = producer->m_prefixes.m_prefixes;
  for (int i = 1; i < 10; i++) {
    producer->updateSeqNo(longNameToExceedDataSize.toUri() + "-" + to_string(i), 1);
  }

  advanceClocks(ndn::time::milliseconds(1000));
  BOOST_CHECK_EQUAL(numSyncDataRcvd, 0);

  advanceClocks(ndn::time::milliseconds(1500));
  BOOST_CHECK_EQUAL(numSyncDataRcvd, 1);

  // Simulate sending delayed interest for second segment
  face.sentData.clear();
  consumerFaces[0]->sentData.clear();

  BOOST_CHECK_EQUAL(producer->m_segmentPublisher.m_ims.size(), 2);

  advanceClocks(ndn::time::milliseconds(2000));
  BOOST_CHECK_EQUAL(producer->m_segmentPublisher.m_ims.size(), 0);

  producer->onSyncInterest(consumers[0]->m_syncInterestPrefix, Interest(syncInterestName));
  advanceClocks(ndn::time::milliseconds(10));
  BOOST_CHECK_EQUAL(producer->m_segmentPublisher.m_ims.size(), 2);
  BOOST_CHECK_EQUAL(face.sentData.front().getName()[-1].toSegment(), 1);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace psync