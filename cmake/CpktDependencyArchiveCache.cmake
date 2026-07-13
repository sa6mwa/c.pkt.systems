function(cpkt_acquire_dependency_archive out_var)
  cmake_parse_arguments(ARG "" "NAME;SHA256" "URLS;SEED_PATHS" ${ARGN})
  if(NOT ARG_NAME OR NOT ARG_SHA256 OR NOT ARG_URLS)
    message(FATAL_ERROR "cpkt_acquire_dependency_archive requires NAME, SHA256, and URLS")
  endif()
  string(LENGTH "${ARG_SHA256}" _cpkt_sha256_length)
  if(NOT _cpkt_sha256_length EQUAL 64 OR NOT "${ARG_SHA256}" MATCHES "^[A-Fa-f0-9]+$")
    message(FATAL_ERROR "dependency archive ${ARG_NAME} has an invalid SHA-256: ${ARG_SHA256}")
  endif()
  get_filename_component(_cpkt_archive_name "${ARG_NAME}" NAME)
  if(NOT _cpkt_archive_name STREQUAL "${ARG_NAME}")
    message(FATAL_ERROR "dependency archive name must not contain a path: ${ARG_NAME}")
  endif()
  if(NOT DEFINED CPKT_DEPENDENCY_CACHE OR "${CPKT_DEPENDENCY_CACHE}" STREQUAL "")
    message(FATAL_ERROR "CPKT_DEPENDENCY_CACHE is required for dependency archive acquisition")
  endif()

  string(TOLOWER "${ARG_SHA256}" _cpkt_expected_sha256)
  set(_cpkt_archive_dir "${CPKT_DEPENDENCY_CACHE}/archives/sha256/${_cpkt_expected_sha256}")
  set(_cpkt_archive_path "${_cpkt_archive_dir}/${_cpkt_archive_name}")
  set(_cpkt_lock_path "${CPKT_DEPENDENCY_CACHE}/locks/${_cpkt_expected_sha256}.lock")
  file(MAKE_DIRECTORY "${_cpkt_archive_dir}" "${CPKT_DEPENDENCY_CACHE}/locks")
  file(LOCK "${_cpkt_lock_path}"
    GUARD FUNCTION
    TIMEOUT "${CPKT_DEPENDENCY_CACHE_LOCK_TIMEOUT}"
    RESULT_VARIABLE _cpkt_lock_result)
  if(NOT _cpkt_lock_result STREQUAL "0")
    message(FATAL_ERROR
      "dependency-acquisition: could not lock shared cache entry for ${ARG_NAME}\n"
      "expected_sha256=${_cpkt_expected_sha256}\n"
      "cache_lock=${_cpkt_lock_path}\n"
      "lock_result=${_cpkt_lock_result}")
  endif()

  if(EXISTS "${_cpkt_archive_path}")
    file(SHA256 "${_cpkt_archive_path}" _cpkt_existing_sha256)
    if(NOT _cpkt_existing_sha256 STREQUAL _cpkt_expected_sha256)
      message(STATUS "dependency-acquisition: discarding corrupt shared cache archive ${_cpkt_archive_path}")
      file(REMOVE "${_cpkt_archive_path}")
    endif()
  endif()

  if(NOT EXISTS "${_cpkt_archive_path}")
    foreach(_cpkt_seed_path IN LISTS ARG_SEED_PATHS)
      if(EXISTS "${_cpkt_seed_path}")
        file(SHA256 "${_cpkt_seed_path}" _cpkt_seed_sha256)
        if(_cpkt_seed_sha256 STREQUAL _cpkt_expected_sha256)
          string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef _cpkt_seed_suffix)
          set(_cpkt_seed_temporary_path
            "${_cpkt_archive_dir}/.${_cpkt_archive_name}.part-seed-${_cpkt_seed_suffix}")
          file(COPY_FILE "${_cpkt_seed_path}" "${_cpkt_seed_temporary_path}")
          file(SHA256 "${_cpkt_seed_temporary_path}" _cpkt_seed_copy_sha256)
          if(NOT _cpkt_seed_copy_sha256 STREQUAL _cpkt_expected_sha256)
            file(REMOVE "${_cpkt_seed_temporary_path}")
            message(FATAL_ERROR
              "dependency-acquisition: verified local archive copy failed for ${ARG_NAME}\n"
              "seed_path=${_cpkt_seed_path}\n"
              "cache_path=${_cpkt_archive_path}\n"
              "expected_sha256=${_cpkt_expected_sha256}\n"
              "actual_sha256=${_cpkt_seed_copy_sha256}")
          endif()
          file(RENAME "${_cpkt_seed_temporary_path}" "${_cpkt_archive_path}" RESULT _cpkt_seed_rename_result)
          if(NOT _cpkt_seed_rename_result STREQUAL "0")
            file(REMOVE "${_cpkt_seed_temporary_path}")
            message(FATAL_ERROR
              "dependency-acquisition: could not publish seeded archive ${ARG_NAME}\n"
              "seed_path=${_cpkt_seed_path}\n"
              "cache_path=${_cpkt_archive_path}\n"
              "rename_result=${_cpkt_seed_rename_result}")
          endif()
          break()
        endif()
      endif()
    endforeach()
  endif()

  if(NOT EXISTS "${_cpkt_archive_path}")
    string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef _cpkt_temp_suffix)
    set(_cpkt_temporary_path "${_cpkt_archive_dir}/.${_cpkt_archive_name}.part-${_cpkt_temp_suffix}")
    set(_cpkt_download_errors "")
    foreach(_cpkt_url IN LISTS ARG_URLS)
      # Verify below instead of passing EXPECTED_HASH: CMake aborts before this
      # helper can remove a failed temporary file or try a fallback URL.
      file(DOWNLOAD "${_cpkt_url}" "${_cpkt_temporary_path}"
        TLS_VERIFY ON
        TIMEOUT "${CPKT_DEPENDENCY_DOWNLOAD_TIMEOUT}"
        INACTIVITY_TIMEOUT "${CPKT_DEPENDENCY_DOWNLOAD_INACTIVITY_TIMEOUT}"
        STATUS _cpkt_download_status
        LOG _cpkt_download_log)
      list(GET _cpkt_download_status 0 _cpkt_download_result)
      if(_cpkt_download_result EQUAL 0)
        file(SHA256 "${_cpkt_temporary_path}" _cpkt_download_sha256)
        if(_cpkt_download_sha256 STREQUAL _cpkt_expected_sha256)
          file(RENAME "${_cpkt_temporary_path}" "${_cpkt_archive_path}" RESULT _cpkt_rename_result)
          if(NOT _cpkt_rename_result STREQUAL "0")
            file(REMOVE "${_cpkt_temporary_path}")
            message(FATAL_ERROR
              "dependency-acquisition: could not publish verified archive ${ARG_NAME}\n"
              "temporary_path=${_cpkt_temporary_path}\n"
              "cache_path=${_cpkt_archive_path}\n"
              "rename_result=${_cpkt_rename_result}")
          endif()
          break()
        endif()
        string(APPEND _cpkt_download_errors
          "url=${_cpkt_url}\nresult=checksum-mismatch\nexpected_sha256=${_cpkt_expected_sha256}\nactual_sha256=${_cpkt_download_sha256}\n")
      endif()
      string(APPEND _cpkt_download_errors
        "url=${_cpkt_url}\nresult=${_cpkt_download_result}\n${_cpkt_download_log}\n")
      file(REMOVE "${_cpkt_temporary_path}")
    endforeach()
    if(NOT EXISTS "${_cpkt_archive_path}")
      message(FATAL_ERROR
        "dependency-acquisition: failed to acquire ${ARG_NAME}\n"
        "urls=${ARG_URLS}\n"
        "expected_sha256=${_cpkt_expected_sha256}\n"
        "cache_path=${_cpkt_archive_path}\n"
        "download_errors=${_cpkt_download_errors}")
    endif()
  endif()

  file(SHA256 "${_cpkt_archive_path}" _cpkt_final_sha256)
  if(NOT _cpkt_final_sha256 STREQUAL _cpkt_expected_sha256)
    message(FATAL_ERROR
      "dependency-acquisition: shared cache archive did not verify after acquisition\n"
      "cache_path=${_cpkt_archive_path}\n"
      "expected_sha256=${_cpkt_expected_sha256}\n"
      "actual_sha256=${_cpkt_final_sha256}")
  endif()
  set(${out_var} "${_cpkt_archive_path}" PARENT_SCOPE)
endfunction()
