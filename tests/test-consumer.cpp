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
 */

#include "PSync/consumer.hpp"

#include "tests/boost-test.hpp"
#include "tests/io-fixture.hpp"

#include <ndn-cxx/util/dummy-client-face.hpp>

namespace psync {

using ndn::Name;

BOOST_AUTO_TEST_SUITE(TestConsumer)

BOOST_AUTO_TEST_CASE(AddSubscription)
{
  ndn::DummyClientFace face;
  Consumer consumer(Name("/psync"), face,
                    [] (const auto&) {},
                    [] (const auto&) {},
                    40, 0.001);

  Name subscription("test");

  BOOST_CHECK(!consumer.isSubscribed(subscription));
  BOOST_CHECK(consumer.addSubscription(subscription, 0));
  BOOST_CHECK(!consumer.addSubscription(subscription, 0));
}

BOOST_AUTO_TEST_CASE(RemoveSubscription)
{
  ndn::DummyClientFace face;
  Consumer consumer(Name("/psync"), face,
                    [] (const auto&) {},
                    [] (const auto&) {},
                    40, 0.001);

  Name subscription("test");
  consumer.addSubscription(subscription, 0);

  BOOST_CHECK(consumer.isSubscribed(subscription));
  BOOST_CHECK(consumer.removeSubscription(subscription));
  BOOST_CHECK(!consumer.removeSubscription(subscription));
  BOOST_CHECK(!consumer.isSubscribed(subscription));
}

BOOST_FIXTURE_TEST_CASE(ConstantTimeoutForFirstSegment, tests::IoFixture)
{
  ndn::DummyClientFace face(m_io);
  Consumer consumer(Name("/psync"), face,
                    [] (const auto&) {},
                    [] (const auto&) {},
                    40, 0.001, 4_s, 4_s);

  consumer.sendHelloInterest();
  advanceClocks(4_s);
  BOOST_CHECK_EQUAL(face.sentInterests.size(), 1);
  face.sentInterests.clear();
  consumer.stop();

  consumer.m_iblt = Name("test");
  consumer.sendSyncInterest();
  advanceClocks(3999_ms);
  BOOST_CHECK_EQUAL(face.sentInterests.size(), 1);
  consumer.stop();
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace psync
