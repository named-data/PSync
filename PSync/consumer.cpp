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

#include "PSync/consumer.hpp"
#include "PSync/detail/state.hpp"

#include <ndn-cxx/util/logger.hpp>
#include <ndn-cxx/security/validator-null.hpp>

#include <boost/algorithm/string.hpp>

namespace psync {

NDN_LOG_INIT(psync.Consumer);

Consumer::Consumer(const ndn::Name& syncPrefix,
                   ndn::Face& face,
                   const ReceiveHelloCallback& onReceiveHelloData,
                   const UpdateCallback& onUpdate,
                   unsigned int count,
                   double false_positive = 0.001,
                   ndn::time::milliseconds helloInterestLifetime,
                   ndn::time::milliseconds syncInterestLifetime)
 : m_face(face)
 , m_scheduler(m_face.getIoService())
 , m_syncPrefix(syncPrefix)
 , m_helloInterestPrefix(ndn::Name(m_syncPrefix).append("hello"))
 , m_syncInterestPrefix(ndn::Name(m_syncPrefix).append("sync"))
 , m_syncDataContentType(ndn::tlv::ContentType_Blob)
 , m_onReceiveHelloData(onReceiveHelloData)
 , m_onUpdate(onUpdate)
 , m_bloomFilter(count, false_positive)
 , m_helloInterestLifetime(helloInterestLifetime)
 , m_syncInterestLifetime(syncInterestLifetime)
 , m_rng(ndn::random::getRandomNumberEngine())
 , m_rangeUniformRandom(100, 500)
{
}

bool
Consumer::addSubscription(const ndn::Name& prefix)
{
  auto it = m_prefixes.insert(std::pair<ndn::Name, uint64_t>(prefix, 0));
  if (!it.second) {
    return false;
  }
  m_subscriptionList.insert(prefix);
  m_bloomFilter.insert(prefix.toUri());
  return true;
}

void
Consumer::stop()
{
  if (m_syncFetcher) {
    m_syncFetcher->stop();
    m_syncFetcher.reset();
  }

  if (m_helloFetcher) {
    m_helloFetcher->stop();
    m_helloFetcher.reset();
  }
}

void
Consumer::sendHelloInterest()
{
  ndn::Interest helloInterest(m_helloInterestPrefix);
  NDN_LOG_DEBUG("Send Hello Interest " << helloInterest);

  if (m_helloFetcher) {
    m_helloFetcher->stop();
  }

  using ndn::util::SegmentFetcher;
  SegmentFetcher::Options options;
  options.interestLifetime = m_helloInterestLifetime;
  options.maxTimeout = m_helloInterestLifetime;

  m_helloFetcher = SegmentFetcher::start(m_face, helloInterest,
                                         ndn::security::v2::getAcceptAllValidator(), options);

  m_helloFetcher->afterSegmentValidated.connect([this] (const ndn::Data& data) {
    if (data.getFinalBlock()) {
      m_helloDataName = data.getName().getPrefix(-2);
    }
  });

  m_helloFetcher->onComplete.connect([this] (const ndn::ConstBufferPtr& bufferPtr) {
    onHelloData(bufferPtr);
  });

  m_helloFetcher->onError.connect([this] (uint32_t errorCode, const std::string& msg) {
    NDN_LOG_TRACE("Cannot fetch hello data, error: " << errorCode << " message: " << msg);
    ndn::time::milliseconds after(m_rangeUniformRandom(m_rng));
    NDN_LOG_TRACE("Scheduling after " << after);
    m_scheduler.schedule(after, [this] { sendHelloInterest(); });
  });
}

void
Consumer::onHelloData(const ndn::ConstBufferPtr& bufferPtr)
{
  NDN_LOG_DEBUG("On Hello Data");

  // Extract IBF from name which is the last element in hello data's name
  m_iblt = m_helloDataName.getSubName(m_helloDataName.size()-1, 1);

  NDN_LOG_TRACE("m_iblt: " << std::hash<std::string>{}(m_iblt.toUri()));

  State state{ndn::Block{bufferPtr}};

  std::vector<MissingDataInfo> updates;
  std::vector<ndn::Name> availableSubscriptions;

  NDN_LOG_DEBUG("Hello Data:  " << state);

  for (const auto& content : state.getContent()) {
    const ndn::Name& prefix = content.getPrefix(-1);
    uint64_t seq = content.get(-1).toNumber();
    // If consumer is subscribed then prefix must already be present in
    // m_prefixes (see addSubscription). So [] operator is safe to use.
    if (isSubscribed(prefix) && seq > m_prefixes[prefix]) {
      // In case we are behind on this prefix and consumer is subscribed to it
      updates.push_back(MissingDataInfo{prefix, m_prefixes[prefix] + 1, seq});
      m_prefixes[prefix] = seq;
    }
    availableSubscriptions.push_back(prefix);
  }

  m_onReceiveHelloData(availableSubscriptions);

  if (!updates.empty()) {
    NDN_LOG_DEBUG("Updating application with missed updates");
    m_onUpdate(updates);
  }
}

void
Consumer::sendSyncInterest()
{
  BOOST_ASSERT(!m_iblt.empty());

  ndn::Name syncInterestName(m_syncInterestPrefix);

  // Append subscription list
  m_bloomFilter.appendToName(syncInterestName);

  // Append IBF received in hello/sync data
  syncInterestName.append(m_iblt);

  ndn::Interest syncInterest(syncInterestName);

  NDN_LOG_DEBUG("sendSyncInterest, nonce: " << syncInterest.getNonce() <<
                " hash: " << std::hash<std::string>{}(syncInterest.getName().toUri()));

  if (m_syncFetcher) {
    m_syncFetcher->stop();
  }

  using ndn::util::SegmentFetcher;
  SegmentFetcher::Options options;
  options.interestLifetime = m_syncInterestLifetime;
  options.maxTimeout = m_syncInterestLifetime;;

  m_syncFetcher = SegmentFetcher::start(m_face, syncInterest,
                                        ndn::security::v2::getAcceptAllValidator(), options);

  m_syncFetcher->afterSegmentValidated.connect([this] (const ndn::Data& data) {
    if (data.getFinalBlock()) {
      m_syncDataName = data.getName().getPrefix(-2);
      m_syncDataContentType = data.getContentType();
    }

    if (m_syncDataContentType == ndn::tlv::ContentType_Nack) {
      NDN_LOG_DEBUG("Received application Nack from producer, sending hello again");
      sendHelloInterest();
    }
  });

  m_syncFetcher->onComplete.connect([this] (const ndn::ConstBufferPtr& bufferPtr) {
    if (m_syncDataContentType == ndn::tlv::ContentType_Nack) {
      m_syncDataContentType = ndn::tlv::ContentType_Blob;
      return;
    }
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
Consumer::onSyncData(const ndn::ConstBufferPtr& bufferPtr)
{
  // Extract IBF from sync data name which is the last component
  m_iblt = m_syncDataName.getSubName(m_syncDataName.size() - 1, 1);

  State state{ndn::Block{bufferPtr}};

  std::vector<MissingDataInfo> updates;

  for (const auto& content : state.getContent()) {
    NDN_LOG_DEBUG(content);
    const ndn::Name& prefix = content.getPrefix(-1);
    uint64_t seq = content.get(-1).toNumber();
    if (m_prefixes.find(prefix) == m_prefixes.end() || seq > m_prefixes[prefix]) {
      // If this is just the next seq number then we had already informed the consumer about
      // the previous sequence number and hence seq low and seq high should be equal to current seq
      updates.push_back(MissingDataInfo{prefix, m_prefixes[prefix] + 1, seq});
      m_prefixes[prefix] = seq;
    }
    // Else updates will be empty and consumer will not be notified.
  }

  NDN_LOG_DEBUG("Sync Data: " << state);

  if (!updates.empty()) {
    m_onUpdate(updates);
  }

  sendSyncInterest();
}

} // namespace psync
