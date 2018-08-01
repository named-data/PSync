/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2018,  The University of Memphis
 *
 * This file is part of PSync.
 * See AUTHORS.md for complete list of PSync authors and contributors.
 *
 * PSync is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * PSync is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * PSync, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "full-producer.hpp"
#include "consumer.hpp"
#include "unit-test-time-fixture.hpp"
#include "detail/state.hpp"

#include <boost/test/unit_test.hpp>
#include <ndn-cxx/name.hpp>
#include <ndn-cxx/util/dummy-client-face.hpp>

namespace psync {

using namespace ndn;
using namespace std;

class FullSyncFixture : public tests::UnitTestTimeFixture
{
public:
  FullSyncFixture()
   : syncPrefix("psync")
  {
  }

  void
  addNode(int id)
  {
    faces[id] = std::make_shared<util::DummyClientFace>(io, util::DummyClientFace::Options{true, true});
    userPrefixes[id] = Name("userPrefix" + to_string(id));
    nodes[id] = make_shared<FullProducer>(40, *faces[id], syncPrefix, userPrefixes[id],
                                          [] (const std::vector<MissingDataInfo>& updates) {});
  }

  Name syncPrefix;
  shared_ptr<util::DummyClientFace> faces[4];
  Name userPrefixes[4];
  shared_ptr<FullProducer> nodes[4];
};

BOOST_FIXTURE_TEST_SUITE(FullSync, FullSyncFixture)

BOOST_AUTO_TEST_CASE(TwoNodesSimple)
{
  addNode(0);
  addNode(1);

  faces[0]->linkTo(*faces[1]);
  advanceClocks(ndn::time::milliseconds(10));

  nodes[0]->publishName(userPrefixes[0]);
  advanceClocks(ndn::time::milliseconds(10), 100);

  BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(userPrefixes[0]).value_or(-1), 1);
  BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefixes[0]).value_or(-1), 1);

  nodes[1]->publishName(userPrefixes[1]);
  advanceClocks(ndn::time::milliseconds(10), 100);
  BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(userPrefixes[1]).value_or(-1), 1);
  BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefixes[1]).value_or(-1), 1);

  nodes[1]->publishName(userPrefixes[1]);
  advanceClocks(ndn::time::milliseconds(10), 100);
  BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(userPrefixes[1]).value_or(-1), 2);
  BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefixes[1]).value_or(-1), 2);
}

BOOST_AUTO_TEST_CASE(TwoNodesForceSeqNo)
{
  addNode(0);
  addNode(1);

  faces[0]->linkTo(*faces[1]);
  advanceClocks(ndn::time::milliseconds(10));

  nodes[0]->publishName(userPrefixes[0], 3);
  advanceClocks(ndn::time::milliseconds(10), 100);

  BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(userPrefixes[0]).value_or(-1), 3);
  BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefixes[0]).value_or(-1), 3);
}

BOOST_AUTO_TEST_CASE(TwoNodesWithMultipleUserNodes)
{
  addNode(0);
  addNode(1);

  faces[0]->linkTo(*faces[1]);
  advanceClocks(ndn::time::milliseconds(10));

  Name nodeZeroExtraUser("userPrefix0-1");
  Name nodeOneExtraUser("userPrefix1-1");

  nodes[0]->addUserNode(nodeZeroExtraUser);
  nodes[1]->addUserNode(nodeOneExtraUser);

  nodes[0]->publishName(userPrefixes[0]);
  advanceClocks(ndn::time::milliseconds(10), 100);

  BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(userPrefixes[0]).value_or(-1), 1);
  BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefixes[0]).value_or(-1), 1);

  nodes[0]->publishName(nodeZeroExtraUser);
  advanceClocks(ndn::time::milliseconds(10), 100);

  BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(nodeZeroExtraUser).value_or(-1), 1);
  BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(nodeZeroExtraUser).value_or(-1), 1);

  nodes[1]->publishName(nodeOneExtraUser);
  advanceClocks(ndn::time::milliseconds(10), 100);

  BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(nodeOneExtraUser).value_or(-1), 1);
  BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(nodeOneExtraUser).value_or(-1), 1);
}

