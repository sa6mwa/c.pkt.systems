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

function(cpkt_validate_repo_dependency_root_contract root contract_file contract target_id root_label)
  if(NOT EXISTS "${root}")
    return()
  endif()
  if(EXISTS "${contract_file}")
    file(READ "${contract_file}" _existing_contract)
  else()
    set(_existing_contract "")
  endif()
  if(NOT _existing_contract STREQUAL "${contract}")
    message(FATAL_ERROR
      "repo-local ${root_label} dependency root for ${target_id} is stale: ${root}\n"
      "Dependency rebuilding is disabled, so CMake cannot refresh this root safely.\n"
      "Reconfigure with CPKT_BUILD_DEPENDENCIES=ON or remove the stale root before using it.")
  endif()
endfunction()

function(cpkt_append_lifecycle_owned_dependency_root out_labels out_paths root root_label)
  get_filename_component(_root_abs "${root}" ABSOLUTE BASE_DIR "${CMAKE_BINARY_DIR}")
  if(root_label STREQUAL "external")
    set(_disposable_root_base "${CMAKE_SOURCE_DIR}/.cache/deps")
  elseif(root_label STREQUAL "build")
    set(_disposable_root_base "${CMAKE_SOURCE_DIR}/.cache/deps-build")
  else()
    message(FATAL_ERROR
      "unknown lifecycle-owned dependency root label: ${root_label}")
  endif()
  get_filename_component(_disposable_root_base_abs "${_disposable_root_base}" ABSOLUTE)
  file(RELATIVE_PATH _disposable_root_base_rel "${CMAKE_SOURCE_DIR}" "${_disposable_root_base_abs}")
  string(REPLACE "/" ";" _disposable_root_base_components "${_disposable_root_base_rel}")
  set(_disposable_root_base_component_path "${CMAKE_SOURCE_DIR}")
  foreach(_disposable_root_base_component IN LISTS _disposable_root_base_components)
    set(_disposable_root_base_component_path
      "${_disposable_root_base_component_path}/${_disposable_root_base_component}")
    if(IS_SYMLINK "${_disposable_root_base_component_path}")
      message(FATAL_ERROR
        "lifecycle-owned ${root_label} dependency root ancestor must not be a symlink: ${_disposable_root_base_component_path}\n"
        "CMake will not recursively delete dependency roots below symlinked ancestors.")
    endif()
  endforeach()
  string(FIND "${_root_abs}" "${_disposable_root_base_abs}/" _root_under_disposable_base)
  if(_root_abs STREQUAL "${_disposable_root_base_abs}"
      OR NOT _root_under_disposable_base EQUAL 0)
    message(FATAL_ERROR
      "lifecycle-owned ${root_label} dependency root must be under ${_disposable_root_base_abs}: ${_root_abs}\n"
      "Set CPKT_CALLER_OWNED_DEPENDENCY_ROOTS=ON with CPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE=ON "
      "when reusing dependency roots managed outside the repository lifecycle.")
  endif()
  if(IS_SYMLINK "${_disposable_root_base_abs}")
    message(FATAL_ERROR
      "lifecycle-owned ${root_label} dependency root base must not be a symlink: ${_disposable_root_base_abs}\n"
      "Set CPKT_CALLER_OWNED_DEPENDENCY_ROOTS=ON with CPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE=ON "
      "when reusing dependency roots managed outside the repository lifecycle.")
  endif()
  file(RELATIVE_PATH _root_rel "${_disposable_root_base_abs}" "${_root_abs}")
  string(REPLACE "/" ";" _root_components "${_root_rel}")
  set(_root_component_path "${_disposable_root_base_abs}")
  foreach(_root_component IN LISTS _root_components)
    set(_root_component_path "${_root_component_path}/${_root_component}")
    if(IS_SYMLINK "${_root_component_path}")
      message(FATAL_ERROR
        "lifecycle-owned ${root_label} dependency root must not contain symlink components: ${_root_component_path}\n"
        "CMake will not recursively delete symlinked dependency roots.")
    endif()
  endforeach()
  set(_labels ${${out_labels}})
  set(_paths ${${out_paths}})
  list(APPEND _labels "${root_label}")
  list(APPEND _paths "${_root_abs}")
  set(${out_labels} "${_labels}" PARENT_SCOPE)
  set(${out_paths} "${_paths}" PARENT_SCOPE)
