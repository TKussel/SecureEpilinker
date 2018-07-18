/**
 \file    clear_epilinker.cpp
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
 \brief Epilink Algorithm on clear text values / integer and exact double implementation
*/

#include <stdexcept>
#include <algorithm>
#include "util.h"
#include "clear_epilinker.h"

using namespace std;
using fmt::print, fmt::format;

namespace sel::clear_epilink {

constexpr auto BIN = FieldComparator::BINARY;
constexpr auto BM = FieldComparator::DICE;

/******************** FieldWeight ********************/
/**
 * A FieldWeight represents the addends in the sums of the Epilink algorithm.
 * The EpiLink score is sum(w_i * c_i) / sum(w_i), where the w_i are the weights
 * and c_i is the result of a comparison (binary (0/1) or dice-coefficient).
 * field_weight() returns w_i*c_i in the member fw and the weight w_i in w.
 * A FieldWeight with w==0 is interpreted as if any entry in the comparison was
 * empty.
 */
template<typename T>
struct FieldWeight {
  T fw{0}, w{0};
};

/**
 * When we add two FieldWeights in this context, we simply sum the field-weights
 * and weights. This happens before determining the maximum score.
 */
template<typename T>
FieldWeight<T>& operator+=(FieldWeight<T>& me, const FieldWeight<T>& other) {
  me.fw += other.fw;
  me.w += other.w;
  return me;
}

/**
 * We compare two FieldWeights as quotients because they are treated as the
 * resulting Epilink score of two different records. This happens after summing
 * individual FieldWeights.
 */
template<typename T>
bool operator<(const FieldWeight<T>& left, const FieldWeight<T>& right) {
  if (right.w == 0) return false;
  if (left.w == 0) return true;
#ifdef DEBUG_SEL_CLEAR
  if constexpr(is_same_v<T, CircUnit>) {
    CircUnit a = left.fw * right.w;
    CircUnit b = right.fw * left.w;
    uint64_t al = uint64_t(left.fw) * right.w;
    uint64_t bl = uint64_t(right.fw) * left.w;
    if (a != al) print("< Overflow left: {:x}*{:x} = {:x} != {:x}\n",
        left.fw, right.w, a, al);
    if (b != bl) print("< Overflow right: {:x}*{:x} = {:x} != {:x}\n",
        right.fw, left.w, b, bl);
  }
#endif
  return left.fw * right.w < right.fw * left.w;
}

/******************** Input Class ********************/
Input::Input(const std::map<FieldName, FieldEntry>& record,
        const std::map<FieldName, VFieldEntry>& database) :
  record{record}, database{database},
  nvals{(*database.cbegin()).second.size()}
{
  for (const auto& col : database) {
    check_vector_size(col.second, nvals, "Database column "s + col.first);
  }
}

Input::Input(const EpilinkClientInput& in_client,
    const EpilinkServerInput& in_server) :
  Input{in_client.record, in_server.database}
{
  if (in_client.nvals != in_server.nvals)
    throw logic_error("Client and server input need to be of same nvals.");
}

/******************** Comparators & Threshold ********************
 * Comparators and threshold introduce a left-shift for integer calculation but
 * don't for exact double calculation.
 */

template<typename T> constexpr
T scale(T val, size_t prec) {
  if constexpr (is_same_v<T, CircUnit>) val <<= prec;
  return val;
}

template<typename T>
T dice(const Bitmask& left, const Bitmask& right, size_t prec) {
  size_t hw_plus = hw(left) + hw(right);
  if (hw_plus == 0) return 0;

  size_t hw_and = hw(bm_and(left, right));
  return (T)(2 * scale<T>(hw_and, prec)) / (T)(hw_plus);
}

template<typename T>
T equality(const Bitmask& left, const Bitmask& right, size_t prec) {
  return ((left == right) ? scale<T>(1, prec) : 0);
}

template<typename T>
bool test_threshold(const FieldWeight<T>& q, const double thr, const size_t prec) {
  return (T)(thr * scale<T>(1, prec)) * q.w < q.fw;
}

template<typename T>
T scaled_weight(const FieldName& ileft, const FieldName& iright, const EpilinkConfig& cfg) {
  const double _weight = (cfg.fields.at(ileft).weight + cfg.fields.at(iright).weight)/2;
  if constexpr (is_same_v<T, CircUnit>) {
    return rescale_weight(_weight, cfg.weight_prec, cfg.max_weight);
  } else {
    return _weight;
  }
}

/******************** Algorithm Flow Components ********************/
template<typename T>
FieldWeight<T> field_weight(const Input& input, const EpilinkConfig& cfg,
    const size_t idx, const FieldName& ileft, const FieldName& iright) {
  const FieldComparator ftype = cfg.fields.at(ileft).comparator;

  // 1. Check if both entries have values
  const FieldEntry& client_entry = input.record.at(ileft);
  const FieldEntry& server_entry = input.database.at(iright)[idx];
  const bool delta = (client_entry.has_value() && server_entry.has_value());
  if (!delta){
#ifdef DEBUG_SEL_CLEAR
    string who = "both";
    if (client_entry.has_value()) {
      who = "right";
    } else if (server_entry.has_value()) {
      who = "left";
    }
    print("({}|{}|{})[{}] <{} empty>\n", ftype, ileft, iright, idx, who);
#endif
    return {0, 0};
  }

  const T weight = scaled_weight<T>(ileft, iright, cfg);
  // 2. Compare values
  T comp;
  switch(ftype) {
    case BM: {
      comp = dice<T>(client_entry.value(), server_entry.value(), cfg.dice_prec);
      break;
    }
    case BIN: {
      comp = equality<T>(client_entry.value(), server_entry.value(), cfg.dice_prec);
      break;
    }
  }

#ifdef DEBUG_SEL_CLEAR
  string tf = (is_integral_v<T>) ? ":x" : "";
  print("({}|{}|{})[{}] weight: {"+tf+"}; comp: {"+tf+"}; field weight: {"+tf+"}\n",
      ftype, ileft, iright, idx, weight, comp, weight*comp);
#endif

  return {comp * weight, weight};
}

#ifdef DEBUG_SEL_CLEAR
template<typename Ref>
void print_score(const string& pfx, const Ref& r,
    const FieldWeight<CircUnit>& score, const size_t prec) {
  print(">>> {} {} score: {:x}/({:x} << {} = {:x}) = {}\n",
      pfx, r, score.fw, score.w, prec, (score.w << prec),
      ((double)score.fw)/(score.w << prec));
}

template<typename Ref>
void print_score(const string& pfx, const Ref& r,
    const FieldWeight<double>& score, const size_t) {
  print(">>> {} {} score: {}/{} = {}\n",
      pfx, r, score.fw, score.w, (score.fw/score.w));
}
#endif

template<typename T>
FieldWeight<T> best_group_weight(const Input& input, const EpilinkConfig& cfg,
    const size_t idx, const IndexSet& group_set) {
  vector<FieldName> group{begin(group_set), end(group_set)};
  // copy group to store permutations
  vector<FieldName> groupPerm{group.begin(), group.end()};
  size_t size = group.size();

#ifdef DEBUG_SEL_CLEAR
  print("---------- Group {} ----------\n", group);
  vector<FieldName> groupBest;
#endif

  // iterate over all group permutations and calc field-weight
  FieldWeight<T> best_perm;
  do {
    FieldWeight<T> score;
    for (size_t i = 0; i != size; ++i) {
      const auto& ileft = group[i];
      const auto& iright = groupPerm[i];
      score += field_weight<T>(input, cfg, idx, ileft, iright);
    }

#ifdef DEBUG_SEL_CLEAR
  print_score("Permutation", groupPerm, score, cfg.dice_prec);
#endif

    if (best_perm < score) {
      best_perm = score;
#ifdef DEBUG_SEL_CLEAR
      groupBest = groupPerm;
#endif
    }
  } while(next_permutation(groupPerm.begin(), groupPerm.end()));

#ifdef DEBUG_SEL_CLEAR
  print_score("Best group:", groupBest, best_perm, cfg.dice_prec);
#endif

  return best_perm;
}

template<typename T>
Result<T> calc(const Input& input, const EpilinkConfig& cfg) {
  // lock inputs during calculation
  const size_t nvals = input.nvals;

  // Accumulator of individual field_weights
  vector<FieldWeight<T>> scores(nvals);

  // 1. Field weights of individual fields
  // 1.1 For all exchange groups, find the permutation with the highest score
  // Where we collect indices not already used in an exchange group
  IndexSet no_x_group;
  // fill with field names, remove later
  for (const auto& field : cfg.fields) no_x_group.insert(field.first);
  // for each group, store the best permutation's weight into field_weights
  for (const auto& group : cfg.exchange_groups) {
    // add this group's field weight to vector
    for (size_t idx = 0; idx != nvals; ++idx) {
      scores[idx] += best_group_weight<T>(input, cfg, idx, group);
    }
    // remove all indices that were covered by this index group
    for (const auto& i : group) no_x_group.erase(i);
  }

#ifdef DEBUG_SEL_CLEAR
  print("---------- No-X-Group {} ----------\n", no_x_group);
#endif

  // 1.2 Remaining indices
  for (const auto& i : no_x_group) {
    for (size_t idx = 0; idx != nvals; ++idx) {
      scores[idx] += field_weight<T>(input, cfg, idx, i, i);
    }
  }

#ifdef DEBUG_SEL_CLEAR
  print("---------- Final Scores ({}) ----------\n", nvals);
  for (size_t idx = 0; idx != nvals; ++idx) {
    print_score("Idx", idx, scores[idx], cfg.dice_prec);
  }
#endif

  // 2. Determine best score (index)
  const auto best_score_it = max_element(scores.cbegin(), scores.cend());
  const CircUnit best_idx = distance(scores.cbegin(), best_score_it);
  const auto& best_score = *best_score_it;

  // 3. Test thresholds
  const bool match = test_threshold(best_score, cfg.threshold, cfg.dice_prec);
  const bool tmatch = test_threshold(best_score, cfg.tthreshold, cfg.dice_prec);

  // Need to apply dice precision shift to sum(weights) to bring to same scale
  // as sum(field-weights). This was implicitly done in the threshold test
  // before.
  return {best_idx, match, tmatch,
    best_score.fw, scale<T>(best_score.w, cfg.dice_prec)};
}

Result<CircUnit> calc_integer(const Input& input, const EpilinkConfig& cfg) {
  return calc<CircUnit>(input, cfg);
}
Result<double> calc_exact(const Input& input, const EpilinkConfig& cfg) {
  return calc<double>(input, cfg);
}

} /* end of namespace sel::clear_epilink */
