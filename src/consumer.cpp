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

#include "consumer.hpp"
#include "detail/state.hpp"

#include <ndn-cxx/util/logger.hpp>

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
 , m_onReceiveHelloData(onReceiveHelloData)
 , m_onUpdate(onUpdate)
 , m_bloomFilter(count, false_positive)
 , m_helloInterestLifetime(helloInterestLifetime)
 , m_syncInterestLifetime(syncInterestLifetime)
 , m_rng(std::random_device{}())
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
Consumer::sendHelloInterest()
{
  ndn::Interest helloInterest(m_helloInterestPrefix);
  helloInterest.setInterestLifetime(m_helloInterestLifetime);
  helloInterest.setCanBePrefix(true);
  helloInterest.setMustBeFresh(true);

  NDN_LOG_DEBUG("Send Hello Interest " << helloInterest);

  m_face.expressInterest(helloInterest,
                         std::bind(&Consumer::onHelloData, this, _1, _2),
                         std::bind(&Consumer::onNackForHello, this, _1, _2),
                         std::bind(&Consumer::onHelloTimeout, this, _1));
}

void
Consumer::onHelloData(const ndn::Interest& interest, const ndn::Data& data)
{
  ndn::Name helloDataName = data.getName();

  NDN_LOG_DEBUG("On Hello Data");

  // Extract IBF from name which is the last element in hello data's name
  m_iblt = helloDataName.getSubName(helloDataName.size()-1, 1);

  NDN_LOG_TRACE("m_iblt: " << std::hash<std::string>{}(m_iblt.toUri()));

  State state(data.getContent());
  std::vector<MissingDataInfo> updates;
  std::vector<ndn::Name> availableSubscriptions;

  for (const auto& content : state.getContent()) {
    ndn::Name prefix = content.getPrefix(-1);
    uint64_t seq = content.get(content.size()-1).toNumber();
    if (m_prefixes.find(prefix) == m_prefixes.end()) {
      // In case this the first prefix ever received via hello data,
      // add it to the available subscriptions
      availableSubscriptions.push_back(prefix);
    }
    else if (seq > m_prefixes[prefix]) {
      // Else this is not the first time we have seen this prefix,
      // we must let application know that there is missing data
      // (scenario: application nack triggers another hello interest)
      updates.push_back(MissingDataInfo{prefix, m_prefixes[prefix] + 1, seq});
      m_prefixes[prefix] = seq;
    }
  }

  NDN_LOG_DEBUG("Hello Data:  " << state);

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
  syncInterest.setInterestLifetime(m_syncInterestLifetime);
  syncInterest.setCanBePrefix(true);
  syncInterest.setMustBeFresh(true);

  NDN_LOG_DEBUG("sendSyncInterest, nonce: " << syncInterest.getNonce() <<
                " hash: " << std::hash<std::string>{}(syncInterest.getName().toUri()));

  // Remove last pending interest before sending a new one
  if (m_outstandingInterestId != nullptr) {
      m_face.removePendingInterest(m_outstandingInterestId);
      m_outstandingInterestId = nullptr;
  }

  m_outstandingInterestId = m_face.expressInterest(syncInterest,
                              std::bind(&Consumer::onSyncData, this, _1, _2),
                              std::bind(&Consumer::onNackForSync, this, _1, _2),
                              std::bind(&Consumer::onSyncTimeout, this, _1));
}

void
Consumer::onSyncData(const ndn::Interest& interest, const ndn::Data& data)
{
  ndn::Name syncDataName = data.getName();

  // Extract IBF from sync data name which is the last component
  m_iblt = syncDataName.getSubName(syncDataName.size()-1, 1);

  if (data.getContentType() == ndn::tlv::ContentType_Nack) {
    NDN_LOG_DEBUG("Received application Nack from producer, send hello again");
    sendHelloInterest();
    return;
  }

  State state(data.getContent());
  std::vector <MissingDataInfo> updates;

  for (const auto& content : state.getContent()) {
    NDN_LOG_DEBUG(content);
    ndn::Name prefix = content.getPrefix(-1);
    uint64_t seq = content.get(content.size()-1).toNumber();
    if (m_prefixes.find(prefix) == m_prefixes.end() || seq > m_prefixes[prefix]) {
      // If this is just the next seq number then we had already informed the consumer about
      // the previous sequence number and hence seq low and seq high should be equal to current seq
      updates.push_back(MissingDataInfo{prefix, m_prefixes[prefix] + 1, seq});
      m_prefixes[prefix] = seq;
    }
    // Else updates will be empty and consumer will not be notified.
  }

  NDN_LOG_DEBUG("Sync Data:  " << state);

  if (!updates.empty()) {
    m_onUpdate(updates);
  }

  sendSyncInterest();
}

void
Consumer::onHelloTimeout(const ndn::Interest& interest)
{
  NDN_LOG_DEBUG("on hello timeout");
  this->sendHelloInterest();
}

void
Consumer::onSyncTimeout(const ndn::Interest& interest)
{
  NDN_LOG_DEBUG("on sync timeout " << interest.getNonce());

  ndn::time::milliseconds after(m_rangeUniformRandom(m_rng));
  m_scheduler.scheduleEvent(after, [this] { sendSyncInterest(); });
}

void
Consumer::onNackForHello(const ndn::Interest& interest, const ndn::lp::Nack& nack)
{
  NDN_LOG_DEBUG("received Nack with reason " << nack.getReason() <<
                " for interest " << interest << std::endl);

  ndn::time::milliseconds after(m_rangeUniformRandom(m_rng));
  m_scheduler.scheduleEvent(after, [this] { sendHelloInterest(); });
}

void
Consumer::onNackForSync(const ndn::Interest& interest, const ndn::lp::Nack& nack)
{
  NDN_LOG_DEBUG("received Nack with reason " << nack.getReason() <<
                " for interest " << interest << std::endl);

  ndn::time::milliseconds after(m_rangeUniformRandom(m_rng));
  m_scheduler.scheduleEvent(after, [this] { sendSyncInterest(); });
}

} // namespace psync
