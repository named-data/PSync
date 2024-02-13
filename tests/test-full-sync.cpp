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

#include "PSync/full-producer.hpp"
#include "PSync/consumer.hpp"
#include "PSync/detail/state.hpp"
#include "PSync/detail/util.hpp"

#include "tests/boost-test.hpp"
#include "tests/io-fixture.hpp"
#include "tests/key-chain-fixture.hpp"

#include <array>
#include <ndn-cxx/util/dummy-client-face.hpp>

namespace psync::tests {

using ndn::Interest;
using ndn::Name;

class FullSyncFixture : public IoFixture, public KeyChainFixture
{
protected:
  void
  addNode(int id)
  {
    BOOST_ASSERT(id >= 0 && id < MAX_NODES);
    userPrefixes[id] = "/userPrefix" + std::to_string(id);
    faces[id] = std::make_unique<ndn::DummyClientFace>(m_io, m_keyChain,
                                                       ndn::DummyClientFace::Options{true, true});
    FullProducer::Options opts;
    opts.ibfCount = 40;
    nodes[id] = std::make_unique<FullProducer>(*faces[id], m_keyChain, syncPrefix, opts);
    nodes[id]->addUserNode(userPrefixes[id]);
  }

  void
  clearNodes()
  {
    nodes = {};
    faces = {};
    userPrefixes = {};
  }

  /**
   * @brief Return a user prefix in the form /userNode<id>-<i>.
   * @param id update originator node index.
   * @param i user prefix index.
   */
  static Name
  makeSubPrefix(int id, int i)
  {
    return "/userNode" + std::to_string(id) + "-" + std::to_string(i);
  }

  /**
   * @brief Publish a batch of updates.
   * @param id node index.
   * @param min minimum user prefix index.
   * @param min maximum user prefix index.
   * @param seq update sequence number.
   * @post nodes[id] has user nodes /userNode<id>-<i> ∀i∈[min,max] , with sequence number
   *       set to @p seq ; only one sync Data may be sent after the last update.
   */
  void
  batchUpdate(int id, int min, int max, uint64_t seq)
  {
    FullProducer& node = *nodes.at(id);
    for (int i = min; i <= max; i++) {
      auto userPrefix = makeSubPrefix(id, i);
      node.addUserNode(userPrefix);
      if (i < max) {
        node.updateSeqNo(userPrefix, seq);
      }
      else {
        node.publishName(userPrefix, seq);
      }
    }
  }

  /**
   * @brief Check sequence number on a batch of user prefixes.
   * @param id node index where the check is performed.
   * @param origin update originator node index for deriving user prefixes.
   * @param min minimum user prefix index.
   * @param max maximum user prefix index.
   * @param seq expected sequence number.
   */
  void
  batchCheck(int id, int origin, int min, int max, std::optional<uint64_t> seq)
  {
    uint64_t expected = seq.value_or(NOT_EXIST);
    FullProducer& node = *nodes.at(id);
    for (int i = min; i <= max; i++) {
      auto userPrefix = makeSubPrefix(origin, i);
      BOOST_TEST_CONTEXT("node=" << id << " userPrefix=" << userPrefix) {
        BOOST_CHECK_EQUAL(node.getSeqNo(userPrefix).value_or(NOT_EXIST), expected);
      }
    }
  }

  struct IbfDecodeFailureCounts
  {
    size_t aboveThreshold = 0;
    size_t belowThreshold = 0;
  };

  /**
   * @brief Return the sum of IBF decode failure counters among created nodes.
   */
  IbfDecodeFailureCounts
  countIbfDecodeFailures() const
  {
    IbfDecodeFailureCounts result;
    for (const auto& node : nodes) {
      if (node == nullptr) {
        continue;
      }
      result.aboveThreshold += node->nIbfDecodeFailuresAboveThreshold;
      result.belowThreshold += node->nIbfDecodeFailuresBelowThreshold;
    }
    return result;
  }

