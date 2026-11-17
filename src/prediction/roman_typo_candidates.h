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

// Mozlumi: Generates romaji candidates that are one keystroke away from the
// user's raw input (ATOK-like "誤入力補正").
//
// This file intentionally depends only on the standard library so that it can
// be unit-tested in isolation.

#ifndef MOZC_PREDICTION_ROMAN_TYPO_CANDIDATES_H_
#define MOZC_PREDICTION_ROMAN_TYPO_CANDIDATES_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace mozc {
namespace prediction {

struct RomanTypoCandidate {
  enum Operation {
    SUBSTITUTION = 0,  // Neighbor key on QWERTY was pressed.
    TRANSPOSITION,     // Two adjacent keys were swapped.
    DELETION,          // An extra key was pressed.
    INSERTION,         // A key was missed.
  };

  std::string romaji;
  // Smaller is more plausible.
  float cost = 0.0f;
  Operation operation = SUBSTITUTION;
};

// Returns true if `c` is next to `d` on a QWERTY keyboard.
bool IsQwertyNeighbor(char c, char d);

// Returns the candidates sorted by cost (then lexicographically) with
// duplicates and the input itself removed.
// Returns an empty vector when `raw` is shorter than `min_length`, longer than
// `max_length` or contains characters other than lower-case ASCII letters and
// '-'. Upper-case letters mean that the user intends to type alphabets, so
// such input is never corrected.
std::vector<RomanTypoCandidate> GenerateRomanTypoCandidates(
    std::string_view raw, size_t min_length = 4, size_t max_length = 24);

}  // namespace prediction
}  // namespace mozc

#endif  // MOZC_PREDICTION_ROMAN_TYPO_CANDIDATES_H_
