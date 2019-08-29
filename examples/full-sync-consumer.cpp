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

#include <PSync/full-consumer.hpp>

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/logger.hpp>
#include <ndn-cxx/util/random.hpp>
#include <ndn-cxx/util/scheduler.hpp>

#include <iostream>

NDN_LOG_INIT(examples.FullSyncConsumberApp);

// Example to test full-sync-consumer
class FullSyncConsumer
{
public:

  FullSyncConsumer(const ndn::Name& syncPrefix)
    : m_scheduler(m_face.getIoService())
    , m_consumer(syncPrefix, m_face,
                 std::bind(&FullSyncConsumer::onUpdate, this, _1), ndn::time::milliseconds(10000))
  {
  }

  void
  run()
  {
    m_face.processEvents();
  }

private:
  void onUpdate(std::vector<psync::MissingDataInfo> updates){

    for (auto const& value: updates){
      NDN_LOG_INFO("Received: " << value.prefix << "/" <<value.lowSeq << "/" << value.highSeq);
    }
  }

  ndn::Face m_face;
  ndn::Scheduler m_scheduler;
  psync::FullConsumer m_consumer;

};

int
main(int argc, char* argv[])
{
  if (argc != 2) {
    std::cout << "usage: " << argv[0] << " <sync-prefix> "
              << "<max-number-of-updates-per-user-prefix>"
              << std::endl;
    return 1;
  }

  try {
    FullSyncConsumer consumer(argv[1]);
    consumer.run();
  }
  catch (const std::exception& e) {
    NDN_LOG_ERROR(e.what());
  }
}
