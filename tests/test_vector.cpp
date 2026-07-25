#include "cppanp/matrix.hpp"

#include <gtest/gtest.h>

#include <utility>

using cppanp::DimensionError;
using cppanp::Vector;

TEST(VectorTest, DefaultIsEmpty) {
  const Vector v;
  EXPECT_EQ(v.size(), 0u);
  EXPECT_TRUE(v.empty());
}

TEST(VectorTest, SizedConstructorFills) {
  const Vector v(4, 2.5);
  EXPECT_EQ(v.size(), 4u);
  EXPECT_DOUBLE_EQ(v[0], 2.5);
  EXPECT_DOUBLE_EQ(v[3], 2.5);
}

TEST(VectorTest, IndexOutOfRangeThrows) {
  Vector v(2);
  EXPECT_THROW(v[2], DimensionError);
  EXPECT_THROW((void)static_cast<const Vector&>(v)[5], DimensionError);
}

TEST(VectorTest, AdditionAndSubtraction) {
  Vector a(3);
  a[0] = 1;
  a[1] = 2;
  a[2] = 3;

  Vector b(3);
  b[0] = 3;
  b[1] = 2;
  b[2] = 1;

  const Vector sum = a + b;
  EXPECT_DOUBLE_EQ(sum[0], 4.0);
  EXPECT_DOUBLE_EQ(sum[1], 4.0);
  EXPECT_DOUBLE_EQ(sum[2], 4.0);

  const Vector diff = a - b;
  EXPECT_DOUBLE_EQ(diff[0], -2.0);
  EXPECT_DOUBLE_EQ(diff[1], 0.0);
  EXPECT_DOUBLE_EQ(diff[2], 2.0);
}

TEST(VectorTest, CompoundAssignment) {
  Vector a(2, 1.0);
  Vector b(2, 2.0);
  a += b;
  EXPECT_TRUE(a.is_equal(Vector(2, 3.0)));
  a -= b;
  EXPECT_TRUE(a.is_equal(Vector(2, 1.0)));
  a *= 5.0;
  EXPECT_TRUE(a.is_equal(Vector(2, 5.0)));
}

TEST(VectorTest, ScalarMultiply) {
  const Vector a(3, 1.5);
  const Vector b = a * 2.0;
  const Vector c = 2.0 * a;
  EXPECT_TRUE(b.is_equal(Vector(3, 3.0)));
  EXPECT_TRUE(c.is_equal(b));
}

TEST(VectorTest, DimensionMismatchThrows) {
  const Vector a(2);
  const Vector b(3);
  EXPECT_THROW(a + b, DimensionError);
  EXPECT_THROW(a - b, DimensionError);
}

TEST(VectorTest, SumAndNormalize) {
  Vector v(3);
  v[0] = 1;
  v[1] = 2;
  v[2] = 3;
  EXPECT_DOUBLE_EQ(v.sum(), 6.0);

  const Vector n = v.normalized();
  EXPECT_NEAR(n[0], 1.0 / 6.0, 1e-12);
  EXPECT_NEAR(n[1], 2.0 / 6.0, 1e-12);
  EXPECT_NEAR(n[2], 3.0 / 6.0, 1e-12);
  EXPECT_NEAR(n.sum(), 1.0, 1e-12);
}

TEST(VectorTest, NormalizeZeroVectorLeavesUnchanged) {
  Vector v(3, 0.0);
  v.normalize();
  EXPECT_TRUE(v.is_equal(Vector(3, 0.0)));
}

TEST(VectorTest, NearEqualityUsesRelativeTolerance) {
  Vector a(1, 1000.0);
  Vector b(1, 1000.0 + 1e-6);
  EXPECT_TRUE(a.is_near(b, 0.0, 1e-8));
  EXPECT_FALSE(a.is_equal(b, 1e-12));
}

TEST(VectorTest, CopyAndMove) {
  Vector a(3, 7.0);
  Vector b = a;
  EXPECT_TRUE(a.is_equal(b));
  Vector c = std::move(b);
  EXPECT_TRUE(a.is_equal(c));
}
