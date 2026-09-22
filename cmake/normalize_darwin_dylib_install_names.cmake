if(NOT DEFINED CPKT_DARWIN_LIBRARY_DIR OR CPKT_DARWIN_LIBRARY_DIR STREQUAL "")
  message(FATAL_ERROR "CPKT_DARWIN_LIBRARY_DIR is required")
endif()
if(NOT DEFINED CPKT_DARWIN_INSTALL_NAME_TOOL OR CPKT_DARWIN_INSTALL_NAME_TOOL STREQUAL "")
  message(FATAL_ERROR "CPKT_DARWIN_INSTALL_NAME_TOOL is required")
endif()
if(NOT DEFINED CPKT_DARWIN_OTOOL OR CPKT_DARWIN_OTOOL STREQUAL "")
  message(FATAL_ERROR "CPKT_DARWIN_OTOOL is required")
endif()

foreach(_required_path
    "${CPKT_DARWIN_LIBRARY_DIR}"
    "${CPKT_DARWIN_INSTALL_NAME_TOOL}"
    "${CPKT_DARWIN_OTOOL}")
  if(NOT EXISTS "${_required_path}")
    message(FATAL_ERROR "Darwin install-name normalizer input is missing: ${_required_path}")
  endif()
endforeach()

# Loadable bundles may live below lib/ (for example Cyrus SASL's .so files in
# lib/sasl2). They need dependency rewrites but have no dylib install ID.
file(GLOB_RECURSE _dylib_candidates
  "${CPKT_DARWIN_LIBRARY_DIR}/*.dylib"
  "${CPKT_DARWIN_LIBRARY_DIR}/*.so")
set(_dylibs "")
set(_dylib_names "")
foreach(_candidate IN LISTS _dylib_candidates)
  # Mutating a symlink alters its referent.  Normalize the real library once;
  # its conventional link names retain the same Mach-O identity.
  if(IS_SYMLINK "${_candidate}")
    continue()
  endif()
  get_filename_component(_candidate_name "${_candidate}" NAME)
  list(APPEND _dylibs "${_candidate}")
  list(APPEND _dylib_names "${_candidate_name}")
endforeach()

if(NOT _dylibs)
  message(FATAL_ERROR "No real Darwin dylibs found in ${CPKT_DARWIN_LIBRARY_DIR}")
endif()

foreach(_dylib IN LISTS _dylibs)
  get_filename_component(_dylib_name "${_dylib}" NAME)
  execute_process(
    COMMAND "${CPKT_DARWIN_OTOOL}" -L "${_dylib}"
    RESULT_VARIABLE _otool_result
    OUTPUT_VARIABLE _otool_output
    ERROR_VARIABLE _otool_error)
  if(NOT _otool_result EQUAL 0)
    message(FATAL_ERROR "failed to inspect ${_dylib}: ${_otool_error}")
  endif()

  string(REPLACE "\r\n" "\n" _otool_output "${_otool_output}")
  string(REPLACE "\n" ";" _otool_lines "${_otool_output}")
  foreach(_otool_line IN LISTS _otool_lines)
    string(STRIP "${_otool_line}" _otool_line)
    string(REGEX MATCH "^[^ \t(]+" _load_path "${_otool_line}")
    if(NOT _load_path MATCHES "^/usr/lib/([^/]+\\.dylib)$")
      continue()
    endif()
    set(_load_name "${CMAKE_MATCH_1}")
    list(FIND _dylib_names "${_load_name}" _bundled_index)
    if(_bundled_index EQUAL -1)
      # This is a real Darwin system dependency, not another library in this
      # staged bundle, and must retain its platform-owned install name.
      continue()
    endif()
    execute_process(
      COMMAND "${CPKT_DARWIN_INSTALL_NAME_TOOL}"
        -change "${_load_path}" "@rpath/${_load_name}" "${_dylib}"
      RESULT_VARIABLE _change_result
      ERROR_VARIABLE _change_error)
    if(NOT _change_result EQUAL 0)
      message(FATAL_ERROR
        "failed to rewrite bundled Darwin dependency ${_load_path} in ${_dylib}: ${_change_error}")
    endif()
  endforeach()

  if(_dylib_name MATCHES "[.]dylib$")
    execute_process(
      COMMAND "${CPKT_DARWIN_INSTALL_NAME_TOOL}" -id "@rpath/${_dylib_name}" "${_dylib}"
      RESULT_VARIABLE _id_result
      ERROR_VARIABLE _id_error)
    if(NOT _id_result EQUAL 0)
      message(FATAL_ERROR "failed to set Darwin install name for ${_dylib}: ${_id_error}")
    endif()
  endif()
endforeach()
