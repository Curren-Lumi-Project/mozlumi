// Copyright 2026
// Licensed under the same terms as Mozc.

#ifndef MOZC_SESSION_ZENZ_RECENT_COMMIT_STORE_H_
#define MOZC_SESSION_ZENZ_RECENT_COMMIT_STORE_H_

#include <cstddef>
#include <functional>
#include <map>
#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "session/zenz_recent_commit_context.h"

namespace mozc {
namespace session {

// Mozlumi: the recently committed text of all the input sessions in this
// process, kept by application (the client tells which application an input
// belongs to, e.g. the macOS bundle identifier). It is Zenz's left context
// for applications that do not report the text before the cursor.
//
// - By application: the text committed in that application only. It stays
//   when the focus moves to another application, and is used again when the
//   user comes back.
// - Across applications: the text committed in any application, in order.
//   Used only when the user turns on zenz_context_across_apps.
//
// Memory only, never written to a file. The text expires after
// ZenzRecentCommitContext::kLifetime without input.
class ZenzRecentCommitStore {
 public:
  // The number of applications kept; the least recently used one is dropped.
  static constexpr size_t kMaxApps = 32;

  // The process-wide store shared by the sessions.
  static ZenzRecentCommitStore* Get();

  ZenzRecentCommitStore() = default;
  ZenzRecentCommitStore(const ZenzRecentCommitStore&) = delete;
  ZenzRecentCommitStore& operator=(const ZenzRecentCommitStore&) = delete;

  // `app` must not be empty (then nothing happens). Each event updates both
  // that application's text and the text across applications.
  void OnCommit(absl::string_view app, absl::string_view text,
                absl::Time now);
  void OnLineBreak(absl::string_view app, absl::Time now);
  void OnBackspace(absl::string_view app, absl::Time now);
  // The text before the cursor is no longer known (cursor keys, shortcuts,
  // password fields): forgets that application's text and the text across
  // applications.
  void Clear(absl::string_view app);

  // Empty when nothing is remembered or it has expired.
  std::string GetContext(absl::string_view app, bool across_apps,
                         absl::Time now) const;

  // For tests.
  void ClearAll();
  size_t AppCountForTest() const;

 private:
  struct AppEntry {
    ZenzRecentCommitContext context;
    absl::Time last_used = absl::InfinitePast();
  };
  AppEntry& EntryLocked(absl::string_view app, absl::Time now)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable absl::Mutex mutex_;
  std::map<std::string, AppEntry, std::less<>> apps_ ABSL_GUARDED_BY(mutex_);
  ZenzRecentCommitContext across_apps_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace session
}  // namespace mozc

#endif  // MOZC_SESSION_ZENZ_RECENT_COMMIT_STORE_H_
