cmake_minimum_required(VERSION 3.20)

project(cppanp
  VERSION 0.1.0
  DESCRIPTION "Analytic Network Process desktop application"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CPPANP_BUILD_GUI "Build Qt 6 GUI application" ON)
option(CPPANP_BUILD_DOCS "Build Doxygen GUI documentation" ON)

# Prefer a sibling ../libanpcpp checkout during development; otherwise fetch
# from GitHub (pin GIT_TAG once libanpcpp publishes releases).
if(NOT ANPCPP_SOURCE_DIR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../libanpcpp/CMakeLists.txt")
  set(ANPCPP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../libanpcpp" CACHE PATH
      "Path to libanpcpp source tree")
endif()

include(FetchContent)
if(ANPCPP_SOURCE_DIR)
  message(STATUS "Using local anpcpp from ${ANPCPP_SOURCE_DIR}")
  FetchContent_Declare(anpcpp SOURCE_DIR "${ANPCPP_SOURCE_DIR}")
else()
  FetchContent_Declare(anpcpp
    GIT_REPOSITORY https://github.com/wjadams/libanpcpp.git
    GIT_TAG v0.1.0
  )
endif()

set(ANPCPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ANPCPP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(anpcpp)

if(CPPANP_BUILD_GUI)
  find_package(Qt6 QUIET COMPONENTS Widgets)
  if(Qt6_FOUND)
    add_subdirectory(gui)
  else()
    message(WARNING "CPPANP_BUILD_GUI=ON but Qt6 Widgets was not found; skipping gui/")
  endif()
endif()

if(CPPANP_BUILD_DOCS)
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Doxygen.cmake")
endif()
