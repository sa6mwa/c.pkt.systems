foreach(_required CPKT_ARCHIVE CPKT_TARGET_ID CPKT_BUNDLE_VERSION)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

if(NOT EXISTS "${CPKT_ARCHIVE}")
  message(FATAL_ERROR "missing package archive: ${CPKT_ARCHIVE}")
endif()
get_filename_component(_archive_dir "${CPKT_ARCHIVE}" DIRECTORY)
get_filename_component(_archive_name "${CPKT_ARCHIVE}" NAME)
set(_archive_stem "c.pkt.systems-${CPKT_BUNDLE_VERSION}-${CPKT_TARGET_ID}")
string(REGEX REPLACE "([][+.*()^$?{}|\\])" "\\\\\\1" _archive_stem_re "${_archive_stem}")
set(_checksums_path "${_archive_dir}/c.pkt.systems-${CPKT_BUNDLE_VERSION}-CHECKSUMS")
if(NOT EXISTS "${_checksums_path}")
  message(FATAL_ERROR "missing package checksums: ${_checksums_path}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E tar tf "${CPKT_ARCHIVE}"
  RESULT_VARIABLE _list_result
  OUTPUT_VARIABLE _listing
  ERROR_VARIABLE _list_error
)
if(NOT _list_result EQUAL 0)
  message(FATAL_ERROR "failed to list ${CPKT_ARCHIVE}\n${_list_error}")
endif()

if(_listing MATCHES "(^|\n)\\./")
  message(FATAL_ERROR "archive entries must be rooted at ${_archive_stem}, not ./")
endif()
foreach(_internal_root cmocka curl libssh2 nghttp2 openssl zlib)
  if(_listing MATCHES "(^|\n)${_internal_root}/")
    message(FATAL_ERROR "archive exposes internal dependency root: ${_internal_root}/")
  endif()
endforeach()
if(_listing MATCHES "(^|\n)${_archive_stem_re}/[^ \n]*/install/")
  message(FATAL_ERROR "archive exposes internal install/ directories")
endif()
if(_listing MATCHES "(^|\n)${_archive_stem_re}/([^ \n]*/)*cmocka")
  message(FATAL_ERROR "release archive must not contain cmocka")
endif()

function(cpkt_assert_archive_contains regex description)
  if(NOT _listing MATCHES "${regex}")
    message(FATAL_ERROR "archive is missing ${description}: ${regex}")
  endif()
endfunction()

function(cpkt_assert_archive_lacks regex description)
  if(_listing MATCHES "${regex}")
    message(FATAL_ERROR "archive contains forbidden ${description}: ${regex}")
  endif()
endfunction()

function(cpkt_extract_archive_for_assertions out_var)
  string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef _extract_suffix)
  set(_extract_root "${CMAKE_CURRENT_BINARY_DIR}/package-assertions-${_archive_stem}-${_extract_suffix}")
  file(REMOVE_RECURSE "${_extract_root}")
  file(MAKE_DIRECTORY "${_extract_root}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar xf "${CPKT_ARCHIVE}"
    WORKING_DIRECTORY "${_extract_root}"
    RESULT_VARIABLE _extract_result
    ERROR_VARIABLE _extract_error
  )
  if(NOT _extract_result EQUAL 0)
    message(FATAL_ERROR "failed to extract ${CPKT_ARCHIVE}\n${_extract_error}")
  endif()
  set(${out_var} "${_extract_root}" PARENT_SCOPE)
endfunction()

function(cpkt_assert_elf_runpath file_path expected_runpath description)
  find_program(CPKT_READELF_BIN NAMES readelf)
  if(NOT CPKT_READELF_BIN)
    message(FATAL_ERROR "readelf is required to verify ${description}")
  endif()
  if(NOT EXISTS "${file_path}")
    message(FATAL_ERROR "missing ${description}: ${file_path}")
  endif()

  execute_process(
    COMMAND "${CPKT_READELF_BIN}" -d "${file_path}"
    RESULT_VARIABLE _readelf_result
    OUTPUT_VARIABLE _readelf_output
    ERROR_VARIABLE _readelf_error
  )
  if(NOT _readelf_result EQUAL 0)
    message(FATAL_ERROR "failed to inspect ${description}: ${file_path}\n${_readelf_error}")
  endif()
  if(NOT _readelf_output MATCHES "\\((RUNPATH|RPATH)\\)[^\n]*\\[${expected_runpath}\\]")
    message(FATAL_ERROR "${description} must have RUNPATH/RPATH [${expected_runpath}]")
  endif()
endfunction()

