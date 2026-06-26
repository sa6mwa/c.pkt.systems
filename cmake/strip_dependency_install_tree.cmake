if(NOT DEFINED CPKT_STRIP_ROOT OR CPKT_STRIP_ROOT STREQUAL "")
  message(FATAL_ERROR "CPKT_STRIP_ROOT is required")
endif()

if(NOT EXISTS "${CPKT_STRIP_ROOT}")
  message(FATAL_ERROR "dependency install tree does not exist: ${CPKT_STRIP_ROOT}")
endif()

if(NOT DEFINED CPKT_STRIP_BIN OR CPKT_STRIP_BIN STREQUAL "")
  message(FATAL_ERROR "CPKT_STRIP_BIN is required")
endif()

if(NOT EXISTS "${CPKT_STRIP_BIN}")
  message(FATAL_ERROR "strip tool does not exist: ${CPKT_STRIP_BIN}")
endif()

if(NOT DEFINED CPKT_STRIP_STATIC_ARCHIVES)
  set(CPKT_STRIP_STATIC_ARCHIVES ON)
endif()

file(GLOB_RECURSE _cpkt_strip_candidates LIST_DIRECTORIES false
  "${CPKT_STRIP_ROOT}/lib/*.a"
  "${CPKT_STRIP_ROOT}/lib/*.dylib"
  "${CPKT_STRIP_ROOT}/lib/*.so"
  "${CPKT_STRIP_ROOT}/lib/*.so.*"
)

function(cpkt_fix_darwin_dylib install_dylib)
  if(NOT DEFINED CPKT_DARWIN_DEPENDENCY_ROOT OR CPKT_DARWIN_DEPENDENCY_ROOT STREQUAL "")
    return()
  endif()
  if(NOT DEFINED CPKT_INSTALL_NAME_TOOL OR NOT EXISTS "${CPKT_INSTALL_NAME_TOOL}")
    message(FATAL_ERROR "CPKT_INSTALL_NAME_TOOL is required for Darwin dependency fixups")
  endif()
  if(NOT DEFINED CPKT_OTOOL OR NOT EXISTS "${CPKT_OTOOL}")
    message(FATAL_ERROR "CPKT_OTOOL is required for Darwin dependency fixups")
  endif()

  get_filename_component(_dylib_name "${install_dylib}" NAME)
  set(_dylib_install_name "${_dylib_name}")
  if(_dylib_name MATCHES "^libmqttc\\.([0-9]+)(\\.[0-9]+)*\\.dylib$")
    set(_dylib_install_name "libmqttc.${CMAKE_MATCH_1}.dylib")
  endif()
  execute_process(
    COMMAND "${CPKT_INSTALL_NAME_TOOL}" -id "@rpath/${_dylib_install_name}" "${install_dylib}"
    RESULT_VARIABLE _id_result
    ERROR_VARIABLE _id_error
  )
  if(NOT _id_result EQUAL 0)
    message(FATAL_ERROR "failed to rewrite Darwin install name for ${install_dylib}\n${_id_error}")
  endif()

  execute_process(
    COMMAND "${CPKT_OTOOL}" -L "${install_dylib}"
    RESULT_VARIABLE _otool_result
    OUTPUT_VARIABLE _otool_output
    ERROR_VARIABLE _otool_error
  )
  if(NOT _otool_result EQUAL 0)
    message(FATAL_ERROR "failed to inspect Darwin dependency ${install_dylib}\n${_otool_error}")
  endif()

  string(REGEX REPLACE "([][+.*()^$?{}|\\])" "\\\\\\1" _dependency_root_re "${CPKT_DARWIN_DEPENDENCY_ROOT}")
  string(REPLACE "\n" ";" _otool_lines "${_otool_output}")
  foreach(_otool_line IN LISTS _otool_lines)
    string(STRIP "${_otool_line}" _dependency_line)
    if(NOT _dependency_line MATCHES "^/")
      continue()
    endif()
    string(REGEX MATCH "^[^ \t]+" _dependency_path "${_dependency_line}")
    if(NOT _dependency_path MATCHES "^${_dependency_root_re}/")
      continue()
    endif()
    get_filename_component(_dependency_name "${_dependency_path}" NAME)
    execute_process(
      COMMAND "${CPKT_INSTALL_NAME_TOOL}"
        -change "${_dependency_path}" "@rpath/${_dependency_name}" "${install_dylib}"
      RESULT_VARIABLE _change_result
      ERROR_VARIABLE _change_error
    )
    if(NOT _change_result EQUAL 0)
      message(FATAL_ERROR
        "failed to rewrite Darwin dependency ${_dependency_path} in ${install_dylib}\n${_change_error}")
    endif()
  endforeach()
endfunction()

foreach(_candidate IN LISTS _cpkt_strip_candidates)
  if(IS_SYMLINK "${_candidate}" OR IS_DIRECTORY "${_candidate}")
    continue()
  endif()
  if(NOT CPKT_STRIP_STATIC_ARCHIVES AND _candidate MATCHES "\\.a$")
    continue()
  endif()
  if(_candidate MATCHES "\\.dylib$")
    cpkt_fix_darwin_dylib("${_candidate}")
  endif()

  execute_process(
    COMMAND "${CPKT_STRIP_BIN}" -S "${_candidate}"
    RESULT_VARIABLE _strip_result
    ERROR_VARIABLE _strip_error
  )
  if(NOT _strip_result EQUAL 0)
    message(FATAL_ERROR "failed to strip dependency artifact ${_candidate}\n${_strip_error}")
  endif()
endforeach()
