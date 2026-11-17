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

#include "prediction/roman_typo_corrector.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/query.h"
#include "composer/table.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "prediction/roman_typo_candidates.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "transliteration/transliteration.h"

namespace mozc {
namespace prediction {
namespace {

using ::mozc::dictionary::DictionaryInterface;

// Minimum length of the hiragana key to be corrected.
constexpr size_t kMinKeyLength = 3;
// Upper bound of edit candidates checked against the dictionary per request.
constexpr size_t kMaxCandidatesToCheck = 400;
// Extra cost when the corrected key only matches as a prefix (the user is
// still typing the word).
constexpr float kPredictiveOnlyCost = 0.3f;
// Weight of the word frequency (dictionary cost / 1000) when ranking the
// corrected keys. Tuned on synthetic one-keystroke typos of dictionary words
// (0.2-0.8 performed almost equally).
constexpr float kFrequencyWeight = 0.3f;
// Used when no token cost is available.
constexpr int kUnknownCost = 10000;
// Number of tokens inspected to estimate the cost of a predictive-only key.
constexpr int kMaxPredictiveTokens = 20;
// When the input is a prefix of a word whose cost is at most this value, the
// user is probably still typing that word, so the input is not corrected.
// Prefixes that only lead to rare words (e.g. "すんかい" -> "スンカイ駅" in
// the bundled UT dictionary) are still corrected. Tuned on synthetic typos
// with the bundled dictionaries (5000-8000 performed similarly).
constexpr int kPrefixSkipMaxCost = 7000;
// Two-keystroke typos are searched only on explicit prediction (Tab) and on
// conversion, for raw input of at least this length, and only when no
// one-keystroke correction was found. Doing it on every keystroke doubled
// the corrections shown while typing correct phrases (synthetic evaluation:
// 8.6% -> 16.4% of keystrokes), while it fixes about 60% of two-typo words.
constexpr size_t kTwoEditMinRawLength = 8;
constexpr size_t kTwoEditFirstK = 40;
constexpr size_t kMaxTwoEditCandidatesToCheck = 1000;
constexpr float kTwoEditExtraCost = 0.3f;
// bias = -(kBiasBase + cost * kBiasPerCost). The predictor adds
// -1150 * bias to wcost, so corrected candidates rank below the literal ones.
constexpr float kBiasBase = 0.3f;
constexpr float kBiasPerCost = 0.3f;

bool IsAsciiAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool ContainsAsciiAlpha(absl::string_view s) {
  return std::any_of(s.begin(), s.end(), IsAsciiAlpha);
}

// Number of trailing ASCII letters, i.e. romaji still pending in the
// composition ("かいs" -> 1).
size_t TrailingAsciiLength(absl::string_view s) {
  size_t n = 0;
  while (n < s.size() && IsAsciiAlpha(s[s.size() - 1 - n])) {
    ++n;
  }
  return n;
}

struct Scored {
  std::string key;
  float cost;
  float rank_score;
  std::string edit;
};

bool CandidateLess(const RomanTypoCandidate& a, const RomanTypoCandidate& b) {
  if (a.cost != b.cost) return a.cost < b.cost;
  return a.romaji < b.romaji;
}

}  // namespace

bool RomanTypoCorrector::IsEnabled(const ConversionRequest& request) {
  if (!request.config().use_typing_correction()) {
    return false;
  }
  if (request.request().mixed_conversion()) {
    // Mobile uses the SupplementalModel based typing correction.
    return false;
  }
  if (request.config().preedit_method() != config::Config::ROMAN) {
    return false;
  }
  if (request.composer().GetInputMode() != transliteration::HIRAGANA) {
    return false;
  }
  if (request.request_type() != ConversionRequest::SUGGESTION &&
      request.request_type() != ConversionRequest::PREDICTION) {
    return false;
  }
  return Util::CharsLen(request.key()) >= kMinKeyLength;
}

bool RomanTypoCorrector::HasExactKey(absl::string_view key) const {
  return dictionary_.HasKey(key);
}

bool RomanTypoCorrector::HasPredictiveKey(
    absl::string_view key, const ConversionRequest& request) const {
  bool found = false;
  dictionary::InlineCallback callback;
  callback.OnToken([&found](absl::string_view, absl::string_view,
                            const dictionary::Token&) {
    found = true;
    return DictionaryInterface::Callback::TRAVERSE_DONE;
  });
  dictionary_.LookupPredictive(key, request.options(), &callback);
  return found;
}

int RomanTypoCorrector::GetMinCost(absl::string_view key, bool exact,
                                   const ConversionRequest& request) const {
  int min_cost = kUnknownCost;
  int count = 0;
  dictionary::InlineCallback callback;
  callback.OnToken([&](absl::string_view, absl::string_view,
                       const dictionary::Token& token) {
    min_cost = std::min(min_cost, token.cost);
    return (++count >= kMaxPredictiveTokens)
               ? DictionaryInterface::Callback::TRAVERSE_DONE
               : DictionaryInterface::Callback::TRAVERSE_CONTINUE;
  });
  if (exact) {
    dictionary_.LookupExact(key, request.options(), &callback);
  } else {
    dictionary_.LookupPredictive(key, request.options(), &callback);
  }
  return min_cost;
}

std::vector<composer::TypeCorrectedQuery> RomanTypoCorrector::Correct(
    const ConversionRequest& request) const {
  if (!IsEnabled(request)) {
    return {};
  }

  // The original key may still contain ASCII letters when the typo produced
  // invalid romaji (e.g. "kqisya" -> "kqいしゃ"). Such input is corrected too.
  const std::string original_key = request.composer().GetQueryForPrediction();
  if (original_key.empty()) {
    return {};
  }
  // The input is a valid word. Do not correct.
  if (HasExactKey(original_key)) {
    return {};
  }
  // The input is a prefix of a common word; the user is probably still typing
  // it correctly. Correcting here would add noise on almost every keystroke.
  const bool original_has_predictive =
      !ContainsAsciiAlpha(original_key) &&
      HasPredictiveKey(original_key, request);
  if (original_has_predictive &&
      GetMinCost(original_key, /*exact=*/false, request) <=
          kPrefixSkipMaxCost) {
    return {};
  }
  const size_t original_pending =
      TrailingAsciiLength(request.composer().GetStringForPreedit());

  const std::string raw = request.composer().GetRawString();
  // Typos the user has made before are more plausible.
  auto apply_habits = [this](std::vector<RomanTypoCandidate>* cands) {
    if (habit_store_ == nullptr) {
      return;
    }
    for (RomanTypoCandidate& c : *cands) {
      c.cost = std::max(0.0f, c.cost + habit_store_->CostAdjustment(c.edit));
    }
    std::stable_sort(cands->begin(), cands->end(), CandidateLess);
  };
  std::vector<RomanTypoCandidate> candidates =
      GenerateRomanTypoCandidates(raw);
  if (candidates.empty()) {
    return {};
  }
  apply_habits(&candidates);
  if (candidates.size() > kMaxCandidatesToCheck) {
    candidates.resize(kMaxCandidatesToCheck);
  }

  std::shared_ptr<const composer::Table> table;
  {
    absl::MutexLock lock(mutex_);
    table = table_manager_.GetTable(request.request(), request.config());
  }
  if (table == nullptr) {
    return {};
  }
  auto shared_request =
      std::make_shared<const commands::Request>(request.request());
  auto shared_config = std::make_shared<const config::Config>(request.config());

  absl::flat_hash_set<std::string> seen = {original_key};
  std::vector<Scored> accepted;
  auto check = [&](const RomanTypoCandidate& candidate, bool allow_predictive) {
    composer::Composer composer(table, shared_request, shared_config);
    composer.SetInputMode(transliteration::HIRAGANA);
    composer.InsertCharacter(candidate.romaji);
    // Reject candidates that leave a different amount of pending romaji,
    // e.g. "kaihsa" -> "kaihs" ("かいhs"), which would otherwise be trimmed
    // to the unrelated key "かい".
    if (TrailingAsciiLength(composer.GetStringForPreedit()) !=
        original_pending) {
      return;
    }
    std::string key = composer.GetQueryForPrediction();
    if (key.empty() || ContainsAsciiAlpha(key) ||
        Util::CharsLen(key) < kMinKeyLength - 1 || !seen.insert(key).second) {
      return;
    }
    if (HasExactKey(key)) {
      const int min_cost = GetMinCost(key, /*exact=*/true, request);
      accepted.push_back({std::move(key), candidate.cost,
                          candidate.cost + kFrequencyWeight * min_cost / 1000,
                          candidate.edit});
    } else if (allow_predictive && !original_has_predictive &&
               HasPredictiveKey(key, request)) {
      const float cost = candidate.cost + kPredictiveOnlyCost;
      const int min_cost = GetMinCost(key, /*exact=*/false, request);
      accepted.push_back({std::move(key), cost,
                          cost + kFrequencyWeight * min_cost / 1000,
                          candidate.edit});
    }
  };
  for (const RomanTypoCandidate& candidate : candidates) {
    check(candidate, /*allow_predictive=*/true);
  }
  if (accepted.empty() &&
      request.request_type() == ConversionRequest::PREDICTION &&
      raw.size() >= kTwoEditMinRawLength) {
    std::vector<RomanTypoCandidate> two_edit =
        GenerateTwoEditRomanTypoCandidates(raw, kTwoEditFirstK,
                                           kMaxTwoEditCandidatesToCheck,
                                           kTwoEditExtraCost);
    apply_habits(&two_edit);
    for (const RomanTypoCandidate& candidate : two_edit) {
      check(candidate, /*allow_predictive=*/false);
    }
  }

  std::stable_sort(accepted.begin(), accepted.end(),
                   [](const Scored& a, const Scored& b) {
                     return a.rank_score < b.rank_score;
                   });
  if (accepted.size() > kMaxQueries) {
    accepted.resize(kMaxQueries);
  }

  std::vector<composer::TypeCorrectedQuery> queries;
  queries.reserve(accepted.size());
  for (Scored& s : accepted) {
    if (habit_store_ != nullptr) {
      habit_store_->Remember(raw, s.key, s.edit);
    }
    composer::TypeCorrectedQuery query;
    query.correction = std::move(s.key);
    query.type = composer::TypeCorrectedQuery::CORRECTION;
    query.score = -s.cost;
    query.bias = -(kBiasBase + s.cost * kBiasPerCost);
    queries.push_back(std::move(query));
  }
  return queries;
}

}  // namespace prediction
}  // namespace mozc
