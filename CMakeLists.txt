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
string(TOLOWER "${CPKT_TARGET_ARCH}" CPKT_TARGET_ARCH_LOWER)
if(CPKT_TARGET_ARCH_LOWER STREQUAL "amd64")
  set(CPKT_TARGET_ARCH_LOWER "x86_64")
endif()
if(CPKT_TARGET_ARCH_LOWER STREQUAL "arm64" AND NOT CPKT_TARGET_OS_LOWER STREQUAL "darwin")
  set(CPKT_TARGET_ARCH_LOWER "aarch64")
endif()
if(CPKT_TARGET_ARCH_LOWER STREQUAL "arm")
  set(CPKT_TARGET_ARCH_LOWER "armhf")
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
  "openssl-${CPKT_OPENSSL_VERSION}_zlib-${CPKT_ZLIB_VERSION}_curl-${CPKT_CURL_VERSION}_nghttp2-${CPKT_NGHTTP2_VERSION}_libssh2-${CPKT_LIBSSH2_VERSION}_cmocka-${CPKT_CMOCKA_VERSION}"
  CACHE INTERNAL "Version key for the pinned third-party dependency set.")
set(_cpkt_legacy_external_root "${CMAKE_SOURCE_DIR}/.cache/deps/${CPKT_TARGET_ID}")
set(_cpkt_legacy_dependency_build_root "${CMAKE_SOURCE_DIR}/.cache/deps-build/${CPKT_TARGET_ID}")
set(_cpkt_default_external_root "${_cpkt_legacy_external_root}/${CPKT_DEPENDENCY_SET_ID}")
set(_cpkt_default_dependency_build_root "${_cpkt_legacy_dependency_build_root}/${CPKT_DEPENDENCY_SET_ID}")

if(NOT DEFINED CPKT_EXTERNAL_ROOT OR "${CPKT_EXTERNAL_ROOT}" STREQUAL "${_cpkt_legacy_external_root}")
  set(CPKT_EXTERNAL_ROOT "${_cpkt_default_external_root}" CACHE PATH "Version-keyed install root for dependency artifacts." FORCE)
endif()
if(NOT DEFINED CPKT_DEPENDENCY_BUILD_ROOT OR "${CPKT_DEPENDENCY_BUILD_ROOT}" STREQUAL "${_cpkt_legacy_dependency_build_root}")
  set(CPKT_DEPENDENCY_BUILD_ROOT "${_cpkt_default_dependency_build_root}" CACHE PATH "Version-keyed build root for dependency artifacts." FORCE)
endif()

include(cmake/CpktDependencies.cmake)
cpkt_configure_dependencies()

if(CPKT_BUILD_TESTS)
  add_executable(cpkt_abi_smoke_shared examples/abi_smoke.c)
  target_compile_options(cpkt_abi_smoke_shared PRIVATE -std=c89)
  target_link_libraries(cpkt_abi_smoke_shared PRIVATE
    cpkt::curl_shared
  )

  add_executable(cpkt_abi_smoke_static examples/abi_smoke.c)
  target_compile_options(cpkt_abi_smoke_static PRIVATE -std=c89)
  target_link_libraries(cpkt_abi_smoke_static PRIVATE
    cpkt::curl_static
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
      cpkt::cmocka
    )

    add_executable(cpkt_abi_test_static tests/abi_smoke_test.c)
    target_link_libraries(cpkt_abi_test_static PRIVATE
      cpkt::curl_static
      cpkt::cmocka
    )
    if(CPKT_BUILD_DEPENDENCIES)
      add_dependencies(cpkt_abi_test_shared cpkt_deps)
      add_dependencies(cpkt_abi_test_static cpkt_deps)
    endif()
  endif()

  if(NOT CMAKE_CROSSCOMPILING OR CMAKE_CROSSCOMPILING_EMULATOR)
    add_test(NAME abi_example_shared COMMAND cpkt_abi_smoke_shared)
    add_test(NAME abi_example_static COMMAND cpkt_abi_smoke_static)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
      add_test(NAME abi_test_shared COMMAND cpkt_abi_test_shared)
      add_test(NAME abi_test_static COMMAND cpkt_abi_test_static)
    endif()
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
        -P ${CMAKE_SOURCE_DIR}/cmake/package_bundle.cmake
    DEPENDS cpkt_deps
    VERBATIM
  )
endif()
