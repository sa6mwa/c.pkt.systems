set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

include("${CMAKE_CURRENT_LIST_DIR}/cpkt_linux_toolchain_common.cmake")

if(DEFINED ENV{CPKT_AARCH64_MUSL_PREFIX})
  set(_cpkt_local_root "$ENV{CPKT_AARCH64_MUSL_PREFIX}")
elseif(DEFINED ENV{HOME})
  set(_cpkt_local_root "$ENV{HOME}/.local/cross/aarch64-linux-musl")
else()
  set(_cpkt_local_root "")
endif()

cpkt_select_linux_toolchain(
  aarch64-linux-musl
  "${_cpkt_local_root}"
  aarch64-linux-musl
  "${_cpkt_local_root}/aarch64-linux-musl"
  aarch64-linux
  aarch64-buildroot-linux-musl/sysroot
  CPKT_SELECTED_TOOLCHAIN_ROOT
  CPKT_SELECTED_TOOLCHAIN_PREFIX
  CPKT_SELECTED_SYSROOT)
cpkt_configure_linux_toolchain(
  "${CPKT_SELECTED_TOOLCHAIN_ROOT}"
  "${CPKT_SELECTED_TOOLCHAIN_PREFIX}"
  "${CPKT_SELECTED_SYSROOT}")

set(CMAKE_CROSSCOMPILING_EMULATOR /usr/bin/qemu-aarch64;-L;${CPKT_SELECTED_SYSROOT} CACHE STRING "" FORCE)

set(CPKT_TARGET_ARCH aarch64 CACHE STRING "" FORCE)
set(CPKT_TARGET_OS linux CACHE STRING "" FORCE)
set(CPKT_TARGET_LIBC musl CACHE STRING "" FORCE)
