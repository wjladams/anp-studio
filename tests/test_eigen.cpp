#include "cppanp/eigen.hpp"

#include <gtest/gtest.h>

#include <initializer_list>
#include <vector>

using cppanp::ConvergenceError;
using cppanp::DimensionError;
using cppanp::EigenOptions;
using cppanp::EigenResult;
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

// A[i][j] = w[i]/w[j] is perfectly consistent, so the principal eigenvector
// is w and the principal eigenvalue is exactly n.
Matrix ratio_matrix(const std::vector<double>& weights) {
  const std::size_t n = weights.size();
  Matrix m(n, n);
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j < n; ++j) {
      m(i, j) = weights[i] / weights[j];
    }
  }
  return m;
}

}  // namespace

TEST(EigenTest, EmptyMatrixGivesEmptyVector) {
  const EigenResult result = cppanp::principal_eigen(Matrix{});
  EXPECT_TRUE(result.vector.empty());
  EXPECT_DOUBLE_EQ(result.value, 0.0);
  EXPECT_TRUE(result.converged);
}

TEST(EigenTest, NonSquareThrows) {
  const Matrix m(2, 3, 1.0);
  EXPECT_THROW(cppanp::principal_eigen(m), DimensionError);
  EXPECT_THROW(cppanp::harker_fix(m), DimensionError);
}

TEST(EigenTest, SingleElement) {
  const Matrix m = make_matrix(1, 1, {5.0});
  const EigenResult result = cppanp::principal_eigen(m);
  EXPECT_DOUBLE_EQ(result.vector[0], 1.0);
  EXPECT_NEAR(result.value, 5.0, 1e-12);
}

TEST(EigenTest, IdentityGivesUniformPriorities) {
  const Vector v = cppanp::principal_eigenvector(Matrix::identity(4));
  EXPECT_NEAR(v[0], 0.25, 1e-9);
  EXPECT_NEAR(v[1], 0.25, 1e-9);
  EXPECT_NEAR(v[2], 0.25, 1e-9);
  EXPECT_NEAR(v[3], 0.25, 1e-9);
  EXPECT_NEAR(cppanp::principal_eigenvalue(Matrix::identity(4)), 1.0, 1e-9);
}

TEST(EigenTest, TwoByTwoConsistentMatrix) {
  const Matrix m = make_matrix(2, 2, {1.0, 2.0, 0.5, 1.0});
  const EigenResult result = cppanp::principal_eigen(m);
  EXPECT_NEAR(result.vector[0], 2.0 / 3.0, 1e-9);
  EXPECT_NEAR(result.vector[1], 1.0 / 3.0, 1e-9);
  EXPECT_NEAR(result.value, 2.0, 1e-9);
}

TEST(EigenTest, ThreeByThreeConsistentMatrix) {
  const Matrix m =
      make_matrix(3, 3, {1.0, 2.0, 4.0, 0.5, 1.0, 2.0, 0.25, 0.5, 1.0});
  const EigenResult result = cppanp::principal_eigen(m);
  EXPECT_NEAR(result.vector[0], 4.0 / 7.0, 1e-9);
  EXPECT_NEAR(result.vector[1], 2.0 / 7.0, 1e-9);
  EXPECT_NEAR(result.vector[2], 1.0 / 7.0, 1e-9);
  EXPECT_NEAR(result.value, 3.0, 1e-9);
}

TEST(EigenTest, TwoThreeSixPairwiseExample) {
  const Matrix m = make_matrix(
      3, 3,
      {1.0, 2.0, 6.0, 0.5, 1.0, 3.0, 1.0 / 6.0, 1.0 / 3.0, 1.0});
  const EigenResult result = cppanp::principal_eigen(m);

  EXPECT_TRUE(result.converged);
  EXPECT_NEAR(result.vector[0], 0.6, 1e-9);
  EXPECT_NEAR(result.vector[1], 0.3, 1e-9);
  EXPECT_NEAR(result.vector[2], 0.1, 1e-9);
  EXPECT_NEAR(result.value, 3.0, 1e-9);
}

TEST(EigenTest, ConsistentMatrixRecoversWeightsAndEigenvalueEqualsSize) {
  const std::vector<double> weights = {0.5, 0.3, 0.15, 0.05};
  const Matrix m = ratio_matrix(weights);
  const EigenResult result = cppanp::principal_eigen(m);
  for (std::size_t i = 0; i < weights.size(); ++i) {
    EXPECT_NEAR(result.vector[i], weights[i], 1e-9);
  }
  EXPECT_NEAR(result.value, static_cast<double>(weights.size()), 1e-9);
}

TEST(EigenTest, EigenvectorSumsToOne) {
  const Matrix m =
      make_matrix(3, 3, {1.0, 2.0, 5.0, 0.5, 1.0, 2.0, 0.2, 0.5, 1.0});
  EXPECT_NEAR(cppanp::principal_eigenvector(m).sum(), 1.0, 1e-12);
}

TEST(EigenTest, SatisfiesEigenEquation) {
  const Matrix m =
      make_matrix(3, 3, {1.0, 3.0, 7.0, 1.0 / 3.0, 1.0, 2.0, 1.0 / 7.0, 0.5,
                         1.0});
  const EigenResult result = cppanp::principal_eigen(m);
  const Vector lhs = m * result.vector;
  const Vector rhs = result.vector * result.value;
  EXPECT_TRUE(lhs.is_near(rhs, 1e-8, 1e-8));
}

