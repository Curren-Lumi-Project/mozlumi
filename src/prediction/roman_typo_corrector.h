// Copyright 2010-2021, Google Inc.
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
//     * Neither the name of Google Inc. nor the names of its
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

// Mozlumi: Desktop typing correction (ATOK-like "誤入力補正") for romaji input.
//
// Example: "sunkai" (すんかい) -> "sinkai" (しんかい) -> 深海.
//
// The OSS SupplementalModel is a stub and has no access to the dictionary, so
// this corrector validates romaji edit candidates against the system/user
// dictionary directly.

#ifndef MOZC_PREDICTION_ROMAN_TYPO_CORRECTOR_H_
#define MOZC_PREDICTION_ROMAN_TYPO_CORRECTOR_H_

#include <cstddef>
#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "composer/query.h"
#include "composer/table.h"
#include "dictionary/dictionary_interface.h"
#include "request/conversion_request.h"

namespace mozc {
namespace prediction {

class RomanTypoCorrector {
 public:
  // Maximum number of corrected queries returned by Correct().
  static constexpr size_t kMaxQueries = 3;

  explicit RomanTypoCorrector(const dictionary::DictionaryInterface& dictionary)
      : dictionary_(dictionary) {}

  RomanTypoCorrector(const RomanTypoCorrector&) = delete;
  RomanTypoCorrector& operator=(const RomanTypoCorrector&) = delete;

  // Returns true if the request is eligible for desktop typing correction.
  static bool IsEnabled(const ConversionRequest& request);

  // Returns corrected hiragana queries ordered by plausibility. Returns an
  // empty vector when the original input looks correct.
  std::vector<composer::TypeCorrectedQuery> Correct(
      const ConversionRequest& request) const;

 private:
  bool HasExactKey(absl::string_view key) const;
  bool HasPredictiveKey(absl::string_view key,
                        const ConversionRequest& request) const;
  // Returns the smallest token cost of `key` (exact or predictive match).
  int GetMinCost(absl::string_view key, bool exact,
                 const ConversionRequest& request) const;

  const dictionary::DictionaryInterface& dictionary_;
  mutable absl::Mutex mutex_;
  mutable composer::TableManager table_manager_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace prediction
}  // namespace mozc

#endif  // MOZC_PREDICTION_ROMAN_TYPO_CORRECTOR_H_
