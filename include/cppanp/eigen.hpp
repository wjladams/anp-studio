#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>

#include "cppanp/matrix.hpp"

namespace cppanp {

class ConvergenceError : public std::runtime_error {
public:
  explicit ConvergenceError(const std::string& message)
      : std::runtime_error(message) {}
};

// Diagonal of each row becomes 1 + (number of zero off-diagonal entries in
// that row), which lets power iteration handle incomplete comparisons.
[[nodiscard]] Matrix harker_fix(const Matrix& mat);

struct EigenOptions {
  // Stop once the max-norm change between successive iterates is <= error.
  double error = 1e-10;
  bool use_harker = false;
  // pyanp's pri_eigen has no cap and can spin forever on oscillating input.
  std::size_t max_iterations = 10000;
};

struct EigenResult {
  Vector vector;  // normalized so the components sum to 1
  double value = 0.0;
  std::size_t iterations = 0;
  bool converged = false;
};

// Power iteration with sum-normalization, matching pyanp's pri_eigen. Reports
// non-convergence through EigenResult::converged rather than throwing.
[[nodiscard]] EigenResult principal_eigen(const Matrix& mat,
                                          const EigenOptions& options = {});

// Throw ConvergenceError if the iteration hits max_iterations.
[[nodiscard]] Vector principal_eigenvector(const Matrix& mat,
                                           const EigenOptions& options = {});
[[nodiscard]] double principal_eigenvalue(const Matrix& mat,
                                          const EigenOptions& options = {});

}  // namespace cppanp
