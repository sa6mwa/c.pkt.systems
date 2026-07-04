function(cpkt_get_osxcross_lookup out_host_var out_hints_var)
  set(_osxcross_host "${CPKT_OSXCROSS_HOST}")
  if(_osxcross_host STREQUAL "" AND DEFINED ENV{CPKT_OSXCROSS_HOST})
    set(_osxcross_host "$ENV{CPKT_OSXCROSS_HOST}")
  endif()
  if(_osxcross_host STREQUAL "")
    set(_osxcross_host "arm64-apple-darwin25")
  endif()

  set(_osxcross_root "${CPKT_OSXCROSS_ROOT}")
  if(_osxcross_root STREQUAL "" AND DEFINED ENV{OSXCROSS_ROOT})
    set(_osxcross_root "$ENV{OSXCROSS_ROOT}")
  endif()
  if(_osxcross_root STREQUAL "" AND DEFINED ENV{HOME})
    set(_osxcross_root "$ENV{HOME}/.local/cross/osxcross")
  endif()

  set(_osxcross_hints "")
  if(NOT _osxcross_root STREQUAL "")
    list(APPEND _osxcross_hints "${_osxcross_root}/bin")
  endif()
  if(DEFINED ENV{HOME})
    list(APPEND _osxcross_hints "$ENV{HOME}/.local/cross/osxcross/bin")
  endif()

  set(${out_host_var} "${_osxcross_host}" PARENT_SCOPE)
  set(${out_hints_var} "${_osxcross_hints}" PARENT_SCOPE)
endfunction()

function(cpkt_find_darwin_otool out_var)
  if(DEFINED CPKT_OTOOL AND NOT "${CPKT_OTOOL}" STREQUAL "")
    if(EXISTS "${CPKT_OTOOL}")
      set(${out_var} "${CPKT_OTOOL}" PARENT_SCOPE)
      return()
    endif()
    message(FATAL_ERROR "configured CPKT_OTOOL does not exist: ${CPKT_OTOOL}")
  endif()

  cpkt_get_osxcross_lookup(_osxcross_host _osxcross_hints)

  find_program(_cpkt_otool_bin
    NAMES "${_osxcross_host}-otool" arm64-apple-darwin25-otool otool
    HINTS ${_osxcross_hints})
  if(NOT _cpkt_otool_bin)
    message(FATAL_ERROR
      "otool is required to verify Darwin package artifacts; tried ${_osxcross_host}-otool, arm64-apple-darwin25-otool, and otool")
  endif()

  set(${out_var} "${_cpkt_otool_bin}" PARENT_SCOPE)
endfunction()

function(cpkt_assert_archive_contains regex description)
  if(NOT _listing MATCHES "${regex}")
    message(FATAL_ERROR "archive is missing ${description}: ${regex}")
  endif()
endfunction()

function(cpkt_assert_archive_lacks regex description)
  if(_listing MATCHES "${regex}")
    message(FATAL_ERROR "archive contains forbidden ${description}: ${regex}")
  endif()
endfunction()

function(cpkt_assert_archive_exact_matches regex expected_count description)
  string(REPLACE "\n" ";" _listing_lines "${_listing}")
  set(_actual_count 0)
  foreach(_listing_line IN LISTS _listing_lines)
    if(_listing_line MATCHES "${regex}")
      math(EXPR _actual_count "${_actual_count} + 1")
    endif()
  endforeach()
  if(NOT _actual_count EQUAL expected_count)
    message(FATAL_ERROR
      "archive must contain exactly ${expected_count} ${description}, found ${_actual_count}: ${regex}")
  endif()
endfunction()

function(cpkt_extract_archive_for_assertions out_var)
  string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef _extract_suffix)
  set(_extract_root "${CMAKE_CURRENT_BINARY_DIR}/package-assertions-${_archive_stem}-${_extract_suffix}")
  file(REMOVE_RECURSE "${_extract_root}")
  file(MAKE_DIRECTORY "${_extract_root}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar xf "${CPKT_ARCHIVE}"
    WORKING_DIRECTORY "${_extract_root}"
    RESULT_VARIABLE _extract_result
    ERROR_VARIABLE _extract_error
  )
  if(NOT _extract_result EQUAL 0)
    message(FATAL_ERROR "failed to extract ${CPKT_ARCHIVE}\n${_extract_error}")
  endif()
  set(${out_var} "${_extract_root}" PARENT_SCOPE)
endfunction()

function(cpkt_find_nm out_var)
  if(DEFINED CPKT_NM AND NOT "${CPKT_NM}" STREQUAL "")
    if(EXISTS "${CPKT_NM}")
      set(${out_var} "${CPKT_NM}" PARENT_SCOPE)
      return()
    endif()
    message(FATAL_ERROR "configured CPKT_NM does not exist: ${CPKT_NM}")
  endif()

  set(_nm_names "${CPKT_TARGET_ID}-nm" llvm-nm nm)
  set(_nm_hints "")
  if(CPKT_TARGET_ID MATCHES "darwin")
    cpkt_get_osxcross_lookup(_osxcross_host _osxcross_hints)
    set(_nm_names "${_osxcross_host}-nm" "${CPKT_TARGET_ID}-nm" llvm-nm nm)
    set(_nm_hints ${_osxcross_hints})
  endif()

  find_program(_cpkt_nm_bin
    NAMES ${_nm_names}
    HINTS ${_nm_hints})
  if(NOT _cpkt_nm_bin)
    message(FATAL_ERROR "nm is required to verify packaged static archive symbols")
  endif()

  set(${out_var} "${_cpkt_nm_bin}" PARENT_SCOPE)
endfunction()

function(cpkt_find_ar out_var)
  if(DEFINED CPKT_AR AND NOT "${CPKT_AR}" STREQUAL "")
    if(EXISTS "${CPKT_AR}")
      set(${out_var} "${CPKT_AR}" PARENT_SCOPE)
      return()
    endif()
    message(FATAL_ERROR "configured CPKT_AR does not exist: ${CPKT_AR}")
  endif()

  find_program(_cpkt_ar_bin
    NAMES "${CPKT_TARGET_ID}-ar" llvm-ar ar)
  if(NOT _cpkt_ar_bin)
    message(FATAL_ERROR "ar is required to verify packaged static archive contents")
  endif()

  set(${out_var} "${_cpkt_ar_bin}" PARENT_SCOPE)
endfunction()

function(cpkt_find_readelf out_var)
  if(DEFINED CPKT_READELF AND NOT "${CPKT_READELF}" STREQUAL "")
    if(EXISTS "${CPKT_READELF}")
      set(${out_var} "${CPKT_READELF}" PARENT_SCOPE)
      return()
    endif()
    message(FATAL_ERROR "configured CPKT_READELF does not exist: ${CPKT_READELF}")
  endif()

  find_program(_cpkt_readelf_bin
    NAMES "${CPKT_TARGET_ID}-readelf" llvm-readelf readelf)
  if(NOT _cpkt_readelf_bin)
    message(FATAL_ERROR "readelf is required to verify packaged static archive objects")
  endif()

  set(${out_var} "${_cpkt_readelf_bin}" PARENT_SCOPE)
