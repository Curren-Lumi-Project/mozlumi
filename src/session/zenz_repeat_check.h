// Copyright 2026
// Licensed under the same terms as Mozc.

#ifndef MOZC_SESSION_ZENZ_REPEAT_CHECK_H_
#define MOZC_SESSION_ZENZ_REPEAT_CHECK_H_

#include <cstddef>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace mozc {
namespace session {

// Mozlumi: Zenz (zenz-v3.2-small) tends to make a word the same as a word of
// the same reading before or after it: "同期してたら同期がした" for
// "どうきしてたらどうきがした" (eval/zenz_cases_20261125c.tsv). When the AI
// conversion changes a word into a word that is also in the sentence or in
// the preceding text, the change is judged again without the other word: the
// preceding text is cut after it, and the text after the segment is left out.
// Then Zenz chose the right words, also for a sentence that really repeats a
// word (eval/zenz_cases_20261125d.tsv, 2026-11-25).

// One segment of the conversion compared by the AI rank, in order.
struct ZenzRankPart {
  // Index into the conversion segments (as ExternalSegmentCandidate).
  size_t segment_index = 0;
  std::string key;
  std::string mozc_value;
  std::string ai_value;
  // True when the AI conversion changes this segment.
  bool changed = false;
};

// A changed segment to judge again.
struct ZenzRepeatCheck {
  size_t segment_index = 0;
  std::string key;
  std::string mozc_value;
  std::string ai_value;
  // The text before the segment (the preceding text and the AI conversion
  // before it), cut after the other word when it is there.
  std::string left_context;
};

// The part of `ai_value` that differs from `mozc_value`: the common beginning
// and end (whole characters) are removed. "同期してたら" and "動悸してたら"
// give "動悸".
std::string ZenzChangedWord(absl::string_view mozc_value,
                            absl::string_view ai_value);

// The changed segments of `parts` whose new word (2 characters or more, with
// kanji or katakana) is also in `left_context` or in the AI conversion of the
// other parts. At most `max_checks`, from the beginning.
std::vector<ZenzRepeatCheck> FindZenzRepeatChecks(
    absl::string_view left_context, absl::Span<const ZenzRankPart> parts,
    size_t max_checks);

}  // namespace session
}  // namespace mozc

#endif  // MOZC_SESSION_ZENZ_REPEAT_CHECK_H_
