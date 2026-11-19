#include "session/zenz_typo_decision.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace mozc {
namespace session {

std::vector<ZenzTypoChoice> DecideZenzTypoChoices(
    bool typed_scored, double typed_logprob,
    const std::vector<ZenzTypoScoredFix>& fixes,
    const ZenzTypoDecisionOptions& options, std::string* trace) {
  auto note = [&](const ZenzTypoScoredFix& fix, double gain, char mark) {
    if (trace == nullptr) {
      return;
    }
    char item[64];
    std::snprintf(item, sizeof(item), "%so%dg%.1ft%.1f:%c",
                  trace->empty() ? "" : " ", fix.offered ? 1 : 0, gain,
                  fix.typo_cost, mark);
    trace->append(item);
  };
  std::vector<ZenzTypoChoice> choices;
  if (!typed_scored) {
    for (const ZenzTypoScoredFix& fix : fixes) {
      if (fix.offered && choices.size() < options.max_choices) {
        choices.push_back({fix.key, fix.mozc_value, fix.edit, false});
        note(fix, 0.0, 'U');
      } else {
        note(fix, 0.0, 'u');
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
      note(fix, 0.0, fix.offered ? 'U' : 'u');
      continue;
    }
    const double gain = fix.logprob - typed_logprob;
    const double rank = gain - fix.typo_cost;
    if (fix.offered) {
      if (gain > options.keep_margin) {
        ranked.push_back({rank, {fix.key, fix.mozc_value, fix.edit, false}});
        note(fix, gain, 'K');
      } else {
        note(fix, gain, 'D');
      }
      continue;
    }
    if (gain - options.add_cost_per_typo * fix.typo_cost > options.add_margin) {
      const std::string& value =
          fix.zenz_value.empty() ? fix.mozc_value : fix.zenz_value;
      if (!value.empty()) {
        ranked.push_back({rank, {fix.key, value, fix.edit, true}});
        note(fix, gain, 'A');
        continue;
      }
    }
    note(fix, gain, '-');
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
