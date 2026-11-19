// Copyright 2026 Curren Lumi Project.
// Licensed under the same terms as Mozc.

#ifndef MOZC_PREDICTION_TYPO_SELF_FIX_STORE_H_
#define MOZC_PREDICTION_TYPO_SELF_FIX_STORE_H_

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mozc {
namespace prediction {

// Mozlumi: typos the user fixed by typing again. When a converted romaji
// input was edited by one or two keys before committing (for example,
// "issunn" converted to 一寸, then retyped as "issyunn" for 一瞬), the edited
// part with a few keys around it is remembered ("issunn" -> "issyunn"). The
// next time the input contains it, the fix is offered as a candidate even if
// the conversion of the input itself looks natural.
class TypoSelfFixStore {
 public:
  static constexpr size_t kContextChars = 3;
  static constexpr size_t kMinPatternChars = 4;
  static constexpr size_t kMaxEditChars = 2;
  static constexpr size_t kMaxPatterns = 200;
  static constexpr int kMaxCount = 20;

  struct Fix {
    std::string romaji;  // The whole input with the fix applied.
    int count = 0;       // How many times the user made this fix.
  };

  // `path` is the file to load from and save to. An empty path keeps the
  // fixes in memory only.
  explicit TypoSelfFixStore(std::string path);
  TypoSelfFixStore(const TypoSelfFixStore&) = delete;
  TypoSelfFixStore& operator=(const TypoSelfFixStore&) = delete;

  // The store shared by the converter process, saved in the user profile
  // directory.
  static TypoSelfFixStore* Get();

  // The remembered pattern for `typed` retyped as `committed`, or nullopt
  // when they differ by more than a small local edit.
  static std::optional<std::pair<std::string, std::string>> ExtractPattern(
      std::string_view typed, std::string_view committed);

  // Learns the pattern of `typed` retyped as `committed`. Returns true if
  // one was learned.
  bool Learn(std::string_view typed, std::string_view committed);

  // Fixes for the romaji input `raw`, most frequent first.
  std::vector<Fix> FindFixes(std::string_view raw, size_t max_fixes) const;

 private:
  void LoadLocked();
  void SaveLocked() const;

  const std::string path_;
  mutable std::mutex mutex_;
  // wrong pattern -> (right pattern, count)
  std::map<std::string, std::pair<std::string, int>, std::less<>> fixes_;
};

}  // namespace prediction
}  // namespace mozc

#endif  // MOZC_PREDICTION_TYPO_SELF_FIX_STORE_H_