  /**
   * @brief Repeat a test function until there are IBF decode failures.
   * @param minTotalUpdates minimum totalUpdates parameter.
   * @param maxTotalUpdates maximum totalUpdates parameter.
   * @param f test function.
   *
   * This method searches for totalUpdates ∈ [minTotalUpdates,maxTotalUpdates] until
   * there is at least one execution that caused an IBF decode failure above threshold.
   * If such an execution never occurs within the range, the test case fails.
   *
   * Current FullSync logic cannot reliably recover from an IBF decode failure below threshold.
   * Hence, that condition is not tested.
   */
  void
  searchIbfDecodeFailures(int minTotalUpdates, int maxTotalUpdates,
                          const std::function<void(int totalUpdates)>& f)
  {
    bool hasAboveThreshold = false;
    for (int totalUpdates = minTotalUpdates; totalUpdates <= maxTotalUpdates; ++totalUpdates) {
      clearNodes();
      BOOST_TEST_CONTEXT("totalUpdates=" << totalUpdates) {
        f(totalUpdates);

        auto cnt = countIbfDecodeFailures();
        BOOST_TEST_MESSAGE("aboveThreshold=" << cnt.aboveThreshold << " "
                           "belowThreshold=" << cnt.belowThreshold);
        hasAboveThreshold = hasAboveThreshold || cnt.aboveThreshold > 0;
        if (hasAboveThreshold) {
          return;
        }
      }
    }
    BOOST_TEST_FAIL("cannot find viable totalUpdates for IBF decode failures");
  }

protected:
  const Name syncPrefix = "/psync";
  static constexpr int MAX_NODES = 4;
  std::array<Name, MAX_NODES> userPrefixes;
  std::array<std::unique_ptr<ndn::DummyClientFace>, MAX_NODES> faces;
  std::array<std::unique_ptr<FullProducer>, MAX_NODES> nodes;
  static constexpr uint64_t NOT_EXIST = std::numeric_limits<uint64_t>::max();
};

BOOST_FIXTURE_TEST_SUITE(TestFullSync, FullSyncFixture)

BOOST_AUTO_TEST_CASE(TwoNodesSimple)
{
  addNode(0);
  addNode(1);

  faces[0]->linkTo(*faces[1]);
  advanceClocks(10_ms);

  nodes[0]->publishName(userPrefixes[0]);
  advanceClocks(10_ms, 100);

  BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(userPrefixes[0]).value_or(NOT_EXIST), 1);
  BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefixes[0]).value_or(NOT_EXIST), 1);

  nodes[1]->publishName(userPrefixes[1]);
  advanceClocks(10_ms, 100);
  BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(userPrefixes[1]).value_or(NOT_EXIST), 1);
  BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefixes[1]).value_or(NOT_EXIST), 1);

  nodes[1]->publishName(userPrefixes[1]);
  advanceClocks(10_ms, 100);
  BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(userPrefixes[1]).value_or(NOT_EXIST), 2);
  BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefixes[1]).value_or(NOT_EXIST), 2);
}

BOOST_AUTO_TEST_CASE(TwoNodesForceSeqNo)
{
  addNode(0);
  addNode(1);

  faces[0]->linkTo(*faces[1]);
  advanceClocks(10_ms);

  nodes[0]->publishName(userPrefixes[0], 3);
  advanceClocks(10_ms, 100);

  BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(userPrefixes[0]).value_or(NOT_EXIST), 3);
  BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefixes[0]).value_or(NOT_EXIST), 3);
}

BOOST_AUTO_TEST_CASE(TwoNodesWithMultipleUserNodes)
{
  addNode(0);
  addNode(1);

  faces[0]->linkTo(*faces[1]);
  advanceClocks(10_ms);

  Name nodeZeroExtraUser("/userPrefix0-1");
  Name nodeOneExtraUser("/userPrefix1-1");

  nodes[0]->addUserNode(nodeZeroExtraUser);
  nodes[1]->addUserNode(nodeOneExtraUser);

  nodes[0]->publishName(userPrefixes[0]);
  advanceClocks(10_ms, 100);

  BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(userPrefixes[0]).value_or(NOT_EXIST), 1);
  BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefixes[0]).value_or(NOT_EXIST), 1);

  nodes[0]->publishName(nodeZeroExtraUser);
  advanceClocks(10_ms, 100);

  BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(nodeZeroExtraUser).value_or(NOT_EXIST), 1);
  BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(nodeZeroExtraUser).value_or(NOT_EXIST), 1);

  nodes[1]->publishName(nodeOneExtraUser);
  advanceClocks(10_ms, 100);

  BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(nodeOneExtraUser).value_or(NOT_EXIST), 1);
  BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(nodeOneExtraUser).value_or(NOT_EXIST), 1);
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
  advanceClocks(10_ms, 100);
  for (int i = 0; i < 4; i++) {
    BOOST_CHECK_EQUAL(nodes[i]->getSeqNo(userPrefixes[0]).value_or(NOT_EXIST), 1);
  }

  nodes[1]->publishName(userPrefixes[1]);
  advanceClocks(10_ms, 100);
  for (int i = 0; i < 4; i++) {
    BOOST_CHECK_EQUAL(nodes[i]->getSeqNo(userPrefixes[1]).value_or(NOT_EXIST), 1);
  }

  nodes[1]->publishName(userPrefixes[1]);
  advanceClocks(10_ms, 100);
  for (int i = 0; i < 4; i++) {
    BOOST_CHECK_EQUAL(nodes[i]->getSeqNo(userPrefixes[1]).value_or(NOT_EXIST), 2);
  }
}