endfunction()

function(cpkt_read_defined_symbols out_var archive_path description)
  set(_nm_args -g --defined-only)

  if(NOT EXISTS "${archive_path}")
    message(FATAL_ERROR "missing ${description}: ${archive_path}")
  endif()
  cpkt_find_nm(_cpkt_nm)
  if(CPKT_TARGET_ID MATCHES "darwin")
    set(_nm_args -gU)
  endif()
  execute_process(
    COMMAND "${_cpkt_nm}" ${_nm_args} "${archive_path}"
    RESULT_VARIABLE _nm_result
    OUTPUT_VARIABLE _nm_output
    ERROR_VARIABLE _nm_error
  )
  if(NOT _nm_result EQUAL 0)
    message(FATAL_ERROR "failed to read symbols from ${description}: ${archive_path}\n${_nm_error}")
  endif()
  set(${out_var} "${_nm_output}" PARENT_SCOPE)
endfunction()

function(cpkt_assert_static_archive_lacks_lto archive_path description)
  if(NOT EXISTS "${archive_path}")
    message(FATAL_ERROR "missing ${description}: ${archive_path}")
  endif()

  cpkt_find_ar(_cpkt_ar)
  cpkt_find_readelf(_cpkt_readelf)
  string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef _archive_extract_suffix)
  set(_archive_extract_dir
    "${CMAKE_CURRENT_BINARY_DIR}/package-assertions-${_archive_stem}-archive-${_archive_extract_suffix}")
  file(REMOVE_RECURSE "${_archive_extract_dir}")
  file(MAKE_DIRECTORY "${_archive_extract_dir}")
  execute_process(
    COMMAND "${_cpkt_ar}" x "${archive_path}"
    WORKING_DIRECTORY "${_archive_extract_dir}"
    RESULT_VARIABLE _ar_result
    ERROR_VARIABLE _ar_error
  )
  if(NOT _ar_result EQUAL 0)
    message(FATAL_ERROR "failed to extract ${description}: ${archive_path}\n${_ar_error}")
  endif()

  file(GLOB _archive_objects "${_archive_extract_dir}/*")
  foreach(_archive_object IN LISTS _archive_objects)
    if(IS_DIRECTORY "${_archive_object}")
      continue()
    endif()
    execute_process(
      COMMAND "${_cpkt_readelf}" -S "${_archive_object}"
      RESULT_VARIABLE _readelf_result
      OUTPUT_VARIABLE _readelf_output
      ERROR_VARIABLE _readelf_error
    )
    if(NOT _readelf_result EQUAL 0)
      message(FATAL_ERROR
        "failed to inspect object from ${description}: ${_archive_object}\n${_readelf_error}")
    endif()
    if(_readelf_output MATCHES "\\.gnu\\.lto")
      message(FATAL_ERROR
        "${description} contains LTO object sections that can emit upstream diagnostics during downstream links: ${archive_path}")
    endif()
  endforeach()
  file(REMOVE_RECURSE "${_archive_extract_dir}")
endfunction()

function(cpkt_assert_darwin_install_name file_path expected_install_name description)
  cpkt_find_darwin_otool(CPKT_OTOOL_BIN)
  if(NOT CPKT_OTOOL_BIN)
    message(FATAL_ERROR "otool is required to verify ${description}")
  endif()
  if(NOT EXISTS "${file_path}")
    message(FATAL_ERROR "missing ${description}: ${file_path}")
  endif()

  execute_process(
    COMMAND "${CPKT_OTOOL_BIN}" -D "${file_path}"
    RESULT_VARIABLE _otool_result
    OUTPUT_VARIABLE _otool_output
    ERROR_VARIABLE _otool_error
  )
  if(NOT _otool_result EQUAL 0)
    message(FATAL_ERROR "failed to inspect ${description}: ${file_path}\n${_otool_error}")
  endif()
  string(REPLACE "\r\n" "\n" _otool_output "${_otool_output}")
  string(REPLACE "\n" ";" _otool_lines "${_otool_output}")
  set(_install_name_found OFF)
  foreach(_otool_line IN LISTS _otool_lines)
    string(STRIP "${_otool_line}" _otool_line)
    if(_otool_line STREQUAL "${expected_install_name}")
      set(_install_name_found ON)
    endif()
  endforeach()
  if(NOT _install_name_found)
    message(FATAL_ERROR "${description} must have Darwin install name [${expected_install_name}]")
  endif()
endfunction()

function(cpkt_assert_darwin_dylib_relocatable file_path description)
  cpkt_find_darwin_otool(CPKT_OTOOL_BIN)
  if(NOT EXISTS "${file_path}")
    message(FATAL_ERROR "missing ${description}: ${file_path}")
  endif()

  execute_process(
    COMMAND "${CPKT_OTOOL_BIN}" -D "${file_path}"
    RESULT_VARIABLE _id_result
    OUTPUT_VARIABLE _id_output
    ERROR_VARIABLE _id_error
  )
  if(NOT _id_result EQUAL 0)
    message(FATAL_ERROR "failed to inspect Darwin install name for ${description}: ${file_path}\n${_id_error}")
  endif()
  string(REPLACE "\r\n" "\n" _id_output "${_id_output}")
  string(REPLACE "\n" ";" _id_lines "${_id_output}")
  set(_id_found OFF)
  foreach(_id_line IN LISTS _id_lines)
    string(STRIP "${_id_line}" _id_line)
    if(_id_line MATCHES "^@rpath/[^/]+\\.dylib$")
      set(_id_found ON)
    elseif(_id_line MATCHES "^(|.*:)$")
      continue()
    elseif(NOT _id_line STREQUAL "")
      message(FATAL_ERROR "${description} has non-rpath Darwin install name: ${_id_line}")
    endif()
  endforeach()
  if(NOT _id_found)
    message(FATAL_ERROR "${description} must have an @rpath Darwin install name")
  endif()

  execute_process(
    COMMAND "${CPKT_OTOOL_BIN}" -L "${file_path}"
    RESULT_VARIABLE _load_result
    OUTPUT_VARIABLE _load_output
    ERROR_VARIABLE _load_error
  )
  if(NOT _load_result EQUAL 0)
    message(FATAL_ERROR "failed to inspect Darwin load commands for ${description}: ${file_path}\n${_load_error}")
  endif()
  string(REPLACE "\r\n" "\n" _load_output "${_load_output}")
  string(REPLACE "\n" ";" _load_lines "${_load_output}")
  set(_metadata_lines "")
  foreach(_load_line IN LISTS _load_lines)
    string(STRIP "${_load_line}" _load_line)
    if(_load_line STREQUAL "" OR _load_line MATCHES ":$")
      continue()
    endif()
    list(APPEND _metadata_lines "${_load_line}")
    string(REGEX MATCH "^[^ \t(]+" _load_path "${_load_line}")
    if(_load_path MATCHES "^@rpath/[^/]+\\.dylib$")
      continue()
    endif()
    if(_load_path MATCHES "^/usr/lib/" OR _load_path MATCHES "^/System/Library/")
      continue()
    endif()
    message(FATAL_ERROR "${description} has non-relocatable Darwin dependency: ${_load_path}")
  endforeach()

  execute_process(
    COMMAND "${CPKT_OTOOL_BIN}" -l "${file_path}"
    RESULT_VARIABLE _commands_result
    OUTPUT_VARIABLE _commands_output
    ERROR_VARIABLE _commands_error
  )
  if(NOT _commands_result EQUAL 0)
    message(FATAL_ERROR "failed to inspect Darwin load-command details for ${description}: ${file_path}\n${_commands_error}")
  endif()
  string(REPLACE "\r\n" "\n" _commands_output "${_commands_output}")
  string(REPLACE "\n" ";" _command_lines "${_commands_output}")
  foreach(_command_line IN LISTS _command_lines)
    string(STRIP "${_command_line}" _command_line)
    if(_command_line MATCHES "^path[ \t]+([^ \t]+)")
      set(_rpath "${CMAKE_MATCH_1}")
      list(APPEND _metadata_lines "${_command_line}")
      if(NOT _rpath MATCHES "^@(loader_path|executable_path)(/.*)?$")
        message(FATAL_ERROR "${description} has non-relocatable Darwin rpath: ${_rpath}")
      endif()
    endif()
  endforeach()

  set(_private_path_pattern "(/home/|/Users/|/tmp/|/var/tmp/|/usr/local/|\\.cache|deps-build|package-stage|CMakeFiles)")
  foreach(_metadata_line IN LISTS _id_lines _metadata_lines)
    string(STRIP "${_metadata_line}" _metadata_line)
    if(_metadata_line STREQUAL "" OR _metadata_line MATCHES ":$")
      continue()
    endif()
    if(_metadata_line MATCHES "${_private_path_pattern}")
      message(FATAL_ERROR "${description} contains local/private Darwin path material: ${_metadata_line}")
    endif()
  endforeach()
