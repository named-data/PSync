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

#ifndef PSYNC_CONSUMER_HPP
#define PSYNC_CONSUMER_HPP

#include "detail/bloom-filter.hpp"
#include "detail/util.hpp"
#include "detail/test-access-control.hpp"

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <ndn-cxx/util/time.hpp>

#include <random>
#include <map>
#include <vector>

namespace psync {

using namespace ndn::literals::time_literals;

typedef std::function<void(const std::vector<ndn::Name>&)> ReceiveHelloCallback;
typedef std::function<void(const std::vector<MissingDataInfo>&)> UpdateCallback;

const ndn::time::milliseconds HELLO_INTEREST_LIFETIME = 1_s;
const ndn::time::milliseconds SYNC_INTEREST_LIFETIME = 1_s;

/**
 * @brief Consumer logic to subscribe to producer's data
 *
 * Application needs to call sendHelloInterest to get the subscription list
 * in ReceiveHelloCallback. It can then add the desired names using addSubscription.
 * Finally application will call sendSyncInterest. If the application adds something
 * later to the subscription list then it may call sendSyncInterest again for
 * sending the next sync interest with updated IBF immediately to reduce any delay in sync data.
 * Whenever there is new data UpdateCallback will be called to notify the application.
 * Currently, fetching of the data needs to be handled by the application.
 */
class Consumer
{
public:
  /**
   * @brief constructor
   *
   * @param syncPrefix syncPrefix to send hello/sync interests to producer
   * @param face application's face
   * @param onReceiveHelloData call back to give hello data back to application
   * @param onUpdate call back to give sync data back to application
   * @param count bloom filter number of expected elements (subscriptions) in bloom filter
   * @param false_positive bloom filter false positive probability
   * @param helloInterestLifetime lifetime of hello interest
   * @param syncInterestLifetime lifetime of sync interest
   */
  Consumer(const ndn::Name& syncPrefix,
           ndn::Face& face,
           const ReceiveHelloCallback& onReceiveHelloData,
           const UpdateCallback& onUpdate,
           unsigned int count,
           double false_positive,
           ndn::time::milliseconds helloInterestLifetime = HELLO_INTEREST_LIFETIME,
           ndn::time::milliseconds syncInterestLifetime = SYNC_INTEREST_LIFETIME);

  /**
   * @brief send hello interest /<sync-prefix>/hello/
   *
   * Should be called by the application whenever it wants to send a hello
   */
  void
  sendHelloInterest();

  /**
   * @brief send sync interest /<sync-prefix>/sync/\<BF\>/\<producers-IBF\>
   *
   * Should be called after subscription list is set or updated
   */
  void
  sendSyncInterest();

  /**
   * @brief Add prefix to subscription list
   *
   * @param prefix prefix to be added to the list
   */
  bool
  addSubscription(const ndn::Name& prefix);

  std::set<ndn::Name>
  getSubscriptionList() const
  {
    return m_subscriptionList;
  }

  bool
  isSubscribed(const ndn::Name& prefix) const
  {
    return m_subscriptionList.find(prefix) != m_subscriptionList.end();
  }

  ndn::optional<uint64_t>
  getSeqNo(const ndn::Name& prefix) const
  {
    auto it = m_prefixes.find(prefix);
    if (it == m_prefixes.end()) {
      return ndn::nullopt;
    }
    return it->second;
  }

private:
  /**
   * @brief Get hello data from the producer
   *
   * Format: /<sync-prefix>/hello/\<BF\>/\<producer-IBF\>
   * Data content is all the prefixes the producer has.
   * We store the producer's IBF to be used in sending sync interest
   *
   * m_onReceiveHelloData is called to let the application know
   * so that it can set the subscription list using addSubscription
   *
   * @param interest hello interest
   * @param data hello data
   */
  void
  onHelloData(const ndn::Interest& interest, const ndn::Data& data);

  /**
   * @brief Get hello data from the producer
   *
   * Format: <sync-prefix>/sync/\<BF\>/\<producers-IBF\>/\<producers-latest-IBF\>
   * Data content is all the prefixes the producer thinks the consumer doesn't have
   * have the latest update for. We update our copy of producer's IBF with the latest one.
   * Then we send another sync interest after a random jitter.
   *
   * @param interest sync interest
   * @param data sync data
   */
  void
  onSyncData(const ndn::Interest& interest, const ndn::Data& data);

  void
  onHelloTimeout(const ndn::Interest& interest);

  void
  onSyncTimeout(const ndn::Interest& interest);

  void
  onNackForHello(const ndn::Interest& interest, const ndn::lp::Nack& nack);

  void
  onNackForSync(const ndn::Interest& interest, const ndn::lp::Nack& nack);

PUBLIC_WITH_TESTS_ELSE_PRIVATE:
  ndn::Face& m_face;
  ndn::Scheduler m_scheduler;

  ndn::Name m_syncPrefix;
  ndn::Name m_helloInterestPrefix;
  ndn::Name m_syncInterestPrefix;
  ndn::Name m_iblt;

  ReceiveHelloCallback m_onReceiveHelloData;

  // Called when new sync update is received from producer.
  UpdateCallback m_onUpdate;

  // Bloom filter is used to store application/user's subscription list.
  BloomFilter m_bloomFilter;

  ndn::time::milliseconds m_helloInterestLifetime;
  ndn::time::milliseconds m_syncInterestLifetime;

  // Store sequence number for the prefix.
  std::map<ndn::Name, uint64_t> m_prefixes;
  std::set<ndn::Name> m_subscriptionList;

  const ndn::PendingInterestId* m_outstandingInterestId;

  std::mt19937 m_rng;
  std::uniform_int_distribution<> m_rangeUniformRandom;
};

} // namespace psync

#endif // PSYNC_CONSUMER_HPP
