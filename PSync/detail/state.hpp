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

#ifndef PSYNC_DETAIL_STATE_HPP
#define PSYNC_DETAIL_STATE_HPP

#include <ndn-cxx/name.hpp>

namespace psync::tlv {

enum {
  PSyncContent = 128
};

} // namespace psync::tlv

namespace psync::detail {

class State
{
public:
  State() = default;

  explicit
  State(const ndn::Block& block);

  void
  addContent(const ndn::Name& prefix);

  const std::vector<ndn::Name>&
  getContent() const
  {
    return m_content;
  }

  const ndn::Block&
  wireEncode() const;

  template<ndn::encoding::Tag TAG>
  size_t
  wireEncode(ndn::EncodingImpl<TAG>& block) const;

  void
  wireDecode(const ndn::Block& wire);

  std::vector<ndn::Name>::const_iterator
  begin() const
  {
    return m_content.cbegin();
  }

  std::vector<ndn::Name>::const_iterator
  end() const
  {
    return m_content.cend();
  }

private:
  std::vector<ndn::Name> m_content;
  mutable ndn::Block m_wire;
};

NDN_CXX_DECLARE_WIRE_ENCODE_INSTANTIATIONS(State);

std::ostream&
operator<<(std::ostream& os, const State& State);

} // namespace psync::detail

#endif // PSYNC_DETAIL_STATE_HPP
