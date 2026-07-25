#include "cppanp/pairwise.hpp"

#include <stdexcept>
#include <utility>

namespace cppanp {

PairwiseJudgments::PairwiseJudgments(std::vector<std::string> alternatives)
    : alternatives_(std::move(alternatives)) {
  const std::size_t n = alternatives_.size();
  if (n == 0) {
    return;
  }
  matrix_ = Matrix::identity(n);
}

bool PairwiseJudgments::has_alternative(const std::string& name) const {
  for (const auto& alt : alternatives_) {
    if (alt == name) {
      return true;
    }
  }
  return false;
}

std::size_t PairwiseJudgments::index_of(const std::string& name) const {
  for (std::size_t i = 0; i < alternatives_.size(); ++i) {
    if (alternatives_[i] == name) {
      return i;
    }
  }
  throw std::invalid_argument("unknown pairwise alternative: " + name);
}

void PairwiseJudgments::add_alternative(const std::string& name,
                                        bool ignore_existing) {
  if (has_alternative(name)) {
    if (ignore_existing) {
      return;
    }
    throw std::invalid_argument("pairwise alternative already exists: " + name);
  }

  const std::size_t n = alternatives_.size();
  Matrix next(n + 1, n + 1, 0.0);
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j < n; ++j) {
      next(i, j) = matrix_(i, j);
    }
  }
  for (std::size_t i = 0; i <= n; ++i) {
    next(i, i) = 1.0;
  }
  matrix_ = std::move(next);
  alternatives_.push_back(name);
}

void PairwiseJudgments::remove_alternative(const std::string& name,
                                           bool ignore_missing) {
  if (!has_alternative(name)) {
    if (ignore_missing) {
      return;
    }
    throw std::invalid_argument("unknown pairwise alternative: " + name);
  }
  const std::size_t idx = index_of(name);
  const std::size_t n = alternatives_.size();
  if (n == 1) {
    alternatives_.clear();
    matrix_ = Matrix{};
    return;
  }
  Matrix next(n - 1, n - 1, 0.0);
  for (std::size_t i = 0, ni = 0; i < n; ++i) {
    if (i == idx) continue;
    for (std::size_t j = 0, nj = 0; j < n; ++j) {
      if (j == idx) continue;
      next(ni, nj) = matrix_(i, j);
      ++nj;
    }
    ++ni;
  }
  matrix_ = std::move(next);
  alternatives_.erase(alternatives_.begin() +
                      static_cast<std::ptrdiff_t>(idx));
}

void PairwiseJudgments::set_comparison(std::size_t i,
                                       std::size_t j,
                                       double value) {
  if (i == j) {
    throw std::invalid_argument("cannot set a pairwise diagonal comparison");
  }
  if (i >= size() || j >= size()) {
    throw DimensionError("pairwise comparison index out of range");
  }
  if (value == 0.0) {
    matrix_(i, j) = 0.0;
    matrix_(j, i) = 0.0;
    return;
  }
  matrix_(i, j) = value;
  matrix_(j, i) = 1.0 / value;
}

void PairwiseJudgments::set_comparison(const std::string& a,
                                       const std::string& b,
                                       double value) {
  set_comparison(index_of(a), index_of(b), value);
}

double PairwiseJudgments::comparison(std::size_t i, std::size_t j) const {
  return matrix_(i, j);
}

double PairwiseJudgments::comparison(const std::string& a,
                                     const std::string& b) const {
  return comparison(index_of(a), index_of(b));
}

Vector PairwiseJudgments::priorities(const EigenOptions& options) const {
  if (empty()) {
    return Vector{};
  }
  if (size() == 1) {
    return Vector(1, 1.0);
  }
  EigenOptions opts = options;
  // Incomplete comparisons: enable Harker unless the caller already set it.
  bool has_zero = false;
  for (std::size_t i = 0; i < size(); ++i) {
    for (std::size_t j = 0; j < size(); ++j) {
      if (i != j && matrix_(i, j) == 0.0) {
        has_zero = true;
        break;
      }
    }
    if (has_zero) {
      break;
    }
  }
  if (has_zero) {
    opts.use_harker = true;
  }
  return principal_eigenvector(matrix_, opts);
}

double PairwiseJudgments::consistency_ratio(ConsistencyOptions options) const {
  if (size() <= 2) {
    return 0.0;
  }
  bool has_zero = false;
  for (std::size_t i = 0; i < size(); ++i) {
    for (std::size_t j = 0; j < size(); ++j) {
      if (i != j && matrix_(i, j) == 0.0) {
        has_zero = true;
        break;
      }
    }
    if (has_zero) {
      break;
    }
  }
  options.use_harker = has_zero || options.use_harker;
  return cppanp::consistency_ratio(matrix_, options);
}

}  // namespace cppanp