foreach(_path
    "include/openssl/ssl.h"
    "lib/libssl.a"
    "lib/libcrypto.a"
    "lib/cmake/OpenSSL/OpenSSLConfig.cmake"
    "lib/cmake/OpenSSL/OpenSSLConfigVersion.cmake"
    "lib/pkgconfig/libssl.pc"
    "lib/pkgconfig/libcrypto.pc"
    "lib/pkgconfig/openssl.pc"
    "include/curl/curl.h"
    "lib/libcurl.a"
    "lib/cmake/CURL/CURLConfig.cmake"
    "lib/cmake/CURL/CURLConfigVersion.cmake"
    "lib/pkgconfig/libcurl.pc"
    "include/libssh2.h"
    "include/libssh2_sftp.h"
    "lib/libssh2.a"
    "lib/cmake/libssh2/libssh2-config.cmake"
    "lib/cmake/libssh2/libssh2-config-version.cmake"
    "lib/pkgconfig/libssh2.pc"
    "include/zlib.h"
    "lib/libz.a"
    "lib/cmake/zlib/ZLIBConfig.cmake"
    "lib/cmake/zlib/ZLIBConfigVersion.cmake"
    "lib/pkgconfig/zlib.pc"
    "include/nghttp2/nghttp2.h"
    "lib/libnghttp2.a"
    "lib/cmake/nghttp2/nghttp2Config.cmake"
    "lib/cmake/nghttp2/nghttp2ConfigVersion.cmake"
    "lib/pkgconfig/libnghttp2.pc"
    "share/c.pkt.systems/manifest.txt"
    "share/doc/c.pkt.systems/third_party/openssl/LICENSE"
    "share/doc/c.pkt.systems/third_party/curl/LICENSE"
    "share/doc/c.pkt.systems/third_party/libssh2/LICENSE"
    "share/doc/c.pkt.systems/third_party/zlib/LICENSE"
    "share/doc/c.pkt.systems/third_party/nghttp2/LICENSE")
  cpkt_assert_archive_contains("(^|\n)${_archive_stem_re}/${_path}(\n|$)" "${_path}")
endforeach()

foreach(_path
    "lib/cmake/Libssh2/"
    "lib/cmake/ZLIB/")
  cpkt_assert_archive_lacks("(^|\n)${_archive_stem_re}/${_path}" "${_path}")
endforeach()

cpkt_extract_archive_for_assertions(_metadata_extract_root)
file(GLOB_RECURSE _metadata_files
  "${_metadata_extract_root}/${_archive_stem}/lib/cmake/*"
  "${_metadata_extract_root}/${_archive_stem}/lib/pkgconfig/*")
foreach(_metadata_file IN LISTS _metadata_files)
  if(IS_DIRECTORY "${_metadata_file}")
    continue()
  endif()
  file(READ "${_metadata_file}" _metadata_text)
  if(_metadata_text MATCHES "(/home/|/tmp/|/var/tmp/|\\.cache|deps-build|package-stage|CMakeFiles|CPKT_EXTERNAL_ROOT|CPKT_DEPENDENCY_BUILD_ROOT)")
    message(FATAL_ERROR "package metadata contains local build/cache path material: ${_metadata_file}")
  endif()
endforeach()
file(REMOVE_RECURSE "${_metadata_extract_root}")

if(CPKT_TARGET_ID STREQUAL "arm64-apple-darwin")
  foreach(_path
      "lib/libssl.dylib"
      "lib/libcrypto.dylib"
      "lib/libcurl.dylib"
      "lib/libssh2.1.dylib"
      "lib/libz.dylib"
      "lib/libz.1.dylib"
      "lib/libnghttp2.dylib")
    cpkt_assert_archive_contains("(^|\n)${_archive_stem_re}/${_path}(\n|$)" "${_path}")
  endforeach()
else()
  foreach(_path
      "lib/libssl.so"
      "lib/libcrypto.so"
      "lib/libcurl.so"
      "lib/libssh2.so"
      "lib/libssh2.so.1"
      "lib/libssh2.so.1.0.1"
      "lib/libz.so"
      "lib/libz.so.1"
      "lib/libnghttp2.so")
    cpkt_assert_archive_contains("(^|\n)${_archive_stem_re}/${_path}(\n|$)" "${_path}")
  endforeach()
  cpkt_extract_archive_for_assertions(_assert_extract_root)
  foreach(_runpath_library
      "lib/libcrypto.so.3"
      "lib/libssl.so.3"
      "lib/libssh2.so.1.0.1"
      "lib/libcurl.so.4.8.0")
    cpkt_assert_elf_runpath(
      "${_assert_extract_root}/${_archive_stem}/${_runpath_library}"
      "\\$ORIGIN"
      "${_runpath_library}")
  endforeach()
  file(REMOVE_RECURSE "${_assert_extract_root}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E sha256sum "${CPKT_ARCHIVE}"
  RESULT_VARIABLE _sha_result
  OUTPUT_VARIABLE _sha_output
  ERROR_VARIABLE _sha_error
)
if(NOT _sha_result EQUAL 0)
  message(FATAL_ERROR "failed to checksum package archive: ${CPKT_ARCHIVE}\n${_sha_error}")
endif()
string(STRIP "${_sha_output}" _sha_output)
string(REGEX MATCHALL "[^ ]+" _checksum_fields "${_sha_output}")
list(LENGTH _checksum_fields _checksum_field_count)
if(_checksum_field_count LESS 1)
  message(FATAL_ERROR "unexpected checksum output: ${_sha_output}")
endif()
list(GET _checksum_fields 0 _checksum_hash)

file(READ "${_checksums_path}" _checksum_text)
if(NOT _checksum_text MATCHES "(^|\n)${_checksum_hash}[ \t]+${_archive_name}(\n|$)")
  message(FATAL_ERROR
    "checksums file does not contain SHA-256 for ${_archive_name}: ${_checksums_path}")
endif()
