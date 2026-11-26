// Copyright 2026
// Licensed under the same terms as Mozc.

#include "renderer/candidate_position_log.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <ios>
#include <mutex>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/file_util.h"
#include "base/system_util.h"

namespace mozc {
namespace renderer {
namespace {

constexpr int64_t kMaxCandidatePositionLogBytes = 1 << 20;

}  // namespace

void WriteCandidatePositionLogToFile(const std::string &path,
                                     absl::string_view message,
                                     int64_t max_bytes) {
  static std::mutex *const mutex = new std::mutex();
  std::lock_guard<std::mutex> lock(*mutex);
  {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (in) {
      const std::streamoff size = in.tellg();
      if (size > max_bytes) {
        in.close();
        std::rename(path.c_str(), (path + ".1").c_str());
      }
    }
  }
  // Write the whole line at once, so that the lines of the input method and
  // the renderer do not mix.
  const std::string line =
      absl::StrCat(absl::FormatTime("%Y-%m-%d %H:%M:%E3S ", absl::Now(),
                                    absl::LocalTimeZone()),
                   message, "\n");
  std::ofstream out(path, std::ios::app | std::ios::binary);
  if (!out) {
    return;
  }
  out.write(line.data(), static_cast<std::streamsize>(line.size()));
}

void WriteCandidatePositionLog(absl::string_view message) {
#if defined(__APPLE__)
  const std::string dir = SystemUtil::GetLoggingDirectory();
  if (dir.empty()) {
    return;
  }
  WriteCandidatePositionLogToFile(
      FileUtil::JoinPath(dir, "candidate_position.log"), message,
      kMaxCandidatePositionLogBytes);
#endif  // __APPLE__
}

}  // namespace renderer
}  // namespace mozc
