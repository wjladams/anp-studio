# Doxygen documentation target for ANP Studio GUI.

find_package(Doxygen)

if(NOT DOXYGEN_FOUND)
  message(WARNING "Doxygen not found; ANPSTUDIO_BUILD_DOCS disabled")
  return()
endif()

set(DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/docs")
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in"
  "${DOXYGEN_OUTPUT_DIR}/Doxyfile"
  @ONLY
)

add_custom_target(anpstudio_docs
  COMMAND "${DOXYGEN_EXECUTABLE}" "${DOXYGEN_OUTPUT_DIR}/Doxyfile"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Generating ANP Studio GUI documentation"
  VERBATIM
)