BOOST_AUTO_TEST_CASE(MultipleNodes)
{
  for (int i = 0; i < 4; i++) {
    addNode(i);
  }

  for (int i = 0; i < 3; i++) {
    faces[i]->linkTo(*faces[i + 1]);
  }

  nodes[0]->publishName(userPrefixes[0]);
  advanceClocks(ndn::time::milliseconds(10), 100);
  for (int i = 0; i < 4; i++) {
    BOOST_CHECK_EQUAL(nodes[i]->getSeqNo(userPrefixes[0]).value_or(-1), 1);
  }

  nodes[1]->publishName(userPrefixes[1]);
  advanceClocks(ndn::time::milliseconds(10), 100);
  for (int i = 0; i < 4; i++) {
    BOOST_CHECK_EQUAL(nodes[i]->getSeqNo(userPrefixes[1]).value_or(-1), 1);
  }

  nodes[1]->publishName(userPrefixes[1]);
  advanceClocks(ndn::time::milliseconds(10), 100);
  for (int i = 0; i < 4; i++) {
    BOOST_CHECK_EQUAL(nodes[i]->getSeqNo(userPrefixes[1]).value_or(-1), 2);
  }
}

BOOST_AUTO_TEST_CASE(MultipleNodesSimulataneousPublish)
{
  for (int i = 0; i < 4; i++) {
    addNode(i);
  }

  for (int i = 0; i < 3; i++) {
    faces[i]->linkTo(*faces[i + 1]);
  }

  for (int i = 0; i < 4; i++) {
    nodes[i]->publishName(userPrefixes[i]);
  }

  advanceClocks(ndn::time::milliseconds(10), 100);
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      BOOST_CHECK_EQUAL(nodes[i]->getSeqNo(userPrefixes[j]).value_or(-1), 1);
    }
  }

  for (int i = 0; i < 4; i++) {
    nodes[i]->publishName(userPrefixes[i], 4);
  }

  advanceClocks(ndn::time::milliseconds(10), 100);
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      BOOST_CHECK_EQUAL(nodes[i]->getSeqNo(userPrefixes[j]).value_or(-1), 4);
    }
  }
}

BOOST_AUTO_TEST_CASE(NetworkPartition)
{
  for (int i = 0; i < 4; i++) {
    addNode(i);
  }

  for (int i = 0; i < 3; i++) {
    faces[i]->linkTo(*faces[i + 1]);
  }

  nodes[0]->publishName(userPrefixes[0]);
  advanceClocks(ndn::time::milliseconds(10), 100);
  for (int i = 0; i < 4; i++) {
    BOOST_CHECK_EQUAL(nodes[i]->getSeqNo(userPrefixes[0]).value_or(-1), 1);
  }

  for (int i = 0; i < 3; i++) {
    faces[i]->unlink();
  }

  faces[0]->linkTo(*faces[1]);
  faces[2]->linkTo(*faces[3]);

  nodes[0]->publishName(userPrefixes[0]);
  advanceClocks(ndn::time::milliseconds(10), 100);
  BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefixes[0]).value_or(-1), 2);
  BOOST_CHECK_EQUAL(nodes[2]->getSeqNo(userPrefixes[0]).value_or(-1), 1);
  BOOST_CHECK_EQUAL(nodes[3]->getSeqNo(userPrefixes[0]).value_or(-1), 1);

  nodes[1]->publishName(userPrefixes[1], 2);
  advanceClocks(ndn::time::milliseconds(10), 100);
  BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(userPrefixes[1]).value_or(-1), 2);

  nodes[2]->publishName(userPrefixes[2], 2);
  advanceClocks(ndn::time::milliseconds(10), 100);
  BOOST_CHECK_EQUAL(nodes[3]->getSeqNo(userPrefixes[2]).value_or(-1), 2);

  nodes[3]->publishName(userPrefixes[3], 2);
  advanceClocks(ndn::time::milliseconds(10), 100);
  BOOST_CHECK_EQUAL(nodes[2]->getSeqNo(userPrefixes[3]).value_or(-1), 2);

  BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(userPrefixes[3]).value_or(-1), -1);
  BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefixes[3]).value_or(-1), -1);

  for (int i = 0; i < 3; i++) {
    faces[i]->unlink();
  }

  for (int i = 0; i < 3; i++) {
    faces[i]->linkTo(*faces[i + 1]);
  }

  advanceClocks(ndn::time::milliseconds(10), 100);
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      BOOST_CHECK_EQUAL(nodes[i]->getSeqNo(userPrefixes[j]).value_or(-1), 2);
    }
  }
}

BOOST_AUTO_TEST_CASE(IBFOverflow)
{
  addNode(0);
  addNode(1);

  faces[0]->linkTo(*faces[1]);
  advanceClocks(ndn::time::milliseconds(10));

  // 50 > 40 (expected number of entries in IBF)
  for (int i = 0; i < 50; i++) {
    nodes[0]->addUserNode(Name("userNode0-" + to_string(i)));
  }

  for (int i = 0; i < 20; i++) {
    // Suppose all sync data were lost for these:
    nodes[0]->updateSeqNo(Name("userNode0-" + to_string(i)), 1);
  }
  nodes[0]->publishName(Name("userNode0-" + to_string(20)));
  advanceClocks(ndn::time::milliseconds(10), 100);

  for (int i = 0; i <= 20; i++) {
    Name userPrefix("userNode0-" + to_string(i));
    BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefix).value_or(-1), 1);
  }

  for (int i = 21; i < 49; i++) {
    nodes[0]->updateSeqNo(Name("userNode0-" + to_string(i)), 1);
  }
  nodes[0]->publishName(Name("userNode0-49"));
  advanceClocks(ndn::time::milliseconds(10), 100);

  for (int i = 21; i < 49; i++) {
    Name userPrefix("userNode0-" + to_string(i));
    BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefix).value_or(-1), 1);
  }
}

