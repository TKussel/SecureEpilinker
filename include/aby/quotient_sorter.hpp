/**
 \file    sel/aby/quotient_sorter.h
 \author  Tobias Kussel <tobias.kussel@cysec.de>
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
 \brief Sort Share Quotients with min or max, returns max/min k elements
        * may choose to apply more complex tie logix
        * may select same index in other (vector of) target share(s)
*/

#ifndef SEL_ABY_QUOTIENT_SORTER_H
#define SEL_ABY_QUOTIENT_SORTER_H
#include <string>
#pragma once

#include "gadgets.h"
#include <vector>

namespace sel {

template <class ShareT>
class QuotientSorter {
  static constexpr bool do_conversion = std::is_same_v<ShareT, ArithShare>;
public:
  enum class SortOp { MIN, MIN_TIE, MAX, MAX_TIE };

  QuotientSorter(Quotient<ShareT>&& selector, SortOp _sort_op = SortOp::MAX_TIE,
      std::vector<BoolShare>&& targets = {}, size_t _k = 3)
    : sort_op{_sort_op}, k{_k}
  {
    assert(k<= selector.num.get_nvals());
    assert(selector.num.get_nvals() == selector.den.get_nvals());
    auto tempvec = nval_to_vec(std::move(selector));
    auto [rq, mq] = split_vec(std::move(tempvec), k);
    max_quot = std::move(mq);
    rest = std::move(rq);
    auto [rt, mt] = split_vec(std::move(targets[0].split(1)), k);
    rest_target = std::move(rt);
    max_target = std::move(mt);
    if constexpr (!do_conversion) {
      static const T2BConverter<BoolShare> bool_identity = [](auto x){return x;};
      to_bool = &bool_identity;
    }
  }

  void set_sort_operation(SortOp _sort_op) {
    sort_op = _sort_op;
  }

  /**
   * Set converters and denominator bits
   * We only convert back and forth if we use arithmetic shares.
   * In this case, it also makes sense to tell the bitsize of denominators, so
   * that arithmetic shares that are converted back to boolean space can be
   * truncated.
   */
  void set_converters_and_den_bits(
    T2BConverter<ShareT> const* _to_bool, B2AConverter const* _to_arith,
    const size_t _den_bits = 0) {
      to_bool = _to_bool;
      to_arith = _to_arith;
      den_bits = _den_bits;
  }

  void reset(){
  for(auto& q : max_quot){q.num.reset(); q.den.reset();}
  for(auto& q : rest){q.num.reset(); q.den.reset();}
  for(auto& t : max_target){t.reset();}
  for(auto& t : rest_target){t.reset();}
  }

  auto get_selector(size_t i) const {assert(i<k);return max_quot[i];}
  auto get_target(size_t i) const {assert(i<k);return max_target[i];}

  void slow_sort(){
    auto op_select = make_selector();
    bubble_sort_all(op_select);
    sorted = true;
  }
  void sort() {
    auto op_select = make_selector();
    // Sort max_quot
    bubble_sort(op_select);
    assert(rest.size() == rest_target.size());
    for (size_t i = 0; i != rest.size(); i++){
      ////sort other in
      sort_once(rest[i], rest_target[i], op_select);
    }
    sorted = true;
  }

private:
  std::vector<Quotient<ShareT>> max_quot;
  std::vector<Quotient<ShareT>> rest;
  std::vector<BoolShare> max_target;
  std::vector<BoolShare> rest_target;
  SortOp sort_op;
  T2BConverter<ShareT> const* to_bool = nullptr;
  B2AConverter const* to_arith = nullptr;
  size_t den_bits = 0;
  bool sorted = false;
  size_t k = 3;

  QuotientSelector<ShareT> make_selector() {
    switch (sort_op) {
      case SortOp::MIN:
        return make_min_selector(*to_bool);
      case SortOp::MAX:
        return make_max_selector(*to_bool);
      case SortOp::MIN_TIE:
        return make_min_tie_selector(*to_bool, den_bits);
      default: // MAX_TIE is default
        return make_max_tie_selector(*to_bool, den_bits);
    }
  }

