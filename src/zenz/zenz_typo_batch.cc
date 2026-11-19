#include "zenz/zenz_typo_batch.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace mozc {
namespace zenz {
namespace {

std::vector<std::string_view> Split(std::string_view text,
                                    std::string_view separator) {
  std::vector<std::string_view> parts;
  size_t start = 0;
  while (true) {
    const size_t pos = text.find(separator, start);
    if (pos == std::string_view::npos) {
      parts.push_back(text.substr(start));
      return parts;
    }
    parts.push_back(text.substr(start, pos - start));
    start = pos + separator.size();
  }
}

bool ParseDouble(std::string_view text, double* value) {
  if (text.empty() || text.size() > 32) {
    return false;
  }
  const std::string copy(text);
  char* end = nullptr;
  *value = std::strtod(copy.c_str(), &end);
  return end != nullptr && *end == '\0';
}

}  // namespace

std::string EncodeZenzTypoBatchPrompt(std::string_view prefix,
                                      const std::vector<std::string>& forced) {
  std::string prompt(kZenzTypoBatchMarker);
  prompt.append(prefix);
  for (const std::string& f : forced) {
    prompt.append(kZenzTypoBatchSeparator);
    prompt.append(f);
  }
  return prompt;
}

bool DecodeZenzTypoBatchPrompt(std::string_view prompt, std::string* prefix,
                               std::vector<std::string>* forced) {
  if (prompt.substr(0, kZenzTypoBatchMarker.size()) != kZenzTypoBatchMarker) {
    return false;
  }
  const std::vector<std::string_view> parts =
      Split(prompt.substr(kZenzTypoBatchMarker.size()),
            kZenzTypoBatchSeparator);
  if (parts.size() < 2) {
    return false;
  }
  prefix->assign(parts[0]);
  forced->clear();
  for (size_t i = 1; i < parts.size(); ++i) {
    if (parts[i].empty()) {
      return false;
    }
    forced->emplace_back(parts[i]);
  }
  return true;
}

std::string EncodeZenzTypoBatchResponse(
    const std::vector<ZenzTypoBatchResult>& results) {
  std::string out;
  for (size_t i = 0; i < results.size(); ++i) {
    const ZenzTypoBatchResult& r = results[i];
    if (i > 0) {
      out.push_back('\n');
    }
    char numbers[96];
    std::snprintf(numbers, sizeof(numbers), "%d\t%.4f\t%.4f\t", r.ok ? 1 : 0,
                  r.forced_logprob, r.value_logprob);
    out.append(numbers);
    // A value never contains a newline or a tab (the grammar excludes them),
    // but be safe.
    for (const char c : r.value) {
      out.push_back(c == '\n' || c == '\t' || c == '\r' ? ' ' : c);
    }
  }
  return out;
}

bool DecodeZenzTypoBatchResponse(std::string_view value,
                                 std::vector<ZenzTypoBatchResult>* results) {
  results->clear();
  if (value.empty()) {
    return false;
  }
  for (std::string_view line : Split(value, "\n")) {
    const std::vector<std::string_view> f = Split(line, "\t");
    if (f.size() != 4 || (f[0] != "0" && f[0] != "1")) {
      return false;
    }
    ZenzTypoBatchResult r;
    r.ok = f[0] == "1";
    if (!ParseDouble(f[1], &r.forced_logprob) ||
        !ParseDouble(f[2], &r.value_logprob)) {
      return false;
    }
    r.value.assign(f[3]);
    results->push_back(std::move(r));
  }
  return true;
}

}  // namespace zenz
}  // namespace mozc
