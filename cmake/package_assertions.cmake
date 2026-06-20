function(cpkt_find_darwin_otool out_var)
  if(DEFINED CPKT_OTOOL AND NOT "${CPKT_OTOOL}" STREQUAL "")
    if(EXISTS "${CPKT_OTOOL}")
      set(${out_var} "${CPKT_OTOOL}" PARENT_SCOPE)
      return()
    endif()
    message(FATAL_ERROR "configured CPKT_OTOOL does not exist: ${CPKT_OTOOL}")
  endif()

  set(_osxcross_host "${CPKT_OSXCROSS_HOST}")
  if(_osxcross_host STREQUAL "" AND DEFINED ENV{CPKT_OSXCROSS_HOST})
    set(_osxcross_host "$ENV{CPKT_OSXCROSS_HOST}")
  endif()
  if(_osxcross_host STREQUAL "")
    set(_osxcross_host "arm64-apple-darwin25")
  endif()

  set(_osxcross_root "${CPKT_OSXCROSS_ROOT}")
  if(_osxcross_root STREQUAL "" AND DEFINED ENV{OSXCROSS_ROOT})
    set(_osxcross_root "$ENV{OSXCROSS_ROOT}")
  endif()
  if(_osxcross_root STREQUAL "" AND DEFINED ENV{HOME})
    set(_osxcross_root "$ENV{HOME}/.local/cross/osxcross")
  endif()

  set(_otool_hints "")
  if(NOT _osxcross_root STREQUAL "")
    list(APPEND _otool_hints "${_osxcross_root}/bin")
  endif()
  if(DEFINED ENV{HOME})
    list(APPEND _otool_hints "$ENV{HOME}/.local/cross/osxcross/bin")
  endif()

  find_program(_cpkt_otool_bin
    NAMES "${_osxcross_host}-otool" arm64-apple-darwin25-otool otool
    HINTS ${_otool_hints})
  if(NOT _cpkt_otool_bin)
    message(FATAL_ERROR
      "otool is required to verify Darwin package artifacts; tried ${_osxcross_host}-otool, arm64-apple-darwin25-otool, and otool")
  endif()

  set(${out_var} "${_cpkt_otool_bin}" PARENT_SCOPE)
endfunction()

if(DEFINED CPKT_PACKAGE_ASSERTIONS_TEST_DARWIN_OTOOL_LOOKUP AND CPKT_PACKAGE_ASSERTIONS_TEST_DARWIN_OTOOL_LOOKUP)
  cpkt_find_darwin_otool(_cpkt_test_otool)
  message(STATUS "CPKT_TEST_OTOOL=${_cpkt_test_otool}")
  return()
endif()

foreach(_required CPKT_ARCHIVE CPKT_TARGET_ID CPKT_BUNDLE_VERSION)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

if(NOT EXISTS "${CPKT_ARCHIVE}")
  message(FATAL_ERROR "missing package archive: ${CPKT_ARCHIVE}")
endif()
get_filename_component(CPKT_ARCHIVE "${CPKT_ARCHIVE}" ABSOLUTE)
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
foreach(_internal_root cmocka curl libssh2 libxml2 lua nghttp2 openssl zlib)
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

function(cpkt_assert_archive_exact_matches regex expected_count description)
  string(REPLACE "\n" ";" _listing_lines "${_listing}")
  set(_actual_count 0)
  foreach(_listing_line IN LISTS _listing_lines)
    if(_listing_line MATCHES "${regex}")
      math(EXPR _actual_count "${_actual_count} + 1")
    endif()
  endforeach()
  if(NOT _actual_count EQUAL expected_count)
    message(FATAL_ERROR
      "archive must contain exactly ${expected_count} ${description}, found ${_actual_count}: ${regex}")
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

cpkt_extract_archive_for_assertions(_manifest_extract_root)
set(_manifest_path "${_manifest_extract_root}/${_archive_stem}/share/c.pkt.systems/manifest.txt")
if(NOT EXISTS "${_manifest_path}")
  message(FATAL_ERROR "missing package manifest: ${_manifest_path}")
endif()
file(READ "${_manifest_path}" _manifest_text)
if(NOT _manifest_text MATCHES "(^|\n)lua_runtime_abi_version=([A-Za-z0-9_.+-]+)(\n|$)")
  message(FATAL_ERROR "package manifest is missing lua_runtime_abi_version")
