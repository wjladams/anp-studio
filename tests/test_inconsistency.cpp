#include "cppanp/inconsistency.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <initializer_list>

using cppanp::ConsistencyOptions;
using cppanp::ConsistencyResult;
using cppanp::Matrix;

namespace {

Matrix make_matrix(std::size_t rows,
                   std::size_t cols,
                   std::initializer_list<double> values) {
  Matrix m(rows, cols);
  auto it = values.begin();
  for (std::size_t i = 0; i < rows; ++i) {
    for (std::size_t j = 0; j < cols; ++j) {
      m(i, j) = *it++;
    }
  }
  return m;
}

}  // namespace

TEST(InconsistencyTest, RandomIndexSaatyTable) {
  EXPECT_DOUBLE_EQ(cppanp::random_index(1), 0.0);
  EXPECT_DOUBLE_EQ(cppanp::random_index(2), 0.0);
  EXPECT_DOUBLE_EQ(cppanp::random_index(3), 0.52);
  EXPECT_DOUBLE_EQ(cppanp::random_index(4), 0.89);
  EXPECT_DOUBLE_EQ(cppanp::random_index(5), 1.12);
  EXPECT_DOUBLE_EQ(cppanp::random_index(6), 1.25);
  EXPECT_DOUBLE_EQ(cppanp::random_index(7), 1.35);
  EXPECT_DOUBLE_EQ(cppanp::random_index(8), 1.40);
  EXPECT_DOUBLE_EQ(cppanp::random_index(9), 1.45);
  EXPECT_DOUBLE_EQ(cppanp::random_index(10), 1.49);
  EXPECT_DOUBLE_EQ(cppanp::random_index(11), 1.51);
  EXPECT_DOUBLE_EQ(cppanp::random_index(12), 1.54);
  EXPECT_DOUBLE_EQ(cppanp::random_index(13), 1.56);
  EXPECT_DOUBLE_EQ(cppanp::random_index(14), 1.57);
  EXPECT_DOUBLE_EQ(cppanp::random_index(15), 1.58);
}

TEST(InconsistencyTest, RandomIndexLargeNUsesAlonsoLamata) {
  // Official CR uses RI ≈ 1.98*(n-2)/n for n > 15.
  // pyanp's inconsistency_divisor omitted the (n-1) factor here; we do not.
  EXPECT_DOUBLE_EQ(cppanp::random_index(16), 1.98 * 14.0 / 16.0);
  EXPECT_DOUBLE_EQ(cppanp::random_index(20), 1.98 * 18.0 / 20.0);
}

TEST(InconsistencyTest, ConsistencyIndexFormula) {
  EXPECT_DOUBLE_EQ(cppanp::consistency_index(3.0, 3), 0.0);
  EXPECT_NEAR(cppanp::consistency_index(3.104, 3), 0.052, 1e-12);
  EXPECT_DOUBLE_EQ(cppanp::consistency_index(5.0, 1), 0.0);
}

TEST(InconsistencyTest, ConsistencyRatioFormula) {
  // CR = CI / RI = 0.052 / 0.52 = 0.1
  EXPECT_NEAR(cppanp::consistency_ratio(3.104, 3), 0.1, 1e-12);
  EXPECT_DOUBLE_EQ(cppanp::consistency_ratio(2.0, 2), 0.0);
  EXPECT_DOUBLE_EQ(cppanp::consistency_ratio(1.0, 1), 0.0);
}

TEST(InconsistencyTest, ConsistentMatrixHasZeroInconsistency) {
  const Matrix m = make_matrix(
      3, 3,
      {1.0, 2.0, 6.0, 0.5, 1.0, 3.0, 1.0 / 6.0, 1.0 / 3.0, 1.0});

  ConsistencyOptions options;
  options.use_harker = false;
  const ConsistencyResult result = cppanp::consistency(m, options);

  EXPECT_TRUE(result.converged);
  EXPECT_NEAR(result.lambda_max, 3.0, 1e-9);
  EXPECT_NEAR(result.consistency_index, 0.0, 1e-9);
  EXPECT_NEAR(result.consistency_ratio, 0.0, 1e-9);
  EXPECT_DOUBLE_EQ(result.random_index, 0.52);
}

