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
 **/

#include "PSync/segment-publisher.hpp"

namespace psync {

SegmentPublisher::SegmentPublisher(ndn::Face& face, ndn::KeyChain& keyChain,
                                   const ndn::security::SigningInfo& signingInfo, size_t imsLimit)
  : m_face(face)
  , m_scheduler(m_face.getIoContext())
  , m_segmenter(keyChain, signingInfo)
  , m_ims(imsLimit)
{
}

void
SegmentPublisher::publish(const ndn::Name& interestName, const ndn::Name& dataName,
                          ndn::span<const uint8_t> buffer, ndn::time::milliseconds freshness)
{
  auto segments = m_segmenter.segment(buffer, ndn::Name(dataName).appendVersion(),
                                      ndn::MAX_NDN_PACKET_SIZE >> 1, freshness);
  for (const auto& data : segments) {
    m_ims.insert(*data, freshness);
    m_scheduler.schedule(freshness, [this, name = data->getName()] { m_ims.erase(name); });
  }

  // Put on face only the segment which has a pending interest,
  // otherwise the segment is unsolicited
  uint64_t interestSegment = 0;
  if (interestName[-1].isSegment()) {
    interestSegment = interestName[-1].toSegment();
  }
  if (interestSegment < segments.size()) {
    m_face.put(*segments[interestSegment]);
  }
}

bool
SegmentPublisher::replyFromStore(const ndn::Name& interestName)
{
  auto it = m_ims.find(interestName);
  if (it != nullptr) {
    m_face.put(*it);
    return true;
  }
  return false;
}

} // namespace psync
