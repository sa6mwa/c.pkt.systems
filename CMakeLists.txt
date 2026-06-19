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

project(c_pkt_systems VERSION "${CPKT_PROJECT_VERSION}" LANGUAGES C)

include(CTest)
include(ProcessorCount)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

option(CPKT_BUILD_DEPENDENCIES "Download and build pinned third-party dependencies." ON)
option(CPKT_BUILD_TESTS "Build ABI/link smoke tests for published dependency artifacts." ON)
option(CPKT_ENABLE_FUZZING "Build libFuzzer facade fuzz targets." OFF)
option(CPKT_FACADE_ONLY "Build only repo-owned facade hardening targets." OFF)

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

function(cpkt_add_lua_runtime_mock_test target_name source_name)
  add_executable(${target_name}
    src/lua_runtime.c
    tests/lua_mock/lua_mock.c
    ${source_name})
  target_compile_options(${target_name} PRIVATE -std=c99 -Wall -Wextra -Wpedantic)
  target_include_directories(${target_name} PRIVATE
    "${CMAKE_SOURCE_DIR}/tests/lua_mock/include"
    "${CMAKE_SOURCE_DIR}/include")
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
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
      target_compile_options(cpkt_lua_runtime_fuzz PRIVATE
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer)
      target_link_options(cpkt_lua_runtime_fuzz PRIVATE
        -fsanitize=fuzzer,address,undefined)
    else()
      message(FATAL_ERROR "CPKT_ENABLE_FUZZING requires a Clang-compatible compiler with libFuzzer")
    endif()
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
set(CPKT_DOWNLOAD_ROOT "${CMAKE_SOURCE_DIR}/.cache/downloads" CACHE PATH "Shared source archive download root.")
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
  "openssl-${CPKT_OPENSSL_VERSION}_zlib-${CPKT_ZLIB_VERSION}_curl-${CPKT_CURL_VERSION}_nghttp2-${CPKT_NGHTTP2_VERSION}_libssh2-${CPKT_LIBSSH2_VERSION}_libxml2-${CPKT_LIBXML2_VERSION}_lua-${CPKT_LUA_VERSION}_cmocka-${CPKT_CMOCKA_VERSION}"
  CACHE INTERNAL "Version key for the pinned third-party dependency set.")
set(_cpkt_dependency_toolchain_id
  "${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}")
if(CMAKE_C_COMPILER_TARGET)
  string(APPEND _cpkt_dependency_toolchain_id "-${CMAKE_C_COMPILER_TARGET}")
endif()
string(MAKE_C_IDENTIFIER "${_cpkt_dependency_toolchain_id}" _cpkt_dependency_toolchain_id)
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
    OR (_cpkt_external_root_under_legacy EQUAL 0 AND NOT "${CPKT_EXTERNAL_ROOT}" STREQUAL "${_cpkt_default_external_root}"))
  set(CPKT_EXTERNAL_ROOT "${_cpkt_default_external_root}" CACHE PATH "Version-keyed install root for dependency artifacts." FORCE)
endif()
if(DEFINED CPKT_DEPENDENCY_BUILD_ROOT)
  string(FIND "${CPKT_DEPENDENCY_BUILD_ROOT}" "${_cpkt_legacy_dependency_build_root}/" _cpkt_dependency_build_root_under_legacy)
else()
  set(_cpkt_dependency_build_root_under_legacy -1)
endif()
if(NOT DEFINED CPKT_DEPENDENCY_BUILD_ROOT
    OR "${CPKT_DEPENDENCY_BUILD_ROOT}" STREQUAL "${_cpkt_legacy_dependency_build_root}"
    OR (_cpkt_dependency_build_root_under_legacy EQUAL 0 AND NOT "${CPKT_DEPENDENCY_BUILD_ROOT}" STREQUAL "${_cpkt_default_dependency_build_root}"))
  set(CPKT_DEPENDENCY_BUILD_ROOT "${_cpkt_default_dependency_build_root}" CACHE PATH "Version-keyed build root for dependency artifacts." FORCE)
endif()

include(cmake/CpktDependencies.cmake)
cpkt_configure_dependencies()

