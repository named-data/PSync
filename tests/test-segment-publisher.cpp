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

#include "PSync/segment-publisher.hpp"
#include "PSync/detail/state.hpp"

#include "tests/boost-test.hpp"
#include "tests/io-fixture.hpp"
#include "tests/key-chain-fixture.hpp"

#include <ndn-cxx/security/validator-null.hpp>
#include <ndn-cxx/util/dummy-client-face.hpp>
#include <ndn-cxx/util/segment-fetcher.hpp>

namespace psync::tests {

using namespace ndn::time_literals;
using ndn::Interest;
using ndn::Name;

class SegmentPublisherFixture : public IoFixture, public KeyChainFixture
{
protected:
  SegmentPublisherFixture()
  {
    m_face.setInterestFilter(ndn::InterestFilter("/hello/world"),
                             bind(&SegmentPublisherFixture::onInterest, this, _2),
                             [] (auto&&...) { BOOST_CHECK(false); });
    advanceClocks(10_ms);

    for (int i = 0; i < 1000; ++i) {
      state.addContent(Name("/test").appendNumber(i));
    }
  }

  ~SegmentPublisherFixture() override
  {
    fetcher->stop();
  }

  void
  expressInterest(const Interest& interest)
  {
    fetcher = ndn::SegmentFetcher::start(m_face, interest, ndn::security::getAcceptAllValidator());
    fetcher->onComplete.connect([this] (auto&&...) { numComplete++; });
    fetcher->onError.connect([] (auto&&...) { BOOST_CHECK(false); });

    advanceClocks(10_ms);
  }

  void
  onInterest(const Interest& interest)
  {
    if (publisher.replyFromStore(interest.getName())) {
      numRepliesFromStore++;
      return;
    }

    // If dataName is same as interest name
    if (dataName.empty()) {
      publisher.publish(interest.getName(), interest.getName(), state.wireEncode(), freshness);
    }
    else {
      publisher.publish(interest.getName(), dataName, state.wireEncode(), freshness);
    }
  }

protected:
  ndn::DummyClientFace m_face{m_io, m_keyChain, {true, true}};
  SegmentPublisher publisher{m_face, m_keyChain};
  std::shared_ptr<ndn::SegmentFetcher> fetcher;
  Name dataName;
  detail::State state;

  int numComplete = 0;
  int numRepliesFromStore = 0;

  static constexpr ndn::time::milliseconds freshness = 1_s;
};

BOOST_FIXTURE_TEST_SUITE(TestSegmentPublisher, SegmentPublisherFixture)

BOOST_AUTO_TEST_CASE(Basic)
{
  BOOST_CHECK_EQUAL(publisher.m_ims.size(), 0);

  expressInterest(Interest("/hello/world"));
  BOOST_CHECK_EQUAL(numComplete, 1);
  // First segment is answered directly in publish,
  // Rest two are satisfied by the store
  BOOST_CHECK_EQUAL(numRepliesFromStore, 2);
  BOOST_CHECK_EQUAL(publisher.m_ims.size(), 3);

  for (const auto& data : publisher.m_ims) {
    BOOST_TEST_CONTEXT(data.getName()) {
      BOOST_REQUIRE_EQUAL(data.getName().size(), 4);
      BOOST_CHECK(data.getName()[-1].isSegment());
      BOOST_CHECK(data.getName()[-2].isVersion());
    }
  }

  numRepliesFromStore = 0;
  expressInterest(Interest("/hello/world"));
  BOOST_CHECK_EQUAL(numComplete, 2);
  BOOST_CHECK_EQUAL(numRepliesFromStore, 3);

  advanceClocks(freshness);
  BOOST_CHECK_EQUAL(publisher.m_ims.size(), 0);

  numRepliesFromStore = 0;
  expressInterest(Interest("/hello/world"));
  BOOST_CHECK_EQUAL(numComplete, 3);
  BOOST_CHECK_EQUAL(numRepliesFromStore, 2);

  numRepliesFromStore = 0;
  m_face.expressInterest(Interest("/hello/world").setCanBePrefix(true),
                         [this] (auto&&...) { this->numComplete++; },
                         [] (auto&&...) { BOOST_CHECK(false); },
                         [] (auto&&...) { BOOST_CHECK(false); });
  advanceClocks(10_ms);
  BOOST_CHECK_EQUAL(numComplete, 4);
  BOOST_CHECK_EQUAL(numRepliesFromStore, 1);
}

BOOST_AUTO_TEST_CASE(LongerDataName)
{
  dataName = Name("/hello/world/IBF");
  BOOST_CHECK_EQUAL(publisher.m_ims.size(), 0);

  expressInterest(Interest("/hello/world"));
  BOOST_CHECK_EQUAL(numComplete, 1);
  // First segment is answered directly in publish,
  // Rest two are satisfied by the store
  BOOST_CHECK_EQUAL(numRepliesFromStore, 2);
  BOOST_CHECK_EQUAL(publisher.m_ims.size(), 3);

  for (const auto& data : publisher.m_ims) {
    BOOST_TEST_CONTEXT(data.getName()) {
      BOOST_REQUIRE_EQUAL(data.getName().size(), 5);
      BOOST_CHECK(data.getName()[-1].isSegment());
      BOOST_CHECK(data.getName()[-2].isVersion());
    }
  }

  advanceClocks(freshness);
  BOOST_CHECK_EQUAL(publisher.m_ims.size(), 0);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace psync::tests
