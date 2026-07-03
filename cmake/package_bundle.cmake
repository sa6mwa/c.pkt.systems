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
    CPKT_CMOCKA_VERSION
    CPKT_LIBXML2_VERSION
    CPKT_LUA_VERSION
    CPKT_MQTTC_VERSION
    CPKT_MQTTC_COMMIT
    CPKT_OPEN62541_VERSION
    CPKT_OPEN62541_PATCHSET
    CPKT_LUA_RUNTIME_ABI_VERSION
    CPKT_LUA_RUNTIME_INCLUDE_DIR
    CPKT_LUA_RUNTIME_STATIC_LIBRARY
    CPKT_LUA_RUNTIME_SHARED_LIBRARY
    CPKT_MINIAUDIO_VERSION
    CPKT_AUDIO_ABI_VERSION
    CPKT_AUDIO_STATIC_LIBRARY
    CPKT_AUDIO_SHARED_LIBRARY
    CPKT_WHISPER_VERSION
    CPKT_SUS_ABI_VERSION
    CPKT_SUS_STATIC_LIBRARY
    CPKT_SUS_SHARED_LIBRARY
    CPKT_OPCUA_ABI_VERSION
    CPKT_OPCUA_STATIC_LIBRARY
    CPKT_OPCUA_SHARED_LIBRARY)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

if(NOT EXISTS "${CPKT_EXTERNAL_ROOT}")
  message(FATAL_ERROR "dependency install root does not exist: ${CPKT_EXTERNAL_ROOT}")
endif()

include("${CPKT_SOURCE_DIR}/cmake/gnu_tar.cmake")
cpkt_find_gnu_tar(_cpkt_gnu_tar)

set(_archive_stem "c.pkt.systems-${CPKT_BUNDLE_VERSION}-${CPKT_TARGET_ID}")
set(_stage_parent "${CMAKE_CURRENT_BINARY_DIR}/package-stage")
set(_stage_root "${_stage_parent}/${_archive_stem}")
set(_archive_path "${CPKT_DIST_DIR}/${_archive_stem}.tar.gz")
set(_checksums_path "${CPKT_DIST_DIR}/c.pkt.systems-${CPKT_BUNDLE_VERSION}-CHECKSUMS")
set(_cpkt_static_library_suffix ".a")
if(CPKT_TARGET_ID STREQUAL "arm64-apple-darwin")
  set(_cpkt_shared_library_suffix ".dylib")
  set(_cpkt_lua_runtime_shared_library_link_name "libcpkt_lua_runtime.dylib")
  set(_cpkt_lua_runtime_shared_library_abi_name "libcpkt_lua_runtime.${CPKT_LUA_RUNTIME_ABI_VERSION}.dylib")
  set(_cpkt_lua_runtime_shared_library_real_name "libcpkt_lua_runtime.${CPKT_BUNDLE_VERSION}.dylib")
  set(_cpkt_audio_shared_library_link_name "libcpkt_audio.dylib")
  set(_cpkt_audio_shared_library_abi_name "libcpkt_audio.${CPKT_AUDIO_ABI_VERSION}.dylib")
  set(_cpkt_audio_shared_library_real_name "libcpkt_audio.${CPKT_BUNDLE_VERSION}.dylib")
  set(_cpkt_sus_shared_library_link_name "libcpktsus.dylib")
  set(_cpkt_sus_shared_library_abi_name "libcpktsus.${CPKT_SUS_ABI_VERSION}.dylib")
  set(_cpkt_sus_shared_library_real_name "libcpktsus.${CPKT_BUNDLE_VERSION}.dylib")
  set(_cpkt_opcua_shared_library_link_name "libcpkt_opcua.dylib")
  set(_cpkt_opcua_shared_library_abi_name "libcpkt_opcua.${CPKT_OPCUA_ABI_VERSION}.dylib")
  set(_cpkt_opcua_shared_library_real_name "libcpkt_opcua.${CPKT_BUNDLE_VERSION}.dylib")
  set(_cpkt_libssh2_shared_library_name "libssh2.1.dylib")
  set(_cpkt_libxml2_shared_library_name "libxml2.dylib")
  set(_cpkt_lua_shared_library_name "liblua.dylib")
  set(_cpkt_mqttc_shared_library_name "libmqttc.dylib")
  set(_cpkt_open62541_shared_library_name "libopen62541.dylib")
else()
  set(_cpkt_shared_library_suffix ".so")
  set(_cpkt_lua_runtime_shared_library_link_name "libcpkt_lua_runtime.so")
  set(_cpkt_lua_runtime_shared_library_abi_name "libcpkt_lua_runtime.so.${CPKT_LUA_RUNTIME_ABI_VERSION}")
  set(_cpkt_lua_runtime_shared_library_real_name "libcpkt_lua_runtime.so.${CPKT_BUNDLE_VERSION}")
  set(_cpkt_audio_shared_library_link_name "libcpkt_audio.so")
  set(_cpkt_audio_shared_library_abi_name "libcpkt_audio.so.${CPKT_AUDIO_ABI_VERSION}")
  set(_cpkt_audio_shared_library_real_name "libcpkt_audio.so.${CPKT_BUNDLE_VERSION}")
  set(_cpkt_sus_shared_library_link_name "libcpktsus.so")
  set(_cpkt_sus_shared_library_abi_name "libcpktsus.so.${CPKT_SUS_ABI_VERSION}")
  set(_cpkt_sus_shared_library_real_name "libcpktsus.so.${CPKT_BUNDLE_VERSION}")
  set(_cpkt_opcua_shared_library_link_name "libcpkt_opcua.so")
  set(_cpkt_opcua_shared_library_abi_name "libcpkt_opcua.so.${CPKT_OPCUA_ABI_VERSION}")
  set(_cpkt_opcua_shared_library_real_name "libcpkt_opcua.so.${CPKT_BUNDLE_VERSION}")
  set(_cpkt_libssh2_shared_library_name "libssh2.so")
  set(_cpkt_libxml2_shared_library_name "libxml2.so")
  set(_cpkt_lua_shared_library_name "liblua.so")
  set(_cpkt_mqttc_shared_library_name "libmqttc.so")
  set(_cpkt_open62541_shared_library_name "libopen62541.so")
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

