include_guard(GLOBAL)

function(cpkt_dependency_set_cache_id out_var)
  foreach(_cpkt_required_var IN ITEMS
      CPKT_OPENSSL_VERSION
      CPKT_OPENSSL_BUILD_CONFIG_REVISION
      CPKT_ZLIB_VERSION
      CPKT_CURL_VERSION
      CPKT_NGHTTP2_VERSION
      CPKT_LIBSSH2_VERSION
      CPKT_LIBXML2_VERSION
      CPKT_LUA_VERSION
      CPKT_MINIAUDIO_VERSION
      CPKT_WHISPER_VERSION
      CPKT_MQTTC_VERSION
      CPKT_MQTTC_COMMIT
      CPKT_OPEN62541_VERSION
      CPKT_OPEN62541_PATCHSET
      CPKT_CMOCKA_VERSION)
    if(NOT DEFINED ${_cpkt_required_var} OR "${${_cpkt_required_var}}" STREQUAL "")
      message(FATAL_ERROR
        "Dependency set identity requires ${_cpkt_required_var}")
    endif()
  endforeach()

  set(_cpkt_identity
    "openssl-${CPKT_OPENSSL_VERSION}-${CPKT_OPENSSL_BUILD_CONFIG_REVISION}_zlib-${CPKT_ZLIB_VERSION}_curl-${CPKT_CURL_VERSION}_nghttp2-${CPKT_NGHTTP2_VERSION}_libssh2-${CPKT_LIBSSH2_VERSION}_libxml2-${CPKT_LIBXML2_VERSION}_lua-${CPKT_LUA_VERSION}_miniaudio-${CPKT_MINIAUDIO_VERSION}_whisper-${CPKT_WHISPER_VERSION}_mqtt-c-${CPKT_MQTTC_VERSION}-${CPKT_MQTTC_COMMIT}_open62541-${CPKT_OPEN62541_VERSION}-${CPKT_OPEN62541_PATCHSET}_cmocka-${CPKT_CMOCKA_VERSION}")
  set(${out_var} "${_cpkt_identity}" PARENT_SCOPE)
endfunction()
