// Copyright 2026
// Licensed under the same terms as Mozc.

#include "zenz_scorer/typo_batch_runner.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "zenz/zenz_typo_batch.h"

namespace mozc {
namespace zenz_scorer {
namespace {

int Utf8Chars(std::string_view text) {
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

bool IsJsonSpace(char c) {
  return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

}  // namespace

std::string ZenzBatchGrammar(std::string_view forced) {
  std::string literal;
  literal.reserve(forced.size());
  for (const char c : forced) {
    if (c == '\\' || c == '"') {
      literal.push_back('\\');
    }
    literal.push_back(c);
  }
  return "root ::= \"" + literal + "\" [^\\n\\r\\uee00-\\uee0f]*";
}

std::string RunZenzTypoBatch(const std::string& prefix,
                             const std::vector<std::string>& jobs,
                             uint32_t timeout_msec, int base_n_predict,
                             const ZenzBatchCompletion& complete,
                             size_t* succeeded) {
  using Clock = std::chrono::steady_clock;
  // Leaves a little time for the response to reach the client, whose deadline
  // is the same timeout.
  const uint32_t budget_msec =
      timeout_msec > 200 ? timeout_msec - 100 : timeout_msec;
  const Clock::time_point deadline =
      Clock::now() + std::chrono::milliseconds(budget_msec);
  std::vector<::mozc::zenz::ZenzTypoBatchResult> results;
  size_t ok_count = 0;
  for (const std::string& job : jobs) {
    std::string prompt;
    std::string forced;
    ::mozc::zenz::SplitZenzTypoBatchJob(prefix, job, &prompt, &forced);
    ::mozc::zenz::ZenzTypoBatchResult result;
    const int64_t remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline -
                                                              Clock::now())
            .count();
    if (remaining < 50) {
      results.push_back(std::move(result));
      continue;
    }
    ZenzBatchCompletionRequest request;
    request.prompt = std::move(prompt);
    request.grammar = ZenzBatchGrammar(forced);
    request.n_predict = std::min(256, base_n_predict + Utf8Chars(forced));
    request.timeout_msec = static_cast<uint32_t>(remaining);
    const ZenzBatchCompletionResult completion = complete(request);
    if (completion.ok &&
        completion.content.compare(0, forced.size(), forced) == 0) {
      size_t consumed = 0;
      for (const ZenzBatchTokenLogprob& token : completion.tokens) {
        if (consumed < forced.size()) {
          result.forced_logprob += token.logprob;
        } else if (consumed + token.bytes <= completion.content.size()) {
          result.value_logprob += token.logprob;
        }
        consumed += token.bytes;
      }
      if (consumed >= forced.size()) {
        result.ok = true;
        result.value = TrimAscii(completion.content.substr(forced.size()));
        ++ok_count;
      }
    }
    results.push_back(std::move(result));
  }
  if (succeeded != nullptr) {
    *succeeded = ok_count;
  }
  return ::mozc::zenz::EncodeZenzTypoBatchResponse(results);
}

// The same parser as ParseLlamaHttpTokenLogprobs (llama_http_client.cc, macOS
// only).
bool ParseZenzCompletionTokenLogprobs(
    const std::string& json, std::vector<ZenzBatchTokenLogprob>* tokens) {
  tokens->clear();
  size_t p = json.find("\"completion_probabilities\"");
  if (p == std::string::npos) {
    return false;
  }
  p = json.find(':', p);
  if (p == std::string::npos) {
    return false;
  }
  ++p;
  while (p < json.size() && IsJsonSpace(json[p])) {
    ++p;
  }
  if (p >= json.size() || json[p] != '[') {
    return false;
  }

  // depth 1: inside the array; depth 2: inside one token object.
  int depth = 0;
  ZenzBatchTokenLogprob current;
  bool has_logprob = false;
  auto skip_string = [&](size_t* pos, std::string* text) {
    // *pos is at the opening quote.
    ++*pos;
    while (*pos < json.size()) {
      const char ch = json[(*pos)++];
      if (ch == '\\') {
        if (text) text->push_back(ch);
        if (*pos < json.size()) {
          if (text) text->push_back(json[*pos]);
          ++*pos;
        }
        continue;
      }
      if (ch == '"') {
        return true;
      }
      if (text) text->push_back(ch);
    }
    return false;
  };
  while (p < json.size()) {
    const char ch = json[p];
    if (ch == '"') {
      std::string text;
      if (!skip_string(&p, depth == 2 ? &text : nullptr)) {
        return false;
      }
      if (depth != 2) {
        continue;
      }
      size_t q = p;
      while (q < json.size() && IsJsonSpace(json[q])) {
        ++q;
      }
      if (q >= json.size() || json[q] != ':') {
        continue;  // A string value, not a key.
      }
      ++q;
      while (q < json.size() && IsJsonSpace(json[q])) {
        ++q;
      }
      if (text == "logprob") {
        const char* begin = json.c_str() + q;
        char* end = nullptr;
        const double value = std::strtod(begin, &end);
        if (end == begin) {
          return false;
        }
        current.logprob = value;
        has_logprob = true;
        p = q + static_cast<size_t>(end - begin);
        continue;
      }
      if (text == "bytes") {
        if (q >= json.size() || json[q] != '[') {
          return false;
        }
        const size_t close = json.find(']', q);
        if (close == std::string::npos) {
          return false;
        }
        size_t count = 0;
        bool in_number = false;
        for (size_t i = q + 1; i < close; ++i) {
          const bool digit = json[i] >= '0' && json[i] <= '9';
          if (digit && !in_number) {
            ++count;
          }
          in_number = digit;
        }
        current.bytes = count;
        p = close + 1;
        continue;
      }
      p = q;
      continue;
    }
    if (ch == '[' || ch == '{') {
      ++depth;
      if (depth == 2 && ch == '{') {
        current = ZenzBatchTokenLogprob();
        has_logprob = false;
      }
    } else if (ch == ']' || ch == '}') {
      if (depth == 2 && ch == '}') {
        if (!has_logprob) {
          return false;
        }
        tokens->push_back(current);
      }
      --depth;
      if (depth == 0) {
        return true;
      }
    }
    ++p;
  }
  return false;
}

}  // namespace zenz_scorer
}  // namespace mozc
