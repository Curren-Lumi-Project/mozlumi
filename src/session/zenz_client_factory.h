#ifndef MOZC_SESSION_ZENZ_CLIENT_FACTORY_H_
#define MOZC_SESSION_ZENZ_CLIENT_FACTORY_H_

#include <memory>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_OSX
#include <sys/types.h>
#endif  // TARGET_OS_OSX
#endif  // __APPLE__

namespace mozc {
namespace session {

class ZenzClient;

// Creates the platform transport used by ZenzLiveCorrector.
//
// This factory keeps Session independent from concrete IPC transports.  The
// Windows uses the existing named-pipe client. macOS uses a local Unix-domain
// socket client. Other platforms retain the unavailable named-pipe fallback
// until a platform transport is explicitly implemented.
std::unique_ptr<ZenzClient> CreateZenzClient();

// For tests: while disabled, CreateZenzClient() returns nullptr (the AI
// requests fail at once), so that unit tests never talk to, or start, the
// Zenz scorer of the Mozlumi installed on the machine.
void SetZenzClientDisabledForTest(bool disabled);

#if defined(__APPLE__) && TARGET_OS_OSX
namespace internal {
// Starts the asynchronous waitpid() reaper for a child owned by this process.
// Exposed for the macOS process-lifecycle regression tests; callers must pass
// only a positive PID returned by a successful child spawn.
void ReapScorerAsync(pid_t pid);
}  // namespace internal
#endif  // __APPLE__ && TARGET_OS_OSX

}  // namespace session
}  // namespace mozc

#endif  // MOZC_SESSION_ZENZ_CLIENT_FACTORY_H_