BOOST_AUTO_TEST_CASE(MultipleNodesSimultaneousPublish)
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

  advanceClocks(10_ms, 100);
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      BOOST_CHECK_EQUAL(nodes[i]->getSeqNo(userPrefixes[j]).value_or(NOT_EXIST), 1);
    }
  }

  for (int i = 0; i < 4; i++) {
    nodes[i]->publishName(userPrefixes[i], 4);
  }

  advanceClocks(10_ms, 100);
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      BOOST_CHECK_EQUAL(nodes[i]->getSeqNo(userPrefixes[j]).value_or(NOT_EXIST), 4);
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
  advanceClocks(10_ms, 100);
  for (int i = 0; i < 4; i++) {
    BOOST_CHECK_EQUAL(nodes[i]->getSeqNo(userPrefixes[0]).value_or(NOT_EXIST), 1);
  }

  for (int i = 0; i < 3; i++) {
    faces[i]->unlink();
  }
  faces[0]->linkTo(*faces[1]);
  faces[2]->linkTo(*faces[3]);

  nodes[0]->publishName(userPrefixes[0]);
  advanceClocks(10_ms, 100);
  BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefixes[0]).value_or(NOT_EXIST), 2);
  BOOST_CHECK_EQUAL(nodes[2]->getSeqNo(userPrefixes[0]).value_or(NOT_EXIST), 1);
  BOOST_CHECK_EQUAL(nodes[3]->getSeqNo(userPrefixes[0]).value_or(NOT_EXIST), 1);

  nodes[1]->publishName(userPrefixes[1], 2);
  advanceClocks(10_ms, 100);
  BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(userPrefixes[1]).value_or(NOT_EXIST), 2);

  nodes[2]->publishName(userPrefixes[2], 2);
  advanceClocks(10_ms, 100);
  BOOST_CHECK_EQUAL(nodes[3]->getSeqNo(userPrefixes[2]).value_or(NOT_EXIST), 2);

  nodes[3]->publishName(userPrefixes[3], 2);
  advanceClocks(10_ms, 100);
  BOOST_CHECK_EQUAL(nodes[2]->getSeqNo(userPrefixes[3]).value_or(NOT_EXIST), 2);

  BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(userPrefixes[3]).value_or(NOT_EXIST), NOT_EXIST);
  BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefixes[3]).value_or(NOT_EXIST), NOT_EXIST);

  for (int i = 0; i < 3; i++) {
    faces[i]->unlink();
  }
  for (int i = 0; i < 3; i++) {
    faces[i]->linkTo(*faces[i + 1]);
  }

  advanceClocks(10_ms, 100);
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      BOOST_CHECK_EQUAL(nodes[i]->getSeqNo(userPrefixes[j]).value_or(NOT_EXIST), 2);
    }
  }
}

BOOST_AUTO_TEST_CASE(IBFOverflow)
{
  addNode(0);
  addNode(1);

  faces[0]->linkTo(*faces[1]);
  advanceClocks(10_ms);

  // 50 > 40 (expected number of entries in IBF)
  for (int i = 0; i < 50; i++) {
    nodes[0]->addUserNode(makeSubPrefix(0, i));
  }
  batchUpdate(0, 0, 20, 1);
  advanceClocks(10_ms, 100);
  batchCheck(1, 0, 0, 20, 1);

  batchUpdate(0, 21, 49, 1);
  advanceClocks(10_ms, 100);
  batchCheck(1, 0, 21, 49, 1);
}

