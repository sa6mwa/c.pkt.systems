foreach(_required
    CPKT_DARWIN_SMOKE_BINARY
    CPKT_DARWIN_SMOKE_LIB_DIR
    CPKT_EXTERNAL_ROOT
    CPKT_OTOOL)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

if(NOT EXISTS "${CPKT_DARWIN_SMOKE_BINARY}")
  message(FATAL_ERROR "Darwin smoke binary does not exist: ${CPKT_DARWIN_SMOKE_BINARY}")
endif()
if(NOT EXISTS "${CPKT_OTOOL}")
  message(FATAL_ERROR "Darwin otool does not exist: ${CPKT_OTOOL}")
endif()

execute_process(
  COMMAND "${CPKT_OTOOL}" -L "${CPKT_DARWIN_SMOKE_BINARY}"
  RESULT_VARIABLE _otool_result
  OUTPUT_VARIABLE _otool_output
  ERROR_VARIABLE _otool_error
)
if(NOT _otool_result EQUAL 0)
  message(FATAL_ERROR "failed to inspect Darwin smoke binary libraries:\n${_otool_error}")
endif()

file(MAKE_DIRECTORY "${CPKT_DARWIN_SMOKE_LIB_DIR}")
string(REPLACE "\n" ";" _otool_lines "${_otool_output}")
set(_copied_libraries 0)
foreach(_otool_line IN LISTS _otool_lines)
  string(STRIP "${_otool_line}" _otool_line)
  if(NOT _otool_line MATCHES "^@rpath/([^ ]+)")
    continue()
  endif()

  set(_library_name "${CMAKE_MATCH_1}")
  file(GLOB _matches
    "${CPKT_EXTERNAL_ROOT}/*/install/lib/${_library_name}"
  )
  list(LENGTH _matches _match_count)
  if(NOT _match_count EQUAL 1)
    message(FATAL_ERROR
      "expected exactly one dependency library named ${_library_name}, found ${_match_count}")
  endif()
  list(GET _matches 0 _library_path)
  file(COPY_FILE
    "${_library_path}"
    "${CPKT_DARWIN_SMOKE_LIB_DIR}/${_library_name}"
  )
  math(EXPR _copied_libraries "${_copied_libraries} + 1")
endforeach()

if(_copied_libraries EQUAL 0)
  message(FATAL_ERROR "Darwin smoke binary has no @rpath dependency libraries to package")
endif()
