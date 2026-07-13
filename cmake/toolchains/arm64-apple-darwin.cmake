set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR arm64)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(DEFINED ENV{OSXCROSS_ROOT} AND NOT "$ENV{OSXCROSS_ROOT}" STREQUAL "")
    set(CPKT_OSXCROSS_ROOT "$ENV{OSXCROSS_ROOT}")
elseif(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
    set(CPKT_OSXCROSS_ROOT "$ENV{HOME}/.local/cross/osxcross")
else()
    message(FATAL_ERROR "OSXCROSS_ROOT is not set and HOME is unavailable")
endif()

set(CPKT_OSXCROSS_HOST "arm64-apple-darwin25" CACHE STRING "osxcross target host triple")
set(CPKT_MACOS_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum macOS deployment target")
set(CMAKE_OSX_DEPLOYMENT_TARGET "${CPKT_MACOS_DEPLOYMENT_TARGET}" CACHE STRING "" FORCE)

set(CPKT_OSXCROSS_BIN_DIR "${CPKT_OSXCROSS_ROOT}/bin")
set(ENV{PATH} "${CPKT_OSXCROSS_BIN_DIR}:$ENV{PATH}")
set(CMAKE_C_COMPILER "${CPKT_OSXCROSS_BIN_DIR}/${CPKT_OSXCROSS_HOST}-clang" CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER "${CPKT_OSXCROSS_BIN_DIR}/${CPKT_OSXCROSS_HOST}-clang++" CACHE FILEPATH "")
set(CMAKE_AR "${CPKT_OSXCROSS_BIN_DIR}/${CPKT_OSXCROSS_HOST}-ar" CACHE FILEPATH "")
set(CMAKE_RANLIB "${CPKT_OSXCROSS_BIN_DIR}/${CPKT_OSXCROSS_HOST}-ranlib" CACHE FILEPATH "")
set(CMAKE_LINKER "${CPKT_OSXCROSS_BIN_DIR}/${CPKT_OSXCROSS_HOST}-ld" CACHE FILEPATH "")
set(CMAKE_INSTALL_NAME_TOOL "${CPKT_OSXCROSS_BIN_DIR}/${CPKT_OSXCROSS_HOST}-install_name_tool" CACHE FILEPATH "")
set(CPKT_OTOOL "${CPKT_OSXCROSS_BIN_DIR}/${CPKT_OSXCROSS_HOST}-otool" CACHE FILEPATH "")

foreach(_cpkt_required_tool
        CMAKE_C_COMPILER
        CMAKE_AR
        CMAKE_RANLIB
        CMAKE_LINKER
        CMAKE_INSTALL_NAME_TOOL
        CPKT_OTOOL)
    if(NOT EXISTS "${${_cpkt_required_tool}}")
        message(FATAL_ERROR
            "The arm64 Apple Darwin osxcross toolchain is missing ${_cpkt_required_tool}: "
            "${${_cpkt_required_tool}}. Set OSXCROSS_ROOT or install osxcross under $HOME/.local/cross/osxcross.")
    endif()
endforeach()

set(_cpkt_darwin_linker_flag "--ld-path=${CMAKE_LINKER}")
foreach(_cpkt_linker_flags
        CMAKE_EXE_LINKER_FLAGS
        CMAKE_SHARED_LINKER_FLAGS
        CMAKE_MODULE_LINKER_FLAGS)
    set(_cpkt_existing_linker_flags "${${_cpkt_linker_flags}}")
    string(REPLACE "-fuse-ld=${CMAKE_LINKER}" "" _cpkt_existing_linker_flags "${_cpkt_existing_linker_flags}")
    string(STRIP "${_cpkt_existing_linker_flags}" _cpkt_existing_linker_flags)
    if(NOT "${_cpkt_existing_linker_flags}" MATCHES "(^| )--ld-path=")
        if("${_cpkt_existing_linker_flags}" STREQUAL "")
            set(_cpkt_existing_linker_flags "${_cpkt_darwin_linker_flag}")
        else()
            set(_cpkt_existing_linker_flags "${_cpkt_darwin_linker_flag} ${_cpkt_existing_linker_flags}")
        endif()
    endif()
    if(NOT "${_cpkt_existing_linker_flags}" STREQUAL "${${_cpkt_linker_flags}}")
        set(${_cpkt_linker_flags} "${_cpkt_existing_linker_flags}" CACHE STRING "" FORCE)
    endif()
endforeach()
unset(_cpkt_linker_flags)
unset(_cpkt_darwin_linker_flag)
unset(_cpkt_existing_linker_flags)

file(GLOB _cpkt_osxcross_sdks LIST_DIRECTORIES true "${CPKT_OSXCROSS_ROOT}/SDK/MacOSX*.sdk")
if(NOT _cpkt_osxcross_sdks)
    message(FATAL_ERROR "failed to locate a usable osxcross macOS SDK under ${CPKT_OSXCROSS_ROOT}/SDK")
endif()
list(SORT _cpkt_osxcross_sdks)
list(REVERSE _cpkt_osxcross_sdks)
list(GET _cpkt_osxcross_sdks 0 CPKT_OSXCROSS_SDK)
if(NOT EXISTS "${CPKT_OSXCROSS_SDK}/usr/include")
    message(FATAL_ERROR "failed to locate a usable osxcross macOS SDK under ${CPKT_OSXCROSS_ROOT}/SDK")
endif()

set(CMAKE_OSX_SYSROOT "${CPKT_OSXCROSS_SDK}" CACHE PATH "" FORCE)
set(CMAKE_FIND_ROOT_PATH "${CPKT_OSXCROSS_SDK}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CPKT_TARGET_ARCH arm64 CACHE STRING "" FORCE)
set(CPKT_TARGET_OS darwin CACHE STRING "" FORCE)
set(CPKT_TARGET_LIBC "" CACHE STRING "" FORCE)
