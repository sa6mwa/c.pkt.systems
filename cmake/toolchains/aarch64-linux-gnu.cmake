set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_SYSROOT "" CACHE PATH "" FORCE)

set(CMAKE_C_COMPILER /usr/bin/aarch64-linux-gnu-gcc CACHE FILEPATH "")
set(CMAKE_AR /usr/bin/aarch64-linux-gnu-ar CACHE FILEPATH "")
set(CMAKE_RANLIB /usr/bin/aarch64-linux-gnu-ranlib CACHE FILEPATH "")
set(CPKT_GNU_ROOT /usr/aarch64-linux-gnu)

if(NOT EXISTS "${CPKT_GNU_ROOT}/include/stdio.h")
    message(FATAL_ERROR
        "The aarch64 GNU cross toolchain is missing target libc headers under ${CPKT_GNU_ROOT}/include. "
        "Install the Debian/Ubuntu development sysroot package libc6-dev-arm64-cross.")
endif()

if(DEFINED CMAKE_C_FLAGS)
    set(_cpkt_aarch64_c_flags "${CMAKE_C_FLAGS}")
else()
    set(_cpkt_aarch64_c_flags "")
endif()
if(NOT _cpkt_aarch64_c_flags MATCHES "(^| )-isystem ${CPKT_GNU_ROOT}/include($| )")
    string(APPEND _cpkt_aarch64_c_flags " -isystem ${CPKT_GNU_ROOT}/include")
endif()
string(STRIP "${_cpkt_aarch64_c_flags}" _cpkt_aarch64_c_flags)
set(CMAKE_C_FLAGS "${_cpkt_aarch64_c_flags}" CACHE STRING "" FORCE)
unset(_cpkt_aarch64_c_flags)

set(CMAKE_FIND_ROOT_PATH ${CPKT_GNU_ROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_CROSSCOMPILING_EMULATOR /usr/bin/qemu-aarch64;-L;${CPKT_GNU_ROOT} CACHE STRING "")

set(CPKT_TARGET_ARCH aarch64 CACHE STRING "" FORCE)
set(CPKT_TARGET_OS linux CACHE STRING "" FORCE)
set(CPKT_TARGET_LIBC gnu CACHE STRING "" FORCE)
