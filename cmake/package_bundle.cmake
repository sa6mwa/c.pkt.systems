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
set(_cpkt_static_library_suffix ".a")
if(CPKT_TARGET_ID STREQUAL "arm64-apple-darwin")
  set(_cpkt_shared_library_suffix ".dylib")
  set(_cpkt_libssh2_shared_library_name "libssh2.1.dylib")
else()
  set(_cpkt_shared_library_suffix ".so")
  set(_cpkt_libssh2_shared_library_name "libssh2.so")
endif()
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

function(cpkt_write_config_version package_dir config_stem package_version)
  file(MAKE_DIRECTORY "${_stage_root}/lib/cmake/${package_dir}")
  file(WRITE "${_stage_root}/lib/cmake/${package_dir}/${config_stem}ConfigVersion.cmake"
    "set(PACKAGE_VERSION \"${package_version}\")\n"
    "if(PACKAGE_FIND_VERSION VERSION_EQUAL PACKAGE_VERSION)\n"
    "  set(PACKAGE_VERSION_EXACT TRUE)\n"
    "endif()\n"
    "if(PACKAGE_FIND_VERSION VERSION_LESS_EQUAL PACKAGE_VERSION)\n"
    "  set(PACKAGE_VERSION_COMPATIBLE TRUE)\n"
    "endif()\n"
  )
endfunction()

set(_cpkt_armhf_static_extra_libs "")
set(_cpkt_armhf_static_extra_pc_libs "")
if(CPKT_TARGET_ID MATCHES "^armhf-")
  set(_cpkt_armhf_static_extra_libs "atomic")
  set(_cpkt_armhf_static_extra_pc_libs "-latomic")
endif()

set(_cpkt_openssl_static_private_pc_libs "")
if(CPKT_TARGET_ID MATCHES "-linux-")
  set(_cpkt_openssl_static_private_pc_libs "-ldl -pthread ${_cpkt_armhf_static_extra_pc_libs}")
endif()

file(MAKE_DIRECTORY "${_stage_root}/lib/cmake/OpenSSL")
file(WRITE "${_stage_root}/lib/cmake/OpenSSL/OpenSSLConfig.cmake"
  "include(CMakeFindDependencyMacro)\n"
  "find_dependency(Threads REQUIRED)\n"
  "get_filename_component(_cpkt_openssl_prefix \"\${CMAKE_CURRENT_LIST_DIR}/../../..\" ABSOLUTE)\n"
  "set(OPENSSL_FOUND TRUE)\n"
  "set(OPENSSL_VERSION \"${CPKT_OPENSSL_VERSION}\")\n"
  "set(OPENSSL_INCLUDE_DIR \"\${_cpkt_openssl_prefix}/include\")\n"
  "set(OPENSSL_CRYPTO_LIBRARY \"\${_cpkt_openssl_prefix}/lib/libcrypto${_cpkt_static_library_suffix}\")\n"
  "set(OPENSSL_SSL_LIBRARY \"\${_cpkt_openssl_prefix}/lib/libssl${_cpkt_static_library_suffix}\")\n"
  "set(_cpkt_openssl_static_extra_libs \"${_cpkt_armhf_static_extra_libs}\")\n"
  "if(NOT TARGET OpenSSL::Crypto)\n"
  "  add_library(OpenSSL::Crypto STATIC IMPORTED)\n"
  "  set_target_properties(OpenSSL::Crypto PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${OPENSSL_CRYPTO_LIBRARY}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${OPENSSL_INCLUDE_DIR}\"\n"
  "    INTERFACE_LINK_LIBRARIES \"\${CMAKE_DL_LIBS};Threads::Threads;\${_cpkt_openssl_static_extra_libs}\"\n"
  "  )\n"
  "endif()\n"
  "if(NOT TARGET OpenSSL::SSL)\n"
  "  add_library(OpenSSL::SSL STATIC IMPORTED)\n"
  "  set_target_properties(OpenSSL::SSL PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${OPENSSL_SSL_LIBRARY}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${OPENSSL_INCLUDE_DIR}\"\n"
  "    INTERFACE_LINK_LIBRARIES OpenSSL::Crypto\n"
  "  )\n"
  "endif()\n"
  "if(NOT TARGET cpkt::openssl_crypto_shared)\n"
  "  add_library(cpkt::openssl_crypto_shared SHARED IMPORTED)\n"
  "  set_target_properties(cpkt::openssl_crypto_shared PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_openssl_prefix}/lib/libcrypto${_cpkt_shared_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${OPENSSL_INCLUDE_DIR}\"\n"
  "  )\n"
  "endif()\n"
  "if(NOT TARGET cpkt::openssl_ssl_shared)\n"
  "  add_library(cpkt::openssl_ssl_shared SHARED IMPORTED)\n"
  "  set_target_properties(cpkt::openssl_ssl_shared PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_openssl_prefix}/lib/libssl${_cpkt_shared_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${OPENSSL_INCLUDE_DIR}\"\n"
  "    INTERFACE_LINK_LIBRARIES cpkt::openssl_crypto_shared\n"
  "  )\n"
  "endif()\n"
)
cpkt_write_config_version("OpenSSL" "OpenSSL" "${CPKT_OPENSSL_VERSION}")

