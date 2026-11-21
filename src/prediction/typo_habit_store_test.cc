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

#include <string>

#include "base/file/temp_dir.h"
#include "base/file_util.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace prediction {
namespace {

TEST(TypoHabitStoreTest, LearnsOnlyRememberedCorrections) {
  TypoHabitStore store("");
  EXPECT_EQ(store.CostAdjustment("s:aq"), 0.0f);

  store.Remember("kqisya", "かいしゃ", "s:aq");
  // A different reading was committed.
  EXPECT_FALSE(store.LearnCommitted("kqisya", "しんかい"));
  // A longer word predicted from the corrected reading.
  EXPECT_TRUE(store.LearnCommitted("kqisya", "かいしゃいん"));
  EXPECT_EQ(store.Count("s:aq"), 1);
  EXPECT_FLOAT_EQ(store.CostAdjustment("s:aq"),
                  -TypoHabitStore::kBonusPerCount);
}

TEST(TypoHabitStoreTest, BonusIsBounded) {
  TypoHabitStore store("");
  store.Remember("sunkai", "しんかい", "s:iu");
  store.Remember("kqisya", "かいしゃ", "s:aq");
  for (int i = 0; i < 30; ++i) {
    store.LearnCommitted("sunkai", "しんかい");
    store.LearnCommitted("kqisya", "かいしゃ");
  }
  EXPECT_EQ(store.Count("s:iu"), TypoHabitStore::kMaxCount);
  EXPECT_FLOAT_EQ(store.CostAdjustment("s:iu"),
                  -TypoHabitStore::kMaxBonusPerEdit);
  EXPECT_FLOAT_EQ(store.CostAdjustment("s:iu|s:aq"),
                  -TypoHabitStore::kMaxBonus);
}

TEST(TypoHabitStoreTest, IgnoresInvalidPatterns) {
  TypoHabitStore store("");
  store.Remember("abc", "あいう", "bad|s:zz");
  EXPECT_TRUE(store.LearnCommitted("abc", "あいう"));
  EXPECT_EQ(store.Count("bad"), 0);
  EXPECT_EQ(store.Count("s:zz"), 1);
}

TEST(TypoHabitStoreTest, SavesAndLoads) {
  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();
  const std::string path =
      FileUtil::JoinPath(temp_dir.path(), "typo_habits.tsv");
  {
    TypoHabitStore store(path);
    store.Remember("sunkai", "しんかい", "s:iu");
    ASSERT_TRUE(store.LearnCommitted("sunkai", "しんかい"));
  }
  TypoHabitStore loaded(path);
  EXPECT_EQ(loaded.Count("s:iu"), 1);
}

// Mozlumi: saving again must replace the file (std::rename() did not on
// Windows), and the profile path may contain non-ASCII characters (e.g. a user
// name in Japanese).
TEST(TypoHabitStoreTest, SavesRepeatedlyUnderNonAsciiDirectory) {
  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();
  const std::string dir = FileUtil::JoinPath(temp_dir.path(), "もずるみ");
  ASSERT_TRUE(FileUtil::CreateDirectory(dir).ok());
  const std::string path = FileUtil::JoinPath(dir, "typo_habits.tsv");
  {
    TypoHabitStore store(path);
    store.Remember("sunkai", "しんかい", "s:iu");
    ASSERT_TRUE(store.LearnCommitted("sunkai", "しんかい"));
    // The second save must replace the file written by the first one.
    ASSERT_TRUE(store.LearnCommitted("sunkai", "しんかい"));
  }
  TypoHabitStore loaded(path);
  EXPECT_EQ(loaded.Count("s:iu"), 2);
}

}  // namespace
}  // namespace prediction
}  // namespace mozc
