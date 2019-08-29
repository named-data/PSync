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

#ifndef PSYNC_FULL_CONSUMER_HPP
#define PSYNC_FULL_CONSUMER_HPP

#include "PSync/detail/access-specifiers.hpp"
#include "PSync/detail/bloom-filter.hpp"
#include "PSync/detail/util.hpp"

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/random.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <ndn-cxx/util/segment-fetcher.hpp>
#include <ndn-cxx/util/time.hpp>

#include <map>
#include <vector>

namespace psync {

using namespace ndn::literals::time_literals;

typedef std::function<void(const std::vector<MissingDataInfo>&)> UpdateCallback;

const ndn::time::milliseconds HELLO_INTEREST_LIFETIME = 1_s;
const ndn::time::milliseconds SYNC_INTEREST_LIFETIME = 1_s;

/**
 * @brief Pure consumer logic to subscribe producer's data
 *
 * Pure Consumer application sends a periodic sync interest to a multicast face and waits
 * for the data from the producer. It appends a empty IBF to the initial sync interest before
 * sending it. Once the data is received, the old IBF is replaced with the one received from
 * the producer. Onwards, all the future interest contains the IBF received from the producer
 * itself. Currently, fetching of the data needs to be handled by the application.
 */

// most of the logic/code is same as that of normal Consumer
class FullConsumer
{
public:
  /**
   * @brief constructor
   *
   * @param syncPrefix syncPrefix to send hello/sync interests to producer
   * @param face application's face
   * @param syncInterestLifetime lifetime of sync interest
   */
  FullConsumer(const ndn::Name& syncPrefix,
               ndn::Face& face,
               const UpdateCallback& onUpdate,
               ndn::time::milliseconds syncInterestLifetime = SYNC_INTEREST_LIFETIME);

private:
  /**
   * @brief send sync interest /<sync-prefix>/sync/\<consumer-IBF\>
   * initial interest will contain empty IBF, currently hard-coded
   */
  void
  sendSyncInterest();

  /**
   * @brief Stop segment fetcher to stop the sync and free resources
   */
  void
  stop();

  /**
   *
   * Format: <sync-prefix>/sync/\<consumer-old-IBF\>/\<producers-latest-IBF\>
   * Data content is all the prefixes the producer thinks the consumer doesn't have.
   * Since this is consumer only IBF computations, it replaces its own IBF with the
   * one received from the producer.
   *
   * @param bufferPtr sync data content
   */
  void
  onSyncData(const ndn::ConstBufferPtr& bufferPtr);

PSYNC_PUBLIC_WITH_TESTS_ELSE_PRIVATE:
  ndn::Face& m_face;
  ndn::Scheduler m_scheduler;

  ndn::Name m_syncPrefix;
  ndn::Name m_syncInterestPrefix;
  ndn::Name m_iblt;
  ndn::Name m_syncDataName;

  UpdateCallback m_onUpdate;

  ndn::time::milliseconds m_syncInterestLifetime;

  // Store sequence number for the prefix.
  std::map<ndn::Name, uint64_t> m_prefixes;

  std::uniform_int_distribution<> m_jitter;
  ndn::scheduler::ScopedEventId m_scheduledSyncInterestId;
  ndn::random::RandomNumberEngine& m_rng;
  std::uniform_int_distribution<> m_rangeUniformRandom;
  std::shared_ptr<ndn::util::SegmentFetcher> m_syncFetcher;
};

} // namespace psync

#endif // PSYNC_FULL_CONSUMER_HPP
