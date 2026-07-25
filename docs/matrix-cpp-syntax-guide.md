# C++ syntax walkthrough: `src/matrix.cpp`

This guide is for someone who knows C (and other languages) and wants to
understand the C++ idioms used in `matrix.cpp` / `matrix.hpp`.

## First: `&` here is usually a *reference*, not a pointer

In C, `&` means “address of” (or bitwise AND). In C++ declarations, `T&` means
**reference to `T`**.

| Concept | C mental model | C++ syntax | Can be null? | Must rebind? |
|---|---|---|---|---|
| Pointer | `double* p` | `double* p` | Yes (`NULL`/`nullptr`) | Yes (`p = &x`) |
| Reference | “always-valid alias for an object” | `double& r` | No | No (bound at init) |

Examples:

```cpp
double x = 1.0;
double& r = x;   // r is another name for x
r = 2.0;         // changes x

double* p = &x;  // pointer: can be reseated, can be null
*p = 3.0;
```

**Important for this file:** almost nothing returns a *pointer*. When you see
`double&` or `Vector&` as a return type, that is a **reference** (an alias),
not `double*` / `Vector*`.

The only intentional raw-pointer APIs on these types are the inline
`data()` methods in the header (for C-style interop with the underlying
storage). They are not defined in `matrix.cpp`.

---

## Why pass `const Vector&` instead of `Vector` or `Vector*`?

For large objects (vectors/matrices owning heap data via `std::vector`):

1. **`Vector other` (by value)** — copies the whole object. Safe but expensive.
2. **`const Vector& other`** — no copy; caller’s object is visible read-only.
   Prefer this for “read input” parameters.
3. **`Vector* other`** — also avoids copy, but can be null, needs `->`, and
   signals optional/ownership more than “required input”. Modern C++ prefers
   references for required inputs.

Rule of thumb used here:

- **Inputs we only read** → `const T&`
- **Cheap scalars** (`double`, `std::size_t`) → by value
- **Outputs that are new objects** → return `T` by value
- **Mutate existing object / expose an element** → return `T&` (reference)
- **In-place mutation with no useful return** → `void`

Modern C++ also relies on **Return Value Optimization (RVO)** / moves, so
returning a `Vector`/`Matrix` by value is normal and usually cheap enough
(often no deep copy of the temporary).

---

## File-level C++ details (before the functions)

```cpp
#include "cppanp/matrix.hpp"
#include <cmath>
#include <sstream>

namespace cppanp {
namespace {
  // helpers...
}  // namespace
```

| Detail | C analogy / meaning |
|---|---|
| `#include "..."` vs `<...>` | Project header vs standard library. |
| `namespace cppanp { ... }` | Like a package/module name; avoids global-name clashes. Qualifies symbols as `cppanp::Vector`. |
| Anonymous `namespace { }` | Like `static` file-local linkage in C: helpers visible only in this `.cpp`. |
| `[[nodiscard]]` | Attribute: compiler warns if you ignore the return value. |
| `std::size_t` | Unsigned size type (like C’s `size_t`, in namespace `std`). |
| `std::ostringstream` | String-building stream (like `snprintf` into a growing buffer). |
| `throw DimensionError(...)` | Exceptions instead of `errno` / out-params. Unwinds the stack until caught. |
| `Class::method` | Method definition outside the class body. |
| Trailing `const` on methods | Promises “does not modify `*this`”. Required to call on `const Vector`/`const Matrix`. |
| `*this` | The current object (like the implicit `this` pointer in C++ methods; `*this` is the object itself). |
| Range-for `for (double& value : data_)` | Like iterating an array, but `value` can be a reference into the container. |
| Member initializer `: data_(size, fill)` | Constructs members before the constructor body runs. |

---

# Anonymous helpers

## `values_near`

```cpp
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
```

### `&` usage
None. All parameters are `double` by value (cheap scalars).

### Return: value, not pointer/reference
Returns `bool` by value. No heap object to alias.

### C→C++ notes
- `std::abs` / `std::max` live in `<cmath>` / `<algorithm>`-style overload sets; prefer `std::` over C’s macros/functions when mixing types.
- `const double diff` is a local constant (C allows this too in C99+); C++ culture uses `const` liberally for locals.

---

## `size_mismatch`

```cpp
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
```

### `&` usage
None in the signature. `op` is a C-style `const char*` string literal / pointer (fine for read-only C strings).

