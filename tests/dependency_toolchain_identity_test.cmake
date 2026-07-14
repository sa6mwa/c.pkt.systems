if(NOT DEFINED CPKT_SOURCE_DIR OR CPKT_SOURCE_DIR STREQUAL "")
  message(FATAL_ERROR "CPKT_SOURCE_DIR is required")
endif()

include("${CPKT_SOURCE_DIR}/cmake/CpktDependencyToolchainIdentity.cmake")

set(CMAKE_C_COMPILER_ID GNU)
set(CMAKE_C_COMPILER_VERSION 14.3.0)
unset(CMAKE_C_COMPILER_TARGET)
unset(CPKT_TOOLCHAIN_IDENTITY)
cpkt_dependency_toolchain_cache_id(_cpkt_host_identity)

set(CPKT_TOOLCHAIN_IDENTITY
  "bootlin-x86_64-linux-gnu-x86-64--glibc--stable-2025.08-1-x86_64-buildroot-linux-gnu/sysroot")
cpkt_dependency_toolchain_cache_id(_cpkt_bootlin_identity)
if(_cpkt_bootlin_identity STREQUAL _cpkt_host_identity)
  message(FATAL_ERROR "Bootlin dependency identity must not reuse the host GCC cache: ${_cpkt_bootlin_identity}")
endif()
if(NOT _cpkt_bootlin_identity MATCHES "bootlin_x86_64_linux_gnu_x86_64__glibc__stable_2025_08_1")
  message(FATAL_ERROR "Bootlin dependency identity omitted the pinned collection: ${_cpkt_bootlin_identity}")
endif()

set(CPKT_TOOLCHAIN_IDENTITY
  "bootlin-x86_64-linux-gnu-x86-64--glibc--stable-2026.01-1-x86_64-buildroot-linux-gnu/sysroot")
cpkt_dependency_toolchain_cache_id(_cpkt_updated_bootlin_identity)
if(_cpkt_updated_bootlin_identity STREQUAL _cpkt_bootlin_identity)
  message(FATAL_ERROR "A Bootlin collection update must select a new dependency cache identity")
endif()

message(STATUS "dependency toolchain cache identity passed")