// The 2/3/6 matrix is the textbook illustration of perfect consistency: apple
// is preferred 2x over orange, orange 3x over banana, so apple must be 6x over
// banana. Priorities are 0.6 / 0.3 / 0.1 and lambda_max is exactly n.
TEST(InconsistencyTest, TwoThreeSixIsPerfectlyConsistent) {
  const Matrix m = make_matrix(
      3, 3,
      {1.0, 2.0, 6.0, 0.5, 1.0, 3.0, 1.0 / 6.0, 1.0 / 3.0, 1.0});

  for (const bool use_harker : {false, true}) {
    ConsistencyOptions options;
    options.use_harker = use_harker;
    const ConsistencyResult result = cppanp::consistency(m, options);

    EXPECT_TRUE(result.converged);
    EXPECT_NEAR(result.lambda_max, 3.0, 1e-9);
    EXPECT_NEAR(result.consistency_index, 0.0, 1e-9);
    EXPECT_NEAR(result.consistency_ratio, 0.0, 1e-9);

    EXPECT_NEAR(result.priority[0], 0.6, 1e-9);
    EXPECT_NEAR(result.priority[1], 0.3, 1e-9);
    EXPECT_NEAR(result.priority[2], 0.1, 1e-9);
  }
}

// Same story as above, but the apple/banana judgment is 4 instead of the
// consistent 6, which introduces a small, acceptable inconsistency.
// Cross-checked against numpy.linalg.eigvals.
TEST(InconsistencyTest, TwoThreeFourIsSlightlyInconsistent) {
  const Matrix m = make_matrix(
      3, 3, {1.0, 2.0, 4.0, 0.5, 1.0, 3.0, 0.25, 1.0 / 3.0, 1.0});

  ConsistencyOptions options;
  options.use_harker = false;
  const ConsistencyResult result = cppanp::consistency(m, options);

  EXPECT_NEAR(result.lambda_max, 3.018294707281254, 1e-9);
  EXPECT_NEAR(result.consistency_index, 0.00914735364062702, 1e-9);
  EXPECT_NEAR(result.consistency_ratio, 0.00914735364062702 / 0.52, 1e-9);

  EXPECT_NEAR(result.priority[0], 0.558424543097, 1e-9);
  EXPECT_NEAR(result.priority[1], 0.319618263935, 1e-9);
  EXPECT_NEAR(result.priority[2], 0.121957192968, 1e-9);

  EXPECT_LT(result.consistency_ratio, 0.1);
}

// Laptop criteria example (Price / Performance / Design) from
// mathresearcher.com/analytic-hierarchy-process-ahp. The published walkthrough
// rounds intermediate weights and reports lambda_max ~ 3.005; the exact value
// below is confirmed with numpy.linalg.eigvals.
TEST(InconsistencyTest, LaptopCriteriaExample) {
  const Matrix m = make_matrix(
      3, 3, {1.0, 1.0 / 3.0, 0.5, 3.0, 1.0, 2.0, 2.0, 0.5, 1.0});

  ConsistencyOptions options;
  options.use_harker = false;
  const ConsistencyResult result = cppanp::consistency(m, options);

  EXPECT_NEAR(result.lambda_max, 3.0092027127147167, 1e-9);
  EXPECT_NEAR(result.consistency_index, 0.004601356357358366, 1e-9);
  EXPECT_NEAR(result.consistency_ratio, 0.004601356357358366 / 0.52, 1e-9);

  EXPECT_NEAR(result.priority[0], 0.163424118566, 1e-9);
  EXPECT_NEAR(result.priority[1], 0.539614550220, 1e-9);
  EXPECT_NEAR(result.priority[2], 0.296961331213, 1e-9);

  // Performance ranks highest, then Design, then Price.
  EXPECT_GT(result.priority[1], result.priority[2]);
  EXPECT_GT(result.priority[2], result.priority[0]);
}