### Return: `std::string` by value
Returns an owning string object, not a `char*`. Caller does not `free` it; the string destructor frees its buffer automatically (**RAII**).

### C→C++ notes
- Returning `std::string` is the idiomatic replacement for “malloc a message and hope the caller frees it.”
- `oss << ...` overloads `operator<<` for streaming (like Tcl string append / Python f-strings built piecewise).

---

# `Vector`

## `Vector::Vector` (constructor)

```cpp
Vector::Vector(std::size_t size, double fill) : data_(size, fill) {}
```

### `&` usage
None. Scalars by value.

### Return
Constructors have no return type. They initialize a new object.

### C→C++ notes
- `: data_(size, fill)` is a **member initializer list**. It constructs the
  `std::vector<double>` with `size` elements set to `fill` (like
  `calloc`+fill, but owned by the vector).
- Empty body `{}` means “nothing else to do after members are built.”
- In the header, `explicit Vector(...)` prevents silent conversions like
  `Vector v = 3;` from compiling unintentionally.

---

## `Vector::operator[]` (mutable)

```cpp
double& Vector::operator[](std::size_t i) {
  if (i >= data_.size()) {
    throw DimensionError("vector index out of range");
  }
  return data_[i];
}
```

### `&` usage
**Returns `double&`:** an alias to the stored element so callers can write:

```cpp
v[i] = 3.14;   // modifies the vector’s data
```

If it returned `double` by value, assignment to `v[i]` would be impossible
(you’d only get a temporary copy).

### Pointer vs reference return
Reference, not pointer. Callers write `v[i]`, not `*v[i]` or `v->...`.
A pointer return would force awkward syntax and invite null checks that are
unnecessary here.

### C→C++ notes
- `operator[]` overloads the `[]` syntax for the class.
- Bounds check + `throw` replaces C’s undefined behavior on out-of-range access
  (or manual `assert`).
- Dual overloads (see next) exist so `const Vector` can still be read.

---

## `Vector::operator[]` (const)

```cpp
const double& Vector::operator[](std::size_t i) const {
  if (i >= data_.size()) {
    throw DimensionError("vector index out of range");
  }
  return data_[i];
}
```

### `&` usage
Returns `const double&`: alias for reading only. Prevents `v[i] = ...` when
`v` is const.

Trailing `const` on the method means “this method does not mutate the
`Vector`.”

### Why not return `double` by value?
Returning `const double&` avoids copying and keeps the same shape as the
mutable overload. For `double`, copy is cheap either way; the pattern matters
more for larger types. Consistency with matrix element access is the main win.

### C→C++ notes
C has no `const` member functions. In C++ they are how you mark read-only
methods and enable calls through `const Vector&` parameters.

---

## `Vector::check_same_size`

```cpp
void Vector::check_same_size(const Vector& other, const char* op) const {
  if (data_.size() != other.data_.size()) {
    throw DimensionError(size_mismatch(op, data_.size(), 1, other.data_.size(), 1));
  }
}
```

### `&` usage
`const Vector& other`: required input, no copy, no mutation of `other`.

### Return
`void` — either succeeds or throws. No status code.

### C→C++ notes
Private helper (declared under `private:` in the header). Exceptions replace
`if (err) return -1;` patterns common in C APIs.

---

## `Vector::operator+`

```cpp
Vector Vector::operator+(const Vector& other) const {
  check_same_size(other, "Vector::operator+");
  Vector result(data_.size());
  for (std::size_t i = 0; i < data_.size(); ++i) {
    result.data_[i] = data_[i] + other.data_[i];
  }
  return result;
}
```

### `&` usage
- Parameter: `const Vector& other` (read-only, no copy of input).
- Return: `Vector` **by value** (a new vector). Not `Vector&`, because the
  result is a local temporary; returning a reference to `result` would dangle
  (classic C++ bug, analogous to returning a pointer to a local stack variable
  in C).

### Why value, not pointer?
Returning `Vector*` would force heap allocation (`new`) and a manual/`unique_ptr`
ownership story. Returning by value lets RAII + move/RVO handle lifetime.

### C→C++ notes
`a + b` becomes `a.operator+(b)`. The method is `const` because addition does
not mutate `a`.

---

## `Vector::operator-`

```cpp
Vector Vector::operator-(const Vector& other) const {
  check_same_size(other, "Vector::operator-");
  Vector result(data_.size());
  for (std::size_t i = 0; i < data_.size(); ++i) {
    result.data_[i] = data_[i] - other.data_[i];
  }
  return result;
}
```

