/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2022,  The University of Memphis
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

#include "PSync/detail/state.hpp"

#include <ndn-cxx/util/exception.hpp>
#include <ndn-cxx/util/ostream-joiner.hpp>

namespace psync::detail {

State::State(const ndn::Block& block)
{
  wireDecode(block);
}

void
State::addContent(const ndn::Name& prefix)
{
  m_wire.reset();
  m_content.emplace_back(prefix);
}

const ndn::Block&
State::wireEncode() const
{
  if (m_wire.hasWire()) {
    return m_wire;
  }

  ndn::EncodingEstimator estimator;
  size_t estimatedSize = wireEncode(estimator);

  ndn::EncodingBuffer buffer(estimatedSize, 0);
  wireEncode(buffer);

  m_wire = buffer.block();
  return m_wire;
}

template<ndn::encoding::Tag TAG>
size_t
State::wireEncode(ndn::EncodingImpl<TAG>& block) const
{
  size_t totalLength = 0;

  for (auto it = m_content.rbegin(); it != m_content.rend(); ++it) {
    totalLength += it->wireEncode(block);
  }

  totalLength += block.prependVarNumber(totalLength);
  totalLength += block.prependVarNumber(tlv::PSyncContent);

  return totalLength;
}

NDN_CXX_DEFINE_WIRE_ENCODE_INSTANTIATIONS(State);

void
State::wireDecode(const ndn::Block& wire)
{
  if (wire.type() != tlv::PSyncContent) {
    NDN_THROW(ndn::tlv::Error("PSyncContent", wire.type()));
  }

  m_content.clear();

  wire.parse();
  m_wire = wire;

  for (auto it = m_wire.elements_begin(); it != m_wire.elements_end(); ++it) {
    if (it->type() == ndn::tlv::Name) {
      m_content.emplace_back(*it);
    }
    else {
      NDN_THROW(ndn::tlv::Error("Name", it->type()));
    }
  }
}

std::ostream&
operator<<(std::ostream& os, const State& state)
{
  auto content = state.getContent();

  os << "[";
  std::copy(content.begin(), content.end(), ndn::make_ostream_joiner(os, ", "));
  os << "]";

  return os;
}

} // namespace psync::detail
