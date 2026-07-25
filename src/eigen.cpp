#include "cppanp/eigen.hpp"

#include <cmath>
#include <sstream>

namespace cppanp {
namespace {

void check_square(const Matrix& mat, const char* op) {
  if (mat.rows() != mat.cols()) {
    std::ostringstream oss;
    oss << op << " requires a square matrix, got (" << mat.rows() << "x"
        << mat.cols() << ")";
    throw DimensionError(oss.str());
  }
}

[[nodiscard]] double max_abs_diff(const Vector& a, const Vector& b) {
  double worst = 0.0;
  for (std::size_t i = 0; i < a.size(); ++i) {
    worst = std::max(worst, std::abs(a[i] - b[i]));
  }
  return worst;
}

}  // namespace

Matrix harker_fix(const Matrix& mat) {
  check_square(mat, "harker_fix");
  Matrix fixed = mat;
  for (std::size_t row = 0; row < fixed.rows(); ++row) {
    double value = 1.0;
    for (std::size_t col = 0; col < fixed.cols(); ++col) {
      if (col != row && mat(row, col) == 0.0) {
        value += 1.0;
      }
    }
    fixed(row, row) = value;
  }
  return fixed;
}

EigenResult principal_eigen(const Matrix& mat, const EigenOptions& options) {
  check_square(mat, "principal_eigen");

  EigenResult result;
  if (mat.empty()) {
    result.converged = true;
    return result;
  }

  const Matrix work = options.use_harker ? harker_fix(mat) : mat;
  const std::size_t size = work.rows();

  Vector vec(size, 1.0);
  double diff = 1.0;
  std::size_t iterations = 0;

  while (diff > options.error && iterations < options.max_iterations) {
    Vector next = work * vec;
    const double total = next.sum();
    if (total == 0.0) {
      throw ConvergenceError(
          "principal_eigen: iterate summed to zero, the matrix has no "
          "positive principal eigenvector");
    }
    for (std::size_t i = 0; i < size; ++i) {
      next[i] /= total;
    }
    diff = max_abs_diff(next, vec);
    vec = next;
    ++iterations;
  }

  result.vector = vec;
  result.iterations = iterations;
  result.converged = diff <= options.error;
  result.value = (work * vec).sum();
  return result;
}

Vector principal_eigenvector(const Matrix& mat, const EigenOptions& options) {
  EigenResult result = principal_eigen(mat, options);
  if (!result.converged) {
    throw ConvergenceError(
        "principal_eigenvector: power iteration did not converge within the "
        "iteration limit");
  }
  return result.vector;
}

double principal_eigenvalue(const Matrix& mat, const EigenOptions& options) {
  const EigenResult result = principal_eigen(mat, options);
  if (!result.converged) {
    throw ConvergenceError(
        "principal_eigenvalue: power iteration did not converge within the "
        "iteration limit");
  }
  return result.value;
}

}  // namespace cppanp