endfunction()

if(DEFINED CPKT_PACKAGE_ASSERTIONS_TEST_DARWIN_OTOOL_LOOKUP AND CPKT_PACKAGE_ASSERTIONS_TEST_DARWIN_OTOOL_LOOKUP)
  cpkt_find_darwin_otool(_cpkt_test_otool)
  message(STATUS "CPKT_TEST_OTOOL=${_cpkt_test_otool}")
endif()
if(DEFINED CPKT_PACKAGE_ASSERTIONS_TEST_NM_LOOKUP AND CPKT_PACKAGE_ASSERTIONS_TEST_NM_LOOKUP)
  cpkt_find_nm(_cpkt_test_nm)
  message(STATUS "CPKT_TEST_NM=${_cpkt_test_nm}")
endif()
if(DEFINED CPKT_PACKAGE_ASSERTIONS_TEST_NM_SYMBOL_READ AND CPKT_PACKAGE_ASSERTIONS_TEST_NM_SYMBOL_READ)
  cpkt_read_defined_symbols(_cpkt_test_symbols "${CPKT_PACKAGE_ASSERTIONS_TEST_ARCHIVE}" "test archive")
  message(STATUS "CPKT_TEST_SYMBOLS=${_cpkt_test_symbols}")
endif()
if(DEFINED CPKT_PACKAGE_ASSERTIONS_TEST_DARWIN_INSTALL_NAME AND CPKT_PACKAGE_ASSERTIONS_TEST_DARWIN_INSTALL_NAME)
  cpkt_assert_darwin_install_name(
    "${CPKT_PACKAGE_ASSERTIONS_TEST_DYLIB}"
    "${CPKT_PACKAGE_ASSERTIONS_TEST_EXPECTED_INSTALL_NAME}"
    "test Darwin install name")
  message(STATUS "CPKT_TEST_DARWIN_INSTALL_NAME=ok")
endif()
if(DEFINED CPKT_PACKAGE_ASSERTIONS_TEST_DARWIN_RELOCATABLE AND CPKT_PACKAGE_ASSERTIONS_TEST_DARWIN_RELOCATABLE)
  cpkt_assert_darwin_dylib_relocatable(
    "${CPKT_PACKAGE_ASSERTIONS_TEST_DYLIB}"
    "test Darwin relocatable dylib")
  message(STATUS "CPKT_TEST_DARWIN_RELOCATABLE=ok")
endif()
if((DEFINED CPKT_PACKAGE_ASSERTIONS_TEST_DARWIN_OTOOL_LOOKUP AND CPKT_PACKAGE_ASSERTIONS_TEST_DARWIN_OTOOL_LOOKUP) OR
    (DEFINED CPKT_PACKAGE_ASSERTIONS_TEST_NM_LOOKUP AND CPKT_PACKAGE_ASSERTIONS_TEST_NM_LOOKUP) OR
    (DEFINED CPKT_PACKAGE_ASSERTIONS_TEST_NM_SYMBOL_READ AND CPKT_PACKAGE_ASSERTIONS_TEST_NM_SYMBOL_READ) OR
    (DEFINED CPKT_PACKAGE_ASSERTIONS_TEST_DARWIN_INSTALL_NAME AND CPKT_PACKAGE_ASSERTIONS_TEST_DARWIN_INSTALL_NAME) OR
    (DEFINED CPKT_PACKAGE_ASSERTIONS_TEST_DARWIN_RELOCATABLE AND CPKT_PACKAGE_ASSERTIONS_TEST_DARWIN_RELOCATABLE))
  return()
endif()

foreach(_required CPKT_ARCHIVE CPKT_TARGET_ID CPKT_BUNDLE_VERSION)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

if(NOT EXISTS "${CPKT_ARCHIVE}")
  message(FATAL_ERROR "missing package archive: ${CPKT_ARCHIVE}")
endif()
get_filename_component(CPKT_ARCHIVE "${CPKT_ARCHIVE}" ABSOLUTE)
get_filename_component(_archive_dir "${CPKT_ARCHIVE}" DIRECTORY)
get_filename_component(_archive_name "${CPKT_ARCHIVE}" NAME)
set(_archive_stem "c.pkt.systems-${CPKT_BUNDLE_VERSION}-${CPKT_TARGET_ID}")
string(REGEX REPLACE "([][+.*()^$?{}|\\])" "\\\\\\1" _archive_stem_re "${_archive_stem}")
set(_checksums_path "${_archive_dir}/c.pkt.systems-${CPKT_BUNDLE_VERSION}-CHECKSUMS")
if(NOT EXISTS "${_checksums_path}")
  message(FATAL_ERROR "missing package checksums: ${_checksums_path}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E tar tf "${CPKT_ARCHIVE}"
  RESULT_VARIABLE _list_result
  OUTPUT_VARIABLE _listing
  ERROR_VARIABLE _list_error
)
if(NOT _list_result EQUAL 0)
  message(FATAL_ERROR "failed to list ${CPKT_ARCHIVE}\n${_list_error}")
endif()

