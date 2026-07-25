#include "cppanp/matrix.hpp"

#include <cmath>
#include <sstream>

namespace cppanp {
namespace {

[[nodiscard]] bool values_near(double a,
                               double b,
                               double abs_tol,
                               double rel_tol) {
  const double diff = std::abs(a - b);
  if (diff <= abs_tol) {
    return true;
  }
  const double scale = std::max(std::abs(a), std::abs(b));
  return diff <= rel_tol * scale;
}

[[nodiscard]] std::string size_mismatch(const char* op,
                                        std::size_t a_rows,
                                        std::size_t a_cols,
                                        std::size_t b_rows,
                                        std::size_t b_cols) {
  std::ostringstream oss;
  oss << "dimension mismatch in " << op << ": (" << a_rows << "x" << a_cols
      << ") vs (" << b_rows << "x" << b_cols << ")";
  return oss.str();
}

}  // namespace

// ---------------------------------------------------------------------------
// Vector
// ---------------------------------------------------------------------------

Vector::Vector(std::size_t size, double fill) : data_(size, fill) {}

double& Vector::operator[](std::size_t i) {
  if (i >= data_.size()) {
    throw DimensionError("vector index out of range");
  }
  return data_[i];
}

const double& Vector::operator[](std::size_t i) const {
  if (i >= data_.size()) {
    throw DimensionError("vector index out of range");
  }
  return data_[i];
}

void Vector::check_same_size(const Vector& other, const char* op) const {
  if (data_.size() != other.data_.size()) {
    throw DimensionError(size_mismatch(op, data_.size(), 1, other.data_.size(), 1));
  }
}

Vector Vector::operator+(const Vector& other) const {
  check_same_size(other, "Vector::operator+");
  Vector result(data_.size());
  for (std::size_t i = 0; i < data_.size(); ++i) {
    result.data_[i] = data_[i] + other.data_[i];
  }
  return result;
}

Vector Vector::operator-(const Vector& other) const {
  check_same_size(other, "Vector::operator-");
  Vector result(data_.size());
  for (std::size_t i = 0; i < data_.size(); ++i) {
    result.data_[i] = data_[i] - other.data_[i];
  }
  return result;
}

Vector Vector::operator*(double scalar) const {
  Vector result(data_.size());
  for (std::size_t i = 0; i < data_.size(); ++i) {
    result.data_[i] = data_[i] * scalar;
  }
  return result;
}

Vector& Vector::operator+=(const Vector& other) {
  check_same_size(other, "Vector::operator+=");
  for (std::size_t i = 0; i < data_.size(); ++i) {
    data_[i] += other.data_[i];
  }
  return *this;
}

Vector& Vector::operator-=(const Vector& other) {
  check_same_size(other, "Vector::operator-=");
  for (std::size_t i = 0; i < data_.size(); ++i) {
    data_[i] -= other.data_[i];
  }
  return *this;
}

Vector& Vector::operator*=(double scalar) {
  for (double& value : data_) {
    value *= scalar;
  }
  return *this;
}

double Vector::sum() const {
  double total = 0.0;
  for (double value : data_) {
    total += value;
  }
  return total;
}

void Vector::normalize() {
  const double total = sum();
  if (total == 0.0) {
    return;
  }
  for (double& value : data_) {
    value /= total;
  }
}

Vector Vector::normalized() const {
  Vector result = *this;
  result.normalize();
  return result;
}

bool Vector::is_equal(const Vector& other, double abs_tol) const {
  if (data_.size() != other.data_.size()) {
    return false;
  }
  for (std::size_t i = 0; i < data_.size(); ++i) {
    if (std::abs(data_[i] - other.data_[i]) > abs_tol) {
      return false;
    }
  }
  return true;
}

bool Vector::is_near(const Vector& other,
                     double abs_tol,
                     double rel_tol) const {
  if (data_.size() != other.data_.size()) {
    return false;
  }
  for (std::size_t i = 0; i < data_.size(); ++i) {
    if (!values_near(data_[i], other.data_[i], abs_tol, rel_tol)) {
      return false;
    }
  }
  return true;
}

Vector operator*(double scalar, const Vector& v) {
  return v * scalar;
}

// ---------------------------------------------------------------------------
// Matrix
// ---------------------------------------------------------------------------

Matrix::Matrix(std::size_t rows, std::size_t cols, double fill)
    : rows_(rows), cols_(cols), data_(rows * cols, fill) {
  if ((rows == 0) != (cols == 0)) {
    throw DimensionError("matrix cannot have a zero dimension unless both are zero");
  }
}

Matrix Matrix::identity(std::size_t n) {
  Matrix m(n, n, 0.0);
  for (std::size_t i = 0; i < n; ++i) {
    m(i, i) = 1.0;
  }
  return m;
}

Matrix Matrix::zeros(std::size_t rows, std::size_t cols) {
  return Matrix(rows, cols, 0.0);
}

Matrix Matrix::ones(std::size_t rows, std::size_t cols) {
  return Matrix(rows, cols, 1.0);
}

std::size_t Matrix::index(std::size_t i, std::size_t j) const {
  return i * cols_ + j;
}

void Matrix::check_bounds(std::size_t i, std::size_t j) const {
  if (i >= rows_ || j >= cols_) {
    throw DimensionError("matrix index out of range");
  }
}

void Matrix::check_same_shape(const Matrix& other, const char* op) const {
  if (rows_ != other.rows_ || cols_ != other.cols_) {
    throw DimensionError(
        size_mismatch(op, rows_, cols_, other.rows_, other.cols_));
  }
}

double& Matrix::operator()(std::size_t i, std::size_t j) {
  check_bounds(i, j);
  return data_[index(i, j)];
}

const double& Matrix::operator()(std::size_t i, std::size_t j) const {
  check_bounds(i, j);
  return data_[index(i, j)];
}

Matrix Matrix::operator+(const Matrix& other) const {
  check_same_shape(other, "Matrix::operator+");
  Matrix result(rows_, cols_);
  for (std::size_t k = 0; k < data_.size(); ++k) {
    result.data_[k] = data_[k] + other.data_[k];
  }
  return result;
}

Matrix Matrix::operator-(const Matrix& other) const {
  check_same_shape(other, "Matrix::operator-");
  Matrix result(rows_, cols_);
  for (std::size_t k = 0; k < data_.size(); ++k) {
    result.data_[k] = data_[k] - other.data_[k];
  }
  return result;
}

Matrix Matrix::operator*(double scalar) const {
  Matrix result(rows_, cols_);
  for (std::size_t k = 0; k < data_.size(); ++k) {
    result.data_[k] = data_[k] * scalar;
  }
  return result;
}

Matrix Matrix::operator*(const Matrix& other) const {
  if (cols_ != other.rows_) {
    throw DimensionError(
        size_mismatch("Matrix::operator*", rows_, cols_, other.rows_, other.cols_));
  }
  Matrix result(rows_, other.cols_, 0.0);
  for (std::size_t i = 0; i < rows_; ++i) {
    for (std::size_t k = 0; k < cols_; ++k) {
      const double aik = (*this)(i, k);
      for (std::size_t j = 0; j < other.cols_; ++j) {
        result(i, j) += aik * other(k, j);
      }
    }
  }
  return result;
}

Vector Matrix::operator*(const Vector& v) const {
  if (cols_ != v.size()) {
    throw DimensionError(
        size_mismatch("Matrix::operator*(Vector)", rows_, cols_, v.size(), 1));
  }
  Vector result(rows_, 0.0);
  for (std::size_t i = 0; i < rows_; ++i) {
    double sum = 0.0;
    for (std::size_t j = 0; j < cols_; ++j) {
      sum += (*this)(i, j) * v[j];
    }
    result[i] = sum;
  }
  return result;
}

Matrix& Matrix::operator+=(const Matrix& other) {
  check_same_shape(other, "Matrix::operator+=");
  for (std::size_t k = 0; k < data_.size(); ++k) {
    data_[k] += other.data_[k];
  }
  return *this;
}

Matrix& Matrix::operator-=(const Matrix& other) {
  check_same_shape(other, "Matrix::operator-=");
  for (std::size_t k = 0; k < data_.size(); ++k) {
    data_[k] -= other.data_[k];
  }
  return *this;
}

Matrix& Matrix::operator*=(double scalar) {
  for (double& value : data_) {
    value *= scalar;
  }
  return *this;
}

Matrix Matrix::transposed() const {
  Matrix result(cols_, rows_);
  for (std::size_t i = 0; i < rows_; ++i) {
    for (std::size_t j = 0; j < cols_; ++j) {
      result(j, i) = (*this)(i, j);
    }
  }
  return result;
}

Vector Matrix::row_sums() const {
  Vector sums(rows_, 0.0);
  for (std::size_t i = 0; i < rows_; ++i) {
    double total = 0.0;
    for (std::size_t j = 0; j < cols_; ++j) {
      total += (*this)(i, j);
    }
    sums[i] = total;
  }
  return sums;
}

Vector Matrix::col_sums() const {
  Vector sums(cols_, 0.0);
  for (std::size_t j = 0; j < cols_; ++j) {
    double total = 0.0;
    for (std::size_t i = 0; i < rows_; ++i) {
      total += (*this)(i, j);
    }
    sums[j] = total;
  }
  return sums;
}

void Matrix::normalize_rows() {
  for (std::size_t i = 0; i < rows_; ++i) {
    double total = 0.0;
    for (std::size_t j = 0; j < cols_; ++j) {
      total += (*this)(i, j);
    }
    if (total == 0.0) {
      continue;
    }
    for (std::size_t j = 0; j < cols_; ++j) {
      (*this)(i, j) /= total;
    }
  }
}

void Matrix::normalize_cols() {
  for (std::size_t j = 0; j < cols_; ++j) {
    double total = 0.0;
    for (std::size_t i = 0; i < rows_; ++i) {
      total += (*this)(i, j);
    }
    if (total == 0.0) {
      continue;
    }
    for (std::size_t i = 0; i < rows_; ++i) {
      (*this)(i, j) /= total;
    }
  }
}

Matrix Matrix::rows_normalized() const {
  Matrix result = *this;
  result.normalize_rows();
  return result;
}

Matrix Matrix::cols_normalized() const {
  Matrix result = *this;
  result.normalize_cols();
  return result;
}

bool Matrix::is_equal(const Matrix& other, double abs_tol) const {
  if (rows_ != other.rows_ || cols_ != other.cols_) {
    return false;
  }
  for (std::size_t k = 0; k < data_.size(); ++k) {
    if (std::abs(data_[k] - other.data_[k]) > abs_tol) {
      return false;
    }
  }
  return true;
}

bool Matrix::is_near(const Matrix& other,
                     double abs_tol,
                     double rel_tol) const {
  if (rows_ != other.rows_ || cols_ != other.cols_) {
    return false;
  }
  for (std::size_t k = 0; k < data_.size(); ++k) {
    if (!values_near(data_[k], other.data_[k], abs_tol, rel_tol)) {
      return false;
    }
  }
  return true;
}

Matrix operator*(double scalar, const Matrix& m) {
  return m * scalar;
}

}  // namespace cppanp
