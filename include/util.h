/**
 \file    sel/util.h
 \author  Sebastian Stammler <sebastian.stammler@cysec.de>
 \copyright SEL - Secure EpiLinker
      Copyright (C) 2018 Computational Biology & Simulation Group TU-Darmstadt
      This program is free software: you can redistribute it and/or modify
      it under the terms of the GNU Affero General Public License as published
      by the Free Software Foundation, either version 3 of the License, or
      (at your option) any later version.
      This program is distributed in the hope that it will be useful,
      but WITHOUT ANY WARRANTY; without even the implied warranty of
      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
      GNU Affero General Public License for more details.
      You should have received a copy of the GNU Affero General Public License
      along with this program. If not, see <http://www.gnu.org/licenses/>.
 \brief general utilities
*/

#ifndef SEL_UTIL_H
#define SEL_UTIL_H
#pragma once

#include <vector>
#include <map>
#include <algorithm>
#include <iterator>
#include <functional>
#include <memory>
#include <fmt/format.h>
#include <stdexcept>
#include <cctype>
#include <locale>
#include <sstream>

namespace sel {

constexpr size_t bitbytes(size_t b) { return (b + 7)/8; }

/**
 * Ceiling of integer log2
 */
int ceil_log2(unsigned long long x);

/**
 * Concatenates the vectors to a single vector, i.e., it flattenes the given
 * vector of vectors.
 */
template <typename T>
std::vector<T> concat_vec(const std::vector<std::vector<T>>& vs) {
  size_t total_size{0};
  for (auto& v : vs) total_size += v.size();

  std::vector<T> c;
  c.reserve(total_size);
  for (auto& v : vs) {
    c.insert(c.end(), v.begin(), v.end());
  }
  return c;
}

/**
 * Repeats the vectors n times.
 */
template <typename T>
std::vector<T> repeat_vec(const std::vector<T>& v, const size_t n) {
  std::vector<T> c;
  c.reserve(v.size() * n);
  for (size_t i = 0; i != n; ++i) {
    c.insert(c.end(), v.begin(), v.end());
  }
  return c;
}

/**
 * Generates a bitmaks of given size (rounded up to next multiple of 8) with
 * given bit set at all positions.
 */
std::vector<uint8_t> repeat_bit(const bool bit, const size_t n);

std::vector<uint8_t> vector_bool_to_bitmask(const std::vector<bool>&);

/*
std::vector<uint8_t> concat_vec(const std::vector<std::vector<uint8_t>>& vs) {
  size_t total_size{0};
  for (auto& v : vs) total_size += v.size();

  std::vector<uint8_t> c;
  c.reserve(total_size);
  for (auto& v : vs) {
    c.insert(c.end(), v.begin(), v.end());
  }
  return c;
}
*/
template <typename InType,
  template <typename U, typename alloc = std::allocator<U>>
            class InContainer,
  template <typename V, typename alloc = std::allocator<V>>
            class OutContainer = InContainer,
  typename OutType = InType>
OutContainer<OutType> transform_vec(const InContainer<InType>& vec,
   std::function<OutType(const InType&)> op) {
  OutContainer<OutType> out;
  out.reserve(vec.size());
  std::transform(vec.cbegin(), vec.cend(), std::back_inserter(out), op);
  return out;
}

template <typename T>
void check_vector_size(const std::vector<T>& r,
    const size_t& size, const std::string& name) {
  if (r.size() != size)
    throw std::invalid_argument{fmt::format(
        "check_vector_size: size mismatch: all {} vectors need to be of same size {}. Found size {}",
        name, size, r.size())};
}

template <typename T>
void check_vectors_size(const std::vector<std::vector<T>>& vec,
    const size_t& size, const std::string& name) {
  for (auto& r : vec) {
    check_vector_size(r, size, name);
  }
}

/**
 * Transforms the values of the given map with the given transformation function
 * and returns the transformed map.
 */
template <class Key, class FromValue, class Transformer,
  class ToValue = decltype(std::declval<Transformer>()(std::declval<FromValue>()))>
std::map<Key, ToValue> transform_map(const std::map<Key, FromValue>& _map,
    Transformer _tr) {
  std::map<Key, ToValue> res;
  std::for_each(_map.cbegin(), _map.cend(),
      [&res, &_tr](const std::pair<const Key, FromValue>& kv) {
        res[kv.first] = _tr(kv.second);
      });
  return res;
}

/**
 * Print vectors to cout
 */
template<typename T>
std::ostream& operator<< (std::ostream& out, const std::vector<T>& v) {
    std::ios_base::fmtflags outf( out.flags() ); // save cout flags
    out << "[" << std::hex;
    size_t last = v.size() - 1;
    for(size_t i = 0; i < v.size(); ++i) {
        out << v[i];
        if (i != last)
            out << ", ";
    }
    out << "]";
    out.flags(outf); // restore old flags
    return out;
}

// specialization for uint8_t / unsigned char
std::ostream& operator<< (std::ostream& out, const std::vector<uint8_t>& v);

// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

// trim from start (copying)
static inline std::string ltrim_copy(std::string s) {
    ltrim(s);
    return s;
}

// trim from end (copying)
static inline std::string rtrim_copy(std::string s) {
    rtrim(s);
    return s;
}

// trim from both ends (copying)
static inline std::string trim_copy(std::string s) {
    trim(s);
    return s;
}
template<typename Out>
void split(const std::string &s, char delim, Out result) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        *(result++) = item;
    }
}
std::vector<std::string> split(const std::string &s, char delim);

} // namespace sel

#endif /* end of include guard: SEL_UTIL_H */
