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

#include "prediction/roman_typo_corrector.h"

#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "composer/composer.h"
#include "composer/query.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/dictionary_token.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "request/options.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace prediction {
namespace {

using ::mozc::dictionary::MockDictionary;
using ::testing::_;
using ::testing::Return;

class RomanTypoCorrectorTest : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override {
    config::ConfigHandler::GetDefaultConfig(&config_);
    config_.set_use_typing_correction(true);
    table_ = std::make_shared<composer::Table>();
    table_->InitializeWithRequestAndConfig(request_, config_);
    ON_CALL(dictionary_, HasKey(_)).WillByDefault(Return(false));
  }

  ConversionRequest MakeRequest(absl::string_view romaji,
                                ConversionRequest::RequestType type =
                                    ConversionRequest::PREDICTION) {
    composer_ = std::make_unique<composer::Composer>(table_, request_, config_);
    composer_->InsertCharacter(std::string(romaji));
    return ConversionRequestBuilder()
        .SetComposer(*composer_)
        .SetRequest(request_)
        .SetConfig(config_)
        .SetRequestType(type)
        .SetKey(composer_->GetQueryForPrediction())
        .Build();
  }

  commands::Request request_;
  config::Config config_;
  std::shared_ptr<composer::Table> table_;
  std::unique_ptr<composer::Composer> composer_;
  ::testing::NiceMock<MockDictionary> dictionary_;
};

TEST_F(RomanTypoCorrectorTest, CorrectsSunkaiToShinkai) {
  ON_CALL(dictionary_, HasKey(absl::string_view("しんかい")))
      .WillByDefault(Return(true));
  RomanTypoCorrector corrector(dictionary_);
  const std::vector<composer::TypeCorrectedQuery> queries =
      corrector.Correct(MakeRequest("sunkai"));
  ASSERT_FALSE(queries.empty());
  EXPECT_EQ(queries[0].correction, "しんかい");
  EXPECT_EQ(queries[0].type, composer::TypeCorrectedQuery::CORRECTION);
  EXPECT_LT(queries[0].bias, 0.0f);
}

TEST_F(RomanTypoCorrectorTest, CorrectsInvalidRomaji) {
  // "kqisya" leaves "kq" unconverted ("kqいしゃ").
  ON_CALL(dictionary_, HasKey(absl::string_view("かいしゃ")))
      .WillByDefault(Return(true));
  RomanTypoCorrector corrector(dictionary_);
  const std::vector<composer::TypeCorrectedQuery> queries =
      corrector.Correct(MakeRequest("kqisya"));
  ASSERT_FALSE(queries.empty());
  EXPECT_EQ(queries[0].correction, "かいしゃ");
}

TEST_F(RomanTypoCorrectorTest, DoesNotCorrectPrefixOfValidWord) {
  // "すんか" is a prefix of a dictionary word: the user may still be typing.
  ON_CALL(dictionary_, HasKey(absl::string_view("しんか")))
      .WillByDefault(Return(true));
  ON_CALL(dictionary_, LookupPredictive(absl::string_view("すんか"), _, _))
      .WillByDefault([](absl::string_view key, const ConversionOptions&,
                        dictionary::DictionaryInterface::Callback* callback) {
        dictionary::Token token;
        token.key = std::string(key);
        token.value = "寸暇";
        callback->OnToken(key, key, token);
      });
  RomanTypoCorrector corrector(dictionary_);
  EXPECT_TRUE(corrector.Correct(MakeRequest("sunka")).empty());
}

TEST_F(RomanTypoCorrectorTest, DoesNotCorrectValidWord) {
  EXPECT_CALL(dictionary_, HasKey(absl::string_view("しんかい")))
      .WillOnce(Return(true));
  RomanTypoCorrector corrector(dictionary_);
  EXPECT_TRUE(corrector.Correct(MakeRequest("sinkai")).empty());
}

TEST_F(RomanTypoCorrectorTest, DisabledByConfig) {
  config_.set_use_typing_correction(false);
  EXPECT_CALL(dictionary_, HasKey(_)).Times(0);
  RomanTypoCorrector corrector(dictionary_);
  EXPECT_TRUE(corrector.Correct(MakeRequest("sunkai")).empty());
}

TEST_F(RomanTypoCorrectorTest, DisabledForConversion) {
  EXPECT_CALL(dictionary_, HasKey(_)).Times(0);
  RomanTypoCorrector corrector(dictionary_);
  EXPECT_TRUE(
      corrector.Correct(MakeRequest("sunkai", ConversionRequest::CONVERSION))
          .empty());
}

TEST_F(RomanTypoCorrectorTest, ReturnsAtMostMaxQueries) {
  ON_CALL(dictionary_, HasKey(_)).WillByDefault(Return(true));
  // The original key itself must not be a word.
  ON_CALL(dictionary_, HasKey(absl::string_view("すんかい")))
      .WillByDefault(Return(false));
  RomanTypoCorrector corrector(dictionary_);
  EXPECT_EQ(corrector.Correct(MakeRequest("sunkai")).size(),
            RomanTypoCorrector::kMaxQueries);
}

}  // namespace
}  // namespace prediction
}  // namespace mozc
