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
 **/

#include <PSync/partial-producer.hpp>

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/util/logger.hpp>
#include <ndn-cxx/util/random.hpp>
#include <ndn-cxx/util/scheduler.hpp>

#include <iostream>

NDN_LOG_INIT(examples.PartialSyncProducerApp);

class PSyncPartialProducer
{
public:
  /**
   * @brief Initialize producer and schedule updates
   *
   * IBF size is set to 40 in m_producer as the expected number of update to IBF in a sync cycle
   */
  PSyncPartialProducer(const ndn::Name& syncPrefix, const std::string& userPrefix,
                       int numDataStreams, int maxNumPublish)
    : m_producer(m_face, m_keyChain, 40, syncPrefix, userPrefix + "-0")
    , m_maxNumPublish(maxNumPublish)
  {
    // Add user prefixes and schedule updates for them
    for (int i = 0; i < numDataStreams; i++) {
      ndn::Name updateName(userPrefix + "-" + std::to_string(i));

      // Add the user prefix to the producer
      // Note that this does not add the already added userPrefix-0 in the constructor
      m_producer.addUserNode(updateName);

      // Each user prefix is updated at a random interval between 0 and 60 seconds
      m_scheduler.schedule(ndn::time::milliseconds(m_uniformRand(m_rng)),
                           [this, updateName] { doUpdate(updateName); });
    }
  }

  void
  run()
  {
    m_face.processEvents();
  }

private:
  void
  doUpdate(const ndn::Name& updateName)
  {
    // Publish an update to this user prefix
    m_producer.publishName(updateName);

    uint64_t seqNo =  m_producer.getSeqNo(updateName).value();
    NDN_LOG_INFO("Publish: " << updateName << "/" << seqNo);

    if (seqNo < m_maxNumPublish) {
      // Schedule the next update for this user prefix between 0 and 60 seconds
      m_scheduler.schedule(ndn::time::milliseconds(m_uniformRand(m_rng)),
                           [this, updateName] { doUpdate(updateName); });
    }
  }

private:
  ndn::Face m_face;
  ndn::KeyChain m_keyChain;
  ndn::Scheduler m_scheduler{m_face.getIoService()};

  psync::PartialProducer m_producer;
  uint64_t m_maxNumPublish;

  ndn::random::RandomNumberEngine& m_rng{ndn::random::getRandomNumberEngine()};
  std::uniform_int_distribution<> m_uniformRand{0, 60000};
};

int
main(int argc, char* argv[])
{
  if (argc != 5) {
    std::cerr << "Usage: " << argv[0] << " <sync-prefix> <user-prefix> "
              << "<number-of-user-prefixes> <max-number-of-updates-per-user-prefix>\n";
    return 1;
  }

  try {
    PSyncPartialProducer producer(argv[1], argv[2], std::stoi(argv[3]), std::stoi(argv[4]));
    producer.run();
  }
  catch (const std::exception& e) {
    NDN_LOG_ERROR(e.what());
  }
}
