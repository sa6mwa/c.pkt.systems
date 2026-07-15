if(NOT DEFINED CPKT_SOURCE_DIR OR CPKT_SOURCE_DIR STREQUAL "")
  message(FATAL_ERROR "CPKT_SOURCE_DIR is required")
endif()

include("${CPKT_SOURCE_DIR}/cmake/CpktDependencySetIdentity.cmake")

set(CPKT_OPENSSL_VERSION 3.6.2)
set(CPKT_OPENSSL_BUILD_CONFIG_REVISION origin-runpath-1)
set(CPKT_ZLIB_VERSION 1.3.2)
set(CPKT_CURL_VERSION 8.20.0)
set(CPKT_NGHTTP2_VERSION 1.69.0)
set(CPKT_LIBSSH2_VERSION 1.11.1)
set(CPKT_LIBXML2_VERSION 2.15.3)
set(CPKT_LUA_VERSION 5.5.0)
set(CPKT_MINIAUDIO_VERSION 0.11.25)
set(CPKT_WHISPER_VERSION v1.9.1)
set(CPKT_MQTTC_VERSION 1.1.2)
set(CPKT_MQTTC_COMMIT 0f4c34c8cc00b16cfee094745d68b8cdbaecd8e0)
set(CPKT_OPEN62541_VERSION 1.5.4)
set(CPKT_OPEN62541_PATCHSET mqtt-prefix-musl-warning-1)
set(CPKT_CMOCKA_VERSION 2.0.2)

cpkt_dependency_set_cache_id(_cpkt_origin_runpath_identity)
if(NOT _cpkt_origin_runpath_identity MATCHES "^openssl-3\\.6\\.2-origin-runpath-1_")
  message(FATAL_ERROR
    "OpenSSL build-config revision is absent from the dependency identity: ${_cpkt_origin_runpath_identity}")
endif()

set(CPKT_OPENSSL_BUILD_CONFIG_REVISION origin-runpath-2)
cpkt_dependency_set_cache_id(_cpkt_changed_identity)
if(_cpkt_changed_identity STREQUAL _cpkt_origin_runpath_identity)
  message(FATAL_ERROR
    "Changing OpenSSL Configure flags must select a new dependency identity")
endif()

message(STATUS "dependency set identity passed")
