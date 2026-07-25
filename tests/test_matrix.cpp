#include "cppanp/matrix.hpp"

#include <gtest/gtest.h>

using cppanp::DimensionError;
using cppanp::Matrix;
using cppanp::Vector;

TEST(MatrixTest, DefaultIsEmpty) {
  const Matrix m;
  EXPECT_EQ(m.rows(), 0u);
  EXPECT_EQ(m.cols(), 0u);
  EXPECT_TRUE(m.empty());
}

TEST(MatrixTest, SizedConstructorFills) {
  const Matrix m(2, 3, 1.5);
  EXPECT_EQ(m.rows(), 2u);
  EXPECT_EQ(m.cols(), 3u);
  EXPECT_DOUBLE_EQ(m(0, 0), 1.5);
  EXPECT_DOUBLE_EQ(m(1, 2), 1.5);
}

TEST(MatrixTest, RejectsOneSidedZeroDimension) {
  EXPECT_THROW(Matrix(2, 0), DimensionError);
  EXPECT_THROW(Matrix(0, 3), DimensionError);
}

TEST(MatrixTest, Identity) {
  const Matrix i = Matrix::identity(3);
  EXPECT_DOUBLE_EQ(i(0, 0), 1.0);
  EXPECT_DOUBLE_EQ(i(1, 1), 1.0);
  EXPECT_DOUBLE_EQ(i(2, 2), 1.0);
  EXPECT_DOUBLE_EQ(i(0, 1), 0.0);
  EXPECT_DOUBLE_EQ(i(2, 0), 0.0);
}

TEST(MatrixTest, ZerosAndOnes) {
  const Matrix z = Matrix::zeros(2, 2);
  const Matrix o = Matrix::ones(2, 2);
  EXPECT_TRUE(z.is_equal(Matrix(2, 2, 0.0)));
  EXPECT_TRUE(o.is_equal(Matrix(2, 2, 1.0)));
}

TEST(MatrixTest, IndexOutOfRangeThrows) {
  Matrix m(2, 2);
  EXPECT_THROW(m(2, 0), DimensionError);
  EXPECT_THROW(m(0, 2), DimensionError);
  EXPECT_THROW((void)static_cast<const Matrix&>(m)(1, 5), DimensionError);
}

TEST(MatrixTest, AdditionAndSubtraction) {
  Matrix a(2, 2);
  a(0, 0) = 1;
  a(0, 1) = 2;
  a(1, 0) = 3;
  a(1, 1) = 4;

  Matrix b(2, 2);
  b(0, 0) = 4;
  b(0, 1) = 3;
  b(1, 0) = 2;
  b(1, 1) = 1;

  const Matrix sum = a + b;
  EXPECT_DOUBLE_EQ(sum(0, 0), 5.0);
  EXPECT_DOUBLE_EQ(sum(0, 1), 5.0);
  EXPECT_DOUBLE_EQ(sum(1, 0), 5.0);
  EXPECT_DOUBLE_EQ(sum(1, 1), 5.0);

  const Matrix diff = a - b;
  EXPECT_DOUBLE_EQ(diff(0, 0), -3.0);
  EXPECT_DOUBLE_EQ(diff(0, 1), -1.0);
  EXPECT_DOUBLE_EQ(diff(1, 0), 1.0);
  EXPECT_DOUBLE_EQ(diff(1, 1), 3.0);
}

TEST(MatrixTest, CompoundAssignment) {
  Matrix a(2, 2, 1.0);
  Matrix b(2, 2, 2.0);
  a += b;
  EXPECT_TRUE(a.is_equal(Matrix(2, 2, 3.0)));
  a -= b;
  EXPECT_TRUE(a.is_equal(Matrix(2, 2, 1.0)));
  a *= 4.0;
  EXPECT_TRUE(a.is_equal(Matrix(2, 2, 4.0)));
}

TEST(MatrixTest, ScalarMultiply) {
  const Matrix a(2, 2, 1.5);
  const Matrix b = a * 2.0;
  const Matrix c = 2.0 * a;
  EXPECT_TRUE(b.is_equal(Matrix(2, 2, 3.0)));
  EXPECT_TRUE(c.is_equal(b));
}

TEST(MatrixTest, MatrixMultiply) {
  Matrix a(2, 3);
  a(0, 0) = 1;
  a(0, 1) = 2;
  a(0, 2) = 3;
  a(1, 0) = 4;
  a(1, 1) = 5;
  a(1, 2) = 6;

  Matrix b(3, 2);
  b(0, 0) = 7;
  b(0, 1) = 8;
  b(1, 0) = 9;
  b(1, 1) = 10;
  b(2, 0) = 11;
  b(2, 1) = 12;

  const Matrix c = a * b;
  EXPECT_EQ(c.rows(), 2u);
  EXPECT_EQ(c.cols(), 2u);
  EXPECT_DOUBLE_EQ(c(0, 0), 58.0);
  EXPECT_DOUBLE_EQ(c(0, 1), 64.0);
  EXPECT_DOUBLE_EQ(c(1, 0), 139.0);
  EXPECT_DOUBLE_EQ(c(1, 1), 154.0);
}

