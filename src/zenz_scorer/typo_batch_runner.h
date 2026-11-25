// Copyright 2026
// Licensed under the same terms as Mozc.

#ifndef MOZC_ZENZ_SCORER_TYPO_BATCH_RUNNER_H_
#define MOZC_ZENZ_SCORER_TYPO_BATCH_RUNNER_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace mozc {
namespace zenz_scorer {

// Mozlumi: runs the scoring jobs of a typo batch (zenz/zenz_typo_batch.h) for
// the Windows scorer (main.cc), the same way as the macOS scorer
// (llama_backend.cc RunTypoBatch). Without it, the Windows scorer sent a batch
// to llama-server as a normal prompt, and the AI rank, its checks and the
// Zenz typo choice never worked on Windows (found on 2026-11-25).

struct ZenzBatchTokenLogprob {
  size_t bytes = 0;
  double logprob = 0.0;
};

// One llama-server completion of a job: after `prompt`, the text forced by
// `grammar`, then what Zenz writes after it.
struct ZenzBatchCompletionRequest {
  std::string prompt;
  std::string grammar;
  int n_predict = 0;
  uint32_t timeout_msec = 0;
};

struct ZenzBatchCompletionResult {
  bool ok = false;
  // The generated text: the forced part, then the rest.
  std::string content;
  // The log-probability of each generated token, in order.
  std::vector<ZenzBatchTokenLogprob> tokens;
};

using ZenzBatchCompletion =
    std::function<ZenzBatchCompletionResult(const ZenzBatchCompletionRequest&)>;

// Runs `jobs` (decoded with DecodeZenzTypoBatchPrompt; a job may have its own
// prefix) one by one within `timeout_msec`; a job is skipped when less than
// 50 ms is left. Returns the response value (EncodeZenzTypoBatchResponse).
// `succeeded` (may be nullptr) gets the number of jobs that succeeded.
std::string RunZenzTypoBatch(const std::string& prefix,
                             const std::vector<std::string>& jobs,
                             uint32_t timeout_msec, int base_n_predict,
                             const ZenzBatchCompletion& complete,
                             size_t* succeeded);

// The llama-server grammar of a job: `forced`, then anything but newlines and
// the Zenz markers U+EE00..U+EE0F.
std::string ZenzBatchGrammar(std::string_view forced);

// The log-probability and the byte length of each token in
// "completion_probabilities" of a llama-server response (n_probs > 0).
bool ParseZenzCompletionTokenLogprobs(
    const std::string& json, std::vector<ZenzBatchTokenLogprob>* tokens);

}  // namespace zenz_scorer
}  // namespace mozc

#endif  // MOZC_ZENZ_SCORER_TYPO_BATCH_RUNNER_H_