endfunction()

function(cpkt_refresh_repo_dependency_roots_if_stale
    external_root_lifecycle_owned
    dependency_build_root_lifecycle_owned)
  set(_root_labels_to_refresh "")
  set(_root_paths_to_refresh "")
  if(external_root_lifecycle_owned)
    cpkt_append_lifecycle_owned_dependency_root(
      _root_labels_to_refresh _root_paths_to_refresh "${CPKT_EXTERNAL_ROOT}" "external")
  endif()
  if(dependency_build_root_lifecycle_owned)
    cpkt_append_lifecycle_owned_dependency_root(
      _root_labels_to_refresh _root_paths_to_refresh "${CPKT_DEPENDENCY_BUILD_ROOT}" "build")
  endif()
  if(NOT _root_paths_to_refresh)
    return()
  endif()

  set(_contract "cpkt-repo-dependency-contract-v1\n")
  foreach(_var IN ITEMS
      CMAKE_SYSTEM_NAME
      CMAKE_SYSTEM_PROCESSOR
      CMAKE_CROSSCOMPILING
      CMAKE_GENERATOR
      CMAKE_MAKE_PROGRAM
      CMAKE_TOOLCHAIN_FILE
      CMAKE_SYSROOT
      CMAKE_OSX_SYSROOT
      CMAKE_OSX_DEPLOYMENT_TARGET
      CMAKE_C_FLAGS
      CMAKE_C_COMPILER
      CMAKE_C_COMPILER_ID
      CMAKE_C_COMPILER_VERSION
      CMAKE_CXX_FLAGS
      CMAKE_CXX_COMPILER
      CMAKE_CXX_COMPILER_ID
      CMAKE_CXX_COMPILER_VERSION
      CMAKE_EXE_LINKER_FLAGS
      CMAKE_SHARED_LINKER_FLAGS
      CMAKE_MODULE_LINKER_FLAGS
      CMAKE_STATIC_LINKER_FLAGS
      CMAKE_LINKER
      CMAKE_AR
      CMAKE_RANLIB
      CMAKE_STRIP
      CMAKE_NM
      CMAKE_OBJCOPY
      CMAKE_OBJDUMP
      CMAKE_ADDR2LINE
      CMAKE_READELF
      CPKT_TARGET_ID
      CPKT_TARGET_ARCH
      CPKT_TARGET_OS
      CPKT_TARGET_LIBC
      CPKT_EXTERNAL_ROOT
      CPKT_DEPENDENCY_BUILD_ROOT
      CPKT_TOOLCHAIN_ROOT
      CPKT_OSXCROSS_ROOT
      CPKT_OPENSSL_VERSION
      CPKT_OPENSSL_BUILD_CONFIG_REVISION
      CPKT_ZLIB_VERSION
      CPKT_CURL_VERSION
      CPKT_NGHTTP2_VERSION
      CPKT_LIBSSH2_VERSION
      CPKT_CMOCKA_VERSION
      CPKT_LIBXML2_VERSION
      CPKT_LUA_VERSION
      CPKT_MINIAUDIO_VERSION
      CPKT_WHISPER_VERSION
      CPKT_MQTTC_VERSION
      CPKT_MQTTC_COMMIT
      CPKT_OPEN62541_VERSION
      CPKT_OPEN62541_PATCHSET
      CPKT_DEPENDENCY_BUILD_TYPE
      CPKT_SUS_CPU_ONLY)
    cpkt_append_dependency_contract_var(_contract "${_var}")
  endforeach()

  set(_contract_files
    "${CMAKE_SOURCE_DIR}/cmake/CpktDependencyContract.cmake"
    "${CMAKE_SOURCE_DIR}/cmake/CpktDependencies.cmake"
    "${CMAKE_SOURCE_DIR}/cmake/CpktDependencyArchiveCache.cmake"
    "${CMAKE_SOURCE_DIR}/cmake/apply_patch_series.cmake"
    "${CMAKE_SOURCE_DIR}/cmake/install_lua.cmake"
    "${CMAKE_SOURCE_DIR}/cmake/patch_darwin_generated_install_names.cmake"
    "${CMAKE_SOURCE_DIR}/cmake/patch_libssh2_single_pass.cmake"
    "${CMAKE_SOURCE_DIR}/cmake/patch_openssl_buildinfo.cmake"
    "${CMAKE_SOURCE_DIR}/cmake/patch_whisper_buildinfo.cmake"
    "${CMAKE_SOURCE_DIR}/cmake/patch_zlib_single_pass.cmake"
    "${CMAKE_SOURCE_DIR}/cmake/strip_dependency_install_tree.cmake")
  file(GLOB _contract_toolchain_files CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/cmake/toolchains/*.cmake")
  file(GLOB _contract_open62541_patch_files CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/vendor/open62541/patches/*")
  list(APPEND _contract_files ${_contract_toolchain_files})
  list(APPEND _contract_files ${_contract_open62541_patch_files})
  if(CMAKE_TOOLCHAIN_FILE)
    list(APPEND _contract_files "${CMAKE_TOOLCHAIN_FILE}")
  endif()
  list(REMOVE_DUPLICATES _contract_files)
  list(SORT _contract_files)
  foreach(_contract_file IN LISTS _contract_files)
    cpkt_append_dependency_contract_file(_contract "${_contract_file}" "build-contract")
  endforeach()

  string(SHA256 _contract_sha256 "${_contract}")
  string(PREPEND _contract "sha256:${_contract_sha256}\n")
  set(_contract_dir "${CMAKE_SOURCE_DIR}/.cache/dependency-contracts")

  list(LENGTH _root_paths_to_refresh _root_count)
  math(EXPR _last_root_index "${_root_count} - 1")
  foreach(_root_index RANGE 0 ${_last_root_index})
    list(GET _root_labels_to_refresh ${_root_index} _root_label)
    list(GET _root_paths_to_refresh ${_root_index} _root)
    set(_contract_file "${_contract_dir}/${CPKT_TARGET_ID}-${_root_label}.txt")
    if(NOT CPKT_BUILD_DEPENDENCIES)
      cpkt_validate_repo_dependency_root_contract(
        "${_root}" "${_contract_file}" "${_contract}" "${CPKT_TARGET_ID}" "${_root_label}")
      continue()
    endif()
    if(EXISTS "${_contract_file}")
      file(READ "${_contract_file}" _existing_contract)
    else()
      set(_existing_contract "")
    endif()
    if(NOT _existing_contract STREQUAL _contract)
      if(_root_label STREQUAL "external"
          AND NOT dependency_build_root_lifecycle_owned
          AND EXISTS "${CPKT_DEPENDENCY_BUILD_ROOT}")
        message(FATAL_ERROR
          "repo-local external dependency root for ${CPKT_TARGET_ID} is stale: ${_root}\n"
          "The paired dependency build root is an explicit caller-owned override: ${CPKT_DEPENDENCY_BUILD_ROOT}\n"
          "CMake will not delete caller-owned dependency build state.\n"
          "Remove or refresh the caller-owned build root, or reconfigure without the build-root override.")
      endif()
      if(EXISTS "${_root}")
        file(REMOVE_RECURSE "${_root}")
      endif()
      file(MAKE_DIRECTORY "${_contract_dir}")
      file(WRITE "${_contract_file}" "${_contract}")
      message(STATUS "Refreshed repo-local ${_root_label} dependency root for ${CPKT_TARGET_ID} after dependency build contract change")
    endif()
  endforeach()
endfunction()
