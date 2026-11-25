// Copyright 2026
// Licensed under the same terms as Mozc.

#include "session/zenz_recent_commit_store.h"

#include <string>

#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "session/zenz_recent_commit_context.h"
#include "testing/gunit.h"

namespace mozc {
namespace session {
namespace {

const absl::Time kStart = absl::FromUnixSeconds(1700000000);
constexpr char kAppA[] = "com.example.a";
constexpr char kAppB[] = "com.example.b";

TEST(ZenzRecentCommitStoreTest, KeepsTextByApplication) {
  ZenzRecentCommitStore store;
  store.OnCommit(kAppA, "説明の仕方", kStart);
  store.OnCommit(kAppB, "同期して", kStart + absl::Seconds(1));
  const absl::Time now = kStart + absl::Seconds(2);
  EXPECT_EQ(store.GetContext(kAppA, /*across_apps=*/false, now), "説明の仕方");
  EXPECT_EQ(store.GetContext(kAppB, /*across_apps=*/false, now), "同期して");
  // Coming back to A after B: A's text is still there, without B's.
  store.OnCommit(kAppA, "して", now);
  EXPECT_EQ(store.GetContext(kAppA, false, now), "説明の仕方して");
}

TEST(ZenzRecentCommitStoreTest, AcrossApplications) {
  ZenzRecentCommitStore store;
  store.OnCommit(kAppA, "説明の仕方", kStart);
  store.OnCommit(kAppB, "同期して", kStart + absl::Seconds(1));
  const absl::Time now = kStart + absl::Seconds(2);
  EXPECT_EQ(store.GetContext(kAppA, /*across_apps=*/true, now),
            "説明の仕方同期して");
  EXPECT_EQ(store.GetContext(kAppB, /*across_apps=*/true, now),
            "説明の仕方同期して");
}

TEST(ZenzRecentCommitStoreTest, UnknownOrEmptyApplication) {
  ZenzRecentCommitStore store;
  store.OnCommit("", "無視", kStart);
  EXPECT_EQ(store.GetContext("", false, kStart), "");
  EXPECT_EQ(store.GetContext("", true, kStart), "");
  EXPECT_EQ(store.GetContext(kAppA, false, kStart), "");
  EXPECT_EQ(store.AppCountForTest(), 0u);
}

TEST(ZenzRecentCommitStoreTest, ClearForgetsThatAppAndAcross) {
  ZenzRecentCommitStore store;
  store.OnCommit(kAppA, "あ", kStart);
  store.OnCommit(kAppB, "い", kStart);
  store.Clear(kAppA);
  EXPECT_EQ(store.GetContext(kAppA, false, kStart), "");
  EXPECT_EQ(store.GetContext(kAppB, false, kStart), "い");
  EXPECT_EQ(store.GetContext(kAppB, true, kStart), "");
}

TEST(ZenzRecentCommitStoreTest, LineBreakAndBackspace) {
  ZenzRecentCommitStore store;
  store.OnCommit(kAppA, "雨です", kStart);
  store.OnLineBreak(kAppA, kStart);
  EXPECT_EQ(store.GetContext(kAppA, false, kStart), "雨です\n");
  store.OnBackspace(kAppA, kStart);
  store.OnBackspace(kAppA, kStart);
  EXPECT_EQ(store.GetContext(kAppA, false, kStart), "雨で");
  EXPECT_EQ(store.GetContext(kAppA, true, kStart), "雨で");
}

TEST(ZenzRecentCommitStoreTest, Expires) {
  ZenzRecentCommitStore store;
  store.OnCommit(kAppA, "古い", kStart);
  const absl::Time later =
      kStart + ZenzRecentCommitContext::kLifetime + absl::Seconds(1);
  EXPECT_EQ(store.GetContext(kAppA, false, later), "");
  EXPECT_EQ(store.GetContext(kAppA, true, later), "");
}

TEST(ZenzRecentCommitStoreTest, DropsLeastRecentlyUsedApplication) {
  ZenzRecentCommitStore store;
  for (size_t i = 0; i < ZenzRecentCommitStore::kMaxApps; ++i) {
    store.OnCommit(absl::StrCat("app", i), "x", kStart + absl::Seconds(i));
  }
  EXPECT_EQ(store.AppCountForTest(), ZenzRecentCommitStore::kMaxApps);
  // app0 is the least recently used one.
  const absl::Time now =
      kStart + absl::Seconds(ZenzRecentCommitStore::kMaxApps);
  store.OnCommit("new", "y", now);
  EXPECT_EQ(store.AppCountForTest(), ZenzRecentCommitStore::kMaxApps);
  EXPECT_EQ(store.GetContext("app0", false, now), "");
  EXPECT_EQ(store.GetContext("app1", false, now), "x");
  EXPECT_EQ(store.GetContext("new", false, now), "y");
}

}  // namespace
}  // namespace session
}  // namespace mozc