BOOST_AUTO_TEST_CASE(DiffIBFDecodeFailureSimple)
{
  addNode(0);
  addNode(1);

  faces[0]->linkTo(*faces[1]);
  advanceClocks(ndn::time::milliseconds(10));

  // Lowest number that triggers a decode failure for IBF size of 40
  int totalUpdates = 47;

  for (int i = 0; i <= totalUpdates; i++) {
    nodes[0]->addUserNode(Name("userNode0-" + to_string(i)));
    if (i != totalUpdates) {
      nodes[0]->updateSeqNo(Name("userNode0-" + to_string(i)), 1);
    }
  }
  nodes[0]->publishName(Name("userNode0-" + to_string(totalUpdates)));
  advanceClocks(ndn::time::milliseconds(10), 100);

  // No mechanism to recover yet
  for (int i = 0; i <= totalUpdates; i++) {
    Name userPrefix("userNode0-" + to_string(i));
    BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefix).value_or(-1), 1);
  }

  BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(userPrefixes[1]).value_or(-1), -1);
  BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefixes[0]).value_or(-1), -1);

  nodes[1]->publishName(userPrefixes[1]);
  advanceClocks(ndn::time::milliseconds(10), 100);
  BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(userPrefixes[1]).value_or(-1), 1);

  nodes[0]->publishName(userPrefixes[0]);
  advanceClocks(ndn::time::milliseconds(10), 100);
  BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefixes[0]).value_or(-1), 1);
}

BOOST_AUTO_TEST_CASE(DiffIBFDecodeFailureSimpleSegmentedRecovery)
{
  addNode(0);
  addNode(1);
  faces[0]->linkTo(*faces[1]);

  advanceClocks(ndn::time::milliseconds(10));

  // Lowest number that triggers a decode failure for IBF size of 40
  int totalUpdates = 270;

  for (int i = 0; i <= totalUpdates; i++) {
    nodes[0]->addUserNode(Name("userNode0-" + to_string(i)));
    if (i != totalUpdates) {
      nodes[0]->updateSeqNo(Name("userNode0-" + to_string(i)), 1);
    }
  }
  nodes[0]->publishName(Name("userNode0-" + to_string(totalUpdates)));
  advanceClocks(ndn::time::milliseconds(10), 100);

  // No mechanism to recover yet
  for (int i = 0; i <= totalUpdates; i++) {
    Name userPrefix("userNode0-" + to_string(i));
    BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefix).value_or(-1), 1);
  }

  BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(userPrefixes[1]).value_or(-1), -1);
  BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefixes[0]).value_or(-1), -1);

  nodes[1]->publishName(userPrefixes[1]);
  advanceClocks(ndn::time::milliseconds(10), 100);
  BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(userPrefixes[1]).value_or(-1), 1);

  nodes[0]->publishName(userPrefixes[0]);
  advanceClocks(ndn::time::milliseconds(10), 100);
  BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefixes[0]).value_or(-1), 1);
}

BOOST_AUTO_TEST_CASE(DiffIBFDecodeFailureMultipleNodes)
{
  for (int i = 0; i < 4; i++) {
    addNode(i);
  }

  for (int i = 0; i < 3; i++) {
    faces[i]->linkTo(*faces[i + 1]);
  }

  // Lowest number that triggers a decode failure for IBF size of 40
  int totalUpdates = 47;

  for (int i = 0; i <= totalUpdates; i++) {
    nodes[0]->addUserNode(Name("userNode0-" + to_string(i)));
    if (i != totalUpdates) {
      nodes[0]->updateSeqNo(Name("userNode0-" + to_string(i)), 1);
    }
  }
  nodes[0]->publishName(Name("userNode0-" + to_string(totalUpdates)));
  advanceClocks(ndn::time::milliseconds(10), 100);

  for (int i = 0; i <= totalUpdates; i++) {
    Name userPrefix("userNode0-" + to_string(i));
    for (int j = 0; j < 4; j++) {
      BOOST_CHECK_EQUAL(nodes[j]->getSeqNo(userPrefix).value_or(-1), 1);
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace psync