endif()
set(_manifest_lua_runtime_abi_version "${CMAKE_MATCH_2}")
if(DEFINED CPKT_LUA_RUNTIME_ABI_VERSION AND NOT "${CPKT_LUA_RUNTIME_ABI_VERSION}" STREQUAL "")
  if(NOT "${CPKT_LUA_RUNTIME_ABI_VERSION}" STREQUAL "${_manifest_lua_runtime_abi_version}")
    message(FATAL_ERROR
      "configured Lua runtime ABI ${CPKT_LUA_RUNTIME_ABI_VERSION} does not match package manifest ABI ${_manifest_lua_runtime_abi_version}")
  endif()
else()
  set(CPKT_LUA_RUNTIME_ABI_VERSION "${_manifest_lua_runtime_abi_version}")
endif()
file(REMOVE_RECURSE "${_manifest_extract_root}")

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

function(cpkt_assert_elf_soname file_path expected_soname description)
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
  if(NOT _readelf_output MATCHES "\\(SONAME\\)[^\n]*\\[${expected_soname}\\]")
    message(FATAL_ERROR "${description} must have SONAME [${expected_soname}]")
  endif()
endfunction()

function(cpkt_assert_darwin_install_name file_path expected_install_name description)
  cpkt_find_darwin_otool(CPKT_OTOOL_BIN)
  if(NOT CPKT_OTOOL_BIN)
    message(FATAL_ERROR "otool is required to verify ${description}")
  endif()
  if(NOT EXISTS "${file_path}")
    message(FATAL_ERROR "missing ${description}: ${file_path}")
  endif()

  execute_process(
    COMMAND "${CPKT_OTOOL_BIN}" -D "${file_path}"
    RESULT_VARIABLE _otool_result
    OUTPUT_VARIABLE _otool_output
    ERROR_VARIABLE _otool_error
  )
  if(NOT _otool_result EQUAL 0)
    message(FATAL_ERROR "failed to inspect ${description}: ${file_path}\n${_otool_error}")
  endif()
  if(NOT _otool_output MATCHES "(^|\n)${expected_install_name}(\n|$)")
    message(FATAL_ERROR "${description} must have Darwin install name [${expected_install_name}]")
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
    "include/libxml2/libxml/parser.h"
    "lib/libxml2.a"
    "lib/cmake/libxml2/libxml2-config.cmake"
    "lib/cmake/libxml2/libxml2-config-version.cmake"
    "lib/pkgconfig/libxml-2.0.pc"
    "include/lua.h"
    "include/lauxlib.h"
    "include/lualib.h"
    "include/cpkt/lua_runtime.h"
    "lib/liblua.a"
    "lib/libcpkt_lua_runtime.a"
    "lib/cmake/Lua/LuaConfig.cmake"
    "lib/cmake/Lua/LuaConfigVersion.cmake"
    "lib/cmake/CpktLuaRuntime/CpktLuaRuntimeConfig.cmake"
    "lib/cmake/CpktLuaRuntime/CpktLuaRuntimeConfigVersion.cmake"
    "lib/pkgconfig/lua.pc"
    "lib/pkgconfig/lua5.5.pc"
    "lib/pkgconfig/cpkt-lua-runtime.pc"
    "share/c.pkt.systems/manifest.txt"
    "share/doc/c.pkt.systems/third_party/openssl/LICENSE"
    "share/doc/c.pkt.systems/third_party/curl/LICENSE"
    "share/doc/c.pkt.systems/third_party/libssh2/LICENSE"
    "share/doc/c.pkt.systems/third_party/zlib/LICENSE"
    "share/doc/c.pkt.systems/third_party/nghttp2/LICENSE"
    "share/doc/c.pkt.systems/third_party/libxml2/LICENSE"
    "share/doc/c.pkt.systems/third_party/lua/LICENSE")
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

cpkt_extract_archive_for_assertions(_facade_header_extract_root)
set(_facade_header "${_facade_header_extract_root}/${_archive_stem}/include/cpkt/lua_runtime.h")
if(NOT EXISTS "${_facade_header}")
  message(FATAL_ERROR "missing Lua C89 facade header: ${_facade_header}")
