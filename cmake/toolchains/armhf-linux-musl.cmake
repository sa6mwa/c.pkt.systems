set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

include("${CMAKE_CURRENT_LIST_DIR}/cpkt_linux_toolchain_common.cmake")

if(DEFINED ENV{CPKT_ARMHF_MUSL_PREFIX})
  set(_cpkt_local_root "$ENV{CPKT_ARMHF_MUSL_PREFIX}")
elseif(DEFINED ENV{HOME})
  set(_cpkt_local_root "$ENV{HOME}/.local/cross/arm-linux-musleabihf")
else()
  set(_cpkt_local_root "")
endif()

cpkt_select_linux_toolchain(
  armhf-linux-musl
  "${_cpkt_local_root}"
  arm-linux-musleabihf
  "${_cpkt_local_root}/arm-linux-musleabihf"
  arm-linux
  arm-buildroot-linux-musleabihf/sysroot
  CPKT_SELECTED_TOOLCHAIN_ROOT
  CPKT_SELECTED_TOOLCHAIN_PREFIX
  CPKT_SELECTED_SYSROOT
  CPKT_SELECTED_FIND_ROOT)
cpkt_configure_linux_toolchain(
  "${CPKT_SELECTED_TOOLCHAIN_ROOT}"
  "${CPKT_SELECTED_TOOLCHAIN_PREFIX}"
  "${CPKT_SELECTED_SYSROOT}"
  "${CPKT_SELECTED_FIND_ROOT}")

set(CMAKE_CROSSCOMPILING_EMULATOR /usr/bin/qemu-arm;-L;${CPKT_SELECTED_SYSROOT} CACHE STRING "" FORCE)

set(CPKT_TARGET_ARCH armhf CACHE STRING "" FORCE)
set(CPKT_TARGET_OS linux CACHE STRING "" FORCE)
set(CPKT_TARGET_LIBC musl CACHE STRING "" FORCE)
