// Copyright 2026
// Licensed under the same terms as Mozc.

#include "session/zenz_recent_commit_store.h"

#include <cstddef>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"

namespace mozc {
namespace session {

ZenzRecentCommitStore* ZenzRecentCommitStore::Get() {
  static ZenzRecentCommitStore* const store = new ZenzRecentCommitStore();
  return store;
}

ZenzRecentCommitStore::AppEntry& ZenzRecentCommitStore::EntryLocked(
    absl::string_view app, absl::Time now) {
  auto it = apps_.find(app);
  if (it == apps_.end()) {
    if (apps_.size() >= kMaxApps) {
      auto oldest = apps_.begin();
      for (auto i = apps_.begin(); i != apps_.end(); ++i) {
        if (i->second.last_used < oldest->second.last_used) {
          oldest = i;
        }
      }
      apps_.erase(oldest);
    }
    it = apps_.emplace(std::string(app), AppEntry()).first;
  }
  it->second.last_used = now;
  return it->second;
}

void ZenzRecentCommitStore::OnCommit(absl::string_view app,
                                     absl::string_view text, absl::Time now) {
  if (app.empty() || text.empty()) {
    return;
  }
  absl::MutexLock lock(&mutex_);
  EntryLocked(app, now).context.OnCommit(text, now);
  across_apps_.OnCommit(text, now);
}

void ZenzRecentCommitStore::OnLineBreak(absl::string_view app,
                                        absl::Time now) {
  if (app.empty()) {
    return;
  }
  absl::MutexLock lock(&mutex_);
  EntryLocked(app, now).context.OnLineBreak(now);
  across_apps_.OnLineBreak(now);
}

void ZenzRecentCommitStore::OnBackspace(absl::string_view app,
                                        absl::Time now) {
  if (app.empty()) {
    return;
  }
  absl::MutexLock lock(&mutex_);
  EntryLocked(app, now).context.OnBackspace(now);
  across_apps_.OnBackspace(now);
}

void ZenzRecentCommitStore::Clear(absl::string_view app) {
  if (app.empty()) {
    return;
  }
  absl::MutexLock lock(&mutex_);
  auto it = apps_.find(app);
  if (it != apps_.end()) {
    it->second.context.Clear();
  }
  across_apps_.Clear();
}

std::string ZenzRecentCommitStore::GetContext(absl::string_view app,
                                              bool across_apps,
                                              absl::Time now) const {
  if (app.empty()) {
    return std::string();
  }
  absl::MutexLock lock(&mutex_);
  if (across_apps) {
    return std::string(across_apps_.Get(now));
  }
  auto it = apps_.find(app);
  if (it == apps_.end()) {
    return std::string();
  }
  return std::string(it->second.context.Get(now));
}

void ZenzRecentCommitStore::ClearAll() {
  absl::MutexLock lock(&mutex_);
  apps_.clear();
  across_apps_.Clear();
}

size_t ZenzRecentCommitStore::AppCountForTest() const {
  absl::MutexLock lock(&mutex_);
  return apps_.size();
}

}  // namespace session
}  // namespace mozc