execute_process(
  COMMAND tar --numeric-owner -tvf "${CPKT_ARCHIVE}"
  RESULT_VARIABLE _owner_list_result
  OUTPUT_VARIABLE _owner_listing
  ERROR_VARIABLE _owner_list_error
)
if(NOT _owner_list_result EQUAL 0)
  message(FATAL_ERROR "failed to list package archive ownership: ${CPKT_ARCHIVE}\n${_owner_list_error}")
endif()
string(REPLACE "\r\n" "\n" _owner_listing "${_owner_listing}")
string(REPLACE "\n" ";" _owner_lines "${_owner_listing}")
foreach(_owner_line IN LISTS _owner_lines)
  if(_owner_line STREQUAL "")
    continue()
  endif()
  if(NOT _owner_line MATCHES "^[^ ]+[ ]+0/0[ ]+")
    message(FATAL_ERROR "package archive entries must be owned by 0/0: ${_owner_line}")
  endif()
endforeach()

if(_listing MATCHES "(^|\n)\\./")
  message(FATAL_ERROR "archive entries must be rooted at ${_archive_stem}, not ./")
endif()
foreach(_internal_root cmocka curl libssh2 libxml2 lua mqtt-c nghttp2 openssl zlib)
  if(_listing MATCHES "(^|\n)${_internal_root}/")
    message(FATAL_ERROR "archive exposes internal dependency root: ${_internal_root}/")
  endif()
endforeach()
if(_listing MATCHES "(^|\n)${_archive_stem_re}/[^ \n]*/install/")
  message(FATAL_ERROR "archive exposes internal install/ directories")
endif()
if(_listing MATCHES "(^|\n)${_archive_stem_re}/([^ \n]*/)*cmocka")
  message(FATAL_ERROR "release archive must not contain cmocka")
endif()

cpkt_extract_archive_for_assertions(_manifest_extract_root)
set(_manifest_path "${_manifest_extract_root}/${_archive_stem}/share/c.pkt.systems/manifest.txt")
if(NOT EXISTS "${_manifest_path}")
  message(FATAL_ERROR "missing package manifest: ${_manifest_path}")
endif()
file(READ "${_manifest_path}" _manifest_text)
if(NOT _manifest_text MATCHES "(^|\n)lua_runtime_abi_version=([A-Za-z0-9_.+-]+)(\n|$)")
  message(FATAL_ERROR "package manifest is missing lua_runtime_abi_version")
endif()
set(_manifest_lua_runtime_abi_version "${CMAKE_MATCH_2}")
if(NOT _manifest_text MATCHES "(^|\n)opcua_abi_version=([A-Za-z0-9_.+-]+)(\n|$)")
  message(FATAL_ERROR "package manifest is missing opcua_abi_version")
endif()
if(NOT _manifest_text MATCHES "(^|\n)audio_abi_version=([A-Za-z0-9_.+-]+)(\n|$)")
  message(FATAL_ERROR "package manifest is missing audio_abi_version")
endif()
set(_manifest_audio_abi_version "${CMAKE_MATCH_2}")
if(NOT _manifest_text MATCHES "(^|\n)sus_abi_version=([A-Za-z0-9_.+-]+)(\n|$)")
  message(FATAL_ERROR "package manifest is missing sus_abi_version")
endif()
set(_manifest_sus_abi_version "${CMAKE_MATCH_2}")
if(NOT _manifest_text MATCHES "(^|\n)miniaudio_version=([A-Za-z0-9_.+-]+)(\n|$)")
  message(FATAL_ERROR "package manifest is missing miniaudio_version")
endif()
if(NOT _manifest_text MATCHES "(^|\n)whisper_version=([A-Za-z0-9_.+-]+)(\n|$)")
  message(FATAL_ERROR "package manifest is missing whisper_version")
endif()
if(NOT _manifest_text MATCHES "(^|\n)sus_backend_capabilities=([A-Za-z0-9_,.+-]+)(\n|$)")
  message(FATAL_ERROR "package manifest is missing sus_backend_capabilities")
endif()
set(_manifest_sus_backend_capabilities "${CMAKE_MATCH_2}")
if(NOT "${_manifest_sus_backend_capabilities}" STREQUAL "cpu")
  message(FATAL_ERROR
    "package manifest reports unsupported sus backend capabilities: ${_manifest_sus_backend_capabilities}")
endif()
if(NOT _manifest_text MATCHES "(^|\n)open62541_version=([A-Za-z0-9_.+-]+)(\n|$)")
  message(FATAL_ERROR "package manifest is missing open62541_version")
endif()
if(NOT _manifest_text MATCHES "(^|\n)open62541_patchset=([A-Za-z0-9_.+-]+)(\n|$)")
  message(FATAL_ERROR "package manifest is missing open62541_patchset")
endif()
set(_manifest_open62541_patchset "${CMAKE_MATCH_2}")
if(DEFINED CPKT_OPEN62541_PATCHSET AND NOT "${CPKT_OPEN62541_PATCHSET}" STREQUAL "" AND
    NOT "${CPKT_OPEN62541_PATCHSET}" STREQUAL "${_manifest_open62541_patchset}")
  message(FATAL_ERROR
    "configured open62541 patchset ${CPKT_OPEN62541_PATCHSET} does not match package manifest patchset ${_manifest_open62541_patchset}")
endif()
if(NOT _manifest_text MATCHES "(^|\n)mqtt_c_version=([A-Za-z0-9_.+-]+)(\n|$)")
  message(FATAL_ERROR "package manifest is missing mqtt_c_version")
endif()
if(NOT _manifest_text MATCHES "(^|\n)mqtt_c_commit=([A-Fa-f0-9]+)(\n|$)")
  message(FATAL_ERROR "package manifest is missing mqtt_c_commit")
endif()
if(DEFINED CPKT_LUA_RUNTIME_ABI_VERSION AND NOT "${CPKT_LUA_RUNTIME_ABI_VERSION}" STREQUAL "")
  if(NOT "${CPKT_LUA_RUNTIME_ABI_VERSION}" STREQUAL "${_manifest_lua_runtime_abi_version}")
    message(FATAL_ERROR
      "configured Lua runtime ABI ${CPKT_LUA_RUNTIME_ABI_VERSION} does not match package manifest ABI ${_manifest_lua_runtime_abi_version}")
  endif()
else()
  set(CPKT_LUA_RUNTIME_ABI_VERSION "${_manifest_lua_runtime_abi_version}")
endif()
if(DEFINED CPKT_AUDIO_ABI_VERSION AND NOT "${CPKT_AUDIO_ABI_VERSION}" STREQUAL "")
  if(NOT "${CPKT_AUDIO_ABI_VERSION}" STREQUAL "${_manifest_audio_abi_version}")
    message(FATAL_ERROR
      "configured audio ABI ${CPKT_AUDIO_ABI_VERSION} does not match package manifest ABI ${_manifest_audio_abi_version}")
  endif()
else()
  set(CPKT_AUDIO_ABI_VERSION "${_manifest_audio_abi_version}")
