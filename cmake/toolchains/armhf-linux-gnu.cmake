set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

include("${CMAKE_CURRENT_LIST_DIR}/cpkt_linux_toolchain_common.cmake")

cpkt_select_linux_toolchain(
  armhf-linux-gnu
  /usr
  arm-linux-gnueabihf
  /usr/arm-linux-gnueabihf
  arm-linux
  arm-buildroot-linux-gnueabihf/sysroot
  CPKT_SELECTED_TOOLCHAIN_ROOT
  CPKT_SELECTED_TOOLCHAIN_PREFIX
  CPKT_SELECTED_SYSROOT)
cpkt_configure_linux_toolchain(
  "${CPKT_SELECTED_TOOLCHAIN_ROOT}"
  "${CPKT_SELECTED_TOOLCHAIN_PREFIX}"
  "${CPKT_SELECTED_SYSROOT}")

set(CMAKE_CROSSCOMPILING_EMULATOR /usr/bin/qemu-arm;-L;${CPKT_SELECTED_SYSROOT} CACHE STRING "" FORCE)

set(CPKT_TARGET_ARCH armhf CACHE STRING "" FORCE)
set(CPKT_TARGET_OS linux CACHE STRING "" FORCE)
set(CPKT_TARGET_LIBC gnu CACHE STRING "" FORCE)