// Fruit ranking example from math.stackexchange.com/questions/1272705, which
// publishes lambda_max = 4.170149768 and the Perron eigenvector
// (6.884563466, 1.859400323, 4.693747683, 1.0). Exercises the n = 4 random
// index.
TEST(InconsistencyTest, FourByFourPerronVectorExample) {
  const Matrix m = make_matrix(4, 4,
                               {1.0, 4.0, 2.0, 5.0,
                                0.25, 1.0, 0.25, 3.0,
                                0.5, 4.0, 1.0, 4.0,
                                0.2, 1.0 / 3.0, 0.25, 1.0});

  ConsistencyOptions options;
  options.use_harker = false;
  const ConsistencyResult result = cppanp::consistency(m, options);

  EXPECT_NEAR(result.lambda_max, 4.170149768332779, 1e-8);
  EXPECT_NEAR(result.consistency_index, 0.05671658933, 1e-8);
  EXPECT_DOUBLE_EQ(result.random_index, 0.89);
  EXPECT_NEAR(result.consistency_ratio, 0.05671658944425969 / 0.89, 1e-8);

  // Published Perron vector, rescaled to sum to 1.
  EXPECT_NEAR(result.priority[0], 0.476845889502, 1e-8);
  EXPECT_NEAR(result.priority[1], 0.128787751522, 1e-8);
  EXPECT_NEAR(result.priority[2], 0.325103304957, 1e-8);
  EXPECT_NEAR(result.priority[3], 0.069263054019, 1e-8);
}

// Many AHP texts and calculators use Saaty's 1980 random index table, where
// RI(3) = 0.58. This library follows the ANP-era table used by pyanp and
// SuperDecisions, where RI(3) = 0.52, so published CR values from those
// sources are smaller than ours for the same matrix. CI is unaffected because
// it does not depend on RI.
TEST(InconsistencyTest, RandomIndexTableChoiceOnlyAffectsCr) {
  const Matrix m = make_matrix(
      3, 3, {1.0, 2.0, 4.0, 0.5, 1.0, 3.0, 0.25, 1.0 / 3.0, 1.0});

  ConsistencyOptions options;
  options.use_harker = false;
  const ConsistencyResult result = cppanp::consistency(m, options);

  const double classic_saaty_cr = result.consistency_index / 0.58;
  EXPECT_DOUBLE_EQ(result.random_index, 0.52);
  EXPECT_NEAR(result.consistency_ratio, result.consistency_index / 0.52,
              1e-12);
  EXPECT_GT(result.consistency_ratio, classic_saaty_cr);
}

TEST(InconsistencyTest, InconsistentMatrixMatchesOfficialCr) {
  // Same matrix used in eigen golden tests against pyanp.
  const Matrix m =
      make_matrix(3, 3, {1.0, 2.0, 5.0, 0.5, 1.0, 2.0, 0.2, 0.5, 1.0});

  ConsistencyOptions options;
  options.use_harker = false;
  const ConsistencyResult result = cppanp::consistency(m, options);

  // lambda_max ≈ 3.005535111748623 from pyanp/pri_eigen
  const double lambda = 3.005535111748623;
  const double ci = (lambda - 3.0) / 2.0;
  const double cr = ci / 0.52;

  EXPECT_NEAR(result.lambda_max, lambda, 1e-9);
  EXPECT_NEAR(result.consistency_index, ci, 1e-9);
  EXPECT_NEAR(result.consistency_ratio, cr, 1e-9);

  // For n=3, pyanp.incon_std is also CR (divisor = 0.52 * 2).
  EXPECT_NEAR(cppanp::consistency_ratio(m, options), cr, 1e-9);
  EXPECT_NEAR(cppanp::consistency_index(m, options), ci, 1e-9);
}

TEST(InconsistencyTest, LargeNUsesCorrectCrDivisor) {
  // Synthetic check: CI / RI with the (n-1) factor present.
  // Official: CR = (λ - n) / ((n - 1) * RI)
  // Bug in pyanp n>15: (λ - n) / RI
  constexpr std::size_t n = 16;
  const double lambda = 16.5;
  const double ri = 1.98 * 14.0 / 16.0;
  const double expected_cr = (lambda - 16.0) / (15.0 * ri);
  const double pyanp_buggy_cr = (lambda - 16.0) / ri;

  EXPECT_NEAR(cppanp::consistency_ratio(lambda, n), expected_cr, 1e-12);
  EXPECT_GT(std::abs(pyanp_buggy_cr - expected_cr), 1e-6);
}

TEST(InconsistencyTest, TwoByTwoAlwaysZeroCr) {
  const Matrix m = make_matrix(2, 2, {1.0, 3.0, 1.0 / 3.0, 1.0});
  ConsistencyOptions options;
  options.use_harker = false;
  EXPECT_NEAR(cppanp::consistency_ratio(m, options), 0.0, 1e-12);
  EXPECT_NEAR(cppanp::consistency_index(m, options), 0.0, 1e-12);
}
