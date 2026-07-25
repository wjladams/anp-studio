# Example programs. Each source file becomes its own runnable executable that
# prints inputs, intermediate matrices, and results for one ANP model.

set(CPPANP_EXAMPLES
  tree134
  network23
  hamburger_std
  benefits_costs_subnet
)

foreach(example ${CPPANP_EXAMPLES})
  add_executable(${example} ${example}.cpp)
  target_link_libraries(${example} PRIVATE cppanp::core)
  target_include_directories(${example} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()