foreach(_dependency openssl zlib nghttp2 libssh2 curl libxml2 lua miniaudio whisper mqtt-c open62541)
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
file(GLOB _legacy_open62541_shared_libraries
  "${_stage_root}/lib/libopen62541.so.0.4"
  "${_stage_root}/lib/libopen62541.so.0.4.*")
if(_legacy_open62541_shared_libraries)
  file(REMOVE ${_legacy_open62541_shared_libraries})
endif()
file(COPY "${CPKT_LUA_RUNTIME_INCLUDE_DIR}/cpkt" DESTINATION "${_stage_root}/include")
function(cpkt_stage_facade_library facade_label static_source static_name shared_source shared_real_name shared_abi_name shared_link_name)
  file(COPY_FILE
    "${static_source}"
    "${_stage_root}/lib/${static_name}${_cpkt_static_library_suffix}"
  )
  get_filename_component(_facade_shared_library_dir "${shared_source}" DIRECTORY)
  set(_facade_shared_library_real_path "${_facade_shared_library_dir}/${shared_real_name}")
  if(NOT EXISTS "${_facade_shared_library_real_path}")
    message(FATAL_ERROR
      "missing ${facade_label} shared library ${shared_real_name} in ${_facade_shared_library_dir}")
  endif()
  file(COPY_FILE
    "${_facade_shared_library_real_path}"
    "${_stage_root}/lib/${shared_real_name}"
  )
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E create_symlink
      "${shared_real_name}"
      "${_stage_root}/lib/${shared_abi_name}"
    RESULT_VARIABLE _facade_abi_symlink_result
  )
  if(NOT _facade_abi_symlink_result EQUAL 0)
    message(FATAL_ERROR "failed to create ${facade_label} ABI symlink")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E create_symlink
      "${shared_abi_name}"
      "${_stage_root}/lib/${shared_link_name}"
    RESULT_VARIABLE _facade_link_symlink_result
  )
  if(NOT _facade_link_symlink_result EQUAL 0)
    message(FATAL_ERROR "failed to create ${facade_label} linker symlink")
  endif()
endfunction()

cpkt_stage_facade_library(
  "Lua runtime facade"
  "${CPKT_LUA_RUNTIME_STATIC_LIBRARY}"
  "libcpkt_lua_runtime"
  "${CPKT_LUA_RUNTIME_SHARED_LIBRARY}"
  "${_cpkt_lua_runtime_shared_library_real_name}"
  "${_cpkt_lua_runtime_shared_library_abi_name}"
  "${_cpkt_lua_runtime_shared_library_link_name}")
cpkt_stage_facade_library(
  "audio facade"
  "${CPKT_AUDIO_STATIC_LIBRARY}"
  "libcpkt_audio"
  "${CPKT_AUDIO_SHARED_LIBRARY}"
  "${_cpkt_audio_shared_library_real_name}"
  "${_cpkt_audio_shared_library_abi_name}"
  "${_cpkt_audio_shared_library_link_name}")
cpkt_stage_facade_library(
  "sus facade"
  "${CPKT_SUS_STATIC_LIBRARY}"
  "libcpktsus"
  "${CPKT_SUS_SHARED_LIBRARY}"
  "${_cpkt_sus_shared_library_real_name}"
  "${_cpkt_sus_shared_library_abi_name}"
  "${_cpkt_sus_shared_library_link_name}")
cpkt_stage_facade_library(
  "OPC UA facade"
  "${CPKT_OPCUA_STATIC_LIBRARY}"
  "libcpkt_opcua"
  "${CPKT_OPCUA_SHARED_LIBRARY}"
  "${_cpkt_opcua_shared_library_real_name}"
  "${_cpkt_opcua_shared_library_abi_name}"
  "${_cpkt_opcua_shared_library_link_name}")

if(DEFINED CPKT_CXX_STDLIB_STATIC_LIBRARY AND NOT "${CPKT_CXX_STDLIB_STATIC_LIBRARY}" STREQUAL "")
  if(NOT EXISTS "${CPKT_CXX_STDLIB_STATIC_LIBRARY}")
    message(FATAL_ERROR "configured static C++ standard library does not exist: ${CPKT_CXX_STDLIB_STATIC_LIBRARY}")
  endif()
  file(MAKE_DIRECTORY "${_stage_root}/lib/cpkt-cxx")
  file(COPY_FILE
    "${CPKT_CXX_STDLIB_STATIC_LIBRARY}"
    "${_stage_root}/lib/cpkt-cxx/libstdc++.a")
endif()
set(_cpkt_cxx_stdlib_static_pc_lib "")
if(EXISTS "${_stage_root}/lib/cpkt-cxx/libstdc++.a")
  set(_cpkt_cxx_stdlib_static_pc_lib "\${libdir}/cpkt-cxx/libstdc++.a")
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
set(_cpkt_libxml2_static_private_pc_libs "-lm")
set(_cpkt_libxml2_static_iconv_pc_libs "")
set(_cpkt_libxml2_static_iconv_cmake_libs "Iconv::Iconv")
set(_cpkt_libxml2_shared_iconv_cmake_libs "Iconv::Iconv")
set(_cpkt_lua_static_private_pc_libs "-lm")
set(_cpkt_open62541_static_private_pc_libs "-lm")
if(CPKT_TARGET_ID MATCHES "-linux-")
  set(_cpkt_libxml2_static_private_pc_libs "-ldl -lm -pthread")
  set(_cpkt_lua_static_private_pc_libs "-lm -ldl")
  set(_cpkt_open62541_static_private_pc_libs "-lm -lrt")
