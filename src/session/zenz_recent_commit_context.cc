// Copyright 2026
// Licensed under the same terms as Mozc.

#include "session/zenz_recent_commit_context.h"

#include <string>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "base/util.h"

namespace mozc {
namespace session {

void ZenzRecentCommitContext::OnCommit(absl::string_view text,
                                       absl::Time now) {
  if (text.empty()) {
    return;
  }
  if (now - last_update_ > kLifetime) {
    text_.clear();
  }
  text_.append(text.data(), text.size());
  last_update_ = now;
  Trim();
}

void ZenzRecentCommitContext::OnLineBreak(absl::Time now) {
  if (text_.empty() || now - last_update_ > kLifetime) {
    Clear();
    return;
  }
  if (text_.back() != '\n') {
    text_.push_back('\n');
  }
  last_update_ = now;
  Trim();
}

void ZenzRecentCommitContext::OnBackspace(absl::Time now) {
  if (text_.empty() || now - last_update_ > kLifetime) {
    Clear();
    return;
  }
  const size_t len = Util::CharsLen(text_);
  text_ = std::string(Util::Utf8SubString(text_, 0, len - 1));
  last_update_ = now;
}

void ZenzRecentCommitContext::Clear() {
  text_.clear();
  last_update_ = absl::InfinitePast();
}

absl::string_view ZenzRecentCommitContext::Get(absl::Time now) const {
  if (text_.empty() || now - last_update_ > kLifetime) {
    return absl::string_view();
  }
  return text_;
}

void ZenzRecentCommitContext::Trim() {
  const size_t len = Util::CharsLen(text_);
  if (len > kMaxChars) {
    text_ = std::string(Util::Utf8SubString(text_, len - kMaxChars, kMaxChars));
  }
}

}  // namespace session
}  // namespace mozc
