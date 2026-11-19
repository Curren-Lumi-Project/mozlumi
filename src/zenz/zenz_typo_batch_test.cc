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
