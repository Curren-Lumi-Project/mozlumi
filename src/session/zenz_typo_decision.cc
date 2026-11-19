#include "session/zenz_typo_decision.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace mozc {
namespace session {

std::vector<ZenzTypoChoice> DecideZenzTypoChoices(
    bool typed_scored, double typed_logprob,
    const std::vector<ZenzTypoScoredFix>& fixes,
    const ZenzTypoDecisionOptions& options) {
  std::vector<ZenzTypoChoice> choices;
  if (!typed_scored) {
    for (const ZenzTypoScoredFix& fix : fixes) {
      if (fix.offered && choices.size() < options.max_choices) {
        choices.push_back({fix.key, fix.mozc_value, fix.edit, false});
      }
    }
    return choices;
  }

  struct Ranked {
    double rank;
    ZenzTypoChoice choice;
  };
  std::vector<Ranked> ranked;
  std::vector<ZenzTypoChoice> unscored_offers;
  for (const ZenzTypoScoredFix& fix : fixes) {
    const bool duplicated =
        std::any_of(ranked.begin(), ranked.end(), [&](const Ranked& r) {
          return r.choice.key == fix.key;
        });
    if (duplicated || fix.key.empty()) {
      continue;
    }
    if (!fix.scored) {
      // No information from Zenz: keep Mozc's decision.
      if (fix.offered) {
        unscored_offers.push_back({fix.key, fix.mozc_value, fix.edit, false});
      }
      continue;
    }
    const double gain = fix.logprob - typed_logprob;
    const double rank = gain - fix.typo_cost;
    if (fix.offered) {
      if (gain > options.keep_margin) {
        ranked.push_back({rank, {fix.key, fix.mozc_value, fix.edit, false}});
      }
      continue;
    }
    if (gain - options.add_cost_per_typo * fix.typo_cost > options.add_margin) {
      const std::string& value =
          fix.zenz_value.empty() ? fix.mozc_value : fix.zenz_value;
      if (!value.empty()) {
        ranked.push_back({rank, {fix.key, value, fix.edit, true}});
      }
    }
  }
  std::stable_sort(ranked.begin(), ranked.end(),
                   [](const Ranked& a, const Ranked& b) {
                     return a.rank > b.rank;
                   });
  for (Ranked& r : ranked) {
    if (choices.size() >= options.max_choices) break;
    choices.push_back(std::move(r.choice));
  }
  for (ZenzTypoChoice& c : unscored_offers) {
    if (choices.size() >= options.max_choices) break;
    choices.push_back(std::move(c));
  }
  return choices;
}

}  // namespace session
}  // namespace mozc
