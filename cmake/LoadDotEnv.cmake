# Load KEY=VALUE pairs from a dotenv file into CMake variables.
# Lines that are empty or start with # are ignored. Values may be quoted.
#
# Usage:
#   include(cmake/LoadDotEnv.cmake)
#   anpstudio_load_dotenv("${CMAKE_SOURCE_DIR}/.env")

function(anpstudio_load_dotenv env_file)
  if(NOT EXISTS "${env_file}")
    return()
  endif()
  file(STRINGS "${env_file}" _anpstudio_dotenv_lines)
  foreach(_line IN LISTS _anpstudio_dotenv_lines)
    string(STRIP "${_line}" _line)
    if(_line STREQUAL "" OR _line MATCHES "^#")
      continue()
    endif()
    if(NOT _line MATCHES "^([A-Za-z_][A-Za-z0-9_]*)=(.*)$")
      continue()
    endif()
    set(_key "${CMAKE_MATCH_1}")
    set(_val "${CMAKE_MATCH_2}")
    # Strip optional surrounding quotes.
    if(_val MATCHES "^\"(.*)\"$")
      set(_val "${CMAKE_MATCH_1}")
    elseif(_val MATCHES "^'(.*)'$")
      set(_val "${CMAKE_MATCH_1}")
    endif()
    set(${_key} "${_val}" PARENT_SCOPE)
  endforeach()
endfunction()
