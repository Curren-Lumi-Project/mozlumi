// Copyright 2026
// Licensed under the same terms as Mozc.

#ifndef MOZC_SESSION_ZENZ_RECENT_COMMIT_CONTEXT_H_
#define MOZC_SESSION_ZENZ_RECENT_COMMIT_CONTEXT_H_

#include <cstddef>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"

namespace mozc {
namespace session {

// Mozlumi: text this session committed recently, used as Zenz's left context
// when the application does not provide its own surrounding text (some chat
// apps never answer the request). It lives in memory only and is never
// written anywhere. It is forgotten after kLifetime without input, or when
// the cursor may have moved.
class ZenzRecentCommitContext {
 public:
  static constexpr size_t kMaxChars = 64;
  static constexpr absl::Duration kLifetime = absl::Minutes(5);

  // Appends committed text, keeping the last kMaxChars characters.
  void OnCommit(absl::string_view text, absl::Time now);
  // Enter that the application received (for example, sending a message).
  void OnLineBreak(absl::Time now);
  // Backspace that the application received: drops the last character.
  void OnBackspace(absl::Time now);
  void Clear();

  // Empty when nothing is remembered or it has expired.
  absl::string_view Get(absl::Time now) const;

 private:
  void Trim();

  std::string text_;
  absl::Time last_update_ = absl::InfinitePast();
};

}  // namespace session
}  // namespace mozc

#endif  // MOZC_SESSION_ZENZ_RECENT_COMMIT_CONTEXT_H_
