// Copyright 2026
// Licensed under the same terms as Mozc.

#include "zenz_scorer/typo_batch_runner.h"

#include <string>
#include <vector>

#include "testing/gunit.h"
#include "zenz/zenz_typo_batch.h"

namespace mozc {
namespace zenz_scorer {
namespace {

using ::mozc::zenz::DecodeZenzTypoBatchResponse;
using ::mozc::zenz::MakeZenzTypoBatchJob;
using ::mozc::zenz::ZenzTypoBatchResult;

TEST(ZenzTypoBatchRunnerTest, Grammar) {
  EXPECT_EQ(ZenzBatchGrammar("a\"b\\c"),
            "root ::= \"a\\\"b\\\\c\" [^\\n\\r\\uee00-\\uee0f]*");
}

TEST(ZenzTypoBatchRunnerTest, ParsesTokenLogprobs) {
  // The same response as LlamaHttpClientTest.ParsesTokenLogprobs.
  const std::string json =
      R"({"index":0,"content":"キ今日","completion_probabilities":[)"
      R"({"id":436,"token":"キ","bytes":[227,130,173],"logprob":-4.17,)"
      R"("top_logprobs":[{"id":298,"token":"ア","bytes":[227,130,162],)"
      R"("logprob":-3.30}]},)"
      R"({"id":5,"token":"\"{x}","bytes":[34,123],"logprob":-0.5e0,)"
      R"("top_logprobs":[]}],"stop":true})";
  std::vector<ZenzBatchTokenLogprob> tokens;
  ASSERT_TRUE(ParseZenzCompletionTokenLogprobs(json, &tokens));
  ASSERT_EQ(tokens.size(), 2);
  EXPECT_EQ(tokens[0].bytes, 3);
  EXPECT_DOUBLE_EQ(tokens[0].logprob, -4.17);
  EXPECT_EQ(tokens[1].bytes, 2);
  EXPECT_DOUBLE_EQ(tokens[1].logprob, -0.5);

  EXPECT_FALSE(ParseZenzCompletionTokenLogprobs(R"({"content":"x"})", &tokens));
}

TEST(ZenzTypoBatchRunnerTest, RunsJobsWithTheirPrefixes) {
  const std::string prefix = "P";
  const std::vector<std::string> jobs = {
      "ドウキ\xEE\xB8\x81同期",
      MakeZenzTypoBatchJob("Q", "ドウキ\xEE\xB8\x81"),
  };
  std::vector<ZenzBatchCompletionRequest> requests;
  const auto complete = [&](const ZenzBatchCompletionRequest& request) {
    requests.push_back(request);
    ZenzBatchCompletionResult result;
    result.ok = true;
    if (request.prompt == "P") {
      // The forced part in two tokens, then the end.
      result.content = "ドウキ\xEE\xB8\x81同期";
      result.tokens = {{12, -1.0}, {6, -2.0}};
    } else {
      // The forced part, then Zenz's answer.
      result.content = "ドウキ\xEE\xB8\x81動悸 ";
      result.tokens = {{12, -1.5}, {6, -0.25}, {1, -0.5}};
    }
    return result;
  };
  size_t succeeded = 0;
  const std::string value =
      RunZenzTypoBatch(prefix, jobs, 1500, 64, complete, &succeeded);
  EXPECT_EQ(succeeded, 2u);
  ASSERT_EQ(requests.size(), 2u);
  EXPECT_EQ(requests[0].prompt, "P");
  EXPECT_EQ(requests[0].grammar,
            ZenzBatchGrammar("ドウキ\xEE\xB8\x81同期"));
  EXPECT_EQ(requests[0].n_predict, 64 + 6);
  EXPECT_EQ(requests[1].prompt, "Q");

  std::vector<ZenzTypoBatchResult> results;
  ASSERT_TRUE(DecodeZenzTypoBatchResponse(value, &results));
  ASSERT_EQ(results.size(), 2u);
  EXPECT_TRUE(results[0].ok);
  EXPECT_DOUBLE_EQ(results[0].forced_logprob, -3.0);
  EXPECT_DOUBLE_EQ(results[0].value_logprob, 0.0);
  EXPECT_EQ(results[0].value, "");
  EXPECT_TRUE(results[1].ok);
  EXPECT_DOUBLE_EQ(results[1].forced_logprob, -1.5);
  EXPECT_DOUBLE_EQ(results[1].value_logprob, -0.75);
  EXPECT_EQ(results[1].value, "動悸");
}

TEST(ZenzTypoBatchRunnerTest, FailedAndLateJobs) {
  const std::vector<std::string> jobs = {"キ", "キ"};
  int calls = 0;
  const auto fail = [&](const ZenzBatchCompletionRequest&) {
    ++calls;
    return ZenzBatchCompletionResult();
  };
  size_t succeeded = 9;
  std::vector<ZenzTypoBatchResult> results;
  ASSERT_TRUE(DecodeZenzTypoBatchResponse(
      RunZenzTypoBatch("P", jobs, 1500, 64, fail, &succeeded), &results));
  EXPECT_EQ(succeeded, 0u);
  EXPECT_EQ(calls, 2);
  ASSERT_EQ(results.size(), 2u);
  EXPECT_FALSE(results[0].ok);

  // No time: the jobs are not run.
  calls = 0;
  ASSERT_TRUE(DecodeZenzTypoBatchResponse(
      RunZenzTypoBatch("P", jobs, 0, 64, fail, nullptr), &results));
  EXPECT_EQ(calls, 0);
  ASSERT_EQ(results.size(), 2u);
  EXPECT_FALSE(results[1].ok);
}

}  // namespace
}  // namespace zenz_scorer
}  // namespace mozc
