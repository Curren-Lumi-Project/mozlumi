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
#include <string>
#include <vector>

#include "testing/gunit.h"

namespace mozc {
namespace prediction {
namespace {

bool Contains(const std::vector<RomanTypoCandidate>& candidates,
              const std::string& romaji) {
  return std::any_of(
      candidates.begin(), candidates.end(),
      [&](const RomanTypoCandidate& c) { return c.romaji == romaji; });
}

TEST(RomanTypoCandidatesTest, NeighborSubstitution) {
  const auto candidates = GenerateRomanTypoCandidates("sunkai");
  EXPECT_TRUE(Contains(candidates, "sinkai"));  // u -> i (深海)
  EXPECT_FALSE(Contains(candidates, "sunkai"));
}

float CostOf(const std::vector<RomanTypoCandidate>& candidates,
             const std::string& romaji) {
  for (const RomanTypoCandidate& c : candidates) {
    if (c.romaji == romaji) return c.cost;
  }
  return 1e9f;
}

TEST(RomanTypoCandidatesTest, VowelTypoIsPreferredOverDroppingN) {
  const auto candidates = GenerateRomanTypoCandidates("sunkai");
  EXPECT_LT(CostOf(candidates, "sinkai"), CostOf(candidates, "sukai"));
}

TEST(RomanTypoCandidatesTest, LongVowelSpelling) {
  // Not QWERTY neighbors, but the same long vowel spelled as it sounds.
  const auto candidates = GenerateRomanTypoCandidates("syooryuuken");
  EXPECT_TRUE(Contains(candidates, "syouryuuken"));  // 昇竜拳
  EXPECT_LT(CostOf(candidates, "syouryuuken"), 1.0f);
  EXPECT_TRUE(Contains(GenerateRomanTypoCandidates("eega"), "eiga"));  // 映画
  EXPECT_TRUE(Contains(GenerateRomanTypoCandidates("toui"), "tooi"));  // 遠い
  EXPECT_TRUE(Contains(GenerateRomanTypoCandidates("keeki"), "keiki"));
}

TEST(RomanTypoCandidatesTest, Transposition) {
  EXPECT_TRUE(Contains(GenerateRomanTypoCandidates("sunkai"), "suknai"));
}

TEST(RomanTypoCandidatesTest, Deletion) {
  EXPECT_TRUE(Contains(GenerateRomanTypoCandidates("kkaisya"), "kaisya"));
}

TEST(RomanTypoCandidatesTest, Insertion) {
  EXPECT_TRUE(Contains(GenerateRomanTypoCandidates("ktta"), "katta"));
  EXPECT_TRUE(Contains(GenerateRomanTypoCandidates("kata"), "katta"));
}

TEST(RomanTypoCandidatesTest, InsertionOfAnyLetter) {
  // A dropped "d" ("tomoati" -> "tomodati").
  EXPECT_TRUE(Contains(GenerateRomanTypoCandidates("tomoati"), "tomodati"));
}

TEST(RomanTypoCandidatesTest, RejectsInvalidInput) {
  EXPECT_TRUE(GenerateRomanTypoCandidates("Sunkai").empty());
  EXPECT_TRUE(GenerateRomanTypoCandidates("abc").empty());
  EXPECT_TRUE(GenerateRomanTypoCandidates("sun1ai").empty());
}

TEST(RomanTypoCandidatesTest, SortedByCost) {
  const auto candidates = GenerateRomanTypoCandidates("sunkai");
  ASSERT_FALSE(candidates.empty());
  for (size_t i = 1; i < candidates.size(); ++i) {
    EXPECT_LE(candidates[i - 1].cost, candidates[i].cost);
  }
}

TEST(RomanTypoCandidatesTest, QwertyNeighbor) {
  EXPECT_TRUE(IsQwertyNeighbor('u', 'i'));
  EXPECT_TRUE(IsQwertyNeighbor('a', 's'));
  EXPECT_FALSE(IsQwertyNeighbor('a', 'p'));
}

}  // namespace
}  // namespace prediction
}  // namespace mozc
