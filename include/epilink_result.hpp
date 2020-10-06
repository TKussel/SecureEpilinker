/**
 \file    epilink_result.hpp
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
 \brief Epilink Result / for secure and clear circuit
*/

#ifndef SEL_EPILINK_RESULT_HPP
#define SEL_EPILINK_RESULT_HPP
#pragma once

#include "fmt/format.h"

namespace sel {

template<typename T>
struct Result {
  T index;
  bool match;
  bool tmatch;
  T sum_field_weights;
  T sum_weights;
};

template<typename T>
struct CountResult {
  T matches;
  T tmatches;
};

template<typename T>
struct KMaxResult {
  std::vector<T> index;
  std::vector<bool> match;
  std::vector<bool> tmatch;
  std::vector<T> sum_field_weights;
  std::vector<T> sum_weights;
};

template<typename T>
bool operator==(const Result<T>& l, const Result<T>& r) {
  return l.index == r.index && l.match == r.match && l.tmatch == r.tmatch
    && l.sum_field_weights == r.sum_field_weights && l.sum_weights == r.sum_weights;
}
template<typename T>
bool operator==(const KMaxResult<T>& l, const KMaxResult<T>& r) {
  assert(l.index.size() == r.index.size());
  assert(l.match.size() == r.match.size());
  assert(l.tmatch.size() == r.tmatch.size());
  assert(l.sum_field_weights.size() == r.sum_field_weights.size());
  assert(l.sum_weights.size() == r.sum_weights.size());
  assert(l.index.size == l.match.size() && l.match.size() == l.tmatch.size());
  bool same = true;
  for(size_t i = 0; i != l.index.size(); i++){
    same &= (l.index[i] == r.index[i]);
    same &= (l.match[i] == r.match[i]);
    same &= (l.tmatch[i] == r.tmatch[i]);
    same &= (l.sum_field_weights[i] == r.sum_field_weights[i]);
    same &= (l.sum_weights[i] == r.sum_weights[i]);
  }
  return same;
}

} /* END namespace sel */

// Custom fmt formatters
namespace fmt {
template <typename T>
struct formatter<sel::Result<T>> {
  template <typename ParseContext>
  constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const sel::Result<T>& r, FormatContext &ctx) {
    std::string type_spec;
    if constexpr (std::is_integral_v<T>) type_spec = ":x";
    return format_to(ctx.begin(),
        "best index: {}; match(/tent.)? {}/{}; "
        "num: {" + type_spec + "}; den: {" + type_spec + "}; score: {}"
        , (uint64_t)r.index, r.match, r.tmatch
        , r.sum_field_weights, r.sum_weights,
        (((double)r.sum_field_weights)/r.sum_weights)
        );
  }
};

template <typename T>
struct formatter<sel::CountResult<T>> {
  template <typename ParseContext>
  constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const sel::CountResult<T>& r, FormatContext &ctx) {
    return format_to(ctx.begin(), "matches/tent.: {}/{}", r.matches, r.tmatches);
  }
};
template <typename T>
struct formatter<sel::KMaxResult<T>> {
  template <typename ParseContext>
  constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const sel::KMaxResult<T>& r, FormatContext &ctx) {
    std::string type_spec;
    if constexpr (std::is_integral_v<T>) type_spec = ":x";
    std::string fmt_string = "";
    for (size_t i = 0; i != r.index.size(); i ++){
      fmt_string += fmt::format(
        "{}th best index: {}; match(/tent.)? {}/{}; "
        "num: {" + type_spec + "}; den: {" + type_spec + "}; score: {}\n"
        ,i, (uint64_t)r.index[i], r.match[i], r.tmatch[i]
        , r.sum_field_weights[i], r.sum_weights[i],
        (((double)r.sum_field_weights[i])/r.sum_weights[i])
          );
    }
    return format_to(ctx.out(), fmt_string);
  }
};

} // namespace fmt

#endif /* end of include guard: SEL_EPILINK_RESULT_HPP */