endif()
if(DEFINED CPKT_SUS_ABI_VERSION AND NOT "${CPKT_SUS_ABI_VERSION}" STREQUAL "")
  if(NOT "${CPKT_SUS_ABI_VERSION}" STREQUAL "${_manifest_sus_abi_version}")
    message(FATAL_ERROR
      "configured sus ABI ${CPKT_SUS_ABI_VERSION} does not match package manifest ABI ${_manifest_sus_abi_version}")
  endif()
else()
  set(CPKT_SUS_ABI_VERSION "${_manifest_sus_abi_version}")
endif()
file(REMOVE_RECURSE "${_manifest_extract_root}")

function(cpkt_assert_elf_runpath file_path expected_runpath description)
  find_program(CPKT_READELF_BIN NAMES readelf)
  if(NOT CPKT_READELF_BIN)
    message(FATAL_ERROR "readelf is required to verify ${description}")
  endif()
  if(NOT EXISTS "${file_path}")
    message(FATAL_ERROR "missing ${description}: ${file_path}")
  endif()

  execute_process(
    COMMAND "${CPKT_READELF_BIN}" -d "${file_path}"
    RESULT_VARIABLE _readelf_result
    OUTPUT_VARIABLE _readelf_output
    ERROR_VARIABLE _readelf_error
  )
  if(NOT _readelf_result EQUAL 0)
    message(FATAL_ERROR "failed to inspect ${description}: ${file_path}\n${_readelf_error}")
  endif()
  if(NOT _readelf_output MATCHES "\\((RUNPATH|RPATH)\\)[^\n]*\\[${expected_runpath}\\]")
    message(FATAL_ERROR "${description} must have RUNPATH/RPATH [${expected_runpath}]")
  endif()
endfunction()

function(cpkt_assert_elf_soname file_path expected_soname description)
  find_program(CPKT_READELF_BIN NAMES readelf)
  if(NOT CPKT_READELF_BIN)
    message(FATAL_ERROR "readelf is required to verify ${description}")
  endif()
  if(NOT EXISTS "${file_path}")
    message(FATAL_ERROR "missing ${description}: ${file_path}")
  endif()

  execute_process(
    COMMAND "${CPKT_READELF_BIN}" -d "${file_path}"
    RESULT_VARIABLE _readelf_result
    OUTPUT_VARIABLE _readelf_output
    ERROR_VARIABLE _readelf_error
  )
  if(NOT _readelf_result EQUAL 0)
    message(FATAL_ERROR "failed to inspect ${description}: ${file_path}\n${_readelf_error}")
  endif()
  if(NOT _readelf_output MATCHES "\\(SONAME\\)[^\n]*\\[${expected_soname}\\]")
    message(FATAL_ERROR "${description} must have SONAME [${expected_soname}]")
  endif()
endfunction()

foreach(_path
    "include/openssl/ssl.h"
    "lib/libssl.a"
    "lib/libcrypto.a"
    "lib/cmake/OpenSSL/OpenSSLConfig.cmake"
    "lib/cmake/OpenSSL/OpenSSLConfigVersion.cmake"
    "lib/pkgconfig/libssl.pc"
    "lib/pkgconfig/libcrypto.pc"
    "lib/pkgconfig/openssl.pc"
    "include/curl/curl.h"
    "lib/libcurl.a"
    "lib/cmake/CURL/CURLConfig.cmake"
    "lib/cmake/CURL/CURLConfigVersion.cmake"
    "lib/pkgconfig/libcurl.pc"
    "include/libssh2.h"
    "include/libssh2_sftp.h"
    "lib/libssh2.a"
    "lib/cmake/libssh2/libssh2-config.cmake"
    "lib/cmake/libssh2/libssh2-config-version.cmake"
    "lib/pkgconfig/libssh2.pc"
    "include/zlib.h"
    "lib/libz.a"
    "lib/cmake/zlib/ZLIBConfig.cmake"
    "lib/cmake/zlib/ZLIBConfigVersion.cmake"
    "lib/pkgconfig/zlib.pc"
    "include/nghttp2/nghttp2.h"
    "lib/libnghttp2.a"
    "lib/cmake/nghttp2/nghttp2Config.cmake"
    "lib/cmake/nghttp2/nghttp2ConfigVersion.cmake"
    "lib/pkgconfig/libnghttp2.pc"
    "include/libxml2/libxml/parser.h"
    "lib/libxml2.a"
    "lib/cmake/libxml2/libxml2-config.cmake"
    "lib/cmake/libxml2/libxml2-config-version.cmake"
    "lib/pkgconfig/libxml-2.0.pc"
    "include/lua.h"
    "include/lauxlib.h"
    "include/lualib.h"
    "include/miniaudio/miniaudio.h"
    "include/whisper.h"
    "include/mqtt.h"
    "include/mqtt_pal.h"
    "include/open62541/client.h"
    "include/open62541/server.h"
    "include/open62541/plugin/securitypolicy.h"
    "include/cpkt/audio.h"
    "include/cpkt/lua_runtime.h"
    "include/cpkt/sus.h"
    "include/cpkt/opcua.h"
    "lib/libminiaudio.a"
    "lib/libwhisper.a"
    "lib/libggml.a"
    "lib/libggml-base.a"
    "lib/libggml-cpu.a"
    "lib/liblua.a"
    "lib/libmqttc.a"
    "lib/libopen62541.a"
    "lib/libcpkt_audio.a"
    "lib/libcpkt_lua_runtime.a"
    "lib/libcpktsus.a"
    "lib/libcpkt_opcua.a"
    "lib/cmake/Lua/LuaConfig.cmake"
    "lib/cmake/Lua/LuaConfigVersion.cmake"
    "lib/cmake/miniaudio/miniaudioConfig.cmake"
    "lib/cmake/miniaudio/miniaudioConfigVersion.cmake"
    "lib/cmake/whisper/whisperConfig.cmake"
    "lib/cmake/whisper/whisperConfigVersion.cmake"
    "lib/cmake/mqtt-c/mqtt-cConfig.cmake"
    "lib/cmake/mqtt-c/mqtt-cConfigVersion.cmake"
    "lib/cmake/CpktAudio/CpktAudioConfig.cmake"
    "lib/cmake/CpktAudio/CpktAudioConfigVersion.cmake"
    "lib/cmake/CpktLuaRuntime/CpktLuaRuntimeConfig.cmake"
    "lib/cmake/CpktLuaRuntime/CpktLuaRuntimeConfigVersion.cmake"
    "lib/cmake/CpktSus/CpktSusConfig.cmake"
    "lib/cmake/CpktSus/CpktSusConfigVersion.cmake"
    "lib/cmake/CpktOpcUa/CpktOpcUaConfig.cmake"
    "lib/cmake/CpktOpcUa/CpktOpcUaConfigVersion.cmake"
    "lib/cmake/open62541/open62541Config.cmake"
    "lib/cmake/open62541/open62541ConfigVersion.cmake"
    "lib/pkgconfig/lua.pc"
    "lib/pkgconfig/lua5.5.pc"
    "lib/pkgconfig/miniaudio.pc"
    "lib/pkgconfig/whisper.pc"
    "lib/pkgconfig/mqtt-c.pc"
    "lib/pkgconfig/cpkt-audio.pc"
    "lib/pkgconfig/cpkt-lua-runtime.pc"
    "lib/pkgconfig/cpkt-sus.pc"
    "lib/pkgconfig/cpkt-opcua.pc"
    "lib/pkgconfig/open62541.pc"
    "share/c.pkt.systems/manifest.txt"
    "share/doc/c.pkt.systems/LICENSE"
    "share/doc/c.pkt.systems/README.md"
    "share/doc/c.pkt.systems/docs/audio-sus-facade-spec.md"
    "share/doc/c.pkt.systems/docs/opcua-c89-facade-spec.md"
    "share/doc/c.pkt.systems/examples/abi_smoke.c"
    "share/doc/c.pkt.systems/examples/cmake-consumer/CMakeLists.txt"
    "share/doc/c.pkt.systems/examples/lua-runtime-c89/CMakeLists.txt"
    "share/doc/c.pkt.systems/examples/lua-runtime-c89/build-pkg-config.sh"
    "share/doc/c.pkt.systems/examples/lua-runtime-c89/host_module.c"
    "share/doc/c.pkt.systems/examples/lua-runtime-c89/main.c"
    "share/doc/c.pkt.systems/examples/mqttc_smoke.c"
    "share/doc/c.pkt.systems/examples/opcua-c89/CMakeLists.txt"
    "share/doc/c.pkt.systems/examples/opcua-c89/build-pkg-config.sh"
    "share/doc/c.pkt.systems/examples/opcua-c89/main.c"
    "share/doc/c.pkt.systems/examples/pkg-config-consumer/build.sh"
    "share/doc/c.pkt.systems/third_party/openssl/LICENSE"
    "share/doc/c.pkt.systems/third_party/curl/LICENSE"
    "share/doc/c.pkt.systems/third_party/libssh2/LICENSE"
    "share/doc/c.pkt.systems/third_party/zlib/LICENSE"
    "share/doc/c.pkt.systems/third_party/nghttp2/LICENSE"
    "share/doc/c.pkt.systems/third_party/libxml2/LICENSE"
    "share/doc/c.pkt.systems/third_party/lua/LICENSE"
    "share/doc/c.pkt.systems/third_party/miniaudio/LICENSE"
    "share/doc/c.pkt.systems/third_party/whisper.cpp/LICENSE"
    "share/doc/c.pkt.systems/third_party/mqtt-c/LICENSE"
    "share/doc/c.pkt.systems/third_party/open62541/LICENSE"
    "share/doc/c.pkt.systems/third_party/open62541/patches/series"
    "share/doc/c.pkt.systems/third_party/open62541/patches/0001-prefix-embedded-mqtt-c-symbols.patch"
    "share/doc/c.pkt.systems/third_party/open62541/patches/0002-avoid-glibc-private-stdio-limit-header-on-musl.patch"
    "share/doc/c.pkt.systems/third_party/open62541/patches/0003-stub-posix-ethernet-when-packet-headers-are-missing.patch"
    "share/doc/c.pkt.systems/third_party/open62541/patches/0004-avoid-cert-store-path-strncpy-warning.patch")
  cpkt_assert_archive_contains("(^|\n)${_archive_stem_re}/${_path}(\n|$)" "${_path}")
