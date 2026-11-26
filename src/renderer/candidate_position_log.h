// Copyright 2026
// Licensed under the same terms as Mozc.

#ifndef MOZC_RENDERER_CANDIDATE_POSITION_LOG_H_
#define MOZC_RENDERER_CANDIDATE_POSITION_LOG_H_

#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"

namespace mozc {
namespace renderer {

// Mozlumi (diagnostic, Build 63): the candidate window sometimes covers the
// text being typed in some applications (for example chat applications whose
// text field changes its height).  To find out why, the input method and the
// renderer write where the application said the composition is and where the
// window was placed to <logging directory>/candidate_position.log
// (~/Library/Logs/Mozlumi on macOS).  Only coordinates, sizes, counts and the
// application's bundle ID are written, never the typed text.

// Appends "<local time with milliseconds> <message>\n" to the log.  Does
// nothing outside macOS.
void WriteCandidatePositionLog(absl::string_view message);

// Appends the same line to |path|.  When |path| is already larger than
// |max_bytes|, it is renamed to |path| + ".1" first.  Exposed for testing.
void WriteCandidatePositionLogToFile(const std::string &path,
                                     absl::string_view message,
                                     int64_t max_bytes);

}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_CANDIDATE_POSITION_LOG_H_
