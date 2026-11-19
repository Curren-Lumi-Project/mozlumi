// Copyright 2026 Curren Lumi Project.
// Licensed under the same terms as Mozc.

#include "prediction/typo_self_fix_store.h"

#include <string>

#include "base/file/temp_dir.h"
#include "base/file_util.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace prediction {
namespace {

TEST(TypoSelfFixStoreTest, ExtractPattern) {
  // A dropped "y": 一寸 -> 一瞬.
  auto p = TypoSelfFixStore::ExtractPattern("issunnnodekigotodatta",
                                            "issyunnnodekigotodatta");
  ASSERT_TRUE(p.has_value());
  EXPECT_EQ(p->first, "issunn");
  EXPECT_EQ(p->second, "issyunn");

  // An extra "i".
  p = TypoSelfFixStore::ExtractPattern("issyiunn", "issyunn");
  ASSERT_TRUE(p.has_value());
  EXPECT_EQ(p->first, "ssyiunn");
  EXPECT_EQ(p->second, "ssyunn");

  // Too different, the same, or not romaji.
  EXPECT_FALSE(TypoSelfFixStore::ExtractPattern("kyouhaame", "asitahahare"));
  EXPECT_FALSE(TypoSelfFixStore::ExtractPattern("issyunn", "issyunn"));
  EXPECT_FALSE(TypoSelfFixStore::ExtractPattern("issunn", "ISSYUNN"));
  // Too short to be specific.
  EXPECT_FALSE(TypoSelfFixStore::ExtractPattern("ka", "ga"));
}

TEST(TypoSelfFixStoreTest, LearnFindAndSave) {
  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();
  const std::string path =
      FileUtil::JoinPath(temp_dir.path(), "typo_self_fixes.tsv");
  {
    TypoSelfFixStore store(path);
    EXPECT_TRUE(store.FindFixes("issunnnodekigoto", 2).empty());
    EXPECT_TRUE(store.Learn("issunnnodekigotodatta", "issyunnnodekigotodatta"));
    EXPECT_TRUE(store.Learn("issunndemiwakeru", "issyunndemiwakeru"));
    EXPECT_FALSE(store.Learn("kyouhaame", "asitahahare"));
  }
  // Loaded from the file.
  TypoSelfFixStore store(path);
  const auto fixes = store.FindFixes("sorehaissunnnokotodesita", 2);
  ASSERT_EQ(fixes.size(), 1);
  EXPECT_EQ(fixes[0].romaji, "sorehaissyunnnokotodesita");
  EXPECT_EQ(fixes[0].count, 2);
  EXPECT_TRUE(store.FindFixes("kyouhaame", 2).empty());
}

}  // namespace
}  // namespace prediction
}  // namespace mozc