elseif(CPKT_TARGET_ID STREQUAL "arm64-apple-darwin")
  set(_cpkt_libxml2_static_iconv_pc_libs "-liconv")
  set(_cpkt_libxml2_static_iconv_cmake_libs "Iconv::Iconv;iconv")
  set(_cpkt_libxml2_shared_iconv_cmake_libs "Iconv::Iconv;iconv")
endif()
set(_cpkt_libxml2_static_private_pc_libs_full "${_cpkt_libxml2_static_private_pc_libs}")
if(_cpkt_libxml2_static_iconv_pc_libs)
  string(APPEND _cpkt_libxml2_static_private_pc_libs_full " ${_cpkt_libxml2_static_iconv_pc_libs}")
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

file(MAKE_DIRECTORY "${_stage_root}/lib/cmake/libxml2")
file(WRITE "${_stage_root}/lib/cmake/libxml2/libxml2-config.cmake"
  "include(CMakeFindDependencyMacro)\n"
  "get_filename_component(_cpkt_libxml2_prefix \"\${CMAKE_CURRENT_LIST_DIR}/../../..\" ABSOLUTE)\n"
  "set(ZLIB_DIR \"\${_cpkt_libxml2_prefix}/lib/cmake/zlib\")\n"
  "find_dependency(ZLIB CONFIG REQUIRED)\n"
  "find_dependency(Iconv REQUIRED)\n"
  "find_dependency(Threads REQUIRED)\n"
  "set(libxml2_FOUND TRUE)\n"
  "set(LibXml2_FOUND TRUE)\n"
  "set(LIBXML2_FOUND TRUE)\n"
  "set(libxml2_VERSION \"${CPKT_LIBXML2_VERSION}\")\n"
  "set(LibXml2_VERSION \"${CPKT_LIBXML2_VERSION}\")\n"
  "set(LIBXML2_VERSION_STRING \"${CPKT_LIBXML2_VERSION}\")\n"
  "set(LIBXML2_INCLUDE_DIR \"\${_cpkt_libxml2_prefix}/include/libxml2\")\n"
  "set(LIBXML2_INCLUDE_DIRS \"\${LIBXML2_INCLUDE_DIR}\")\n"
  "set(LIBXML2_LIBRARIES LibXml2::LibXml2)\n"
  "if(NOT TARGET LibXml2::LibXml2)\n"
  "  add_library(LibXml2::LibXml2 STATIC IMPORTED)\n"
  "  set_target_properties(LibXml2::LibXml2 PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_libxml2_prefix}/lib/libxml2${_cpkt_static_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${LIBXML2_INCLUDE_DIR}\"\n"
  "    INTERFACE_LINK_LIBRARIES \"ZLIB::ZLIB;${_cpkt_libxml2_static_iconv_cmake_libs};Threads::Threads;\${CMAKE_DL_LIBS};m\"\n"
  "  )\n"
  "endif()\n"
  "if(NOT TARGET cpkt::libxml2_shared)\n"
  "  add_library(cpkt::libxml2_shared SHARED IMPORTED)\n"
  "  set_target_properties(cpkt::libxml2_shared PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_libxml2_prefix}/lib/${_cpkt_libxml2_shared_library_name}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${LIBXML2_INCLUDE_DIR}\"\n"
  "    INTERFACE_LINK_LIBRARIES \"cpkt::zlib_shared;${_cpkt_libxml2_shared_iconv_cmake_libs}\"\n"
  "  )\n"
  "endif()\n"
)
file(WRITE "${_stage_root}/lib/cmake/libxml2/libxml2-config-version.cmake"
  "set(PACKAGE_VERSION \"${CPKT_LIBXML2_VERSION}\")\n"
  "if(PACKAGE_FIND_VERSION VERSION_EQUAL PACKAGE_VERSION)\n"
  "  set(PACKAGE_VERSION_EXACT TRUE)\n"
  "endif()\n"
  "if(PACKAGE_FIND_VERSION VERSION_LESS_EQUAL PACKAGE_VERSION)\n"
  "  set(PACKAGE_VERSION_COMPATIBLE TRUE)\n"
  "endif()\n"
)

file(MAKE_DIRECTORY "${_stage_root}/lib/cmake/Lua")
file(WRITE "${_stage_root}/lib/cmake/Lua/LuaConfig.cmake"
  "get_filename_component(_cpkt_lua_prefix \"\${CMAKE_CURRENT_LIST_DIR}/../../..\" ABSOLUTE)\n"
  "set(Lua_FOUND TRUE)\n"
  "set(LUA_FOUND TRUE)\n"
  "set(Lua_VERSION \"${CPKT_LUA_VERSION}\")\n"
  "set(LUA_VERSION_STRING \"${CPKT_LUA_VERSION}\")\n"
  "set(LUA_INCLUDE_DIR \"\${_cpkt_lua_prefix}/include\")\n"
  "set(LUA_INCLUDE_DIRS \"\${LUA_INCLUDE_DIR}\")\n"
  "set(LUA_LIBRARIES Lua::Lua)\n"
  "if(NOT TARGET Lua::Lua)\n"
  "  add_library(Lua::Lua STATIC IMPORTED)\n"
  "  set_target_properties(Lua::Lua PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_lua_prefix}/lib/liblua${_cpkt_static_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${LUA_INCLUDE_DIR}\"\n"
  "    INTERFACE_LINK_LIBRARIES \"m;\${CMAKE_DL_LIBS}\"\n"
  "  )\n"
  "endif()\n"
  "if(NOT TARGET cpkt::lua_shared)\n"
  "  add_library(cpkt::lua_shared SHARED IMPORTED)\n"
  "  set_target_properties(cpkt::lua_shared PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_lua_prefix}/lib/${_cpkt_lua_shared_library_name}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${LUA_INCLUDE_DIR}\"\n"
  "  )\n"
  "endif()\n"
)
cpkt_write_config_version("Lua" "Lua" "${CPKT_LUA_VERSION}")

