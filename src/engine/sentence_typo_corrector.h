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

// Mozlumi: sentence-level typo correction (誤入力補正).
//
// The word-level corrector only finds a fix when the whole reading is one
// dictionary word. A typo inside a phrase ("よくやる", "かけそばをたべる")
// is found here instead: one-key fixes of the typed romaji are converted as
// whole sentences, and a fix is chosen when its sentence is much more
// natural (lower conversion cost) than the typed text, after a penalty for
// the typo itself.

#ifndef MOZC_ENGINE_SENTENCE_TYPO_CORRECTOR_H_
#define MOZC_ENGINE_SENTENCE_TYPO_CORRECTOR_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "composer/composer.h"
#include "converter/converter_interface.h"
#include "converter/segments.h"
#include "prediction/typo_habit_store.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"

namespace mozc {
namespace engine {

struct SentenceTypoOptions {
  // Romaji shorter than this is left to the word-level corrector.
  size_t min_raw_length = 6;
  // At most this many fixes are converted (each is one conversion).
  size_t max_conversions = 48;
  // Romaji longer than this is not corrected.
  size_t max_raw_length = 64;
  // Nothing is converted unless a segment of the typed text is at least this
  // bad (cost per character, or romaji left). Correctly typed phrases are
  // mostly below it: in the evaluation, 2000 skipped about a quarter of them
  // without losing any fix.
  int64_t min_worst_badness = 2000;
  // Two typos: the most plausible one-key fixes (scored with
  // seed_cost_per_typo) are corrected once more, converting at most
  // second_round_conversions fixes for each. 0 seeds disables it.
  size_t second_round_seeds = 2;
  size_t second_round_conversions = 16;
  float seed_cost_per_typo = 2000.0f;
  // Only the top candidate of each segment is used, so the fixes are
  // converted with fewer candidates per segment (0: the usual 200) and
  // without the slow rewriters (user boundary history, transliteration).
  int conversion_candidates = 20;
  bool skip_slow_rewriters = true;
};

struct SentenceTypoCandidate {
  std::string romaji;
  std::string key;
  std::string value;
  std::string edit;
  float typo_cost = 0.0f;
  int64_t sentence_cost = 0;
  // 0 for the most suspicious segment of the typed text, 1 for the next.
  size_t segment_rank = 0;
};

struct SentenceTypoAnalysis {
  std::string value;
  int64_t sentence_cost = 0;
  // The largest "badness" among the segments of the typed text.
  int64_t worst_badness = 0;
  size_t generated = 0;
  std::vector<SentenceTypoCandidate> candidates;
};

struct SentenceTypoDecision {
  // Cost added per unit of typo cost (about 1.0 for one key).
  float cost_per_typo = 2000.0f;
  // The fix must be better than the typed text by more than this.
  int64_t margin = 0;
  // The typed text is not corrected unless one of its segments is at least
  // this bad.
  int64_t min_worst_badness = 0;
  // Added to the score of a fix of two typos.
  int64_t two_edit_penalty = 0;
};

// Sum of the costs of the top candidates of the conversion segments.
int64_t SentenceCost(const Segments& segments);
// Concatenated values of the top candidates of the conversion segments.
std::string SentenceValue(const Segments& segments);

// Converts one-key fixes of the typed romaji. `original` is the conversion of
// the typed text. `habits` may be null.
SentenceTypoAnalysis AnalyzeSentenceTypos(
    const ConverterInterface& converter, const composer::Composer& composer,
    const Segments& original, const commands::Request& request,
    const config::Config& config, const commands::Context& context,
    bool use_history, const SentenceTypoOptions& options,
    const prediction::TypoHabitStore* habits);

// Returns the index of the chosen candidate, if any.
std::optional<size_t> ChooseSentenceTypoFix(
    const SentenceTypoAnalysis& analysis,
    const SentenceTypoDecision& decision);

// Returns up to `max_offers` candidates (indices, best first, distinct
// values) that pass the decision.
std::vector<size_t> RankSentenceTypoOffers(
    const SentenceTypoAnalysis& analysis, const SentenceTypoDecision& decision,
    size_t max_offers);

}  // namespace engine
}  // namespace mozc

#endif  // MOZC_ENGINE_SENTENCE_TYPO_CORRECTOR_H_
