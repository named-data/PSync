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
 */

#ifndef PSYNC_SEGMENT_PUBLISHER_HPP
#define PSYNC_SEGMENT_PUBLISHER_HPP

#include "PSync/detail/access-specifiers.hpp"

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/name.hpp>
#include <ndn-cxx/ims/in-memory-storage-fifo.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <ndn-cxx/util/time.hpp>

namespace psync {

const int MAX_SEGMENTS_STORED = 100;

/**
 * @brief Segment Publisher to publish segmented data
 *
 */
class SegmentPublisher
{
public:
  SegmentPublisher(ndn::Face& face, ndn::KeyChain& keyChain,
                   size_t imsLimit = MAX_SEGMENTS_STORED);

  /**
   * @brief Put all the segments in memory.
   *
   * @param interestName the interest name, to determine the sequence to be answered immediately
   * @param dataName the data name, has components after interest name
   * @param block the content of the data
   * @param freshness freshness of the segments
   * @param signingInfo signing info to sign the data with
   */
  void
  publish(const ndn::Name& interestName, const ndn::Name& dataName,
          const ndn::Block& block, ndn::time::milliseconds freshness,
          const ndn::security::SigningInfo& signingInfo = ndn::security::SigningInfo());

  /**
   * @brief Put all the segments in memory.
   *
   * @param interestName the interest name, to determine the sequence to be answered immediately
   * @param dataName the data name, has components after interest name
   * @param buffer the content of the data
   * @param freshness freshness of the segments
   * @param signingInfo signing info to sign the data with
   */
  void
  publish(const ndn::Name& interestName, const ndn::Name& dataName,
          const std::shared_ptr<const ndn::Buffer>& buffer, ndn::time::milliseconds freshness,
          const ndn::security::SigningInfo& signingInfo = ndn::security::SigningInfo());

  /**
   * @brief Try to reply from memory, return false if we cannot find the segment.
   *
   * The caller is then expected to use publish if this returns false.
   */
  bool
  replyFromStore(const ndn::Name& interestName);

private:
  ndn::Face& m_face;
  ndn::Scheduler m_scheduler;
  ndn::KeyChain& m_keyChain;

PSYNC_PUBLIC_WITH_TESTS_ELSE_PRIVATE:
  ndn::InMemoryStorageFifo m_ims;
};

} // namespace psync

#endif // PSYNC_SEGMENT_PUBLISHER_HPP
