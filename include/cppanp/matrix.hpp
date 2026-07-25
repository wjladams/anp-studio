#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace cppanp {

class DimensionError : public std::runtime_error {
public:
  explicit DimensionError(const std::string& message)
      : std::runtime_error(message) {}
};

class Vector {
public:
  Vector() = default;
  explicit Vector(std::size_t size, double fill = 0.0);

  Vector(const Vector&) = default;
  Vector(Vector&&) noexcept = default;
  Vector& operator=(const Vector&) = default;
  Vector& operator=(Vector&&) noexcept = default;

  [[nodiscard]] std::size_t size() const noexcept { return data_.size(); }
  [[nodiscard]] bool empty() const noexcept { return data_.empty(); }

  double& operator[](std::size_t i);
  [[nodiscard]] const double& operator[](std::size_t i) const;

  [[nodiscard]] double* data() noexcept { return data_.data(); }
  [[nodiscard]] const double* data() const noexcept { return data_.data(); }

  [[nodiscard]] Vector operator+(const Vector& other) const;
  [[nodiscard]] Vector operator-(const Vector& other) const;
  [[nodiscard]] Vector operator*(double scalar) const;

  Vector& operator+=(const Vector& other);
  Vector& operator-=(const Vector& other);
  Vector& operator*=(double scalar);

  [[nodiscard]] double sum() const;
  void normalize();
  [[nodiscard]] Vector normalized() const;

  [[nodiscard]] bool is_equal(const Vector& other,
                              double abs_tol = 1e-12) const;
  [[nodiscard]] bool is_near(const Vector& other,
                             double abs_tol = 1e-9,
                             double rel_tol = 1e-9) const;

  friend Vector operator*(double scalar, const Vector& v);

private:
  std::vector<double> data_;

  void check_same_size(const Vector& other, const char* op) const;
};

class Matrix {
public:
  Matrix() = default;
  Matrix(std::size_t rows, std::size_t cols, double fill = 0.0);

  Matrix(const Matrix&) = default;
  Matrix(Matrix&&) noexcept = default;
  Matrix& operator=(const Matrix&) = default;
  Matrix& operator=(Matrix&&) noexcept = default;

  [[nodiscard]] static Matrix identity(std::size_t n);
  [[nodiscard]] static Matrix zeros(std::size_t rows, std::size_t cols);
  [[nodiscard]] static Matrix ones(std::size_t rows, std::size_t cols);

  [[nodiscard]] std::size_t rows() const noexcept { return rows_; }
  [[nodiscard]] std::size_t cols() const noexcept { return cols_; }
  [[nodiscard]] bool empty() const noexcept { return rows_ == 0 || cols_ == 0; }

  double& operator()(std::size_t i, std::size_t j);
  [[nodiscard]] const double& operator()(std::size_t i, std::size_t j) const;

  [[nodiscard]] double* data() noexcept { return data_.data(); }
  [[nodiscard]] const double* data() const noexcept { return data_.data(); }

  [[nodiscard]] Matrix operator+(const Matrix& other) const;
  [[nodiscard]] Matrix operator-(const Matrix& other) const;
  [[nodiscard]] Matrix operator*(double scalar) const;
  [[nodiscard]] Matrix operator*(const Matrix& other) const;
  [[nodiscard]] Vector operator*(const Vector& v) const;

  Matrix& operator+=(const Matrix& other);
  Matrix& operator-=(const Matrix& other);
  Matrix& operator*=(double scalar);

  [[nodiscard]] Matrix transposed() const;

  [[nodiscard]] Vector row_sums() const;
  [[nodiscard]] Vector col_sums() const;
  void normalize_rows();
  void normalize_cols();
  [[nodiscard]] Matrix rows_normalized() const;
  [[nodiscard]] Matrix cols_normalized() const;

  [[nodiscard]] bool is_equal(const Matrix& other,
                              double abs_tol = 1e-12) const;
  [[nodiscard]] bool is_near(const Matrix& other,
                             double abs_tol = 1e-9,
                             double rel_tol = 1e-9) const;

  friend Matrix operator*(double scalar, const Matrix& m);

private:
  std::size_t rows_ = 0;
  std::size_t cols_ = 0;
  std::vector<double> data_;  // row-major

  [[nodiscard]] std::size_t index(std::size_t i, std::size_t j) const;
  void check_bounds(std::size_t i, std::size_t j) const;
  void check_same_shape(const Matrix& other, const char* op) const;
};

}  // namespace cppanp
