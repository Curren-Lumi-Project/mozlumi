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

#include "engine/sentence_typo_corrector.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "base/strings/japanese.h"
#include "base/util.h"
#include "composer/composer.h"
#include "converter/candidate.h"
#include "converter/converter_interface.h"
#include "converter/segments.h"
#include "prediction/roman_typo_candidates.h"
#include "prediction/typo_habit_store.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"

namespace mozc {
namespace engine {
namespace {

constexpr int64_t kAsciiBadness = 100000;

bool ContainsLatin(absl::string_view s) {
  const std::string half = japanese::FullWidthAsciiToHalfWidthAscii(s);
  return std::any_of(half.begin(), half.end(), [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
  });
}

// How unlikely the conversion of a segment looks: its cost per character,
// or a large value when romaji is left in it.
int64_t SegmentBadness(const Segment& segment) {
  if (ContainsLatin(segment.key())) {
    return kAsciiBadness;
  }
  if (segment.candidates_size() == 0) {
    return 0;
  }
  const size_t len = std::max<size_t>(1, Util::CharsLen(segment.key()));
  return segment.candidate(0).cost / static_cast<int64_t>(len);
}

}  // namespace

int64_t SentenceCost(const Segments& segments) {
  int64_t cost = 0;
  for (const Segment& segment : segments.conversion_segments()) {
    if (segment.candidates_size() > 0) {
      cost += segment.candidate(0).cost;
    }
  }
  return cost;
}

std::string SentenceValue(const Segments& segments) {
  std::string value;
  for (const Segment& segment : segments.conversion_segments()) {
    if (segment.candidates_size() > 0) {
      value.append(segment.candidate(0).value);
    }
  }
  return value;
}

SentenceTypoAnalysis AnalyzeSentenceTypos(
    const ConverterInterface& converter, const composer::Composer& composer,
    const Segments& original, const commands::Request& request,
    const config::Config& config, const commands::Context& context,
    bool use_history, const SentenceTypoOptions& options,
    const prediction::TypoHabitStore* habits) {
  SentenceTypoAnalysis analysis;
  analysis.value = SentenceValue(original);
  analysis.sentence_cost = SentenceCost(original);

  const std::string raw = composer.GetRawString();
  if (raw.size() < options.min_raw_length ||
      raw.size() > options.max_raw_length ||
      original.conversion_segments_size() == 0) {
    return analysis;
  }

  // Rank the segments of the typed text by how suspicious they look, and
  // remember where each one starts in the reading.
  std::vector<std::pair<int64_t, size_t>> badness;  // (badness, index)
  std::vector<size_t> segment_end;                  // in characters
  std::u32string original_key;
  for (const Segment& segment : original.conversion_segments()) {
    badness.emplace_back(SegmentBadness(segment), segment_end.size());
    original_key += Util::Utf8ToUtf32(segment.key());
    segment_end.push_back(original_key.size());
  }
  std::stable_sort(badness.begin(), badness.end(),
                   [](const auto& a, const auto& b) { return a.first > b.first; });
  analysis.worst_badness = badness.front().first;
  std::vector<size_t> rank_of(badness.size());
  for (size_t r = 0; r < badness.size(); ++r) {
    rank_of[badness[r].second] = r;
  }
  auto rank_at = [&](size_t pos) {
    for (size_t i = 0; i < segment_end.size(); ++i) {
      if (pos < segment_end[i]) {
        return rank_of[i];
      }
    }
    return rank_of.back();
  };

  std::vector<prediction::RomanTypoCandidate> generated =
      prediction::GenerateRomanTypoCandidates(raw, options.min_raw_length,
                                              options.max_raw_length);
  analysis.generated = generated.size();
  if (habits != nullptr) {
    for (prediction::RomanTypoCandidate& c : generated) {
      c.cost = std::max(0.0f, c.cost + habits->CostAdjustment(c.edit));
    }
  }

  // Keep the fixes that become kana only, ordered by the suspiciousness of
  // the segment they change, then by how plausible the typo is.
  struct Pending {
    size_t rank;
    prediction::RomanTypoCandidate typo;
    composer::Composer composer;
    std::string key;
  };
  std::vector<Pending> pending;
  std::vector<std::u32string> seen = {original_key};
  for (const prediction::RomanTypoCandidate& typo : generated) {
    composer::Composer fixed = composer;
    fixed.EditErase();
    fixed.SetInputMode(composer.GetInputMode());
    fixed.InsertCharacter(typo.romaji);
    if (ContainsLatin(fixed.GetStringForPreedit())) {
      continue;
    }
    std::string key = fixed.GetQueryForConversion();
    const std::u32string key32 = Util::Utf8ToUtf32(key);
    if (key32.empty() ||
        std::find(seen.begin(), seen.end(), key32) != seen.end()) {
      continue;
    }
    seen.push_back(key32);
    size_t prefix = 0;
    while (prefix < key32.size() && prefix < original_key.size() &&
           key32[prefix] == original_key[prefix]) {
      ++prefix;
    }
    pending.push_back({rank_at(prefix), typo, std::move(fixed), std::move(key)});
  }
  std::stable_sort(pending.begin(), pending.end(),
                   [](const Pending& a, const Pending& b) {
                     if (a.rank != b.rank) return a.rank < b.rank;
                     return a.typo.cost < b.typo.cost;
                   });
  if (pending.size() > options.max_conversions) {
    pending.resize(options.max_conversions);
  }

  for (const Pending& p : pending) {
    ConversionRequest::Options request_options;
    request_options.request_type = ConversionRequest::CONVERSION;
    request_options.enable_user_history_for_conversion = use_history;
    const ConversionRequest fixed_request =
        ConversionRequestBuilder()
            .SetComposer(p.composer)
            .SetRequestView(request)
            .SetContextView(context)
            .SetConfigView(config)
            .SetOptions(std::move(request_options))
            .Build();
    Segments segments;
    if (!converter.StartConversion(fixed_request, &segments) ||
        segments.conversion_segments_size() == 0) {
      continue;
    }
    SentenceTypoCandidate candidate;
    candidate.romaji = p.typo.romaji;
    candidate.key = p.key;
    candidate.value = SentenceValue(segments);
    candidate.edit = p.typo.edit;
    candidate.typo_cost = p.typo.cost;
    candidate.sentence_cost = SentenceCost(segments);
    candidate.segment_rank = p.rank;
    if (!candidate.value.empty()) {
      analysis.candidates.push_back(std::move(candidate));
    }
  }
  return analysis;
}

std::optional<size_t> ChooseSentenceTypoFix(
    const SentenceTypoAnalysis& analysis,
    const SentenceTypoDecision& decision) {
  if (analysis.worst_badness < decision.min_worst_badness) {
    return std::nullopt;
  }
  std::optional<size_t> best;
  double best_score = 0;
  for (size_t i = 0; i < analysis.candidates.size(); ++i) {
    const SentenceTypoCandidate& c = analysis.candidates[i];
    if (c.value == analysis.value) {
      continue;
    }
    const double score = static_cast<double>(c.sentence_cost) +
                         decision.cost_per_typo * c.typo_cost;
    if (!best.has_value() || score < best_score) {
      best = i;
      best_score = score;
    }
  }
  if (best.has_value() &&
      best_score + static_cast<double>(decision.margin) <
          static_cast<double>(analysis.sentence_cost)) {
    return best;
  }
  return std::nullopt;
}

std::vector<size_t> RankSentenceTypoOffers(
    const SentenceTypoAnalysis& analysis, const SentenceTypoDecision& decision,
    size_t max_offers) {
  std::vector<std::pair<double, size_t>> scored;
  if (analysis.worst_badness < decision.min_worst_badness) {
    return {};
  }
  for (size_t i = 0; i < analysis.candidates.size(); ++i) {
    const SentenceTypoCandidate& c = analysis.candidates[i];
    if (c.value == analysis.value) {
      continue;
    }
    const double score = static_cast<double>(c.sentence_cost) +
                         decision.cost_per_typo * c.typo_cost;
    if (score + static_cast<double>(decision.margin) <
        static_cast<double>(analysis.sentence_cost)) {
      scored.emplace_back(score, i);
    }
  }
  std::stable_sort(scored.begin(), scored.end(),
                   [](const auto& a, const auto& b) { return a.first < b.first; });
  std::vector<size_t> result;
  std::vector<absl::string_view> values;
  for (const auto& [score, index] : scored) {
    const std::string& value = analysis.candidates[index].value;
    if (std::find(values.begin(), values.end(), value) != values.end()) {
      continue;
    }
    values.push_back(value);
    result.push_back(index);
    if (result.size() >= max_offers) {
      break;
    }
  }
  return result;
}

}  // namespace engine
}  // namespace mozc