BOOST_AUTO_TEST_CASE(DiffIBFDecodeFailureSimple)
{
  searchIbfDecodeFailures(46, 52, [this] (int totalUpdates) {
    addNode(0);
    addNode(1);

    faces[0]->linkTo(*faces[1]);
    advanceClocks(10_ms);

    batchUpdate(0, 0, totalUpdates, 1);
    advanceClocks(10_ms, 100);
    batchCheck(1, 0, 0, totalUpdates, 1);

    BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(userPrefixes[1]).value_or(NOT_EXIST), NOT_EXIST);
    BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefixes[0]).value_or(NOT_EXIST), NOT_EXIST);

    nodes[1]->publishName(userPrefixes[1]);
    advanceClocks(10_ms, 100);
    BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(userPrefixes[1]).value_or(NOT_EXIST), 1);

    nodes[0]->publishName(userPrefixes[0]);
    advanceClocks(10_ms, 100);
    BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefixes[0]).value_or(NOT_EXIST), 1);
  });
}

BOOST_AUTO_TEST_CASE(DiffIBFDecodeFailureSimpleSegmentedRecovery)
{
  searchIbfDecodeFailures(46, 52, [this] (int totalUpdates) {
    addNode(0);
    addNode(1);
    faces[0]->linkTo(*faces[1]);

    advanceClocks(10_ms);

    batchUpdate(0, 0, totalUpdates, 1);
    advanceClocks(10_ms, 100);
    batchCheck(1, 0, 0, totalUpdates, 1);

    BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(userPrefixes[1]).value_or(NOT_EXIST), NOT_EXIST);
    BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefixes[0]).value_or(NOT_EXIST), NOT_EXIST);

    nodes[1]->publishName(userPrefixes[1]);
    advanceClocks(10_ms, 100);
    BOOST_CHECK_EQUAL(nodes[0]->getSeqNo(userPrefixes[1]).value_or(NOT_EXIST), 1);

    nodes[0]->publishName(userPrefixes[0]);
    advanceClocks(10_ms, 100);
    BOOST_CHECK_EQUAL(nodes[1]->getSeqNo(userPrefixes[0]).value_or(NOT_EXIST), 1);
  });
}

BOOST_AUTO_TEST_CASE(DiffIBFDecodeFailureMultipleNodes)
{
  searchIbfDecodeFailures(46, 52, [this] (int totalUpdates) {
    for (int i = 0; i < 4; i++) {
      addNode(i);
    }
    for (int i = 0; i < 3; i++) {
      faces[i]->linkTo(*faces[i + 1]);
    }

    batchUpdate(0, 0, totalUpdates, 1);
    advanceClocks(10_ms, 100);
    for (int i = 0; i < 4; i++) {
      batchCheck(i, 0, 0, totalUpdates, 1);
    }
  });
}

BOOST_AUTO_TEST_CASE(DelayedSecondSegment)
{
  addNode(0);

  int i = 0;
  detail::State state;
  std::shared_ptr<ndn::Buffer> compressed;
  do {
    auto prefixToPublish = makeSubPrefix(0, i++);
    nodes[0]->addUserNode(prefixToPublish);
    nodes[0]->publishName(prefixToPublish);

    state.addContent(Name(prefixToPublish).appendNumber(nodes[0]->m_prefixes[prefixToPublish]));

    auto block = state.wireEncode();
    compressed = detail::compress(nodes[0]->m_contentCompression, block);
  } while (compressed->size() < (ndn::MAX_NDN_PACKET_SIZE >> 1));

  advanceClocks(10_ms, 100);

  Name syncInterestName(syncPrefix);
  detail::IBLT iblt(40, nodes[0]->m_ibltCompression);
  iblt.appendToName(syncInterestName);

  nodes[0]->onSyncInterest(syncPrefix, Interest(syncInterestName));

  advanceClocks(10_ms);

  BOOST_CHECK_EQUAL(nodes[0]->m_segmentPublisher.m_ims.size(), 2);
  // Expire contents from segmentPublisher
  advanceClocks(10_ms, 100);
  BOOST_CHECK_EQUAL(nodes[0]->m_segmentPublisher.m_ims.size(), 0);

  // Get data name from face and increase segment number to form next interest
  BOOST_REQUIRE(!faces[0]->sentData.empty());
  Name dataName = faces[0]->sentData.front().getName();
  Name interestName = dataName.getPrefix(-1).appendSegment(1);
  faces[0]->sentData.clear();

  nodes[0]->onSyncInterest(syncPrefix, Interest(interestName));
  advanceClocks(10_ms);

  // Should have repopulated SegmentPublisher
  BOOST_CHECK_EQUAL(nodes[0]->m_segmentPublisher.m_ims.size(), 2);
  // Should have received the second data segment this time
  BOOST_REQUIRE(!faces[0]->sentData.empty());
  BOOST_CHECK_EQUAL(faces[0]->sentData.front().getName().at(-1).toSegment(), 1);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace psync::tests
