/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2019,  The University of Memphis
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

#include <PSync/full-producer-arbitrary.hpp>

#include <ndn-cxx/util/logger.hpp>
#include <ndn-cxx/util/random.hpp>

#include <iostream>

NDN_LOG_INIT(examples.FullSyncApp);

using namespace ndn::time_literals;

class Producer
{
public:
  /**
   * @brief Initialize producer and schedule updates
   *
   * Set IBF size as 80 expecting 80 updates to IBF in a sync cycle
   * Set syncInterestLifetime and syncReplyFreshness to 1.6 seconds
   */
  Producer(const ndn::Name& syncPrefix, const std::string& userPrefix,
           int numDataPrefixes, int maxNumPublish)
    : m_scheduler(m_face.getIoService())
    , m_fullProducer(80, m_face, syncPrefix,
                     std::bind(&Producer::processSyncUpdate, this, _1),
                     1600_ms, 1600_ms)
    , m_maxNumPublish(maxNumPublish)
    , m_rng(ndn::random::getRandomNumberEngine())
    , m_rangeUniformRandom(0, 6000)
  {
    // Add name prefixes and schedule updates for them in specified the interval.
    for (int i = 0; i < numDataPrefixes; i++) {
      ndn::Name dataPrefix(userPrefix + "-" + ndn::to_string(i));
      m_nPublished[dataPrefix] = 0;

      m_scheduler.schedule(ndn::time::milliseconds(m_rangeUniformRandom(m_rng)),
                           [this, dataPrefix] {
                             doUpdate(dataPrefix);
                           });
    }
  }

  void
  run()
  {
    m_face.processEvents();
  }

private:
  void
  doUpdate(const ndn::Name& dataPrefix)
  {
    ndn::Name name = ndn::Name(dataPrefix).appendVersion();

    NDN_LOG_INFO("Publish: " << name);
    m_fullProducer.publishName(name);

    ++m_nPublished[dataPrefix];

    if (m_nPublished[dataPrefix] < m_maxNumPublish) {
      m_scheduler.schedule(ndn::time::milliseconds(m_rangeUniformRandom(m_rng)),
                           [this, dataPrefix] {
                             doUpdate(dataPrefix);
                           });
    }
  }

  void
  processSyncUpdate(const std::vector<ndn::Name>& updates)
  {
    for (const auto& name : updates) {
      NDN_LOG_INFO("Update " << name);
    }
  }

private:
  ndn::Face m_face;
  ndn::Scheduler m_scheduler;

  psync::FullProducerArbitrary m_fullProducer;

  uint64_t m_maxNumPublish;
  std::map<ndn::Name, uint64_t> m_nPublished;

  ndn::random::RandomNumberEngine& m_rng;
  std::uniform_int_distribution<> m_rangeUniformRandom;
};

int
main(int argc, char* argv[])
{
  if (argc != 5) {
    std::cout << "usage: " << argv[0] << " <syncPrefix> <user-prefix> "
              << "<number-of-data-prefixes> <max-number-of-updates-per-data-prefix>"
              << std::endl;
    return 1;
  }

  try {
    Producer producer(argv[1], argv[2], std::stoi(argv[3]), std::stoi(argv[4]));
    producer.run();
  }
  catch (const std::exception& e) {
    NDN_LOG_ERROR(e.what());
  }
}
