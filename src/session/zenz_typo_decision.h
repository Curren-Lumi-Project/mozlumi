#ifndef MOZC_SESSION_ZENZ_TYPO_DECISION_H_
#define MOZC_SESSION_ZENZ_TYPO_DECISION_H_

#include <cstddef>
#include <string>
#include <vector>

namespace mozc {
namespace session {

// Mozlumi: chooses the sentence typo fixes with Zenz.
//
// Zenz scores each reading (the typed one and Mozc's best fixes) with the
// left context: log P(reading, conversion | context). Tuned on the phrases of
// the bundled Aozora text (eval/zenz_typo_eval.py, 追記27・28):
//   - a fix that Mozc offers is kept only when Zenz prefers it to the typed
//     reading (fewer extra candidates for correctly typed phrases), and
//   - a fix that Mozc does not offer is added when Zenz prefers it clearly,
//     taking the typo cost into account.
// On two sets of phrases, the right fix was offered for 45-46% (Mozc: 39%)
// of the phrases broken by one typo, and the extra candidates for correctly
// typed phrases went from 8.3%/5.0% to 5.0%/3.7%.

struct ZenzTypoScoredFix {
  std::string key;          // Hiragana reading of the fix.
  std::string mozc_value;   // Mozc's conversion of the reading.
  std::string zenz_value;   // Zenz's conversion (may be empty).
  std::string edit;
  float typo_cost = 0.0f;
  bool offered = false;     // Offered by Mozc's rule.
  bool scored = false;      // Zenz returned a score.
  double logprob = 0.0;     // log P(reading, conversion | context).
};

struct ZenzTypoChoice {
  std::string key;
  std::string value;
  std::string edit;
  bool from_zenz = false;   // Added by Zenz (not offered by Mozc).
};

struct ZenzTypoDecisionOptions {
  double keep_margin = 0.0;
  double add_cost_per_typo = 4.0;
  double add_margin = 6.0;
  size_t max_choices = 3;
};

// Returns the fixes to show, best first. When the typed reading was not
// scored, Mozc's offers are returned unchanged.
std::vector<ZenzTypoChoice> DecideZenzTypoChoices(
    bool typed_scored, double typed_logprob,
    const std::vector<ZenzTypoScoredFix>& fixes,
    const ZenzTypoDecisionOptions& options = ZenzTypoDecisionOptions());

}  // namespace session
}  // namespace mozc

#endif  // MOZC_SESSION_ZENZ_TYPO_DECISION_H_
