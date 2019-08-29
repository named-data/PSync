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

#include "PSync/full-consumer.hpp"
#include "PSync/detail/state.hpp"

#include <ndn-cxx/util/logger.hpp>
#include <ndn-cxx/security/validator-null.hpp>

#include <boost/algorithm/string.hpp>

#include <iostream>

namespace psync {

NDN_LOG_INIT(psync.FullConsumer);

FullConsumer::FullConsumer(const ndn::Name& syncPrefix,
                   ndn::Face& face,
                   const UpdateCallback& onUpdate,
                   ndn::time::milliseconds syncInterestLifetime)
 : m_face(face)
 , m_scheduler(m_face.getIoService())
 , m_syncPrefix(syncPrefix)
 , m_iblt("x%9Cc%60%18%05%A3%60%F8%00%00%02%D0%00%01") //empty IBF
 , m_onUpdate(onUpdate)
 , m_syncInterestLifetime(syncInterestLifetime)
 , m_rng(ndn::random::getRandomNumberEngine())
 , m_rangeUniformRandom(100, 500)
{
  int jitter = m_syncInterestLifetime.count() * .20;
  m_jitter = std::uniform_int_distribution<>(-jitter, jitter);

  sendSyncInterest();
}

void
FullConsumer::stop()
{
  m_scheduler.cancelAllEvents();

  if (m_syncFetcher) {
    m_syncFetcher->stop();
    m_syncFetcher.reset();
  }
}

void
FullConsumer::sendSyncInterest()
{
  BOOST_ASSERT(!m_iblt.empty());

  ndn::Name syncInterestName(m_syncPrefix);

  // Append IBF received in sync data
  syncInterestName.append(m_iblt);

  // schedule interest
  m_scheduledSyncInterestId =
    m_scheduler.schedule(m_syncInterestLifetime / 2 + ndn::time::milliseconds(m_jitter(m_rng)),
                         [this] { sendSyncInterest(); });

  ndn::Interest syncInterest(syncInterestName);

  NDN_LOG_DEBUG("sendSyncInterest, nonce: " << syncInterest.getNonce() <<
                " hash: " << std::hash<std::string>{}(syncInterest.getName().toUri()));

  if (m_syncFetcher) {
    m_syncFetcher->stop();
  }

  using ndn::util::SegmentFetcher;
  SegmentFetcher::Options options;
  options.interestLifetime = m_syncInterestLifetime;
  options.maxTimeout = m_syncInterestLifetime;

  m_syncFetcher = SegmentFetcher::start(m_face, syncInterest,
                                        ndn::security::v2::getAcceptAllValidator(), options);

  m_syncFetcher->afterSegmentValidated.connect([this] (const ndn::Data& data) {
    if (data.getFinalBlock()) {
      m_syncDataName = data.getName().getPrefix(-2);
    }
  });

  m_syncFetcher->onComplete.connect([this] (const ndn::ConstBufferPtr& bufferPtr) {
    NDN_LOG_TRACE("Segment fetcher got sync data");
    onSyncData(bufferPtr);
  });

  m_syncFetcher->onError.connect([this] (uint32_t errorCode, const std::string& msg) {
    NDN_LOG_TRACE("Cannot fetch sync data, error: " << errorCode << " message: " << msg);
    ndn::time::milliseconds after(m_rangeUniformRandom(m_rng));
    NDN_LOG_TRACE("Scheduling after " << after);
    m_scheduler.schedule(after, [this] { sendSyncInterest(); });
  });
}

void
FullConsumer::onSyncData(const ndn::ConstBufferPtr& bufferPtr)
{
  // Extract IBF from sync data name which is the last component and use it to replace
  // consumers old IBF

  m_iblt = m_syncDataName.getSubName(m_syncDataName.size() - 1, 1);

  State state{ndn::Block{bufferPtr}};

  std::vector<MissingDataInfo> updates;

  for (const auto& content : state.getContent()) {
    NDN_LOG_DEBUG(content);
    const ndn::Name& prefix = content.getPrefix(-1);
    uint64_t seq = content.get(content.size() - 1).toNumber();
    if (m_prefixes.find(prefix) == m_prefixes.end() || seq > m_prefixes[prefix]) {
      // If this is just the next seq number then we had already informed the consumer about
      // the previous sequence number and hence seq low and seq high should be equal to current seq
      updates.push_back(MissingDataInfo{prefix, m_prefixes[prefix] + 1, seq});
      m_prefixes[prefix] = seq;
    }
  }

  NDN_LOG_DEBUG("Sync Data: " << state);

  if (!updates.empty()) {
    m_onUpdate(updates);
  }
}

} // namespace psync
