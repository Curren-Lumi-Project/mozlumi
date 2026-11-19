#include "zenz_scorer/llama_backend.h"

#if defined(_WIN32)
#error "llama_backend.cc must not be built on Windows"
#endif

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "zenz/zenz_typo_batch.h"
#include "zenz/zenz_wire_protocol.h"
#include "zenz_scorer/llama_http_client.h"

namespace mozc {
namespace zenz_scorer {
namespace {

PosixZenzResponse ConvertResponse(
    LlamaHttpCompletionResponse completion) {
  PosixZenzResponse response;
  response.value = std::move(completion.value);
  response.debug = completion.debug.empty() ? "llama_http_unknown"
                                            : std::move(completion.debug);

  switch (completion.status) {
    case LlamaHttpCompletionStatus::kOk:
      response.status = ::mozc::zenz::kZenzWireStatusOk;
      break;
    case LlamaHttpCompletionStatus::kTimeout:
      response.status = ::mozc::zenz::kZenzWireStatusTimeout;
      response.value.clear();
      break;
    case LlamaHttpCompletionStatus::kError:
      response.status = ::mozc::zenz::kZenzWireStatusError;
      response.value.clear();
      break;
  }
  return response;
}

std::string EscapeGbnfLiteral(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  for (const char c : text) {
    if (c == '\\' || c == '"') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  return out;
}

int Utf8Chars(const std::string& text) {
  int count = 0;
  for (const char c : text) {
    if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) {
      ++count;
    }
  }
  return count;
}

std::string TrimAscii(std::string text) {
  const auto space = [](char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
  };
  while (!text.empty() && space(text.back())) text.pop_back();
  size_t start = 0;
  while (start < text.size() && space(text[start])) ++start;
  return text.substr(start);
}

// Mozlumi: runs the scoring jobs of a typo batch one by one within the time
// budget of the request. A job that fails or runs out of time is returned
// with ok = false.
PosixZenzResponse RunTypoBatch(const LlamaBackendOptions& options,
                               const PosixZenzRequest& request,
                               const std::string& prefix,
                               const std::vector<std::string>& forced) {
  using Clock = std::chrono::steady_clock;
  // Leaves a little time for the response to reach the client, whose socket
  // deadline is the same timeout.
  const uint32_t budget_msec = request.timeout_msec > 200
                                   ? request.timeout_msec - 100
                                   : request.timeout_msec;
  const Clock::time_point deadline =
      Clock::now() + std::chrono::milliseconds(budget_msec);
  std::vector<::mozc::zenz::ZenzTypoBatchResult> results;
  size_t succeeded = 0;
  for (const std::string& f : forced) {
    ::mozc::zenz::ZenzTypoBatchResult result;
    const int64_t remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline -
                                                              Clock::now())
            .count();
    if (remaining < 50) {
      results.push_back(std::move(result));
      continue;
    }
    LlamaHttpCompletionRequest completion_request;
    completion_request.port = options.port;
    completion_request.api_key = options.api_key;
    completion_request.n_predict =
        std::min(256, options.n_predict + Utf8Chars(f));
    completion_request.timeout_msec = static_cast<uint32_t>(remaining);
    completion_request.max_output_chars = 256;
    completion_request.prompt = prefix;
    // The forced part, then the conversion (anything but newlines and the
    // Zenz markers U+EE00..U+EE0F).
    completion_request.grammar = "root ::= \"" + EscapeGbnfLiteral(f) +
                                 "\" [^\\n\\r\\uee00-\\uee0f]*";
    completion_request.n_probs = 1;
    const LlamaHttpCompletionResponse completion =
        PostLlamaHttpCompletion(completion_request);
    if (completion.status == LlamaHttpCompletionStatus::kOk &&
        completion.value.compare(0, f.size(), f) == 0) {
      size_t consumed = 0;
      for (const LlamaHttpTokenLogprob& token : completion.token_logprobs) {
        if (consumed < f.size()) {
          result.forced_logprob += token.logprob;
        } else if (consumed + token.bytes <= completion.value.size()) {
          result.value_logprob += token.logprob;
        }
        consumed += token.bytes;
      }
      if (consumed >= f.size()) {
        result.ok = true;
        result.value = TrimAscii(completion.value.substr(f.size()));
        ++succeeded;
      }
    }
    results.push_back(std::move(result));
  }
  PosixZenzResponse response;
  response.status = ::mozc::zenz::kZenzWireStatusOk;
  response.value = ::mozc::zenz::EncodeZenzTypoBatchResponse(results);
  response.debug = "typo_batch_" + std::to_string(succeeded) + "_of_" +
                   std::to_string(forced.size());
  return response;
}

}  // namespace

PosixZenzBackend CreateLlamaHttpBackend(LlamaBackendOptions options) {
  return [options = std::move(options)](
             const PosixZenzRequest& request) -> PosixZenzResponse {
    std::string batch_prefix;
    std::vector<std::string> batch_forced;
    if (::mozc::zenz::DecodeZenzTypoBatchPrompt(request.prompt, &batch_prefix,
                                                &batch_forced)) {
      return RunTypoBatch(options, request, batch_prefix, batch_forced);
    }
    LlamaHttpCompletionRequest completion_request;
    completion_request.port = options.port;
    completion_request.api_key = options.api_key;
    completion_request.n_predict = options.n_predict;
    completion_request.timeout_msec = request.timeout_msec;
    completion_request.max_output_chars = request.max_output_chars;
    completion_request.prompt = request.prompt;
    return ConvertResponse(
        PostLlamaHttpCompletion(completion_request));
  };
}

}  // namespace zenz_scorer
}  // namespace mozc
