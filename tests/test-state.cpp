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

#include "PSync/detail/state.hpp"

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

  // Simulate getting buffer content from segment fetcher
  Data data;
  data.setContent(state.wireEncode());
  ndn::Buffer buffer(data.getContent().value_size());
  std::copy(data.getContent().value_begin(),
            data.getContent().value_end(),
            buffer.begin());

  ndn::ConstBufferPtr bufferPtr = make_shared<ndn::Buffer>(buffer);

  ndn::Block block(std::move(bufferPtr));

  State rcvdState;
  rcvdState.wireDecode(block);

  BOOST_CHECK(state.getContent() == rcvdState.getContent());
}

BOOST_AUTO_TEST_CASE(EmptyContent)
{
  State state;

  // Simulate getting buffer content from segment fetcher
  Data data;
  data.setContent(state.wireEncode());
  ndn::Buffer buffer(data.getContent().value_size());
  std::copy(data.getContent().value_begin(),
            data.getContent().value_end(),
            buffer.begin());
  ndn::ConstBufferPtr bufferPtr = make_shared<ndn::Buffer>(buffer);

  ndn::Block block(std::move(bufferPtr));

  BOOST_CHECK_NO_THROW(State state2(block));

  State state2(block);
  BOOST_CHECK_EQUAL(state2.getContent().size(), 0);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace psync