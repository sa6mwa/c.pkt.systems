if(NOT DEFINED CPKT_CHECKSUMS_PATH OR CPKT_CHECKSUMS_PATH STREQUAL "")
  message(FATAL_ERROR "CPKT_CHECKSUMS_PATH is required")
endif()
if(NOT DEFINED CPKT_ARTIFACT_PATH OR CPKT_ARTIFACT_PATH STREQUAL "")
  message(FATAL_ERROR "CPKT_ARTIFACT_PATH is required")
endif()
if(NOT EXISTS "${CPKT_ARTIFACT_PATH}")
  message(FATAL_ERROR "artifact to checksum does not exist: ${CPKT_ARTIFACT_PATH}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E sha256sum "${CPKT_ARTIFACT_PATH}"
  RESULT_VARIABLE _sha_result
  OUTPUT_VARIABLE _sha_output
  ERROR_VARIABLE _sha_error
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _sha_result EQUAL 0)
  message(FATAL_ERROR "failed to checksum ${CPKT_ARTIFACT_PATH}: ${_sha_error}")
endif()

string(REGEX MATCH "^([0-9a-fA-F]+)[ \t]+(.+)$" _sha_match "${_sha_output}")
if(NOT _sha_match)
  message(FATAL_ERROR "unexpected checksum output for ${CPKT_ARTIFACT_PATH}: ${_sha_output}")
endif()
set(_sha_hash "${CMAKE_MATCH_1}")
get_filename_component(_artifact_name "${CPKT_ARTIFACT_PATH}" NAME)

set(_existing_checksums "")
if(EXISTS "${CPKT_CHECKSUMS_PATH}")
  file(STRINGS "${CPKT_CHECKSUMS_PATH}" _checksum_lines)
  foreach(_checksum_line IN LISTS _checksum_lines)
    if(NOT _checksum_line MATCHES "[ \t]${_artifact_name}$")
      string(APPEND _existing_checksums "${_checksum_line}\n")
    endif()
  endforeach()
endif()

file(WRITE "${CPKT_CHECKSUMS_PATH}" "${_existing_checksums}${_sha_hash}  ${_artifact_name}\n")
