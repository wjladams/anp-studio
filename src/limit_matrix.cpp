#include "cppanp/limit_matrix.hpp"

#include "cppanp/eigen.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace cppanp {
namespace {

[[nodiscard]] double matrix_max_abs(const Matrix& mat) {
  double best = 0.0;
  for (std::size_t i = 0; i < mat.rows(); ++i) {
    for (std::size_t j = 0; j < mat.cols(); ++j) {
      best = std::max(best, std::abs(mat(i, j)));
    }
  }
  return best;
}

[[nodiscard]] Vector column_maxes(const Matrix& mat) {
  Vector m(mat.cols(), 0.0);
  for (std::size_t j = 0; j < mat.cols(); ++j) {
    double best = 0.0;
    for (std::size_t i = 0; i < mat.rows(); ++i) {
      best = std::max(best, std::abs(mat(i, j)));
    }
    m[j] = best == 0.0 ? 1.0 : best;
  }
  return m;
}

// mat^N with N a power of 2 >= power, optionally rescaling by 1/max after each square.
[[nodiscard]] Matrix mat_pow2(const Matrix& mat,
                              std::size_t power,
                              bool rescale) {
  Matrix last = mat;
  Matrix next = mat;
  std::size_t count = 1;
  while (count <= power) {
    next = last * last;
    if (rescale) {
      const double mmax = matrix_max_abs(next);
      if (mmax != 0.0) {
        next *= (1.0 / mmax);
      }
    }
    std::swap(last, next);
    count *= 2;
  }
  return last;
}

[[nodiscard]] std::size_t calculus_start_power(const Matrix& mat) {
  const std::size_t n = mat.rows();
  if (n == 0) {
    return 1;
  }
  const double epsilon = 1.0 / (20.0 * static_cast<double>(n));
  std::vector<double> small_entries;
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j < n; ++j) {
      const double val = std::abs(mat(i, j));
      if (val < epsilon && val != 0.0) {
        small_entries.push_back(val);
      }
    }
  }
  if (small_entries.empty()) {
    return 20 * n * n + 10;
  }
  double sum = 0.0;
  for (double v : small_entries) {
    sum += v;
  }
  const double avg = sum / static_cast<double>(small_entries.size());
  const double A = 1.0 / avg;
  return static_cast<std::size_t>(A) * n * n;
}

[[nodiscard]] double normalize_cols_dist(const Matrix& mat1,
                                         const Matrix& mat2) {
  const Vector div1 = column_maxes(mat1);
  const Vector div2 = column_maxes(mat2);
  double worst = 0.0;
  for (std::size_t i = 0; i < mat1.rows(); ++i) {
    for (std::size_t j = 0; j < mat1.cols(); ++j) {
      const double a = mat1(i, j) / div1[j];
      const double b = mat2(i, j) / div2[j];
      worst = std::max(worst, std::abs(a - b));
    }
  }
  return worst;
}

}  // namespace

Matrix column_normalize(const Matrix& mat) {
  Matrix out = mat;
  column_normalize_inplace(out);
  return out;
}

void column_normalize_inplace(Matrix& mat) {
  for (std::size_t j = 0; j < mat.cols(); ++j) {
    double total = 0.0;
    for (std::size_t i = 0; i < mat.rows(); ++i) {
      total += mat(i, j);
    }
    if (total == 0.0) {
      continue;
    }
    for (std::size_t i = 0; i < mat.rows(); ++i) {
      mat(i, j) /= total;
    }
  }
}

Matrix hierarchy_formula(const Matrix& mat) {
  if (mat.rows() != mat.cols()) {
    throw DimensionError("hierarchy_formula requires a square matrix");
  }
  const std::size_t size = mat.rows();
  if (size == 0) {
    return Matrix{};
  }

  const Matrix big = mat_pow2(mat, size + 1, false);
  if (matrix_max_abs(big) != 0.0) {
    return Matrix{};  // not a hierarchy
  }

  Matrix summ = mat;
  Matrix thispow = mat;
  for (std::size_t i = 0; i + 2 < size; ++i) {
    const Matrix nextpow = thispow * mat;
    summ += nextpow;
    thispow = nextpow;
  }
  return column_normalize(summ);
}

Matrix calculus_limit(const Matrix& mat, const LimitMatrixOptions& options) {
  if (mat.rows() != mat.cols()) {
    throw DimensionError("calculus_limit requires a square matrix");
  }
  const std::size_t size = mat.rows();
  if (size == 0) {
    return Matrix{};
  }

  std::size_t start_pow = options.start_pow;
  if (start_pow == 0) {
    start_pow = calculus_start_power(mat);
  }

  Matrix start = mat_pow2(mat, start_pow, true);
  if (options.use_hierarchy_formula && matrix_max_abs(start) == 0.0) {
    start = mat_pow2(mat, size, false);
    if (matrix_max_abs(start) == 0.0) {
      const Matrix hier = hierarchy_formula(mat);
      if (!hier.empty()) {
        return hier;
      }
    }
  }

  std::vector<Matrix> pows;
  pows.reserve(size);
  pows.push_back(start);
  for (std::size_t i = 0; i + 1 < size; ++i) {
    pows.push_back(mat * pows.back());
    const double diff =
        normalize_cols_dist(pows[pows.size() - 1], pows[pows.size() - 2]);
    if (diff < options.error) {
      return column_normalize(pows.back());
    }
  }

  for (std::size_t count = 0; count < options.max_iters; ++count) {
    Matrix nextp = pows.back() * mat;
    for (std::size_t i = 0; i + 1 < pows.size(); ++i) {
      pows[i] = std::move(pows[i + 1]);
    }
    pows.back() = nextp;

    bool converged = false;
    for (std::size_t i = 0; i + 1 < pows.size(); ++i) {
      if (normalize_cols_dist(pows[i], pows.back()) < options.error) {
        converged = true;
        break;
      }
    }
    if (converged) {
      return column_normalize(pows.back());
    }
  }

  throw ConvergenceError(
      "calculus_limit: did not converge within the iteration limit");
}

Vector priority_from_limit(const Matrix& limit_matrix) {
  Vector rval = limit_matrix.row_sums();
  const double total = rval.sum();
  if (total != 0.0) {
    rval.normalize();
  }
  return rval;
}

}  // namespace cppanp
