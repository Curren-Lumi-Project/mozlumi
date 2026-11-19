// Copyright 2010-2021, Google Inc.
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
//     * Neither the name of Google Inc. nor the names of its
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

#include "prediction/roman_typo_candidates.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mozc {
namespace prediction {
namespace {

// QWERTY neighbors (same row and the rows above/below).
std::string_view GetNeighbors(char c) {
  switch (c) {
    case 'q': return "wa";
    case 'w': return "qeas";
    case 'e': return "wrsd";
    case 'r': return "etdf";
    case 't': return "ryfg";
    case 'y': return "tugh";
    case 'u': return "yihj";
    case 'i': return "uojk";
    case 'o': return "ipkl";
    case 'p': return "ol";
    case 'a': return "qwsz";
    case 's': return "adwezx";
    case 'd': return "sferxc";
    case 'f': return "dgrtcv";
    case 'g': return "fhtyvb";
    case 'h': return "gjyubn";
    case 'j': return "hkuinm";
    case 'k': return "jliom";
    case 'l': return "kop";
    case 'z': return "asx";
    case 'x': return "zcsd";
    case 'c': return "xvdf";
    case 'v': return "cbfg";
    case 'b': return "vngh";
    case 'n': return "bmhj";
    case 'm': return "njk";
    default: return "";
  }
}

bool IsVowel(char c) {
  return c == 'a' || c == 'i' || c == 'u' || c == 'e' || c == 'o';
}

bool IsLowerAlpha(char c) { return c >= 'a' && c <= 'z'; }

// Characters that are frequently dropped while typing romaji. Other letters
// can also be dropped, with a slightly higher cost.
constexpr std::string_view kFrequentlyDroppedChars = "aiueonhy";
constexpr std::string_view kAllChars = "abcdefghijklmnopqrstuvwxyz";

// Costs. Smaller is more plausible.
constexpr float kSubstitutionCost = 1.0f;
// Vowel to neighboring vowel ("u" <-> "i", "i" <-> "o") is the most common
// romaji typo, e.g. "sunkai" -> "sinkai" (深海).
constexpr float kVowelSubstitutionCost = 0.8f;
// Dropping "n" before a consonant removes "ん" (a whole mora), which changes
// the reading more than a vowel typo, e.g. "sunkai" -> "sukai".
constexpr float kMoraNDeletionCost = 1.2f;
constexpr float kTranspositionCost = 0.8f;
constexpr float kDoubledKeyDeletionCost = 0.7f;
constexpr float kDeletionCost = 1.0f;
constexpr float kInsertionCost = 1.2f;
// A long vowel spelled as it sounds: "oo" for "ou" ("syooryuuken" ->
// "syouryuuken", 昇竜拳), "ee" for "ei" ("eega" -> "eiga", 映画), and the
// other way around ("toui" -> "tooi", 遠い). Not a key slip, so the keys
// need not be neighbors.
constexpr float kLongVowelSpellingCost = 0.8f;
constexpr float kRareInsertionCost = 1.3f;
// Typos on the first key are less likely (and more confusing when
// corrected).
constexpr float kFirstCharPenalty = 0.3f;

bool CandidateLess(const RomanTypoCandidate& a, const RomanTypoCandidate& b) {
  if (a.cost != b.cost) return a.cost < b.cost;
  return a.romaji < b.romaji;
}

}  // namespace

bool IsQwertyNeighbor(char c, char d) {
  return GetNeighbors(c).find(d) != std::string_view::npos;
}

namespace {

std::vector<RomanTypoCandidate> GenerateImpl(std::string_view raw,
                                             size_t min_length,
                                             size_t max_length,
                                             bool rare_insertions) {
  if (raw.size() < min_length || raw.size() > max_length) {
    return {};
  }
  for (const char c : raw) {
    if (!IsLowerAlpha(c) && c != '-') {
      return {};
    }
  }

  std::unordered_map<std::string, RomanTypoCandidate> best;
  auto add = [&](std::string romaji, float cost,
                 RomanTypoCandidate::Operation op, size_t pos,
                 std::string edit) {
    if (romaji.empty() || romaji == raw) {
      return;
    }
    if (pos == 0) {
      cost += kFirstCharPenalty;
    }
    auto it = best.find(romaji);
    if (it != best.end() && it->second.cost <= cost) {
      return;
    }
    RomanTypoCandidate cand;
    cand.romaji = romaji;
    cand.cost = cost;
    cand.operation = op;
    cand.edit = std::move(edit);
    best.insert_or_assign(std::move(romaji), std::move(cand));
  };

  const std::string base(raw);

  // Substitution with QWERTY neighbors.
  for (size_t i = 0; i < base.size(); ++i) {
    for (const char n : GetNeighbors(base[i])) {
      std::string s = base;
      s[i] = n;
      add(std::move(s),
          (IsVowel(base[i]) && IsVowel(n)) ? kVowelSubstitutionCost
                                           : kSubstitutionCost,
          RomanTypoCandidate::SUBSTITUTION, i,
          std::string("s:") + n + base[i]);
    }
  }

  // Long vowel spelling ("oo" <-> "ou", "ee" <-> "ei").
  for (size_t i = 1; i < base.size(); ++i) {
    char alt = 0;
    if (base[i - 1] == 'o' && base[i] == 'o') {
      alt = 'u';
    } else if (base[i - 1] == 'o' && base[i] == 'u') {
      alt = 'o';
    } else if (base[i - 1] == 'e' && base[i] == 'e') {
      alt = 'i';
    } else if (base[i - 1] == 'e' && base[i] == 'i') {
      alt = 'e';
    }
    if (alt == 0) {
      continue;
    }
    std::string s = base;
    s[i] = alt;
    add(std::move(s), kLongVowelSpellingCost, RomanTypoCandidate::SUBSTITUTION,
        i, std::string("s:") + alt + base[i]);
  }

  // Transposition of two adjacent keys.
  for (size_t i = 0; i + 1 < base.size(); ++i) {
    if (base[i] == base[i + 1]) {
      continue;
    }
    std::string s = base;
    std::swap(s[i], s[i + 1]);
    add(std::move(s), kTranspositionCost, RomanTypoCandidate::TRANSPOSITION, i,
        std::string("t:") + base[i] + base[i + 1]);
  }

  // Deletion of an extra key.
  for (size_t i = 0; i < base.size(); ++i) {
    // A doubled key ("kka" -> "ka") is a very common typo, but "nn" is a
    // legitimate way to type "ん" and doubled consonants are "っ", so the
    // dictionary check decides which one survives.
    const bool doubled = (i > 0 && base[i] == base[i - 1]);
    // A vowel next to a neighbor vowel key (e.g. "uio") is an extra press.
    const bool neighbor_extra =
        (i > 0 && IsQwertyNeighbor(base[i - 1], base[i])) ||
        (i + 1 < base.size() && IsQwertyNeighbor(base[i + 1], base[i]));
    const bool mora_n = !doubled && base[i] == 'n' && i + 1 < base.size() &&
                        IsLowerAlpha(base[i + 1]) && !IsVowel(base[i + 1]) &&
                        base[i + 1] != 'y' && base[i + 1] != 'n';
    float cost = kDeletionCost;
    if (doubled) {
      cost = kDoubledKeyDeletionCost;
    } else if (mora_n) {
      cost = kMoraNDeletionCost;
    } else if (neighbor_extra) {
      cost = kTranspositionCost;
    }
    std::string s = base;
    s.erase(i, 1);
    add(std::move(s), cost, RomanTypoCandidate::DELETION, i,
        std::string("d:") + base[i]);
  }

  // Insertion of a missed key.
  for (size_t i = 0; i <= base.size(); ++i) {
    for (const char c : kAllChars) {
      const bool frequent =
          kFrequentlyDroppedChars.find(c) != std::string_view::npos;
      if (!frequent && !rare_insertions) {
        continue;
      }
      // Invalid romaji produced here is rejected later by the romaji table
      // and the dictionary check.
      std::string s = base;
      s.insert(s.begin() + i, c);
      add(std::move(s), frequent ? kInsertionCost : kRareInsertionCost,
          RomanTypoCandidate::INSERTION, i, std::string("i:") + c);
    }
    // Missed doubled consonant ("kata" -> "katta").
    if (i < base.size() && IsLowerAlpha(base[i]) && !IsVowel(base[i]) &&
        base[i] != 'n' && (i == 0 || base[i - 1] != base[i])) {
      std::string s = base;
      s.insert(s.begin() + i, base[i]);
      add(std::move(s), kInsertionCost, RomanTypoCandidate::INSERTION, i,
          std::string("i:") + base[i]);
    }
  }

  std::vector<RomanTypoCandidate> results;
  results.reserve(best.size());
  for (auto& [unused, cand] : best) {
    results.push_back(std::move(cand));
  }
  std::sort(results.begin(), results.end(), CandidateLess);
  return results;
}

}  // namespace

std::vector<RomanTypoCandidate> GenerateRomanTypoCandidates(
    std::string_view raw, size_t min_length, size_t max_length) {
  return GenerateImpl(raw, min_length, max_length, /*rare_insertions=*/true);
}

std::vector<RomanTypoCandidate> GenerateTwoEditRomanTypoCandidates(
    std::string_view raw, size_t first_k, size_t max_results,
    float extra_cost, size_t min_length, size_t max_length) {
  std::vector<RomanTypoCandidate> first =
      GenerateImpl(raw, min_length, max_length, /*rare_insertions=*/true);
  std::unordered_map<std::string, bool> one_edit;
  for (const RomanTypoCandidate& c : first) {
    one_edit.emplace(c.romaji, true);
  }
  if (first.size() > first_k) {
    first.resize(first_k);
  }
  std::unordered_map<std::string, RomanTypoCandidate> best;
  for (const RomanTypoCandidate& c1 : first) {
    for (RomanTypoCandidate& c2 :
         GenerateImpl(c1.romaji, min_length, max_length + 1,
                      /*rare_insertions=*/false)) {
      if (c2.romaji == raw || one_edit.contains(c2.romaji)) {
        continue;
      }
      const float cost = c1.cost + c2.cost + extra_cost;
      auto it = best.find(c2.romaji);
      if (it != best.end() && it->second.cost <= cost) {
        continue;
      }
      RomanTypoCandidate cand;
      cand.romaji = c2.romaji;
      cand.cost = cost;
      cand.operation = c1.operation;
      cand.edit = c1.edit + "|" + c2.edit;
      best.insert_or_assign(std::move(c2.romaji), std::move(cand));
    }
  }
  std::vector<RomanTypoCandidate> results;
  results.reserve(best.size());
  for (auto& [unused, cand] : best) {
    results.push_back(std::move(cand));
  }
  std::sort(results.begin(), results.end(), CandidateLess);
  if (results.size() > max_results) {
    results.resize(max_results);
  }
  return results;
}

}  // namespace prediction
}  // namespace mozc
