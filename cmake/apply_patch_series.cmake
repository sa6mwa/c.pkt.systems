foreach(_required CPKT_PATCH_WORKING_DIRECTORY CPKT_PATCH_SERIES)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

if(NOT IS_DIRECTORY "${CPKT_PATCH_WORKING_DIRECTORY}")
  message(FATAL_ERROR "patch working directory does not exist: ${CPKT_PATCH_WORKING_DIRECTORY}")
endif()
if(NOT EXISTS "${CPKT_PATCH_SERIES}")
  message(FATAL_ERROR "patch series does not exist: ${CPKT_PATCH_SERIES}")
endif()

find_program(CPKT_PATCH_EXECUTABLE NAMES patch)
if(NOT CPKT_PATCH_EXECUTABLE)
  message(FATAL_ERROR "patch executable is required to apply ${CPKT_PATCH_SERIES}")
endif()

get_filename_component(_series_dir "${CPKT_PATCH_SERIES}" DIRECTORY)
file(READ "${CPKT_PATCH_SERIES}" _series_text)
string(REPLACE "\r\n" "\n" _series_text "${_series_text}")
string(REPLACE "\n" ";" _series_entries "${_series_text}")

foreach(_patch_name IN LISTS _series_entries)
  string(STRIP "${_patch_name}" _patch_name)
  if(_patch_name STREQUAL "" OR _patch_name MATCHES "^#")
    continue()
  endif()

  set(_patch_path "${_series_dir}/${_patch_name}")
  if(NOT EXISTS "${_patch_path}")
    message(FATAL_ERROR "patch listed in series does not exist: ${_patch_path}")
  endif()

  execute_process(
    COMMAND "${CPKT_PATCH_EXECUTABLE}" --dry-run --reverse -p1 -i "${_patch_path}"
    WORKING_DIRECTORY "${CPKT_PATCH_WORKING_DIRECTORY}"
    RESULT_VARIABLE _patch_reverse_result
    OUTPUT_QUIET
    ERROR_QUIET
  )
  if(_patch_reverse_result EQUAL 0)
    message(STATUS "Skipping already-applied patch ${_patch_path}")
    continue()
  endif()

  execute_process(
    COMMAND "${CPKT_PATCH_EXECUTABLE}" --dry-run -p1 -i "${_patch_path}"
    WORKING_DIRECTORY "${CPKT_PATCH_WORKING_DIRECTORY}"
    RESULT_VARIABLE _patch_dry_run_result
    OUTPUT_VARIABLE _patch_dry_run_output
    ERROR_VARIABLE _patch_dry_run_error
  )
  if(NOT _patch_dry_run_result EQUAL 0)
    message(FATAL_ERROR
      "failed to dry-run ${_patch_path} in ${CPKT_PATCH_WORKING_DIRECTORY}\n"
      "${_patch_dry_run_output}\n${_patch_dry_run_error}")
  endif()

  execute_process(
    COMMAND "${CPKT_PATCH_EXECUTABLE}" -p1 -i "${_patch_path}"
    WORKING_DIRECTORY "${CPKT_PATCH_WORKING_DIRECTORY}"
    RESULT_VARIABLE _patch_result
    OUTPUT_VARIABLE _patch_output
    ERROR_VARIABLE _patch_error
  )
  if(NOT _patch_result EQUAL 0)
    message(FATAL_ERROR
      "failed to apply ${_patch_path} in ${CPKT_PATCH_WORKING_DIRECTORY}\n"
      "${_patch_output}\n${_patch_error}")
  endif()
endforeach()
