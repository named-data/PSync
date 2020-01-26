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

#ifndef PSYNC_BLOOM_FILTER_HPP
#define PSYNC_BLOOM_FILTER_HPP

#include <ndn-cxx/name.hpp>

#include <string>
#include <vector>
#include <cmath>
#include <cstdlib>

namespace psync {

struct optimal_parameters_t
{
  optimal_parameters_t()
  : number_of_hashes(0),
    table_size(0)
  {}

  unsigned int number_of_hashes;
  unsigned int table_size;
};

class BloomParameters
{
public:
  BloomParameters();

  bool
  compute_optimal_parameters();

  bool operator!() const
  {
    return (minimum_size > maximum_size)      ||
           (minimum_number_of_hashes > maximum_number_of_hashes) ||
           (minimum_number_of_hashes < 1)     ||
           (0 == maximum_number_of_hashes)    ||
           (0 == projected_element_count)     ||
           (false_positive_probability < 0.0) ||
           (std::numeric_limits<double>::infinity() == std::abs(false_positive_probability)) ||
           (0 == random_seed)                 ||
           (0xFFFFFFFFFFFFFFFFULL == random_seed);
  }

  unsigned int           minimum_size;
  unsigned int           maximum_size;
  unsigned int           minimum_number_of_hashes;
  unsigned int           maximum_number_of_hashes;
  unsigned int           projected_element_count;
  double                 false_positive_probability;
  unsigned long long int random_seed;
  optimal_parameters_t   optimal_parameters;
};

class BloomFilter
{
protected:
  typedef uint32_t bloom_type;
  typedef uint8_t cell_type;
  typedef std::vector <cell_type>::iterator Iterator;

public:
  class Error : public std::runtime_error
  {
  public:
    using std::runtime_error::runtime_error;
  };

  BloomFilter();

  explicit BloomFilter(const BloomParameters& p);

  BloomFilter(unsigned int projected_element_count,
              double false_positive_probability);

  BloomFilter(unsigned int projected_element_count,
              double false_positive_probability,
              const ndn::name::Component& bfName);

  BloomParameters
  getParameters(unsigned int projected_element_count,
                double false_positive_probability);

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
  insert(const std::string& key);

  bool
  contains(const std::string& key) const;

  std::vector<cell_type>
  table() const;

private:
  void
  generate_unique_salt();

  void
  compute_indices(const bloom_type& hash,
                  std::size_t& bit_index, std::size_t& bit) const;

private:
  std::vector <bloom_type> salt_;
  std::vector <cell_type>  bit_table_;
  unsigned int             salt_count_;
  unsigned int             table_size_; // 8 * raw_table_size;
  unsigned int             raw_table_size_;
  unsigned int             projected_element_count_;
  unsigned int             inserted_element_count_;
  unsigned long long int   random_seed_;
  double                   desired_false_positive_probability_;
};

bool
operator==(const BloomFilter& bf1, const BloomFilter& bf2);

std::ostream&
operator<<(std::ostream& out, const BloomFilter& bf);

} // namespace psync

#endif // PSYNC_BLOOM_FILTER_HPP
