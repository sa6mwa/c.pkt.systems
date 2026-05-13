foreach(_required
    CPKT_SOURCE_DIR
    CPKT_EXTERNAL_ROOT
    CPKT_DEPENDENCY_BUILD_ROOT
    CPKT_DIST_DIR
    CPKT_TARGET_ID
    CPKT_BUNDLE_VERSION
    CPKT_OPENSSL_VERSION
    CPKT_ZLIB_VERSION
    CPKT_CURL_VERSION
    CPKT_NGHTTP2_VERSION
    CPKT_LIBSSH2_VERSION
    CPKT_CMOCKA_VERSION)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

if(NOT EXISTS "${CPKT_EXTERNAL_ROOT}")
  message(FATAL_ERROR "dependency install root does not exist: ${CPKT_EXTERNAL_ROOT}")
endif()

set(_archive_stem "c.pkt.systems-${CPKT_BUNDLE_VERSION}-${CPKT_TARGET_ID}")
set(_stage_parent "${CMAKE_CURRENT_BINARY_DIR}/package-stage")
set(_stage_root "${_stage_parent}/${_archive_stem}")
set(_archive_path "${CPKT_DIST_DIR}/${_archive_stem}.tar.gz")
set(_checksums_path "${CPKT_DIST_DIR}/c.pkt.systems-${CPKT_BUNDLE_VERSION}-CHECKSUMS")
file(REMOVE_RECURSE "${_stage_parent}")
file(MAKE_DIRECTORY "${_stage_root}/include" "${_stage_root}/lib" "${CPKT_DIST_DIR}")

function(cpkt_stage_dependency_install dependency_name)
  set(_install_root "${CPKT_EXTERNAL_ROOT}/${dependency_name}/install")
  if(NOT EXISTS "${_install_root}")
    message(FATAL_ERROR "dependency install root does not exist: ${_install_root}")
  endif()

  foreach(_subdir include lib)
    if(EXISTS "${_install_root}/${_subdir}")
      file(COPY "${_install_root}/${_subdir}/" DESTINATION "${_stage_root}/${_subdir}")
    endif()
  endforeach()
endfunction()

foreach(_dependency openssl zlib nghttp2 libssh2 curl)
  cpkt_stage_dependency_install("${_dependency}")
endforeach()

file(REMOVE_RECURSE
  "${_stage_root}/lib/engines-3"
  "${_stage_root}/lib/cmake"
  "${_stage_root}/lib/ossl-modules"
  "${_stage_root}/lib/pkgconfig"
  "${_stage_root}/share/man"
)
file(GLOB _libtool_archives "${_stage_root}/lib/*.la")
if(_libtool_archives)
  file(REMOVE ${_libtool_archives})
endif()

file(MAKE_DIRECTORY "${_stage_root}/share/c.pkt.systems")
file(WRITE "${_stage_root}/share/c.pkt.systems/manifest.txt"
  "bundle_version=${CPKT_BUNDLE_VERSION}\n"
  "target_id=${CPKT_TARGET_ID}\n"
  "openssl_version=${CPKT_OPENSSL_VERSION}\n"
  "zlib_version=${CPKT_ZLIB_VERSION}\n"
  "curl_version=${CPKT_CURL_VERSION}\n"
  "nghttp2_version=${CPKT_NGHTTP2_VERSION}\n"
  "libssh2_version=${CPKT_LIBSSH2_VERSION}\n"
)

function(cpkt_stage_license package_name source_path)
  if(NOT EXISTS "${source_path}")
    message(FATAL_ERROR "missing license file for ${package_name}: ${source_path}")
  endif()
  file(MAKE_DIRECTORY "${_stage_root}/share/doc/c.pkt.systems/third_party/${package_name}")
  file(COPY_FILE
    "${source_path}"
    "${_stage_root}/share/doc/c.pkt.systems/third_party/${package_name}/LICENSE"
  )
endfunction()

cpkt_stage_license("openssl" "${CPKT_DEPENDENCY_BUILD_ROOT}/openssl/src/LICENSE.txt")
cpkt_stage_license("curl" "${CPKT_DEPENDENCY_BUILD_ROOT}/curl/src/COPYING")
cpkt_stage_license("libssh2" "${CPKT_DEPENDENCY_BUILD_ROOT}/libssh2/src/COPYING")
cpkt_stage_license("zlib" "${CPKT_DEPENDENCY_BUILD_ROOT}/zlib/src/LICENSE")
cpkt_stage_license("nghttp2" "${CPKT_DEPENDENCY_BUILD_ROOT}/nghttp2/src/COPYING")

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E tar czf "${_archive_path}" "${_archive_stem}"
  WORKING_DIRECTORY "${_stage_parent}"
  RESULT_VARIABLE _tar_result
  ERROR_VARIABLE _tar_error
)
if(NOT _tar_result EQUAL 0)
  message(FATAL_ERROR "failed to create bundle archive: ${_archive_path}\n${_tar_error}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E sha256sum "${_archive_path}"
  RESULT_VARIABLE _sha_result
  OUTPUT_VARIABLE _sha_output
  ERROR_VARIABLE _sha_error
)
if(NOT _sha_result EQUAL 0)
  message(FATAL_ERROR "failed to checksum bundle archive: ${_archive_path}\n${_sha_error}")
endif()
string(STRIP "${_sha_output}" _sha_output)
string(REGEX MATCHALL "[^ ]+" _sha_fields "${_sha_output}")
list(LENGTH _sha_fields _sha_field_count)
if(_sha_field_count LESS 1)
  message(FATAL_ERROR "unexpected checksum output for ${_archive_path}: ${_sha_output}")
endif()
list(GET _sha_fields 0 _sha_hash)
get_filename_component(_archive_name "${_archive_path}" NAME)

set(_existing_checksums "")
if(EXISTS "${_checksums_path}")
  file(STRINGS "${_checksums_path}" _checksum_lines)
  foreach(_checksum_line IN LISTS _checksum_lines)
    if(NOT _checksum_line MATCHES "[ \t]${_archive_name}$")
      string(APPEND _existing_checksums "${_checksum_line}\n")
    endif()
  endforeach()
endif()
file(WRITE "${_checksums_path}" "${_existing_checksums}${_sha_hash}  ${_archive_name}\n")

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -DCPKT_ROOT=${CPKT_SOURCE_DIR}
    -DCPKT_SCAN_LABEL=bundle
    -DCPKT_SCAN_PATHS=${_archive_path}
    -P "${CPKT_SOURCE_DIR}/tests/privacy_scan.cmake"
  RESULT_VARIABLE _privacy_result
  ERROR_VARIABLE _privacy_error
)
if(NOT _privacy_result EQUAL 0)
  message(FATAL_ERROR "bundle privacy scan failed: ${_archive_path}\n${_privacy_error}")
endif()

message(STATUS "Wrote ${_archive_path}")
message(STATUS "Updated ${_checksums_path}")