file(MAKE_DIRECTORY "${_stage_root}/lib/cmake/zlib")
file(WRITE "${_stage_root}/lib/cmake/zlib/ZLIBConfig.cmake"
  "get_filename_component(_cpkt_zlib_prefix \"\${CMAKE_CURRENT_LIST_DIR}/../../..\" ABSOLUTE)\n"
  "set(ZLIB_FOUND TRUE)\n"
  "set(ZLIB_VERSION \"${CPKT_ZLIB_VERSION}\")\n"
  "set(ZLIB_INCLUDE_DIR \"\${_cpkt_zlib_prefix}/include\")\n"
  "set(ZLIB_LIBRARY \"\${_cpkt_zlib_prefix}/lib/libz${_cpkt_static_library_suffix}\")\n"
  "if(NOT TARGET ZLIB::ZLIB)\n"
  "  add_library(ZLIB::ZLIB STATIC IMPORTED)\n"
  "  set_target_properties(ZLIB::ZLIB PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${ZLIB_LIBRARY}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${ZLIB_INCLUDE_DIR}\"\n"
  "  )\n"
  "endif()\n"
  "if(NOT TARGET cpkt::zlib_shared)\n"
  "  add_library(cpkt::zlib_shared SHARED IMPORTED)\n"
  "  set_target_properties(cpkt::zlib_shared PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_zlib_prefix}/lib/libz${_cpkt_shared_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${ZLIB_INCLUDE_DIR}\"\n"
  "  )\n"
  "endif()\n"
)
cpkt_write_config_version("zlib" "ZLIB" "${CPKT_ZLIB_VERSION}")

file(MAKE_DIRECTORY "${_stage_root}/lib/cmake/nghttp2")
file(WRITE "${_stage_root}/lib/cmake/nghttp2/nghttp2Config.cmake"
  "get_filename_component(_cpkt_nghttp2_prefix \"\${CMAKE_CURRENT_LIST_DIR}/../../..\" ABSOLUTE)\n"
  "set(nghttp2_FOUND TRUE)\n"
  "set(nghttp2_VERSION \"${CPKT_NGHTTP2_VERSION}\")\n"
  "if(NOT TARGET nghttp2::nghttp2)\n"
  "  add_library(nghttp2::nghttp2 STATIC IMPORTED)\n"
  "  set_target_properties(nghttp2::nghttp2 PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_nghttp2_prefix}/lib/libnghttp2${_cpkt_static_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${_cpkt_nghttp2_prefix}/include\"\n"
  "  )\n"
  "endif()\n"
  "if(NOT TARGET cpkt::nghttp2_shared)\n"
  "  add_library(cpkt::nghttp2_shared SHARED IMPORTED)\n"
  "  set_target_properties(cpkt::nghttp2_shared PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_nghttp2_prefix}/lib/libnghttp2${_cpkt_shared_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${_cpkt_nghttp2_prefix}/include\"\n"
  "  )\n"
  "endif()\n"
)
cpkt_write_config_version("nghttp2" "nghttp2" "${CPKT_NGHTTP2_VERSION}")

