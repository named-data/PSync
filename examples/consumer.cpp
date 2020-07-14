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

#include <PSync/consumer.hpp>

#include <ndn-cxx/name.hpp>
#include <ndn-cxx/util/logger.hpp>
#include <ndn-cxx/util/random.hpp>

#include <iostream>

NDN_LOG_INIT(examples.PartialSyncConsumerApp);

class PSyncConsumer
{
public:
  /**
   * @brief Initialize consumer and start hello process
   *
   * 0.001 is the false positive probability of the bloom filter
   *
   * @param syncPrefix should be the same as producer
   * @param nSub number of subscriptions is used for the bloom filter (subscription list) size
   */
  PSyncConsumer(const ndn::Name& syncPrefix, int nSub)
    : m_nSub(nSub)
    , m_consumer(syncPrefix, m_face,
                 std::bind(&PSyncConsumer::afterReceiveHelloData, this, _1),
                 std::bind(&PSyncConsumer::processSyncUpdate, this, _1),
                 m_nSub, 0.001)
    , m_rng(ndn::random::getRandomNumberEngine())
  {
    // This starts the consumer side by sending a hello interest to the producer
    // When the producer responds with hello data, afterReceiveHelloData is called
    m_consumer.sendHelloInterest();
  }

  void
  run()
  {
    m_face.processEvents();
  }

private:
  void
  afterReceiveHelloData(const std::map<ndn::Name, uint64_t>& availSubs)
  {
    std::vector<ndn::Name> sensors;
    sensors.reserve(availSubs.size());
    for (const auto& it : availSubs) {
      sensors.insert(sensors.end(), it.first);
    }

    std::shuffle(sensors.begin(), sensors.end(), m_rng);

    // Randomly subscribe to m_nSub prefixes
    for (int i = 0; i < m_nSub; i++) {
      ndn::Name prefix = sensors[i];
      NDN_LOG_INFO("Subscribing to: " << prefix);
      auto it = availSubs.find(prefix);
      m_consumer.addSubscription(prefix, it->second);
    }

    // After setting the subscription list, send the sync interest
    // The sync interest contains the subscription list
    // When new data is received for any subscribed prefix, processSyncUpdate is called
    m_consumer.sendSyncInterest();
  }

  void
  processSyncUpdate(const std::vector<psync::MissingDataInfo>& updates)
  {
    for (const auto& update : updates) {
      for (uint64_t i = update.lowSeq; i <= update.highSeq; i++) {
        // Data can now be fetched using the prefix and sequence
        NDN_LOG_INFO("Update: " << update.prefix << "/" << i);
      }
    }
  }

private:
  ndn::Face m_face;
  int m_nSub;

  psync::Consumer m_consumer;
  ndn::random::RandomNumberEngine& m_rng;
};

int
main(int argc, char* argv[])
{
  if (argc != 3) {
    std::cout << "usage: " << argv[0] << " "
              << "<sync-prefix> <number-of-subscriptions>" << std::endl;
    return 1;
  }

  try {
    PSyncConsumer consumer(argv[1], std::stoi(argv[2]));
    consumer.run();
  }
  catch (const std::exception& e) {
    NDN_LOG_ERROR(e.what());
  }
}