TEST(MatrixTest, MatrixVectorMultiply) {
  Matrix a(2, 3);
  a(0, 0) = 1;
  a(0, 1) = 2;
  a(0, 2) = 3;
  a(1, 0) = 4;
  a(1, 1) = 5;
  a(1, 2) = 6;

  Vector v(3);
  v[0] = 1;
  v[1] = 2;
  v[2] = 3;

  const Vector r = a * v;
  EXPECT_EQ(r.size(), 2u);
  EXPECT_DOUBLE_EQ(r[0], 14.0);
  EXPECT_DOUBLE_EQ(r[1], 32.0);
}

TEST(MatrixTest, MultiplyDimensionMismatchThrows) {
  const Matrix a(2, 3);
  const Matrix b(2, 2);
  EXPECT_THROW(a * b, DimensionError);
  EXPECT_THROW(a * Vector(2), DimensionError);
}

TEST(MatrixTest, AddDimensionMismatchThrows) {
  const Matrix a(2, 2);
  const Matrix b(2, 3);
  EXPECT_THROW(a + b, DimensionError);
  EXPECT_THROW(a - b, DimensionError);
}

TEST(MatrixTest, Transpose) {
  Matrix a(2, 3);
  a(0, 0) = 1;
  a(0, 1) = 2;
  a(0, 2) = 3;
  a(1, 0) = 4;
  a(1, 1) = 5;
  a(1, 2) = 6;

  const Matrix t = a.transposed();
  EXPECT_EQ(t.rows(), 3u);
  EXPECT_EQ(t.cols(), 2u);
  EXPECT_DOUBLE_EQ(t(0, 1), 4.0);
  EXPECT_DOUBLE_EQ(t(2, 0), 3.0);
  EXPECT_TRUE(t.transposed().is_equal(a));
}

TEST(MatrixTest, RowAndColSums) {
  Matrix a(2, 3);
  a(0, 0) = 1;
  a(0, 1) = 2;
  a(0, 2) = 3;
  a(1, 0) = 4;
  a(1, 1) = 5;
  a(1, 2) = 6;

  const Vector rows = a.row_sums();
  EXPECT_DOUBLE_EQ(rows[0], 6.0);
  EXPECT_DOUBLE_EQ(rows[1], 15.0);

  const Vector cols = a.col_sums();
  EXPECT_DOUBLE_EQ(cols[0], 5.0);
  EXPECT_DOUBLE_EQ(cols[1], 7.0);
  EXPECT_DOUBLE_EQ(cols[2], 9.0);
}

TEST(MatrixTest, NormalizeRowsAndCols) {
  Matrix a(2, 2);
  a(0, 0) = 1;
  a(0, 1) = 3;
  a(1, 0) = 2;
  a(1, 1) = 2;

  Matrix expected_rows(2, 2);
  expected_rows(0, 0) = 0.25;
  expected_rows(0, 1) = 0.75;
  expected_rows(1, 0) = 0.5;
  expected_rows(1, 1) = 0.5;
  EXPECT_TRUE(a.rows_normalized().is_near(expected_rows));

  Matrix b = a;
  b.normalize_cols();
  EXPECT_NEAR(b(0, 0), 1.0 / 3.0, 1e-12);
  EXPECT_NEAR(b(1, 0), 2.0 / 3.0, 1e-12);
  EXPECT_NEAR(b(0, 1), 3.0 / 5.0, 1e-12);
  EXPECT_NEAR(b(1, 1), 2.0 / 5.0, 1e-12);

  EXPECT_TRUE(a.cols_normalized().is_near(b));
}

TEST(MatrixTest, NormalizeZeroRowLeavesRowUnchanged) {
  Matrix a(2, 2);
  a(0, 0) = 0;
  a(0, 1) = 0;
  a(1, 0) = 1;
  a(1, 1) = 1;

  a.normalize_rows();
  EXPECT_DOUBLE_EQ(a(0, 0), 0.0);
  EXPECT_DOUBLE_EQ(a(0, 1), 0.0);
  EXPECT_DOUBLE_EQ(a(1, 0), 0.5);
  EXPECT_DOUBLE_EQ(a(1, 1), 0.5);
}

TEST(MatrixTest, NormalizeZeroColLeavesColUnchanged) {
  Matrix a(2, 2);
  a(0, 0) = 0;
  a(1, 0) = 0;
  a(0, 1) = 2;
  a(1, 1) = 2;

  a.normalize_cols();
  EXPECT_DOUBLE_EQ(a(0, 0), 0.0);
  EXPECT_DOUBLE_EQ(a(1, 0), 0.0);
  EXPECT_DOUBLE_EQ(a(0, 1), 0.5);
  EXPECT_DOUBLE_EQ(a(1, 1), 0.5);
}

TEST(MatrixTest, NearEqualityUsesRelativeTolerance) {
  Matrix a(1, 1, 1000.0);
  Matrix b(1, 1, 1000.0 + 1e-6);
  EXPECT_TRUE(a.is_near(b, 0.0, 1e-8));
  EXPECT_FALSE(a.is_equal(b, 1e-12));
}

TEST(MatrixTest, CopyAndMove) {
  Matrix a(2, 2, 3.0);
  Matrix b = a;
  EXPECT_TRUE(a.is_equal(b));
  Matrix c = std::move(b);
  EXPECT_TRUE(a.is_equal(c));
}