endif()
file(READ "${_facade_header}" _facade_header_text)
foreach(_forbidden_header_token
    "lua.h"
    "lauxlib.h"
    "lualib.h"
    "lua_State"
    "lua_Integer"
    "lua_Number"
    "lua_Unsigned"
    "long long"
    "inline")
  if(_facade_header_text MATCHES "${_forbidden_header_token}")
    message(FATAL_ERROR "Lua C89 facade header contains forbidden token: ${_forbidden_header_token}")
  endif()
endforeach()
file(REMOVE_RECURSE "${_facade_header_extract_root}")

if(CPKT_TARGET_ID STREQUAL "arm64-apple-darwin")
  cpkt_assert_archive_exact_matches(
    "^${_archive_stem_re}/lib/libcpkt_lua_runtime([^/]*)?\\.dylib$"
    3
    "Lua runtime facade Darwin shared library entries")
  foreach(_path
      "lib/libssl.dylib"
      "lib/libcrypto.dylib"
      "lib/libcurl.dylib"
      "lib/libssh2.1.dylib"
      "lib/libz.dylib"
      "lib/libz.1.dylib"
      "lib/libnghttp2.dylib"
      "lib/libxml2.dylib"
      "lib/libxml2.16.dylib"
      "lib/liblua.dylib"
      "lib/liblua.5.5.dylib"
      "lib/libcpkt_lua_runtime.dylib"
      "lib/libcpkt_lua_runtime.${CPKT_LUA_RUNTIME_ABI_VERSION}.dylib"
      "lib/libcpkt_lua_runtime.${CPKT_BUNDLE_VERSION}.dylib")
    cpkt_assert_archive_contains("(^|\n)${_archive_stem_re}/${_path}(\n|$)" "${_path}")
  endforeach()
  cpkt_extract_archive_for_assertions(_assert_extract_root)
  cpkt_assert_darwin_install_name(
    "${_assert_extract_root}/${_archive_stem}/lib/libcpkt_lua_runtime.${CPKT_BUNDLE_VERSION}.dylib"
    "@rpath/libcpkt_lua_runtime.${CPKT_LUA_RUNTIME_ABI_VERSION}.dylib"
    "libcpkt_lua_runtime Darwin install name")
  file(REMOVE_RECURSE "${_assert_extract_root}")
else()
  cpkt_assert_archive_exact_matches(
    "^${_archive_stem_re}/lib/libcpkt_lua_runtime\\.so([^/]*)?$"
    3
    "Lua runtime facade Linux shared library entries")
  foreach(_path
      "lib/libssl.so"
      "lib/libcrypto.so"
      "lib/libcurl.so"
      "lib/libssh2.so"
      "lib/libssh2.so.1"
      "lib/libssh2.so.1.0.1"
      "lib/libz.so"
      "lib/libz.so.1"
      "lib/libnghttp2.so"
      "lib/libxml2.so"
      "lib/libxml2.so.16"
      "lib/libxml2.so.16.1.3"
      "lib/liblua.so"
      "lib/liblua.so.5.5"
      "lib/liblua.so.5.5.0"
      "lib/libcpkt_lua_runtime.so"
      "lib/libcpkt_lua_runtime.so.${CPKT_LUA_RUNTIME_ABI_VERSION}"
      "lib/libcpkt_lua_runtime.so.${CPKT_BUNDLE_VERSION}")
    cpkt_assert_archive_contains("(^|\n)${_archive_stem_re}/${_path}(\n|$)" "${_path}")
  endforeach()
  cpkt_extract_archive_for_assertions(_assert_extract_root)
  foreach(_runpath_library
      "lib/libcrypto.so.3"
      "lib/libssl.so.3"
      "lib/libssh2.so.1.0.1"
      "lib/libcurl.so.4.8.0"
      "lib/libxml2.so.16.1.3"
      "lib/libcpkt_lua_runtime.so")
    cpkt_assert_elf_runpath(
      "${_assert_extract_root}/${_archive_stem}/${_runpath_library}"
      "\\$ORIGIN"
      "${_runpath_library}")
  endforeach()
  cpkt_assert_elf_soname(
    "${_assert_extract_root}/${_archive_stem}/lib/libcpkt_lua_runtime.so.${CPKT_BUNDLE_VERSION}"
    "libcpkt_lua_runtime.so.${CPKT_LUA_RUNTIME_ABI_VERSION}"
    "libcpkt_lua_runtime SONAME")
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
