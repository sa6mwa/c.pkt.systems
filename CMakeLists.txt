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
option(CPKT_CALLER_OWNED_DEPENDENCY_ROOTS "Treat explicitly overridden dependency roots as caller-owned and skip repo-local lifecycle invalidation for them." OFF)
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

include(cmake/CpktDependencyContract.cmake)

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

include(cmake/CpktLocalRuntime.cmake)

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

# CMake's C90 standard mapping does not express c.pkt.systems' C89 contract.
# Keep the compiler invocation explicit for every project-owned C89 target.
function(cpkt_configure_c89_target target_name)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(${target_name} PRIVATE
      -std=c89
      -Wall
      -Wextra
      -Wpedantic
      -pedantic-errors)
  endif()
endfunction()

# Lua's configured public header uses long long for its native 64-bit integer
# ABI.  Keep facade implementation source in strict C89 while admitting that
# upstream declaration-only extension; LUA_C89_NUMBERS would silently reduce
# the Lua integer ABI to 32 bits on armhf and is therefore not an option.
function(cpkt_configure_c89_lua_native_header_target target_name)
  cpkt_configure_c89_target(${target_name})
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(${target_name} PRIVATE
      -Wno-long-long
      -DLLONG_MAX=9223372036854775807LL
      -DLLONG_MIN=-9223372036854775807LL-1LL
      -DULLONG_MAX=18446744073709551615ULL)
  endif()
endfunction()

function(cpkt_add_lua_runtime_mock_test target_name source_name)
  add_executable(${target_name}
    src/lua_runtime.c
    tests/lua_mock/lua_mock.c
    ${source_name})
  cpkt_use_local_runtime(${target_name})
  target_compile_options(${target_name} PRIVATE -std=c99)
  cpkt_add_repo_warning_errors(${target_name})
  target_include_directories(${target_name} PRIVATE
    "${CMAKE_SOURCE_DIR}/tests/lua_mock/include"
    "${CMAKE_SOURCE_DIR}/include")
endfunction()

function(cpkt_configure_afl_target target_name)
  if(NOT DEFINED CPKT_AFLPP_ROOT OR "${CPKT_AFLPP_ROOT}" STREQUAL "")
    message(FATAL_ERROR
      "CPKT_ENABLE_FUZZING requires the pinned AFL++ toolchain; "
      "configure with the fuzz or opcua-fuzz preset.")
  endif()
  if(NOT EXISTS "${CPKT_AFLPP_ROOT}/bin/afl-fuzz" OR
      NOT EXISTS "${CPKT_AFLPP_ROOT}/lib/afl/afl-gcc-pass.so")
    message(FATAL_ERROR
      "CPKT_ENABLE_FUZZING requires a complete pinned AFL++ toolchain at "
      "CPKT_AFLPP_ROOT=${CPKT_AFLPP_ROOT}.")
  endif()
  if(NOT CMAKE_C_COMPILER MATCHES "/cpkt-afl-gcc$" OR
      NOT CMAKE_CXX_COMPILER MATCHES "/cpkt-afl-g\\+\\+$")
    message(FATAL_ERROR
      "CPKT_ENABLE_FUZZING requires the pinned AFL++ compiler wrappers; "
      "configure with the fuzz or opcua-fuzz preset.")
  endif()
  target_sources(${target_name} PRIVATE "${CMAKE_SOURCE_DIR}/fuzz/afl_input_driver.c")
  target_compile_options(${target_name} PRIVATE -std=c99)
  cpkt_add_repo_warning_errors(${target_name})
endfunction()