  void swap_quotients(std::vector<Quotient<ShareT>>& container, std::size_t i, std::size_t j, BoolShare swap){
    Quotient<ShareT> temp = container[i];
    if constexpr(do_conversion){
      ArithShare arith_sel = (*to_arith)(swap);
      ArithmeticCircuit* ac = container[i].num.get_circuit();
      ArithShare one = constant_simd(ac, 1u, ac->GetShareBitLen(), 1);
      ArithShare not_sel = one - arith_sel;
      container[i].num = arith_sel * container[j].num + not_sel * container[i].num;
      container[i].den = arith_sel * container[j].den + not_sel * container[i].den;
      container[j].num = arith_sel * temp.num + not_sel * container[j].num;
      container[j].den = arith_sel * temp.den + not_sel * container[j].den;
    } else {
      container[i].num = swap.mux(container[j].num, container[i].num);
      container[i].den = swap.mux(container[j].den, container[i].den);
      container[j].num = swap.mux(temp.num, container[j].num);
      container[j].den = swap.mux(temp.den, container[j].den);
    }
  }
  void swap_targets(std::vector<BoolShare>& container, std::size_t i, std::size_t j, BoolShare swap){
    BoolShare temp = container[i];
    container[i] = swap.mux(container[j], container[i]);
    container[j] = swap.mux(temp, container[j]);
  }

  void sort_once(const Quotient<ShareT>& with, BoolShare& target, const QuotientSelector<ShareT>& op_select) {
    assert(max_quot.size() == k);
    //auto selection = op_select(base.selector, with.selector);
    std::vector<BoolShare> comp_array(k);
    std::vector<BoolShare> swap_array(k-1);
    for(size_t i = 0; i != k; i++){
      comp_array[i] = op_select(with, max_quot[i]);
    }
    for(size_t i = 0; i != k-1; i++){
      swap_array[i] = (comp_array[i] & comp_array[i+1]);
    }
    // Swap elements, so that smallest/largest value on replace position
    if(sort_op == SortOp::MAX || sort_op == SortOp::MAX_TIE){ // For Max swap from right
      for (size_t i = k-1; i != 0; i--){
        swap_quotients(max_quot, i-1, i, swap_array[i-1]);
        swap_targets(max_target, i-1, i, swap_array[i-1]);
      }
    } else if(sort_op == SortOp::MIN || sort_op == SortOp::MIN_TIE){ // For Min swap from left
      for (size_t i = 0; i != k-1; i++){
        swap_quotients(max_quot, i, i+1, swap_array[i]);
        swap_targets(max_target, i, i+1, swap_array[i]);
      }
    }

    std::vector<BoolShare> replace_array(k);
    replace_array[0] = comp_array[0];
    for(size_t i = 1; i != k; i++){
      replace_array[i] = (~comp_array[i-1]) & comp_array[i];
    }
    for(size_t i = 0; i != k; i++){ // replace element in appropriate prosition
      if constexpr (do_conversion) {
        ArithShare arith_sel = (*to_arith)(replace_array[i]);
        ArithmeticCircuit* ac = max_quot[0].num.get_circuit();
        ArithShare one = constant_simd(ac, 1u, ac->GetShareBitLen(), 1);
        ArithShare not_sel = one - arith_sel;
        max_quot[i].num = arith_sel * with.num + not_sel * max_quot[i].num;
        max_quot[i].den = arith_sel * with.den + not_sel * max_quot[i].den;
      } else {
        max_quot[i].num = replace_array[i].mux(with.num, max_quot[i].num);
        max_quot[i].den = replace_array[i].mux(with.den, max_quot[i].den);
      }
      //targets always in BoolShare
      max_target[i] = replace_array[i].mux(target, max_target[i]);
    }
  }
  void bubble_sort(const QuotientSelector<ShareT>& op_select){
    for (size_t i = 0; i != k-1; i++){
      for(size_t j = 0; j != k-1; j++){
        auto swap = op_select(max_quot[j+1], max_quot[j]);
        swap_quotients(max_quot, j, j+1, swap);
        swap_targets(max_target, j, j+1, swap);
      }
    }
  }
  void bubble_sort_all(const QuotientSelector<ShareT>& op_select){
    std::vector<Quotient<ShareT>> all;
    std::vector<BoolShare> all_targets;
    all.reserve(k+ rest.size());
    all_targets.reserve(k+ rest_target.size());
    all.insert(all.end(), max_quot.begin(), max_quot.end());
    all.insert(all.end(), rest.begin(), rest.end());
    all_targets.insert(all_targets.end(), max_target.begin(), max_target.end());
    all_targets.insert(all_targets.end(), rest_target.begin(), rest_target.end());
    for (size_t i = 0; i != all.size()-1; i++){
      for(size_t j = 0; j != all.size()-1; j++){
        auto swap = op_select(all[j+1], all[j]);
        swap_quotients(all, j, j+1, swap);
        swap_targets(all_targets, j, j+1, swap);
      }
    }
    auto [m, r] = split_vec(std::move(all), k);
    auto [mt, rt] = split_vec(std::move(all_targets), k);
    max_quot = std::move(m);
    max_target = std::move(mt);
    rest = std::move(r);
    rest_target = std::move(rt);
  }
};


} /* end of namespace: sel */

#endif /* end of include guard: SEL_ABY_QUOTIENT_SORTER_H */