endforeach()

cpkt_extract_archive_for_assertions(_symbol_extract_root)
set(_symbol_root "${_symbol_extract_root}/${_archive_stem}")
if(CPKT_TARGET_ID MATCHES "linux")
  file(GLOB _packaged_static_archives "${_symbol_root}/lib/*.a")
  foreach(_packaged_static_archive IN LISTS _packaged_static_archives)
    get_filename_component(_packaged_static_archive_name "${_packaged_static_archive}" NAME)
    cpkt_assert_static_archive_lacks_lto(
      "${_packaged_static_archive}"
      "${_packaged_static_archive_name}")
  endforeach()
endif()
set(_raw_mqtt_symbol_regex "(^|\n)[0-9A-Fa-f ]+ [A-Za-z] _?(__mqtt_|mqtt_)")
cpkt_read_defined_symbols(_mqttc_symbols "${_symbol_root}/lib/libmqttc.a" "standalone MQTT-C static archive")
if(NOT _mqttc_symbols MATCHES "${_raw_mqtt_symbol_regex}")
  message(FATAL_ERROR "standalone MQTT-C archive does not expose expected MQTT-C symbols")
endif()
cpkt_read_defined_symbols(_open62541_symbols "${_symbol_root}/lib/libopen62541.a" "open62541 static archive")
if(_open62541_symbols MATCHES "${_raw_mqtt_symbol_regex}")
  message(FATAL_ERROR "open62541 archive exposes unprefixed embedded MQTT-C symbols")
endif()
if(NOT _open62541_symbols MATCHES "(^|\n)[0-9A-Fa-f ]+ [A-Za-z] _?cpkt_open62541_mqtt_")
  message(FATAL_ERROR "open62541 archive does not expose prefixed embedded MQTT-C symbols")
endif()
file(REMOVE_RECURSE "${_symbol_extract_root}")

foreach(_path
    "lib/cmake/Libssh2/"
    "lib/cmake/ZLIB/")
  cpkt_assert_archive_lacks("(^|\n)${_archive_stem_re}/${_path}" "${_path}")
endforeach()

cpkt_extract_archive_for_assertions(_metadata_extract_root)
file(GLOB_RECURSE _metadata_files
  "${_metadata_extract_root}/${_archive_stem}/lib/cmake/*"
  "${_metadata_extract_root}/${_archive_stem}/lib/pkgconfig/*")
foreach(_metadata_file IN LISTS _metadata_files)
  if(IS_DIRECTORY "${_metadata_file}")
    continue()
  endif()
  file(READ "${_metadata_file}" _metadata_text)
  if(_metadata_text MATCHES "(/home/|/tmp/|/var/tmp/|\\.cache|deps-build|package-stage|CMakeFiles|CPKT_EXTERNAL_ROOT|CPKT_DEPENDENCY_BUILD_ROOT)")
    message(FATAL_ERROR "package metadata contains local build/cache path material: ${_metadata_file}")
  endif()
endforeach()
file(REMOVE_RECURSE "${_metadata_extract_root}")

cpkt_extract_archive_for_assertions(_facade_header_extract_root)
set(_facade_header "${_facade_header_extract_root}/${_archive_stem}/include/cpkt/lua_runtime.h")
if(NOT EXISTS "${_facade_header}")
  message(FATAL_ERROR "missing Lua C89 facade header: ${_facade_header}")
endif()
file(READ "${_facade_header}" _facade_header_text)
foreach(_forbidden_header_token
    "lua.h"
    "lauxlib.h"
    "lualib.h"
    "lua_State"
    "lua_Integer"
    "lua_Number"
    "lua_Unsigned"
    "long long"
    "inline")
  if(_facade_header_text MATCHES "${_forbidden_header_token}")
    message(FATAL_ERROR "Lua C89 facade header contains forbidden token: ${_forbidden_header_token}")
  endif()
