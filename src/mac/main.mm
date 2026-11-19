// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <InputMethodKit/InputMethodKit.h>

#import "mac/mozc_imk_input_controller.h"
#import "mac/renderer_receiver.h"

#include <unistd.h>

#include <memory>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "base/const.h"
#include "base/init_mozc.h"
#include "base/run_level.h"
#include "client/client.h"
#include "config/stats_config_util.h"

ABSL_FLAG(bool, register_input_source, false,
          "Register and enable the input source in the system, then exit.");
ABSL_FLAG(bool, reactivate_input_source, false,
          "With --register_input_source: on a fresh installation, disable the "
          "input source and its modes first if they are enabled, then enable "
          "them again, so that they show in the input menu without a logout.");

namespace {

// Writes one line for the installer log (postinstall passes it through).
void RegisterLog(NSString *message) {
  printf("Mozlumi register: %s\n", message.UTF8String);
  fflush(stdout);
}

bool IsSourceEnabled(TISInputSourceRef source) {
  CFBooleanRef enabled =
      (CFBooleanRef)TISGetInputSourceProperty(source, kTISPropertyInputSourceIsEnabled);
  return enabled != nullptr && CFBooleanGetValue(enabled);
}

// The input method itself (ID == bundle ID) and its modes (bundle ID + ".").
// The input menu lists the modes, so the modes must be enabled as well.
NSArray *FindOwnSources(NSString *bundleID) {
  NSMutableArray *result = [NSMutableArray array];
  NSArray *sourceList = CFBridgingRelease(TISCreateInputSourceList(nullptr, true));
  NSString *modePrefix = [bundleID stringByAppendingString:@"."];
  for (id object in sourceList) {
    TISInputSourceRef source = (__bridge TISInputSourceRef)object;
    NSString *sourceID =
        (__bridge NSString *)TISGetInputSourceProperty(source, kTISPropertyInputSourceID);
    if ([sourceID isEqualToString:bundleID] || [sourceID hasPrefix:modePrefix]) {
      [result addObject:object];
    }
  }
  return result;
}

NSString *SourceID(id object) {
  return (__bridge NSString *)TISGetInputSourceProperty((__bridge TISInputSourceRef)object,
                                                        kTISPropertyInputSourceID);
}

// Registers the input source bundle with Text Input Services (TIS) and
// enables it with its modes.  An already enabled input source is left alone
// (no permission prompt, the user's active input source is kept) unless
// `reactivate` is set.  This entry point is intended for the installed app
// executable.
void RegisterInputSource(bool reactivate) {
  NSBundle *bundle = [NSBundle mainBundle];
  NSString *bundleID = [bundle bundleIdentifier];
  CFURLRef bundleURL = (__bridge CFURLRef)[bundle bundleURL];
  if (bundleID == nil || bundleURL == nullptr) {
    RegisterLog(@"no bundle identifier or URL");
    return;
  }
  OSStatus status = TISRegisterInputSource(bundleURL);
  RegisterLog([NSString stringWithFormat:@"register status=%d reactivate=%d uid=%d",
                                         (int)status, reactivate ? 1 : 0, (int)getuid()]);

  // Registration may take a moment before the sources are listed.
  NSArray *sources = FindOwnSources(bundleID);
  for (int i = 0; i < 50 && sources.count == 0; ++i) {
    [NSThread sleepForTimeInterval:0.1];
    sources = FindOwnSources(bundleID);
  }
  RegisterLog([NSString stringWithFormat:@"found %lu sources", (unsigned long)sources.count]);
  if (sources.count == 0) {
    return;
  }

  bool anyEnabled = false;
  for (id object in sources) {
    bool enabled = IsSourceEnabled((__bridge TISInputSourceRef)object);
    anyEnabled = anyEnabled || enabled;
    RegisterLog([NSString stringWithFormat:@"before %@ enabled=%d", SourceID(object),
                                           enabled ? 1 : 0]);
  }
  if (anyEnabled && !reactivate) {
    RegisterLog(@"already enabled; nothing changed");
    return;
  }
  if (anyEnabled) {
    // Like removing it in System Settings before adding it again.
    for (id object in sources) {
      TISInputSourceRef source = (__bridge TISInputSourceRef)object;
      if (IsSourceEnabled(source)) {
        status = TISDisableInputSource(source);
        RegisterLog([NSString stringWithFormat:@"disable %@ status=%d", SourceID(object),
                                               (int)status]);
      }
    }
  }

  // Enable the input method first, then its modes.
  NSArray *ordered = [sources sortedArrayUsingComparator:^NSComparisonResult(id a, id b) {
    return [@(SourceID(a).length) compare:@(SourceID(b).length)];
  }];
  id hiragana = nil;
  for (id object in ordered) {
    status = TISEnableInputSource((__bridge TISInputSourceRef)object);
    RegisterLog([NSString stringWithFormat:@"enable %@ status=%d", SourceID(object),
                                           (int)status]);
    if ([SourceID(object) isEqualToString:[bundleID stringByAppendingString:@".base"]]) {
      hiragana = object;
    }
  }
  id toSelect = hiragana != nil ? hiragana : ordered.firstObject;
  status = TISSelectInputSource((__bridge TISInputSourceRef)toSelect);
  RegisterLog([NSString stringWithFormat:@"select %@ status=%d", SourceID(toSelect),
                                         (int)status]);

  for (id object in FindOwnSources(bundleID)) {
    RegisterLog([NSString stringWithFormat:@"after %@ enabled=%d", SourceID(object),
                                           IsSourceEnabled((__bridge TISInputSourceRef)object)
                                               ? 1
                                               : 0]);
  }
}

}  // namespace

int main(int argc, char *argv[]) {
  mozc::InitMozc(argv[0], &argc, &argv);

  if (absl::GetFlag(FLAGS_register_input_source)) {
    RegisterInputSource(absl::GetFlag(FLAGS_reactivate_input_source));
    return 0;
  }

  if (!mozc::RunLevel::IsValidClientRunLevel()) {
    return -1;
  }

  // Initialize imkServer
  NSBundle *bundle = [NSBundle mainBundle];
  NSDictionary *infoDictionary = [bundle infoDictionary];
  NSString *connectionName = [infoDictionary objectForKey:@"InputMethodConnectionName"];
  IMKServer *imkServer = [[IMKServer alloc] initWithName:connectionName
                                        bundleIdentifier:[bundle bundleIdentifier]];
  if (!imkServer) {
    LOG(FATAL) << mozc::kProductNameInEnglish << " failed to initialize";
    return -1;
  }
  DLOG(INFO) << mozc::kProductNameInEnglish << " initialized";

  NSString *rendererConnectionName = @kMozlumiMacAppName "_Renderer_Connection";
  RendererReceiver *rendererReceiver =
      [[RendererReceiver alloc] initWithName:rendererConnectionName];
  [MozcImkInputController setGlobalRendererReceiver:rendererReceiver];

  // Start the converter server at this time explicitly to prevent the
  // slow-down of the response for initial key event.
  {
    std::unique_ptr<mozc::client::Client> client(new mozc::client::Client);
    client->PingServer();
  }
  NSApplicationMain(argc, (const char **)argv);
  return 0;
}
