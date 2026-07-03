function(cpkt_bootstrap_linux_toolchain target_id out_root_var)
  if(DEFINED CPKT_AUTO_TOOLCHAINS AND NOT CPKT_AUTO_TOOLCHAINS)
    message(FATAL_ERROR
      "Missing compiler set for ${target_id} and CPKT_AUTO_TOOLCHAINS is OFF. "
      "Install the toolchain locally or enable CPKT_AUTO_TOOLCHAINS.")
  endif()

  get_filename_component(_cpkt_toolchain_dir "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
  get_filename_component(_cpkt_cmake_dir "${_cpkt_toolchain_dir}" DIRECTORY)
  get_filename_component(_cpkt_repo_root "${_cpkt_cmake_dir}" DIRECTORY)
  execute_process(
    COMMAND "${_cpkt_repo_root}/scripts/ensure-toolchain.sh" "${target_id}"
    RESULT_VARIABLE _cpkt_toolchain_result
    OUTPUT_VARIABLE _cpkt_toolchain_root
    ERROR_VARIABLE _cpkt_toolchain_error
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT _cpkt_toolchain_result EQUAL 0)
    message(FATAL_ERROR
      "failed to bootstrap toolchain for ${target_id}: ${_cpkt_toolchain_error}")
  endif()
  set(${out_root_var} "${_cpkt_toolchain_root}" PARENT_SCOPE)
endfunction()

function(cpkt_select_linux_toolchain
    target_id
    local_root
    local_prefix
    local_sysroot
    bootlin_prefix
    bootlin_sysroot
    out_root_var
    out_prefix_var
    out_sysroot_var)
  if(NOT "${local_root}" STREQUAL ""
      AND EXISTS "${local_root}/bin/${local_prefix}-gcc"
      AND EXISTS "${local_root}/bin/${local_prefix}-g++"
      AND EXISTS "${local_root}/bin/${local_prefix}-ar"
      AND EXISTS "${local_root}/bin/${local_prefix}-ranlib"
      AND EXISTS "${local_sysroot}/include/stdio.h")
    set(${out_root_var} "${local_root}" PARENT_SCOPE)
    set(${out_prefix_var} "${local_prefix}" PARENT_SCOPE)
    set(${out_sysroot_var} "${local_sysroot}" PARENT_SCOPE)
    return()
  endif()

  cpkt_bootstrap_linux_toolchain("${target_id}" _cpkt_bootstrap_root)
  set(${out_root_var} "${_cpkt_bootstrap_root}" PARENT_SCOPE)
  set(${out_prefix_var} "${bootlin_prefix}" PARENT_SCOPE)
  set(${out_sysroot_var} "${_cpkt_bootstrap_root}/${bootlin_sysroot}" PARENT_SCOPE)
endfunction()

function(cpkt_configure_linux_toolchain root prefix sysroot)
  set(CMAKE_C_COMPILER "${root}/bin/${prefix}-gcc" CACHE FILEPATH "" FORCE)
  set(CMAKE_CXX_COMPILER "${root}/bin/${prefix}-g++" CACHE FILEPATH "" FORCE)
  set(CMAKE_AR "${root}/bin/${prefix}-ar" CACHE FILEPATH "" FORCE)
  set(CMAKE_RANLIB "${root}/bin/${prefix}-ranlib" CACHE FILEPATH "" FORCE)
  set(CMAKE_STRIP "${root}/bin/${prefix}-strip" CACHE FILEPATH "" FORCE)
  set(CMAKE_READELF "${root}/bin/${prefix}-readelf" CACHE FILEPATH "" FORCE)

  set(CMAKE_SYSROOT "${sysroot}" CACHE PATH "" FORCE)
  set(CMAKE_FIND_ROOT_PATH "${sysroot}" "${root}" CACHE STRING "" FORCE)
  set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER CACHE STRING "" FORCE)
  set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY CACHE STRING "" FORCE)
  set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY CACHE STRING "" FORCE)
  set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY CACHE STRING "" FORCE)
endfunction()
