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

#include "detail/state.hpp"

#include <boost/test/unit_test.hpp>
#include <ndn-cxx/name.hpp>
#include <ndn-cxx/data.hpp>

namespace psync {

using namespace ndn;

BOOST_AUTO_TEST_SUITE(TestState)

BOOST_AUTO_TEST_CASE(EncodeDecode)
{
  State state;
  state.addContent(ndn::Name("test1"));
  state.addContent(ndn::Name("test2"));

  ndn::Data data;
  data.setContent(state.wireEncode());
  State rcvdState(data.getContent());

  BOOST_CHECK(state.getContent() == rcvdState.getContent());

  // Simulate getting buffer content from segment fetcher
  ndn::Buffer buffer(data.getContent().value_size());
  std::copy(data.getContent().value_begin(),
            data.getContent().value_end(),
            buffer.begin());

  ndn::ConstBufferPtr buffer2 = make_shared<ndn::Buffer>(buffer);

  ndn::Block block(std::move(buffer2));

  State state2;
  state2.wireDecode(block);

  BOOST_CHECK(state.getContent() == state2.getContent());
}

BOOST_AUTO_TEST_CASE(EmptyContent)
{
  ndn::Data data;
  BOOST_CHECK_NO_THROW(State state(data.getContent()));

  State state(data.getContent());
  BOOST_CHECK_EQUAL(state.getContent().size(), 0);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace psync