file(MAKE_DIRECTORY "${_stage_root}/lib/cmake/libssh2")
file(WRITE "${_stage_root}/lib/cmake/libssh2/libssh2-config.cmake"
  "include(CMakeFindDependencyMacro)\n"
  "get_filename_component(_cpkt_libssh2_prefix \"\${CMAKE_CURRENT_LIST_DIR}/../../..\" ABSOLUTE)\n"
  "set(OpenSSL_DIR \"\${_cpkt_libssh2_prefix}/lib/cmake/OpenSSL\")\n"
  "set(ZLIB_DIR \"\${_cpkt_libssh2_prefix}/lib/cmake/zlib\")\n"
  "find_dependency(OpenSSL CONFIG REQUIRED)\n"
  "find_dependency(ZLIB CONFIG REQUIRED)\n"
  "set(libssh2_FOUND TRUE)\n"
  "set(Libssh2_FOUND TRUE)\n"
  "set(libssh2_VERSION \"${CPKT_LIBSSH2_VERSION}\")\n"
  "set(Libssh2_VERSION \"${CPKT_LIBSSH2_VERSION}\")\n"
  "if(NOT TARGET libssh2::libssh2_static)\n"
  "  add_library(libssh2::libssh2_static STATIC IMPORTED)\n"
  "  set_target_properties(libssh2::libssh2_static PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_libssh2_prefix}/lib/libssh2${_cpkt_static_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${_cpkt_libssh2_prefix}/include\"\n"
  "    INTERFACE_LINK_LIBRARIES \"OpenSSL::Crypto;ZLIB::ZLIB\"\n"
  "  )\n"
  "endif()\n"
  "if(NOT TARGET Libssh2::libssh2)\n"
  "  add_library(Libssh2::libssh2 ALIAS libssh2::libssh2_static)\n"
  "endif()\n"
  "if(NOT TARGET cpkt::libssh2_shared)\n"
  "  add_library(cpkt::libssh2_shared SHARED IMPORTED)\n"
  "  set_target_properties(cpkt::libssh2_shared PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_libssh2_prefix}/lib/${_cpkt_libssh2_shared_library_name}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${_cpkt_libssh2_prefix}/include\"\n"
  "    INTERFACE_LINK_LIBRARIES \"cpkt::zlib_shared\"\n"
  "  )\n"
  "endif()\n"
)
file(WRITE "${_stage_root}/lib/cmake/libssh2/libssh2-config-version.cmake"
  "set(PACKAGE_VERSION \"${CPKT_LIBSSH2_VERSION}\")\n"
  "if(PACKAGE_FIND_VERSION VERSION_EQUAL PACKAGE_VERSION)\n"
  "  set(PACKAGE_VERSION_EXACT TRUE)\n"
  "endif()\n"
  "if(PACKAGE_FIND_VERSION VERSION_LESS_EQUAL PACKAGE_VERSION)\n"
  "  set(PACKAGE_VERSION_COMPATIBLE TRUE)\n"
  "endif()\n"
)

set(_cpkt_curl_static_platform_libs "")
set(_cpkt_curl_static_platform_pc_libs "")
if(CPKT_TARGET_ID STREQUAL "arm64-apple-darwin")
  set(_cpkt_curl_static_platform_libs "-framework CoreFoundation;-framework SystemConfiguration")
  set(_cpkt_curl_static_platform_pc_libs "-framework CoreFoundation -framework SystemConfiguration")
