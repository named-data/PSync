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

#include "tests/clock-fixture.hpp"

namespace psync::tests {

ClockFixture::ClockFixture()
  : m_steadyClock(std::make_shared<time::UnitTestSteadyClock>())
  , m_systemClock(std::make_shared<time::UnitTestSystemClock>())
{
  time::setCustomClocks(m_steadyClock, m_systemClock);
}

ClockFixture::~ClockFixture()
{
  time::setCustomClocks(nullptr, nullptr);
}

void
ClockFixture::advanceClocks(time::nanoseconds tick, time::nanoseconds total)
{
  BOOST_ASSERT(tick > time::nanoseconds::zero());
  BOOST_ASSERT(total >= time::nanoseconds::zero());

  while (total > time::nanoseconds::zero()) {
    auto t = std::min(tick, total);
    m_steadyClock->advance(t);
    m_systemClock->advance(t);
    total -= t;

    afterTick();
  }
}

} // namespace psync::tests
