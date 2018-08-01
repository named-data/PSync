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

#include "segment-publisher.hpp"

#include <ndn-cxx/name-component.hpp>

namespace psync {

SegmentPublisher::SegmentPublisher(ndn::Face& face, ndn::KeyChain& keyChain,
                                   size_t imsLimit)
  : m_face(face)
  , m_scheduler(m_face.getIoService())
  , m_keyChain(keyChain)
  , m_ims(imsLimit)
{
}

void
SegmentPublisher::publish(const ndn::Name& interestName, const ndn::Name& dataName,
                          const ndn::Block& block, ndn::time::milliseconds freshness)
{
  uint64_t interestSegment = 0;
  if (interestName[-1].isSegment()) {
    interestSegment = interestName[-1].toSegment();
  }

  ndn::EncodingBuffer buffer;
  buffer.prependBlock(std::move(block));

  const uint8_t* rawBuffer = buffer.buf();
  const uint8_t* segmentBegin = rawBuffer;
  const uint8_t* end = rawBuffer + buffer.size();

  size_t maxPacketSize = (ndn::MAX_NDN_PACKET_SIZE >> 1);

  uint64_t totalSegments = buffer.size() / maxPacketSize;

  uint64_t segmentNo = 0;
  do {
    const uint8_t* segmentEnd = segmentBegin + maxPacketSize;
    if (segmentEnd > end) {
      segmentEnd = end;
    }

    ndn::Name segmentName(dataName);
    segmentName.appendSegment(segmentNo);

    // We get a std::exception: bad_weak_ptr from m_ims if we don't use shared_ptr for data
    std::shared_ptr<ndn::Data> data = std::make_shared<ndn::Data>(segmentName);
    data->setContent(segmentBegin, segmentEnd - segmentBegin);
    data->setFreshnessPeriod(freshness);
    data->setFinalBlock(ndn::name::Component::fromSegment(totalSegments));

    segmentBegin = segmentEnd;

    m_keyChain.sign(*data);

    // Put on face only the segment which has a pending interest
    // otherwise the segment is unsolicited
    if (interestSegment == segmentNo) {
      m_face.put(*data);
    }

    m_ims.insert(*data, freshness);
    m_scheduler.scheduleEvent(freshness,
                                [this, segmentName] {
                                m_ims.erase(segmentName);
                              });

    ++segmentNo;
  } while (segmentBegin < end);
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
