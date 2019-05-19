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

#include "PSync/consumer.hpp"
#include "unit-test-time-fixture.hpp"

#include <boost/test/unit_test.hpp>
#include <ndn-cxx/name.hpp>
#include <ndn-cxx/util/dummy-client-face.hpp>

namespace psync {

using namespace ndn;
using namespace std;

BOOST_AUTO_TEST_SUITE(TestConsumer)

BOOST_AUTO_TEST_CASE(Constructor)
{
  util::DummyClientFace face({true, true});
  BOOST_REQUIRE_NO_THROW(Consumer(Name("/psync"),
                                  face,
                                  [] (const vector<Name>&) {},
                                  [] (const vector<MissingDataInfo>&) {},
                                  40,
                                  0.001));
}

BOOST_AUTO_TEST_CASE(AddSubscription)
{
  util::DummyClientFace face({true, true});
  Consumer consumer(Name("/psync"), face,
                    [] (const vector<Name>&) {},
                    [] (const vector<MissingDataInfo>&) {},
                    40, 0.001);

  Name subscription("test");

  BOOST_CHECK(!consumer.isSubscribed(subscription));
  BOOST_CHECK(consumer.addSubscription(subscription));
  BOOST_CHECK(!consumer.addSubscription(subscription));
}

BOOST_FIXTURE_TEST_CASE(ConstantTimeoutForFirstSegment, ndn::tests::UnitTestTimeFixture)
{
  util::DummyClientFace face(io, {true, true});
  Consumer consumer(Name("/psync"), face,
                    [] (const vector<Name>&) {},
                    [] (const vector<MissingDataInfo>&) {},
                    40, 0.001,
                    ndn::time::milliseconds(4000),
                    ndn::time::milliseconds(4000));

  consumer.sendHelloInterest();
  advanceClocks(ndn::time::milliseconds(4000));
  BOOST_CHECK_EQUAL(face.sentInterests.size(), 1);
  face.sentInterests.clear();
  consumer.stop();

  consumer.m_iblt = ndn::Name("test");
  consumer.sendSyncInterest();
  advanceClocks(ndn::time::milliseconds(4000));
  BOOST_CHECK_EQUAL(face.sentInterests.size(), 1);
  consumer.stop();
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace psync