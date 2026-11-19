// Copyright 2026
// Licensed under the same terms as Mozc.

#include "session/zenz_recent_commit_context.h"

#include <string>

#include "absl/time/time.h"
#include "testing/gunit.h"

namespace mozc {
namespace session {
namespace {

const absl::Time kStart = absl::FromUnixSeconds(1700000000);

TEST(ZenzRecentCommitContextTest, AppendsCommits) {
  ZenzRecentCommitContext context;
  EXPECT_EQ(context.Get(kStart), "");
  context.OnCommit("今日は", kStart);
  context.OnCommit("雨です", kStart + absl::Seconds(1));
  EXPECT_EQ(context.Get(kStart + absl::Seconds(2)), "今日は雨です");
}

TEST(ZenzRecentCommitContextTest, LineBreakAndBackspace) {
  ZenzRecentCommitContext context;
  context.OnLineBreak(kStart);  // Nothing to separate yet.
  EXPECT_EQ(context.Get(kStart), "");
  context.OnCommit("雨です", kStart);
  context.OnLineBreak(kStart);
  context.OnLineBreak(kStart);
  EXPECT_EQ(context.Get(kStart), "雨です\n");
  context.OnBackspace(kStart);
  context.OnBackspace(kStart);
  EXPECT_EQ(context.Get(kStart), "雨で");
}

TEST(ZenzRecentCommitContextTest, KeepsLastCharacters) {
  ZenzRecentCommitContext context;
  std::string text;
  for (int i = 0; i < 70; ++i) {
    text += "あ";
  }
  context.OnCommit(text, kStart);
  context.OnCommit("い", kStart);
  const std::string got(context.Get(kStart));
  EXPECT_EQ(got.size(), ZenzRecentCommitContext::kMaxChars * 3);
  EXPECT_EQ(got.substr(got.size() - 3), "い");
}

TEST(ZenzRecentCommitContextTest, ExpiresAndClears) {
  ZenzRecentCommitContext context;
  context.OnCommit("雨です", kStart);
  EXPECT_EQ(context.Get(kStart + absl::Minutes(5)), "雨です");
  EXPECT_EQ(context.Get(kStart + absl::Minutes(5) + absl::Seconds(1)), "");
  // A commit after expiry starts over.
  context.OnCommit("晴れ", kStart + absl::Minutes(6));
  EXPECT_EQ(context.Get(kStart + absl::Minutes(6)), "晴れ");
  context.Clear();
  EXPECT_EQ(context.Get(kStart + absl::Minutes(6)), "");
}

}  // namespace
}  // namespace session
}  // namespace mozc