file(MAKE_DIRECTORY "${_stage_root}/lib/cmake/CpktLuaRuntime")
file(WRITE "${_stage_root}/lib/cmake/CpktLuaRuntime/CpktLuaRuntimeConfig.cmake"
  "include(CMakeFindDependencyMacro)\n"
  "get_filename_component(_cpkt_lua_runtime_prefix \"\${CMAKE_CURRENT_LIST_DIR}/../../..\" ABSOLUTE)\n"
  "set(Lua_DIR \"\${_cpkt_lua_runtime_prefix}/lib/cmake/Lua\")\n"
  "find_dependency(Lua CONFIG REQUIRED)\n"
  "set(CpktLuaRuntime_FOUND TRUE)\n"
  "set(CpktLuaRuntime_VERSION \"${CPKT_LUA_VERSION}\")\n"
  "if(NOT TARGET cpkt::lua_runtime)\n"
  "  add_library(cpkt::lua_runtime STATIC IMPORTED)\n"
  "  set_target_properties(cpkt::lua_runtime PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_lua_runtime_prefix}/lib/libcpkt_lua_runtime${_cpkt_static_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${_cpkt_lua_runtime_prefix}/include\"\n"
  "    INTERFACE_LINK_LIBRARIES Lua::Lua\n"
  "  )\n"
  "endif()\n"
  "if(NOT TARGET cpkt::lua_runtime_shared)\n"
  "  add_library(cpkt::lua_runtime_shared SHARED IMPORTED)\n"
  "  set_target_properties(cpkt::lua_runtime_shared PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_lua_runtime_prefix}/lib/libcpkt_lua_runtime${_cpkt_shared_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${_cpkt_lua_runtime_prefix}/include\"\n"
  "    INTERFACE_LINK_LIBRARIES cpkt::lua_shared\n"
  "  )\n"
  "endif()\n"
)
cpkt_write_config_version("CpktLuaRuntime" "CpktLuaRuntime" "${CPKT_LUA_VERSION}")

file(MAKE_DIRECTORY "${_stage_root}/lib/cmake/miniaudio")
file(WRITE "${_stage_root}/lib/cmake/miniaudio/miniaudioConfig.cmake"
  "include(CMakeFindDependencyMacro)\n"
  "find_dependency(Threads REQUIRED)\n"
  "get_filename_component(_cpkt_miniaudio_prefix \"\${CMAKE_CURRENT_LIST_DIR}/../../..\" ABSOLUTE)\n"
  "set(miniaudio_FOUND TRUE)\n"
  "set(miniaudio_VERSION \"${CPKT_MINIAUDIO_VERSION}\")\n"
  "if(NOT TARGET miniaudio::miniaudio)\n"
  "  add_library(miniaudio::miniaudio STATIC IMPORTED)\n"
  "  set_target_properties(miniaudio::miniaudio PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_miniaudio_prefix}/lib/libminiaudio${_cpkt_static_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${_cpkt_miniaudio_prefix}/include/miniaudio\"\n"
  "    INTERFACE_LINK_LIBRARIES \"m;Threads::Threads\"\n"
  "  )\n"
  "endif()\n"
  "if(NOT TARGET cpkt::miniaudio_shared)\n"
  "  add_library(cpkt::miniaudio_shared SHARED IMPORTED)\n"
  "  set_target_properties(cpkt::miniaudio_shared PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_miniaudio_prefix}/lib/libminiaudio${_cpkt_shared_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${_cpkt_miniaudio_prefix}/include/miniaudio\"\n"
  "    INTERFACE_LINK_LIBRARIES \"m;Threads::Threads\"\n"
  "  )\n"
  "endif()\n"
)
cpkt_write_config_version("miniaudio" "miniaudio" "${CPKT_MINIAUDIO_VERSION}")

file(MAKE_DIRECTORY "${_stage_root}/lib/cmake/CpktAudio")
file(WRITE "${_stage_root}/lib/cmake/CpktAudio/CpktAudioConfig.cmake"
  "include(CMakeFindDependencyMacro)\n"
  "get_filename_component(_cpkt_audio_prefix \"\${CMAKE_CURRENT_LIST_DIR}/../../..\" ABSOLUTE)\n"
  "set(miniaudio_DIR \"\${_cpkt_audio_prefix}/lib/cmake/miniaudio\")\n"
  "find_dependency(miniaudio CONFIG REQUIRED)\n"
  "set(CpktAudio_FOUND TRUE)\n"
  "set(CpktAudio_VERSION \"${CPKT_MINIAUDIO_VERSION}\")\n"
  "if(NOT TARGET cpkt::audio)\n"
  "  add_library(cpkt::audio STATIC IMPORTED)\n"
  "  set_target_properties(cpkt::audio PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_audio_prefix}/lib/libcpkt_audio${_cpkt_static_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${_cpkt_audio_prefix}/include\"\n"
  "    INTERFACE_LINK_LIBRARIES miniaudio::miniaudio\n"
  "  )\n"
  "endif()\n"
  "if(NOT TARGET cpkt::audio_shared)\n"
  "  add_library(cpkt::audio_shared SHARED IMPORTED)\n"
  "  set_target_properties(cpkt::audio_shared PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_audio_prefix}/lib/libcpkt_audio${_cpkt_shared_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${_cpkt_audio_prefix}/include\"\n"
  "    INTERFACE_LINK_LIBRARIES cpkt::miniaudio_shared\n"
  "  )\n"
  "endif()\n"
)
cpkt_write_config_version("CpktAudio" "CpktAudio" "${CPKT_MINIAUDIO_VERSION}")

