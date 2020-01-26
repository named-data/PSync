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
 * PSync, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>
 *
 **/

#ifndef PSYNC_DETAIL_ACCESS_SPECIFIERS_HPP
#define PSYNC_DETAIL_ACCESS_SPECIFIERS_HPP

#include "PSync/detail/config.hpp"

#ifdef PSYNC_WITH_TESTS
#define PSYNC_VIRTUAL_WITH_TESTS virtual
#define PSYNC_PUBLIC_WITH_TESTS_ELSE_PROTECTED public
#define PSYNC_PUBLIC_WITH_TESTS_ELSE_PRIVATE public
#define PSYNC_PROTECTED_WITH_TESTS_ELSE_PRIVATE protected
#else
#define PSYNC_VIRTUAL_WITH_TESTS
#define PSYNC_PUBLIC_WITH_TESTS_ELSE_PROTECTED protected
#define PSYNC_PUBLIC_WITH_TESTS_ELSE_PRIVATE private
#define PSYNC_PROTECTED_WITH_TESTS_ELSE_PRIVATE private
#endif

#endif // PSYNC_DETAIL_ACCESS_SPECIFIERS_HPP