TEST(EigenTest, InconsistentMatrixHasEigenvalueAboveSize) {
  const Matrix m =
      make_matrix(3, 3, {1.0, 2.0, 5.0, 0.5, 1.0, 2.0, 0.2, 0.5, 1.0});
  const double value = cppanp::principal_eigenvalue(m);
  EXPECT_GT(value, 3.0);
  EXPECT_LT(value, 3.1);
}

// Golden values produced by pyanp's pri_eigen algorithm (numpy, error=1e-10).
TEST(EigenTest, MatchesPyanpReferenceValues) {
  const Matrix a =
      make_matrix(3, 3, {1.0, 2.0, 5.0, 0.5, 1.0, 2.0, 0.2, 0.5, 1.0});
  const EigenResult ra = cppanp::principal_eigen(a);
  EXPECT_NEAR(ra.vector[0], 0.59537902, 1e-8);
  EXPECT_NEAR(ra.vector[1], 0.27635046, 1e-8);
  EXPECT_NEAR(ra.vector[2], 0.12827052, 1e-8);
  EXPECT_NEAR(ra.value, 3.005535111748623, 1e-9);

  const Matrix b = make_matrix(
      3, 3, {1.0, 3.0, 7.0, 1.0 / 3.0, 1.0, 2.0, 1.0 / 7.0, 0.5, 1.0});
  const EigenResult rb = cppanp::principal_eigen(b);
  EXPECT_NEAR(rb.vector[0], 0.68165043, 1e-8);
  EXPECT_NEAR(rb.vector[1], 0.21583649, 1e-8);
  EXPECT_NEAR(rb.vector[2], 0.10251308, 1e-8);
  EXPECT_NEAR(rb.value, 3.0026408512031315, 1e-9);
}

TEST(EigenTest, ReportsIterationCountAndConvergence) {
  const Matrix m = make_matrix(2, 2, {1.0, 2.0, 0.5, 1.0});
  const EigenResult result = cppanp::principal_eigen(m);
  EXPECT_TRUE(result.converged);
  EXPECT_GT(result.iterations, 0u);
  EXPECT_LT(result.iterations, 100u);
}

TEST(EigenTest, LooserErrorConvergesInFewerIterations) {
  const Matrix m =
      make_matrix(3, 3, {1.0, 2.0, 5.0, 0.5, 1.0, 2.0, 0.2, 0.5, 1.0});

  EigenOptions tight;
  tight.error = 1e-12;
  EigenOptions loose;
  loose.error = 1e-4;

  EXPECT_LT(cppanp::principal_eigen(m, loose).iterations,
            cppanp::principal_eigen(m, tight).iterations);
}

TEST(EigenTest, IterationLimitReportsNonConvergence) {
  const Matrix m =
      make_matrix(3, 3, {1.0, 2.0, 5.0, 0.5, 1.0, 2.0, 0.2, 0.5, 1.0});

  EigenOptions options;
  options.max_iterations = 1;
  const EigenResult result = cppanp::principal_eigen(m, options);
  EXPECT_FALSE(result.converged);
  EXPECT_EQ(result.iterations, 1u);

  EXPECT_THROW((void)cppanp::principal_eigenvector(m, options),
               ConvergenceError);
  EXPECT_THROW((void)cppanp::principal_eigenvalue(m, options),
               ConvergenceError);
}

TEST(EigenTest, ZeroMatrixThrows) {
  const Matrix m = Matrix::zeros(3, 3);
  EXPECT_THROW(cppanp::principal_eigen(m), ConvergenceError);
}

TEST(EigenTest, HarkerFixCountsMissingComparisons) {
  const Matrix m =
      make_matrix(3, 3, {1.0, 0.0, 4.0, 0.0, 1.0, 0.0, 0.25, 0.0, 1.0});
  const Matrix fixed = cppanp::harker_fix(m);

  EXPECT_DOUBLE_EQ(fixed(0, 0), 2.0);
  EXPECT_DOUBLE_EQ(fixed(1, 1), 3.0);
  EXPECT_DOUBLE_EQ(fixed(2, 2), 2.0);

  EXPECT_DOUBLE_EQ(fixed(0, 2), 4.0);
  EXPECT_DOUBLE_EQ(fixed(2, 0), 0.25);
  EXPECT_DOUBLE_EQ(m(0, 0), 1.0);
}

TEST(EigenTest, HarkerFixLeavesCompleteMatrixDiagonalAtOne) {
  const Matrix m = make_matrix(2, 2, {1.0, 2.0, 0.5, 1.0});
  const Matrix fixed = cppanp::harker_fix(m);
  EXPECT_TRUE(fixed.is_equal(m));
}

TEST(EigenTest, UseHarkerHandlesIncompleteMatrix) {
  const Matrix m =
      make_matrix(3, 3, {1.0, 2.0, 0.0, 0.5, 1.0, 3.0, 0.0, 1.0 / 3.0, 1.0});

  EigenOptions options;
  options.use_harker = true;
  const EigenResult result = cppanp::principal_eigen(m, options);

  EXPECT_TRUE(result.converged);
  EXPECT_NEAR(result.vector.sum(), 1.0, 1e-12);

  // pyanp reference for the same matrix with use_harker=True.
  EXPECT_NEAR(result.vector[0], 0.6, 1e-8);
  EXPECT_NEAR(result.vector[1], 0.3, 1e-8);
  EXPECT_NEAR(result.vector[2], 0.1, 1e-8);
  EXPECT_NEAR(result.value, 3.0, 1e-8);
}
