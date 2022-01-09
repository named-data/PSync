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
 *

 * This file incorporates work covered by the following copyright and
 * permission notice:

 * The MIT License (MIT)

 * Copyright (c) 2000 Arash Partow

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/

#ifndef PSYNC_DETAIL_BLOOM_FILTER_HPP
#define PSYNC_DETAIL_BLOOM_FILTER_HPP

#include <ndn-cxx/name.hpp>
#include <ndn-cxx/util/string-helper.hpp>

#include <string>
#include <vector>

namespace psync {
namespace detail {

class bloom_parameters;

class BloomFilter
{
public:
  class Error : public std::runtime_error
  {
  public:
    using std::runtime_error::runtime_error;
  };

  BloomFilter() = default;

  BloomFilter(unsigned int projected_element_count,
              double false_positive_probability);

  BloomFilter(unsigned int projected_element_count,
              double false_positive_probability,
              const ndn::name::Component& bfName);

  /**
   * @brief Append our bloom filter to the given name
   *
   * Append the count and false positive probability
   * along with the bloom filter so that producer (PartialProducer) can construct a copy.
   *
   * @param name append bloom filter to this name
   */
  void
  appendToName(ndn::Name& name) const;

  void
  clear();

  void
  insert(const ndn::Name& key);

  bool
  contains(const ndn::Name& key) const;

private:
  typedef uint32_t bloom_type;
  typedef uint8_t cell_type;

  explicit
  BloomFilter(const bloom_parameters& p);

  void
  compute_indices(const bloom_type& hash, std::size_t& bit_index, std::size_t& bit) const;

  void
  generate_unique_salt();

private: // non-member operators
  // NOTE: the following "hidden friend" operators are available via
  //       argument-dependent lookup only and must be defined inline.

  friend bool
  operator==(const BloomFilter& lhs, const BloomFilter& rhs)
  {
    return lhs.bit_table_ == rhs.bit_table_;
  }

  friend bool
  operator!=(const BloomFilter& lhs, const BloomFilter& rhs)
  {
    return lhs.bit_table_ != rhs.bit_table_;
  }

  friend std::ostream&
  operator<<(std::ostream& os, const BloomFilter& bf)
  {
    ndn::printHex(os, bf.bit_table_.data(), bf.bit_table_.size(), false);
    return os;
  }

private:
  std::vector<bloom_type> salt_;
  std::vector<cell_type>  bit_table_;
  unsigned int            salt_count_ = 0;
  unsigned int            table_size_ = 0;
  unsigned int            projected_element_count_ = 0;
  unsigned int            inserted_element_count_ = 0;
  unsigned long long int  random_seed_ = 0;
  double                  desired_false_positive_probability_ = 0.0;
};

} // namespace detail
} // namespace psync

#endif // PSYNC_DETAIL_BLOOM_FILTER_HPP
