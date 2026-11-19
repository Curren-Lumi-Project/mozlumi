#include "session/zenz_typo_decision.h"

#include <string>
#include <vector>

#include "testing/gunit.h"

namespace mozc {
namespace session {
namespace {

ZenzTypoScoredFix Fix(const char* key, bool offered, double logprob,
                      float typo_cost = 1.0f, bool scored = true) {
  ZenzTypoScoredFix fix;
  fix.key = key;
  fix.mozc_value = std::string("M:") + key;
  fix.zenz_value = std::string("Z:") + key;
  fix.edit = "e";
  fix.typo_cost = typo_cost;
  fix.offered = offered;
  fix.scored = scored;
  fix.logprob = logprob;
  return fix;
}

TEST(ZenzTypoDecisionTest, KeepsOfferPreferredByZenz) {
  // Typed: -30. The offer scores -20 (better): kept with Mozc's value.
  const std::vector<ZenzTypoChoice> choices =
      DecideZenzTypoChoices(true, -30.0, {Fix("a", true, -20.0)});
  ASSERT_EQ(choices.size(), 1);
  EXPECT_EQ(choices[0].key, "a");
  EXPECT_EQ(choices[0].value, "M:a");
  EXPECT_FALSE(choices[0].from_zenz);
}

TEST(ZenzTypoDecisionTest, DropsOfferNotPreferredByZenz) {
  EXPECT_TRUE(
      DecideZenzTypoChoices(true, -30.0, {Fix("a", true, -31.0)}).empty());
}

TEST(ZenzTypoDecisionTest, AddsClearZenzPick) {
  // gain 20 - 4 * 1 = 16 > 6: added with Zenz's value.
  std::vector<ZenzTypoChoice> choices =
      DecideZenzTypoChoices(true, -40.0, {Fix("b", false, -20.0)});
  ASSERT_EQ(choices.size(), 1);
  EXPECT_EQ(choices[0].value, "Z:b");
  EXPECT_TRUE(choices[0].from_zenz);

  // gain 9 - 4 * 1 = 5 <= 6: not added.
  choices = DecideZenzTypoChoices(true, -29.0, {Fix("b", false, -20.0)});
  EXPECT_TRUE(choices.empty());
}

TEST(ZenzTypoDecisionTest, OrdersByGainAndLimitsToThree) {
  const std::vector<ZenzTypoChoice> choices = DecideZenzTypoChoices(
      true, -50.0,
      {Fix("a", true, -40.0), Fix("b", true, -30.0), Fix("c", false, -20.0),
       Fix("d", true, -45.0)});
  ASSERT_EQ(choices.size(), 3);
  EXPECT_EQ(choices[0].key, "c");
  EXPECT_EQ(choices[1].key, "b");
  EXPECT_EQ(choices[2].key, "a");
}

TEST(ZenzTypoDecisionTest, UnscoredKeepsMozcOffers) {
  // The typed reading was not scored: Mozc's offers as they are.
  std::vector<ZenzTypoChoice> choices = DecideZenzTypoChoices(
      false, 0.0, {Fix("a", true, 0.0), Fix("b", false, 0.0)});
  ASSERT_EQ(choices.size(), 1);
  EXPECT_EQ(choices[0].key, "a");

  // A fix without a score keeps Mozc's decision, after the scored ones.
  choices = DecideZenzTypoChoices(
      true, -30.0,
      {Fix("a", true, 0.0, 1.0f, /*scored=*/false), Fix("b", true, -10.0)});
  ASSERT_EQ(choices.size(), 2);
  EXPECT_EQ(choices[0].key, "b");
  EXPECT_EQ(choices[1].key, "a");
}

}  // namespace
}  // namespace session
}  // namespace mozc
