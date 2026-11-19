#ifndef MOZC_ZENZ_SCORER_LLAMA_HTTP_CLIENT_H_
#define MOZC_ZENZ_SCORER_LLAMA_HTTP_CLIENT_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mozc {
namespace zenz_scorer {

enum class LlamaHttpCompletionStatus {
  kOk,
  kError,
  kTimeout,
};

struct LlamaHttpCompletionRequest {
  int port = 0;
  std::string api_key;
  int n_predict = 64;
  uint32_t timeout_msec = 0;
  uint32_t max_output_chars = 0;
  std::string prompt;

  // Mozlumi: scoring. With a grammar, the stop list holds only newlines (the
  // forced part contains Zenz markers) and the generated text is returned
  // as is, without cutting it at a marker.
  std::string grammar;
  // When positive, the log-probability of each generated token is returned.
  int n_probs = 0;
};

struct LlamaHttpTokenLogprob {
  size_t bytes = 0;
  double logprob = 0.0;
};

struct LlamaHttpCompletionResponse {
  LlamaHttpCompletionStatus status = LlamaHttpCompletionStatus::kError;
  std::string value;
  std::string debug;
  // Filled when n_probs is positive, in the order of generation.
  std::vector<LlamaHttpTokenLogprob> token_logprobs;
};

// Sends one non-streaming completion request to a llama-server listening on
// 127.0.0.1.  The timeout is an absolute budget shared by connect, write, and
// read operations.
LlamaHttpCompletionResponse PostLlamaHttpCompletion(
    const LlamaHttpCompletionRequest& request);

// Exposed for testing: reads "completion_probabilities" of a /completion
// response (the format with "bytes" and "logprob" for each token).
bool ParseLlamaHttpTokenLogprobs(const std::string& json,
                                 std::vector<LlamaHttpTokenLogprob>* tokens);

}  // namespace zenz_scorer
}  // namespace mozc

#endif  // MOZC_ZENZ_SCORER_LLAMA_HTTP_CLIENT_H_
