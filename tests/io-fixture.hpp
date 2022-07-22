/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2013-2022  Regents of the University of California
 *                          The University of Memphis
 *
 * This file is part of PSync.
 *
 * PSync is free software: you can redistribute it and/or modify it under the terms
 * of the GNU Lesser General Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * PSync is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along with
 * PSync, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PSYNC_TESTS_IO_FIXTURE_HPP
#define PSYNC_TESTS_IO_FIXTURE_HPP

#include "tests/clock-fixture.hpp"

#include <boost/asio/io_service.hpp>

namespace ndn::tests {

class IoFixture : public ClockFixture
{
private:
  void
  afterTick() final
  {
    if (m_io.stopped()) {
#if BOOST_VERSION >= 106600
      m_io.restart();
#else
      m_io.reset();
#endif
    }
    m_io.poll();
  }

protected:
  boost::asio::io_service m_io;
};

} // namespace ndn::tests

#endif // PSYNC_TESTS_IO_FIXTURE_HPP