Same pattern as `operator+`: `const Vector&` in, `Vector` out by value.

---

## `Vector::operator*` (scalar)

```cpp
Vector Vector::operator*(double scalar) const {
  Vector result(data_.size());
  for (std::size_t i = 0; i < data_.size(); ++i) {
    result.data_[i] = data_[i] * scalar;
  }
  return result;
}
```

### `&` usage
None on parameters (`double` by value). Returns new `Vector` by value.

### C→C++ notes
This enables `v * 2.0`. The free function below enables `2.0 * v`.

---

## `Vector::operator+=`

```cpp
Vector& Vector::operator+=(const Vector& other) {
  check_same_size(other, "Vector::operator+=");
  for (std::size_t i = 0; i < data_.size(); ++i) {
    data_[i] += other.data_[i];
  }
  return *this;
}
```

### `&` usage
- Input: `const Vector& other`.
- Return: `Vector&` — reference to **this same object**, not a copy.

### Why return a reference?
Supports chaining like built-in types:

```cpp
(a += b) += c;
```

Also matches the conventional signature of compound-assignment operators.

### Why not pointer?
`return this;` would be `Vector*`. Returning `*this` as `Vector&` keeps usage
natural (`a += b`) without pointer syntax.

### C→C++ notes
Not `const`: it mutates the left-hand side.

---

## `Vector::operator-=`

```cpp
Vector& Vector::operator-=(const Vector& other) {
  check_same_size(other, "Vector::operator-=");
  for (std::size_t i = 0; i < data_.size(); ++i) {
    data_[i] -= other.data_[i];
  }
  return *this;
}
```

Same idiom as `operator+=`: mutate in place, return `*this` by reference.

---

## `Vector::operator*=` (scalar)

```cpp
Vector& Vector::operator*=(double scalar) {
  for (double& value : data_) {
    value *= scalar;
  }
  return *this;
}
```

### `&` usage
- Return: `Vector&` (`*this`).
- Loop: `double& value` binds to each element so `value *= scalar` writes
  through into `data_`.

If the loop were `for (double value : data_)`, `value` would be a **copy**,
and multiplying it would not change the vector.

### C→C++ notes
Range-based for is C++11+. Think: “foreach with optional reference binding.”

---

## `Vector::sum`

```cpp
double Vector::sum() const {
  double total = 0.0;
  for (double value : data_) {
    total += value;
  }
  return total;
}
```

### `&` usage
None needed. Loop uses copies of `double` (fine; cheap). Method is `const`.

### Return
`double` by value.

---

## `Vector::normalize`

```cpp
void Vector::normalize() {
  const double total = sum();
  if (total == 0.0) {
    return;
  }
  for (double& value : data_) {
    value /= total;
  }
}
```

### `&` usage
`double&` in the loop to mutate elements in place.

### Return
`void` — in-place API. Contrast with `normalized()` below.

### C→C++ notes
Naming convention used here:
- `normalize()` → mutate self
- `normalized()` → return a new object, leave self unchanged

---

## `Vector::normalized`

```cpp
Vector Vector::normalized() const {
  Vector result = *this;
  result.normalize();
  return result;
}
```

### `&` usage
None in the signature. `*this` copies the current object into `result`
(uses the copy constructor).

### Return by value
New vector; must not return `Vector&` to `result` (would dangle).

### C→C++ notes
`Vector result = *this;` is copy construction. Because `std::vector` owns its
buffer, this deep-copies the numeric data.

---

## `Vector::is_equal`

```cpp
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
```

### `&` usage
`const Vector& other` — compare without copying.

### Return
`bool` by value. Default `abs_tol` is declared in the **header**, not here
(C++ default arguments belong on the declaration).

---

## `Vector::is_near`

```cpp
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
```

Same parameter/return pattern as `is_equal`.

---

## Free function: `operator*(double, const Vector&)`

```cpp
Vector operator*(double scalar, const Vector& v) {
  return v * scalar;
}
```

### `&` usage
`const Vector& v` — no copy of the vector.

### Why a free function (not a method)?
A method only receives the left operand as `*this`. For `2.0 * v`, the left
operand is `double`, so it cannot be `Vector::operator*`. A non-member
function fixes left-scalar multiplication.

Declared `friend` in the header so it is part of the class’s public operator
set (and could access privates if needed; here it does not need to).

### Return
New `Vector` by value.

---

# `Matrix`

## `Matrix::Matrix` (constructor)

