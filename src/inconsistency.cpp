#include "cppanp/inconsistency.hpp"

#include <array>

namespace cppanp {
namespace {

// Saaty RI for n = 1..15 (1-based indexing via [n]). Values match pyanp.
constexpr std::array<double, 16> kSaatyRi = {
    0.0,   // unused / n=0
    0.0,   // n=1
    0.0,   // n=2
    0.52,  // n=3
    0.89,  // n=4
    1.12,  // n=5
    1.25,  // n=6
    1.35,  // n=7
    1.40,  // n=8
    1.45,  // n=9
    1.49,  // n=10
    1.51,  // n=11
    1.54,  // n=12
    1.56,  // n=13
    1.57,  // n=14
    1.58,  // n=15
};

}  // namespace

double random_index(std::size_t n) {
  if (n <= 15) {
    return kSaatyRi[n];
  }
  // Alonso-Lamata (2006): RI ≈ 1.98 * (n - 2) / n
  // Equivalent to pyanp's expression once simplified:
  // 1.98 * (1 - (n-1)/(n*(n-1)/2)) = 1.98 * (n-2)/n
  return 1.98 * static_cast<double>(n - 2) / static_cast<double>(n);
}

double consistency_index(double lambda_max, std::size_t n) {
  if (n < 2) {
    return 0.0;
  }
  return (lambda_max - static_cast<double>(n)) / static_cast<double>(n - 1);
}

double consistency_ratio(double lambda_max, std::size_t n) {
  const double ri = random_index(n);
  if (ri == 0.0) {
    return 0.0;
  }
  return consistency_index(lambda_max, n) / ri;
}

ConsistencyResult consistency(const Matrix& mat, ConsistencyOptions options) {
  if (mat.rows() != mat.cols()) {
    throw DimensionError("consistency requires a square matrix");
  }

  options.eigen.use_harker = options.use_harker;
  const EigenResult eigen = principal_eigen(mat, options.eigen);

  ConsistencyResult result;
  result.lambda_max = eigen.value;
  result.priority = eigen.vector;
  result.iterations = eigen.iterations;
  result.converged = eigen.converged;

  const std::size_t n = mat.rows();
  result.random_index = random_index(n);
  result.consistency_index = consistency_index(eigen.value, n);
  result.consistency_ratio = consistency_ratio(eigen.value, n);
  return result;
}

double consistency_index(const Matrix& mat, ConsistencyOptions options) {
  const ConsistencyResult result = consistency(mat, options);
  if (!result.converged) {
    throw ConvergenceError(
        "consistency_index: power iteration did not converge within the "
        "iteration limit");
  }
  return result.consistency_index;
}

double consistency_ratio(const Matrix& mat, ConsistencyOptions options) {
  const ConsistencyResult result = consistency(mat, options);
  if (!result.converged) {
    throw ConvergenceError(
        "consistency_ratio: power iteration did not converge within the "
        "iteration limit");
  }
  return result.consistency_ratio;
}

}  // namespace cppanp