file(MAKE_DIRECTORY "${_stage_root}/lib/cmake/whisper")
file(WRITE "${_stage_root}/lib/cmake/whisper/whisperConfig.cmake"
  "include(CMakeFindDependencyMacro)\n"
  "find_dependency(Threads REQUIRED)\n"
  "get_filename_component(_cpkt_whisper_prefix \"\${CMAKE_CURRENT_LIST_DIR}/../../..\" ABSOLUTE)\n"
  "set(whisper_FOUND TRUE)\n"
  "set(whisper_VERSION \"${CPKT_WHISPER_VERSION}\")\n"
  "set(_cpkt_cxx_stdlib_static \"\${_cpkt_whisper_prefix}/lib/cpkt-cxx/libstdc++.a\")\n"
  "if(NOT EXISTS \"\${_cpkt_cxx_stdlib_static}\")\n"
  "  set(_cpkt_cxx_stdlib_static \"\")\n"
  "endif()\n"
  "if(NOT TARGET ggml::ggml)\n"
  "  add_library(ggml::ggml STATIC IMPORTED)\n"
  "  set_target_properties(ggml::ggml PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_whisper_prefix}/lib/libggml${_cpkt_static_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${_cpkt_whisper_prefix}/include\"\n"
  "  )\n"
  "endif()\n"
  "if(NOT TARGET ggml::base)\n"
  "  add_library(ggml::base STATIC IMPORTED)\n"
  "  set_target_properties(ggml::base PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_whisper_prefix}/lib/libggml-base${_cpkt_static_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${_cpkt_whisper_prefix}/include\"\n"
  "  )\n"
  "endif()\n"
  "if(NOT TARGET ggml::cpu)\n"
  "  add_library(ggml::cpu STATIC IMPORTED)\n"
  "  set_target_properties(ggml::cpu PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_whisper_prefix}/lib/libggml-cpu${_cpkt_static_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${_cpkt_whisper_prefix}/include\"\n"
  "  )\n"
  "endif()\n"
  "if(NOT TARGET whisper::whisper)\n"
  "  add_library(whisper::whisper STATIC IMPORTED)\n"
  "  set_target_properties(whisper::whisper PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_whisper_prefix}/lib/libwhisper${_cpkt_static_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${_cpkt_whisper_prefix}/include\"\n"
  "    INTERFACE_LINK_LIBRARIES \"ggml::ggml;ggml::base;ggml::cpu;Threads::Threads;m;\${_cpkt_cxx_stdlib_static}\"\n"
  "  )\n"
  "endif()\n"
  "if(NOT TARGET cpkt::whisper_shared)\n"
  "  add_library(cpkt::whisper_shared SHARED IMPORTED)\n"
  "  set_target_properties(cpkt::whisper_shared PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_whisper_prefix}/lib/libwhisper${_cpkt_shared_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${_cpkt_whisper_prefix}/include\"\n"
  "  )\n"
  "endif()\n"
)
cpkt_write_config_version("whisper" "whisper" "${CPKT_WHISPER_VERSION}")

file(MAKE_DIRECTORY "${_stage_root}/lib/cmake/CpktSus")
file(WRITE "${_stage_root}/lib/cmake/CpktSus/CpktSusConfig.cmake"
  "include(CMakeFindDependencyMacro)\n"
  "get_filename_component(_cpkt_sus_prefix \"\${CMAKE_CURRENT_LIST_DIR}/../../..\" ABSOLUTE)\n"
  "set(whisper_DIR \"\${_cpkt_sus_prefix}/lib/cmake/whisper\")\n"
  "find_dependency(whisper CONFIG REQUIRED)\n"
  "set(CpktSus_FOUND TRUE)\n"
  "set(CpktSus_VERSION \"${CPKT_WHISPER_VERSION}\")\n"
  "if(NOT TARGET cpkt::sus)\n"
  "  add_library(cpkt::sus STATIC IMPORTED)\n"
  "  set_target_properties(cpkt::sus PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_sus_prefix}/lib/libcpktsus${_cpkt_static_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${_cpkt_sus_prefix}/include\"\n"
  "    INTERFACE_LINK_LIBRARIES whisper::whisper\n"
  "  )\n"
  "endif()\n"
  "if(NOT TARGET cpkt::sus_shared)\n"
  "  add_library(cpkt::sus_shared SHARED IMPORTED)\n"
  "  set_target_properties(cpkt::sus_shared PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_sus_prefix}/lib/libcpktsus${_cpkt_shared_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${_cpkt_sus_prefix}/include\"\n"
  "    INTERFACE_LINK_LIBRARIES cpkt::whisper_shared\n"
  "  )\n"
  "endif()\n"
)
cpkt_write_config_version("CpktSus" "CpktSus" "${CPKT_WHISPER_VERSION}")

