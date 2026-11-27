// Copyright 2026
// Licensed under the same terms as Mozc.

#include "renderer/candidate_position_log.h"

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/str_split.h"
#include "base/file/temp_dir.h"
#include "base/file_util.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace renderer {
namespace {

std::vector<std::string> ReadLines(const std::string &path) {
  absl::StatusOr<std::string> content = FileUtil::GetContents(path);
  if (!content.ok()) {
    return {};
  }
  return absl::StrSplit(*content, '\n', absl::SkipEmpty());
}

// "2026-11-26 23:20:00.123 " is 24 characters.
constexpr size_t kTimeLength = 24;

TEST(CandidatePositionLogTest, AppendsLinesWithTime) {
  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();
  const std::string path =
      FileUtil::JoinPath(temp_dir.path(), "candidate_position.log");

  WriteCandidatePositionLogToFile(path, "imk hide", 1024);
  WriteCandidatePositionLogToFile(path, "renderer place", 1024);

  const std::vector<std::string> lines = ReadLines(path);
  ASSERT_EQ(lines.size(), 2u);
  ASSERT_GT(lines[0].size(), kTimeLength);
  EXPECT_EQ(lines[0][4], '-');
  EXPECT_EQ(lines[0][10], ' ');
  EXPECT_EQ(lines[0][19], '.');
  EXPECT_EQ(lines[0][23], ' ');
  EXPECT_EQ(lines[0].substr(kTimeLength), "imk hide");
  EXPECT_EQ(lines[1].substr(kTimeLength), "renderer place");
}

TEST(CandidatePositionLogTest, MovesALargeLogAside) {
  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();
  const std::string path =
      FileUtil::JoinPath(temp_dir.path(), "candidate_position.log");

  WriteCandidatePositionLogToFile(path, std::string(100, 'a'), 50);
  WriteCandidatePositionLogToFile(path, "next", 50);

  const std::vector<std::string> lines = ReadLines(path);
  ASSERT_EQ(lines.size(), 1u);
  EXPECT_EQ(lines[0].substr(kTimeLength), "next");

  const std::vector<std::string> old_lines = ReadLines(path + ".1");
  ASSERT_EQ(old_lines.size(), 1u);
  EXPECT_EQ(old_lines[0].substr(kTimeLength), std::string(100, 'a'));
}

TEST(CandidatePositionLogTest, IsOffUntilTurnedOn) {
  EXPECT_FALSE(IsCandidatePositionLogEnabled());
  SetCandidatePositionLogEnabled(true);
  EXPECT_TRUE(IsCandidatePositionLogEnabled());
  SetCandidatePositionLogEnabled(false);
  EXPECT_FALSE(IsCandidatePositionLogEnabled());
}

}  // namespace
}  // namespace renderer
}  // namespace mozc