endforeach()
file(REMOVE_RECURSE "${_facade_header_extract_root}")

cpkt_extract_archive_for_assertions(_audio_facade_header_extract_root)
set(_audio_facade_header "${_audio_facade_header_extract_root}/${_archive_stem}/include/cpkt/audio.h")
if(NOT EXISTS "${_audio_facade_header}")
  message(FATAL_ERROR "missing audio C89 facade header: ${_audio_facade_header}")
endif()
file(READ "${_audio_facade_header}" _audio_facade_header_text)
foreach(_forbidden_header_token
    "miniaudio"
    "ma_"
    "stdint\\.h"
    "stdbool\\.h"
    "uint8_t"
    "uint16_t"
    "uint32_t"
    "uint64_t"
    "int8_t"
    "int16_t"
    "int32_t"
    "int64_t"
    "long long"
    "inline")
  if(_audio_facade_header_text MATCHES "${_forbidden_header_token}")
    message(FATAL_ERROR "audio C89 facade header contains forbidden token: ${_forbidden_header_token}")
  endif()
endforeach()
file(REMOVE_RECURSE "${_audio_facade_header_extract_root}")

cpkt_extract_archive_for_assertions(_sus_facade_header_extract_root)
set(_sus_facade_header "${_sus_facade_header_extract_root}/${_archive_stem}/include/cpkt/sus.h")
if(NOT EXISTS "${_sus_facade_header}")
  message(FATAL_ERROR "sus C89 facade header: ${_sus_facade_header}")
endif()
file(READ "${_sus_facade_header}" _sus_facade_header_text)
foreach(_forbidden_header_token
    "whisper"
    "ggml"
    "stdint\\.h"
    "stdbool\\.h"
    "uint8_t"
    "uint16_t"
    "uint32_t"
    "uint64_t"
    "int8_t"
    "int16_t"
    "int32_t"
    "int64_t"
    "long long"
    "inline")
  if(_sus_facade_header_text MATCHES "${_forbidden_header_token}")
    message(FATAL_ERROR "sus C89 facade header contains forbidden token: ${_forbidden_header_token}")
  endif()
endforeach()
file(REMOVE_RECURSE "${_sus_facade_header_extract_root}")

cpkt_extract_archive_for_assertions(_opcua_facade_header_extract_root)
set(_opcua_facade_header "${_opcua_facade_header_extract_root}/${_archive_stem}/include/cpkt/opcua.h")
if(NOT EXISTS "${_opcua_facade_header}")
  message(FATAL_ERROR "missing OPC UA C89 facade header: ${_opcua_facade_header}")
endif()
file(READ "${_opcua_facade_header}" _opcua_facade_header_text)
foreach(_forbidden_header_token
    "open62541/"
    "UA_Client"
    "UA_Server"
    "UA_StatusCode"
    "UA_NodeId"
    "UA_Variant"
    "stdint\\.h"
    "stdbool\\.h"
    "uint8_t"
    "uint16_t"
    "uint32_t"
    "uint64_t"
    "int8_t"
    "int16_t"
    "int32_t"
    "int64_t"
    "long long"
    "inline")
  if(_opcua_facade_header_text MATCHES "${_forbidden_header_token}")
    message(FATAL_ERROR "OPC UA C89 facade header contains forbidden token: ${_forbidden_header_token}")
  endif()
endforeach()
file(REMOVE_RECURSE "${_opcua_facade_header_extract_root}")

if(CPKT_TARGET_ID STREQUAL "arm64-apple-darwin")
  cpkt_assert_archive_exact_matches(
    "^${_archive_stem_re}/lib/libcpkt_lua_runtime([^/]*)?\\.dylib$"
    3
    "Lua runtime facade Darwin shared library entries")
  cpkt_assert_archive_exact_matches(
    "^${_archive_stem_re}/lib/libcpkt_audio([^/]*)?\\.dylib$"
    3
    "audio facade Darwin shared library entries")
  cpkt_assert_archive_exact_matches(
    "^${_archive_stem_re}/lib/libcpktsus([^/]*)?\\.dylib$"
    3
    "sus facade Darwin shared library entries")
  cpkt_assert_archive_exact_matches(
    "^${_archive_stem_re}/lib/libcpkt_opcua([^/]*)?\\.dylib$"
    3
    "OPC UA facade Darwin shared library entries")
  foreach(_path
      "lib/libssl.dylib"
      "lib/libcrypto.dylib"
      "lib/libcurl.dylib"
      "lib/libssh2.1.dylib"
      "lib/libz.dylib"
      "lib/libz.1.dylib"
      "lib/libnghttp2.dylib"
      "lib/libxml2.dylib"
      "lib/libxml2.16.dylib"
      "lib/liblua.dylib"
      "lib/liblua.5.5.dylib"
      "lib/libminiaudio.dylib"
      "lib/libwhisper.dylib"
      "lib/libmqttc.dylib"
      "lib/libmqttc.1.dylib"
      "lib/libmqttc.1.1.2.dylib"
      "lib/libopen62541.dylib"
      "lib/libopen62541.1.5.dylib"
      "lib/libopen62541.1.5.4.dylib"
      "lib/libcpkt_lua_runtime.dylib"
      "lib/libcpkt_lua_runtime.${CPKT_LUA_RUNTIME_ABI_VERSION}.dylib"
      "lib/libcpkt_lua_runtime.${CPKT_BUNDLE_VERSION}.dylib"
      "lib/libcpkt_audio.dylib"
      "lib/libcpkt_audio.${CPKT_AUDIO_ABI_VERSION}.dylib"
      "lib/libcpkt_audio.${CPKT_BUNDLE_VERSION}.dylib"
      "lib/libcpktsus.dylib"
      "lib/libcpktsus.${CPKT_SUS_ABI_VERSION}.dylib"
      "lib/libcpktsus.${CPKT_BUNDLE_VERSION}.dylib")
    cpkt_assert_archive_contains("(^|\n)${_archive_stem_re}/${_path}(\n|$)" "${_path}")
  endforeach()
  cpkt_extract_archive_for_assertions(_assert_extract_root)
  cpkt_assert_darwin_install_name(
    "${_assert_extract_root}/${_archive_stem}/lib/libcpkt_lua_runtime.${CPKT_BUNDLE_VERSION}.dylib"
    "@rpath/libcpkt_lua_runtime.${CPKT_LUA_RUNTIME_ABI_VERSION}.dylib"
    "libcpkt_lua_runtime Darwin install name")
  cpkt_assert_darwin_install_name(
    "${_assert_extract_root}/${_archive_stem}/lib/libmqttc.1.1.2.dylib"
    "@rpath/libmqttc.1.dylib"
    "libmqttc Darwin install name")
  file(GLOB _packaged_darwin_dylibs
    "${_assert_extract_root}/${_archive_stem}/lib/*.dylib")
  foreach(_packaged_darwin_dylib IN LISTS _packaged_darwin_dylibs)
    if(IS_SYMLINK "${_packaged_darwin_dylib}")
      continue()
    endif()
    get_filename_component(_packaged_darwin_dylib_name "${_packaged_darwin_dylib}" NAME)
    cpkt_assert_darwin_dylib_relocatable(
      "${_packaged_darwin_dylib}"
      "${_packaged_darwin_dylib_name}")
  endforeach()
  file(REMOVE_RECURSE "${_assert_extract_root}")