file(MAKE_DIRECTORY "${_stage_root}/lib/cmake/open62541")
file(WRITE "${_stage_root}/lib/cmake/open62541/open62541Config.cmake"
  "include(CMakeFindDependencyMacro)\n"
  "get_filename_component(_cpkt_open62541_prefix \"\${CMAKE_CURRENT_LIST_DIR}/../../..\" ABSOLUTE)\n"
  "set(OpenSSL_DIR \"\${_cpkt_open62541_prefix}/lib/cmake/OpenSSL\")\n"
  "find_dependency(OpenSSL CONFIG REQUIRED)\n"
  "set(open62541_FOUND TRUE)\n"
  "set(open62541_VERSION \"${CPKT_OPEN62541_VERSION}\")\n"
  "set(open62541_INCLUDE_DIR \"\${_cpkt_open62541_prefix}/include\")\n"
  "set(open62541_INCLUDE_DIRS \"\${open62541_INCLUDE_DIR}\")\n"
  "set(open62541_LIBRARIES open62541::open62541)\n"
  "set(_cpkt_open62541_static_system_libs m)\n"
  "if(CMAKE_SYSTEM_NAME STREQUAL \"Linux\")\n"
  "  list(APPEND _cpkt_open62541_static_system_libs rt)\n"
  "endif()\n"
  "if(NOT TARGET open62541::open62541)\n"
  "  add_library(open62541::open62541 STATIC IMPORTED)\n"
  "  set_target_properties(open62541::open62541 PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_open62541_prefix}/lib/libopen62541${_cpkt_static_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${open62541_INCLUDE_DIR}\"\n"
  "    INTERFACE_LINK_LIBRARIES \"OpenSSL::SSL;OpenSSL::Crypto;\${_cpkt_open62541_static_system_libs}\"\n"
  "    INTERFACE_COMPILE_DEFINITIONS _GNU_SOURCE\n"
  "  )\n"
  "endif()\n"
  "if(NOT TARGET cpkt::open62541_shared)\n"
  "  add_library(cpkt::open62541_shared SHARED IMPORTED)\n"
  "  set_target_properties(cpkt::open62541_shared PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_open62541_prefix}/lib/${_cpkt_open62541_shared_library_name}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${open62541_INCLUDE_DIR}\"\n"
  "    INTERFACE_LINK_LIBRARIES \"cpkt::openssl_ssl_shared;cpkt::openssl_crypto_shared\"\n"
  "    INTERFACE_COMPILE_DEFINITIONS _GNU_SOURCE\n"
  "  )\n"
  "endif()\n"
)
cpkt_write_config_version("open62541" "open62541" "${CPKT_OPEN62541_VERSION}")

file(MAKE_DIRECTORY "${_stage_root}/lib/cmake/CpktOpcUa")
file(WRITE "${_stage_root}/lib/cmake/CpktOpcUa/CpktOpcUaConfig.cmake"
  "include(CMakeFindDependencyMacro)\n"
  "get_filename_component(_cpkt_opcua_prefix \"\${CMAKE_CURRENT_LIST_DIR}/../../..\" ABSOLUTE)\n"
  "set(open62541_DIR \"\${_cpkt_opcua_prefix}/lib/cmake/open62541\")\n"
  "find_dependency(open62541 CONFIG REQUIRED)\n"
  "set(CpktOpcUa_FOUND TRUE)\n"
  "set(CpktOpcUa_VERSION \"${CPKT_OPEN62541_VERSION}\")\n"
  "if(NOT TARGET cpkt::opcua)\n"
  "  add_library(cpkt::opcua STATIC IMPORTED)\n"
  "  set_target_properties(cpkt::opcua PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_opcua_prefix}/lib/libcpkt_opcua${_cpkt_static_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${_cpkt_opcua_prefix}/include\"\n"
  "    INTERFACE_LINK_LIBRARIES open62541::open62541\n"
  "  )\n"
  "endif()\n"
  "if(NOT TARGET cpkt::opcua_shared)\n"
  "  add_library(cpkt::opcua_shared SHARED IMPORTED)\n"
  "  set_target_properties(cpkt::opcua_shared PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_opcua_prefix}/lib/libcpkt_opcua${_cpkt_shared_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${_cpkt_opcua_prefix}/include\"\n"
  "    INTERFACE_LINK_LIBRARIES cpkt::open62541_shared\n"
  "  )\n"
  "endif()\n"
)
cpkt_write_config_version("CpktOpcUa" "CpktOpcUa" "${CPKT_OPEN62541_VERSION}")

file(MAKE_DIRECTORY "${_stage_root}/lib/cmake/mqtt-c")
file(WRITE "${_stage_root}/lib/cmake/mqtt-c/mqtt-cConfig.cmake"
  "include(CMakeFindDependencyMacro)\n"
  "find_dependency(Threads REQUIRED)\n"
  "get_filename_component(_cpkt_mqttc_prefix \"\${CMAKE_CURRENT_LIST_DIR}/../../..\" ABSOLUTE)\n"
  "set(mqtt-c_FOUND TRUE)\n"
  "set(MQTTC_FOUND TRUE)\n"
  "set(MQTTC_VERSION \"${CPKT_MQTTC_VERSION}\")\n"
  "set(MQTTC_COMMIT \"${CPKT_MQTTC_COMMIT}\")\n"
  "set(MQTTC_INCLUDE_DIR \"\${_cpkt_mqttc_prefix}/include\")\n"
  "set(MQTTC_INCLUDE_DIRS \"\${MQTTC_INCLUDE_DIR}\")\n"
  "set(MQTTC_LIBRARIES MQTT-C::mqttc)\n"
  "if(NOT TARGET MQTT-C::mqttc)\n"
  "  add_library(MQTT-C::mqttc STATIC IMPORTED)\n"
  "  set_target_properties(MQTT-C::mqttc PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_mqttc_prefix}/lib/libmqttc${_cpkt_static_library_suffix}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${MQTTC_INCLUDE_DIR}\"\n"
  "    INTERFACE_LINK_LIBRARIES Threads::Threads\n"
  "  )\n"
  "endif()\n"
  "if(NOT TARGET cpkt::mqttc_shared)\n"
  "  add_library(cpkt::mqttc_shared SHARED IMPORTED)\n"
  "  set_target_properties(cpkt::mqttc_shared PROPERTIES\n"
  "    IMPORTED_LOCATION \"\${_cpkt_mqttc_prefix}/lib/${_cpkt_mqttc_shared_library_name}\"\n"
  "    INTERFACE_INCLUDE_DIRECTORIES \"\${MQTTC_INCLUDE_DIR}\"\n"
  "    INTERFACE_LINK_LIBRARIES Threads::Threads\n"
  "  )\n"
  "endif()\n"
)
cpkt_write_config_version("mqtt-c" "mqtt-c" "${CPKT_MQTTC_VERSION}")

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
file(WRITE "${_stage_root}/lib/pkgconfig/libxml-2.0.pc"
  "prefix=\${pcfiledir}/../..\n"
  "exec_prefix=\${prefix}\n"
  "libdir=\${prefix}/lib\n"
  "includedir=\${prefix}/include\n"
  "\n"
  "Name: libXML\n"
  "Description: XML C parser and toolkit from c.pkt.systems\n"
  "Version: ${CPKT_LIBXML2_VERSION}\n"
  "Requires.private: zlib\n"
  "Libs: -L\${libdir} -lxml2\n"
  "Libs.private: ${_cpkt_libxml2_static_private_pc_libs_full}\n"
  "Cflags: -I\${includedir}/libxml2\n"
)
foreach(_lua_pc_name lua lua5.5)
  file(WRITE "${_stage_root}/lib/pkgconfig/${_lua_pc_name}.pc"
    "prefix=\${pcfiledir}/../..\n"
    "exec_prefix=\${prefix}\n"
    "libdir=\${prefix}/lib\n"
    "includedir=\${prefix}/include\n"
    "\n"
    "Name: Lua\n"
    "Description: Lua language runtime from c.pkt.systems\n"
    "Version: ${CPKT_LUA_VERSION}\n"
    "Libs: -L\${libdir} -llua\n"
    "Libs.private: ${_cpkt_lua_static_private_pc_libs}\n"
    "Cflags: -I\${includedir}\n"
  )
