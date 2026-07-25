#pragma once

#include <cstddef>

#include "cppanp/eigen.hpp"
#include "cppanp/matrix.hpp"

namespace cppanp {

// Saaty random index (RI) for matrix order n.
// n = 1,2: conventionally 0 (CR is defined as 0).
// n = 3..15: Saaty table values used by pyanp.
// n > 15: Alonso-Lamata approximation RI ≈ 1.98 * (n - 2) / n.
[[nodiscard]] double random_index(std::size_t n);

// CI = (lambda_max - n) / (n - 1). Returns 0 for n < 2.
[[nodiscard]] double consistency_index(double lambda_max, std::size_t n);

// CR = CI / RI. Returns 0 when RI == 0 (n <= 2).
[[nodiscard]] double consistency_ratio(double lambda_max, std::size_t n);

struct ConsistencyOptions {
  EigenOptions eigen;
  // Match pyanp.incon_std default: apply Harker's fix before eigen.
  bool use_harker = true;
};

struct ConsistencyResult {
  double lambda_max = 0.0;
  double consistency_index = 0.0;
  double consistency_ratio = 0.0;
  double random_index = 0.0;
  Vector priority;
  std::size_t iterations = 0;
  bool converged = false;
};

// Compute Saaty CI and CR for a pairwise comparison matrix.
[[nodiscard]] ConsistencyResult consistency(const Matrix& mat,
                                            ConsistencyOptions options = {});

// Convenience wrappers that throw ConvergenceError on non-convergence.
[[nodiscard]] double consistency_index(const Matrix& mat,
                                       ConsistencyOptions options = {});
[[nodiscard]] double consistency_ratio(const Matrix& mat,
                                       ConsistencyOptions options = {});

}  // namespace cppanp
