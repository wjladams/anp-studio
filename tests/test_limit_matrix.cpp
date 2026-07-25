#include "cppanp/limit_matrix.hpp"

#include <gtest/gtest.h>

using cppanp::LimitMatrixOptions;
using cppanp::Matrix;
using cppanp::Vector;

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

TEST(LimitMatrixTest, ColumnNormalizeLeavesZeroColumn) {
  Matrix m = make_matrix(2, 2, {1.0, 0.0, 3.0, 0.0});
  cppanp::column_normalize_inplace(m);
  EXPECT_DOUBLE_EQ(m(0, 0), 0.25);
  EXPECT_DOUBLE_EQ(m(1, 0), 0.75);
  EXPECT_DOUBLE_EQ(m(0, 1), 0.0);
  EXPECT_DOUBLE_EQ(m(1, 1), 0.0);
}

TEST(LimitMatrixTest, HierarchyFormulaForStrictHierarchy) {
  // Nodes: Goal -> C1,C2 -> A1 (sinks). Column-stochastic hierarchy.
  // Order: Goal, C1, C2, A1
  // Goal compares C1:C2 as 2:1 -> column [0, 2/3, 1/3, 0]
  // C1 and C2 each point only to A1
  Matrix W = make_matrix(4, 4,
                         {0, 0, 0, 0,
                          2.0 / 3.0, 0, 0, 0,
                          1.0 / 3.0, 0, 0, 0,
                          0, 1, 1, 0});

  const Matrix L = cppanp::hierarchy_formula(W);
  ASSERT_FALSE(L.empty());

  // Hand: W + W^2 + W^3, then column normalize.
  // summ col0 = [0, 2/3, 1/3, 1], sum = 2
  EXPECT_NEAR(L(0, 0), 0.0, 1e-12);
  EXPECT_NEAR(L(1, 0), 1.0 / 3.0, 1e-9);
  EXPECT_NEAR(L(2, 0), 1.0 / 6.0, 1e-9);
  EXPECT_NEAR(L(3, 0), 0.5, 1e-9);
}

TEST(LimitMatrixTest, CalculusOnColumnStochasticMatrix) {
  // Simple Markov matrix with unique stationary distribution.
  Matrix W = make_matrix(2, 2, {0.0, 0.5, 1.0, 0.5});
  LimitMatrixOptions options;
  options.start_pow = 8;
  const Matrix L = cppanp::calculus_limit(W, options);

  // Stationary pi satisfies pi = W pi for columns that converged: each column
  // of the limit should be the same distribution [1/3, 2/3].
  EXPECT_NEAR(L(0, 0), 1.0 / 3.0, 1e-6);
  EXPECT_NEAR(L(1, 0), 2.0 / 3.0, 1e-6);
  EXPECT_NEAR(L(0, 1), 1.0 / 3.0, 1e-6);
  EXPECT_NEAR(L(1, 1), 2.0 / 3.0, 1e-6);
}

TEST(LimitMatrixTest, PriorityFromLimitRowSums) {
  Matrix L = make_matrix(2, 2, {0.5, 0.5, 0.5, 0.5});
  const Vector p = cppanp::priority_from_limit(L);
  EXPECT_NEAR(p[0], 0.5, 1e-12);
  EXPECT_NEAR(p[1], 0.5, 1e-12);
}

TEST(LimitMatrixTest, IdentityLimitIsIdentity) {
  const Matrix I = Matrix::identity(3);
  LimitMatrixOptions options;
  options.start_pow = 4;
  const Matrix L = cppanp::calculus_limit(I, options);
  EXPECT_TRUE(L.is_near(I, 1e-8, 1e-8));
}
