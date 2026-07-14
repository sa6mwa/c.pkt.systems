cmake_minimum_required(VERSION 3.21)

execute_process(
  COMMAND "${CMAKE_SOURCE_DIR}/scripts/release-version.sh" "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE CPKT_PROJECT_VERSION
  RESULT_VARIABLE CPKT_PROJECT_VERSION_RESULT
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT CPKT_PROJECT_VERSION_RESULT EQUAL 0)
  message(FATAL_ERROR "failed to resolve c.pkt.systems release version")
endif()

project(c_pkt_systems VERSION "${CPKT_PROJECT_VERSION}" LANGUAGES C CXX)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CPKT_OSXCROSS_ROOT)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK
    "${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=${CPKT_OSXCROSS_ROOT}/lib:$ENV{LD_LIBRARY_PATH}")
endif()

include(CTest)
include(ProcessorCount)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

option(CPKT_BUILD_DEPENDENCIES "Download and build pinned third-party dependencies." ON)
option(CPKT_BUILD_TESTS "Build ABI/link smoke tests for published dependency artifacts." ON)
option(CPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE "Honor explicitly provided dependency roots even when they differ from this compiler's default cache key." OFF)
option(CPKT_ENABLE_FUZZING "Build AFL++ GCC-plugin facade fuzz targets." OFF)
option(CPKT_FACADE_ONLY "Build only repo-owned facade hardening targets." OFF)
option(CPKT_SUS_CPU_ONLY "Build the cpkt_sus whisper.cpp dependency set with CPU backends only." ON)

function(cpkt_normalize_arch arch out_var)
  string(TOLOWER "${arch}" _cpkt_arch)
  if(_cpkt_arch STREQUAL "amd64")
    set(_cpkt_arch "x86_64")
  elseif(_cpkt_arch STREQUAL "arm64")
    set(_cpkt_arch "aarch64")
  elseif(_cpkt_arch STREQUAL "arm")
    set(_cpkt_arch "armhf")
  endif()
  set(${out_var} "${_cpkt_arch}" PARENT_SCOPE)
endfunction()

function(cpkt_target_executables_runnable out_var)
  string(TOLOWER "${CMAKE_SYSTEM_NAME}" _cpkt_target_os)
  string(TOLOWER "${CMAKE_HOST_SYSTEM_NAME}" _cpkt_host_os)
  cpkt_normalize_arch("${CMAKE_SYSTEM_PROCESSOR}" _cpkt_target_arch)
  cpkt_normalize_arch("${CMAKE_HOST_SYSTEM_PROCESSOR}" _cpkt_host_arch)

  if(CMAKE_CROSSCOMPILING_EMULATOR)
    set(${out_var} ON PARENT_SCOPE)
  elseif(_cpkt_target_os STREQUAL _cpkt_host_os AND _cpkt_target_arch STREQUAL _cpkt_host_arch)
    set(${out_var} ON PARENT_SCOPE)
  else()
    set(${out_var} OFF PARENT_SCOPE)
  endif()
endfunction()

cpkt_target_executables_runnable(CPKT_CAN_RUN_TARGET_EXECUTABLES)

set(CPKT_TEST_EXECUTABLE_PREFIX)
if(CMAKE_CROSSCOMPILING AND CMAKE_CROSSCOMPILING_EMULATOR)
  set(CPKT_TEST_EXECUTABLE_PREFIX ${CMAKE_CROSSCOMPILING_EMULATOR})
endif()

function(cpkt_add_repo_warning_errors target_name)
  get_target_property(_cpkt_target_type ${target_name} TYPE)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(${target_name} PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Werror
      "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=src"
      "-ffile-prefix-map=${CMAKE_BINARY_DIR}=build"
      "-fdebug-prefix-map=${CMAKE_SOURCE_DIR}=src"
      "-fdebug-prefix-map=${CMAKE_BINARY_DIR}=build")
    if(NOT _cpkt_target_type STREQUAL "STATIC_LIBRARY")
      if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        target_link_options(${target_name} PRIVATE LINKER:-fatal_warnings)
      else()
        target_link_options(${target_name} PRIVATE LINKER:--fatal-warnings)
      endif()
    endif()
  elseif(MSVC)
    target_compile_options(${target_name} PRIVATE /W4 /WX)
    if(NOT _cpkt_target_type STREQUAL "STATIC_LIBRARY")
      target_link_options(${target_name} PRIVATE LINKER:/WX)
    endif()
  endif()
endfunction()

function(cpkt_add_lua_runtime_mock_test target_name source_name)
  add_executable(${target_name}
    src/lua_runtime.c
    tests/lua_mock/lua_mock.c
    ${source_name})
  target_compile_options(${target_name} PRIVATE -std=c99)
  cpkt_add_repo_warning_errors(${target_name})
  target_include_directories(${target_name} PRIVATE
    "${CMAKE_SOURCE_DIR}/tests/lua_mock/include"
    "${CMAKE_SOURCE_DIR}/include")
endfunction()

function(cpkt_configure_afl_target target_name)
  target_sources(${target_name} PRIVATE "${CMAKE_SOURCE_DIR}/fuzz/afl_input_driver.c")
  target_compile_options(${target_name} PRIVATE -std=c99)
  cpkt_add_repo_warning_errors(${target_name})
endfunction()

if(CPKT_FACADE_ONLY)
  foreach(_cpkt_facade_only_cache_var
      CPKT_DIST_DIR
      CPKT_TARGET_ARCH
      CPKT_TARGET_LIBC
      CPKT_TARGET_OS)
    if(DEFINED ${_cpkt_facade_only_cache_var})
      set(_cpkt_facade_only_ignored "${${_cpkt_facade_only_cache_var}}")
    endif()
  endforeach()
  unset(_cpkt_facade_only_ignored)

  if(NOT CPKT_BUILD_TESTS)
    message(FATAL_ERROR "CPKT_FACADE_ONLY requires CPKT_BUILD_TESTS=ON")
  endif()

  cpkt_add_lua_runtime_mock_test(cpkt_lua_runtime_mock_test tests/lua_runtime_mock_test.c)

  if(CPKT_CAN_RUN_TARGET_EXECUTABLES)
    add_test(NAME lua_runtime_mock_unit COMMAND cpkt_lua_runtime_mock_test)
  endif()

  if(CPKT_ENABLE_FUZZING)
    cpkt_add_lua_runtime_mock_test(cpkt_lua_runtime_fuzz fuzz/lua_runtime_fuzz.c)
    cpkt_configure_afl_target(cpkt_lua_runtime_fuzz)
  endif()

  return()
endif()

if(DEFINED CPKT_BUILD_STATIC AND NOT CPKT_BUILD_STATIC)
  message(FATAL_ERROR "c.pkt.systems release bundles always include static and shared artifacts; CPKT_BUILD_STATIC cannot be disabled.")
endif()
if(DEFINED CPKT_BUILD_SHARED AND NOT CPKT_BUILD_SHARED)
  message(FATAL_ERROR "c.pkt.systems release bundles always include static and shared artifacts; CPKT_BUILD_SHARED cannot be disabled.")
endif()
set(CPKT_BUILD_STATIC ON CACHE INTERNAL "c.pkt.systems release bundles always include static artifacts.")
set(CPKT_BUILD_SHARED ON CACHE INTERNAL "c.pkt.systems release bundles always include shared artifacts.")

set(CPKT_BUNDLE_VERSION "${PROJECT_VERSION}" CACHE STRING "Published dependency bundle version." FORCE)
set(CPKT_OPENSSL_VERSION "3.6.2" CACHE STRING "Pinned OpenSSL release.")
set(CPKT_ZLIB_VERSION "1.3.2" CACHE STRING "Pinned zlib release.")
set(CPKT_CURL_VERSION "8.20.0" CACHE STRING "Pinned curl release.")
set(CPKT_NGHTTP2_VERSION "1.69.0" CACHE STRING "Pinned nghttp2 release.")
set(CPKT_LIBSSH2_VERSION "1.11.1" CACHE STRING "Pinned libssh2 release.")
set(CPKT_CMOCKA_VERSION "2.0.2" CACHE STRING "Pinned cmocka release.")
set(CPKT_LIBXML2_VERSION "2.15.3" CACHE STRING "Pinned libxml2 release.")
set(CPKT_LUA_VERSION "5.5.0" CACHE STRING "Pinned Lua release.")
set(CPKT_MINIAUDIO_VERSION "0.11.25" CACHE STRING "Pinned miniaudio release.")
set(CPKT_WHISPER_VERSION "v1.9.1" CACHE STRING "Pinned whisper.cpp release.")
set(CPKT_MQTTC_VERSION "1.1.2" CACHE STRING "Pinned MQTT-C project version.")
set(CPKT_MQTTC_COMMIT "0f4c34c8cc00b16cfee094745d68b8cdbaecd8e0" CACHE STRING "Pinned MQTT-C commit used by open62541 v1.5.4.")
set(CPKT_OPEN62541_VERSION "1.5.4" CACHE STRING "Pinned open62541 release.")
set(CPKT_OPEN62541_PATCHSET "mqtt-prefix-musl-warning-1" CACHE STRING "c.pkt.systems open62541 patchset identity." FORCE)
set(CPKT_LUA_RUNTIME_ABI_VERSION "0" CACHE STRING "Lua runtime facade ABI major. Do not bump without maintainer approval.")
set(CPKT_AUDIO_ABI_VERSION "0" CACHE STRING "Audio facade ABI major. Do not bump without maintainer approval.")
set(CPKT_SUS_ABI_VERSION "0" CACHE STRING "Susurro/whisper facade ABI major. Do not bump without maintainer approval.")
set(CPKT_OPCUA_ABI_VERSION "0" CACHE STRING "OPC UA facade ABI major. Do not bump without maintainer approval.")
if(NOT CPKT_SUS_CPU_ONLY)
  message(FATAL_ERROR "CPKT_SUS_CPU_ONLY=OFF is reserved for a later GPU-backend packaging phase")
