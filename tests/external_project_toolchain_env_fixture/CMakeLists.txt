cmake_minimum_required(VERSION 3.21)
project(cpkt_external_project_toolchain_env NONE)

if(NOT DEFINED CPKT_SOURCE_DIR OR "${CPKT_SOURCE_DIR}" STREQUAL "")
  message(FATAL_ERROR "CPKT_SOURCE_DIR is required")
endif()

set(CMAKE_SYSTEM_NAME Linux)
set(CPKT_BUILD_DEPENDENCIES ON)
set(test_toolchain_root "${CMAKE_BINARY_DIR}/bootlin")
file(MAKE_DIRECTORY "${test_toolchain_root}/bin")
set(CPKT_TOOLCHAIN_ROOT "${test_toolchain_root}")
set(CPKT_EXTERNAL_ROOT "${CMAKE_BINARY_DIR}/deps")
set(CMAKE_C_COMPILER "${test_toolchain_root}/bin/x86_64-linux-gcc")
set(CMAKE_CXX_COMPILER "${test_toolchain_root}/bin/x86_64-linux-g++")
set(CMAKE_LINKER "${test_toolchain_root}/bin/x86_64-linux-ld")
set(CMAKE_AR "${test_toolchain_root}/bin/x86_64-linux-ar")
set(CMAKE_RANLIB "${test_toolchain_root}/bin/x86_64-linux-ranlib")
set(CMAKE_STRIP "${test_toolchain_root}/bin/x86_64-linux-strip")
set(CMAKE_NM "${test_toolchain_root}/bin/x86_64-linux-nm")
set(CMAKE_OBJCOPY "${test_toolchain_root}/bin/x86_64-linux-objcopy")
set(CMAKE_OBJDUMP "${test_toolchain_root}/bin/x86_64-linux-objdump")
set(CMAKE_ADDR2LINE "${test_toolchain_root}/bin/x86_64-linux-addr2line")
set(CMAKE_READELF "${test_toolchain_root}/bin/x86_64-linux-readelf")

include("${CPKT_SOURCE_DIR}/cmake/CpktDependencies.cmake")
set(environment_args "CFLAGS=-O2")
cpkt_append_pinned_external_toolchain_env_args(environment_args)

foreach(expected IN ITEMS
    "PATH=${test_toolchain_root}/bin:$ENV{PATH}"
    "CC=${CMAKE_C_COMPILER}"
    "CXX=${CMAKE_CXX_COMPILER}"
    "LD=${CMAKE_LINKER}"
    "AR=${CMAKE_AR}"
    "RANLIB=${CMAKE_RANLIB}"
    "STRIP=${CMAKE_STRIP}"
    "NM=${CMAKE_NM}"
    "OBJCOPY=${CMAKE_OBJCOPY}"
    "OBJDUMP=${CMAKE_OBJDUMP}"
    "ADDR2LINE=${CMAKE_ADDR2LINE}"
    "READELF=${CMAKE_READELF}")
  list(FIND environment_args "${expected}" found_index)
  if(found_index LESS 0)
    message(FATAL_ERROR "External-project environment did not pin ${expected}")
  endif()
endforeach()

set(cmake_args "")
cpkt_append_common_external_cmake_args(cmake_args)
foreach(expected IN ITEMS
    "-DCMAKE_LINKER=${CMAKE_LINKER}"
    "-DCMAKE_NM=${CMAKE_NM}"
    "-DCMAKE_STRIP=${CMAKE_STRIP}"
    "-DCMAKE_READELF=${CMAKE_READELF}")
  list(FIND cmake_args "${expected}" found_index)
  if(found_index LESS 0)
    message(FATAL_ERROR "External CMake arguments did not pin ${expected}")
  endif()
endforeach()

set(CMAKE_SYSTEM_NAME Darwin)
set(CPKT_OSXCROSS_HOST arm64-apple-darwin25.4)
set(CPKT_MACOS_DEPLOYMENT_TARGET 15.0)
cpkt_append_common_external_cmake_args(darwin_cmake_args)
foreach(expected IN ITEMS
    "-DCPKT_OSXCROSS_HOST=${CPKT_OSXCROSS_HOST}"
    "-DCPKT_MACOS_DEPLOYMENT_TARGET=${CPKT_MACOS_DEPLOYMENT_TARGET}"
    "-DCMAKE_OSX_DEPLOYMENT_TARGET=${CPKT_MACOS_DEPLOYMENT_TARGET}")
  list(FIND darwin_cmake_args "${expected}" found_index)
  if(found_index LESS 0)
    message(FATAL_ERROR "Darwin ExternalProject arguments did not propagate ${expected}")
  endif()
endforeach()
set(darwin_env_args "")
cpkt_append_darwin_external_env_args(darwin_env_args)
list(FIND darwin_env_args "MACOSX_DEPLOYMENT_TARGET=15.0" minimum_env_index)
if(minimum_env_index LESS 0)
  message(FATAL_ERROR "Darwin external builds omit the deployment target")
endif()
foreach(unexpected IN ITEMS
    "PATH=:$ENV{PATH}"
    "LD_LIBRARY_PATH=/lib:$ENV{LD_LIBRARY_PATH}"
    "LDFLAGS=--ld-path=${CMAKE_LINKER}")
  list(FIND darwin_env_args "${unexpected}" unexpected_index)
  if(unexpected_index GREATER_EQUAL 0)
    message(FATAL_ERROR "Native Darwin external build received ${unexpected}")
  endif()
endforeach()
cpkt_get_external_c_flags(darwin_c_flags)
cpkt_get_external_cxx_flags(darwin_cxx_flags)
foreach(flags IN ITEMS "${darwin_c_flags}" "${darwin_cxx_flags}")
  string(FIND "${flags}" "-mmacosx-version-min=15.0" minimum_flag_index)
  if(minimum_flag_index LESS 0)
    message(FATAL_ERROR "Darwin external compiler flags omit the deployment target")
  endif()
endforeach()
set(CMAKE_SYSTEM_NAME Linux)

set(CPKT_TARGET_ARCH x86_64)
set(CPKT_TARGET_LIBC gnu)
set(CPKT_TARGET_OS Linux)
cpkt_get_openssl_config_args(openssl_config_args)
set(expected_openssl_rpath "-Wl,--enable-new-dtags,-rpath,\\\\$$ORIGIN")
list(FIND openssl_config_args "${expected_openssl_rpath}" found_index)
if(found_index LESS 0)
  message(FATAL_ERROR
    "OpenSSL Configure arguments must preserve \\$$ORIGIN for its generated GNU Makefile")
endif()
