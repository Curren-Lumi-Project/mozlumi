#ifndef MOZC_ZENZ_ZENZ_TYPO_BATCH_H_
#define MOZC_ZENZ_ZENZ_TYPO_BATCH_H_

#include <string>
#include <string_view>
#include <vector>

namespace mozc {
namespace zenz {

// Mozlumi: a batch of scoring jobs sent in the prompt of one ZNZ1 request, so
// that the wire protocol does not change.
//
// Each job shares the prompt prefix (context and the reading marker) and has
// a forced part (the reading in katakana and the output marker). The scorer
// forces the forced part with a grammar, lets Zenz generate the conversion
// after it, and returns the log-probabilities of both parts:
//
//   log P(forced | prefix) and log P(value | prefix + forced)
//
// Request prompt:  kZenzTypoBatchMarker prefix (kZenzTypoBatchSeparator forced)*
//
// Since Mozlumi 1.3.3 Build 57 a job may have its own prefix instead of the
// shared one (MakeZenzTypoBatchJob): "job_prefix kZenzTypoBatchJobPrefixEnd
// forced". Such a batch begins with kZenzTypoBatchMarkerV2, so that an older
// scorer does not take it as a batch.
// Response value:  one line per job, "ok \t forced_lp \t value_lp \t value",
//                  jobs separated by '\n'.

// U+EE0F followed by a version tag. Zenz prompts never start with U+EE0F.
inline constexpr std::string_view kZenzTypoBatchMarker = "\xEE\xB8\x8FZTB1";
inline constexpr std::string_view kZenzTypoBatchMarkerV2 = "\xEE\xB8\x8FZTB2";
// U+EE0D. Ends the own prefix of a job.
inline constexpr std::string_view kZenzTypoBatchJobPrefixEnd = "\xEE\xB8\x8D";
// U+EE0E.
inline constexpr std::string_view kZenzTypoBatchSeparator = "\xEE\xB8\x8E";

struct ZenzTypoBatchResult {
  bool ok = false;
  double forced_logprob = 0.0;
  double value_logprob = 0.0;
  std::string value;
};

std::string EncodeZenzTypoBatchPrompt(std::string_view prefix,
                                      const std::vector<std::string>& forced);

// A job scored after `job_prefix` instead of the shared prefix.
std::string MakeZenzTypoBatchJob(std::string_view job_prefix,
                                 std::string_view forced);

// The prompt and the forced part of a decoded job: its own prefix when it has
// one, otherwise `shared_prefix`.
void SplitZenzTypoBatchJob(std::string_view shared_prefix, std::string_view job,
                           std::string* prompt, std::string* forced);

// Returns false when `prompt` is not a batch.
bool DecodeZenzTypoBatchPrompt(std::string_view prompt, std::string* prefix,
                               std::vector<std::string>* forced);

std::string EncodeZenzTypoBatchResponse(
    const std::vector<ZenzTypoBatchResult>& results);

// Returns false when the response is malformed.
bool DecodeZenzTypoBatchResponse(std::string_view value,
                                 std::vector<ZenzTypoBatchResult>* results);

}  // namespace zenz
}  // namespace mozc

#endif  // MOZC_ZENZ_ZENZ_TYPO_BATCH_H_
