include_guard(GLOBAL)

function(cpkt_dependency_toolchain_cache_id out_var)
  if(NOT DEFINED CMAKE_C_COMPILER_ID OR CMAKE_C_COMPILER_ID STREQUAL ""
      OR NOT DEFINED CMAKE_C_COMPILER_VERSION OR CMAKE_C_COMPILER_VERSION STREQUAL "")
    message(FATAL_ERROR "Dependency cache identity requires the configured C compiler identity and version")
  endif()

  set(_cpkt_identity "${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}")
  if(DEFINED CPKT_TOOLCHAIN_IDENTITY AND NOT CPKT_TOOLCHAIN_IDENTITY STREQUAL "")
    string(APPEND _cpkt_identity "-${CPKT_TOOLCHAIN_IDENTITY}")
  elseif(CMAKE_C_COMPILER_TARGET)
    string(APPEND _cpkt_identity "-${CMAKE_C_COMPILER_TARGET}")
  endif()
  string(MAKE_C_IDENTIFIER "${_cpkt_identity}" _cpkt_identity)
  set(${out_var} "${_cpkt_identity}" PARENT_SCOPE)
endfunction()
