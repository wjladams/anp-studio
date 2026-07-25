#include "cppanp/pairwise.hpp"

#include <gtest/gtest.h>

using cppanp::PairwiseJudgments;

TEST(PairwiseTest, TwoThreeSixPriorities) {
  PairwiseJudgments pw({"A", "B", "C"});
  pw.set_comparison("A", "B", 2.0);
  pw.set_comparison("A", "C", 6.0);
  pw.set_comparison("B", "C", 3.0);

  const auto p = pw.priorities();
  EXPECT_NEAR(p[0], 0.6, 1e-9);
  EXPECT_NEAR(p[1], 0.3, 1e-9);
  EXPECT_NEAR(p[2], 0.1, 1e-9);
  EXPECT_NEAR(pw.consistency_ratio(), 0.0, 1e-9);
}

TEST(PairwiseTest, ReciprocalFill) {
  PairwiseJudgments pw({"X", "Y"});
  pw.set_comparison("X", "Y", 4.0);
  EXPECT_DOUBLE_EQ(pw.comparison("X", "Y"), 4.0);
  EXPECT_DOUBLE_EQ(pw.comparison("Y", "X"), 0.25);
}

TEST(PairwiseTest, AddAlternativeExpandsMatrix) {
  PairwiseJudgments pw({"A", "B"});
  pw.set_comparison("A", "B", 2.0);
  pw.add_alternative("C");
  EXPECT_EQ(pw.size(), 3u);
  EXPECT_DOUBLE_EQ(pw.comparison("A", "B"), 2.0);
  EXPECT_DOUBLE_EQ(pw.comparison("C", "C"), 1.0);
  EXPECT_DOUBLE_EQ(pw.comparison("A", "C"), 0.0);
}

TEST(PairwiseTest, IncompleteUsesHarker) {
  PairwiseJudgments pw({"A", "B", "C"});
  pw.set_comparison("A", "B", 2.0);
  pw.set_comparison("B", "C", 3.0);
  // A vs C left incomplete (0).
  const auto p = pw.priorities();
  EXPECT_NEAR(p.sum(), 1.0, 1e-12);
  EXPECT_GT(p[0], p[2]);
}

TEST(PairwiseTest, SingleAlternativeIsOne) {
  PairwiseJudgments pw({"Only"});
  const auto p = pw.priorities();
  EXPECT_EQ(p.size(), 1u);
  EXPECT_DOUBLE_EQ(p[0], 1.0);
}
