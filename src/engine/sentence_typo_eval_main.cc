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

// Mozlumi: evaluation tool for the sentence-level typo corrector.
//
// Reads "id \t kind \t romaji \t clean_romaji" lines from stdin and prints,
// for each, the conversion of the typed text and every converted fix with its
// costs, so that the decision parameters can be tuned offline.
//
//   sentence_typo_eval_main --engine_data_path=.../mozc.data < cases.tsv

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/strings/str_split.h"
#include "base/init_mozc.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "data_manager/data_manager.h"
#include "engine/engine.h"
#include "engine/sentence_typo_corrector.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "transliteration/transliteration.h"

ABSL_FLAG(std::string, engine_data_path, "", "path to mozc.data");
ABSL_FLAG(int32_t, max_conversions, 64, "fixes converted per input");
ABSL_FLAG(int32_t, second_round_seeds, 0, "fixes corrected once more");
ABSL_FLAG(int32_t, second_round_conversions, 16,
          "fixes converted per seed in the second round");
ABSL_FLAG(int32_t, conversion_candidates, 20,
          "candidates per segment when converting fixes (0: the usual 200)");
ABSL_FLAG(bool, skip_slow_rewriters, true,
          "skip the slow rewriters when converting fixes");

namespace {

std::string Clean(std::string s) {
  for (char& c : s) {
    if (c == '\t' || c == '\n') c = ' ';
  }
  return s;
}

}  // namespace

int main(int argc, char** argv) {
  mozc::InitMozc(argv[0], &argc, &argv);
  auto data_manager =
      mozc::DataManager::CreateFromFile(absl::GetFlag(FLAGS_engine_data_path));
  CHECK_OK(data_manager);
  std::unique_ptr<mozc::Engine> engine =
      mozc::Engine::CreateEngine(*std::move(data_manager)).value();
  const auto converter = engine->GetConverter();

  const mozc::config::Config config =
      mozc::config::ConfigHandler::DefaultConfig();
  const mozc::commands::Request request;
  const mozc::commands::Context context;
  auto table = std::make_shared<mozc::composer::Table>();
  table->InitializeWithRequestAndConfig(request, config);

  mozc::engine::SentenceTypoOptions options;
  options.max_conversions = absl::GetFlag(FLAGS_max_conversions);
  // Converts everything so that the threshold can be tuned offline.
  options.min_worst_badness = 0;
  options.second_round_seeds = absl::GetFlag(FLAGS_second_round_seeds);
  options.second_round_conversions =
      absl::GetFlag(FLAGS_second_round_conversions);
  options.conversion_candidates = absl::GetFlag(FLAGS_conversion_candidates);
  options.skip_slow_rewriters = absl::GetFlag(FLAGS_skip_slow_rewriters);

  auto convert = [&](const mozc::composer::Composer& composer,
                     mozc::Segments* segments) {
    mozc::ConversionRequest::Options o;
    o.request_type = mozc::ConversionRequest::CONVERSION;
    const mozc::ConversionRequest r = mozc::ConversionRequestBuilder()
                                          .SetComposer(composer)
                                          .SetRequestView(request)
                                          .SetContextView(context)
                                          .SetConfigView(config)
                                          .SetOptions(std::move(o))
                                          .Build();
    return converter->StartConversion(r, segments);
  };

  std::string line;
  while (std::getline(std::cin, line)) {
    const std::vector<std::string> f = absl::StrSplit(line, '\t');
    if (f.size() < 4) continue;
    auto make = [&](const std::string& romaji) {
      mozc::composer::Composer c(table, request, config);
      c.SetInputMode(mozc::transliteration::HIRAGANA);
      c.InsertCharacter(romaji);
      return c;
    };
    const mozc::composer::Composer typed = make(f[2]);
    const mozc::composer::Composer clean = make(f[3]);
    mozc::Segments clean_segments, typed_segments;
    if (!convert(clean, &clean_segments) || !convert(typed, &typed_segments)) {
      std::cout << "E\t" << f[0] << "\t" << f[1] << "\n";
      continue;
    }
    const auto start = std::chrono::steady_clock::now();
    const mozc::engine::SentenceTypoAnalysis analysis =
        mozc::engine::AnalyzeSentenceTypos(*converter, typed, typed_segments,
                                           request, config, context,
                                           /*use_history=*/false, options,
                                           nullptr);
    const double ms = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - start)
                          .count();
    // S id kind clean_value typed_value typed_cost worst_badness generated
    //   converted ms
    std::cout << "S\t" << f[0] << "\t" << f[1] << "\t"
              << Clean(mozc::engine::SentenceValue(clean_segments)) << "\t"
              << Clean(analysis.value) << "\t" << analysis.sentence_cost
              << "\t" << analysis.worst_badness << "\t" << analysis.generated
              << "\t" << analysis.candidates.size() << "\t" << ms << "\n";
    // C order value sentence_cost typo_cost segment_rank edit romaji
    for (size_t i = 0; i < analysis.candidates.size(); ++i) {
      const auto& c = analysis.candidates[i];
      std::cout << "C\t" << i << "\t" << Clean(c.value) << "\t"
                << c.sentence_cost << "\t" << c.typo_cost << "\t"
                << c.segment_rank << "\t" << c.edit << "\t" << c.romaji
                << "\n";
    }
  }
  return 0;
}
