// Copyright 2026
// Licensed under the same terms as Mozc.

#include "session/zenz_repeat_check.h"

#include <cstddef>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/util.h"

namespace mozc {
namespace session {
namespace {

bool IsUtf8Continuation(absl::string_view text, size_t pos) {
  return pos < text.size() &&
         (static_cast<unsigned char>(text[pos]) & 0xC0) == 0x80;
}

bool IsRepeatCheckWord(absl::string_view word) {
  return Util::CharsLen(word) >= 2 &&
         (Util::ContainsScriptType(word, Util::KANJI) ||
          Util::ContainsScriptType(word, Util::KATAKANA));
}

}  // namespace

std::string ZenzChangedWord(absl::string_view mozc_value,
                            absl::string_view ai_value) {
  size_t prefix = 0;
  while (prefix < mozc_value.size() && prefix < ai_value.size() &&
         mozc_value[prefix] == ai_value[prefix]) {
    ++prefix;
  }
  // Back to the beginning of a character.
  while (prefix > 0 && IsUtf8Continuation(ai_value, prefix)) {
    --prefix;
  }
  size_t suffix = 0;
  while (suffix < mozc_value.size() - prefix &&
         suffix < ai_value.size() - prefix &&
         mozc_value[mozc_value.size() - 1 - suffix] ==
             ai_value[ai_value.size() - 1 - suffix]) {
    ++suffix;
  }
  // The common end begins at the beginning of a character.
  while (suffix > 0 && IsUtf8Continuation(ai_value, ai_value.size() - suffix)) {
    --suffix;
  }
  return std::string(ai_value.substr(prefix, ai_value.size() - prefix - suffix));
}

std::string ChooseZenzRepeatCheckValue(const ZenzRepeatCheck& check,
                                       const ZenzRepeatCheckScores& scores,
                                       double margin) {
  std::string value;
  double best = 0.0;
  if (scores.ai.has_value() && *scores.ai - scores.mozc >= margin) {
    best = *scores.ai - scores.mozc;
    value = check.ai_value;
  }
  if (scores.local.has_value() && !scores.local_value.empty() &&
      scores.local_value != check.mozc_value &&
      *scores.local - scores.mozc >= margin &&
      (value.empty() || *scores.local - scores.mozc > best)) {
    value = scores.local_value;
  }
  return value;
}

std::vector<ZenzRepeatCheck> FindZenzRepeatChecks(
    absl::string_view left_context, absl::Span<const ZenzRankPart> parts,
    size_t max_checks) {
  std::vector<ZenzRepeatCheck> checks;
  for (size_t k = 0; k < parts.size() && checks.size() < max_checks; ++k) {
    const ZenzRankPart& part = parts[k];
    if (!part.changed) {
      continue;
    }
    const std::string word = ZenzChangedWord(part.mozc_value, part.ai_value);
    if (!IsRepeatCheckWord(word)) {
      continue;
    }
    std::string before(left_context);
    for (size_t j = 0; j < k; ++j) {
      before.append(parts[j].ai_value);
    }
    std::string after;
    for (size_t j = k + 1; j < parts.size(); ++j) {
      after.append(parts[j].ai_value);
    }
    // A match of valid UTF-8 text starts at the beginning of a character.
    const size_t pos = before.rfind(word);
    if (pos == std::string::npos && after.find(word) == std::string::npos) {
      continue;
    }
    ZenzRepeatCheck check;
    check.segment_index = part.segment_index;
    check.key = part.key;
    check.mozc_value = part.mozc_value;
    check.ai_value = part.ai_value;
    check.left_context = pos == std::string::npos
                             ? std::move(before)
                             : before.substr(pos + word.size());
    checks.push_back(std::move(check));
  }
  return checks;
}

std::string ZenzLeadingWord(absl::string_view value) {
  size_t end = 0;
  while (end < value.size()) {
    size_t next = end + 1;
    while (IsUtf8Continuation(value, next)) {
      ++next;
    }
    const Util::ScriptType type =
        Util::GetScriptType(value.substr(end, next - end));
    if (type != Util::KANJI && type != Util::KATAKANA) {
      break;
    }
    end = next;
  }
  return std::string(value.substr(0, end));
}

std::vector<ZenzSameWordCheck> FindZenzSameWordChecks(
    absl::string_view left_context, absl::Span<const ZenzRankPart> parts,
    size_t max_checks) {
  std::vector<ZenzSameWordCheck> checks;
  std::string before(left_context);
  for (size_t k = 0; k < parts.size() && checks.size() < max_checks; ++k) {
    const ZenzRankPart& part = parts[k];
    if (!part.changed && part.mozc_value == part.ai_value) {
      const std::string word = ZenzLeadingWord(part.ai_value);
      const size_t pos = Util::CharsLen(word) >= 2 &&
                                 Util::ContainsScriptType(word, Util::KANJI)
                             ? before.rfind(word)
                             : std::string::npos;
      if (pos != std::string::npos) {
        ZenzSameWordCheck check;
        check.segment_index = part.segment_index;
        check.key = part.key;
        check.value = part.ai_value;
        check.word = word;
        check.left_context = before.substr(pos + word.size());
        checks.push_back(std::move(check));
      }
    }
    before.append(part.ai_value);
  }
  return checks;
}

bool IsZenzSameWordAnswer(const ZenzSameWordCheck& check,
                          absl::string_view answer) {
  if (check.word.empty() || check.value.size() < check.word.size()) {
    return false;
  }
  const absl::string_view rest =
      absl::string_view(check.value).substr(check.word.size());
  return answer.size() > rest.size() && answer != check.value &&
         answer.substr(0, check.word.size()) != check.word &&
         answer.substr(answer.size() - rest.size()) == rest;
}

}  // namespace session
}  // namespace mozc