endif()
file(MAKE_DIRECTORY "${_stage_root}/lib/cmake/CURL")
file(WRITE "${_stage_root}/lib/cmake/CURL/CURLConfig.cmake"
  "include(CMakeFindDependencyMacro)\n"
  "get_filename_component(_cpkt_curl_prefix \"\${CMAKE_CURRENT_LIST_DIR}/../../..\" ABSOLUTE)\n"
  "set(OpenSSL_DIR \"\${_cpkt_curl_prefix}/lib/cmake/OpenSSL\")\n"
  "set(ZLIB_DIR \"\${_cpkt_curl_prefix}/lib/cmake/zlib\")\n"
  "set(nghttp2_DIR \"\${_cpkt_curl_prefix}/lib/cmake/nghttp2\")\n"
  "set(Libssh2_DIR \"\${_cpkt_curl_prefix}/lib/cmake/libssh2\")\n"
  "find_dependency(OpenSSL CONFIG REQUIRED)\n"
  "find_dependency(ZLIB CONFIG REQUIRED)\n"
  "find_dependency(nghttp2 CONFIG REQUIRED)\n"
  "find_dependency(Libssh2 CONFIG REQUIRED)\n"
  "set(CURL_FOUND TRUE)\n"
  "set(CURL_VERSION \"${CPKT_CURL_VERSION}\")\n"
  "set(CURL_INCLUDE_DIRS \"\${_cpkt_curl_prefix}/include\")\n"
  "set(CURL_LIBRARIES CURL::libcurl)\n"
  "set(_cpkt_curl_static_platform_libs \"${_cpkt_curl_static_platform_libs}\")\n"
  "if(NOT TARGET CURL::libcurl)\n"
  "  add_library(CURL::libcurl STATIC IMPORTED)\n"
  "  set_target_properties(CURL::libcurl PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_curl_prefix}/lib/libcurl${_cpkt_static_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${_cpkt_curl_prefix}/include\"\n"
  "    INTERFACE_LINK_LIBRARIES \"Libssh2::libssh2;nghttp2::nghttp2;OpenSSL::SSL;OpenSSL::Crypto;ZLIB::ZLIB;Threads::Threads;\${_cpkt_curl_static_platform_libs}\"\n"
  "  )\n"
  "endif()\n"
  "if(NOT TARGET cpkt::curl_shared)\n"
  "  add_library(cpkt::curl_shared SHARED IMPORTED)\n"
  "  set_target_properties(cpkt::curl_shared PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_curl_prefix}/lib/libcurl${_cpkt_shared_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${_cpkt_curl_prefix}/include\"\n"
  "    INTERFACE_LINK_LIBRARIES \"cpkt::openssl_ssl_shared;cpkt::openssl_crypto_shared;cpkt::nghttp2_shared;cpkt::libssh2_shared;cpkt::zlib_shared\"\n"
  "  )\n"
  "endif()\n"
)
cpkt_write_config_version("CURL" "CURL" "${CPKT_CURL_VERSION}")

