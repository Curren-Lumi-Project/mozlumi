// Copyright 2026
// Licensed under the same terms as Mozc.

#ifndef MOZC_MAC_DEBUG_LOG_EXPORT_H_
#define MOZC_MAC_DEBUG_LOG_EXPORT_H_

#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace mozc {
namespace mac {

// Mozlumi (1.3.4): exports the debug mode logs for the developer.
//
// The logs in the logging directory (zenz_debug.log, candidate_position.log
// and their ".1" files) are put together in one text, encrypted for the
// developer's public key (mozlumi_debug_public_key.h) and saved as one file,
// which the user sends by hand (by mail or DM).  The input method never sends
// it: Mozlumi accesses the network only for the update check.
//
// Encryption: ECIES on P-256 with AES-GCM
// (kSecKeyAlgorithmECIESEncryptionCofactorVariableIVX963SHA256AESGCM of the
// Security framework).  Only the private key on the developer's Mac can read
// the file (tools/mozlumi_debug_log/decrypt.swift).

// The first line of an exported file.
inline constexpr absl::string_view kDebugLogFileMagic = "MOZLUMI-DEBUG-LOG 1\n";

// The log files, oldest first.
inline constexpr absl::string_view kDebugLogFileNames[] = {
    "zenz_debug.log.1",
    "zenz_debug.log",
    "candidate_position.log.1",
    "candidate_position.log",
};

// Puts "Mozlumi debug log\n", |header| and the log files in |log_dir|
// together in |bundle|.  Returns false when none of the log files has
// anything.
bool BuildDebugLogBundle(const std::string &log_dir, absl::string_view header,
                         std::string *bundle);

// Encrypts |plaintext| for |public_key| (P-256, ANSI X9.63 uncompressed, 65
// bytes) and stores kDebugLogFileMagic followed by the encrypted data in
// |file_content|.  Returns false when |public_key| is not a valid key or the
// encryption fails.
bool EncryptDebugLogBundle(absl::Span<const uint8_t> public_key,
                           absl::string_view plaintext,
                           std::string *file_content);

}  // namespace mac
}  // namespace mozc

#endif  // MOZC_MAC_DEBUG_LOG_EXPORT_H_
