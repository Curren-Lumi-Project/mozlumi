#include "zenz/zenz_typo_batch.h"

#include <string>
#include <vector>

#include "testing/gunit.h"

namespace mozc {
namespace zenz {
namespace {

TEST(ZenzTypoBatchTest, PromptRoundTrip) {
  const std::string prefix = "\xEE\xB8\x82猫が好き\xEE\xB8\x80";
  const std::vector<std::string> forced = {"キョウハ\xEE\xB8\x81",
                                           "キョウワ\xEE\xB8\x81"};
  const std::string prompt = EncodeZenzTypoBatchPrompt(prefix, forced);

  std::string decoded_prefix;
  std::vector<std::string> decoded_forced;
  ASSERT_TRUE(
      DecodeZenzTypoBatchPrompt(prompt, &decoded_prefix, &decoded_forced));
  EXPECT_EQ(decoded_prefix, prefix);
  EXPECT_EQ(decoded_forced, forced);
}

TEST(ZenzTypoBatchTest, JobPrefixRoundTrip) {
  const std::string prefix = "\xEE\xB8\x82前の文\xEE\xB8\x80";
  const std::string job_prefix = "\xEE\xB8\x82してたら\xEE\xB8\x80";
  const std::vector<std::string> forced = {
      "ドウキ\xEE\xB8\x81同期",
      MakeZenzTypoBatchJob(job_prefix, "ドウキ\xEE\xB8\x81動悸")};
  const std::string prompt = EncodeZenzTypoBatchPrompt(prefix, forced);
  // An older scorer does not take it as a batch.
  EXPECT_EQ(prompt.substr(0, kZenzTypoBatchMarkerV2.size()),
            kZenzTypoBatchMarkerV2);

  std::string decoded_prefix;
  std::vector<std::string> decoded_forced;
  ASSERT_TRUE(
      DecodeZenzTypoBatchPrompt(prompt, &decoded_prefix, &decoded_forced));
  EXPECT_EQ(decoded_prefix, prefix);
  ASSERT_EQ(decoded_forced, forced);

  std::string job_prompt;
  std::string job_forced;
  SplitZenzTypoBatchJob(decoded_prefix, decoded_forced[0], &job_prompt,
                        &job_forced);
  EXPECT_EQ(job_prompt, prefix);
  EXPECT_EQ(job_forced, "ドウキ\xEE\xB8\x81同期");
  SplitZenzTypoBatchJob(decoded_prefix, decoded_forced[1], &job_prompt,
                        &job_forced);
  EXPECT_EQ(job_prompt, job_prefix);
  EXPECT_EQ(job_forced, "ドウキ\xEE\xB8\x81動悸");
}

TEST(ZenzTypoBatchTest, PlainBatchKeepsTheFirstVersion) {
  const std::string prompt = EncodeZenzTypoBatchPrompt(
      "\xEE\xB8\x80", {"キョウハ\xEE\xB8\x81"});
  EXPECT_EQ(prompt.substr(0, kZenzTypoBatchMarker.size()),
            kZenzTypoBatchMarker);
}

TEST(ZenzTypoBatchTest, JobPrefixNeedsTheSecondVersion) {
  std::string prefix;
  std::vector<std::string> forced;
  const std::string job = MakeZenzTypoBatchJob("\xEE\xB8\x80", "キ");
  EXPECT_FALSE(DecodeZenzTypoBatchPrompt(
      std::string(kZenzTypoBatchMarker) + "p" +
          std::string(kZenzTypoBatchSeparator) + job,
      &prefix, &forced));
  // A job without a forced part.
  EXPECT_FALSE(DecodeZenzTypoBatchPrompt(
      std::string(kZenzTypoBatchMarkerV2) + "p" +
          std::string(kZenzTypoBatchSeparator) +
          MakeZenzTypoBatchJob("\xEE\xB8\x80", ""),
      &prefix, &forced));
}

TEST(ZenzTypoBatchTest, NormalPromptIsNotABatch) {
  std::string prefix;
  std::vector<std::string> forced;
  EXPECT_FALSE(DecodeZenzTypoBatchPrompt(
      "\xEE\xB8\x82\xEE\xB8\x80キョウ\xEE\xB8\x81", &prefix, &forced));
  // A batch without jobs is rejected.
  EXPECT_FALSE(DecodeZenzTypoBatchPrompt(
      std::string(kZenzTypoBatchMarker) + "prefix", &prefix, &forced));
}

TEST(ZenzTypoBatchTest, ResponseRoundTrip) {
  std::vector<ZenzTypoBatchResult> results(2);
  results[0].ok = true;
  results[0].forced_logprob = -12.5;
  results[0].value_logprob = -3.25;
  results[0].value = "今日は";
  results[1].ok = false;

  std::vector<ZenzTypoBatchResult> decoded;
  ASSERT_TRUE(DecodeZenzTypoBatchResponse(
      EncodeZenzTypoBatchResponse(results), &decoded));
  ASSERT_EQ(decoded.size(), 2);
  EXPECT_TRUE(decoded[0].ok);
  EXPECT_DOUBLE_EQ(decoded[0].forced_logprob, -12.5);
  EXPECT_DOUBLE_EQ(decoded[0].value_logprob, -3.25);
  EXPECT_EQ(decoded[0].value, "今日は");
  EXPECT_FALSE(decoded[1].ok);
  EXPECT_EQ(decoded[1].value, "");
}

TEST(ZenzTypoBatchTest, MalformedResponse) {
  std::vector<ZenzTypoBatchResult> decoded;
  EXPECT_FALSE(DecodeZenzTypoBatchResponse("", &decoded));
  EXPECT_FALSE(DecodeZenzTypoBatchResponse("1\t-1.0\t今日", &decoded));
  EXPECT_FALSE(DecodeZenzTypoBatchResponse("1\tx\t-1.0\t今日", &decoded));
}

}  // namespace
}  // namespace zenz
}  // namespace mozc
