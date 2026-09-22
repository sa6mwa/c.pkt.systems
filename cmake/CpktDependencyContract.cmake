function(cpkt_append_dependency_contract_file out_var path label)
  set(_contract "${${out_var}}")
  if(EXISTS "${path}")
    file(SHA256 "${path}" _sha256)
    file(RELATIVE_PATH _relpath "${CMAKE_SOURCE_DIR}" "${path}")
    string(APPEND _contract "file:${label}:${_relpath}:${_sha256}\n")
  else()
    string(APPEND _contract "missing-file:${label}:${path}\n")
  endif()
  set(${out_var} "${_contract}" PARENT_SCOPE)
endfunction()

function(cpkt_append_dependency_contract_var out_var var_name)
  set(_contract "${${out_var}}")
  if(DEFINED ${var_name})
    string(APPEND _contract "var:${var_name}=${${var_name}}\n")
  else()
    string(APPEND _contract "var:${var_name}=<UNDEFINED>\n")
  endif()
  set(${out_var} "${_contract}" PARENT_SCOPE)
endfunction()

function(cpkt_append_dependency_recipe_helper_files out_var recipe_text label)
  string(REGEX MATCHALL "cmake/[A-Za-z0-9_./-]+\\.cmake" _helper_paths
    "${recipe_text}")
  list(REMOVE_DUPLICATES _helper_paths)
  foreach(_helper_path IN LISTS _helper_paths)
    if(_helper_path STREQUAL "cmake/CpktDependencyContract.cmake"
        OR _helper_path STREQUAL "cmake/CpktDependencyArchiveCache.cmake")
      continue()
    endif()
    cpkt_append_dependency_contract_file(
      _contract "${CMAKE_SOURCE_DIR}/${_helper_path}" "${label}")
  endforeach()
  set(${out_var} "${_contract}" PARENT_SCOPE)
endfunction()

function(cpkt_append_dependency_recipe_function out_var function_name)
  set(_recipe_file "${CMAKE_SOURCE_DIR}/cmake/CpktDependencies.cmake")
  file(READ "${_recipe_file}" _recipe_text)
  string(FIND "${_recipe_text}" "function(${function_name}" _function_start)
  if(_function_start LESS 0)
    message(FATAL_ERROR "dependency contract could not find recipe function: ${function_name}")
  endif()
  string(SUBSTRING "${_recipe_text}" ${_function_start} -1 _function_tail)
  string(FIND "${_function_tail}" "endfunction()" _function_end)
  if(_function_end LESS 0)
    message(FATAL_ERROR "dependency contract could not find recipe function terminator: ${function_name}")
  endif()
  math(EXPR _function_length "${_function_end} + 13")
  string(SUBSTRING "${_function_tail}" 0 ${_function_length} _function_text)
  string(SHA256 _function_sha256 "${_function_text}")
  set(_contract "${${out_var}}")
  string(APPEND _contract "recipe:${function_name}:${_function_sha256}\n")
  cpkt_append_dependency_recipe_helper_files(
    _contract "${_function_text}" "recipe-helper")
  set(${out_var} "${_contract}" PARENT_SCOPE)
endfunction()

function(cpkt_append_dependency_recipe_preamble out_var)
  set(_recipe_file "${CMAKE_SOURCE_DIR}/cmake/CpktDependencies.cmake")
  file(READ "${_recipe_file}" _recipe_text)
  string(FIND "${_recipe_text}" "function(cpkt_add_openssl" _first_component_start)
  if(_first_component_start LESS 0)
    message(FATAL_ERROR "dependency contract could not find dependency recipe preamble")
  endif()
  string(SUBSTRING "${_recipe_text}" 0 ${_first_component_start} _preamble_text)
  string(SHA256 _preamble_sha256 "${_preamble_text}")
  set(_contract "${${out_var}}")
  string(APPEND _contract "recipe-preamble:${_preamble_sha256}\n")
  cpkt_append_dependency_recipe_helper_files(
    _contract "${_preamble_text}" "recipe-preamble-helper")
  set(${out_var} "${_contract}" PARENT_SCOPE)
endfunction()

