#pragma once

#include <cstddef>

#include "cppanp/matrix.hpp"

namespace cppanp {

struct LimitMatrixOptions {
  double error = 1e-10;
  std::size_t max_iters = 5000;
  bool use_hierarchy_formula = true;
  // If 0, compute start power from the matrix (pyanp default).
  std::size_t start_pow = 0;
};

// Column-normalize: each nonzero column is divided by its sum (pyanp normalize).
[[nodiscard]] Matrix column_normalize(const Matrix& mat);
void column_normalize_inplace(Matrix& mat);

// Hierarchy formula: normalize(sum_{k=1}^{n-1} W^k) when W is nilpotent.
// Returns empty Matrix if the matrix is not a hierarchy.
[[nodiscard]] Matrix hierarchy_formula(const Matrix& mat);

// SuperDecisions / pyanp "calculus" limit matrix.
[[nodiscard]] Matrix calculus_limit(const Matrix& mat,
                                    const LimitMatrixOptions& options = {});

// Row-sum of the limit matrix, L1-normalized (pyanp priority_from_limit).
[[nodiscard]] Vector priority_from_limit(const Matrix& limit_matrix);

}  // namespace cppanp