```cpp
Matrix::Matrix(std::size_t rows, std::size_t cols, double fill)
    : rows_(rows), cols_(cols), data_(rows * cols, fill) {
  if ((rows == 0) != (cols == 0)) {
    throw DimensionError("matrix cannot have a zero dimension unless both are zero");
  }
}
```

### `&` usage
None. Scalars by value.

### C→C++ notes
- Members `rows_`, `cols_`, `data_` are initialized in the initializer list.
- Flat row-major storage: index `i * cols_ + j` (same idea as a C 2D array
  packed into one buffer).
- `(rows == 0) != (cols == 0)` is XOR-style “exactly one is zero.”

---

## `Matrix::identity` (static)

```cpp
Matrix Matrix::identity(std::size_t n) {
  Matrix m(n, n, 0.0);
  for (std::size_t i = 0; i < n; ++i) {
    m(i, i) = 1.0;
  }
  return m;
}
```

### `&` usage
None in parameters. Returns `Matrix` by value.

### C→C++ notes
`static` method: no `this`. Call as `Matrix::identity(3)` (like a namespaced
factory function). Uses `operator()` for element access (`m(i, i)`).

---

## `Matrix::zeros` / `Matrix::ones`

```cpp
Matrix Matrix::zeros(std::size_t rows, std::size_t cols) {
  return Matrix(rows, cols, 0.0);
}

Matrix Matrix::ones(std::size_t rows, std::size_t cols) {
  return Matrix(rows, cols, 1.0);
}
```

### `&` usage
None. Factory functions returning new matrices by value.

### C→C++ notes
`return Matrix(...);` constructs a temporary returned to the caller (RVO
typically elides extra copies).

---

## `Matrix::index`

```cpp
std::size_t Matrix::index(std::size_t i, std::size_t j) const {
  return i * cols_ + j;
}
```

Private helper. Scalars in/out. `const` because it only reads `cols_`.

---

## `Matrix::check_bounds`

```cpp
void Matrix::check_bounds(std::size_t i, std::size_t j) const {
  if (i >= rows_ || j >= cols_) {
    throw DimensionError("matrix index out of range");
  }
}
```

`void` + throw. No references needed.

---

## `Matrix::check_same_shape`

```cpp
void Matrix::check_same_shape(const Matrix& other, const char* op) const {
  if (rows_ != other.rows_ || cols_ != other.cols_) {
    throw DimensionError(
        size_mismatch(op, rows_, cols_, other.rows_, other.cols_));
  }
}
```

### `&` usage
`const Matrix& other` — compare shape without copying matrix data.

---

## `Matrix::operator()` (mutable)

```cpp
double& Matrix::operator()(std::size_t i, std::size_t j) {
  check_bounds(i, j);
  return data_[index(i, j)];
}
```

### `&` usage
Returns `double&` so `m(i, j) = 1.0` works.

### Why `()` instead of `[]`?
C++ `operator[]` can take only one argument historically (C++23 eases this).
For 2D indexing, `operator()` is the common idiom: `m(i, j)`.

### Pointer vs reference
Same rationale as `Vector::operator[]`: reference keeps assignment syntax
natural and non-nullable.

---

## `Matrix::operator()` (const)

```cpp
const double& Matrix::operator()(std::size_t i, std::size_t j) const {
  check_bounds(i, j);
  return data_[index(i, j)];
}
```

Read-only sibling of the mutable overload. Trailing `const` + `const double&`.

---

## `Matrix::operator+` / `operator-`

```cpp
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
```

### Pattern
- In: `const Matrix&`
- Out: new `Matrix` by value
- Method `const`

Elementwise loops over the flat buffer (faster/simpler than nested `i,j` here).

---

## `Matrix::operator*` (scalar)

```cpp
Matrix Matrix::operator*(double scalar) const {
  Matrix result(rows_, cols_);
  for (std::size_t k = 0; k < data_.size(); ++k) {
    result.data_[k] = data_[k] * scalar;
  }
  return result;
}
```

Scalar by value; matrix result by value.

---

## `Matrix::operator*` (matrix×matrix)

```cpp
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
```

### `&` usage
`const Matrix& other`. Result by value.

### C→C++ notes
- `(*this)(i, k)` calls `operator()` on the current object. Parentheses are
  needed because `*this(i, k)` would be parsed wrong.
- Overload resolution: `m * scalar` vs `m * m2` picks the matching
  `operator*` by argument types (like Java overload resolution).

