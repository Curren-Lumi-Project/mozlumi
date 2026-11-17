// Copyright 2026 Curren Lumi Project.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Mozlumi: Learns the user's typing habits for the romaji typo corrector.
//
// Only typo patterns such as "s:ai" (typed "i" for "a") or "t:hs" (typed
// "hs" for "sh") and their counts are stored; the typed text is not.

#ifndef MOZC_PREDICTION_TYPO_HABIT_STORE_H_
#define MOZC_PREDICTION_TYPO_HABIT_STORE_H_

#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <string_view>

namespace mozc {
namespace prediction {

class TypoHabitStore {
 public:
  // Cost reduction per learned occurrence of a pattern, and the upper bounds.
  static constexpr float kBonusPerCount = 0.1f;
  static constexpr float kMaxBonusPerEdit = 0.4f;
  static constexpr float kMaxBonus = 0.5f;
  static constexpr int kMaxCount = 20;
  static constexpr size_t kMaxPatterns = 300;
  static constexpr size_t kMaxRecent = 64;

  // `path` is the file to load from and save to. An empty path keeps the
  // habits in memory only.
  explicit TypoHabitStore(std::string path);

  TypoHabitStore(const TypoHabitStore&) = delete;
  TypoHabitStore& operator=(const TypoHabitStore&) = delete;

  // The store shared by the converter process, saved in the user profile
  // directory.
  static TypoHabitStore* Get();

  // Returns a non-positive value to be added to the cost of a typo candidate
  // whose edits are `edits` ('|' separated).
  float CostAdjustment(std::string_view edits) const;

  // Remembers that the typo corrector proposed `key` for the raw input `raw`
  // with `edits`, so that the edits can be learned when the user commits it.
  void Remember(std::string_view raw, std::string_view key,
                std::string_view edits);

  // Called when the user commits a typing-correction candidate whose reading
  // is `committed_key` for the raw input `raw`. Returns true if a pattern was
  // learned.
  bool LearnCommitted(std::string_view raw, std::string_view committed_key);

  int Count(std::string_view edit) const;

 private:
  struct Recent {
    std::string raw;
    std::string key;
    std::string edits;
  };

  void LoadLocked();
  void SaveLocked() const;

  const std::string path_;
  mutable std::mutex mutex_;
  std::map<std::string, int, std::less<>> counts_;
  std::deque<Recent> recent_;
};

}  // namespace prediction
}  // namespace mozc

#endif  // MOZC_PREDICTION_TYPO_HABIT_STORE_H_
