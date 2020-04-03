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

#include "PSync/segment-publisher.hpp"
#include "PSync/detail/state.hpp"

#include "tests/boost-test.hpp"
#include "tests/unit-test-time-fixture.hpp"

#include <ndn-cxx/data.hpp>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/util/dummy-client-face.hpp>
#include <ndn-cxx/util/segment-fetcher.hpp>
#include <ndn-cxx/security/validator-null.hpp>

namespace psync {

using namespace ndn;

class SegmentPublisherFixture : public tests::UnitTestTimeFixture
{
public:
  SegmentPublisherFixture()
  : face(io, util::DummyClientFace::Options{true, true})
  , publisher(face, keyChain)
  , freshness(1000)
  , numComplete(0)
  , numRepliesFromStore(0)
  {
    face.setInterestFilter(InterestFilter("/hello/world"),
                           bind(&SegmentPublisherFixture::onInterest, this, _1, _2),
                           [] (const ndn::Name& prefix, const std::string& msg) {
                             BOOST_CHECK(false);
                           });
    advanceClocks(ndn::time::milliseconds(10));

    for (int i = 0; i < 1000; ++i) {
      state.addContent(Name("/test").appendNumber(i));
    }
  }

  ~SegmentPublisherFixture() {
    fetcher->stop();
  }

  void
  expressInterest(const Interest& interest) {
    fetcher = util::SegmentFetcher::start(face, interest, ndn::security::v2::getAcceptAllValidator());
    fetcher->onComplete.connect([this] (ConstBufferPtr data) {
                                 numComplete++;
                               });
    fetcher->onError.connect([] (uint32_t errorCode, const std::string& msg) {
                              BOOST_CHECK(false);
                            });

    advanceClocks(ndn::time::milliseconds(10));
  }

  void
  onInterest(const Name& prefix, const Interest& interest) {
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

  util::DummyClientFace face;
  KeyChain keyChain;
  SegmentPublisher publisher;
  shared_ptr<util::SegmentFetcher> fetcher;
  Name dataName;
  time::milliseconds freshness;
  State state;

  int numComplete;
  int numRepliesFromStore;
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

  numRepliesFromStore = 0;
  expressInterest(Interest("/hello/world"));
  BOOST_CHECK_EQUAL(numComplete, 2);
  BOOST_CHECK_EQUAL(numRepliesFromStore, 3);

  advanceClocks(ndn::time::milliseconds(freshness));
  BOOST_CHECK_EQUAL(publisher.m_ims.size(), 0);

  numRepliesFromStore = 0;
  expressInterest(Interest("/hello/world"));
  BOOST_CHECK_EQUAL(numComplete, 3);
  BOOST_CHECK_EQUAL(numRepliesFromStore, 2);

  numRepliesFromStore = 0;
  face.expressInterest(Interest("/hello/world/").setCanBePrefix(true),
                       [this] (const Interest& interest, const Data& data) {
                         numComplete++;
                       },
                       [] (const Interest& interest, const lp::Nack& nack) {
                         BOOST_CHECK(false);
                       },
                       [] (const Interest& interest) {
                         BOOST_CHECK(false);
                       });
  advanceClocks(ndn::time::milliseconds(10));
  BOOST_CHECK_EQUAL(numComplete, 4);
  BOOST_CHECK_EQUAL(numRepliesFromStore, 1);
}

BOOST_AUTO_TEST_CASE(LargerDataName)
{
  BOOST_CHECK_EQUAL(publisher.m_ims.size(), 0);
  dataName = Name("/hello/world/IBF");

  expressInterest(Interest("/hello/world"));
  BOOST_CHECK_EQUAL(numComplete, 1);
  // First segment is answered directly in publish,
  // Rest two are satisfied by the store
  BOOST_CHECK_EQUAL(numRepliesFromStore, 2);
  BOOST_CHECK_EQUAL(publisher.m_ims.size(), 3);

  advanceClocks(ndn::time::milliseconds(freshness));
  BOOST_CHECK_EQUAL(publisher.m_ims.size(), 0);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace psync
