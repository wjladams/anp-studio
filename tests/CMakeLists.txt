include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(cppanp_tests
  test_matrix.cpp
  test_vector.cpp
  test_eigen.cpp
  test_inconsistency.cpp
  test_pairwise.cpp
  test_limit_matrix.cpp
  test_network.cpp
  test_subnet.cpp
  test_synthesis.cpp
  test_json_io.cpp
)

target_link_libraries(cppanp_tests
  PRIVATE
    cppanp_core
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(cppanp_tests)
