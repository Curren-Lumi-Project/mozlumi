// Copyright 2026 Curren Lumi Project.
// Licensed under the same terms as Mozc.

#include "prediction/typo_self_fix_store.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/file_util.h"
#include "base/system_util.h"

namespace mozc {
namespace prediction {
namespace {

constexpr char kFileName[] = "typo_self_fixes.tsv";

bool IsRomaji(std::string_view s) {
  return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) {
    return (c >= 'a' && c <= 'z') || c == '-';
  });
}

}  // namespace

TypoSelfFixStore::TypoSelfFixStore(std::string path) : path_(std::move(path)) {
  std::lock_guard<std::mutex> lock(mutex_);
  LoadLocked();
}

TypoSelfFixStore* TypoSelfFixStore::Get() {
  static TypoSelfFixStore* const store = new TypoSelfFixStore(
      FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(), kFileName));
  return store;
}

std::optional<std::pair<std::string, std::string>>
TypoSelfFixStore::ExtractPattern(std::string_view typed,
                                 std::string_view committed) {
  if (!IsRomaji(typed) || !IsRomaji(committed) || typed == committed) {
    return std::nullopt;
  }
  size_t prefix = 0;
  while (prefix < typed.size() && prefix < committed.size() &&
         typed[prefix] == committed[prefix]) {
    ++prefix;
  }
  size_t suffix = 0;
  while (suffix < typed.size() - prefix && suffix < committed.size() - prefix &&
         typed[typed.size() - 1 - suffix] ==
             committed[committed.size() - 1 - suffix]) {
    ++suffix;
  }
  const size_t typed_edit = typed.size() - prefix - suffix;
  const size_t committed_edit = committed.size() - prefix - suffix;
  if (std::max(typed_edit, committed_edit) > kMaxEditChars) {
    return std::nullopt;
  }
  const size_t begin = prefix > kContextChars ? prefix - kContextChars : 0;
  const size_t after = std::min(suffix, kContextChars);
  std::string wrong(
      typed.substr(begin, typed.size() - suffix + after - begin));
  std::string right(
      committed.substr(begin, committed.size() - suffix + after - begin));
  if (wrong.size() < kMinPatternChars || right.size() < kMinPatternChars) {
    return std::nullopt;
  }
  return std::make_pair(std::move(wrong), std::move(right));
}

bool TypoSelfFixStore::Learn(std::string_view typed,
                             std::string_view committed) {
  const auto pattern = ExtractPattern(typed, committed);
  if (!pattern.has_value()) {
    return false;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = fixes_.find(pattern->first);
  if (it != fixes_.end() && it->second.first == pattern->second) {
    it->second.second = std::min(kMaxCount, it->second.second + 1);
  } else {
    if (it == fixes_.end() && fixes_.size() >= kMaxPatterns) {
      // Forget the least used one.
      auto least = std::min_element(
          fixes_.begin(), fixes_.end(), [](const auto& a, const auto& b) {
            return a.second.second < b.second.second;
          });
      fixes_.erase(least);
    }
    fixes_[pattern->first] = {pattern->second, 1};
  }
  SaveLocked();
  return true;
}

std::vector<TypoSelfFixStore::Fix> TypoSelfFixStore::FindFixes(
    std::string_view raw, size_t max_fixes) const {
  std::vector<Fix> result;
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto& [wrong, value] : fixes_) {
    const size_t pos = raw.find(wrong);
    if (pos == std::string_view::npos) {
      continue;
    }
    std::string fixed(raw);
    fixed.replace(pos, wrong.size(), value.first);
    if (std::any_of(result.begin(), result.end(),
                    [&](const Fix& f) { return f.romaji == fixed; })) {
      continue;
    }
    result.push_back({std::move(fixed), value.second});
  }
  std::stable_sort(result.begin(), result.end(),
                   [](const Fix& a, const Fix& b) { return a.count > b.count; });
  if (result.size() > max_fixes) {
    result.resize(max_fixes);
  }
  return result;
}

void TypoSelfFixStore::LoadLocked() {
  fixes_.clear();
  if (path_.empty()) {
    return;
  }
  std::ifstream in(path_);
  std::string line;
  while (std::getline(in, line) && fixes_.size() < kMaxPatterns) {
    const size_t tab1 = line.find('\t');
    if (tab1 == std::string::npos) {
      continue;
    }
    const size_t tab2 = line.find('\t', tab1 + 1);
    if (tab2 == std::string::npos) {
      continue;
    }
    const std::string wrong = line.substr(0, tab1);
    const std::string right = line.substr(tab1 + 1, tab2 - tab1 - 1);
    if (!IsRomaji(wrong) || !IsRomaji(right) || wrong == right) {
      continue;
    }
    int count = 0;
    try {
      count = std::stoi(line.substr(tab2 + 1));
    } catch (...) {
      continue;
    }
    if (count > 0) {
      fixes_[wrong] = {right, std::min(kMaxCount, count)};
    }
  }
}

void TypoSelfFixStore::SaveLocked() const {
  if (path_.empty()) {
    return;
  }
  const std::string tmp = path_ + ".tmp";
  {
    std::ofstream out(tmp, std::ios::trunc);
    if (!out) {
      return;
    }
    for (const auto& [wrong, value] : fixes_) {
      out << wrong << '\t' << value.first << '\t' << value.second << '\n';
    }
    if (!out) {
      return;
    }
  }
  std::rename(tmp.c_str(), path_.c_str());
}

}  // namespace prediction
}  // namespace mozc