function(cpkt_append_lifecycle_owned_dependency_component_root
    out_paths root root_label component)
  get_filename_component(_root_abs "${root}" ABSOLUTE BASE_DIR "${CMAKE_BINARY_DIR}")
  if(root_label STREQUAL "external")
    set(_base "${CMAKE_SOURCE_DIR}/.cache/deps")
  elseif(root_label STREQUAL "build")
    set(_base "${CMAKE_SOURCE_DIR}/.cache/deps-build")
  else()
    message(FATAL_ERROR "unknown dependency component root label: ${root_label}")
  endif()
  get_filename_component(_base_abs "${_base}" ABSOLUTE)
  string(FIND "${_root_abs}" "${_base_abs}/" _root_under_base)
  if(_root_abs STREQUAL "${_base_abs}" OR NOT _root_under_base EQUAL 0)
    message(FATAL_ERROR
      "lifecycle-owned ${root_label} dependency component root for ${component} must be below ${_base_abs}: ${_root_abs}")
  endif()
  if(IS_SYMLINK "${_base_abs}")
    message(FATAL_ERROR "lifecycle-owned dependency root base must not be a symlink: ${_base_abs}")
  endif()
  file(RELATIVE_PATH _root_rel "${_base_abs}" "${_root_abs}")
  string(REPLACE "/" ";" _root_components "${_root_rel}")
  set(_component_path "${_base_abs}")
  foreach(_component IN LISTS _root_components)
    set(_component_path "${_component_path}/${_component}")
    if(IS_SYMLINK "${_component_path}")
      message(FATAL_ERROR
        "lifecycle-owned dependency component root must not contain symlinks: ${_component_path}")
    endif()
  endforeach()
  set(_paths "${${out_paths}}")
  list(APPEND _paths "${_root_abs}")
  set(${out_paths} "${_paths}" PARENT_SCOPE)
endfunction()