add_library(cpkt_lua_runtime_static STATIC src/lua_runtime.c)
target_compile_features(cpkt_lua_runtime_static PRIVATE c_std_99)
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
target_include_directories(cpkt_lua_runtime_shared
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>"
)
target_link_libraries(cpkt_lua_runtime_shared PUBLIC cpkt::lua_shared)
set_target_properties(cpkt_lua_runtime_shared PROPERTIES
  OUTPUT_NAME cpkt_lua_runtime
  POSITION_INDEPENDENT_CODE ON
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
  set_source_files_properties(examples/lua-runtime-c89/main.c PROPERTIES
    COMPILE_OPTIONS "-std=c89;-Wall;-Wextra;-Wpedantic")
  set_source_files_properties(examples/lua-runtime-c89/host_module.c PROPERTIES
    COMPILE_OPTIONS "-std=c99;-Wall;-Wextra;-Wpedantic")
  target_link_libraries(cpkt_lua_runtime_c89_example PRIVATE cpkt_lua_runtime_static)
  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt_lua_runtime_c89_example cpkt_deps)
  endif()

  add_executable(cpkt_abi_smoke_shared examples/abi_smoke.c)
  target_compile_options(cpkt_abi_smoke_shared PRIVATE -std=c99)
  target_link_libraries(cpkt_abi_smoke_shared PRIVATE
    cpkt::curl_shared
    cpkt::libxml2_shared
    cpkt::lua_shared
  )

  add_executable(cpkt_abi_smoke_static examples/abi_smoke.c)
  target_compile_options(cpkt_abi_smoke_static PRIVATE -std=c99)
  target_link_libraries(cpkt_abi_smoke_static PRIVATE
    cpkt::curl_static
    cpkt::libxml2_static
    cpkt::lua_static
  )
  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt_abi_smoke_shared cpkt_deps)
    add_dependencies(cpkt_abi_smoke_static cpkt_deps)
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
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
      DEPENDS
        cpkt_abi_smoke_shared
        cpkt_abi_smoke_static
        "${CMAKE_SOURCE_DIR}/cmake/package_darwin_smoke_libraries.cmake"
      VERBATIM
    )
    add_custom_target(package-darwin-smoke-test DEPENDS "${_darwin_smoke_zip}")
  else()
    add_executable(cpkt_abi_test_shared tests/abi_smoke_test.c)
    target_link_libraries(cpkt_abi_test_shared PRIVATE
      cpkt::curl_shared
      cpkt::libxml2_shared
      cpkt::lua_shared
      cpkt::cmocka
    )

    add_executable(cpkt_abi_test_static tests/abi_smoke_test.c)
    target_link_libraries(cpkt_abi_test_static PRIVATE
      cpkt::curl_static
      cpkt::libxml2_static
      cpkt::lua_static
      cpkt::cmocka
    )
    if(CPKT_BUILD_DEPENDENCIES)
      add_dependencies(cpkt_abi_test_shared cpkt_deps)
      add_dependencies(cpkt_abi_test_static cpkt_deps)
    endif()

    add_executable(cpkt_lua_runtime_test tests/lua_runtime_test.c)
    target_compile_options(cpkt_lua_runtime_test PRIVATE -std=c99 -Wall -Wextra -Wpedantic)
    target_link_libraries(cpkt_lua_runtime_test PRIVATE
      cpkt_lua_runtime_static
      cpkt::cmocka
    )

    cpkt_add_lua_runtime_mock_test(cpkt_lua_runtime_mock_test tests/lua_runtime_mock_test.c)
    if(CPKT_BUILD_DEPENDENCIES)
      add_dependencies(cpkt_lua_runtime_test cpkt_deps)
    endif()
  endif()

  if(CPKT_CAN_RUN_TARGET_EXECUTABLES)
    add_test(
      NAME lua_runtime_c89_example
      COMMAND cpkt_lua_runtime_c89_example "${_cpkt_lua_runtime_example_file}")
    add_test(NAME abi_example_shared COMMAND cpkt_abi_smoke_shared)
    add_test(NAME abi_example_static COMMAND cpkt_abi_smoke_static)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
      add_test(NAME abi_test_shared COMMAND cpkt_abi_test_shared)
      add_test(NAME abi_test_static COMMAND cpkt_abi_test_static)
      add_test(NAME lua_runtime_unit COMMAND cpkt_lua_runtime_test)
      add_test(NAME lua_runtime_mock_unit COMMAND cpkt_lua_runtime_mock_test)
      add_test(
        NAME clangd_surface
        COMMAND
          "${CMAKE_SOURCE_DIR}/scripts/verify-clangd-surface.sh"
          "${CMAKE_SOURCE_DIR}"
          "${CMAKE_BINARY_DIR}")
    endif()
  endif()
endif()

if(CPKT_ENABLE_FUZZING)
  cpkt_add_lua_runtime_mock_test(cpkt_lua_runtime_fuzz fuzz/lua_runtime_fuzz.c)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(cpkt_lua_runtime_fuzz PRIVATE
      -fsanitize=fuzzer,address,undefined
      -fno-omit-frame-pointer)
    target_link_options(cpkt_lua_runtime_fuzz PRIVATE
      -fsanitize=fuzzer,address,undefined)
  else()
    message(FATAL_ERROR "CPKT_ENABLE_FUZZING requires a Clang-compatible compiler with libFuzzer")
  endif()
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
        -DCPKT_LUA_RUNTIME_INCLUDE_DIR=${CMAKE_SOURCE_DIR}/include
        -DCPKT_LUA_RUNTIME_STATIC_LIBRARY=$<TARGET_FILE:cpkt_lua_runtime_static>
        -DCPKT_LUA_RUNTIME_SHARED_LIBRARY=$<TARGET_FILE:cpkt_lua_runtime_shared>
        -P ${CMAKE_SOURCE_DIR}/cmake/package_bundle.cmake
    DEPENDS
      cpkt_deps
      cpkt_lua_runtime_static
      cpkt_lua_runtime_shared
    VERBATIM
  )
endif()