file(MAKE_DIRECTORY "${_stage_root}/lib/pkgconfig")
file(WRITE "${_stage_root}/lib/pkgconfig/libcrypto.pc"
  "prefix=\${pcfiledir}/../..\n"
  "exec_prefix=\${prefix}\n"
  "libdir=\${prefix}/lib\n"
  "includedir=\${prefix}/include\n"
  "\n"
  "Name: OpenSSL-libcrypto\n"
  "Description: OpenSSL cryptography library from c.pkt.systems\n"
  "Version: ${CPKT_OPENSSL_VERSION}\n"
  "Libs: -L\${libdir} -lcrypto\n"
  "Libs.private: ${_cpkt_openssl_static_private_pc_libs}\n"
  "Cflags: -I\${includedir}\n"
)
file(WRITE "${_stage_root}/lib/pkgconfig/libssl.pc"
  "prefix=\${pcfiledir}/../..\n"
  "exec_prefix=\${prefix}\n"
  "libdir=\${prefix}/lib\n"
  "includedir=\${prefix}/include\n"
  "\n"
  "Name: OpenSSL-libssl\n"
  "Description: OpenSSL SSL/TLS library from c.pkt.systems\n"
  "Version: ${CPKT_OPENSSL_VERSION}\n"
  "Requires.private: libcrypto\n"
  "Libs: -L\${libdir} -lssl\n"
  "Cflags: -I\${includedir}\n"
)
file(WRITE "${_stage_root}/lib/pkgconfig/openssl.pc"
  "prefix=\${pcfiledir}/../..\n"
  "exec_prefix=\${prefix}\n"
  "libdir=\${prefix}/lib\n"
  "includedir=\${prefix}/include\n"
  "\n"
  "Name: OpenSSL\n"
  "Description: OpenSSL libraries from c.pkt.systems\n"
  "Version: ${CPKT_OPENSSL_VERSION}\n"
  "Requires.private: libssl libcrypto\n"
  "Libs: -L\${libdir} -lssl -lcrypto\n"
  "Cflags: -I\${includedir}\n"
)
file(WRITE "${_stage_root}/lib/pkgconfig/zlib.pc"
  "prefix=\${pcfiledir}/../..\n"
  "exec_prefix=\${prefix}\n"
  "libdir=\${prefix}/lib\n"
  "includedir=\${prefix}/include\n"
  "\n"
  "Name: zlib\n"
  "Description: zlib compression library from c.pkt.systems\n"
  "Version: ${CPKT_ZLIB_VERSION}\n"
  "Libs: -L\${libdir} -lz\n"
  "Cflags: -I\${includedir}\n"
)
file(WRITE "${_stage_root}/lib/pkgconfig/libnghttp2.pc"
  "prefix=\${pcfiledir}/../..\n"
  "exec_prefix=\${prefix}\n"
  "libdir=\${prefix}/lib\n"
  "includedir=\${prefix}/include\n"
  "\n"
  "Name: libnghttp2\n"
  "Description: HTTP/2 C library from c.pkt.systems\n"
  "Version: ${CPKT_NGHTTP2_VERSION}\n"
  "Libs: -L\${libdir} -lnghttp2\n"
  "Cflags: -I\${includedir}\n"
)
file(WRITE "${_stage_root}/lib/pkgconfig/libssh2.pc"
  "prefix=\${pcfiledir}/../..\n"
  "exec_prefix=\${prefix}\n"
  "libdir=\${prefix}/lib\n"
  "includedir=\${prefix}/include\n"
  "\n"
  "Name: libssh2\n"
  "Description: SSH2 client-side library from c.pkt.systems\n"
  "Version: ${CPKT_LIBSSH2_VERSION}\n"
  "Requires.private: libcrypto zlib\n"
  "Libs: -L\${libdir} -lssh2\n"
  "Cflags: -I\${includedir}\n"
)
file(WRITE "${_stage_root}/lib/pkgconfig/libcurl.pc"
  "prefix=\${pcfiledir}/../..\n"
  "exec_prefix=\${prefix}\n"
  "libdir=\${prefix}/lib\n"
  "includedir=\${prefix}/include\n"
  "\n"
  "Name: libcurl\n"
  "Description: URL transfer library from c.pkt.systems\n"
  "Version: ${CPKT_CURL_VERSION}\n"
  "Requires.private: libssh2 libnghttp2 libssl libcrypto zlib\n"
  "Libs: -L\${libdir} -lcurl\n"
  "Libs.private: -pthread ${_cpkt_curl_static_platform_pc_libs}\n"
  "Cflags: -I\${includedir}\n"
)

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