endif()
set(CPKT_SUS_BACKEND_CAPABILITIES "cpu" CACHE INTERNAL "Compiled cpkt_sus whisper.cpp backend capability list.")
set(CPKT_CXX_STDLIB_STATIC_LIBRARY "" CACHE FILEPATH "Static C++ standard library archive used by C-facing static SDK metadata.")
if(CPKT_CXX_STDLIB_STATIC_LIBRARY STREQUAL "" AND CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
  execute_process(
    COMMAND "${CMAKE_CXX_COMPILER}" -print-file-name=libstdc++.a
    RESULT_VARIABLE _cpkt_cxx_stdlib_result
    OUTPUT_VARIABLE _cpkt_cxx_stdlib_output
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(_cpkt_cxx_stdlib_result EQUAL 0 AND
      NOT _cpkt_cxx_stdlib_output STREQUAL "" AND
      NOT _cpkt_cxx_stdlib_output STREQUAL "libstdc++.a" AND
      EXISTS "${_cpkt_cxx_stdlib_output}")
    set(CPKT_CXX_STDLIB_STATIC_LIBRARY "${_cpkt_cxx_stdlib_output}" CACHE FILEPATH
      "Static C++ standard library archive used by C-facing static SDK metadata." FORCE)
  endif()
endif()
set(CPKT_CXX_LIBGCC_STATIC_LIBRARY "" CACHE FILEPATH "Static GCC runtime archive used by C-facing static SDK metadata.")
if(CPKT_CXX_LIBGCC_STATIC_LIBRARY STREQUAL "" AND CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
  execute_process(
    COMMAND "${CMAKE_CXX_COMPILER}" -print-libgcc-file-name
    RESULT_VARIABLE _cpkt_cxx_libgcc_result
    OUTPUT_VARIABLE _cpkt_cxx_libgcc_output
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(_cpkt_cxx_libgcc_result EQUAL 0 AND
      NOT _cpkt_cxx_libgcc_output STREQUAL "" AND
      NOT _cpkt_cxx_libgcc_output STREQUAL "libgcc.a" AND
      EXISTS "${_cpkt_cxx_libgcc_output}")
    set(CPKT_CXX_LIBGCC_STATIC_LIBRARY "${_cpkt_cxx_libgcc_output}" CACHE FILEPATH
      "Static GCC runtime archive used by C-facing static SDK metadata." FORCE)
  endif()
endif()
if(CPKT_BUILD_DEPENDENCIES)
  if(NOT DEFINED CPKT_DEPENDENCY_CACHE OR "${CPKT_DEPENDENCY_CACHE}" STREQUAL "")
    if(DEFINED ENV{CPKT_DEPENDENCY_CACHE} AND NOT "$ENV{CPKT_DEPENDENCY_CACHE}" STREQUAL "")
      set(_cpkt_dependency_cache_default "$ENV{CPKT_DEPENDENCY_CACHE}")
    elseif(DEFINED ENV{XDG_CACHE_HOME} AND NOT "$ENV{XDG_CACHE_HOME}" STREQUAL "")
      set(_cpkt_dependency_cache_default "$ENV{XDG_CACHE_HOME}/c.pkt.systems/deps")
    elseif(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
      set(_cpkt_dependency_cache_default "$ENV{HOME}/.cache/c.pkt.systems/deps")
    else()
      message(FATAL_ERROR "CPKT_DEPENDENCY_CACHE, XDG_CACHE_HOME, or HOME is required for the shared dependency archive cache")
    endif()
    set(CPKT_DEPENDENCY_CACHE "${_cpkt_dependency_cache_default}" CACHE PATH "Shared verified dependency archive cache." FORCE)
  endif()
endif()
unset(_cpkt_dependency_cache_default)
set(CPKT_DEPENDENCY_CACHE_LOCK_TIMEOUT "120" CACHE STRING "Maximum shared dependency archive cache lock wait in seconds.")
set(CPKT_DOWNLOAD_ROOT "${CMAKE_SOURCE_DIR}/.cache/downloads" CACHE PATH "Local ExternalProject archive staging root.")
set(CPKT_DIST_DIR "${CMAKE_SOURCE_DIR}/dist" CACHE PATH "Output directory for package archives.")
set(CPKT_TARGET_ARCH "${CMAKE_SYSTEM_PROCESSOR}" CACHE STRING "Artifact architecture name.")
set(CPKT_TARGET_OS "${CMAKE_SYSTEM_NAME}" CACHE STRING "Artifact operating system name.")
set(CPKT_TARGET_LIBC "" CACHE STRING "Artifact libc suffix for Linux targets.")
set(CPKT_DEPENDENCY_BUILD_TYPE "Release")
ProcessorCount(CPKT_DEPENDENCY_BUILD_JOBS_DEFAULT)
if(NOT CPKT_DEPENDENCY_BUILD_JOBS_DEFAULT OR CPKT_DEPENDENCY_BUILD_JOBS_DEFAULT LESS 1)
  set(CPKT_DEPENDENCY_BUILD_JOBS_DEFAULT 1)
endif()
set(CPKT_DEPENDENCY_BUILD_JOBS "${CPKT_DEPENDENCY_BUILD_JOBS_DEFAULT}" CACHE STRING "Parallel dependency build job count.")
set(CPKT_DEPENDENCY_DOWNLOAD_TIMEOUT "300" CACHE STRING "Maximum source archive download time in seconds.")
set(CPKT_DEPENDENCY_DOWNLOAD_INACTIVITY_TIMEOUT "60" CACHE STRING "Maximum source archive idle download time in seconds.")

string(TOLOWER "${CPKT_TARGET_OS}" CPKT_TARGET_OS_LOWER)
cpkt_normalize_arch("${CPKT_TARGET_ARCH}" CPKT_TARGET_ARCH_LOWER)
if(CPKT_TARGET_OS_LOWER STREQUAL "darwin" AND CPKT_TARGET_ARCH_LOWER STREQUAL "aarch64")
  set(CPKT_TARGET_ARCH_LOWER "arm64")
endif()
set(CPKT_TARGET_ARCH "${CPKT_TARGET_ARCH_LOWER}" CACHE STRING "Artifact architecture name." FORCE)

if(CPKT_TARGET_OS_LOWER STREQUAL "linux" AND NOT CPKT_TARGET_LIBC)
  execute_process(
    COMMAND "${CMAKE_C_COMPILER}" -dumpmachine
    OUTPUT_VARIABLE cpkt_compiler_machine
    RESULT_VARIABLE cpkt_compiler_machine_result
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  if(cpkt_compiler_machine_result EQUAL 0 AND cpkt_compiler_machine MATCHES "musl")
    set(CPKT_TARGET_LIBC "musl")
  else()
    set(CPKT_TARGET_LIBC "gnu")
  endif()
endif()

if(CPKT_TARGET_OS_LOWER STREQUAL "darwin")
  set(CPKT_TARGET_ID "${CPKT_TARGET_ARCH_LOWER}-apple-darwin")
elseif(CPKT_TARGET_OS_LOWER STREQUAL "linux")
  set(CPKT_TARGET_ID "${CPKT_TARGET_ARCH_LOWER}-linux-${CPKT_TARGET_LIBC}")
else()
  set(CPKT_TARGET_ID "${CPKT_TARGET_ARCH_LOWER}-${CPKT_TARGET_OS_LOWER}")
endif()

set(CPKT_DEPENDENCY_SET_ID
  "openssl-${CPKT_OPENSSL_VERSION}_zlib-${CPKT_ZLIB_VERSION}_curl-${CPKT_CURL_VERSION}_nghttp2-${CPKT_NGHTTP2_VERSION}_libssh2-${CPKT_LIBSSH2_VERSION}_libxml2-${CPKT_LIBXML2_VERSION}_lua-${CPKT_LUA_VERSION}_miniaudio-${CPKT_MINIAUDIO_VERSION}_whisper-${CPKT_WHISPER_VERSION}_mqtt-c-${CPKT_MQTTC_VERSION}-${CPKT_MQTTC_COMMIT}_open62541-${CPKT_OPEN62541_VERSION}-${CPKT_OPEN62541_PATCHSET}_cmocka-${CPKT_CMOCKA_VERSION}"
  CACHE INTERNAL "Version key for the pinned third-party dependency set.")
include(cmake/CpktDependencyToolchainIdentity.cmake)
cpkt_dependency_toolchain_cache_id(_cpkt_dependency_toolchain_id)
set(_cpkt_legacy_external_root "${CMAKE_SOURCE_DIR}/.cache/deps/${CPKT_TARGET_ID}")
set(_cpkt_legacy_dependency_build_root "${CMAKE_SOURCE_DIR}/.cache/deps-build/${CPKT_TARGET_ID}")
set(_cpkt_default_external_root
  "${_cpkt_legacy_external_root}/${CPKT_DEPENDENCY_SET_ID}/${_cpkt_dependency_toolchain_id}")
set(_cpkt_default_dependency_build_root
  "${_cpkt_legacy_dependency_build_root}/${CPKT_DEPENDENCY_SET_ID}/${_cpkt_dependency_toolchain_id}")

if(DEFINED CPKT_EXTERNAL_ROOT)
  string(FIND "${CPKT_EXTERNAL_ROOT}" "${_cpkt_legacy_external_root}/" _cpkt_external_root_under_legacy)
else()
  set(_cpkt_external_root_under_legacy -1)
endif()
if(NOT DEFINED CPKT_EXTERNAL_ROOT
    OR "${CPKT_EXTERNAL_ROOT}" STREQUAL "${_cpkt_legacy_external_root}"
    OR (NOT CPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE
      AND _cpkt_external_root_under_legacy EQUAL 0
      AND NOT "${CPKT_EXTERNAL_ROOT}" STREQUAL "${_cpkt_default_external_root}"))
  set(CPKT_EXTERNAL_ROOT "${_cpkt_default_external_root}" CACHE PATH "Version-keyed install root for dependency artifacts." FORCE)
endif()
if(DEFINED CPKT_DEPENDENCY_BUILD_ROOT)
  string(FIND "${CPKT_DEPENDENCY_BUILD_ROOT}" "${_cpkt_legacy_dependency_build_root}/" _cpkt_dependency_build_root_under_legacy)
else()
  set(_cpkt_dependency_build_root_under_legacy -1)
endif()
if(NOT DEFINED CPKT_DEPENDENCY_BUILD_ROOT
    OR "${CPKT_DEPENDENCY_BUILD_ROOT}" STREQUAL "${_cpkt_legacy_dependency_build_root}"
    OR (NOT CPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE
      AND _cpkt_dependency_build_root_under_legacy EQUAL 0
      AND NOT "${CPKT_DEPENDENCY_BUILD_ROOT}" STREQUAL "${_cpkt_default_dependency_build_root}"))
  set(CPKT_DEPENDENCY_BUILD_ROOT "${_cpkt_default_dependency_build_root}" CACHE PATH "Version-keyed build root for dependency artifacts." FORCE)
endif()

include(cmake/CpktDependencies.cmake)
cpkt_configure_dependencies()

add_library(cpkt_lua_runtime_static STATIC src/lua_runtime.c)
target_compile_features(cpkt_lua_runtime_static PRIVATE c_std_99)
cpkt_add_repo_warning_errors(cpkt_lua_runtime_static)
target_include_directories(cpkt_lua_runtime_static
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>"
)
target_link_libraries(cpkt_lua_runtime_static PUBLIC cpkt::lua_static)
set_target_properties(cpkt_lua_runtime_static PROPERTIES
  OUTPUT_NAME cpkt_lua_runtime
  POSITION_INDEPENDENT_CODE ON
)

add_library(cpkt_lua_runtime_shared SHARED src/lua_runtime.c)
target_compile_features(cpkt_lua_runtime_shared PRIVATE c_std_99)
cpkt_add_repo_warning_errors(cpkt_lua_runtime_shared)
target_include_directories(cpkt_lua_runtime_shared
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>"
)
target_link_libraries(cpkt_lua_runtime_shared PUBLIC cpkt::lua_shared)
set_target_properties(cpkt_lua_runtime_shared PROPERTIES
  OUTPUT_NAME cpkt_lua_runtime
  POSITION_INDEPENDENT_CODE ON
  SOVERSION "${CPKT_LUA_RUNTIME_ABI_VERSION}"
  VERSION "${PROJECT_VERSION}"
)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set_target_properties(cpkt_lua_runtime_shared PROPERTIES
    INSTALL_NAME_DIR "@rpath"
    BUILD_RPATH "@loader_path"
    INSTALL_RPATH "@loader_path"
    BUILD_WITH_INSTALL_RPATH ON
  )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set_target_properties(cpkt_lua_runtime_shared PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH ON
  )
endif()
if(CPKT_BUILD_DEPENDENCIES)
  add_dependencies(cpkt_lua_runtime_static cpkt_deps)
  add_dependencies(cpkt_lua_runtime_shared cpkt_deps)
endif()

add_library(cpkt_audio_static STATIC src/audio.c)
target_compile_features(cpkt_audio_static PRIVATE c_std_99)
cpkt_add_repo_warning_errors(cpkt_audio_static)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  target_compile_options(cpkt_audio_static PRIVATE
    -Wno-unused-function
    -ffunction-sections
    -fdata-sections)
endif()
target_include_directories(cpkt_audio_static
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>"
  PRIVATE
    "$<TARGET_PROPERTY:cpkt::miniaudio_static,INTERFACE_INCLUDE_DIRECTORIES>"
)
target_compile_definitions(cpkt_audio_static PRIVATE
  CPKT_AUDIO_NATIVE_RUNTIME_DEVICE_IO)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_compile_definitions(cpkt_audio_static PRIVATE
    CPKT_AUDIO_AUTO_PROCESS_DEVICE_IO)
endif()
target_link_libraries(cpkt_audio_static PUBLIC
  cpkt::curl_static
  m
  ${CMAKE_DL_LIBS}
  Threads::Threads
)
set_target_properties(cpkt_audio_static PROPERTIES
  OUTPUT_NAME cpktaudio
  POSITION_INDEPENDENT_CODE ON
)

add_library(cpkt_audio_shared SHARED src/audio.c)
target_compile_features(cpkt_audio_shared PRIVATE c_std_99)
cpkt_add_repo_warning_errors(cpkt_audio_shared)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  target_compile_options(cpkt_audio_shared PRIVATE -Wno-unused-function)
endif()
target_include_directories(cpkt_audio_shared
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>"
  PRIVATE
    "$<TARGET_PROPERTY:cpkt::miniaudio_shared,INTERFACE_INCLUDE_DIRECTORIES>"
)
target_compile_definitions(cpkt_audio_shared PRIVATE
  CPKT_AUDIO_NATIVE_RUNTIME_DEVICE_IO)
target_link_libraries(cpkt_audio_shared PUBLIC
  cpkt::curl_shared
  m
  ${CMAKE_DL_LIBS}
  Threads::Threads
)
set_target_properties(cpkt_audio_shared PROPERTIES
  OUTPUT_NAME cpktaudio
  C_VISIBILITY_PRESET hidden
  POSITION_INDEPENDENT_CODE ON
  SOVERSION "${CPKT_AUDIO_ABI_VERSION}"
  VERSION "${PROJECT_VERSION}"
)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set_target_properties(cpkt_audio_shared PROPERTIES
    INSTALL_NAME_DIR "@rpath"
    BUILD_RPATH "@loader_path"
    INSTALL_RPATH "@loader_path"
    BUILD_WITH_INSTALL_RPATH ON
  )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set_target_properties(cpkt_audio_shared PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH ON
  )
endif()
if(CPKT_BUILD_DEPENDENCIES)
  add_dependencies(cpkt_audio_static cpkt_deps)
  add_dependencies(cpkt_audio_shared cpkt_deps)
endif()

add_library(cpkt_sus_static STATIC src/sus.c)
target_compile_features(cpkt_sus_static PRIVATE c_std_99)
cpkt_add_repo_warning_errors(cpkt_sus_static)
target_include_directories(cpkt_sus_static
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>"
)
target_link_libraries(cpkt_sus_static PUBLIC
  cpkt_audio_static
  cpkt::whisper_static
  cpkt::curl_static
  cpkt::openssl_crypto_static
)
target_compile_definitions(cpkt_sus_static PRIVATE
  CPKT_SUS_FACADE_VERSION="${CPKT_SUS_ABI_VERSION}"
  CPKT_SUS_BACKEND_CAPABILITIES="${CPKT_SUS_BACKEND_CAPABILITIES}")
set_target_properties(cpkt_sus_static PROPERTIES
  OUTPUT_NAME cpktsus
  POSITION_INDEPENDENT_CODE ON
)

add_library(cpkt_sus_shared SHARED src/sus.c)
target_compile_features(cpkt_sus_shared PRIVATE c_std_99)
cpkt_add_repo_warning_errors(cpkt_sus_shared)
target_include_directories(cpkt_sus_shared
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>"
)
target_link_libraries(cpkt_sus_shared PUBLIC
  cpkt_audio_shared
  cpkt::whisper_shared
  cpkt::curl_shared
  cpkt::openssl_crypto_shared
)
target_compile_definitions(cpkt_sus_shared PRIVATE
  CPKT_SUS_FACADE_VERSION="${CPKT_SUS_ABI_VERSION}"
  CPKT_SUS_BACKEND_CAPABILITIES="${CPKT_SUS_BACKEND_CAPABILITIES}")
set_target_properties(cpkt_sus_shared PROPERTIES
  OUTPUT_NAME cpktsus
  POSITION_INDEPENDENT_CODE ON
  SOVERSION "${CPKT_SUS_ABI_VERSION}"
  VERSION "${PROJECT_VERSION}"
)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set_target_properties(cpkt_sus_shared PROPERTIES
    INSTALL_NAME_DIR "@rpath"
    BUILD_RPATH "@loader_path"
    INSTALL_RPATH "@loader_path"
    BUILD_WITH_INSTALL_RPATH ON
  )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set_target_properties(cpkt_sus_shared PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH ON
  )
endif()
if(CPKT_BUILD_DEPENDENCIES)
  add_dependencies(cpkt_sus_static cpkt_deps)
  add_dependencies(cpkt_sus_shared cpkt_deps)
endif()

if(CPKT_CAN_RUN_TARGET_EXECUTABLES)
  add_executable(cpktxscribe tools/cpktxscribe.c)
  target_compile_options(cpktxscribe PRIVATE -std=c89)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(cpktxscribe PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Werror
      "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=src"
      "-ffile-prefix-map=${CMAKE_BINARY_DIR}=build"
      "-fdebug-prefix-map=${CMAKE_SOURCE_DIR}=src"
      "-fdebug-prefix-map=${CMAKE_BINARY_DIR}=build")
  elseif(MSVC)
    target_compile_options(cpktxscribe PRIVATE /W4 /WX)
  endif()
  target_link_libraries(cpktxscribe PRIVATE cpkt_sus_static)
  set_target_properties(cpktxscribe PROPERTIES
    LINKER_LANGUAGE CXX
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(cpktxscribe PRIVATE -Wl,--gc-sections)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
      target_link_options(cpktxscribe PRIVATE -static-libstdc++ -static-libgcc)
    endif()
  endif()
  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpktxscribe cpkt_deps)
  endif()
  if(CPKT_BUILD_TESTS)
    add_test(
      NAME cpktxscribe_cache_status
      COMMAND ${CMAKE_SOURCE_DIR}/tests/cpktxscribe_cache_status_test.sh
        ${CPKT_TEST_EXECUTABLE_PREFIX}
        $<TARGET_FILE:cpktxscribe>)
    add_test(
      NAME cpktxscribe_link_policy
      COMMAND sh ${CMAKE_SOURCE_DIR}/tests/cpktxscribe_link_policy_test.sh
        $<TARGET_FILE:cpktxscribe>)
  endif()
endif()

add_library(cpkt_opcua_static STATIC src/opcua.c)
target_compile_features(cpkt_opcua_static PRIVATE c_std_99)
cpkt_add_repo_warning_errors(cpkt_opcua_static)
target_include_directories(cpkt_opcua_static
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>"
)
target_link_libraries(cpkt_opcua_static PUBLIC cpkt::open62541_static)
target_compile_definitions(cpkt_opcua_static PRIVATE CPKT_OPCUA_FACADE_VERSION="${CPKT_OPCUA_ABI_VERSION}")
set_target_properties(cpkt_opcua_static PROPERTIES
  OUTPUT_NAME cpkt_opcua
  POSITION_INDEPENDENT_CODE ON
)

add_library(cpkt_opcua_shared SHARED src/opcua.c)
target_compile_features(cpkt_opcua_shared PRIVATE c_std_99)
cpkt_add_repo_warning_errors(cpkt_opcua_shared)
target_include_directories(cpkt_opcua_shared
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>"
)
target_link_libraries(cpkt_opcua_shared PUBLIC cpkt::open62541_shared)
target_compile_definitions(cpkt_opcua_shared PRIVATE CPKT_OPCUA_FACADE_VERSION="${CPKT_OPCUA_ABI_VERSION}")
set_target_properties(cpkt_opcua_shared PROPERTIES
  OUTPUT_NAME cpkt_opcua
  POSITION_INDEPENDENT_CODE ON
  SOVERSION "${CPKT_OPCUA_ABI_VERSION}"
  VERSION "${PROJECT_VERSION}"
)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set_target_properties(cpkt_opcua_shared PROPERTIES
    INSTALL_NAME_DIR "@rpath"
    BUILD_RPATH "@loader_path"
    INSTALL_RPATH "@loader_path"
    BUILD_WITH_INSTALL_RPATH ON
  )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set_target_properties(cpkt_opcua_shared PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH ON
  )
endif()
if(CPKT_BUILD_DEPENDENCIES)
  add_dependencies(cpkt_opcua_static cpkt_deps)
  add_dependencies(cpkt_opcua_shared cpkt_deps)
endif()

if(CPKT_BUILD_TESTS)
  set(_cpkt_lua_runtime_example_file "${CMAKE_CURRENT_BINARY_DIR}/lua-runtime-c89-example-file.lua")
  file(WRITE "${_cpkt_lua_runtime_example_file}"
    "local host = require('example_host')\n"
    "local chunk = require('example_chunk')\n"
    "if host.context ~= 'example-context' then error('bad file context') end\n"
    "if chunk.value ~= 'chunk' then error('bad file chunk') end\n"
    "if package.path ~= 'zero/?.lua;first/?.lua' then error('bad package path') end\n"
    "if package.cpath ~= 'zero/?.so;first/?.so' then error('bad package cpath') end\n"
    "if example_name ~= 'facade' then error('bad string global') end\n"
    "if example_flag ~= true then error('bad boolean global') end\n"
    "if example_count ~= 7 then error('bad integer global') end\n"
    "if example_number ~= 2.5 then error('bad number global') end\n"
    "if required_side_effect ~= 'ok' then error('bad require side effect') end\n"
    "if arg[0] ~= '${_cpkt_lua_runtime_example_file}' then error('bad arg0') end\n"
    "if arg[1] ~= 'one' or arg[2] ~= 'two' then error('bad argv') end\n")

  add_executable(cpkt_lua_runtime_c89_example
    examples/lua-runtime-c89/main.c
    examples/lua-runtime-c89/host_module.c)
  cpkt_add_repo_warning_errors(cpkt_lua_runtime_c89_example)
  set_source_files_properties(examples/lua-runtime-c89/main.c PROPERTIES
    COMPILE_OPTIONS "-std=c89")
  set_source_files_properties(examples/lua-runtime-c89/host_module.c PROPERTIES
    COMPILE_OPTIONS "-std=c99")
  target_link_libraries(cpkt_lua_runtime_c89_example PRIVATE cpkt_lua_runtime_static)
  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt_lua_runtime_c89_example cpkt_deps)
  endif()

  add_executable(cpkt_opcua_c89_example examples/opcua-c89/main.c)
  target_compile_options(cpkt_opcua_c89_example PRIVATE -std=c89)
  cpkt_add_repo_warning_errors(cpkt_opcua_c89_example)
  target_link_libraries(cpkt_opcua_c89_example PRIVATE cpkt_opcua_static)
  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt_opcua_c89_example cpkt_deps)
  endif()

  add_executable(cpkt_audio_sus_c89_example examples/audio-sus-c89/main.c)
  target_compile_options(cpkt_audio_sus_c89_example PRIVATE -std=c89)
  cpkt_add_repo_warning_errors(cpkt_audio_sus_c89_example)
  target_link_libraries(cpkt_audio_sus_c89_example PRIVATE
    cpkt_audio_static
    cpkt_sus_shared)
  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt_audio_sus_c89_example cpkt_deps)
  endif()

  add_executable(cpkt_audio_vox_intro_c89_example
    examples/audio-vox-intro-c89/main.c)
  target_compile_options(cpkt_audio_vox_intro_c89_example PRIVATE -std=c89)
  cpkt_add_repo_warning_errors(cpkt_audio_vox_intro_c89_example)
  target_link_libraries(cpkt_audio_vox_intro_c89_example PRIVATE
    cpkt_audio_static)
  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt_audio_vox_intro_c89_example cpkt_deps)
  endif()

  add_executable(cpkt_audio_live_vox_c89_example
    examples/audio-live-vox-c89/main.c)
  target_compile_options(cpkt_audio_live_vox_c89_example PRIVATE -std=c89)
  cpkt_add_repo_warning_errors(cpkt_audio_live_vox_c89_example)
  target_link_libraries(cpkt_audio_live_vox_c89_example PRIVATE
    cpkt_audio_shared)
  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt_audio_live_vox_c89_example cpkt_deps)
  endif()

  if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CPKT_TARGET_LIBC STREQUAL "musl")
    add_executable(cpkt_audio_live_vox_static_c89_example
      examples/audio-live-vox-c89/main.c)
    target_compile_options(cpkt_audio_live_vox_static_c89_example PRIVATE
      -std=c89)
    cpkt_add_repo_warning_errors(cpkt_audio_live_vox_static_c89_example)
    target_link_libraries(cpkt_audio_live_vox_static_c89_example PRIVATE
      cpkt_audio_static)
    target_link_options(cpkt_audio_live_vox_static_c89_example PRIVATE
      -static -Wl,--gc-sections)
    if(CPKT_BUILD_DEPENDENCIES)
      add_dependencies(cpkt_audio_live_vox_static_c89_example cpkt_deps)
    endif()
  else()
    file(REMOVE
      "${CMAKE_CURRENT_BINARY_DIR}/cpkt_audio_live_vox_static_c89_example${CMAKE_EXECUTABLE_SUFFIX}")
  endif()

  add_executable(cpkt_sus_vox_intro_c89_example
    examples/sus-vox-intro-c89/main.c)
  target_compile_options(cpkt_sus_vox_intro_c89_example PRIVATE -std=c89)
  cpkt_add_repo_warning_errors(cpkt_sus_vox_intro_c89_example)
  target_link_libraries(cpkt_sus_vox_intro_c89_example PRIVATE
    cpkt_audio_static
    cpkt_sus_shared)
  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt_sus_vox_intro_c89_example cpkt_deps)
  endif()

  add_executable(cpkt_sus_live_vox_c89_example
    examples/sus-live-vox-c89/main.c)
  target_compile_options(cpkt_sus_live_vox_c89_example PRIVATE -std=c89)
  cpkt_add_repo_warning_errors(cpkt_sus_live_vox_c89_example)
  target_link_libraries(cpkt_sus_live_vox_c89_example PRIVATE
    cpkt_audio_shared
    cpkt_sus_shared)
  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt_sus_live_vox_c89_example cpkt_deps)
  endif()

  if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CPKT_TARGET_LIBC STREQUAL "musl")
    add_executable(cpkt_sus_live_vox_static_c89_example
      examples/sus-live-vox-c89/main.c)
    target_compile_options(cpkt_sus_live_vox_static_c89_example PRIVATE
      -std=c89)
    cpkt_add_repo_warning_errors(cpkt_sus_live_vox_static_c89_example)
    set_target_properties(cpkt_sus_live_vox_static_c89_example PROPERTIES
      LINKER_LANGUAGE CXX)
    target_link_libraries(cpkt_sus_live_vox_static_c89_example PRIVATE
      cpkt_audio_static
      cpkt_sus_static)
    target_link_options(cpkt_sus_live_vox_static_c89_example PRIVATE
      -static -Wl,--gc-sections)
    if(CPKT_BUILD_DEPENDENCIES)
      add_dependencies(cpkt_sus_live_vox_static_c89_example cpkt_deps)
    endif()
  else()
    file(REMOVE
      "${CMAKE_CURRENT_BINARY_DIR}/cpkt_sus_live_vox_static_c89_example${CMAKE_EXECUTABLE_SUFFIX}")
  endif()

  add_executable(cpkt_abi_smoke_shared examples/abi_smoke.c)
  target_compile_options(cpkt_abi_smoke_shared PRIVATE -std=c99)
  target_link_libraries(cpkt_abi_smoke_shared PRIVATE
    cpkt::curl_shared
    cpkt::libxml2_shared
    cpkt::lua_shared
    cpkt::miniaudio_shared
    cpkt::open62541_shared
  )

  add_executable(cpkt_abi_smoke_static examples/abi_smoke.c)
  target_compile_options(cpkt_abi_smoke_static PRIVATE -std=c99)
  target_link_libraries(cpkt_abi_smoke_static PRIVATE
    cpkt::curl_static
    cpkt::libxml2_static
    cpkt::lua_static
    cpkt::miniaudio_static
    cpkt::open62541_static
  )
  add_executable(cpkt_mqttc_smoke_shared examples/mqttc_smoke.c)
  target_compile_options(cpkt_mqttc_smoke_shared PRIVATE -std=c99)
  target_link_libraries(cpkt_mqttc_smoke_shared PRIVATE cpkt::mqttc_shared)

  add_executable(cpkt_mqttc_smoke_static examples/mqttc_smoke.c)
  target_compile_options(cpkt_mqttc_smoke_static PRIVATE -std=c99)
  target_link_libraries(cpkt_mqttc_smoke_static PRIVATE cpkt::mqttc_static)
  add_executable(cpkt_whisper_smoke_shared examples/whisper_smoke.c)
  target_compile_options(cpkt_whisper_smoke_shared PRIVATE -std=c99)
  target_link_libraries(cpkt_whisper_smoke_shared PRIVATE cpkt::whisper_shared)
  add_library(cpkt_static_archive_pic_smoke_core SHARED examples/abi_smoke.c)
  target_compile_options(cpkt_static_archive_pic_smoke_core PRIVATE -std=c99)
  target_link_libraries(cpkt_static_archive_pic_smoke_core PRIVATE
    cpkt::curl_static
    cpkt::libxml2_static
    cpkt::lua_static
    cpkt::miniaudio_static
    cpkt::open62541_static
  )
  add_library(cpkt_static_archive_pic_smoke_mqttc SHARED examples/mqttc_smoke.c)
  target_compile_options(cpkt_static_archive_pic_smoke_mqttc PRIVATE -std=c99)
  target_link_libraries(cpkt_static_archive_pic_smoke_mqttc PRIVATE cpkt::mqttc_static)
  add_library(cpkt_static_archive_pic_smoke_whisper SHARED examples/whisper_smoke.c)
  target_compile_options(cpkt_static_archive_pic_smoke_whisper PRIVATE -std=c99)
  target_link_libraries(cpkt_static_archive_pic_smoke_whisper PRIVATE cpkt::whisper_static)
  set_target_properties(cpkt_static_archive_pic_smoke_whisper PROPERTIES
    LINKER_LANGUAGE CXX)
  add_library(cpkt_static_archive_pic_smoke_facades SHARED
    tests/static_archive_pic_smoke.c)
  target_compile_options(cpkt_static_archive_pic_smoke_facades PRIVATE -std=c99)
  target_include_directories(cpkt_static_archive_pic_smoke_facades PRIVATE
    "${CMAKE_SOURCE_DIR}/include")
  target_link_libraries(cpkt_static_archive_pic_smoke_facades PRIVATE
    cpkt_lua_runtime_static
    cpkt_audio_static
    cpkt_sus_static
    cpkt_opcua_static)
  set_target_properties(cpkt_static_archive_pic_smoke_facades PROPERTIES
    LINKER_LANGUAGE CXX)
  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt_abi_smoke_shared cpkt_deps)
    add_dependencies(cpkt_abi_smoke_static cpkt_deps)
    add_dependencies(cpkt_mqttc_smoke_shared cpkt_deps)
    add_dependencies(cpkt_mqttc_smoke_static cpkt_deps)
    add_dependencies(cpkt_whisper_smoke_shared cpkt_deps)
    add_dependencies(cpkt_static_archive_pic_smoke_core cpkt_deps)
    add_dependencies(cpkt_static_archive_pic_smoke_mqttc cpkt_deps)
    add_dependencies(cpkt_static_archive_pic_smoke_whisper cpkt_deps)
    add_dependencies(cpkt_static_archive_pic_smoke_facades cpkt_deps)
  endif()

  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set_target_properties(cpkt_abi_smoke_shared PROPERTIES
      BUILD_RPATH "@executable_path/../lib"
    )
    set(_darwin_smoke_stage "${CMAKE_CURRENT_BINARY_DIR}/darwin-smoke-test")
    set(_darwin_smoke_zip "${CPKT_DIST_DIR}/c.pkt.systems-${CPKT_BUNDLE_VERSION}-${CPKT_TARGET_ID}-smoke-test.zip")
    add_custom_command(
      OUTPUT "${_darwin_smoke_zip}"
      COMMAND "${CMAKE_COMMAND}" -E rm -rf "${_darwin_smoke_stage}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${_darwin_smoke_stage}/bin" "${_darwin_smoke_stage}/lib" "${CPKT_DIST_DIR}"
      COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:cpkt_abi_smoke_shared>" "${_darwin_smoke_stage}/bin/"
      COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:cpkt_abi_smoke_static>" "${_darwin_smoke_stage}/bin/"
      COMMAND "${CMAKE_COMMAND}"
        -DCPKT_DARWIN_SMOKE_BINARY=$<TARGET_FILE:cpkt_abi_smoke_shared>
        -DCPKT_DARWIN_SMOKE_LIB_DIR=${_darwin_smoke_stage}/lib
        -DCPKT_EXTERNAL_ROOT=${CPKT_EXTERNAL_ROOT}
        -DCPKT_OTOOL=${CPKT_OTOOL}
        -P "${CMAKE_SOURCE_DIR}/cmake/package_darwin_smoke_libraries.cmake"
      COMMAND "${CMAKE_COMMAND}" -E tar cf "${_darwin_smoke_zip}" --format=zip darwin-smoke-test
      COMMAND "${CMAKE_COMMAND}"
        -DCPKT_CHECKSUMS_PATH=${CPKT_DIST_DIR}/c.pkt.systems-${CPKT_BUNDLE_VERSION}-CHECKSUMS
        -DCPKT_ARTIFACT_PATH=${_darwin_smoke_zip}
        -P "${CMAKE_SOURCE_DIR}/cmake/update_checksum_manifest.cmake"
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
      DEPENDS
        cpkt_abi_smoke_shared
        cpkt_abi_smoke_static
        cpkt_mqttc_smoke_shared
        cpkt_mqttc_smoke_static
        "${CMAKE_SOURCE_DIR}/cmake/package_darwin_smoke_libraries.cmake"
        "${CMAKE_SOURCE_DIR}/cmake/update_checksum_manifest.cmake"
      VERBATIM
    )
    add_custom_target(package-darwin-smoke-test DEPENDS "${_darwin_smoke_zip}")
  else()
    add_executable(cpkt_abi_test_shared tests/abi_smoke_test.c)
    target_link_libraries(cpkt_abi_test_shared PRIVATE
      cpkt::curl_shared
      cpkt::libxml2_shared
      cpkt::lua_shared
      cpkt::miniaudio_shared
      cpkt::open62541_shared
      cpkt::cmocka
    )

    add_executable(cpkt_abi_test_static tests/abi_smoke_test.c)
    target_link_libraries(cpkt_abi_test_static PRIVATE
      cpkt::curl_static
      cpkt::libxml2_static
      cpkt::lua_static
      cpkt::miniaudio_static
      cpkt::open62541_static
      cpkt::cmocka
    )
    if(CPKT_BUILD_DEPENDENCIES)
      add_dependencies(cpkt_abi_test_shared cpkt_deps)
      add_dependencies(cpkt_abi_test_static cpkt_deps)
    endif()

    add_executable(cpkt_audio_facade_test tests/audio_facade_test.c)
    target_compile_options(cpkt_audio_facade_test PRIVATE -std=c99)
    cpkt_add_repo_warning_errors(cpkt_audio_facade_test)
    target_link_libraries(cpkt_audio_facade_test PRIVATE
      cpkt_audio_static
      cpkt::cmocka
    )
    add_executable(cpkt_audio_process_capture_probe
      tests/audio_process_capture_probe.c)
    target_compile_options(cpkt_audio_process_capture_probe PRIVATE -std=c99)
    cpkt_add_repo_warning_errors(cpkt_audio_process_capture_probe)
    target_link_libraries(cpkt_audio_process_capture_probe PRIVATE
      cpkt_audio_static)
    add_executable(cpkt_audio_process_playback_probe
      tests/audio_process_playback_probe.c)
    target_compile_options(cpkt_audio_process_playback_probe PRIVATE -std=c99)
    cpkt_add_repo_warning_errors(cpkt_audio_process_playback_probe)
    target_link_libraries(cpkt_audio_process_playback_probe PRIVATE
      cpkt_audio_static)
    add_executable(cpkt_sus_facade_test tests/sus_facade_test.c)
    target_compile_options(cpkt_sus_facade_test PRIVATE -std=c99)
    cpkt_add_repo_warning_errors(cpkt_sus_facade_test)
    target_link_libraries(cpkt_sus_facade_test PRIVATE
      cpkt_sus_shared
      cpkt::cmocka
    )
    add_executable(cpkt_sus_audio_integration_test
      tests/sus_audio_integration_test.c)
    target_compile_options(cpkt_sus_audio_integration_test PRIVATE -std=c99)
    cpkt_add_repo_warning_errors(cpkt_sus_audio_integration_test)
    target_link_libraries(cpkt_sus_audio_integration_test PRIVATE
      cpkt_audio_static
      cpkt_sus_shared
    )
    if(CPKT_BUILD_DEPENDENCIES)
      add_dependencies(cpkt_audio_facade_test cpkt_deps)
      add_dependencies(cpkt_audio_process_capture_probe cpkt_deps)
      add_dependencies(cpkt_audio_process_playback_probe cpkt_deps)
      add_dependencies(cpkt_sus_facade_test cpkt_deps)
      add_dependencies(cpkt_sus_audio_integration_test cpkt_deps)
    endif()

    add_executable(cpkt_lua_runtime_test tests/lua_runtime_test.c)
    target_compile_options(cpkt_lua_runtime_test PRIVATE -std=c99)
    cpkt_add_repo_warning_errors(cpkt_lua_runtime_test)
    target_link_libraries(cpkt_lua_runtime_test PRIVATE
      cpkt_lua_runtime_static
      cpkt::cmocka
    )

    add_executable(cpkt_opcua_facade_test
      tests/opcua_facade_test.c
      tests/opcua_c89_boundary_peer.c)
    cpkt_add_repo_warning_errors(cpkt_opcua_facade_test)
    set_source_files_properties(tests/opcua_facade_test.c PROPERTIES
      COMPILE_OPTIONS "-std=c99")
    set_source_files_properties(tests/opcua_c89_boundary_peer.c PROPERTIES
      COMPILE_OPTIONS "-std=c89")
    target_link_libraries(cpkt_opcua_facade_test PRIVATE
      cpkt_opcua_static
      cpkt::open62541_static
      cpkt::cmocka
      Threads::Threads
    )

    cpkt_add_lua_runtime_mock_test(cpkt_lua_runtime_mock_test tests/lua_runtime_mock_test.c)
    if(CPKT_BUILD_DEPENDENCIES)
      add_dependencies(cpkt_lua_runtime_test cpkt_deps)
      add_dependencies(cpkt_opcua_facade_test cpkt_deps)
    endif()
  endif()

  if(CPKT_CAN_RUN_TARGET_EXECUTABLES)
    add_test(
      NAME lua_runtime_c89_example
      COMMAND cpkt_lua_runtime_c89_example "${_cpkt_lua_runtime_example_file}")
    add_test(NAME opcua_c89_example COMMAND cpkt_opcua_c89_example)
    add_test(NAME audio_sus_c89_example COMMAND cpkt_audio_sus_c89_example)
    add_test(NAME audio_vox_intro_c89_example
      COMMAND cpkt_audio_vox_intro_c89_example)
    add_test(NAME audio_live_vox_c89_example
      COMMAND cpkt_audio_live_vox_c89_example --smoke)
    add_test(NAME sus_vox_intro_c89_example
      COMMAND cpkt_sus_vox_intro_c89_example)
    add_test(NAME sus_live_vox_c89_example
      COMMAND cpkt_sus_live_vox_c89_example --smoke)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CPKT_TARGET_LIBC STREQUAL "musl")
      add_test(NAME audio_live_vox_static_c89_example
        COMMAND cpkt_audio_live_vox_static_c89_example --smoke)
      add_test(NAME sus_live_vox_static_c89_example
        COMMAND cpkt_sus_live_vox_static_c89_example --smoke)
      add_test(
        NAME audio_sus_static_live_examples_are_static
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/static_executable_policy_test.sh"
          "$<TARGET_FILE:cpkt_audio_live_vox_static_c89_example>"
          "$<TARGET_FILE:cpkt_sus_live_vox_static_c89_example>")
      set_tests_properties(sus_live_vox_static_c89_example PROPERTIES
        ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cpkt_sus_shared>:${CPKT_EXTERNAL_ROOT}/whisper/install/lib:${CPKT_EXTERNAL_ROOT}/curl/install/lib:${CPKT_EXTERNAL_ROOT}/openssl/install/lib:${CPKT_EXTERNAL_ROOT}/nghttp2/install/lib:${CPKT_EXTERNAL_ROOT}/libssh2/install/lib:${CPKT_EXTERNAL_ROOT}/zlib/install/lib")
    endif()
    add_test(
      NAME sus_live_startup_status
      COMMAND
        "${CMAKE_SOURCE_DIR}/tests/sus_live_startup_status_test.sh"
        ${CPKT_TEST_EXECUTABLE_PREFIX}
        "$<TARGET_FILE:cpkt_sus_live_vox_c89_example>")
    set_tests_properties(audio_live_vox_c89_example PROPERTIES
      ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cpkt_audio_shared>:${CPKT_EXTERNAL_ROOT}/curl/install/lib:${CPKT_EXTERNAL_ROOT}/openssl/install/lib:${CPKT_EXTERNAL_ROOT}/nghttp2/install/lib:${CPKT_EXTERNAL_ROOT}/libssh2/install/lib:${CPKT_EXTERNAL_ROOT}/zlib/install/lib")
    set_tests_properties(audio_sus_c89_example PROPERTIES
      ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cpkt_sus_shared>:${CPKT_EXTERNAL_ROOT}/whisper/install/lib:${CPKT_EXTERNAL_ROOT}/curl/install/lib:${CPKT_EXTERNAL_ROOT}/openssl/install/lib:${CPKT_EXTERNAL_ROOT}/nghttp2/install/lib:${CPKT_EXTERNAL_ROOT}/libssh2/install/lib:${CPKT_EXTERNAL_ROOT}/zlib/install/lib")
    set_tests_properties(sus_vox_intro_c89_example PROPERTIES
      ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cpkt_sus_shared>:${CPKT_EXTERNAL_ROOT}/whisper/install/lib:${CPKT_EXTERNAL_ROOT}/curl/install/lib:${CPKT_EXTERNAL_ROOT}/openssl/install/lib:${CPKT_EXTERNAL_ROOT}/nghttp2/install/lib:${CPKT_EXTERNAL_ROOT}/libssh2/install/lib:${CPKT_EXTERNAL_ROOT}/zlib/install/lib")
    set_tests_properties(sus_live_vox_c89_example PROPERTIES
      ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cpkt_sus_shared>:${CPKT_EXTERNAL_ROOT}/whisper/install/lib:${CPKT_EXTERNAL_ROOT}/curl/install/lib:${CPKT_EXTERNAL_ROOT}/openssl/install/lib:${CPKT_EXTERNAL_ROOT}/nghttp2/install/lib:${CPKT_EXTERNAL_ROOT}/libssh2/install/lib:${CPKT_EXTERNAL_ROOT}/zlib/install/lib")
    set_tests_properties(sus_live_startup_status PROPERTIES
      ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cpkt_sus_shared>:${CPKT_EXTERNAL_ROOT}/whisper/install/lib:${CPKT_EXTERNAL_ROOT}/curl/install/lib:${CPKT_EXTERNAL_ROOT}/openssl/install/lib:${CPKT_EXTERNAL_ROOT}/nghttp2/install/lib:${CPKT_EXTERNAL_ROOT}/libssh2/install/lib:${CPKT_EXTERNAL_ROOT}/zlib/install/lib")
    add_test(
      NAME opcua_word_portability
      COMMAND
        "${CMAKE_SOURCE_DIR}/tests/opcua_word_portability_test.sh"
        "${CMAKE_SOURCE_DIR}"
        "${CPKT_EXTERNAL_ROOT}/open62541/install/include")
    add_test(NAME abi_example_shared COMMAND cpkt_abi_smoke_shared)
    add_test(NAME abi_example_static COMMAND cpkt_abi_smoke_static)
    add_test(NAME mqttc_example_shared COMMAND cpkt_mqttc_smoke_shared)
    add_test(NAME mqttc_example_static COMMAND cpkt_mqttc_smoke_static)
    add_test(NAME whisper_example_shared COMMAND cpkt_whisper_smoke_shared)
    add_test(
      NAME static_archive_pic_link
      COMMAND
        "${CMAKE_SOURCE_DIR}/tests/static_archive_pic_link_test.sh"
        "$<TARGET_FILE:cpkt_static_archive_pic_smoke_core>"
        "$<TARGET_FILE:cpkt_static_archive_pic_smoke_mqttc>"
        "$<TARGET_FILE:cpkt_static_archive_pic_smoke_whisper>"
        "$<TARGET_FILE:cpkt_static_archive_pic_smoke_facades>")
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
      set_tests_properties(whisper_example_shared PROPERTIES
        ENVIRONMENT "DYLD_LIBRARY_PATH=${CPKT_EXTERNAL_ROOT}/whisper/install/lib")
    else()
      set_tests_properties(whisper_example_shared PROPERTIES
        ENVIRONMENT "LD_LIBRARY_PATH=${CPKT_EXTERNAL_ROOT}/whisper/install/lib")
    endif()
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
      add_test(NAME audio_facade_unit COMMAND cpkt_audio_facade_test)
      add_test(
        NAME audio_process_capture_realtime
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/audio_process_capture_realtime_test.sh"
          ${CPKT_TEST_EXECUTABLE_PREFIX}
          "$<TARGET_FILE:cpkt_audio_process_capture_probe>")
      add_test(
        NAME audio_process_playback_turns
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/audio_process_playback_turns_test.sh"
          ${CPKT_TEST_EXECUTABLE_PREFIX}
          "$<TARGET_FILE:cpkt_audio_process_playback_probe>")
      add_test(
        NAME audio_runtime_loading_policy
        COMMAND
          bash "${CMAKE_SOURCE_DIR}/tests/audio_runtime_loading_policy_test.sh"
          "$<TARGET_FILE:cpkt_audio_static>"
          "$<TARGET_FILE:cpkt_audio_shared>"
          "${CMAKE_SOURCE_DIR}/src/audio.c"
          "${CMAKE_SOURCE_DIR}/CMakeLists.txt")
      add_test(NAME sus_facade_unit COMMAND cpkt_sus_facade_test)
      set_tests_properties(sus_facade_unit PROPERTIES
        ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cpkt_sus_shared>:${CPKT_EXTERNAL_ROOT}/whisper/install/lib:${CPKT_EXTERNAL_ROOT}/curl/install/lib:${CPKT_EXTERNAL_ROOT}/openssl/install/lib:${CPKT_EXTERNAL_ROOT}/nghttp2/install/lib:${CPKT_EXTERNAL_ROOT}/libssh2/install/lib:${CPKT_EXTERNAL_ROOT}/zlib/install/lib")
      add_test(
        NAME audio_sus_shared_abi
        COMMAND
          bash "${CMAKE_SOURCE_DIR}/tests/audio_sus_shared_abi_test.sh"
          "$<TARGET_FILE:cpkt_audio_shared>"
          "$<TARGET_FILE:cpkt_sus_shared>"
          "${CPKT_AUDIO_ABI_VERSION}"
          "${CPKT_SUS_ABI_VERSION}")
      add_test(NAME sus_audio_integration COMMAND cpkt_sus_audio_integration_test)
      set_tests_properties(sus_audio_integration PROPERTIES
        LABELS "integration;opt-in;sus"
        ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cpkt_sus_shared>:${CPKT_EXTERNAL_ROOT}/whisper/install/lib:${CPKT_EXTERNAL_ROOT}/curl/install/lib:${CPKT_EXTERNAL_ROOT}/openssl/install/lib:${CPKT_EXTERNAL_ROOT}/nghttp2/install/lib:${CPKT_EXTERNAL_ROOT}/libssh2/install/lib:${CPKT_EXTERNAL_ROOT}/zlib/install/lib")
      add_test(NAME abi_test_shared COMMAND cpkt_abi_test_shared)
      add_test(NAME abi_test_static COMMAND cpkt_abi_test_static)
      add_test(NAME lua_runtime_unit COMMAND cpkt_lua_runtime_test)
      add_test(NAME opcua_facade_integration COMMAND cpkt_opcua_facade_test)
      add_test(
        NAME opcua_boundary_server_c99_client_c89
        COMMAND cpkt_opcua_facade_test server-is-c99-and-client-is-c89)
      add_test(
        NAME opcua_boundary_server_c89_client_c99
        COMMAND cpkt_opcua_facade_test server-is-c89-and-client-is-c99)
      add_test(
        NAME opcua_boundary_server_c89_client_c89
        COMMAND cpkt_opcua_facade_test server-is-c89-and-client-is-c89)
      add_test(NAME opcua_facade_failure_modes COMMAND cpkt_opcua_facade_test failure-modes)
      set_tests_properties(
        opcua_facade_integration
        opcua_boundary_server_c99_client_c89
        opcua_boundary_server_c89_client_c99
        opcua_boundary_server_c89_client_c89
        opcua_facade_failure_modes
        opcua_word_portability
        PROPERTIES
          LABELS "integration;offline;opcua"
          TIMEOUT 30)
      add_test(NAME lua_runtime_mock_unit COMMAND cpkt_lua_runtime_mock_test)
      add_test(
        NAME sus_model_catalog_metadata
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/sus_model_catalog_metadata_test.sh"
          "${CMAKE_SOURCE_DIR}")
      add_test(
        NAME sus_cpu_backend_policy
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/sus_cpu_backend_policy_test.sh"
          "${CMAKE_SOURCE_DIR}")
      add_test(
        NAME sus_segmented_policy
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/sus_segmented_policy_test.sh"
          "${CMAKE_SOURCE_DIR}")
      add_test(
        NAME sus_log_policy
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/sus_log_policy_test.sh"
          "${CMAKE_SOURCE_DIR}")
      add_test(
        NAME sus_network_boundary
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/sus_network_boundary_test.sh"
          "${CMAKE_SOURCE_DIR}")
      add_test(
        NAME sus_static_runtime_policy
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/sus_static_runtime_policy_test.sh"
          "${CMAKE_SOURCE_DIR}")
      add_test(
        NAME static_dependency_pic_policy
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/static_dependency_pic_policy_test.sh")
      add_test(
        NAME linux_toolchain_sysroot_policy
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/linux_toolchain_sysroot_policy_test.sh"
          "${CMAKE_SOURCE_DIR}")
      add_test(
        NAME dependency_toolchain_identity
        COMMAND
          "${CMAKE_COMMAND}"
          "-DCPKT_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
          -P "${CMAKE_SOURCE_DIR}/tests/dependency_toolchain_identity_test.cmake")
      add_test(
        NAME external_project_toolchain_environment
        COMMAND
          "${CMAKE_COMMAND}"
          "-DCPKT_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
          "-DCPKT_TEST_BINARY_DIR=${CMAKE_BINARY_DIR}/external-project-toolchain-env-fixture"
          -P "${CMAKE_SOURCE_DIR}/tests/external_project_toolchain_env_test.cmake")
      add_test(
        NAME pinned_toolchain_resolvers
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/toolchain_resolver_test.sh"
          "${CMAKE_SOURCE_DIR}")
      add_test(
        NAME aflpp_resolver_cleanup
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/aflpp_resolver_cleanup_test.sh"
          "${CMAKE_SOURCE_DIR}")
      add_test(
        NAME dependency_archive_cache
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/dependency_archive_cache_test.sh"
          "${CMAKE_SOURCE_DIR}")
      add_test(
        NAME dependency_cache_disabled
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/dependency_cache_disabled_test.sh"
          "${CMAKE_SOURCE_DIR}"
          "${CPKT_EXTERNAL_ROOT}")
      add_test(
        NAME privacy_scan_failure
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/privacy_scan_failure_test.sh")
      set_tests_properties(privacy_scan_failure PROPERTIES
        LABELS "packaging"
        TIMEOUT 30)
      add_test(
        NAME package_assertions_elf_runpath
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/package_assertions_elf_runpath_test.sh")
      set_tests_properties(package_assertions_elf_runpath PROPERTIES
        LABELS "packaging"
        TIMEOUT 30)
      add_test(
        NAME package_verify_toolchain_tools
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/package_verify_toolchain_tools_test.sh"
          "${CMAKE_SOURCE_DIR}")
      set_tests_properties(package_verify_toolchain_tools PROPERTIES
        LABELS "packaging"
        TIMEOUT 30)
      add_test(
        NAME native_hardening_host
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/native_hardening_host_test.sh"
          "${CMAKE_SOURCE_DIR}")
      set_tests_properties(native_hardening_host PROPERTIES
        LABELS "smoke"
        TIMEOUT 30)
      add_test(
        NAME afl_crash_gate
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/run_afl_fuzz_crash_test.sh"
          "${CMAKE_SOURCE_DIR}")
      set_tests_properties(afl_crash_gate PROPERTIES
        LABELS "fuzz;smoke"
        TIMEOUT 30)
      add_test(
        NAME lifecycle_surface
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/lifecycle_surface_test.sh")
      set_tests_properties(lifecycle_surface PROPERTIES
        LABELS "packaging;smoke"
        TIMEOUT 30)
      add_test(
        NAME package_install_smoke_args
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/package_install_smoke_args_test.sh")
      set_tests_properties(package_install_smoke_args PROPERTIES
        LABELS "packaging;smoke"
        TIMEOUT 30)
      add_test(
        NAME mqttc_linker_flags
        COMMAND
          bash
          "${CMAKE_SOURCE_DIR}/tests/mqttc_linker_flags_test.sh")
      set_tests_properties(mqttc_linker_flags PROPERTIES
        LABELS "packaging;smoke"
        TIMEOUT 30)
      add_test(
        NAME receiver_style_examples
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/receiver_style_examples_test.sh"
          "${CMAKE_SOURCE_DIR}")
      add_test(
        NAME audio_sus_header_facade
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/audio_sus_header_facade_test.sh"
          "${CMAKE_SOURCE_DIR}")
      add_test(
        NAME audio_sus_spec_consistency
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/audio_sus_spec_consistency_test.sh"
          "${CMAKE_SOURCE_DIR}")
      # clangd is a host editor tool, not a cross compiler.  Its driver model
      # cannot reliably reproduce every Bootlin target ABI/sysroot, so validate
      # only the native development-host compile database.
      if(CPKT_TARGET_ID STREQUAL "x86_64-linux-gnu")
        add_test(
          NAME clangd_comment_gate
          COMMAND
            bash
            "${CMAKE_SOURCE_DIR}/tests/clangd_comment_gate_test.sh"
            "${CMAKE_SOURCE_DIR}/scripts/verify-clangd-surface.sh")
        set_tests_properties(clangd_comment_gate PROPERTIES
          LABELS "smoke"
          TIMEOUT 30)
        add_test(
          NAME clangd_surface
          COMMAND
            "${CMAKE_SOURCE_DIR}/scripts/verify-clangd-surface.sh"
            "${CMAKE_SOURCE_DIR}"
            "${CMAKE_BINARY_DIR}")
      endif()
    endif()
  endif()
endif()

if(CPKT_ENABLE_FUZZING)
  cpkt_add_lua_runtime_mock_test(cpkt_lua_runtime_fuzz fuzz/lua_runtime_fuzz.c)
  cpkt_configure_afl_target(cpkt_lua_runtime_fuzz)
  add_executable(cpkt_opcua_facade_fuzz fuzz/opcua_facade_fuzz.c)
  cpkt_configure_afl_target(cpkt_opcua_facade_fuzz)
  target_include_directories(cpkt_opcua_facade_fuzz PRIVATE "${CMAKE_SOURCE_DIR}/include")
  target_link_libraries(cpkt_opcua_facade_fuzz PRIVATE cpkt_opcua_static)
endif()

if(CPKT_BUILD_DEPENDENCIES)
  add_custom_target(package-bundle
    COMMAND
      ${CMAKE_COMMAND}
        -DCPKT_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DCPKT_EXTERNAL_ROOT=${CPKT_EXTERNAL_ROOT}
        -DCPKT_DEPENDENCY_BUILD_ROOT=${CPKT_DEPENDENCY_BUILD_ROOT}
        -DCPKT_DIST_DIR=${CPKT_DIST_DIR}
        -DCPKT_TARGET_ID=${CPKT_TARGET_ID}
        -DCPKT_BUNDLE_VERSION=${CPKT_BUNDLE_VERSION}
        -DCPKT_OPENSSL_VERSION=${CPKT_OPENSSL_VERSION}
        -DCPKT_ZLIB_VERSION=${CPKT_ZLIB_VERSION}
        -DCPKT_CURL_VERSION=${CPKT_CURL_VERSION}
        -DCPKT_NGHTTP2_VERSION=${CPKT_NGHTTP2_VERSION}
        -DCPKT_LIBSSH2_VERSION=${CPKT_LIBSSH2_VERSION}
        -DCPKT_CMOCKA_VERSION=${CPKT_CMOCKA_VERSION}
        -DCPKT_LIBXML2_VERSION=${CPKT_LIBXML2_VERSION}
        -DCPKT_LUA_VERSION=${CPKT_LUA_VERSION}
        -DCPKT_MQTTC_VERSION=${CPKT_MQTTC_VERSION}
        -DCPKT_MQTTC_COMMIT=${CPKT_MQTTC_COMMIT}
        -DCPKT_OPEN62541_VERSION=${CPKT_OPEN62541_VERSION}
        -DCPKT_OPEN62541_PATCHSET=${CPKT_OPEN62541_PATCHSET}
        -DCPKT_LUA_RUNTIME_ABI_VERSION=${CPKT_LUA_RUNTIME_ABI_VERSION}
        -DCPKT_LUA_RUNTIME_INCLUDE_DIR=${CMAKE_SOURCE_DIR}/include
        -DCPKT_LUA_RUNTIME_STATIC_LIBRARY=$<TARGET_FILE:cpkt_lua_runtime_static>
        -DCPKT_LUA_RUNTIME_SHARED_LIBRARY=$<TARGET_FILE:cpkt_lua_runtime_shared>
        -DCPKT_MINIAUDIO_VERSION=${CPKT_MINIAUDIO_VERSION}
        -DCPKT_AUDIO_ABI_VERSION=${CPKT_AUDIO_ABI_VERSION}
        -DCPKT_AUDIO_STATIC_LIBRARY=$<TARGET_FILE:cpkt_audio_static>
        -DCPKT_AUDIO_SHARED_LIBRARY=$<TARGET_FILE:cpkt_audio_shared>
        -DCPKT_OPCUA_ABI_VERSION=${CPKT_OPCUA_ABI_VERSION}
        -DCPKT_OPCUA_STATIC_LIBRARY=$<TARGET_FILE:cpkt_opcua_static>
        -DCPKT_OPCUA_SHARED_LIBRARY=$<TARGET_FILE:cpkt_opcua_shared>
        -DCPKT_WHISPER_VERSION=${CPKT_WHISPER_VERSION}
        -DCPKT_SUS_ABI_VERSION=${CPKT_SUS_ABI_VERSION}
        -DCPKT_SUS_BACKEND_CAPABILITIES=${CPKT_SUS_BACKEND_CAPABILITIES}
        -DCPKT_SUS_STATIC_LIBRARY=$<TARGET_FILE:cpkt_sus_static>
        -DCPKT_SUS_SHARED_LIBRARY=$<TARGET_FILE:cpkt_sus_shared>
        -DCPKT_CXX_STDLIB_STATIC_LIBRARY=${CPKT_CXX_STDLIB_STATIC_LIBRARY}
        -DCPKT_CXX_LIBGCC_STATIC_LIBRARY=${CPKT_CXX_LIBGCC_STATIC_LIBRARY}
        -DCPKT_INSTALL_NAME_TOOL=${CMAKE_INSTALL_NAME_TOOL}
        -P ${CMAKE_SOURCE_DIR}/cmake/package_bundle.cmake
    DEPENDS
      cpkt_deps
      cpkt_lua_runtime_static
      cpkt_lua_runtime_shared
      cpkt_audio_static
      cpkt_audio_shared
      cpkt_opcua_static
      cpkt_opcua_shared
      cpkt_sus_static
      cpkt_sus_shared
    VERBATIM
  )
endif()