# Register checks after all targets exist, including facade-only early returns.
function(cpkt_register_local_runtime_checks)
  # Environment paths leak into host subprocesses; native tests use ELF paths.
  get_property(_cpkt_local_tests DIRECTORY PROPERTY TESTS)
  foreach(_cpkt_test IN LISTS _cpkt_local_tests)
    get_test_property(${_cpkt_test} ENVIRONMENT _cpkt_test_environment)
    if(_cpkt_test_environment MATCHES "(^|;)LD_LIBRARY_PATH=")
      message(FATAL_ERROR "Native test ${_cpkt_test} exports LD_LIBRARY_PATH; use private executable runtime paths")
    endif()
  endforeach()
  if(TARGET cpkt_audio_shared)
    add_test(NAME local_runtime_rejects_shared_library
      COMMAND bash "${CMAKE_SOURCE_DIR}/tests/local_executable_runtime_policy_test.sh"
        "${CPKT_RUNTIME_LOADER}" "$<TARGET_FILE:cpkt_audio_shared>")
    set_tests_properties(local_runtime_rejects_shared_library PROPERTIES WILL_FAIL TRUE)
  endif()
  get_property(_cpkt_local_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
  set(_cpkt_local_executables)
  foreach(_cpkt_target IN LISTS _cpkt_local_targets)
    get_target_property(_cpkt_type ${_cpkt_target} TYPE)
    if(_cpkt_type STREQUAL "EXECUTABLE")
      list(APPEND _cpkt_local_executables "$<TARGET_FILE:${_cpkt_target}>")
    endif()
  endforeach()
  add_test(NAME local_executable_runtime_policy
    COMMAND bash "${CMAKE_SOURCE_DIR}/tests/local_executable_runtime_policy_test.sh"
      "${CPKT_RUNTIME_LOADER}" ${_cpkt_local_executables})
endfunction()

if(CPKT_BUILD_TESTS AND CPKT_NATIVE_BOOTLIN_RUNTIME)
  cmake_language(DEFER CALL cpkt_register_local_runtime_checks)
  add_executable(cpkt_bootlin_runtime_test tests/bootlin_runtime_test.c)
  target_compile_options(cpkt_bootlin_runtime_test PRIVATE -std=c89)
  cpkt_add_repo_warning_errors(cpkt_bootlin_runtime_test)
  cpkt_use_local_runtime(cpkt_bootlin_runtime_test)
  add_test(NAME bootlin_runtime_process_boundary
    COMMAND cpkt_bootlin_runtime_test "${CMAKE_SYSROOT}")
  add_test(NAME local_runtime_rejects_wrong_loader
    COMMAND bash "${CMAKE_SOURCE_DIR}/tests/local_executable_runtime_policy_test.sh"
      "${CPKT_RUNTIME_LOADER}.wrong" "$<TARGET_FILE:cpkt_bootlin_runtime_test>")
  set_tests_properties(local_runtime_rejects_wrong_loader PROPERTIES WILL_FAIL TRUE)
endif()

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
set(CPKT_OPENSSL_VERSION "3.6.4" CACHE STRING "Pinned OpenSSL release.")
set(CPKT_OPENSSL_BUILD_CONFIG_REVISION "origin-runpath-1" CACHE INTERNAL
  "OpenSSL Configure contract revision." FORCE)
set(CPKT_ZLIB_VERSION "1.3.2" CACHE STRING "Pinned zlib release.")
set(CPKT_LIBPNG_VERSION "1.6.58" CACHE STRING "Pinned libpng release.")
set(CPKT_LIBHARU_VERSION "2.4.6" CACHE STRING "Pinned libHaru release.")
set(CPKT_IODBC_VERSION "3.52.16" CACHE STRING "Pinned iODBC release.")
set(CPKT_CURL_VERSION "8.22.0" CACHE STRING "Pinned curl release.")
set(CPKT_NGHTTP2_VERSION "1.70.0" CACHE STRING "Pinned nghttp2 release.")
set(CPKT_LIBSSH2_VERSION "1.11.1" CACHE STRING "Pinned libssh2 release.")
set(CPKT_CMOCKA_VERSION "2.0.2" CACHE STRING "Pinned cmocka release.")
set(CPKT_LIBXML2_VERSION "2.15.4" CACHE STRING "Pinned libxml2 release.")
set(CPKT_LUA_VERSION "5.5.1" CACHE STRING "Pinned Lua release.")
set(CPKT_MINIAUDIO_VERSION "0.11.25" CACHE STRING "Pinned miniaudio release.")
set(CPKT_WHISPER_VERSION "v1.9.4" CACHE STRING "Pinned whisper.cpp release.")
set(CPKT_MQTTC_VERSION "1.1.2" CACHE STRING "Pinned MQTT-C project version.")
set(CPKT_MQTTC_COMMIT "0f4c34c8cc00b16cfee094745d68b8cdbaecd8e0" CACHE STRING "Pinned MQTT-C commit used by open62541 v1.5.8.")
set(CPKT_OPEN62541_VERSION "1.5.8" CACHE STRING "Pinned open62541 release.")
set(CPKT_KRB5_VERSION "1.22.2" CACHE STRING "Pinned MIT Kerberos release.")
set(CPKT_CYRUS_SASL_VERSION "2.1.28" CACHE STRING "Pinned Cyrus SASL release.")
set(CPKT_OPENLDAP_VERSION "2.6.15" CACHE STRING "Pinned OpenLDAP LTS release.")
set(CPKT_POSTGRESQL_VERSION "18.6" CACHE STRING "Pinned PostgreSQL/libpq release.")
set(CPKT_SQLITE_VERSION "3.53.4" CACHE STRING "Pinned SQLite release.")
set(CPKT_OPEN62541_PATCHSET "mqtt-prefix-packet-headers-crypto-3" CACHE STRING "c.pkt.systems open62541 patchset identity." FORCE)
set(CPKT_LUA_ABI_VERSION "0" CACHE STRING "Lua C89 facade ABI major. Do not bump without maintainer approval.")
set(CPKT_LUA_RUNTIME_ABI_VERSION "0" CACHE STRING "Lua runtime facade ABI major. Do not bump without maintainer approval.")
set(CPKT_AUDIO_ABI_VERSION "0" CACHE STRING "Audio facade ABI major. Do not bump without maintainer approval.")
set(CPKT_SUS_ABI_VERSION "0" CACHE STRING "Susurro/whisper facade ABI major. Do not bump without maintainer approval.")
set(CPKT_OPCUA_ABI_VERSION "0" CACHE STRING "OPC UA facade ABI major. Do not bump without maintainer approval.")
set(CPKT_GSSAPI_ABI_VERSION "0" CACHE STRING "GSSAPI facade ABI major. Do not bump without maintainer approval.")
set(CPKT_SASL_ABI_VERSION "0" CACHE STRING "Cyrus SASL facade ABI major. Do not bump without maintainer approval.")
set(CPKT_POSTGRES_ABI_VERSION "0" CACHE STRING "PostgreSQL facade ABI major. Do not bump without maintainer approval.")
set(CPKT_SQLITE_ABI_VERSION "0" CACHE STRING "SQLite facade ABI major. Do not bump without maintainer approval.")
set(CPKT_PDF_ABI_VERSION "0" CACHE STRING "PDF facade ABI major. Do not bump without maintainer approval.")
set(CPKT_OPENSSL_ABI_VERSION "0" CACHE STRING "OpenSSL C89 facade ABI major. Do not bump without maintainer approval.")
set(CPKT_MQTTC_ABI_VERSION "0" CACHE STRING "MQTT-C facade ABI major. Do not bump without maintainer approval.")
set(CPKT_NGHTTP2_ABI_VERSION "0" CACHE STRING "nghttp2 C89 facade ABI major. Do not bump without maintainer approval.")
set(CPKT_LIBSSH2_ABI_VERSION "0" CACHE STRING "libssh2 C89 facade ABI major. Do not bump without maintainer approval.")
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
set(CPKT_DEPENDENCY_DOWNLOAD_RETRIES "3" CACHE STRING "Bounded retry count for each dependency archive URL.")

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

set(_cpkt_default_external_root "${CMAKE_SOURCE_DIR}/.cache/deps/${CPKT_TARGET_ID}")
set(_cpkt_default_dependency_build_root "${CMAKE_SOURCE_DIR}/.cache/deps-build/${CPKT_TARGET_ID}")
set(CPKT_DEPENDENCY_CONTRACT_ROOT "${CMAKE_SOURCE_DIR}/.cache/dependency-contracts"
  CACHE PATH "Target-local dependency build contract metadata.")
set(_cpkt_external_root_explicit OFF)
if(DEFINED CPKT_EXTERNAL_ROOT)
  set(_cpkt_external_root_explicit ON)
  set(_cpkt_external_root_original "${CPKT_EXTERNAL_ROOT}")
endif()
set(_cpkt_dependency_build_root_explicit OFF)
if(DEFINED CPKT_DEPENDENCY_BUILD_ROOT)
  set(_cpkt_dependency_build_root_explicit ON)
  set(_cpkt_dependency_build_root_original "${CPKT_DEPENDENCY_BUILD_ROOT}")
endif()

if(DEFINED CPKT_EXTERNAL_ROOT)
  string(FIND "${CPKT_EXTERNAL_ROOT}" "${CMAKE_SOURCE_DIR}/.cache/deps/" _cpkt_external_root_under_cache)
else()
  set(_cpkt_external_root_under_cache -1)
endif()
if(NOT DEFINED CPKT_EXTERNAL_ROOT
    OR "${CPKT_EXTERNAL_ROOT}" STREQUAL "${CMAKE_SOURCE_DIR}/.cache/deps"
    OR (NOT CPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE
      AND _cpkt_external_root_under_cache EQUAL 0
      AND NOT "${CPKT_EXTERNAL_ROOT}" STREQUAL "${_cpkt_default_external_root}"))
  set(CPKT_EXTERNAL_ROOT "${_cpkt_default_external_root}" CACHE PATH "Target-local install root for dependency artifacts." FORCE)
endif()
if(_cpkt_external_root_explicit
    AND (NOT "${_cpkt_external_root_original}" STREQUAL "${CPKT_EXTERNAL_ROOT}"
      OR "${CPKT_EXTERNAL_ROOT}" STREQUAL "${_cpkt_default_external_root}"))
  set(_cpkt_external_root_explicit OFF)
endif()
if(DEFINED CPKT_DEPENDENCY_BUILD_ROOT)
  string(FIND "${CPKT_DEPENDENCY_BUILD_ROOT}" "${CMAKE_SOURCE_DIR}/.cache/deps-build/" _cpkt_dependency_build_root_under_cache)
else()
  set(_cpkt_dependency_build_root_under_cache -1)
endif()
if(NOT DEFINED CPKT_DEPENDENCY_BUILD_ROOT
    OR "${CPKT_DEPENDENCY_BUILD_ROOT}" STREQUAL "${CMAKE_SOURCE_DIR}/.cache/deps-build"
    OR (NOT CPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE
      AND _cpkt_dependency_build_root_under_cache EQUAL 0
      AND NOT "${CPKT_DEPENDENCY_BUILD_ROOT}" STREQUAL "${_cpkt_default_dependency_build_root}"))
  set(CPKT_DEPENDENCY_BUILD_ROOT "${_cpkt_default_dependency_build_root}" CACHE PATH "Target-local build root for dependency artifacts." FORCE)
endif()
if(_cpkt_dependency_build_root_explicit
    AND (NOT "${_cpkt_dependency_build_root_original}" STREQUAL "${CPKT_DEPENDENCY_BUILD_ROOT}"
      OR "${CPKT_DEPENDENCY_BUILD_ROOT}" STREQUAL "${_cpkt_default_dependency_build_root}"))
  set(_cpkt_dependency_build_root_explicit OFF)
endif()
set(CPKT_EXTERNAL_ROOT_LIFECYCLE_OWNED ON)
if(CPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE
    AND CPKT_CALLER_OWNED_DEPENDENCY_ROOTS
    AND _cpkt_external_root_explicit)
  set(CPKT_EXTERNAL_ROOT_LIFECYCLE_OWNED OFF)
endif()
set(CPKT_DEPENDENCY_BUILD_ROOT_LIFECYCLE_OWNED ON)
if(CPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE
    AND CPKT_CALLER_OWNED_DEPENDENCY_ROOTS
    AND _cpkt_dependency_build_root_explicit)
  set(CPKT_DEPENDENCY_BUILD_ROOT_LIFECYCLE_OWNED OFF)
endif()
include(cmake/CpktDependencies.cmake)
cpkt_configure_dependencies()

find_program(CPKT_PYTHON3_EXECUTABLE NAMES python3 REQUIRED)
set(CPKT_NGHTTP2_FACADE_GENERATED_DIR
  "${CMAKE_CURRENT_BINARY_DIR}/generated/nghttp2")
set(CPKT_NGHTTP2_FACADE_INCLUDE_DIR
  "${CPKT_NGHTTP2_FACADE_GENERATED_DIR}/include")
set(CPKT_NGHTTP2_FACADE_HEADER
  "${CPKT_NGHTTP2_FACADE_INCLUDE_DIR}/cpkt/nghttp2.h")
set(CPKT_NGHTTP2_FACADE_SOURCE
  "${CPKT_NGHTTP2_FACADE_GENERATED_DIR}/src/nghttp2.c")
get_target_property(CPKT_NGHTTP2_FACADE_NATIVE_INCLUDE_DIR
  cpkt::nghttp2_static INTERFACE_INCLUDE_DIRECTORIES)
if(NOT CPKT_NGHTTP2_FACADE_NATIVE_INCLUDE_DIR)
  message(FATAL_ERROR "nghttp2 target does not expose public include directories")
endif()
set(CPKT_NGHTTP2_FACADE_NATIVE_HEADERS
  "${CPKT_NGHTTP2_FACADE_NATIVE_INCLUDE_DIR}/nghttp2/nghttp2.h"
  "${CPKT_NGHTTP2_FACADE_NATIVE_INCLUDE_DIR}/nghttp2/nghttp2ver.h")
add_custom_command(
  OUTPUT "${CPKT_NGHTTP2_FACADE_HEADER}" "${CPKT_NGHTTP2_FACADE_SOURCE}"
  COMMAND "${CPKT_PYTHON3_EXECUTABLE}"
    "${CMAKE_SOURCE_DIR}/tools/generate_nghttp2_c89_facade.py"
    --include-dir "$<TARGET_PROPERTY:cpkt::nghttp2_static,INTERFACE_INCLUDE_DIRECTORIES>"
    --header "${CPKT_NGHTTP2_FACADE_HEADER}"
    --source "${CPKT_NGHTTP2_FACADE_SOURCE}"
  DEPENDS
    "${CMAKE_SOURCE_DIR}/tools/generate_nghttp2_c89_facade.py"
    ${CPKT_NGHTTP2_FACADE_NATIVE_HEADERS}
  VERBATIM)
set_source_files_properties("${CPKT_NGHTTP2_FACADE_SOURCE}" PROPERTIES GENERATED TRUE)
add_custom_target(cpkt_nghttp2_facade_generated
  DEPENDS "${CPKT_NGHTTP2_FACADE_HEADER}" "${CPKT_NGHTTP2_FACADE_SOURCE}")
if(TARGET cpkt_nghttp2_project)
  add_dependencies(cpkt_nghttp2_facade_generated cpkt_nghttp2_project)
endif()

add_library(cpkt_nghttp2_static STATIC "${CPKT_NGHTTP2_FACADE_SOURCE}")
add_dependencies(cpkt_nghttp2_static cpkt_nghttp2_facade_generated)
cpkt_configure_c89_target(cpkt_nghttp2_static)
cpkt_add_repo_warning_errors(cpkt_nghttp2_static)
target_compile_definitions(cpkt_nghttp2_static PRIVATE CPKT_NGHTTP2_STATIC)
target_include_directories(cpkt_nghttp2_static PUBLIC
  "$<BUILD_INTERFACE:${CPKT_NGHTTP2_FACADE_INCLUDE_DIR}>")
target_link_libraries(cpkt_nghttp2_static PUBLIC cpkt::nghttp2_static)
set_target_properties(cpkt_nghttp2_static PROPERTIES
  OUTPUT_NAME cpkt_nghttp2
  POSITION_INDEPENDENT_CODE ON)

add_library(cpkt_nghttp2_shared SHARED "${CPKT_NGHTTP2_FACADE_SOURCE}")
add_dependencies(cpkt_nghttp2_shared cpkt_nghttp2_facade_generated)
cpkt_configure_c89_target(cpkt_nghttp2_shared)
cpkt_add_repo_warning_errors(cpkt_nghttp2_shared)
target_compile_definitions(cpkt_nghttp2_shared PRIVATE CPKT_NGHTTP2_BUILDING_SHARED)
target_include_directories(cpkt_nghttp2_shared PUBLIC
  "$<BUILD_INTERFACE:${CPKT_NGHTTP2_FACADE_INCLUDE_DIR}>")
target_link_libraries(cpkt_nghttp2_shared PUBLIC cpkt::nghttp2_shared)
set_target_properties(cpkt_nghttp2_shared PROPERTIES
  OUTPUT_NAME cpkt_nghttp2
  C_VISIBILITY_PRESET hidden
  POSITION_INDEPENDENT_CODE ON
  SOVERSION "${CPKT_NGHTTP2_ABI_VERSION}"
  VERSION "${PROJECT_VERSION}")
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set_target_properties(cpkt_nghttp2_shared PROPERTIES
    INSTALL_NAME_DIR "@rpath"
    BUILD_RPATH "@loader_path"
    INSTALL_RPATH "@loader_path"
    BUILD_WITH_INSTALL_RPATH ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set_target_properties(cpkt_nghttp2_shared PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH ON)
endif()

set(CPKT_LIBSSH2_FACADE_GENERATED_DIR
  "${CMAKE_CURRENT_BINARY_DIR}/generated/libssh2")
set(CPKT_LIBSSH2_FACADE_INCLUDE_DIR
  "${CPKT_LIBSSH2_FACADE_GENERATED_DIR}/include")
set(CPKT_LIBSSH2_FACADE_HEADER
  "${CPKT_LIBSSH2_FACADE_INCLUDE_DIR}/cpkt/libssh2.h")
set(CPKT_LIBSSH2_FACADE_SOURCE
  "${CPKT_LIBSSH2_FACADE_GENERATED_DIR}/src/libssh2.c")
get_target_property(CPKT_LIBSSH2_FACADE_NATIVE_INCLUDE_DIR
  cpkt::libssh2_static INTERFACE_INCLUDE_DIRECTORIES)
if(NOT CPKT_LIBSSH2_FACADE_NATIVE_INCLUDE_DIR)
  message(FATAL_ERROR "libssh2 target does not expose public include directories")
endif()
set(CPKT_LIBSSH2_FACADE_NATIVE_HEADERS
  "${CPKT_LIBSSH2_FACADE_NATIVE_INCLUDE_DIR}/libssh2.h"
  "${CPKT_LIBSSH2_FACADE_NATIVE_INCLUDE_DIR}/libssh2_sftp.h"
  "${CPKT_LIBSSH2_FACADE_NATIVE_INCLUDE_DIR}/libssh2_publickey.h")
add_custom_command(
  OUTPUT "${CPKT_LIBSSH2_FACADE_HEADER}" "${CPKT_LIBSSH2_FACADE_SOURCE}"
  COMMAND "${CPKT_PYTHON3_EXECUTABLE}"
    "${CMAKE_SOURCE_DIR}/tools/generate_libssh2_c89_facade.py"
    --include-dir "$<TARGET_PROPERTY:cpkt::libssh2_static,INTERFACE_INCLUDE_DIRECTORIES>"
    --header "${CPKT_LIBSSH2_FACADE_HEADER}"
    --source "${CPKT_LIBSSH2_FACADE_SOURCE}"
  DEPENDS
    "${CMAKE_SOURCE_DIR}/tools/generate_libssh2_c89_facade.py"
    ${CPKT_LIBSSH2_FACADE_NATIVE_HEADERS}
  VERBATIM)
set_source_files_properties("${CPKT_LIBSSH2_FACADE_SOURCE}" PROPERTIES GENERATED TRUE)
add_custom_target(cpkt_libssh2_facade_generated
  DEPENDS "${CPKT_LIBSSH2_FACADE_HEADER}" "${CPKT_LIBSSH2_FACADE_SOURCE}")
if(TARGET cpkt_libssh2_project)
  add_dependencies(cpkt_libssh2_facade_generated cpkt_libssh2_project)
endif()

add_library(cpkt_libssh2_static STATIC "${CPKT_LIBSSH2_FACADE_SOURCE}")
add_dependencies(cpkt_libssh2_static cpkt_libssh2_facade_generated)
cpkt_configure_c89_target(cpkt_libssh2_static)
cpkt_add_repo_warning_errors(cpkt_libssh2_static)
target_compile_definitions(cpkt_libssh2_static PRIVATE CPKT_LIBSSH2_STATIC)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  # libssh2 exposes long long in its public header; the generated facade does not.
  target_compile_options(cpkt_libssh2_static PRIVATE
    -Wno-long-long
    -Wno-deprecated-declarations)
endif()
target_include_directories(cpkt_libssh2_static PUBLIC
  "$<BUILD_INTERFACE:${CPKT_LIBSSH2_FACADE_INCLUDE_DIR}>")
target_link_libraries(cpkt_libssh2_static PUBLIC cpkt::libssh2_static)
set_target_properties(cpkt_libssh2_static PROPERTIES
  OUTPUT_NAME cpkt_libssh2
  POSITION_INDEPENDENT_CODE ON)

add_library(cpkt_libssh2_shared SHARED "${CPKT_LIBSSH2_FACADE_SOURCE}")
add_dependencies(cpkt_libssh2_shared cpkt_libssh2_facade_generated)
cpkt_configure_c89_target(cpkt_libssh2_shared)
cpkt_add_repo_warning_errors(cpkt_libssh2_shared)
target_compile_definitions(cpkt_libssh2_shared PRIVATE CPKT_LIBSSH2_BUILDING_SHARED)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  target_compile_options(cpkt_libssh2_shared PRIVATE
    -Wno-long-long
    -Wno-deprecated-declarations)
endif()
target_include_directories(cpkt_libssh2_shared PUBLIC
  "$<BUILD_INTERFACE:${CPKT_LIBSSH2_FACADE_INCLUDE_DIR}>")
target_link_libraries(cpkt_libssh2_shared PUBLIC cpkt::libssh2_shared)
set_target_properties(cpkt_libssh2_shared PROPERTIES
  OUTPUT_NAME cpkt_libssh2
  C_VISIBILITY_PRESET hidden
  POSITION_INDEPENDENT_CODE ON
  SOVERSION "${CPKT_LIBSSH2_ABI_VERSION}"
  VERSION "${PROJECT_VERSION}")
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set_target_properties(cpkt_libssh2_shared PROPERTIES
    INSTALL_NAME_DIR "@rpath"
    BUILD_RPATH "@loader_path"
    INSTALL_RPATH "@loader_path"
    BUILD_WITH_INSTALL_RPATH ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set_target_properties(cpkt_libssh2_shared PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH ON)
endif()

set(CPKT_MQTTC_FACADE_GENERATED_DIR
  "${CMAKE_CURRENT_BINARY_DIR}/generated/mqttc")
set(CPKT_MQTTC_FACADE_INCLUDE_DIR
  "${CPKT_MQTTC_FACADE_GENERATED_DIR}/include")
set(CPKT_MQTTC_FACADE_HEADER
  "${CPKT_MQTTC_FACADE_INCLUDE_DIR}/cpkt/mqttc.h")
set(CPKT_MQTTC_FACADE_SOURCE
  "${CPKT_MQTTC_FACADE_GENERATED_DIR}/src/mqttc.c")
get_target_property(CPKT_MQTTC_FACADE_NATIVE_INCLUDE_DIR
  cpkt::mqttc_static INTERFACE_INCLUDE_DIRECTORIES)
if(NOT CPKT_MQTTC_FACADE_NATIVE_INCLUDE_DIR)
  message(FATAL_ERROR "MQTT-C target does not expose public include directories")
endif()
set(CPKT_MQTTC_FACADE_NATIVE_HEADERS
  "${CPKT_MQTTC_FACADE_NATIVE_INCLUDE_DIR}/mqtt.h")
add_custom_command(
  OUTPUT "${CPKT_MQTTC_FACADE_HEADER}" "${CPKT_MQTTC_FACADE_SOURCE}"
  COMMAND "${CPKT_PYTHON3_EXECUTABLE}"
    "${CMAKE_SOURCE_DIR}/tools/generate_mqttc_c89_facade.py"
    --include-dir "$<TARGET_PROPERTY:cpkt::mqttc_static,INTERFACE_INCLUDE_DIRECTORIES>"
    --header "${CPKT_MQTTC_FACADE_HEADER}"
    --source "${CPKT_MQTTC_FACADE_SOURCE}"
  DEPENDS
    "${CMAKE_SOURCE_DIR}/tools/generate_mqttc_c89_facade.py"
    ${CPKT_MQTTC_FACADE_NATIVE_HEADERS}
  VERBATIM)
set_source_files_properties("${CPKT_MQTTC_FACADE_SOURCE}" PROPERTIES GENERATED TRUE)
add_custom_target(cpkt_mqttc_facade_generated
  DEPENDS "${CPKT_MQTTC_FACADE_HEADER}" "${CPKT_MQTTC_FACADE_SOURCE}")
if(TARGET cpkt_mqttc_project)
  add_dependencies(cpkt_mqttc_facade_generated cpkt_mqttc_project)
endif()

add_library(cpkt_mqttc_static STATIC "${CPKT_MQTTC_FACADE_SOURCE}")
add_dependencies(cpkt_mqttc_static cpkt_mqttc_facade_generated)
cpkt_configure_c89_target(cpkt_mqttc_static)
cpkt_add_repo_warning_errors(cpkt_mqttc_static)
target_compile_definitions(cpkt_mqttc_static PRIVATE CPKT_MQTTC_STATIC)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  # MQTT-C's upstream C99 header is implementation detail, not facade surface.
  target_compile_options(cpkt_mqttc_static PRIVATE -Wno-comment)
endif()
target_include_directories(cpkt_mqttc_static PUBLIC
  "$<BUILD_INTERFACE:${CPKT_MQTTC_FACADE_INCLUDE_DIR}>")
target_link_libraries(cpkt_mqttc_static PUBLIC cpkt::mqttc_static)
set_target_properties(cpkt_mqttc_static PROPERTIES
  OUTPUT_NAME cpkt_mqttc
  POSITION_INDEPENDENT_CODE ON)

add_library(cpkt_mqttc_shared SHARED "${CPKT_MQTTC_FACADE_SOURCE}")
add_dependencies(cpkt_mqttc_shared cpkt_mqttc_facade_generated)
cpkt_configure_c89_target(cpkt_mqttc_shared)
cpkt_add_repo_warning_errors(cpkt_mqttc_shared)
target_compile_definitions(cpkt_mqttc_shared PRIVATE CPKT_MQTTC_BUILDING_SHARED)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  target_compile_options(cpkt_mqttc_shared PRIVATE -Wno-comment)
endif()
target_include_directories(cpkt_mqttc_shared PUBLIC
  "$<BUILD_INTERFACE:${CPKT_MQTTC_FACADE_INCLUDE_DIR}>")
target_link_libraries(cpkt_mqttc_shared PUBLIC cpkt::mqttc_shared)
set_target_properties(cpkt_mqttc_shared PROPERTIES
  OUTPUT_NAME cpkt_mqttc
  C_VISIBILITY_PRESET hidden
  POSITION_INDEPENDENT_CODE ON
  SOVERSION "${CPKT_MQTTC_ABI_VERSION}"
  VERSION "${PROJECT_VERSION}")
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set_target_properties(cpkt_mqttc_shared PROPERTIES
    INSTALL_NAME_DIR "@rpath"
    BUILD_RPATH "@loader_path"
    INSTALL_RPATH "@loader_path"
    BUILD_WITH_INSTALL_RPATH ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set_target_properties(cpkt_mqttc_shared PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH ON)
endif()

add_library(cpkt_openssl_static STATIC src/openssl.c)
cpkt_configure_c89_target(cpkt_openssl_static)
cpkt_add_repo_warning_errors(cpkt_openssl_static)
target_compile_definitions(cpkt_openssl_static PRIVATE CPKT_OPENSSL_STATIC)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  # The facade deliberately preserves OpenSSL's configured public legacy API.
  target_compile_options(cpkt_openssl_static PRIVATE -Wno-deprecated-declarations)
endif()
target_include_directories(cpkt_openssl_static PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>")
target_link_libraries(cpkt_openssl_static PUBLIC
  cpkt::openssl_ssl_static
  cpkt::openssl_crypto_static)
set_target_properties(cpkt_openssl_static PROPERTIES
  OUTPUT_NAME cpkt_openssl
  POSITION_INDEPENDENT_CODE ON)

add_library(cpkt_openssl_shared SHARED src/openssl.c)
cpkt_configure_c89_target(cpkt_openssl_shared)
cpkt_add_repo_warning_errors(cpkt_openssl_shared)
target_compile_definitions(cpkt_openssl_shared PRIVATE CPKT_OPENSSL_BUILDING_SHARED)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  target_compile_options(cpkt_openssl_shared PRIVATE -Wno-deprecated-declarations)
endif()
target_include_directories(cpkt_openssl_shared PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>")
target_link_libraries(cpkt_openssl_shared PUBLIC
  cpkt::openssl_ssl_shared
  cpkt::openssl_crypto_shared)
set_target_properties(cpkt_openssl_shared PROPERTIES
  OUTPUT_NAME cpkt_openssl
  C_VISIBILITY_PRESET hidden
  POSITION_INDEPENDENT_CODE ON
  SOVERSION "${CPKT_OPENSSL_ABI_VERSION}"
  VERSION "${PROJECT_VERSION}")
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set_target_properties(cpkt_openssl_shared PROPERTIES
    INSTALL_NAME_DIR "@rpath"
    BUILD_RPATH "@loader_path"
    INSTALL_RPATH "@loader_path"
    BUILD_WITH_INSTALL_RPATH ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set_target_properties(cpkt_openssl_shared PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH ON)
endif()

set(CPKT_LUA_FACADE_GENERATED_DIR
  "${CMAKE_CURRENT_BINARY_DIR}/generated/lua")
set(CPKT_LUA_FACADE_INCLUDE_DIR
  "${CPKT_LUA_FACADE_GENERATED_DIR}/include")
set(CPKT_LUA_FACADE_HEADER
  "${CPKT_LUA_FACADE_INCLUDE_DIR}/cpkt/lua.h")
set(CPKT_LUA_FACADE_SOURCE
  "${CPKT_LUA_FACADE_GENERATED_DIR}/src/lua.c")
get_target_property(CPKT_LUA_FACADE_NATIVE_INCLUDE_DIR
  cpkt::lua_static INTERFACE_INCLUDE_DIRECTORIES)
if(NOT CPKT_LUA_FACADE_NATIVE_INCLUDE_DIR)
  message(FATAL_ERROR "Lua target does not expose public include directories")
endif()
set(CPKT_LUA_FACADE_NATIVE_HEADERS
  "${CPKT_LUA_FACADE_NATIVE_INCLUDE_DIR}/lua.h"
  "${CPKT_LUA_FACADE_NATIVE_INCLUDE_DIR}/luaconf.h"
  "${CPKT_LUA_FACADE_NATIVE_INCLUDE_DIR}/lauxlib.h"
  "${CPKT_LUA_FACADE_NATIVE_INCLUDE_DIR}/lualib.h")
add_custom_command(
  OUTPUT "${CPKT_LUA_FACADE_HEADER}" "${CPKT_LUA_FACADE_SOURCE}"
  COMMAND "${CPKT_PYTHON3_EXECUTABLE}"
    "${CMAKE_SOURCE_DIR}/tools/generate_lua_c89_facade.py"
    --include-dir "$<TARGET_PROPERTY:cpkt::lua_static,INTERFACE_INCLUDE_DIRECTORIES>"
    --header "${CPKT_LUA_FACADE_HEADER}"
    --source "${CPKT_LUA_FACADE_SOURCE}"
  DEPENDS
    "${CMAKE_SOURCE_DIR}/tools/generate_lua_c89_facade.py"
    ${CPKT_LUA_FACADE_NATIVE_HEADERS}
  VERBATIM)
set_source_files_properties("${CPKT_LUA_FACADE_SOURCE}" PROPERTIES GENERATED TRUE)
add_custom_target(cpkt_lua_facade_generated
  DEPENDS "${CPKT_LUA_FACADE_HEADER}" "${CPKT_LUA_FACADE_SOURCE}")
if(TARGET cpkt_lua_project)
  add_dependencies(cpkt_lua_facade_generated cpkt_lua_project)
endif()

add_library(cpkt_lua_static STATIC "${CPKT_LUA_FACADE_SOURCE}")
cpkt_configure_c89_lua_native_header_target(cpkt_lua_static)
add_dependencies(cpkt_lua_static cpkt_lua_facade_generated)
cpkt_add_repo_warning_errors(cpkt_lua_static)
target_compile_definitions(cpkt_lua_static PRIVATE CPKT_LUA_STATIC)
target_include_directories(cpkt_lua_static PUBLIC
  "$<BUILD_INTERFACE:${CPKT_LUA_FACADE_INCLUDE_DIR}>")
target_link_libraries(cpkt_lua_static PUBLIC cpkt::lua_static)
set_target_properties(cpkt_lua_static PROPERTIES
  OUTPUT_NAME cpkt_lua
  POSITION_INDEPENDENT_CODE ON)

add_library(cpkt_lua_shared SHARED "${CPKT_LUA_FACADE_SOURCE}")
cpkt_configure_c89_lua_native_header_target(cpkt_lua_shared)
add_dependencies(cpkt_lua_shared cpkt_lua_facade_generated)
cpkt_add_repo_warning_errors(cpkt_lua_shared)
target_compile_definitions(cpkt_lua_shared PRIVATE CPKT_LUA_BUILDING_SHARED)
target_include_directories(cpkt_lua_shared PUBLIC
  "$<BUILD_INTERFACE:${CPKT_LUA_FACADE_INCLUDE_DIR}>")
target_link_libraries(cpkt_lua_shared PUBLIC cpkt::lua_shared)
set_target_properties(cpkt_lua_shared PROPERTIES
  OUTPUT_NAME cpkt_lua
  C_VISIBILITY_PRESET hidden
  POSITION_INDEPENDENT_CODE ON
  SOVERSION "${CPKT_LUA_ABI_VERSION}"
  VERSION "${PROJECT_VERSION}")
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set_target_properties(cpkt_lua_shared PROPERTIES
    INSTALL_NAME_DIR "@rpath"
    BUILD_RPATH "@loader_path"
    INSTALL_RPATH "@loader_path"
    BUILD_WITH_INSTALL_RPATH ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set_target_properties(cpkt_lua_shared PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH ON)
endif()

add_library(cpkt_lua_runtime_static STATIC src/lua_runtime.c)
cpkt_configure_c89_lua_native_header_target(cpkt_lua_runtime_static)
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
cpkt_configure_c89_lua_native_header_target(cpkt_lua_runtime_shared)
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
add_library(cpkt_audio_static STATIC src/audio.c)
cpkt_configure_c89_target(cpkt_audio_static)
add_dependencies(cpkt_audio_static cpkt::miniaudio_static)
cpkt_add_repo_warning_errors(cpkt_audio_static)
if(UNIX)
  target_compile_definitions(cpkt_audio_static PRIVATE _POSIX_C_SOURCE=200809L)
endif()
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
cpkt_configure_c89_target(cpkt_audio_shared)
add_dependencies(cpkt_audio_shared cpkt::miniaudio_shared)
cpkt_add_repo_warning_errors(cpkt_audio_shared)
if(UNIX)
  target_compile_definitions(cpkt_audio_shared PRIVATE _POSIX_C_SOURCE=200809L)
endif()
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
add_library(cpkt_sus_static STATIC src/sus.c)
cpkt_configure_c89_target(cpkt_sus_static)
cpkt_add_repo_warning_errors(cpkt_sus_static)
if(UNIX)
  target_compile_definitions(cpkt_sus_static PRIVATE _POSIX_C_SOURCE=200809L)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  # GGML's public C header unconditionally uses the C99 restrict keyword.
  # The C89 facade does not depend on restrict semantics at this ABI boundary.
  target_compile_options(cpkt_sus_static PRIVATE -Drestrict=)
endif()
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
cpkt_configure_c89_target(cpkt_sus_shared)
cpkt_add_repo_warning_errors(cpkt_sus_shared)
if(UNIX)
  target_compile_definitions(cpkt_sus_shared PRIVATE _POSIX_C_SOURCE=200809L)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  target_compile_options(cpkt_sus_shared PRIVATE -Drestrict=)
endif()
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
if(CPKT_CAN_RUN_TARGET_EXECUTABLES)
  add_executable(cpktxscribe tools/cpktxscribe.c)
  cpkt_use_local_runtime(cpktxscribe)
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
cpkt_configure_c89_target(cpkt_opcua_static)
cpkt_add_repo_warning_errors(cpkt_opcua_static)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  # open62541 exposes internal convenience functions as static inline in its
  # public headers.  The facade uses only its exported ABI; C89 needs the
  # declaration bodies without the C99 inline keyword.
  target_compile_options(cpkt_opcua_static PRIVATE -Dinline=)
endif()
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
cpkt_configure_c89_target(cpkt_opcua_shared)
cpkt_add_repo_warning_errors(cpkt_opcua_shared)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  target_compile_options(cpkt_opcua_shared PRIVATE -Dinline=)
endif()
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
add_library(cpkt_gssapi_static STATIC src/gssapi.c)
cpkt_configure_c89_target(cpkt_gssapi_static)
cpkt_add_repo_warning_errors(cpkt_gssapi_static)
target_include_directories(cpkt_gssapi_static
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>")
target_link_libraries(cpkt_gssapi_static PUBLIC cpkt::gssapi_krb5_static)
target_compile_definitions(cpkt_gssapi_static PRIVATE CPKT_GSSAPI_FACADE_VERSION="${CPKT_GSSAPI_ABI_VERSION}")
set_target_properties(cpkt_gssapi_static PROPERTIES
  OUTPUT_NAME cpkt_gssapi
  POSITION_INDEPENDENT_CODE ON)

add_library(cpkt_gssapi_shared SHARED src/gssapi.c)
cpkt_configure_c89_target(cpkt_gssapi_shared)
cpkt_add_repo_warning_errors(cpkt_gssapi_shared)
target_include_directories(cpkt_gssapi_shared
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>")
target_link_libraries(cpkt_gssapi_shared PUBLIC cpkt::gssapi_krb5_shared)
target_compile_definitions(cpkt_gssapi_shared PRIVATE CPKT_GSSAPI_FACADE_VERSION="${CPKT_GSSAPI_ABI_VERSION}")
set_target_properties(cpkt_gssapi_shared PROPERTIES
  OUTPUT_NAME cpkt_gssapi
  POSITION_INDEPENDENT_CODE ON
  SOVERSION "${CPKT_GSSAPI_ABI_VERSION}"
  VERSION "${PROJECT_VERSION}")
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set_target_properties(cpkt_gssapi_shared PROPERTIES
    INSTALL_NAME_DIR "@rpath"
    BUILD_RPATH "@loader_path"
    INSTALL_RPATH "@loader_path"
    BUILD_WITH_INSTALL_RPATH ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set_target_properties(cpkt_gssapi_shared PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH ON)
endif()
add_library(cpkt_sasl_static STATIC src/sasl.c)
cpkt_configure_c89_target(cpkt_sasl_static)
cpkt_add_repo_warning_errors(cpkt_sasl_static)
target_include_directories(cpkt_sasl_static
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>")
target_link_libraries(cpkt_sasl_static PUBLIC cpkt::cyrus_sasl_static)
target_compile_definitions(cpkt_sasl_static PRIVATE
  CPKT_SASL_FACADE_VERSION="${CPKT_SASL_ABI_VERSION}")
set_target_properties(cpkt_sasl_static PROPERTIES
  OUTPUT_NAME cpkt_sasl
  POSITION_INDEPENDENT_CODE ON)

add_library(cpkt_sasl_shared SHARED src/sasl.c)
cpkt_configure_c89_target(cpkt_sasl_shared)
cpkt_add_repo_warning_errors(cpkt_sasl_shared)
target_include_directories(cpkt_sasl_shared
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>")
target_link_libraries(cpkt_sasl_shared PUBLIC cpkt::cyrus_sasl_shared)
target_compile_definitions(cpkt_sasl_shared PRIVATE
  CPKT_SASL_FACADE_VERSION="${CPKT_SASL_ABI_VERSION}")
set_target_properties(cpkt_sasl_shared PROPERTIES
  OUTPUT_NAME cpkt_sasl
  POSITION_INDEPENDENT_CODE ON
  SOVERSION "${CPKT_SASL_ABI_VERSION}"
  VERSION "${PROJECT_VERSION}")
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set_target_properties(cpkt_sasl_shared PROPERTIES
    INSTALL_NAME_DIR "@rpath"
    BUILD_RPATH "@loader_path"
    INSTALL_RPATH "@loader_path"
    BUILD_WITH_INSTALL_RPATH ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set_target_properties(cpkt_sasl_shared PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH ON)
endif()
add_library(cpkt_postgres_static STATIC src/postgres.c)
cpkt_configure_c89_target(cpkt_postgres_static)
cpkt_add_repo_warning_errors(cpkt_postgres_static)
target_include_directories(cpkt_postgres_static
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>")
target_link_libraries(cpkt_postgres_static PUBLIC cpkt::postgresql_static PRIVATE Threads::Threads)
target_compile_definitions(cpkt_postgres_static PRIVATE CPKT_POSTGRES_FACADE_VERSION="${CPKT_POSTGRES_ABI_VERSION}")
set_target_properties(cpkt_postgres_static PROPERTIES
  OUTPUT_NAME cpkt_postgres
  POSITION_INDEPENDENT_CODE ON)

add_library(cpkt_postgres_shared SHARED src/postgres.c)
cpkt_configure_c89_target(cpkt_postgres_shared)
cpkt_add_repo_warning_errors(cpkt_postgres_shared)
target_include_directories(cpkt_postgres_shared
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>")
target_link_libraries(cpkt_postgres_shared PUBLIC cpkt::postgresql_shared PRIVATE Threads::Threads)
target_compile_definitions(cpkt_postgres_shared PRIVATE CPKT_POSTGRES_FACADE_VERSION="${CPKT_POSTGRES_ABI_VERSION}")
set_target_properties(cpkt_postgres_shared PROPERTIES
  OUTPUT_NAME cpkt_postgres
  POSITION_INDEPENDENT_CODE ON
  SOVERSION "${CPKT_POSTGRES_ABI_VERSION}"
  VERSION "${PROJECT_VERSION}")
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set_target_properties(cpkt_postgres_shared PROPERTIES
    INSTALL_NAME_DIR "@rpath"
    BUILD_RPATH "@loader_path"
    INSTALL_RPATH "@loader_path"
    BUILD_WITH_INSTALL_RPATH ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # libldap uses OpenLDAP's static liblutil internally.  Keep that private
  # implementation detail out of the facade's ELF ABI while preserving every
  # cpkt_postgres_* symbol defined by this target's own object files.
  target_link_options(cpkt_postgres_shared PRIVATE "-Wl,--exclude-libs,ALL")
  set_target_properties(cpkt_postgres_shared PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH ON)
endif()
add_library(cpkt_pdf_static STATIC src/pdf.c)
cpkt_configure_c89_target(cpkt_pdf_static)
cpkt_add_repo_warning_errors(cpkt_pdf_static)
target_include_directories(cpkt_pdf_static PUBLIC "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>")
target_link_libraries(cpkt_pdf_static PUBLIC cpkt::haru_static)
set_target_properties(cpkt_pdf_static PROPERTIES OUTPUT_NAME cpkt_pdf POSITION_INDEPENDENT_CODE ON)

add_library(cpkt_pdf_shared SHARED src/pdf.c)
cpkt_configure_c89_target(cpkt_pdf_shared)
cpkt_add_repo_warning_errors(cpkt_pdf_shared)
target_include_directories(cpkt_pdf_shared PUBLIC "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>")
target_link_libraries(cpkt_pdf_shared PUBLIC cpkt::haru_shared)
set_target_properties(cpkt_pdf_shared PROPERTIES
  OUTPUT_NAME cpkt_pdf C_VISIBILITY_PRESET hidden POSITION_INDEPENDENT_CODE ON
  SOVERSION "${CPKT_PDF_ABI_VERSION}" VERSION "${PROJECT_VERSION}")
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set_target_properties(cpkt_pdf_shared PROPERTIES
    INSTALL_NAME_DIR "@rpath" BUILD_RPATH "@loader_path"
    INSTALL_RPATH "@loader_path" BUILD_WITH_INSTALL_RPATH ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set_target_properties(cpkt_pdf_shared PROPERTIES
    BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH ON)
endif()

add_library(cpkt_sqlite_static STATIC src/sqlite.c)
cpkt_configure_c89_target(cpkt_sqlite_static)
cpkt_add_repo_warning_errors(cpkt_sqlite_static)
target_include_directories(cpkt_sqlite_static
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>")
target_link_libraries(cpkt_sqlite_static PUBLIC
  cpkt::sqlite_static Threads::Threads)
target_compile_definitions(cpkt_sqlite_static PRIVATE
  CPKT_SQLITE_FACADE_VERSION="${CPKT_SQLITE_ABI_VERSION}"
  SQLITE_ENABLE_CARRAY=1
  SQLITE_ENABLE_NORMALIZE=1
  SQLITE_ENABLE_PREUPDATE_HOOK=1
  SQLITE_ENABLE_SESSION=1)
set_target_properties(cpkt_sqlite_static PROPERTIES
  OUTPUT_NAME cpkt_sqlite
  POSITION_INDEPENDENT_CODE ON)

add_library(cpkt_sqlite_shared SHARED src/sqlite.c)
cpkt_configure_c89_target(cpkt_sqlite_shared)
cpkt_add_repo_warning_errors(cpkt_sqlite_shared)
target_include_directories(cpkt_sqlite_shared
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>")
target_link_libraries(cpkt_sqlite_shared PUBLIC
  cpkt::sqlite_shared Threads::Threads)
target_compile_definitions(cpkt_sqlite_shared PRIVATE
  CPKT_SQLITE_FACADE_VERSION="${CPKT_SQLITE_ABI_VERSION}"
  SQLITE_ENABLE_CARRAY=1
  SQLITE_ENABLE_NORMALIZE=1
  SQLITE_ENABLE_PREUPDATE_HOOK=1
  SQLITE_ENABLE_SESSION=1)
set_target_properties(cpkt_sqlite_shared PROPERTIES
  OUTPUT_NAME cpkt_sqlite
  POSITION_INDEPENDENT_CODE ON
  SOVERSION "${CPKT_SQLITE_ABI_VERSION}"
  VERSION "${PROJECT_VERSION}")
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set_target_properties(cpkt_sqlite_shared PROPERTIES
    INSTALL_NAME_DIR "@rpath"
    BUILD_RPATH "@loader_path"
    INSTALL_RPATH "@loader_path"
    BUILD_WITH_INSTALL_RPATH ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set_target_properties(cpkt_sqlite_shared PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH ON)
endif()
if(CPKT_BUILD_TESTS)
  foreach(_cpkt_curl_linkage IN ITEMS static shared)
    add_executable(cpkt_curl_async_dns_${_cpkt_curl_linkage}_test
      tests/curl_async_dns_test.c)
    cpkt_use_local_runtime(cpkt_curl_async_dns_${_cpkt_curl_linkage}_test)
    target_compile_options(cpkt_curl_async_dns_${_cpkt_curl_linkage}_test
      PRIVATE -std=c99)
    cpkt_add_repo_warning_errors(cpkt_curl_async_dns_${_cpkt_curl_linkage}_test)
    target_link_libraries(cpkt_curl_async_dns_${_cpkt_curl_linkage}_test PRIVATE
      cpkt::curl_${_cpkt_curl_linkage})
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND
        _cpkt_curl_linkage STREQUAL "shared")
      set_target_properties(cpkt_curl_async_dns_shared_test PROPERTIES
        BUILD_RPATH
          "${CPKT_EXTERNAL_ROOT}/curl/install/lib;${CPKT_EXTERNAL_ROOT}/openssl/install/lib;${CPKT_EXTERNAL_ROOT}/nghttp2/install/lib;${CPKT_EXTERNAL_ROOT}/libssh2/install/lib;${CPKT_EXTERNAL_ROOT}/zlib/install/lib")
    endif()
  endforeach()
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
  cpkt_use_local_runtime(cpkt_lua_runtime_c89_example)
  cpkt_add_repo_warning_errors(cpkt_lua_runtime_c89_example)
  set_source_files_properties(examples/lua-runtime-c89/main.c PROPERTIES
    COMPILE_OPTIONS "-std=c89")
  set_source_files_properties(examples/lua-runtime-c89/host_module.c PROPERTIES
    COMPILE_OPTIONS "-std=c99")
  target_link_libraries(cpkt_lua_runtime_c89_example PRIVATE cpkt_lua_runtime_static)
  add_executable(cpkt_opcua_c89_example examples/opcua-c89/main.c)
  cpkt_use_local_runtime(cpkt_opcua_c89_example)
  target_compile_options(cpkt_opcua_c89_example PRIVATE -std=c89)
  cpkt_add_repo_warning_errors(cpkt_opcua_c89_example)
  target_link_libraries(cpkt_opcua_c89_example PRIVATE cpkt_opcua_static)
  add_executable(cpkt_audio_sus_c89_example examples/audio-sus-c89/main.c)
  cpkt_use_local_runtime(cpkt_audio_sus_c89_example)
  target_compile_options(cpkt_audio_sus_c89_example PRIVATE -std=c89)
  cpkt_add_repo_warning_errors(cpkt_audio_sus_c89_example)
  target_link_libraries(cpkt_audio_sus_c89_example PRIVATE
    cpkt_audio_static
    cpkt_sus_shared)
  add_executable(cpkt_audio_vox_intro_c89_example
    examples/audio-vox-intro-c89/main.c)
  cpkt_use_local_runtime(cpkt_audio_vox_intro_c89_example)
  target_compile_options(cpkt_audio_vox_intro_c89_example PRIVATE -std=c89)
  cpkt_add_repo_warning_errors(cpkt_audio_vox_intro_c89_example)
  target_link_libraries(cpkt_audio_vox_intro_c89_example PRIVATE
    cpkt_audio_static)
  add_executable(cpkt_audio_live_vox_c89_example
    examples/audio-live-vox-c89/main.c)
  cpkt_use_local_runtime(cpkt_audio_live_vox_c89_example)
  target_compile_options(cpkt_audio_live_vox_c89_example PRIVATE -std=c89)
  cpkt_add_repo_warning_errors(cpkt_audio_live_vox_c89_example)
  target_link_libraries(cpkt_audio_live_vox_c89_example PRIVATE
    cpkt_audio_shared)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CPKT_TARGET_LIBC STREQUAL "musl")
    add_executable(cpkt_audio_live_vox_static_c89_example
      examples/audio-live-vox-c89/main.c)
    cpkt_use_local_runtime(cpkt_audio_live_vox_static_c89_example)
    target_compile_options(cpkt_audio_live_vox_static_c89_example PRIVATE
      -std=c89)
    cpkt_add_repo_warning_errors(cpkt_audio_live_vox_static_c89_example)
    target_link_libraries(cpkt_audio_live_vox_static_c89_example PRIVATE
      cpkt_audio_static)
    target_link_options(cpkt_audio_live_vox_static_c89_example PRIVATE
      -static -Wl,--gc-sections)
  else()
    file(REMOVE
      "${CMAKE_CURRENT_BINARY_DIR}/cpkt_audio_live_vox_static_c89_example${CMAKE_EXECUTABLE_SUFFIX}")
  endif()

  add_executable(cpkt_sus_vox_intro_c89_example
    examples/sus-vox-intro-c89/main.c)
  cpkt_use_local_runtime(cpkt_sus_vox_intro_c89_example)
  target_compile_options(cpkt_sus_vox_intro_c89_example PRIVATE -std=c89)
  cpkt_add_repo_warning_errors(cpkt_sus_vox_intro_c89_example)
  target_link_libraries(cpkt_sus_vox_intro_c89_example PRIVATE
    cpkt_audio_static
    cpkt_sus_shared)
  add_executable(cpkt_sus_live_vox_c89_example
    examples/sus-live-vox-c89/main.c)
  cpkt_use_local_runtime(cpkt_sus_live_vox_c89_example)
  target_compile_options(cpkt_sus_live_vox_c89_example PRIVATE -std=c89)
  cpkt_add_repo_warning_errors(cpkt_sus_live_vox_c89_example)
  target_link_libraries(cpkt_sus_live_vox_c89_example PRIVATE
    cpkt_audio_shared
    cpkt_sus_shared)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CPKT_TARGET_LIBC STREQUAL "musl")
    add_executable(cpkt_sus_live_vox_static_c89_example
      examples/sus-live-vox-c89/main.c)
    cpkt_use_local_runtime(cpkt_sus_live_vox_static_c89_example)
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
  else()
    file(REMOVE
      "${CMAKE_CURRENT_BINARY_DIR}/cpkt_sus_live_vox_static_c89_example${CMAKE_EXECUTABLE_SUFFIX}")
  endif()

  add_executable(cpkt_abi_smoke_shared examples/abi_smoke.c)
  cpkt_use_local_runtime(cpkt_abi_smoke_shared)
  target_compile_options(cpkt_abi_smoke_shared PRIVATE -std=c99)
  target_link_libraries(cpkt_abi_smoke_shared PRIVATE
    cpkt::curl_shared
    cpkt::libxml2_shared
    cpkt::lua_shared
    cpkt::miniaudio_shared
    cpkt::open62541_shared
  )

  add_executable(cpkt_abi_smoke_static examples/abi_smoke.c)
  cpkt_use_local_runtime(cpkt_abi_smoke_static)
  target_compile_options(cpkt_abi_smoke_static PRIVATE -std=c99)
  target_link_libraries(cpkt_abi_smoke_static PRIVATE
    cpkt::curl_static
    cpkt::libxml2_static
    cpkt::lua_static
    cpkt::miniaudio_static
    cpkt::open62541_static
  )
  add_executable(cpkt_mqttc_smoke_shared examples/mqttc_smoke.c)
  cpkt_use_local_runtime(cpkt_mqttc_smoke_shared)
  target_compile_options(cpkt_mqttc_smoke_shared PRIVATE -std=c99)
  target_link_libraries(cpkt_mqttc_smoke_shared PRIVATE cpkt::mqttc_shared)

  add_executable(cpkt_mqttc_smoke_static examples/mqttc_smoke.c)
  cpkt_use_local_runtime(cpkt_mqttc_smoke_static)
  target_compile_options(cpkt_mqttc_smoke_static PRIVATE -std=c99)
  target_link_libraries(cpkt_mqttc_smoke_static PRIVATE cpkt::mqttc_static)
  add_executable(cpkt_whisper_smoke_shared examples/whisper_smoke.c)
  cpkt_use_local_runtime(cpkt_whisper_smoke_shared)
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
    cpkt_opcua_static
    cpkt_gssapi_static
    cpkt_sqlite_static)
  set_target_properties(cpkt_static_archive_pic_smoke_facades PROPERTIES
    LINKER_LANGUAGE CXX)
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
    cpkt_use_local_runtime(cpkt_abi_test_shared)
    target_link_libraries(cpkt_abi_test_shared PRIVATE
      cpkt::curl_shared
      cpkt::libxml2_shared
      cpkt::lua_shared
      cpkt::miniaudio_shared
      cpkt::open62541_shared
      cpkt::cmocka
    )

    add_executable(cpkt_abi_test_static tests/abi_smoke_test.c)
    cpkt_use_local_runtime(cpkt_abi_test_static)
    target_link_libraries(cpkt_abi_test_static PRIVATE
      cpkt::curl_static
      cpkt::libxml2_static
      cpkt::lua_static
      cpkt::miniaudio_static
      cpkt::open62541_static
      cpkt::cmocka
    )
    add_executable(cpkt_audio_facade_test tests/audio_facade_test.c)
    cpkt_use_local_runtime(cpkt_audio_facade_test)
    target_compile_options(cpkt_audio_facade_test PRIVATE -std=c99)
    cpkt_add_repo_warning_errors(cpkt_audio_facade_test)
    target_link_libraries(cpkt_audio_facade_test PRIVATE
      cpkt_audio_static
      cpkt::cmocka
    )
    add_executable(cpkt_audio_process_capture_probe
      tests/audio_process_capture_probe.c)
    cpkt_use_local_runtime(cpkt_audio_process_capture_probe)
    target_compile_options(cpkt_audio_process_capture_probe PRIVATE -std=c99)
    cpkt_add_repo_warning_errors(cpkt_audio_process_capture_probe)
    target_link_libraries(cpkt_audio_process_capture_probe PRIVATE
      cpkt_audio_static)
    add_executable(cpkt_audio_process_playback_probe
      tests/audio_process_playback_probe.c)
    cpkt_use_local_runtime(cpkt_audio_process_playback_probe)
    target_compile_options(cpkt_audio_process_playback_probe PRIVATE -std=c99)
    cpkt_add_repo_warning_errors(cpkt_audio_process_playback_probe)
    target_link_libraries(cpkt_audio_process_playback_probe PRIVATE
      cpkt_audio_static)
    add_executable(cpkt_sus_facade_test tests/sus_facade_test.c)
    cpkt_use_local_runtime(cpkt_sus_facade_test)
    target_compile_options(cpkt_sus_facade_test PRIVATE -std=c99)
    cpkt_add_repo_warning_errors(cpkt_sus_facade_test)
    target_link_libraries(cpkt_sus_facade_test PRIVATE
      cpkt_sus_shared
      cpkt::cmocka
    )
    add_executable(cpkt_sus_audio_integration_test
      tests/sus_audio_integration_test.c)
    cpkt_use_local_runtime(cpkt_sus_audio_integration_test)
    target_compile_options(cpkt_sus_audio_integration_test PRIVATE -std=c99)
    cpkt_add_repo_warning_errors(cpkt_sus_audio_integration_test)
    target_link_libraries(cpkt_sus_audio_integration_test PRIVATE
      cpkt_audio_static
      cpkt_sus_shared
    )
    add_executable(cpkt_postgres_receiver_test tests/postgres_receiver_test.c)
    cpkt_use_local_runtime(cpkt_postgres_receiver_test)
    target_compile_options(cpkt_postgres_receiver_test PRIVATE -std=c89)
    cpkt_add_repo_warning_errors(cpkt_postgres_receiver_test)
    target_link_libraries(cpkt_postgres_receiver_test PRIVATE cpkt_postgres_static)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      add_executable(cpkt_postgres_oauth_cleanup_test
        tests/postgres_oauth_cleanup_test.c)
      cpkt_use_local_runtime(cpkt_postgres_oauth_cleanup_test)
      cpkt_configure_c89_target(cpkt_postgres_oauth_cleanup_test)
      cpkt_add_repo_warning_errors(cpkt_postgres_oauth_cleanup_test)
      target_link_options(cpkt_postgres_oauth_cleanup_test PRIVATE
        "-Wl,--wrap=calloc")
      target_link_libraries(cpkt_postgres_oauth_cleanup_test PRIVATE
        cpkt_postgres_static cpkt::postgresql_static)
    endif()
    add_executable(cpkt_postgres_integration_test tests/postgres_integration_test.c)
    cpkt_use_local_runtime(cpkt_postgres_integration_test)
    target_compile_options(cpkt_postgres_integration_test PRIVATE -std=c89)
    cpkt_add_repo_warning_errors(cpkt_postgres_integration_test)
    target_link_libraries(cpkt_postgres_integration_test PRIVATE cpkt_postgres_static)
    add_executable(cpkt_postgres_oauth_module_test
      tests/postgres_oauth_module_test.c)
    if(CPKT_BUILD_DEPENDENCIES)
      add_dependencies(cpkt_postgres_oauth_module_test cpkt_postgresql_project)
    endif()
    cpkt_use_local_runtime(cpkt_postgres_oauth_module_test)
    target_compile_options(cpkt_postgres_oauth_module_test PRIVATE -std=c89)
    cpkt_add_repo_warning_errors(cpkt_postgres_oauth_module_test)
    target_link_libraries(cpkt_postgres_oauth_module_test PRIVATE
      ${CMAKE_DL_LIBS} cpkt::postgresql_shared cpkt::curl_shared)
    # Dependency staging keeps curl and its closure in separate prefixes.
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      set(_cpkt_postgresql_oauth_staging_paths)
      foreach(_cpkt_postgresql_oauth_dependency IN ITEMS
          cpkt::curl_shared cpkt::zlib_shared cpkt::openssl_ssl_shared
          cpkt::libssh2_shared cpkt::nghttp2_shared cpkt::gssapi_krb5_shared
          cpkt::openldap_shared cpkt::cyrus_sasl_shared)
        get_target_property(_cpkt_postgresql_oauth_dependency_location
          ${_cpkt_postgresql_oauth_dependency} IMPORTED_LOCATION)
        get_filename_component(_cpkt_postgresql_oauth_dependency_dir
          "${_cpkt_postgresql_oauth_dependency_location}" DIRECTORY)
        list(APPEND _cpkt_postgresql_oauth_staging_paths
          "${_cpkt_postgresql_oauth_dependency_dir}")
      endforeach()
      list(JOIN _cpkt_postgresql_oauth_staging_paths ":"
        _cpkt_postgresql_oauth_staging_rpath)
      target_link_options(cpkt_postgres_oauth_module_test PRIVATE
        LINKER:--disable-new-dtags
        "LINKER:-rpath,${_cpkt_postgresql_oauth_staging_rpath}")
    endif()
    add_executable(cpkt_gssapi_facade_test tests/gssapi_facade_test.c)
    cpkt_use_local_runtime(cpkt_gssapi_facade_test)
    target_compile_options(cpkt_gssapi_facade_test PRIVATE -std=c89)
    cpkt_add_repo_warning_errors(cpkt_gssapi_facade_test)
    target_link_libraries(cpkt_gssapi_facade_test PRIVATE cpkt_gssapi_static)
    add_executable(cpkt_openssl_facade_test tests/openssl_facade_test.c)
    cpkt_use_local_runtime(cpkt_openssl_facade_test)
    target_compile_options(cpkt_openssl_facade_test PRIVATE -std=c89)
    cpkt_add_repo_warning_errors(cpkt_openssl_facade_test)
    target_link_libraries(cpkt_openssl_facade_test PRIVATE cpkt_openssl_static)
    add_executable(cpkt_nghttp2_facade_test tests/nghttp2_facade_test.c)
    cpkt_use_local_runtime(cpkt_nghttp2_facade_test)
    target_compile_options(cpkt_nghttp2_facade_test PRIVATE -std=c89)
    cpkt_add_repo_warning_errors(cpkt_nghttp2_facade_test)
    target_link_libraries(cpkt_nghttp2_facade_test PRIVATE cpkt_nghttp2_static)
    add_executable(cpkt_libssh2_header_facade_test
      tests/libssh2_header_facade_test.c)
    add_dependencies(cpkt_libssh2_header_facade_test
      cpkt_libssh2_facade_generated)
    cpkt_use_local_runtime(cpkt_libssh2_header_facade_test)
    target_compile_options(cpkt_libssh2_header_facade_test PRIVATE -std=c89)
    cpkt_add_repo_warning_errors(cpkt_libssh2_header_facade_test)
    target_include_directories(cpkt_libssh2_header_facade_test PRIVATE
      "${CPKT_LIBSSH2_FACADE_INCLUDE_DIR}")
    add_executable(cpkt_libssh2_facade_test tests/libssh2_facade_test.c)
    cpkt_use_local_runtime(cpkt_libssh2_facade_test)
    target_compile_options(cpkt_libssh2_facade_test PRIVATE -std=c89)
    cpkt_add_repo_warning_errors(cpkt_libssh2_facade_test)
    target_link_libraries(cpkt_libssh2_facade_test PRIVATE cpkt_libssh2_static)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      add_executable(cpkt_libssh2_sftp_eof_test
        tests/libssh2_sftp_eof_test.c)
      cpkt_use_local_runtime(cpkt_libssh2_sftp_eof_test)
      target_compile_options(cpkt_libssh2_sftp_eof_test PRIVATE -std=c99)
      target_link_options(cpkt_libssh2_sftp_eof_test PRIVATE
        "-Wl,--wrap=libssh2_sftp_readdir_ex")
      cpkt_add_repo_warning_errors(cpkt_libssh2_sftp_eof_test)
      target_link_libraries(cpkt_libssh2_sftp_eof_test PRIVATE
        cpkt_libssh2_static cpkt::libssh2_static)
    endif()
    add_executable(cpkt_mqttc_header_facade_test
      tests/mqttc_header_facade_test.c)
    add_dependencies(cpkt_mqttc_header_facade_test cpkt_mqttc_facade_generated)
    cpkt_use_local_runtime(cpkt_mqttc_header_facade_test)
    target_compile_options(cpkt_mqttc_header_facade_test PRIVATE -std=c89)
    cpkt_add_repo_warning_errors(cpkt_mqttc_header_facade_test)
    target_include_directories(cpkt_mqttc_header_facade_test PRIVATE
      "${CPKT_MQTTC_FACADE_INCLUDE_DIR}")
    add_executable(cpkt_mqttc_facade_test tests/mqttc_facade_test.c)
    cpkt_use_local_runtime(cpkt_mqttc_facade_test)
    target_compile_options(cpkt_mqttc_facade_test PRIVATE -std=c89)
    cpkt_add_repo_warning_errors(cpkt_mqttc_facade_test)
    target_link_libraries(cpkt_mqttc_facade_test PRIVATE cpkt_mqttc_static)
    add_executable(cpkt_lua_facade_test tests/lua_facade_test.c)
    cpkt_use_local_runtime(cpkt_lua_facade_test)
    cpkt_configure_c89_target(cpkt_lua_facade_test)
    cpkt_add_repo_warning_errors(cpkt_lua_facade_test)
    target_link_libraries(cpkt_lua_facade_test PRIVATE cpkt_lua_static)
    add_executable(cpkt_sasl_facade_test tests/sasl_facade_test.c)
    cpkt_use_local_runtime(cpkt_sasl_facade_test)
    target_compile_options(cpkt_sasl_facade_test PRIVATE -std=c89)
    cpkt_add_repo_warning_errors(cpkt_sasl_facade_test)
    target_link_libraries(cpkt_sasl_facade_test PRIVATE cpkt_sasl_static)
    add_executable(cpkt_sasl_privileged_plugin_path_test
      tests/sasl_privileged_plugin_path_test.c)
    cpkt_use_local_runtime(cpkt_sasl_privileged_plugin_path_test)
    target_compile_options(cpkt_sasl_privileged_plugin_path_test PRIVATE
      -std=c89)
    cpkt_add_repo_warning_errors(cpkt_sasl_privileged_plugin_path_test)
    target_link_libraries(cpkt_sasl_privileged_plugin_path_test PRIVATE
      cpkt::cyrus_sasl_shared cpkt::gssapi_krb5_shared)
    add_executable(cpkt_sasl_callback_retry_test
      tests/sasl_callback_retry_test.c
      src/sasl.c)
    if(TARGET cpkt_cyrus_sasl_project)
      add_dependencies(cpkt_sasl_callback_retry_test cpkt_cyrus_sasl_project)
    endif()
    cpkt_use_local_runtime(cpkt_sasl_callback_retry_test)
    get_target_property(cpkt_sasl_native_include_dir cpkt::cyrus_sasl_static
      INTERFACE_INCLUDE_DIRECTORIES)
    target_include_directories(cpkt_sasl_callback_retry_test PRIVATE
      "${CMAKE_SOURCE_DIR}/include"
      "${cpkt_sasl_native_include_dir}")
    target_compile_options(cpkt_sasl_callback_retry_test PRIVATE -std=c89)
    cpkt_add_repo_warning_errors(cpkt_sasl_callback_retry_test)
    add_executable(cpkt_iodbc_static_test tests/iodbc_driver_manager_test.c)
    cpkt_use_local_runtime(cpkt_iodbc_static_test)
    cpkt_configure_c89_target(cpkt_iodbc_static_test)
    cpkt_add_repo_warning_errors(cpkt_iodbc_static_test)
    target_link_libraries(cpkt_iodbc_static_test PRIVATE cpkt::iodbc)
    add_executable(cpkt_iodbc_shared_test tests/iodbc_driver_manager_test.c)
    cpkt_use_local_runtime(cpkt_iodbc_shared_test)
    cpkt_configure_c89_target(cpkt_iodbc_shared_test)
    cpkt_add_repo_warning_errors(cpkt_iodbc_shared_test)
    target_link_libraries(cpkt_iodbc_shared_test PRIVATE
      cpkt::iodbc_shared cpkt::iodbcinst_shared)
    add_executable(cpkt_pdf_facade_static_test tests/pdf_facade_test.c)
    cpkt_use_local_runtime(cpkt_pdf_facade_static_test)
    target_compile_options(cpkt_pdf_facade_static_test PRIVATE -std=c89)
    cpkt_add_repo_warning_errors(cpkt_pdf_facade_static_test)
    target_link_libraries(cpkt_pdf_facade_static_test PRIVATE cpkt_pdf_static cpkt::png_static)
    add_executable(cpkt_pdf_facade_shared_test tests/pdf_facade_test.c)
    cpkt_use_local_runtime(cpkt_pdf_facade_shared_test)
    target_compile_options(cpkt_pdf_facade_shared_test PRIVATE -std=c89)
    cpkt_add_repo_warning_errors(cpkt_pdf_facade_shared_test)
    target_link_libraries(cpkt_pdf_facade_shared_test PRIVATE cpkt_pdf_shared cpkt::png_shared)
    add_executable(cpkt_sqlite_facade_test tests/sqlite_facade_test.c)
    cpkt_use_local_runtime(cpkt_sqlite_facade_test)
    target_compile_options(cpkt_sqlite_facade_test PRIVATE -std=c89)
    cpkt_add_repo_warning_errors(cpkt_sqlite_facade_test)
    target_link_libraries(cpkt_sqlite_facade_test PRIVATE cpkt_sqlite_static)
    add_executable(cpkt_sqlite_rebase_collection_test
      tests/sqlite_rebase_collection_test.c)
    cpkt_use_local_runtime(cpkt_sqlite_rebase_collection_test)
    target_compile_options(cpkt_sqlite_rebase_collection_test PRIVATE -std=c89)
    cpkt_add_repo_warning_errors(cpkt_sqlite_rebase_collection_test)
    target_link_libraries(cpkt_sqlite_rebase_collection_test PRIVATE
      cpkt_sqlite_static)
    add_executable(cpkt_sqlite_changeset_null_conflict_test
      tests/sqlite_changeset_null_conflict_test.c)
    cpkt_use_local_runtime(cpkt_sqlite_changeset_null_conflict_test)
    cpkt_configure_c89_target(cpkt_sqlite_changeset_null_conflict_test)
    cpkt_add_repo_warning_errors(cpkt_sqlite_changeset_null_conflict_test)
    target_link_libraries(cpkt_sqlite_changeset_null_conflict_test PRIVATE
      cpkt_sqlite_static)
    add_executable(cpkt_sqlite_vfs_wal_capability_test
      tests/sqlite_vfs_wal_capability_test.c)
    cpkt_use_local_runtime(cpkt_sqlite_vfs_wal_capability_test)
    target_compile_options(cpkt_sqlite_vfs_wal_capability_test PRIVATE -std=c99)
    cpkt_add_repo_warning_errors(cpkt_sqlite_vfs_wal_capability_test)
    target_link_libraries(cpkt_sqlite_vfs_wal_capability_test PRIVATE
      cpkt_sqlite_static cpkt::sqlite_static)
    add_executable(cpkt_sqlite_child_lifetime_concurrency_test
      tests/sqlite_child_lifetime_concurrency_test.c)
    cpkt_use_local_runtime(cpkt_sqlite_child_lifetime_concurrency_test)
    target_compile_options(cpkt_sqlite_child_lifetime_concurrency_test PRIVATE -std=c89)
    cpkt_add_repo_warning_errors(cpkt_sqlite_child_lifetime_concurrency_test)
    target_link_libraries(cpkt_sqlite_child_lifetime_concurrency_test PRIVATE
      cpkt_sqlite_static Threads::Threads)
    add_executable(cpkt_sqlite_callback_synchronization_test
      tests/sqlite_callback_synchronization_test.c)
    cpkt_use_local_runtime(cpkt_sqlite_callback_synchronization_test)
    target_compile_options(cpkt_sqlite_callback_synchronization_test
      PRIVATE -std=c89)
    cpkt_add_repo_warning_errors(cpkt_sqlite_callback_synchronization_test)
    target_link_libraries(cpkt_sqlite_callback_synchronization_test PRIVATE
      cpkt_sqlite_static Threads::Threads)
    add_executable(cpkt_sqlite_registration_concurrency_test
      tests/sqlite_registration_concurrency_test.c)
    cpkt_use_local_runtime(cpkt_sqlite_registration_concurrency_test)
    cpkt_configure_c89_target(cpkt_sqlite_registration_concurrency_test)
    cpkt_add_repo_warning_errors(cpkt_sqlite_registration_concurrency_test)
    target_link_libraries(cpkt_sqlite_registration_concurrency_test PRIVATE
      cpkt_sqlite_static Threads::Threads)
    add_executable(cpkt_sqlite_fts5_registration_concurrency_test
      tests/sqlite_fts5_registration_concurrency_test.c)
    cpkt_use_local_runtime(cpkt_sqlite_fts5_registration_concurrency_test)
    cpkt_configure_c89_target(cpkt_sqlite_fts5_registration_concurrency_test)
    cpkt_add_repo_warning_errors(cpkt_sqlite_fts5_registration_concurrency_test)
    target_link_libraries(cpkt_sqlite_fts5_registration_concurrency_test PRIVATE
      cpkt_sqlite_static Threads::Threads)
    add_executable(cpkt_sqlite_window_aggregate_test
      tests/sqlite_window_aggregate_test.c)
    cpkt_use_local_runtime(cpkt_sqlite_window_aggregate_test)
    cpkt_configure_c89_target(cpkt_sqlite_window_aggregate_test)
    cpkt_add_repo_warning_errors(cpkt_sqlite_window_aggregate_test)
    target_link_libraries(cpkt_sqlite_window_aggregate_test PRIVATE
      cpkt_sqlite_static)
    add_executable(cpkt_sqlite_unlock_notify_groups_test
      tests/sqlite_unlock_notify_groups_test.c)
    cpkt_use_local_runtime(cpkt_sqlite_unlock_notify_groups_test)
    cpkt_configure_c89_target(cpkt_sqlite_unlock_notify_groups_test)
    cpkt_add_repo_warning_errors(cpkt_sqlite_unlock_notify_groups_test)
    target_link_libraries(cpkt_sqlite_unlock_notify_groups_test PRIVATE
      cpkt_sqlite_static)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      add_executable(cpkt_sqlite_unlock_notify_race_test
        tests/sqlite_unlock_notify_race_test.c)
      cpkt_use_local_runtime(cpkt_sqlite_unlock_notify_race_test)
      cpkt_configure_c89_target(cpkt_sqlite_unlock_notify_race_test)
      cpkt_add_repo_warning_errors(cpkt_sqlite_unlock_notify_race_test)
      target_link_options(cpkt_sqlite_unlock_notify_race_test PRIVATE
        "-Wl,--wrap=sqlite3_unlock_notify")
      target_link_libraries(cpkt_sqlite_unlock_notify_race_test PRIVATE
        cpkt_sqlite_static Threads::Threads)
      add_executable(cpkt_sqlite_auto_extension_lock_order_test
        tests/sqlite_auto_extension_lock_order_test.c)
      cpkt_use_local_runtime(cpkt_sqlite_auto_extension_lock_order_test)
      cpkt_configure_c89_target(cpkt_sqlite_auto_extension_lock_order_test)
      cpkt_add_repo_warning_errors(cpkt_sqlite_auto_extension_lock_order_test)
      target_link_options(cpkt_sqlite_auto_extension_lock_order_test PRIVATE
        "-Wl,--wrap=sqlite3_auto_extension"
        "-Wl,--wrap=sqlite3_cancel_auto_extension")
      target_link_libraries(cpkt_sqlite_auto_extension_lock_order_test PRIVATE
        cpkt_sqlite_static Threads::Threads)
    endif()
    add_executable(cpkt_sqlite_page_cache_lifetime_test
      tests/sqlite_page_cache_lifetime_test.c)
    cpkt_use_local_runtime(cpkt_sqlite_page_cache_lifetime_test)
    target_compile_options(cpkt_sqlite_page_cache_lifetime_test PRIVATE -std=c89)
    cpkt_add_repo_warning_errors(cpkt_sqlite_page_cache_lifetime_test)
    target_link_libraries(cpkt_sqlite_page_cache_lifetime_test PRIVATE
      cpkt_sqlite_static)
    add_executable(cpkt_sqlite_auto_extension_shutdown_test
      tests/sqlite_auto_extension_shutdown_test.c)
    cpkt_use_local_runtime(cpkt_sqlite_auto_extension_shutdown_test)
    target_compile_options(cpkt_sqlite_auto_extension_shutdown_test PRIVATE -std=c89)
    cpkt_add_repo_warning_errors(cpkt_sqlite_auto_extension_shutdown_test)
    target_link_libraries(cpkt_sqlite_auto_extension_shutdown_test PRIVATE
      cpkt_sqlite_static)
    add_executable(cpkt_sqlite_rtree_registration_failure_test
      tests/sqlite_rtree_registration_failure_test.c)
    cpkt_use_local_runtime(cpkt_sqlite_rtree_registration_failure_test)
    target_compile_options(cpkt_sqlite_rtree_registration_failure_test PRIVATE -std=c89)
    cpkt_add_repo_warning_errors(cpkt_sqlite_rtree_registration_failure_test)
    target_link_libraries(cpkt_sqlite_rtree_registration_failure_test PRIVATE
      cpkt_sqlite_static)
    add_executable(cpkt_sqlite_session_hook_guard_test
      tests/sqlite_session_hook_guard_test.c)
    cpkt_use_local_runtime(cpkt_sqlite_session_hook_guard_test)
    target_compile_options(cpkt_sqlite_session_hook_guard_test PRIVATE -std=c89)
    cpkt_add_repo_warning_errors(cpkt_sqlite_session_hook_guard_test)
    target_link_libraries(cpkt_sqlite_session_hook_guard_test PRIVATE
      cpkt_sqlite_static)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      add_executable(cpkt_sqlite_page_cache_discard_test
        tests/sqlite_page_cache_discard_test.c)
      cpkt_use_local_runtime(cpkt_sqlite_page_cache_discard_test)
      target_compile_options(cpkt_sqlite_page_cache_discard_test PRIVATE -std=c99)
      target_link_options(cpkt_sqlite_page_cache_discard_test PRIVATE
        "-Wl,--wrap=calloc" "-Wl,--wrap=free")
      cpkt_add_repo_warning_errors(cpkt_sqlite_page_cache_discard_test)
      target_link_libraries(cpkt_sqlite_page_cache_discard_test PRIVATE
        cpkt_sqlite_static cpkt::sqlite_static)
    endif()
    add_executable(cpkt_sqlite_collation_failure_test
      tests/sqlite_collation_failure_test.c)
    cpkt_use_local_runtime(cpkt_sqlite_collation_failure_test)
    target_compile_options(cpkt_sqlite_collation_failure_test PRIVATE -std=c89)
    cpkt_add_repo_warning_errors(cpkt_sqlite_collation_failure_test)
    target_link_libraries(cpkt_sqlite_collation_failure_test PRIVATE
      cpkt_sqlite_static)
    add_executable(cpkt_lua_runtime_test tests/lua_runtime_test.c)
    cpkt_use_local_runtime(cpkt_lua_runtime_test)
    target_compile_options(cpkt_lua_runtime_test PRIVATE -std=c99)
    cpkt_add_repo_warning_errors(cpkt_lua_runtime_test)
    target_link_libraries(cpkt_lua_runtime_test PRIVATE
      cpkt_lua_runtime_static
      cpkt::cmocka
    )

    add_executable(cpkt_opcua_facade_test
      tests/opcua_facade_test.c
      tests/opcua_c89_boundary_peer.c)
    cpkt_use_local_runtime(cpkt_opcua_facade_test)
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
  endif()

  if(CPKT_CAN_RUN_TARGET_EXECUTABLES)
    add_test(NAME curl_server_bind
      COMMAND "${CPKT_PYTHON3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/curl_async_dns_server_test.py")
    set_tests_properties(curl_server_bind PROPERTIES LABELS "integration;smoke" TIMEOUT 10)
    foreach(_cpkt_curl_linkage IN ITEMS static shared)
      add_test(NAME curl_async_dns_${_cpkt_curl_linkage}
        COMMAND cpkt_curl_async_dns_${_cpkt_curl_linkage}_test)
      add_test(NAME curl_multi_socket_${_cpkt_curl_linkage}
        COMMAND "${CPKT_PYTHON3_EXECUTABLE}"
          "${CMAKE_SOURCE_DIR}/tests/curl_async_dns_server.py"
          "$<TARGET_FILE:cpkt_curl_async_dns_${_cpkt_curl_linkage}_test>")
      set_tests_properties(curl_async_dns_${_cpkt_curl_linkage}
        curl_multi_socket_${_cpkt_curl_linkage} PROPERTIES
        LABELS "integration;smoke" TIMEOUT 30)
    endforeach()
    add_test(NAME lua_facade_unit COMMAND cpkt_lua_facade_test)
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
      if(NOT CPKT_NATIVE_BOOTLIN_RUNTIME)
        set_tests_properties(sus_live_vox_static_c89_example PROPERTIES
          ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cpkt_sus_shared>:${CPKT_EXTERNAL_ROOT}/whisper/install/lib:${CPKT_EXTERNAL_ROOT}/curl/install/lib:${CPKT_EXTERNAL_ROOT}/openssl/install/lib:${CPKT_EXTERNAL_ROOT}/nghttp2/install/lib:${CPKT_EXTERNAL_ROOT}/libssh2/install/lib:${CPKT_EXTERNAL_ROOT}/zlib/install/lib")
      endif()
    endif()
    add_test(
      NAME sus_live_startup_status
      COMMAND
        "${CMAKE_SOURCE_DIR}/tests/sus_live_startup_status_test.sh"
        ${CPKT_TEST_EXECUTABLE_PREFIX}
        "$<TARGET_FILE:cpkt_sus_live_vox_c89_example>")
    if(NOT CPKT_NATIVE_BOOTLIN_RUNTIME)
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
    endif()
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
    elseif(NOT CPKT_NATIVE_BOOTLIN_RUNTIME)
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
      if(NOT CPKT_NATIVE_BOOTLIN_RUNTIME)
        set_tests_properties(sus_facade_unit PROPERTIES
          ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cpkt_sus_shared>:${CPKT_EXTERNAL_ROOT}/whisper/install/lib:${CPKT_EXTERNAL_ROOT}/curl/install/lib:${CPKT_EXTERNAL_ROOT}/openssl/install/lib:${CPKT_EXTERNAL_ROOT}/nghttp2/install/lib:${CPKT_EXTERNAL_ROOT}/libssh2/install/lib:${CPKT_EXTERNAL_ROOT}/zlib/install/lib")
      endif()
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
        LABELS "integration;opt-in;sus")
      if(NOT CPKT_NATIVE_BOOTLIN_RUNTIME)
        set_tests_properties(sus_audio_integration PROPERTIES
          ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cpkt_sus_shared>:${CPKT_EXTERNAL_ROOT}/whisper/install/lib:${CPKT_EXTERNAL_ROOT}/curl/install/lib:${CPKT_EXTERNAL_ROOT}/openssl/install/lib:${CPKT_EXTERNAL_ROOT}/nghttp2/install/lib:${CPKT_EXTERNAL_ROOT}/libssh2/install/lib:${CPKT_EXTERNAL_ROOT}/zlib/install/lib")
      endif()
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
      add_test(NAME postgres_receiver_shell COMMAND cpkt_postgres_receiver_test)
      if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME postgres_oauth_cleanup
          COMMAND cpkt_postgres_oauth_cleanup_test)
      endif()
      string(REGEX MATCH "^[0-9]+" _cpkt_postgresql_oauth_major
        "${CPKT_POSTGRESQL_VERSION}")
      get_target_property(_cpkt_postgresql_shared_location
        cpkt::postgresql_shared IMPORTED_LOCATION)
      get_filename_component(_cpkt_postgresql_library_dir
        "${_cpkt_postgresql_shared_location}" DIRECTORY)
      add_test(NAME postgres_oauth_module
        COMMAND cpkt_postgres_oauth_module_test
          "${_cpkt_postgresql_library_dir}/libpq-oauth-${_cpkt_postgresql_oauth_major}${CMAKE_SHARED_MODULE_SUFFIX}")
      add_test(NAME openssl_facade COMMAND cpkt_openssl_facade_test)
      add_test(NAME nghttp2_facade COMMAND cpkt_nghttp2_facade_test)
      add_test(NAME libssh2_header_facade COMMAND cpkt_libssh2_header_facade_test)
      add_test(NAME libssh2_facade COMMAND cpkt_libssh2_facade_test)
      if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME libssh2_sftp_eof COMMAND cpkt_libssh2_sftp_eof_test)
      endif()
      add_test(NAME mqttc_header_facade COMMAND cpkt_mqttc_header_facade_test)
      add_test(NAME mqttc_facade COMMAND cpkt_mqttc_facade_test)
      add_test(NAME sqlite_facade COMMAND cpkt_sqlite_facade_test)
      add_test(NAME sqlite_rebase_collection
        COMMAND cpkt_sqlite_rebase_collection_test)
      add_test(NAME sqlite_changeset_null_conflict
        COMMAND cpkt_sqlite_changeset_null_conflict_test)
      add_test(NAME sqlite_vfs_wal_capability
        COMMAND cpkt_sqlite_vfs_wal_capability_test
          "${CMAKE_CURRENT_BINARY_DIR}/sqlite-vfs-no-shm.db")
      add_test(NAME iodbc_static COMMAND cpkt_iodbc_static_test)
      add_test(NAME iodbc_shared COMMAND cpkt_iodbc_shared_test)
      set_tests_properties(iodbc_static iodbc_shared PROPERTIES
        LABELS "integration;memcheck")
      add_test(NAME pdf_facade_static COMMAND cpkt_pdf_facade_static_test)
      add_test(NAME pdf_facade_shared COMMAND cpkt_pdf_facade_shared_test)
      if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME pdf_api_surface COMMAND python3
          "${CMAKE_SOURCE_DIR}/tests/pdf_api_surface_test.py"
          "${CPKT_DEPENDENCY_BUILD_ROOT}/libharu/src/include/hpdf.h"
          "${CMAKE_SOURCE_DIR}/include/cpkt/pdf.h"
          "$<TARGET_FILE:cpkt_pdf_shared>"
          "${CMAKE_NM}"
          "${CMAKE_SOURCE_DIR}/cmake/exports/cpkt_pdf.txt")
      endif()
      add_test(NAME sqlite_child_lifetime_concurrency
        COMMAND cpkt_sqlite_child_lifetime_concurrency_test)
      add_test(NAME sqlite_callback_synchronization
        COMMAND cpkt_sqlite_callback_synchronization_test)
      set_tests_properties(sqlite_callback_synchronization PROPERTIES TIMEOUT 15)
      add_test(NAME sqlite_registration_concurrency
        COMMAND cpkt_sqlite_registration_concurrency_test)
      set_tests_properties(sqlite_registration_concurrency PROPERTIES TIMEOUT 15)
      add_test(NAME sqlite_fts5_registration_concurrency
        COMMAND cpkt_sqlite_fts5_registration_concurrency_test)
      set_tests_properties(sqlite_fts5_registration_concurrency PROPERTIES
        TIMEOUT 30)
      add_test(NAME sqlite_window_aggregate
        COMMAND cpkt_sqlite_window_aggregate_test)
      add_test(NAME sqlite_unlock_notify_groups
        COMMAND cpkt_sqlite_unlock_notify_groups_test)
      if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME sqlite_unlock_notify_race
          COMMAND cpkt_sqlite_unlock_notify_race_test)
        set_tests_properties(sqlite_unlock_notify_race PROPERTIES TIMEOUT 15)
      endif()
      add_test(NAME sqlite_page_cache_lifetime
        COMMAND cpkt_sqlite_page_cache_lifetime_test)
      add_test(NAME sqlite_auto_extension_shutdown
        COMMAND cpkt_sqlite_auto_extension_shutdown_test)
      if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME sqlite_auto_extension_lock_order
          COMMAND cpkt_sqlite_auto_extension_lock_order_test)
        set_tests_properties(sqlite_auto_extension_lock_order PROPERTIES
          TIMEOUT 15)
      endif()
      add_test(NAME sqlite_rtree_registration_failure
        COMMAND cpkt_sqlite_rtree_registration_failure_test)
      add_test(NAME sqlite_session_hook_guard
        COMMAND cpkt_sqlite_session_hook_guard_test)
      if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME sqlite_page_cache_discard
          COMMAND cpkt_sqlite_page_cache_discard_test)
      endif()
      add_test(NAME sqlite_collation_failure
        COMMAND cpkt_sqlite_collation_failure_test)
      add_test(
        NAME postgres_e2e_harness
        COMMAND bash "${CMAKE_SOURCE_DIR}/tests/postgres_e2e_harness_test.sh")
      add_test(NAME devenv_lifecycle
        COMMAND bash "${CMAKE_SOURCE_DIR}/tests/devenv_lifecycle_test.sh"
          "${CMAKE_SOURCE_DIR}")
      add_test(
        NAME postgres_oauth_loader_patch
        COMMAND bash "${CMAKE_SOURCE_DIR}/tests/postgres_oauth_loader_patch_test.sh")
      add_test(
        NAME clean_source_test_scratch
        COMMAND bash "${CMAKE_SOURCE_DIR}/tests/clean_source_test_scratch_test.sh"
                "${CMAKE_SOURCE_DIR}")
      set_tests_properties(clean_source_test_scratch PROPERTIES
        LABELS "packaging;smoke"
        TIMEOUT 30)
      add_test(NAME gssapi_facade COMMAND cpkt_gssapi_facade_test)
      add_test(NAME sasl_facade COMMAND cpkt_sasl_facade_test)
      file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/sasl-empty-plugin-path")
      add_test(NAME sasl_privileged_plugin_path
        COMMAND cpkt_sasl_privileged_plugin_path_test privileged)
      add_test(NAME sasl_ordinary_plugin_path
        COMMAND cpkt_sasl_privileged_plugin_path_test ordinary)
      set_tests_properties(
        sasl_privileged_plugin_path sasl_ordinary_plugin_path PROPERTIES
        ENVIRONMENT "SASL_PATH=${CMAKE_BINARY_DIR}/sasl-empty-plugin-path")
      add_test(NAME sasl_callback_retry COMMAND cpkt_sasl_callback_retry_test)
      set(_cpkt_memcheck_facade_tests
        abi_test_shared abi_test_static
        lua_facade_unit lua_runtime_unit
        audio_facade_unit sus_facade_unit
        opcua_facade_integration
        opcua_boundary_server_c99_client_c89
        opcua_boundary_server_c89_client_c99
        opcua_boundary_server_c89_client_c89
        opcua_facade_failure_modes
        postgres_receiver_shell
        openssl_facade nghttp2_facade libssh2_header_facade libssh2_facade
        mqttc_header_facade mqttc_facade
        sqlite_facade sqlite_rebase_collection sqlite_changeset_null_conflict
        sqlite_vfs_wal_capability
        pdf_facade_static pdf_facade_shared
        sqlite_child_lifetime_concurrency sqlite_page_cache_lifetime
        sqlite_callback_synchronization sqlite_registration_concurrency
        sqlite_fts5_registration_concurrency sqlite_window_aggregate
        sqlite_unlock_notify_groups
        sqlite_auto_extension_shutdown sqlite_rtree_registration_failure
        sqlite_session_hook_guard sqlite_collation_failure
        gssapi_facade sasl_facade sasl_callback_retry)
      if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set_tests_properties(sqlite_unlock_notify_race PROPERTIES
          LABELS "facade;memcheck")
        set_tests_properties(sqlite_auto_extension_lock_order
          postgres_oauth_cleanup PROPERTIES LABELS "facade;memcheck")
      endif()
      if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND _cpkt_memcheck_facade_tests
          libssh2_sftp_eof sqlite_page_cache_discard)
      endif()
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
      foreach(_cpkt_memcheck_test IN LISTS _cpkt_memcheck_facade_tests)
        set_property(TEST ${_cpkt_memcheck_test} APPEND PROPERTY LABELS
          facade memcheck)
      endforeach()
      set_tests_properties(opcua_facade_integration PROPERTIES TIMEOUT 120)
      set_tests_properties(opcua_facade_failure_modes PROPERTIES TIMEOUT 90)
      add_test(NAME lua_runtime_mock_unit COMMAND cpkt_lua_runtime_mock_test)
      set_tests_properties(lua_runtime_mock_unit PROPERTIES LABELS "facade;memcheck")
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
        NAME osxcross_linker_cache_refresh
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/osxcross_linker_cache_refresh_test.sh"
          "${CMAKE_SOURCE_DIR}")
      add_test(
        NAME repo_dependency_cache_policy
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/repo_dependency_cache_policy_test.sh"
          "${CMAKE_SOURCE_DIR}")
      add_test(
        NAME repo_dependency_stale_root
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/repo_dependency_stale_root_test.sh"
          "${CMAKE_SOURCE_DIR}")
      add_test(
        NAME curl_system_truststore
        COMMAND
          "${CMAKE_COMMAND}"
          "-DCPKT_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
          "-DCPKT_TEST_BINARY_DIR=${CMAKE_BINARY_DIR}/curl-system-truststore-fixture"
          -P "${CMAKE_SOURCE_DIR}/tests/curl_system_truststore_test.cmake")
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
        NAME lifecycle_toolchain_resolvers
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/toolchain_resolver_test.sh"
          "${CMAKE_SOURCE_DIR}"
          "${CMAKE_SOURCE_DIR}/skills/pkt-systems-cmake-lifecycle/scripts/cpkt-toolchains.sh")
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
        NAME patch_series_portability
        COMMAND "${CMAKE_COMMAND}"
          "-DCPKT_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
          "-DCPKT_PATCH_TEST_ROOT=${CMAKE_BINARY_DIR}"
          -P "${CMAKE_SOURCE_DIR}/tests/patch_series_portability_test.cmake")
      add_test(
        NAME static_archive_member_cleanup
        COMMAND "${CMAKE_COMMAND}"
          "-DCPKT_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
          "-DCPKT_TEST_BINARY_DIR=${CMAKE_BINARY_DIR}"
          "-DCPKT_TEST_CC=${CMAKE_C_COMPILER}"
          "-DCPKT_TEST_AR=${CMAKE_AR}"
          "-DCPKT_TEST_RANLIB=${CMAKE_RANLIB}"
          -P "${CMAKE_SOURCE_DIR}/tests/static_archive_member_cleanup_test.cmake")
      if(CPKT_BUILD_DEPENDENCIES)
        add_test(
          NAME open62541_openssl_provenance
          COMMAND "${CMAKE_COMMAND}"
            "-DCPKT_OPEN62541_BUILD_ROOT=${CPKT_DEPENDENCY_BUILD_ROOT}/open62541"
            "-DCPKT_OPENSSL_PREFIX=${CPKT_EXTERNAL_ROOT}/openssl/install"
            "-DCPKT_SHARED_LIBRARY_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
            -P "${CMAKE_SOURCE_DIR}/tests/open62541_openssl_provenance_test.cmake")
      endif()
      add_test(
        NAME dependency_cache_disabled
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/dependency_cache_disabled_test.sh"
          "${CMAKE_SOURCE_DIR}"
          "${CMAKE_SOURCE_DIR}/build/dependency-cache-disabled-external"
          "${CMAKE_SOURCE_DIR}/build/dependency-cache-disabled-build")
      add_test(
        NAME darwin_install_name_normalization
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/darwin_install_name_normalization_test.sh"
          "${CMAKE_SOURCE_DIR}"
          "${CMAKE_COMMAND}")
      set_tests_properties(darwin_install_name_normalization PROPERTIES
        LABELS "packaging;smoke"
        TIMEOUT 30)
      if(CPKT_TARGET_ID STREQUAL "x86_64-linux-gnu" AND CPKT_BUILD_DEPENDENCIES)
        add_test(
          NAME krb5_tls_module_smoke
          COMMAND bash "${CMAKE_SOURCE_DIR}/tests/krb5_tls_module_smoke.sh"
            "${CMAKE_SOURCE_DIR}" "${CMAKE_C_COMPILER}"
            "${CPKT_EXTERNAL_ROOT}/krb5/install/lib"
            "${CPKT_EXTERNAL_ROOT}/openssl/install/lib")
        set_tests_properties(krb5_tls_module_smoke PROPERTIES
          LABELS "packaging;smoke"
          TIMEOUT 30)
        add_test(
          NAME dependency_reuse_configure
          COMMAND
            "${CMAKE_SOURCE_DIR}/tests/dependency_reuse_configure_test.sh"
            "${CMAKE_SOURCE_DIR}"
            "${CMAKE_COMMAND}"
            "${CMAKE_GENERATOR}"
            "${CMAKE_MAKE_PROGRAM}"
            "${CMAKE_TOOLCHAIN_FILE}"
            "${CMAKE_BUILD_TYPE}")
        set_tests_properties(dependency_reuse_configure PROPERTIES
          LABELS "packaging;smoke"
          TIMEOUT 60)
      endif()
      add_test(
        NAME privacy_scan_failure
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/privacy_scan_failure_test.sh")
      set_tests_properties(privacy_scan_failure PROPERTIES
        LABELS "packaging"
        TIMEOUT 30)
      add_test(
        NAME dependency_component_targets
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/dependency_component_targets_test.sh"
          "${CMAKE_SOURCE_DIR}")
      add_test(
        NAME krb5_feature_contract
        COMMAND bash "${CMAKE_SOURCE_DIR}/tests/krb5_feature_contract_test.sh")
      set_tests_properties(krb5_feature_contract PROPERTIES
        LABELS "packaging;smoke"
        TIMEOUT 30)
      add_test(
        NAME package_assertions_elf_runpath
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/package_assertions_elf_runpath_test.sh")
      set_tests_properties(package_assertions_elf_runpath PROPERTIES
        LABELS "packaging"
        TIMEOUT 30)
      add_test(
        NAME package_assertions_cleanup
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/package_assertions_cleanup_test.sh"
          "${CMAKE_SOURCE_DIR}")
      set_tests_properties(package_assertions_cleanup PROPERTIES
        LABELS "packaging"
        TIMEOUT 30)
      add_test(
        NAME package_failure_diagnostics
        COMMAND
          python3 "${CMAKE_SOURCE_DIR}/tests/package_failure_diagnostics_test.py"
          "${CMAKE_SOURCE_DIR}")
      set_tests_properties(package_failure_diagnostics PROPERTIES
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
        NAME fuzz_toolchain_policy
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/fuzz_toolchain_policy_test.sh"
          "${CMAKE_SOURCE_DIR}")
      set_tests_properties(fuzz_toolchain_policy PROPERTIES
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
        NAME facade_c89_target_policy
        COMMAND
          bash
          "${CMAKE_SOURCE_DIR}/tests/facade_c89_target_policy_test.sh"
          "${CMAKE_SOURCE_DIR}")
      add_test(
        NAME lua_symbol_test_registration
        COMMAND
          bash
          "${CMAKE_SOURCE_DIR}/tests/lua_symbol_test_registration_test.sh"
          "${CMAKE_SOURCE_DIR}")
      if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(_cpkt_openssl_catalog_symbol_format macho)
        set(_cpkt_openssl_catalog_symbol_tool "${CMAKE_NM}")
      else()
        set(_cpkt_openssl_catalog_symbol_format elf)
        set(_cpkt_openssl_catalog_symbol_tool "${CMAKE_READELF}")
      endif()
      add_test(
        NAME openssl_api_catalog
        COMMAND
          bash
          "${CMAKE_SOURCE_DIR}/tests/openssl_api_catalog_test.sh"
          "${CMAKE_SOURCE_DIR}"
          "${CPKT_DEPENDENCY_BUILD_ROOT}/openssl/src/util/libcrypto.num"
          "${CPKT_DEPENDENCY_BUILD_ROOT}/openssl/src/util/libssl.num"
          "$<TARGET_PROPERTY:cpkt::openssl_crypto_shared,IMPORTED_LOCATION>"
          "$<TARGET_PROPERTY:cpkt::openssl_ssl_shared,IMPORTED_LOCATION>"
          "${_cpkt_openssl_catalog_symbol_format}"
          "${_cpkt_openssl_catalog_symbol_tool}")
      # The inventory generator uses clang only as a declaration parser.  It
      # is deliberately native-host-only and never participates in a target
      # build, package, or release compiler path.
      if(CPKT_TARGET_ID STREQUAL "x86_64-linux-gnu")
        if(CPKT_OPENSSL_API_INVENTORY_CLANG AND
            NOT EXISTS "${CPKT_OPENSSL_API_INVENTORY_CLANG}")
          unset(CPKT_OPENSSL_API_INVENTORY_CLANG CACHE)
        endif()
        find_program(CPKT_OPENSSL_API_INVENTORY_CLANG NAMES clang REQUIRED
          NO_CMAKE_FIND_ROOT_PATH)
        add_test(
          NAME openssl_api_inventory
          COMMAND
            python3
            "${CMAKE_SOURCE_DIR}/tools/generate_openssl_api_inventory.py"
            --include-dir "$<TARGET_PROPERTY:cpkt::openssl_crypto_shared,INTERFACE_INCLUDE_DIRECTORIES>"
            --num "${CPKT_DEPENDENCY_BUILD_ROOT}/openssl/src/util/libcrypto.num"
            --num "${CPKT_DEPENDENCY_BUILD_ROOT}/openssl/src/util/libssl.num"
            --library "$<TARGET_PROPERTY:cpkt::openssl_crypto_shared,IMPORTED_LOCATION>"
            --library "$<TARGET_PROPERTY:cpkt::openssl_ssl_shared,IMPORTED_LOCATION>"
            --symbol-format elf
            --symbol-tool "${CMAKE_READELF}"
            --clang "${CPKT_OPENSSL_API_INVENTORY_CLANG}"
            --output "${CMAKE_BINARY_DIR}/openssl-api-inventory-test/inventory.json"
            --work-dir "${CMAKE_BINARY_DIR}/openssl-api-inventory-test"
            --facade-header "${CMAKE_SOURCE_DIR}/include/cpkt/openssl.h")
        add_test(
          NAME nghttp2_api_inventory
          COMMAND
            python3
            "${CMAKE_SOURCE_DIR}/tools/generate_nghttp2_api_inventory.py"
            --include-dir "$<TARGET_PROPERTY:cpkt::nghttp2_shared,INTERFACE_INCLUDE_DIRECTORIES>"
            --library "$<TARGET_PROPERTY:cpkt::nghttp2_shared,IMPORTED_LOCATION>"
            --symbol-format elf
            --symbol-tool "${CMAKE_READELF}"
            --clang "${CPKT_OPENSSL_API_INVENTORY_CLANG}"
            --output "${CMAKE_BINARY_DIR}/nghttp2-api-inventory-test/inventory.json"
            --work-dir "${CMAKE_BINARY_DIR}/nghttp2-api-inventory-test"
            --facade-header "${CPKT_NGHTTP2_FACADE_HEADER}")
        add_test(
          NAME libssh2_api_inventory
          COMMAND
            python3
            "${CMAKE_SOURCE_DIR}/tools/generate_libssh2_api_inventory.py"
            --include-dir "$<TARGET_PROPERTY:cpkt::libssh2_shared,INTERFACE_INCLUDE_DIRECTORIES>"
            --library "$<TARGET_PROPERTY:cpkt::libssh2_shared,IMPORTED_LOCATION>"
            --symbol-tool "${CMAKE_NM}"
            --clang "${CPKT_OPENSSL_API_INVENTORY_CLANG}"
            --output "${CMAKE_BINARY_DIR}/libssh2-api-inventory-test/inventory.json"
            --work-dir "${CMAKE_BINARY_DIR}/libssh2-api-inventory-test"
            --facade-header "${CPKT_LIBSSH2_FACADE_HEADER}")
      endif()
      add_test(
        NAME openssl_header_facade
        COMMAND
          bash
          "${CMAKE_SOURCE_DIR}/tests/openssl_header_facade_test.sh"
          "${CMAKE_SOURCE_DIR}"
          "${CMAKE_C_COMPILER}"
          "${CMAKE_SYSROOT}"
          "$<TARGET_PROPERTY:cpkt::openssl_crypto_shared,INTERFACE_INCLUDE_DIRECTORIES>")
      add_test(
        NAME openssl_export_policy
        COMMAND
          bash
          "${CMAKE_SOURCE_DIR}/tests/openssl_export_policy_test.sh"
          "$<TARGET_FILE:cpkt_openssl_shared>"
          "${CMAKE_SYSTEM_NAME}"
          "${CMAKE_NM}"
          "${CMAKE_SOURCE_DIR}/cmake/exports/cpkt_openssl.txt"
          "${CMAKE_C_COMPILER}"
          "${CMAKE_SYSROOT}")
      add_test(
        NAME nghttp2_header_facade
        COMMAND
          bash
          "${CMAKE_SOURCE_DIR}/tests/nghttp2_header_facade_test.sh"
          "${CMAKE_SOURCE_DIR}"
          "${CMAKE_C_COMPILER}"
          "${CMAKE_CXX_COMPILER}"
          "${CMAKE_SYSROOT}"
          "${CPKT_NGHTTP2_FACADE_INCLUDE_DIR}")
      add_test(
        NAME nghttp2_export_policy
        COMMAND
          bash
          "${CMAKE_SOURCE_DIR}/tests/nghttp2_export_policy_test.sh"
          "$<TARGET_FILE:cpkt_nghttp2_shared>"
          "${CMAKE_SYSTEM_NAME}"
          "${CMAKE_NM}"
          "${CMAKE_SOURCE_DIR}/cmake/exports/cpkt_nghttp2.txt"
          "${CMAKE_C_COMPILER}"
          "${CMAKE_SYSROOT}")
      add_test(
        NAME libssh2_export_policy
        COMMAND
          bash
          "${CMAKE_SOURCE_DIR}/tests/libssh2_export_policy_test.sh"
          "$<TARGET_FILE:cpkt_libssh2_shared>"
          "${CMAKE_SYSTEM_NAME}"
          "${CMAKE_NM}"
          "${CMAKE_SOURCE_DIR}/cmake/exports/cpkt_libssh2.txt"
          "${CMAKE_C_COMPILER}"
          "${CMAKE_SYSROOT}")
      add_test(
        NAME mqttc_api_inventory
        COMMAND
          "${CPKT_PYTHON3_EXECUTABLE}"
          "${CMAKE_SOURCE_DIR}/tools/generate_mqttc_api_inventory.py"
          --include-dir "$<TARGET_PROPERTY:cpkt::mqttc_static,INTERFACE_INCLUDE_DIRECTORIES>"
          --library "$<TARGET_FILE:cpkt::mqttc_shared>"
          --symbol-tool "${CMAKE_NM}"
          --facade-header "${CPKT_MQTTC_FACADE_HEADER}"
          --output "${CMAKE_CURRENT_BINARY_DIR}/mqttc-api-inventory.json")
      add_test(
        NAME mqttc_export_policy
        COMMAND
          bash
          "${CMAKE_SOURCE_DIR}/tests/mqttc_export_policy_test.sh"
          "$<TARGET_FILE:cpkt_mqttc_shared>"
          "${CMAKE_SYSTEM_NAME}"
          "${CMAKE_NM}"
          "${CMAKE_SOURCE_DIR}/cmake/exports/cpkt_mqttc.txt"
          "${CMAKE_C_COMPILER}"
          "${CMAKE_SYSROOT}")
      add_test(
        NAME audio_sus_header_facade
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/audio_sus_header_facade_test.sh"
          "${CMAKE_SOURCE_DIR}")
      add_test(
        NAME postgres_header_facade
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/postgres_header_facade_test.sh"
          "${CMAKE_SOURCE_DIR}")
      add_test(
        NAME postgres_api_coverage
        COMMAND
          bash
          "${CMAKE_SOURCE_DIR}/tests/postgres_api_coverage_test.sh"
          "${CMAKE_SOURCE_DIR}/src/postgres.c"
          "$<TARGET_PROPERTY:cpkt::postgresql_static,INTERFACE_INCLUDE_DIRECTORIES>")
      add_test(
        NAME gssapi_header_facade
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/gssapi_header_facade_test.sh"
          "${CMAKE_SOURCE_DIR}")
      add_test(
        NAME sasl_header_facade
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/sasl_header_facade_test.sh"
          "${CMAKE_SOURCE_DIR}")
      add_test(
        NAME sqlite_header_facade
        COMMAND
          "${CMAKE_SOURCE_DIR}/tests/sqlite_header_facade_test.sh"
          "${CMAKE_SOURCE_DIR}")
      add_test(
        NAME sqlite_api_coverage
        COMMAND
          bash
          "${CMAKE_SOURCE_DIR}/tests/sqlite_api_coverage_test.sh"
          "${CMAKE_SOURCE_DIR}/src/sqlite.c"
          "$<TARGET_PROPERTY:cpkt::sqlite_static,INTERFACE_INCLUDE_DIRECTORIES>/sqlite3.h"
          "$<TARGET_PROPERTY:cpkt::sqlite_static,INTERFACE_INCLUDE_DIRECTORIES>/sqlite3session.h"
          "$<TARGET_PROPERTY:cpkt::sqlite_static,INTERFACE_INCLUDE_DIRECTORIES>/sqlite3rtree.h"
          "$<TARGET_PROPERTY:cpkt::sqlite_static,INTERFACE_INCLUDE_DIRECTORIES>/fts5.h")
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

  # Symbol inspection uses host tools on target artifacts. It must remain
  # registered even where the target has no local execution runner.
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(_cpkt_lua_inventory_symbol_format macho)
  else()
    set(_cpkt_lua_inventory_symbol_format elf)
  endif()
  add_test(
    NAME lua_api_inventory
    COMMAND
      "${CPKT_PYTHON3_EXECUTABLE}"
      "${CMAKE_SOURCE_DIR}/tools/generate_lua_api_inventory.py"
      --include-dir "$<TARGET_PROPERTY:cpkt::lua_static,INTERFACE_INCLUDE_DIRECTORIES>"
      --native-library "$<TARGET_FILE:cpkt::lua_shared>"
      --facade-library "$<TARGET_FILE:cpkt_lua_shared>"
      --facade-header "${CPKT_LUA_FACADE_HEADER}"
      --symbol-tool "${CMAKE_NM}"
      --symbol-format "${_cpkt_lua_inventory_symbol_format}"
      --output "${CMAKE_CURRENT_BINARY_DIR}/lua-api-inventory.json")
  add_test(
    NAME lua_gc_facade_generation
    COMMAND
      bash "${CMAKE_SOURCE_DIR}/tests/lua_gc_facade_generation_test.sh"
      "${CPKT_LUA_FACADE_SOURCE}")
  add_test(
    NAME lua_export_policy
    COMMAND
      bash
      "${CMAKE_SOURCE_DIR}/tests/lua_export_policy_test.sh"
      "$<TARGET_FILE:cpkt_lua_shared>"
      "${CMAKE_SYSTEM_NAME}"
      "${CMAKE_NM}"
      "${CMAKE_SOURCE_DIR}/cmake/exports/cpkt_lua.txt"
      "${CMAKE_C_COMPILER}"
      "${CMAKE_SYSROOT}")
endif()

if(CPKT_ENABLE_FUZZING)
  cpkt_add_lua_runtime_mock_test(cpkt_lua_runtime_fuzz fuzz/lua_runtime_fuzz.c)
  cpkt_configure_afl_target(cpkt_lua_runtime_fuzz)
  add_executable(cpkt_opcua_facade_fuzz fuzz/opcua_facade_fuzz.c)
  cpkt_use_local_runtime(cpkt_opcua_facade_fuzz)
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
        -DCPKT_OPENSSL_ABI_VERSION=${CPKT_OPENSSL_ABI_VERSION}
        -DCPKT_ZLIB_VERSION=${CPKT_ZLIB_VERSION}
        -DCPKT_LIBPNG_VERSION=${CPKT_LIBPNG_VERSION}
        -DCPKT_LIBHARU_VERSION=${CPKT_LIBHARU_VERSION}
        -DCPKT_IODBC_VERSION=${CPKT_IODBC_VERSION}
        -DCPKT_CURL_VERSION=${CPKT_CURL_VERSION}
        -DCPKT_NGHTTP2_VERSION=${CPKT_NGHTTP2_VERSION}
        -DCPKT_NGHTTP2_ABI_VERSION=${CPKT_NGHTTP2_ABI_VERSION}
        -DCPKT_LIBSSH2_VERSION=${CPKT_LIBSSH2_VERSION}
        -DCPKT_LIBSSH2_ABI_VERSION=${CPKT_LIBSSH2_ABI_VERSION}
        -DCPKT_MQTTC_ABI_VERSION=${CPKT_MQTTC_ABI_VERSION}
        -DCPKT_CMOCKA_VERSION=${CPKT_CMOCKA_VERSION}
        -DCPKT_LIBXML2_VERSION=${CPKT_LIBXML2_VERSION}
        -DCPKT_LUA_VERSION=${CPKT_LUA_VERSION}
        -DCPKT_MQTTC_VERSION=${CPKT_MQTTC_VERSION}
        -DCPKT_MQTTC_COMMIT=${CPKT_MQTTC_COMMIT}
        -DCPKT_OPEN62541_VERSION=${CPKT_OPEN62541_VERSION}
        -DCPKT_OPEN62541_PATCHSET=${CPKT_OPEN62541_PATCHSET}
        -DCPKT_KRB5_VERSION=${CPKT_KRB5_VERSION}
        -DCPKT_CYRUS_SASL_VERSION=${CPKT_CYRUS_SASL_VERSION}
        -DCPKT_OPENLDAP_VERSION=${CPKT_OPENLDAP_VERSION}
        -DCPKT_POSTGRESQL_VERSION=${CPKT_POSTGRESQL_VERSION}
        -DCPKT_SQLITE_VERSION=${CPKT_SQLITE_VERSION}
        -DCPKT_LUA_ABI_VERSION=${CPKT_LUA_ABI_VERSION}
        -DCPKT_LUA_RUNTIME_ABI_VERSION=${CPKT_LUA_RUNTIME_ABI_VERSION}
        -DCPKT_OPENSSL_STATIC_LIBRARY=$<TARGET_FILE:cpkt_openssl_static>
        -DCPKT_OPENSSL_SHARED_LIBRARY=$<TARGET_FILE:cpkt_openssl_shared>
        -DCPKT_NGHTTP2_FACADE_INCLUDE_DIR=${CPKT_NGHTTP2_FACADE_INCLUDE_DIR}
        -DCPKT_NGHTTP2_STATIC_LIBRARY=$<TARGET_FILE:cpkt_nghttp2_static>
        -DCPKT_NGHTTP2_SHARED_LIBRARY=$<TARGET_FILE:cpkt_nghttp2_shared>
        -DCPKT_LIBSSH2_FACADE_INCLUDE_DIR=${CPKT_LIBSSH2_FACADE_INCLUDE_DIR}
        -DCPKT_LIBSSH2_STATIC_LIBRARY=$<TARGET_FILE:cpkt_libssh2_static>
        -DCPKT_LIBSSH2_SHARED_LIBRARY=$<TARGET_FILE:cpkt_libssh2_shared>
        -DCPKT_MQTTC_FACADE_INCLUDE_DIR=${CPKT_MQTTC_FACADE_INCLUDE_DIR}
        -DCPKT_MQTTC_STATIC_LIBRARY=$<TARGET_FILE:cpkt_mqttc_static>
        -DCPKT_MQTTC_SHARED_LIBRARY=$<TARGET_FILE:cpkt_mqttc_shared>
        -DCPKT_LUA_FACADE_INCLUDE_DIR=${CPKT_LUA_FACADE_INCLUDE_DIR}
        -DCPKT_LUA_STATIC_LIBRARY=$<TARGET_FILE:cpkt_lua_static>
        -DCPKT_LUA_SHARED_LIBRARY=$<TARGET_FILE:cpkt_lua_shared>
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
        -DCPKT_GSSAPI_ABI_VERSION=${CPKT_GSSAPI_ABI_VERSION}
        -DCPKT_GSSAPI_STATIC_LIBRARY=$<TARGET_FILE:cpkt_gssapi_static>
        -DCPKT_GSSAPI_SHARED_LIBRARY=$<TARGET_FILE:cpkt_gssapi_shared>
        -DCPKT_SASL_ABI_VERSION=${CPKT_SASL_ABI_VERSION}
        -DCPKT_SASL_STATIC_LIBRARY=$<TARGET_FILE:cpkt_sasl_static>
        -DCPKT_SASL_SHARED_LIBRARY=$<TARGET_FILE:cpkt_sasl_shared>
        -DCPKT_POSTGRES_ABI_VERSION=${CPKT_POSTGRES_ABI_VERSION}
        -DCPKT_POSTGRES_STATIC_LIBRARY=$<TARGET_FILE:cpkt_postgres_static>
        -DCPKT_POSTGRES_SHARED_LIBRARY=$<TARGET_FILE:cpkt_postgres_shared>
        -DCPKT_SQLITE_ABI_VERSION=${CPKT_SQLITE_ABI_VERSION}
        -DCPKT_SQLITE_STATIC_LIBRARY=$<TARGET_FILE:cpkt_sqlite_static>
        -DCPKT_SQLITE_SHARED_LIBRARY=$<TARGET_FILE:cpkt_sqlite_shared>
        -DCPKT_PDF_ABI_VERSION=${CPKT_PDF_ABI_VERSION}
        -DCPKT_PDF_STATIC_LIBRARY=$<TARGET_FILE:cpkt_pdf_static>
        -DCPKT_PDF_SHARED_LIBRARY=$<TARGET_FILE:cpkt_pdf_shared>
        -DCPKT_WHISPER_VERSION=${CPKT_WHISPER_VERSION}
        -DCPKT_SUS_ABI_VERSION=${CPKT_SUS_ABI_VERSION}
        -DCPKT_SUS_BACKEND_CAPABILITIES=${CPKT_SUS_BACKEND_CAPABILITIES}
        -DCPKT_SUS_STATIC_LIBRARY=$<TARGET_FILE:cpkt_sus_static>
        -DCPKT_SUS_SHARED_LIBRARY=$<TARGET_FILE:cpkt_sus_shared>
        -DCPKT_CXX_STDLIB_STATIC_LIBRARY=${CPKT_CXX_STDLIB_STATIC_LIBRARY}
        -DCPKT_CXX_LIBGCC_STATIC_LIBRARY=${CPKT_CXX_LIBGCC_STATIC_LIBRARY}
        -DCPKT_STRIP_BIN=${CMAKE_STRIP}
        -DCPKT_INSTALL_NAME_TOOL=${CMAKE_INSTALL_NAME_TOOL}
        -P ${CMAKE_SOURCE_DIR}/cmake/package_bundle.cmake
    DEPENDS
      cpkt_deps_all
      cpkt_openssl_static
      cpkt_openssl_shared
      cpkt_nghttp2_static
      cpkt_nghttp2_shared
      cpkt_libssh2_static
      cpkt_libssh2_shared
      cpkt_mqttc_static
      cpkt_mqttc_shared
      cpkt_lua_static
      cpkt_lua_shared
      cpkt_lua_runtime_static
      cpkt_lua_runtime_shared
      cpkt_audio_static
      cpkt_audio_shared
      cpkt_opcua_static
      cpkt_opcua_shared
      cpkt_gssapi_static
      cpkt_gssapi_shared
      cpkt_sasl_static
      cpkt_sasl_shared
      cpkt_postgres_static
      cpkt_postgres_shared
      cpkt_sqlite_static
      cpkt_sqlite_shared
      cpkt_pdf_static
      cpkt_pdf_shared
      cpkt_sus_static
      cpkt_sus_shared
    VERBATIM
  )
endif()
