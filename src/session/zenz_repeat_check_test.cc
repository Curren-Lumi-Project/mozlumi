// Copyright 2026
// Licensed under the same terms as Mozc.

#include "session/zenz_repeat_check.h"

#include <vector>

#include "testing/gunit.h"

namespace mozc {
namespace session {
namespace {

TEST(ZenzRepeatCheckTest, ChangedWord) {
  EXPECT_EQ(ZenzChangedWord("同期してたら", "動悸してたら"), "動悸");
  EXPECT_EQ(ZenzChangedWord("動悸がした", "同期がした"), "同期");
  // Characters sharing some bytes (動/同, 機/期) are not cut in the middle.
  EXPECT_EQ(ZenzChangedWord("ファイルの動機を", "ファイルの同期を"), "同期");
  EXPECT_EQ(ZenzChangedWord("して高いままで", "してたか今まで"), "たか今");
  EXPECT_EQ(ZenzChangedWord("同じ", "同じ"), "");
}

TEST(ZenzRepeatCheckTest, WordBeforeInTheSentenceIsCut) {
  // 「どうきしてたら｜どうきがしてきた」: the AI made the 2nd one 「同期」 too.
  const std::vector<ZenzRankPart> parts = {
      {0, "どうきしてたら", "同期してたら", "同期してたら", false},
      {1, "どうきがしてきた", "動悸がしてきた", "同期がしてきた", true},
  };
  const std::vector<ZenzRepeatCheck> checks =
      FindZenzRepeatChecks("高域は鋭い、", parts, 4);
  ASSERT_EQ(checks.size(), 1u);
  EXPECT_EQ(checks[0].segment_index, 1u);
  EXPECT_EQ(checks[0].key, "どうきがしてきた");
  EXPECT_EQ(checks[0].mozc_value, "動悸がしてきた");
  EXPECT_EQ(checks[0].ai_value, "同期がしてきた");
  EXPECT_EQ(checks[0].left_context, "してたら");
}

TEST(ZenzRepeatCheckTest, WordInThePrecedingTextIsCut) {
  const std::vector<ZenzRankPart> parts = {
      {0, "どうきがした", "動悸がした", "同期がした", true},
  };
  const std::vector<ZenzRepeatCheck> checks =
      FindZenzRepeatChecks("ファイルの同期をしてたら", parts, 4);
  ASSERT_EQ(checks.size(), 1u);
  EXPECT_EQ(checks[0].left_context, "をしてたら");
}

TEST(ZenzRepeatCheckTest, WordAfterLeavesTheContext) {
  // The AI made the 1st one the same as the 2nd one.
  const std::vector<ZenzRankPart> parts = {
      {0, "どうきしてたら", "同期してたら", "動悸してたら", true},
      {1, "どうきがしてきた", "動悸がしてきた", "動悸がしてきた", false},
  };
  const std::vector<ZenzRepeatCheck> checks =
      FindZenzRepeatChecks("前の文。", parts, 4);
  ASSERT_EQ(checks.size(), 1u);
  EXPECT_EQ(checks[0].segment_index, 0u);
  EXPECT_EQ(checks[0].left_context, "前の文。");
}

TEST(ZenzRepeatCheckTest, NoRepeatNoCheck) {
  const std::vector<ZenzRankPart> parts = {
      {0, "どうきしておいて", "動悸しておいて", "同期しておいて", true},
  };
  EXPECT_TRUE(FindZenzRepeatChecks("データを", parts, 4).empty());
  // A changed word of one character or of hiragana only is not checked.
  const std::vector<ZenzRankPart> short_parts = {
      {0, "きがした", "木がした", "気がした", true},
      {1, "きのせい", "気のせい", "気のせい", false},
      {2, "いって", "行って", "言って", true},
      {3, "きょう", "今日", "きょう", true},
      {4, "きょうも", "きょうも", "きょうも", false},
  };
  EXPECT_TRUE(FindZenzRepeatChecks("", short_parts, 4).empty());
}

TEST(ZenzRepeatCheckTest, ChooseValue) {
  ZenzRepeatCheck check;
  check.key = "どうきがしてきた";
  check.mozc_value = "動機がしてきた";
  check.ai_value = "同期がしてきた";

  // 2026-11-25 20:32: the AI conversion was better than the normal one, and
  // Zenz's own answer without the other word was even better.
  ZenzRepeatCheckScores scores;
  scores.mozc = -22.27;
  scores.ai = -19.82;
  scores.local = -16.73;
  scores.local_value = "動悸がしてきた";
  EXPECT_EQ(ChooseZenzRepeatCheckValue(check, scores, 1.5), "動悸がしてきた");

  // Zenz's own answer is the AI conversion.
  scores.local = -19.82;
  scores.local_value = "同期がしてきた";
  EXPECT_EQ(ChooseZenzRepeatCheckValue(check, scores, 1.5), "同期がしてきた");

  // Not better by the margin: the normal conversion stays.
  scores.mozc = -19.0;
  scores.local = -18.0;
  scores.local_value = "動悸がしてきた";
  EXPECT_EQ(ChooseZenzRepeatCheckValue(check, scores, 1.5), "");

  // Zenz's own answer is the normal conversion.
  scores.mozc = -22.27;
  scores.ai = -23.0;
  scores.local = -22.27;
  scores.local_value = "動機がしてきた";
  EXPECT_EQ(ChooseZenzRepeatCheckValue(check, scores, 1.5), "");

  // The AI job failed; Zenz's own answer is used.
  scores.ai.reset();
  scores.local = -16.73;
  scores.local_value = "動悸がしてきた";
  EXPECT_EQ(ChooseZenzRepeatCheckValue(check, scores, 1.5), "動悸がしてきた");

  // No usable answer of Zenz; the AI conversion is better by the margin.
  scores.ai = -19.82;
  scores.local.reset();
  scores.local_value.clear();
  EXPECT_EQ(ChooseZenzRepeatCheckValue(check, scores, 1.5), "同期がしてきた");
}

TEST(ZenzRepeatCheckTest, MaxChecks) {
  const std::vector<ZenzRankPart> parts = {
      {0, "どうきを", "動機を", "同期を", true},
      {1, "どうきが", "動機が", "同期が", true},
      {2, "どうきで", "動機で", "同期で", true},
  };
  const std::vector<ZenzRepeatCheck> checks =
      FindZenzRepeatChecks("", parts, 2);
  ASSERT_EQ(checks.size(), 2u);
  EXPECT_EQ(checks[0].segment_index, 0u);
  // Nothing before the 1st one; the 2nd one is cut after the 1st one.
  EXPECT_EQ(checks[0].left_context, "");
  EXPECT_EQ(checks[1].segment_index, 1u);
  EXPECT_EQ(checks[1].left_context, "を");
}

}  // namespace
}  // namespace session
}  // namespace mozc