else()
  cpkt_assert_archive_exact_matches(
    "^${_archive_stem_re}/lib/libcpkt_lua_runtime\\.so([^/]*)?$"
    3
    "Lua runtime facade Linux shared library entries")
  cpkt_assert_archive_exact_matches(
    "^${_archive_stem_re}/lib/libcpkt_audio\\.so([^/]*)?$"
    3
    "audio facade Linux shared library entries")
  cpkt_assert_archive_exact_matches(
    "^${_archive_stem_re}/lib/libcpktsus\\.so([^/]*)?$"
    3
    "sus facade Linux shared library entries")
  cpkt_assert_archive_exact_matches(
    "^${_archive_stem_re}/lib/libcpkt_opcua\\.so([^/]*)?$"
    3
    "OPC UA facade Linux shared library entries")
  foreach(_path
      "lib/libssl.so"
      "lib/libcrypto.so"
      "lib/libcurl.so"
      "lib/libssh2.so"
      "lib/libssh2.so.1"
      "lib/libssh2.so.1.0.1"
      "lib/libz.so"
      "lib/libz.so.1"
      "lib/libnghttp2.so"
      "lib/libxml2.so"
      "lib/libxml2.so.16"
      "lib/libxml2.so.16.1.3"
      "lib/liblua.so"
      "lib/liblua.so.5.5"
      "lib/liblua.so.5.5.0"
      "lib/libminiaudio.so"
      "lib/libwhisper.so"
      "lib/libwhisper.so.1"
      "lib/libwhisper.so.1.9.1"
      "lib/libggml.so"
      "lib/libggml.so.0"
      "lib/libggml.so.0.15.1"
      "lib/libggml-base.so"
      "lib/libggml-base.so.0"
      "lib/libggml-base.so.0.15.1"
      "lib/libggml-cpu.so"
      "lib/libggml-cpu.so.0"
      "lib/libggml-cpu.so.0.15.1"
      "lib/libmqttc.so"
      "lib/libmqttc.so.1"
      "lib/libmqttc.so.1.1.2"
      "lib/libopen62541.so"
      "lib/libopen62541.so.1.5"
      "lib/libopen62541.so.1.5.4"
      "lib/libcpkt_lua_runtime.so"
      "lib/libcpkt_lua_runtime.so.${CPKT_LUA_RUNTIME_ABI_VERSION}"
      "lib/libcpkt_lua_runtime.so.${CPKT_BUNDLE_VERSION}"
      "lib/libcpkt_audio.so"
      "lib/libcpkt_audio.so.${CPKT_AUDIO_ABI_VERSION}"
      "lib/libcpkt_audio.so.${CPKT_BUNDLE_VERSION}"
      "lib/libcpktsus.so"
      "lib/libcpktsus.so.${CPKT_SUS_ABI_VERSION}"
      "lib/libcpktsus.so.${CPKT_BUNDLE_VERSION}")
    cpkt_assert_archive_contains("(^|\n)${_archive_stem_re}/${_path}(\n|$)" "${_path}")
  endforeach()

  foreach(_legacy_open62541_path
      "lib/libopen62541.so.0.4"
      "lib/libopen62541.so.0.4.0")
    cpkt_assert_archive_lacks(
      "(^|\n)${_archive_stem_re}/${_legacy_open62541_path}(\n|$)"
      "${_legacy_open62541_path}")
  endforeach()
  cpkt_extract_archive_for_assertions(_assert_extract_root)
  foreach(_runpath_library
      "lib/libcrypto.so.3"
      "lib/libssl.so.3"
      "lib/libssh2.so.1.0.1"
      "lib/libcurl.so.4.8.0"
      "lib/libxml2.so.16.1.3"
      "lib/libmqttc.so.1.1.2"
      "lib/libcpkt_lua_runtime.so"
      "lib/libcpkt_audio.so"
      "lib/libcpktsus.so"
      "lib/libwhisper.so.1.9.1"
      "lib/libggml.so.0.15.1"
      "lib/libggml-base.so.0.15.1"
      "lib/libggml-cpu.so.0.15.1")
    cpkt_assert_elf_runpath(
      "${_assert_extract_root}/${_archive_stem}/${_runpath_library}"
      "\\$ORIGIN"
      "${_runpath_library}")
  endforeach()
  cpkt_assert_elf_soname(
    "${_assert_extract_root}/${_archive_stem}/lib/libcpkt_lua_runtime.so.${CPKT_BUNDLE_VERSION}"
    "libcpkt_lua_runtime.so.${CPKT_LUA_RUNTIME_ABI_VERSION}"
    "libcpkt_lua_runtime SONAME")
  cpkt_assert_elf_soname(
    "${_assert_extract_root}/${_archive_stem}/lib/libcpkt_audio.so.${CPKT_BUNDLE_VERSION}"
    "libcpkt_audio.so.${CPKT_AUDIO_ABI_VERSION}"
    "libcpkt_audio SONAME")
  cpkt_assert_elf_soname(
    "${_assert_extract_root}/${_archive_stem}/lib/libcpktsus.so.${CPKT_BUNDLE_VERSION}"
    "libcpktsus.so.${CPKT_SUS_ABI_VERSION}"
    "libcpktsus SONAME")
  cpkt_assert_elf_soname(
    "${_assert_extract_root}/${_archive_stem}/lib/libmqttc.so.1.1.2"
    "libmqttc.so.1"
    "libmqttc SONAME")
  file(REMOVE_RECURSE "${_assert_extract_root}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E sha256sum "${CPKT_ARCHIVE}"
  RESULT_VARIABLE _sha_result
  OUTPUT_VARIABLE _sha_output
  ERROR_VARIABLE _sha_error
)
if(NOT _sha_result EQUAL 0)
  message(FATAL_ERROR "failed to checksum package archive: ${CPKT_ARCHIVE}\n${_sha_error}")
endif()
string(STRIP "${_sha_output}" _sha_output)
string(REGEX MATCHALL "[^ ]+" _checksum_fields "${_sha_output}")
list(LENGTH _checksum_fields _checksum_field_count)
if(_checksum_field_count LESS 1)
  message(FATAL_ERROR "unexpected checksum output: ${_sha_output}")
endif()
list(GET _checksum_fields 0 _checksum_hash)

file(READ "${_checksums_path}" _checksum_text)
if(NOT _checksum_text MATCHES "(^|\n)${_checksum_hash}[ \t]+${_archive_name}(\n|$)")
  message(FATAL_ERROR
    "checksums file does not contain SHA-256 for ${_archive_name}: ${_checksums_path}")
endif()
