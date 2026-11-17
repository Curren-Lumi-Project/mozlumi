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

#include "prediction/typo_habit_store.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

#include "base/file_util.h"
#include "base/system_util.h"

namespace mozc {
namespace prediction {
namespace {

constexpr char kFileName[] = "typo_habits.tsv";

// A pattern is short ASCII such as "s:ai", "t:hs", "d:k" or "i:u".
bool IsValidPattern(std::string_view p) {
  if (p.size() < 3 || p.size() > 4 || p[1] != ':') {
    return false;
  }
  if (p[0] != 's' && p[0] != 't' && p[0] != 'd' && p[0] != 'i') {
    return false;
  }
  for (size_t i = 2; i < p.size(); ++i) {
    const char c = p[i];
    if (!((c >= 'a' && c <= 'z') || c == '-')) {
      return false;
    }
  }
  return true;
}

template <typename F>
void ForEachEdit(std::string_view edits, F f) {
  while (!edits.empty()) {
    const size_t bar = edits.find('|');
    f(edits.substr(0, bar));
    if (bar == std::string_view::npos) {
      break;
    }
    edits.remove_prefix(bar + 1);
  }
}

}  // namespace

TypoHabitStore::TypoHabitStore(std::string path) : path_(std::move(path)) {
  std::lock_guard<std::mutex> lock(mutex_);
  LoadLocked();
}

TypoHabitStore* TypoHabitStore::Get() {
  static TypoHabitStore* const store = new TypoHabitStore(
      FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(), kFileName));
  return store;
}

float TypoHabitStore::CostAdjustment(std::string_view edits) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (counts_.empty()) {
    return 0.0f;
  }
  float bonus = 0.0f;
  ForEachEdit(edits, [&](std::string_view edit) {
    const auto it = counts_.find(edit);
    if (it != counts_.end()) {
      bonus += std::min(kMaxBonusPerEdit, kBonusPerCount * it->second);
    }
  });
  return -std::min(kMaxBonus, bonus);
}

void TypoHabitStore::Remember(std::string_view raw, std::string_view key,
                              std::string_view edits) {
  if (raw.empty() || key.empty() || edits.empty()) {
    return;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  for (const Recent& r : recent_) {
    if (r.raw == raw && r.key == key) {
      return;
    }
  }
  recent_.push_back({std::string(raw), std::string(key), std::string(edits)});
  while (recent_.size() > kMaxRecent) {
    recent_.pop_front();
  }
}

bool TypoHabitStore::LearnCommitted(std::string_view raw,
                                    std::string_view committed_key) {
  std::lock_guard<std::mutex> lock(mutex_);
  // The committed candidate may be a longer word predicted from the corrected
  // reading, e.g. "しんかいぎょ" for "しんかい". Prefer the longest match.
  const Recent* found = nullptr;
  for (const Recent& r : recent_) {
    if (r.raw != raw || !committed_key.starts_with(r.key)) {
      continue;
    }
    if (found == nullptr || r.key.size() > found->key.size()) {
      found = &r;
    }
  }
  if (found == nullptr) {
    return false;
  }
  bool learned = false;
  ForEachEdit(found->edits, [&](std::string_view edit) {
    if (!IsValidPattern(edit)) {
      return;
    }
    auto it = counts_.find(edit);
    if (it == counts_.end()) {
      if (counts_.size() >= kMaxPatterns) {
        return;
      }
      it = counts_.emplace(std::string(edit), 0).first;
    }
    it->second = std::min(kMaxCount, it->second + 1);
    learned = true;
  });
  if (learned) {
    SaveLocked();
  }
  return learned;
}

int TypoHabitStore::Count(std::string_view edit) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = counts_.find(edit);
  return it == counts_.end() ? 0 : it->second;
}

void TypoHabitStore::LoadLocked() {
  counts_.clear();
  if (path_.empty()) {
    return;
  }
  std::ifstream in(path_);
  std::string line;
  while (std::getline(in, line) && counts_.size() < kMaxPatterns) {
    const size_t tab = line.find('\t');
    if (tab == std::string::npos) {
      continue;
    }
    const std::string pattern = line.substr(0, tab);
    if (!IsValidPattern(pattern)) {
      continue;
    }
    int count = 0;
    try {
      count = std::stoi(line.substr(tab + 1));
    } catch (...) {
      continue;
    }
    if (count > 0) {
      counts_[pattern] = std::min(kMaxCount, count);
    }
  }
}

void TypoHabitStore::SaveLocked() const {
  if (path_.empty()) {
    return;
  }
  const std::string tmp = path_ + ".tmp";
  {
    std::ofstream out(tmp, std::ios::trunc);
    if (!out) {
      return;
    }
    for (const auto& [pattern, count] : counts_) {
      out << pattern << '\t' << count << '\n';
    }
    if (!out) {
      return;
    }
  }
  std::rename(tmp.c_str(), path_.c_str());
}

}  // namespace prediction
}  // namespace mozc