function(cpkt_prepare_dependency_component)
  cmake_parse_arguments(component ""
    "NAME;BUILD_ROOT;INSTALL_ROOT"
    "VARIABLES;DEPENDS;INPUT_FILES;RECIPE_FUNCTIONS"
    ${ARGN})
  if(NOT component_NAME OR NOT component_BUILD_ROOT OR NOT component_INSTALL_ROOT)
    message(FATAL_ERROR "cpkt_prepare_dependency_component requires NAME, BUILD_ROOT, and INSTALL_ROOT")
  endif()

  set(_contract "cpkt-dependency-component-contract-v3\n")
  foreach(_var IN ITEMS
      CMAKE_SYSTEM_NAME CMAKE_SYSTEM_PROCESSOR CMAKE_CROSSCOMPILING
      CMAKE_GENERATOR CMAKE_MAKE_PROGRAM CMAKE_TOOLCHAIN_FILE CMAKE_SYSROOT
      CMAKE_OSX_SYSROOT CMAKE_OSX_DEPLOYMENT_TARGET CMAKE_C_FLAGS
      CMAKE_C_COMPILER CMAKE_C_COMPILER_ID CMAKE_C_COMPILER_VERSION
      CMAKE_CXX_FLAGS CMAKE_CXX_COMPILER CMAKE_CXX_COMPILER_ID
      CMAKE_CXX_COMPILER_VERSION CMAKE_EXE_LINKER_FLAGS
      CMAKE_SHARED_LINKER_FLAGS CMAKE_MODULE_LINKER_FLAGS
      CMAKE_STATIC_LINKER_FLAGS CMAKE_LINKER CMAKE_AR CMAKE_RANLIB CMAKE_STRIP
      CMAKE_NM CMAKE_OBJCOPY CMAKE_OBJDUMP CMAKE_ADDR2LINE CMAKE_READELF
      CPKT_TARGET_ID CPKT_TARGET_ARCH CPKT_TARGET_OS CPKT_TARGET_LIBC
      CPKT_TOOLCHAIN_ROOT CPKT_OSXCROSS_ROOT CPKT_DEPENDENCY_BUILD_TYPE
      CPKT_DEPENDENCY_BUILD_JOBS CPKT_DEPENDENCY_DOWNLOAD_TIMEOUT
      CPKT_DEPENDENCY_DOWNLOAD_INACTIVITY_TIMEOUT)
    cpkt_append_dependency_contract_var(_contract "${_var}")
  endforeach()
  foreach(_var IN LISTS component_VARIABLES)
    cpkt_append_dependency_contract_var(_contract "${_var}")
  endforeach()
  cpkt_append_dependency_recipe_preamble(_contract)
  foreach(_function IN LISTS component_RECIPE_FUNCTIONS)
    cpkt_append_dependency_recipe_function(_contract "${_function}")
  endforeach()
  foreach(_input_file IN LISTS component_INPUT_FILES)
    # Contract-engine and verified-archive cache changes govern bookkeeping,
    # not a component's compiled output. Hashing either invalidates every
    # component without changing a component build recipe.
    if(_input_file STREQUAL "${CMAKE_SOURCE_DIR}/cmake/CpktDependencyContract.cmake"
        OR _input_file STREQUAL "${CMAKE_SOURCE_DIR}/cmake/CpktDependencyArchiveCache.cmake")
      continue()
    endif()
    cpkt_append_dependency_contract_file(_contract "${_input_file}" "component-input")
  endforeach()
  if(CMAKE_TOOLCHAIN_FILE)
    cpkt_append_dependency_contract_file(_contract "${CMAKE_TOOLCHAIN_FILE}" "selected-toolchain")
  endif()
  foreach(_dependency IN LISTS component_DEPENDS)
    get_property(_dependency_contract_set GLOBAL PROPERTY "CPKT_DEPENDENCY_CONTRACT_${_dependency}" SET)
    if(NOT _dependency_contract_set)
      message(FATAL_ERROR
        "dependency component ${component_NAME} was configured before its dependency contract: ${_dependency}")
    endif()
    get_property(_dependency_contract GLOBAL PROPERTY "CPKT_DEPENDENCY_CONTRACT_${_dependency}")
    string(APPEND _contract "dependency:${_dependency}:${_dependency_contract}\n")
  endforeach()

  string(SHA256 _contract_sha256 "${_contract}")
  string(PREPEND _contract "sha256:${_contract_sha256}\n")
  set(_contract_dir "${CPKT_DEPENDENCY_CONTRACT_ROOT}/${CPKT_TARGET_ID}")
  set(_contract_file "${_contract_dir}/${component_NAME}.txt")
  if(EXISTS "${_contract_file}")
    file(READ "${_contract_file}" _existing_contract)
  else()
    set(_existing_contract "")
  endif()

  if(NOT EXISTS "${_contract_file}")
    if(CPKT_BUILD_DEPENDENCIES)
      file(MAKE_DIRECTORY "${_contract_dir}")
      file(WRITE "${_contract_file}" "${_contract}")
      if(EXISTS "${component_BUILD_ROOT}" OR EXISTS "${component_INSTALL_ROOT}")
        message(STATUS
          "Adopted existing dependency component ${component_NAME} for ${CPKT_TARGET_ID} into the component contract cache")
      endif()
    endif()
  else()
    string(REGEX MATCH "\ncpkt-dependency-component-contract-v[12]\n"
      _legacy_schema "${_existing_contract}")
    if(_legacy_schema)
      if(CPKT_BUILD_DEPENDENCIES)
        file(WRITE "${_contract_file}" "${_contract}")
        message(STATUS
          "Migrated dependency component ${component_NAME} for ${CPKT_TARGET_ID} to contract schema v3")
      endif()
    elseif(NOT _existing_contract STREQUAL "${_contract}")
      if(NOT CPKT_BUILD_DEPENDENCIES)
        if(EXISTS "${component_BUILD_ROOT}" OR EXISTS "${component_INSTALL_ROOT}")
          message(FATAL_ERROR
            "dependency component ${component_NAME} for ${CPKT_TARGET_ID} is stale.\n"
            "Dependency rebuilding is disabled; reconfigure with CPKT_BUILD_DEPENDENCIES=ON to refresh only this component.")
        endif()
      else()
        if(NOT CPKT_EXTERNAL_ROOT_LIFECYCLE_OWNED
            OR NOT CPKT_DEPENDENCY_BUILD_ROOT_LIFECYCLE_OWNED)
          message(FATAL_ERROR
            "dependency component ${component_NAME} for ${CPKT_TARGET_ID} is stale, but one or more dependency roots are caller-owned.\n"
            "CMake will not delete caller-owned dependency state. Refresh the component roots yourself, or reconfigure without caller-owned root overrides.")
        endif()
        set(_roots_to_refresh "")
        cpkt_append_lifecycle_owned_dependency_component_root(
          _roots_to_refresh "${component_BUILD_ROOT}" "build" "${component_NAME}")
        cpkt_append_lifecycle_owned_dependency_component_root(
          _roots_to_refresh "${component_INSTALL_ROOT}" "external" "${component_NAME}")
        foreach(_root IN LISTS _roots_to_refresh)
          if(EXISTS "${_root}")
            file(REMOVE_RECURSE "${_root}")
          endif()
        endforeach()
        file(MAKE_DIRECTORY "${_contract_dir}")
        file(WRITE "${_contract_file}" "${_contract}")
        message(STATUS "Refreshed dependency component ${component_NAME} for ${CPKT_TARGET_ID} after contract change")
      endif()
    endif()
  endif()
  set_property(GLOBAL PROPERTY "CPKT_DEPENDENCY_CONTRACT_${component_NAME}" "${_contract_sha256}")
endfunction()