endforeach()
file(WRITE "${_stage_root}/lib/pkgconfig/cpkt-lua-runtime.pc"
  "prefix=\${pcfiledir}/../..\n"
  "exec_prefix=\${prefix}\n"
  "libdir=\${prefix}/lib\n"
  "includedir=\${prefix}/include\n"
  "\n"
  "Name: cpkt-lua-runtime\n"
  "Description: C89-safe Lua runtime facade from c.pkt.systems\n"
  "Version: ${CPKT_LUA_VERSION}\n"
  "Requires.private: lua\n"
  "Libs: -L\${libdir} -lcpkt_lua_runtime\n"
  "Cflags: -I\${includedir}\n"
)
file(WRITE "${_stage_root}/lib/pkgconfig/miniaudio.pc"
  "prefix=\${pcfiledir}/../..\n"
  "exec_prefix=\${prefix}\n"
  "libdir=\${prefix}/lib\n"
  "includedir=\${prefix}/include\n"
  "\n"
  "Name: miniaudio\n"
  "Description: miniaudio from c.pkt.systems\n"
  "Version: ${CPKT_MINIAUDIO_VERSION}\n"
  "Libs: -L\${libdir} -lminiaudio\n"
  "Libs.private: -lm -pthread\n"
  "Cflags: -I\${includedir}/miniaudio\n"
)
file(WRITE "${_stage_root}/lib/pkgconfig/cpkt-audio.pc"
  "prefix=\${pcfiledir}/../..\n"
  "exec_prefix=\${prefix}\n"
  "libdir=\${prefix}/lib\n"
  "includedir=\${prefix}/include\n"
  "\n"
  "Name: cpkt-audio\n"
  "Description: C89-safe miniaudio facade from c.pkt.systems\n"
  "Version: ${CPKT_MINIAUDIO_VERSION}\n"
  "Requires.private: miniaudio\n"
  "Libs: -L\${libdir} -lcpkt_audio\n"
  "Cflags: -I\${includedir}\n"
)
file(WRITE "${_stage_root}/lib/pkgconfig/whisper.pc"
  "prefix=\${pcfiledir}/../..\n"
  "exec_prefix=\${prefix}\n"
  "libdir=\${prefix}/lib\n"
  "includedir=\${prefix}/include\n"
  "\n"
  "Name: whisper.cpp\n"
  "Description: whisper.cpp from c.pkt.systems\n"
  "Version: ${CPKT_WHISPER_VERSION}\n"
  "Libs: -L\${libdir} -lwhisper\n"
  "Libs.private: -lggml -lggml-base -lggml-cpu ${_cpkt_cxx_stdlib_static_pc_lib} -lm -pthread\n"
  "Cflags: -I\${includedir}\n"
)
file(WRITE "${_stage_root}/lib/pkgconfig/cpkt-sus.pc"
  "prefix=\${pcfiledir}/../..\n"
  "exec_prefix=\${prefix}\n"
  "libdir=\${prefix}/lib\n"
  "includedir=\${prefix}/include\n"
  "\n"
  "Name: cpkt-sus\n"
  "Description: C89-safe whisper.cpp facade from c.pkt.systems\n"
  "Version: ${CPKT_WHISPER_VERSION}\n"
  "Requires.private: whisper\n"
  "Libs: -L\${libdir} -lcpktsus\n"
  "Cflags: -I\${includedir}\n"
)
file(WRITE "${_stage_root}/lib/pkgconfig/open62541.pc"
  "prefix=\${pcfiledir}/../..\n"
  "exec_prefix=\${prefix}\n"
  "libdir=\${prefix}/lib\n"
  "includedir=\${prefix}/include\n"
  "\n"
  "Name: open62541\n"
  "Description: OPC UA client and server library from c.pkt.systems\n"
  "Version: ${CPKT_OPEN62541_VERSION}\n"
  "Requires.private: openssl\n"
  "Libs: -L\${libdir} -lopen62541\n"
  "Libs.private: ${_cpkt_open62541_static_private_pc_libs}\n"
  "Cflags: -I\${includedir} -D_GNU_SOURCE\n"
)
file(WRITE "${_stage_root}/lib/pkgconfig/cpkt-opcua.pc"
  "prefix=\${pcfiledir}/../..\n"
  "exec_prefix=\${prefix}\n"
  "libdir=\${prefix}/lib\n"
  "includedir=\${prefix}/include\n"
  "\n"
  "Name: cpkt-opcua\n"
  "Description: C89-safe OPC UA facade from c.pkt.systems\n"
  "Version: ${CPKT_OPEN62541_VERSION}\n"
  "Requires.private: open62541\n"
  "Libs: -L\${libdir} -lcpkt_opcua\n"
  "Cflags: -I\${includedir}\n"
)
file(WRITE "${_stage_root}/lib/pkgconfig/mqtt-c.pc"
  "prefix=\${pcfiledir}/../..\n"
  "exec_prefix=\${prefix}\n"
  "libdir=\${prefix}/lib\n"
  "includedir=\${prefix}/include\n"
  "\n"
  "Name: MQTT-C\n"
  "Description: Portable MQTT v3.1.1 client library from c.pkt.systems\n"
  "Version: ${CPKT_MQTTC_VERSION}\n"
  "Libs: -L\${libdir} -lmqttc\n"
  "Libs.private: -pthread\n"
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
  "libxml2_version=${CPKT_LIBXML2_VERSION}\n"
  "lua_version=${CPKT_LUA_VERSION}\n"
  "miniaudio_version=${CPKT_MINIAUDIO_VERSION}\n"
  "whisper_version=${CPKT_WHISPER_VERSION}\n"
  "mqtt_c_version=${CPKT_MQTTC_VERSION}\n"
  "mqtt_c_commit=${CPKT_MQTTC_COMMIT}\n"
  "open62541_version=${CPKT_OPEN62541_VERSION}\n"
  "open62541_patchset=${CPKT_OPEN62541_PATCHSET}\n"
  "lua_runtime_abi_version=${CPKT_LUA_RUNTIME_ABI_VERSION}\n"
  "audio_abi_version=${CPKT_AUDIO_ABI_VERSION}\n"
  "sus_abi_version=${CPKT_SUS_ABI_VERSION}\n"
  "opcua_abi_version=${CPKT_OPCUA_ABI_VERSION}\n"
)

