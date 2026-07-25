#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "cppanp/eigen.hpp"
#include "cppanp/inconsistency.hpp"
#include "cppanp/matrix.hpp"

namespace cppanp {

// Square pairwise comparison table over a named list of alternatives.
class PairwiseJudgments {
public:
  PairwiseJudgments() = default;
  explicit PairwiseJudgments(std::vector<std::string> alternatives);

  [[nodiscard]] std::size_t size() const noexcept { return alternatives_.size(); }
  [[nodiscard]] bool empty() const noexcept { return alternatives_.empty(); }
  [[nodiscard]] const std::vector<std::string>& alternatives() const noexcept {
    return alternatives_;
  }
  [[nodiscard]] const Matrix& matrix() const noexcept { return matrix_; }

  [[nodiscard]] bool has_alternative(const std::string& name) const;
  [[nodiscard]] std::size_t index_of(const std::string& name) const;

  // Append an alternative (diagonal 1). No-op if already present when
  // ignore_existing is true; otherwise throws.
  void add_alternative(const std::string& name, bool ignore_existing = false);
  // Remove an alternative and its row/column. No-op if missing when
  // ignore_missing is true.
  void remove_alternative(const std::string& name, bool ignore_missing = false);

  // Set a_ij = value and a_ji = 1/value when value != 0. value == 0 clears both
  // off-diagonal entries (incomplete comparison).
  void set_comparison(std::size_t i, std::size_t j, double value);
  void set_comparison(const std::string& a,
                      const std::string& b,
                      double value);

  [[nodiscard]] double comparison(std::size_t i, std::size_t j) const;
  [[nodiscard]] double comparison(const std::string& a,
                                  const std::string& b) const;

  [[nodiscard]] Vector priorities(const EigenOptions& options = {}) const;
  [[nodiscard]] double consistency_ratio(
      ConsistencyOptions options = {}) const;

private:
  std::vector<std::string> alternatives_;
  Matrix matrix_;
};

}  // namespace cppanp
