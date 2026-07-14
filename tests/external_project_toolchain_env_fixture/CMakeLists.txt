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
