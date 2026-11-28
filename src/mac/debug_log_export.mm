// Copyright 2026
// Licensed under the same terms as Mozc.

#import "mac/debug_log_export.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include <cstdint>
#include <fstream>
#include <ios>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace mozc {
namespace mac {
namespace {

// A log file is renamed to ".1" when it grows over 1 MiB.  Read a little
// more, in case it grew in the meantime.
constexpr std::streamsize kMaxLogFileBytes = 2 << 20;

// Reads the file at |path| into |content|.  Returns false when it cannot be
// opened (usually because it does not exist).
bool ReadLogFile(const std::string &path, std::string *content) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  content->resize(kMaxLogFileBytes);
  in.read(content->data(), kMaxLogFileBytes);
  content->resize(static_cast<size_t>(in.gcount()));
  return true;
}

}  // namespace

bool BuildDebugLogBundle(const std::string &log_dir, absl::string_view header,
                         std::string *bundle) {
  bundle->clear();
  absl::StrAppend(bundle, "Mozlumi debug log\n", header);
  bool has_log = false;
  for (const absl::string_view name : kDebugLogFileNames) {
    std::string content;
    if (!ReadLogFile(absl::StrCat(log_dir, "/", name), &content) ||
        content.empty()) {
      continue;
    }
    has_log = true;
    absl::StrAppend(bundle, "\n===== ", name, " (", content.size(),
                    " bytes) =====\n", content);
    if (content.back() != '\n') {
      bundle->push_back('\n');
    }
  }
  return has_log;
}

bool EncryptDebugLogBundle(absl::Span<const uint8_t> public_key,
                           absl::string_view plaintext,
                           std::string *file_content) {
  file_content->clear();
  @autoreleasepool {
    NSData *key_data = [NSData dataWithBytes:public_key.data()
                                      length:public_key.size()];
    NSDictionary *attributes = @{
      (__bridge id)kSecAttrKeyType : (__bridge id)kSecAttrKeyTypeECSECPrimeRandom,
      (__bridge id)kSecAttrKeyClass : (__bridge id)kSecAttrKeyClassPublic,
      (__bridge id)kSecAttrKeySizeInBits : @256,
    };
    CFErrorRef error = nullptr;
    SecKeyRef key = SecKeyCreateWithData((__bridge CFDataRef)key_data,
                                         (__bridge CFDictionaryRef)attributes,
                                         &error);
    if (key == nullptr) {
      if (error != nullptr) {
        CFRelease(error);
      }
      return false;
    }
    const SecKeyAlgorithm algorithm =
        kSecKeyAlgorithmECIESEncryptionCofactorVariableIVX963SHA256AESGCM;
    if (!SecKeyIsAlgorithmSupported(key, kSecKeyOperationTypeEncrypt,
                                    algorithm)) {
      CFRelease(key);
      return false;
    }
    NSData *plain = [NSData dataWithBytes:plaintext.data()
                                   length:plaintext.size()];
    CFDataRef encrypted = SecKeyCreateEncryptedData(
        key, algorithm, (__bridge CFDataRef)plain, &error);
    CFRelease(key);
    if (encrypted == nullptr) {
      if (error != nullptr) {
        CFRelease(error);
      }
      return false;
    }
    file_content->assign(kDebugLogFileMagic.data(), kDebugLogFileMagic.size());
    file_content->append(
        reinterpret_cast<const char *>(CFDataGetBytePtr(encrypted)),
        static_cast<size_t>(CFDataGetLength(encrypted)));
    CFRelease(encrypted);
  }
  return true;
}

}  // namespace mac
}  // namespace mozc