---

## `Matrix::operator*` (matrix×vector)

```cpp
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
```

### `&` usage
`const Vector& v`. Returns a new `Vector` by value (not a pointer into the
matrix).

---

## `Matrix::operator+=` / `operator-=` / `operator*=`

```cpp
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
```

### Pattern
Mutate left-hand side; return `Matrix&` (`*this`) for chaining.
`operator*=` only covers scalar multiply-assign here (not matrix multiply-assign).

---

## `Matrix::transposed`

```cpp
Matrix Matrix::transposed() const {
  Matrix result(cols_, rows_);
  for (std::size_t i = 0; i < rows_; ++i) {
    for (std::size_t j = 0; j < cols_; ++j) {
      result(j, i) = (*this)(i, j);
    }
  }
  return result;
}
```

Returns a **new** matrix by value. Name `transposed` (not `transpose`) signals
non-mutating, matching `normalized` vs `normalize`.

---

## `Matrix::row_sums` / `col_sums`

```cpp
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
```

### Return by value
New `Vector` results. Returning `Vector&` to a local would be undefined
behavior (dangling), same as returning `&local_array` in C.

---

## `Matrix::normalize_rows` / `normalize_cols`

```cpp
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
```

### Return
`void` — in-place. Uses `(*this)(i, j)` which returns `double&`, so `/=`
mutates storage.

---

## `Matrix::rows_normalized` / `cols_normalized`

```cpp
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
```

Copy → mutate copy → return by value. Original matrix unchanged (`const`
method).

---

## `Matrix::is_equal` / `is_near`

```cpp
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
```

`const Matrix&` inputs; `bool` results. Defaults live in the header.

---

## Free function: `operator*(double, const Matrix&)`

```cpp
Matrix operator*(double scalar, const Matrix& m) {
  return m * scalar;
}
```

Same left-scalar trick as for `Vector`.

---

# Header-only pieces worth knowing (not in `.cpp`)

These are defined inline in `matrix.hpp` but complete the picture:

### Special member functions (Rule of Zero / Five)

```cpp
Vector(const Vector&) = default;
Vector(Vector&&) noexcept = default;
Vector& operator=(const Vector&) = default;
Vector& operator=(Vector&&) noexcept = default;
```

| Piece | Meaning for a C person |
|---|---|
| Copy ctor / copy `=` | Deep copy via `std::vector`’s copy (like duplicating a malloc’d buffer). |
| Move ctor / move `=` (`&&`) | Steal resources from a temporary (pointer hand-off, leave source empty). No C equivalent built-in; huge performance win. |
| `= default` | “Compiler, generate the obvious correct version.” |
| `noexcept` | Promises not to throw; enables stronger optimizations/containers guarantees. |

Because the only owned resource is `std::vector`, defaulted special members are
correct (**Rule of Zero**): you don’t write manual destructor/`free`.

### Actual pointer returns: `data()`

```cpp
double* data() noexcept { return data_.data(); }
const double* data() const noexcept { return data_.data(); }
```

These **do** return pointers: a raw view into contiguous storage for C APIs /
BLAS-style code. They do **not** transfer ownership. Do not `delete` them.
Lifetime ends when the `Vector`/`Matrix` is destroyed.

### `[[nodiscard]]` on accessors
Encourages treating getters/`operator+` results as meaningful, not ignorable.

---

# Cheat sheet: what this file chooses and why

| Situation | Choice | Why |
|---|---|---|
| Read a big object argument | `const T&` | No copy; not optional; can’t be null |
| Small scalar argument | `T` by value | Cheaper/simpler than reference |
| Create a new vector/matrix | return `T` | Clear ownership; no dangling; RVO/moves |
| Expose an assignable element | return `T&` | Enables `v[i] = ...` / `m(i,j) = ...` |
| Compound assignment | return `T&` (`*this`) | Chaining; conventional |
| In-place normalize | `void` | Side effect is the point |
| Non-mutating normalize | return `T` | Keep original intact |
| Error | `throw` | No ignored status codes |
| Own a buffer | `std::vector` | Automatic free; deep copy/move handled |
| Optional C interop | `double* data()` | Escape hatch only |

## What you will *not* see much of here

- Raw owning pointers (`new` / `delete`)
- Returning pointers to locals
- Output parameters via `T*` when a return value will do
- Manual memory management for matrix storage

That is deliberate modern C++ style: **values and references first; raw
pointers only for non-owning views or C interop.**
