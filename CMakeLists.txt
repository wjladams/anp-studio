cmake_minimum_required(VERSION 3.20)

project(cppanp
  VERSION 0.1.0
  DESCRIPTION "Analytic Network Process library and tools"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CPPANP_BUILD_TESTS "Build unit tests" ON)
option(CPPANP_BUILD_EXAMPLES "Build example programs" ON)
option(CPPANP_BUILD_GUI "Build Qt 6 GUI application" ON)

include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(cppanp_core STATIC
  src/matrix.cpp
  src/eigen.cpp
  src/inconsistency.cpp
  src/pairwise.cpp
  src/limit_matrix.cpp
  src/network.cpp
  src/synthesis.cpp
  src/json_io.cpp
)

add_library(cppanp::core ALIAS cppanp_core)

target_include_directories(cppanp_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(cppanp_core PUBLIC nlohmann_json::nlohmann_json)

if(MSVC)
  target_compile_options(cppanp_core PRIVATE /W4)
else()
  target_compile_options(cppanp_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CPPANP_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(CPPANP_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(CPPANP_BUILD_GUI)
  find_package(Qt6 QUIET COMPONENTS Widgets)
  if(Qt6_FOUND)
    add_subdirectory(gui)
  else()
    message(WARNING "CPPANP_BUILD_GUI=ON but Qt6 Widgets was not found; skipping gui/")
  endif()
endif()