file(MAKE_DIRECTORY
  "${_stage_root}/share/doc/c.pkt.systems"
  "${_stage_root}/share/doc/c.pkt.systems/docs")
file(COPY_FILE
  "${CPKT_SOURCE_DIR}/LICENSE"
  "${_stage_root}/share/doc/c.pkt.systems/LICENSE")
file(COPY_FILE
  "${CPKT_SOURCE_DIR}/README.md"
  "${_stage_root}/share/doc/c.pkt.systems/README.md")
file(COPY_FILE
  "${CPKT_SOURCE_DIR}/docs/opcua-c89-facade-spec.md"
  "${_stage_root}/share/doc/c.pkt.systems/docs/opcua-c89-facade-spec.md")
file(COPY
  "${CPKT_SOURCE_DIR}/examples/"
  DESTINATION "${_stage_root}/share/doc/c.pkt.systems/examples")

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
cpkt_stage_license("libxml2" "${CPKT_DEPENDENCY_BUILD_ROOT}/libxml2/src/Copyright")
cpkt_stage_license("lua" "${CPKT_DEPENDENCY_BUILD_ROOT}/lua/src/src/lua.h")
cpkt_stage_license("miniaudio" "${CPKT_DEPENDENCY_BUILD_ROOT}/miniaudio/src/LICENSE")
cpkt_stage_license("whisper.cpp" "${CPKT_DEPENDENCY_BUILD_ROOT}/whisper/src/LICENSE")
cpkt_stage_license("mqtt-c" "${CPKT_DEPENDENCY_BUILD_ROOT}/mqtt-c/src/LICENSE")
cpkt_stage_license("open62541" "${CPKT_DEPENDENCY_BUILD_ROOT}/open62541/src/LICENSE")
file(MAKE_DIRECTORY "${_stage_root}/share/doc/c.pkt.systems/third_party/open62541/patches")
file(COPY
  "${CPKT_SOURCE_DIR}/vendor/open62541/patches/series"
  DESTINATION "${_stage_root}/share/doc/c.pkt.systems/third_party/open62541/patches")
file(READ "${CPKT_SOURCE_DIR}/vendor/open62541/patches/series" _cpkt_open62541_patch_series)
string(REPLACE "\r\n" "\n" _cpkt_open62541_patch_series "${_cpkt_open62541_patch_series}")
string(REPLACE "\n" ";" _cpkt_open62541_patch_entries "${_cpkt_open62541_patch_series}")
foreach(_cpkt_open62541_patch IN LISTS _cpkt_open62541_patch_entries)
  string(STRIP "${_cpkt_open62541_patch}" _cpkt_open62541_patch)
  if(_cpkt_open62541_patch STREQUAL "" OR _cpkt_open62541_patch MATCHES "^#")
    continue()
  endif()
  set(_cpkt_open62541_patch_path "${CPKT_SOURCE_DIR}/vendor/open62541/patches/${_cpkt_open62541_patch}")
  if(NOT EXISTS "${_cpkt_open62541_patch_path}")
    message(FATAL_ERROR "open62541 patch listed in series does not exist: ${_cpkt_open62541_patch_path}")
  endif()
  file(COPY
    "${_cpkt_open62541_patch_path}"
    DESTINATION "${_stage_root}/share/doc/c.pkt.systems/third_party/open62541/patches")
endforeach()

execute_process(
  COMMAND "${_cpkt_gnu_tar}" --sort=name --owner=0 --group=0 --numeric-owner -czf "${_archive_path}" -- "${_archive_stem}"
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
