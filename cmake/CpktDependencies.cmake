include(ExternalProject)
include("${CMAKE_CURRENT_LIST_DIR}/CpktDependencyArchiveCache.cmake")

macro(cpkt_cached_external_project_add)
  set(_cpkt_ep_args ${ARGV})
  list(FIND _cpkt_ep_args "URL" _cpkt_ep_url_index)
  list(FIND _cpkt_ep_args "URL_HASH" _cpkt_ep_hash_index)
  if(_cpkt_ep_url_index LESS 1 OR _cpkt_ep_hash_index LESS 0 OR _cpkt_ep_hash_index LESS _cpkt_ep_url_index)
    message(FATAL_ERROR "cpkt_cached_external_project_add requires URL and URL_HASH after the target name")
  endif()
  math(EXPR _cpkt_ep_url_start "${_cpkt_ep_url_index} + 1")
  math(EXPR _cpkt_ep_url_count "${_cpkt_ep_hash_index} - ${_cpkt_ep_url_start}")
  if(_cpkt_ep_url_count LESS 1)
    message(FATAL_ERROR "cpkt_cached_external_project_add requires at least one URL")
  endif()
  list(SUBLIST _cpkt_ep_args ${_cpkt_ep_url_start} ${_cpkt_ep_url_count} _cpkt_ep_urls)
  math(EXPR _cpkt_ep_hash_value_index "${_cpkt_ep_hash_index} + 1")
  list(GET _cpkt_ep_args ${_cpkt_ep_hash_value_index} _cpkt_ep_hash)
  string(REGEX REPLACE "^SHA256=" "" _cpkt_ep_sha256 "${_cpkt_ep_hash}")
  string(LENGTH "${_cpkt_ep_sha256}" _cpkt_ep_sha256_length)
  if(NOT _cpkt_ep_hash MATCHES "^SHA256="
      OR NOT _cpkt_ep_sha256_length EQUAL 64
      OR NOT "${_cpkt_ep_sha256}" MATCHES "^[A-Fa-f0-9]+$")
    message(FATAL_ERROR "cpkt_cached_external_project_add requires URL_HASH SHA256=<digest>")
  endif()
  list(FIND _cpkt_ep_args "DOWNLOAD_NAME" _cpkt_ep_download_name_index)
  if(_cpkt_ep_download_name_index LESS 0)
    list(GET _cpkt_ep_urls 0 _cpkt_ep_name_url)
    string(REGEX REPLACE "[?#].*$" "" _cpkt_ep_name_url "${_cpkt_ep_name_url}")
    get_filename_component(_cpkt_ep_archive_name "${_cpkt_ep_name_url}" NAME)
  else()
    math(EXPR _cpkt_ep_download_name_value_index "${_cpkt_ep_download_name_index} + 1")
    list(GET _cpkt_ep_args ${_cpkt_ep_download_name_value_index} _cpkt_ep_archive_name)
  endif()
  if(_cpkt_ep_archive_name STREQUAL "")
    message(FATAL_ERROR "cpkt_cached_external_project_add could not determine an archive name")
  endif()
  cpkt_acquire_dependency_archive(_cpkt_ep_cached_archive
    NAME "${_cpkt_ep_archive_name}"
    SHA256 "${_cpkt_ep_sha256}"
    URLS ${_cpkt_ep_urls}
    SEED_PATHS "${CPKT_DOWNLOAD_ROOT}/${_cpkt_ep_archive_name}")
  math(EXPR _cpkt_ep_remove_count "${_cpkt_ep_url_count} + 1")
  foreach(_cpkt_ep_remove_index RANGE 1 ${_cpkt_ep_remove_count})
    list(REMOVE_AT _cpkt_ep_args ${_cpkt_ep_url_index})
  endforeach()
  list(INSERT _cpkt_ep_args ${_cpkt_ep_url_index} URL "${_cpkt_ep_cached_archive}")
  ExternalProject_Add(${_cpkt_ep_args})
endmacro()

function(cpkt_record_dependency_target target_name)
  set_property(GLOBAL APPEND PROPERTY CPKT_DEPENDENCY_TARGETS "${target_name}")
endfunction()

function(cpkt_require_dependency_file path label)
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR
      "${label} was not found at ${path}\n"
      "Provide the dependency tree for this preset under CPKT_EXTERNAL_ROOT.\n"
      "You can prebuild it explicitly with scripts/deps.sh, but normal build/test/release entry points do not do that for you.")
  endif()
endfunction()

function(cpkt_normalize_prefix var path)
  file(TO_CMAKE_PATH "${path}" _normalized)
  set(${var} "${_normalized}" PARENT_SCOPE)
endfunction()

function(cpkt_get_target_triple out_var)
  string(TOLOWER "${CPKT_TARGET_OS}" _cpkt_target_os_lower)

  if(_cpkt_target_os_lower STREQUAL "darwin")
    if(CPKT_TARGET_ARCH STREQUAL "arm64")
      set(_triple "aarch64-apple-darwin")
    else()
      message(FATAL_ERROR "Unsupported Darwin CPKT_TARGET_ARCH: ${CPKT_TARGET_ARCH}")
    endif()
  elseif(CPKT_TARGET_ARCH STREQUAL "x86_64")
    if(CPKT_TARGET_LIBC STREQUAL "musl")
      set(_triple "x86_64-linux-musl")
    else()
      set(_triple "x86_64-linux-gnu")
    endif()
  elseif(CPKT_TARGET_ARCH STREQUAL "aarch64")
    if(CPKT_TARGET_LIBC STREQUAL "musl")
      set(_triple "aarch64-linux-musl")
    else()
      set(_triple "aarch64-linux-gnu")
    endif()
  elseif(CPKT_TARGET_ARCH STREQUAL "armhf")
    if(CPKT_TARGET_LIBC STREQUAL "musl")
      set(_triple "arm-linux-musleabihf")
    else()
      set(_triple "arm-linux-gnueabihf")
    endif()
  else()
    message(FATAL_ERROR "Unsupported CPKT_TARGET_ARCH: ${CPKT_TARGET_ARCH}")
  endif()

  set(${out_var} "${_triple}" PARENT_SCOPE)
endfunction()

function(cpkt_get_openssl_config_target out_var)
  string(TOLOWER "${CPKT_TARGET_OS}" _cpkt_target_os_lower)

  if(_cpkt_target_os_lower STREQUAL "darwin")
    if(CPKT_TARGET_ARCH STREQUAL "arm64")
      set(_target "darwin64-arm64")
    else()
      message(FATAL_ERROR "Unsupported Darwin CPKT_TARGET_ARCH for OpenSSL: ${CPKT_TARGET_ARCH}")
    endif()
  elseif(CPKT_TARGET_ARCH STREQUAL "x86_64")
    set(_target "linux-x86_64")
  elseif(CPKT_TARGET_ARCH STREQUAL "aarch64")
    set(_target "linux-aarch64")
  elseif(CPKT_TARGET_ARCH STREQUAL "armhf")
    set(_target "linux-armv4")
  else()
    message(FATAL_ERROR "Unsupported CPKT_TARGET_ARCH for OpenSSL: ${CPKT_TARGET_ARCH}")
  endif()

  set(${out_var} "${_target}" PARENT_SCOPE)
endfunction()

function(cpkt_get_external_c_flags out_var)
  set(_flags "-O2 -DNDEBUG -g0")
  if(CMAKE_C_COMPILER_ID MATCHES "^(AppleClang|Clang|GNU)$")
    string(APPEND _flags
      " -fmacro-prefix-map=${CPKT_DEPENDENCY_BUILD_ROOT}=deps-build"
      " -fmacro-prefix-map=${CPKT_EXTERNAL_ROOT}=deps"
    )
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    string(APPEND _flags " -include stdint.h -include sys/types.h")
  endif()
  if(NOT "${CMAKE_C_FLAGS}" STREQUAL "")
    set(_flags "${CMAKE_C_FLAGS} ${_flags}")
  endif()
  string(STRIP "${_flags}" _flags)
  set(${out_var} "${_flags}" PARENT_SCOPE)
endfunction()

function(cpkt_get_external_cxx_flags out_var)
  set(_flags "-O2 -DNDEBUG -g0")
  if(CMAKE_CXX_COMPILER_ID MATCHES "^(AppleClang|Clang|GNU)$")
    string(APPEND _flags
      " -fmacro-prefix-map=${CPKT_DEPENDENCY_BUILD_ROOT}=deps-build"
      " -fmacro-prefix-map=${CPKT_EXTERNAL_ROOT}=deps"
    )
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    string(APPEND _flags " -include stdint.h -include sys/types.h")
  endif()
  if(NOT "${CMAKE_CXX_FLAGS}" STREQUAL "")
    set(_flags "${CMAKE_CXX_FLAGS} ${_flags}")
  endif()
  string(STRIP "${_flags}" _flags)
  set(${out_var} "${_flags}" PARENT_SCOPE)
endfunction()

function(cpkt_append_external_pkg_config_env_args out_var)
  set(_args ${${out_var}})
  set(_pkg_config_dirs "")
  foreach(_prefix_var IN ITEMS
      CPKT_ZLIB_PREFIX
      CPKT_OPENSSL_static_PREFIX
      CPKT_OPENSSL_shared_PREFIX
      CPKT_NGHTTP2_static_PREFIX
      CPKT_NGHTTP2_shared_PREFIX
      CPKT_LIBSSH2_PREFIX
      CPKT_LIBXML2_PREFIX
      CPKT_LUA_PREFIX
      CPKT_MQTTC_PREFIX
      CPKT_OPEN62541_PREFIX
      CPKT_KRB5_PREFIX
      CPKT_CYRUS_SASL_PREFIX
      CPKT_OPENLDAP_PREFIX
      CPKT_POSTGRESQL_PREFIX)
    if(DEFINED ${_prefix_var} AND NOT "${${_prefix_var}}" STREQUAL "")
      list(APPEND _pkg_config_dirs
        "${${_prefix_var}}/lib/pkgconfig"
        "${${_prefix_var}}/share/pkgconfig")
    endif()
  endforeach()

  if(_pkg_config_dirs)
    list(REMOVE_DUPLICATES _pkg_config_dirs)
    string(JOIN ":" _pkg_config_libdir ${_pkg_config_dirs})
  else()
    set(_pkg_config_libdir "${CPKT_EXTERNAL_ROOT}/.pkgconfig-empty")
  endif()
  file(MAKE_DIRECTORY "${CPKT_EXTERNAL_ROOT}/.pkgconfig-empty")

  list(APPEND _args
    PKG_CONFIG_PATH=
    PKG_CONFIG_DIR=
    PKG_CONFIG_LIBDIR=${_pkg_config_libdir})
  set(${out_var} "${_args}" PARENT_SCOPE)
endfunction()

function(cpkt_append_darwin_external_env_args out_var)
  set(_args ${${out_var}})
  cpkt_append_external_pkg_config_env_args(_args)
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    foreach(_cpkt_required_var IN ITEMS
        CMAKE_C_COMPILER
        CMAKE_CXX_COMPILER
        CMAKE_AR
        CMAKE_RANLIB
        CMAKE_STRIP
        CMAKE_NM)
      if(NOT DEFINED ${_cpkt_required_var}
          OR "${${_cpkt_required_var}}" STREQUAL "")
        message(FATAL_ERROR
          "Darwin external builds require ${_cpkt_required_var}")
      endif()
    endforeach()
    list(APPEND _args
      PATH=${CPKT_OSXCROSS_BIN_DIR}:$ENV{PATH}
      LD_LIBRARY_PATH=${CPKT_OSXCROSS_ROOT}/lib:$ENV{LD_LIBRARY_PATH}
      CC=${CMAKE_C_COMPILER}
      CXX=${CMAKE_CXX_COMPILER}
      AR=${CMAKE_AR}
      RANLIB=${CMAKE_RANLIB}
      STRIP=${CMAKE_STRIP}
      NM=${CMAKE_NM}
    )
    if(CMAKE_LINKER)
      list(APPEND _args LDFLAGS=--ld-path=${CMAKE_LINKER})
    endif()
  endif()
  set(${out_var} "${_args}" PARENT_SCOPE)
endfunction()

function(cpkt_append_pinned_external_toolchain_env_args out_var)
  set(_args ${${out_var}})
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CPKT_BUILD_DEPENDENCIES)
    foreach(_cpkt_required_var IN ITEMS
        CPKT_TOOLCHAIN_ROOT
        CMAKE_C_COMPILER
        CMAKE_CXX_COMPILER
        CMAKE_LINKER
        CMAKE_AR
        CMAKE_RANLIB
        CMAKE_STRIP
        CMAKE_NM
        CMAKE_OBJCOPY
        CMAKE_OBJDUMP
        CMAKE_ADDR2LINE
        CMAKE_READELF)
      if(NOT DEFINED ${_cpkt_required_var}
          OR "${${_cpkt_required_var}}" STREQUAL "")
        message(FATAL_ERROR
          "Pinned Linux external builds require ${_cpkt_required_var}")
      endif()
    endforeach()
    set(_cpkt_toolchain_bin "${CPKT_TOOLCHAIN_ROOT}/bin")
    if(NOT IS_DIRECTORY "${_cpkt_toolchain_bin}")
      message(FATAL_ERROR
        "Pinned Linux toolchain bin directory is missing: ${_cpkt_toolchain_bin}")
    endif()
    list(APPEND _args
      PATH=${_cpkt_toolchain_bin}:$ENV{PATH}
      CC=${CMAKE_C_COMPILER}
      CXX=${CMAKE_CXX_COMPILER}
      LD=${CMAKE_LINKER}
      AR=${CMAKE_AR}
      RANLIB=${CMAKE_RANLIB}
      STRIP=${CMAKE_STRIP}
      NM=${CMAKE_NM}
      OBJCOPY=${CMAKE_OBJCOPY}
      OBJDUMP=${CMAKE_OBJDUMP}
      ADDR2LINE=${CMAKE_ADDR2LINE}
      READELF=${CMAKE_READELF})
  endif()
  cpkt_append_darwin_external_env_args(_args)
  set(${out_var} "${_args}" PARENT_SCOPE)
endfunction()

function(cpkt_get_external_cmake_step_commands build_out_var install_out_var)
  set(_build_command ${CMAKE_COMMAND} --build . --parallel ${CPKT_DEPENDENCY_BUILD_JOBS})
  set(_install_command ${CMAKE_COMMAND} --install .)
  set(_env_args "")
  cpkt_append_pinned_external_toolchain_env_args(_env_args)
  if(_env_args)
    set(_build_command ${CMAKE_COMMAND} -E env ${_env_args} ${_build_command})
    set(_install_command ${CMAKE_COMMAND} -E env ${_env_args} ${_install_command})
  endif()
  set(${build_out_var} "${_build_command}" PARENT_SCOPE)
  set(${install_out_var} "${_install_command}" PARENT_SCOPE)
endfunction()

function(cpkt_get_external_cmake_configure_command out_var)
  set(_configure_command ${CMAKE_COMMAND} -S <SOURCE_DIR> -B <BINARY_DIR>)
  if(CMAKE_GENERATOR)
    list(APPEND _configure_command -G "${CMAKE_GENERATOR}")
  endif()
  set(_env_args "")
  cpkt_append_pinned_external_toolchain_env_args(_env_args)
  if(_env_args)
    set(_configure_command ${CMAKE_COMMAND} -E env ${_env_args} ${_configure_command})
  endif()
  set(${out_var} "${_configure_command}" PARENT_SCOPE)
endfunction()

function(cpkt_get_strip_dependency_install_command out_var install_dir)
  if(NOT CMAKE_STRIP)
    message(FATAL_ERROR "CMAKE_STRIP is required when building release dependencies")
  endif()

  set(_strip_static_archives ON)
  set(_strip_shared_libraries ON)
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(_strip_static_archives OFF)
    set(_strip_shared_libraries OFF)
  endif()

  set(_command
    ${CMAKE_COMMAND}
      -DCPKT_STRIP_BIN=${CMAKE_STRIP}
      -DCPKT_STRIP_ROOT=${install_dir}
      -DCPKT_STRIP_STATIC_ARCHIVES=${_strip_static_archives}
      -DCPKT_STRIP_SHARED_LIBRARIES=${_strip_shared_libraries}
      -P ${CMAKE_SOURCE_DIR}/cmake/strip_dependency_install_tree.cmake
  )
  set(${out_var} "${_command}" PARENT_SCOPE)
endfunction()

function(cpkt_get_autotools_link_flags out_var)
  set(_flags "")
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Configure writes this value into generated Makefiles.  Preserve the
    # dollar sign until the target linker receives $ORIGIN.
    set(_flags "-Wl,--enable-new-dtags,-rpath,\\\\$$ORIGIN")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(_flags "-Wl,-rpath,@loader_path")
  endif()
  if(NOT "${CMAKE_SHARED_LINKER_FLAGS}" STREQUAL "")
    string(APPEND _flags " ${CMAKE_SHARED_LINKER_FLAGS}")
  endif()
  string(STRIP "${_flags}" _flags)
  set(${out_var} "${_flags}" PARENT_SCOPE)
endfunction()

function(cpkt_append_common_external_cmake_args out_var)
  set(_args
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_LINKER=${CMAKE_LINKER}
    -DCMAKE_AR=${CMAKE_AR}
    -DCMAKE_RANLIB=${CMAKE_RANLIB}
    -DCMAKE_STRIP=${CMAKE_STRIP}
    -DCMAKE_NM=${CMAKE_NM}
    -DCMAKE_OBJCOPY=${CMAKE_OBJCOPY}
    -DCMAKE_OBJDUMP=${CMAKE_OBJDUMP}
    -DCMAKE_ADDR2LINE=${CMAKE_ADDR2LINE}
    -DCMAKE_READELF=${CMAKE_READELF}
    -Wno-dev
  )

  if(CMAKE_TOOLCHAIN_FILE)
    list(APPEND _args -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
  endif()
  if(CMAKE_CROSSCOMPILING)
    list(APPEND _args -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY)
  endif()

  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND _args
      -DCMAKE_INSTALL_NAME_DIR=@rpath
      -DCMAKE_BUILD_WITH_INSTALL_NAME_DIR=ON
      -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON)
  endif()

  cpkt_get_external_c_flags(_cpkt_external_c_flags)
  list(APPEND _args -DCMAKE_C_FLAGS=${_cpkt_external_c_flags})

  set(${out_var} "${_args}" PARENT_SCOPE)
endfunction()

function(cpkt_append_common_external_cxx_cmake_args out_var)
  cpkt_append_common_external_cmake_args(_args)
  list(APPEND _args
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
  )

  cpkt_get_external_cxx_flags(_cpkt_external_cxx_flags)
  list(APPEND _args -DCMAKE_CXX_FLAGS=${_cpkt_external_cxx_flags})

  set(${out_var} "${_args}" PARENT_SCOPE)
endfunction()

function(cpkt_get_openssl_config_args out_var)
  cpkt_get_openssl_config_target(openssl_config_target)
  set(_args ${openssl_config_target} no-tests no-docs no-module no-apps no-makedepend)
  if(CPKT_TARGET_LIBC STREQUAL "musl")
    list(APPEND _args no-secure-memory no-afalgeng)
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Configure writes this argument into a GNU Makefile.  The Makefile must
    # retain \\$$ORIGIN: make reduces $$ to $, then the remaining backslash
    # prevents the shell from expanding $ORIGIN before it reaches the linker.
    list(APPEND _args shared "-Wl,--enable-new-dtags,-rpath,\\\\$$ORIGIN")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND _args shared "-Wl,-rpath,@loader_path")
  else()
    list(APPEND _args shared)
  endif()
  set(${out_var} "${_args}" PARENT_SCOPE)
endfunction()

function(cpkt_add_openssl)
  set(project_name "cpkt_openssl_project")
  set(prefix_dir "${CPKT_DEPENDENCY_BUILD_ROOT}/openssl")
  set(source_dir "${prefix_dir}/src")
  set(build_dir "${prefix_dir}/build")
  set(install_dir "${CPKT_EXTERNAL_ROOT}/openssl/install")
  set(stamp_dir "${prefix_dir}/stamp")
  set(tmp_dir "${prefix_dir}/tmp")
  set(openssl_dir "/etc/ssl")
  cpkt_get_openssl_config_args(config_args)

  cpkt_normalize_prefix(env_prefix "${install_dir}")
  file(MAKE_DIRECTORY "${install_dir}/include" "${install_dir}/lib")
  # OpenSSL 3.6's generated assembly dependency graph races under parallel
  # make in the pinned cross-toolchain environment.  Keep this producer
  # serial; downstream ExternalProjects can still build in parallel.
  set(build_command make -j1)
  set(install_command make -j${CPKT_DEPENDENCY_BUILD_JOBS} install_sw DESTDIR=${env_prefix})
  set(openssl_post_configure_command "")
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(openssl_post_configure_command
      COMMAND ${CMAKE_COMMAND}
        -DCPKT_DARWIN_INSTALL_NAME_FILE=${source_dir}/Makefile
        -P ${CMAKE_SOURCE_DIR}/cmake/patch_darwin_generated_install_names.cmake)
  endif()
  cpkt_get_strip_dependency_install_command(strip_install_command "${install_dir}")
  set(openssl_env_args
    CC=${CMAKE_C_COMPILER}
    AR=${CMAKE_AR}
    RANLIB=${CMAKE_RANLIB}
  )
  cpkt_get_external_c_flags(openssl_cflags)
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    string(REPLACE " -include stdint.h -include sys/types.h" "" openssl_cflags "${openssl_cflags}")
  endif()
  list(APPEND openssl_env_args CFLAGS=${openssl_cflags})
  cpkt_append_pinned_external_toolchain_env_args(openssl_env_args)

  if(CPKT_BUILD_DEPENDENCIES)
    cpkt_cached_external_project_add(${project_name}
      URL "https://github.com/openssl/openssl/releases/download/openssl-${CPKT_OPENSSL_VERSION}/openssl-${CPKT_OPENSSL_VERSION}.tar.gz"
      URL_HASH "SHA256=9bffaa1ad1e07b354c21bd3324ec02fa15579f45a7d0494b3e74bc449b7333ef"
      DOWNLOAD_NAME "openssl-${CPKT_OPENSSL_VERSION}.tar.gz"
      PREFIX "${prefix_dir}"
      DOWNLOAD_DIR "${CPKT_DOWNLOAD_ROOT}"
      SOURCE_DIR "${source_dir}"
      STAMP_DIR "${stamp_dir}"
      TMP_DIR "${tmp_dir}"
      TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_TIMEOUT}
      INACTIVITY_TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_INACTIVITY_TIMEOUT}
      PATCH_COMMAND
        ${CMAKE_COMMAND}
          -DOPENSSL_SOURCE_DIR=${source_dir}
          -P ${CMAKE_SOURCE_DIR}/cmake/patch_openssl_buildinfo.cmake
      CONFIGURE_COMMAND
        ${CMAKE_COMMAND} -E env
          ${openssl_env_args}
          "${source_dir}/Configure"
          ${config_args}
          --prefix=/
          --openssldir=${openssl_dir}
          --libdir=lib
        ${openssl_post_configure_command}
      BUILD_COMMAND ${CMAKE_COMMAND} -E env ${openssl_env_args} ${build_command}
      INSTALL_COMMAND ${CMAKE_COMMAND} -E env ${openssl_env_args} ${install_command}
        COMMAND ${strip_install_command}
      BUILD_BYPRODUCTS
        "${install_dir}/lib/libcrypto${CMAKE_STATIC_LIBRARY_SUFFIX}"
        "${install_dir}/lib/libssl${CMAKE_STATIC_LIBRARY_SUFFIX}"
        "${install_dir}/lib/libcrypto${CMAKE_SHARED_LIBRARY_SUFFIX}"
        "${install_dir}/lib/libssl${CMAKE_SHARED_LIBRARY_SUFFIX}"
      BUILD_IN_SOURCE 1
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
  endif()

  file(MAKE_DIRECTORY "${install_dir}/include" "${install_dir}/lib")

  set(openssl_crypto_static_extra_libs "")
  if(CPKT_TARGET_ARCH STREQUAL "armhf")
    list(APPEND openssl_crypto_static_extra_libs atomic)
  endif()

  add_library(cpkt::openssl_crypto_static STATIC IMPORTED GLOBAL)
  set_target_properties(cpkt::openssl_crypto_static
    PROPERTIES
      IMPORTED_LOCATION "${install_dir}/lib/libcrypto${CMAKE_STATIC_LIBRARY_SUFFIX}"
      INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
      INTERFACE_LINK_LIBRARIES "${openssl_crypto_static_extra_libs}"
  )
  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt::openssl_crypto_static ${project_name})
    cpkt_record_dependency_target(${project_name})
  else()
    cpkt_require_dependency_file("${install_dir}/lib/libcrypto${CMAKE_STATIC_LIBRARY_SUFFIX}" "OpenSSL crypto (static)")
  endif()

  add_library(cpkt::openssl_ssl_static STATIC IMPORTED GLOBAL)
  set_target_properties(cpkt::openssl_ssl_static
    PROPERTIES
      IMPORTED_LOCATION "${install_dir}/lib/libssl${CMAKE_STATIC_LIBRARY_SUFFIX}"
      INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
      INTERFACE_LINK_LIBRARIES "cpkt::openssl_crypto_static;${CMAKE_DL_LIBS};Threads::Threads"
  )
  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt::openssl_ssl_static ${project_name})
  else()
    cpkt_require_dependency_file("${install_dir}/lib/libssl${CMAKE_STATIC_LIBRARY_SUFFIX}" "OpenSSL ssl (static)")
  endif()

  add_library(cpkt::openssl_crypto_shared SHARED IMPORTED GLOBAL)
  set_target_properties(cpkt::openssl_crypto_shared
    PROPERTIES
      IMPORTED_LOCATION "${install_dir}/lib/libcrypto${CMAKE_SHARED_LIBRARY_SUFFIX}"
      INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
  )
  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt::openssl_crypto_shared ${project_name})
  else()
    cpkt_require_dependency_file("${install_dir}/lib/libcrypto${CMAKE_SHARED_LIBRARY_SUFFIX}" "OpenSSL crypto (shared)")
  endif()

  add_library(cpkt::openssl_ssl_shared SHARED IMPORTED GLOBAL)
  set_target_properties(cpkt::openssl_ssl_shared
    PROPERTIES
      IMPORTED_LOCATION "${install_dir}/lib/libssl${CMAKE_SHARED_LIBRARY_SUFFIX}"
      INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
      INTERFACE_LINK_LIBRARIES "cpkt::openssl_crypto_shared;${CMAKE_DL_LIBS};Threads::Threads"
  )
  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt::openssl_ssl_shared ${project_name})
  else()
    cpkt_require_dependency_file("${install_dir}/lib/libssl${CMAKE_SHARED_LIBRARY_SUFFIX}" "OpenSSL ssl (shared)")
  endif()

  set(CPKT_OPENSSL_static_PREFIX "${install_dir}" PARENT_SCOPE)
  set(CPKT_OPENSSL_shared_PREFIX "${install_dir}" PARENT_SCOPE)
endfunction()

function(cpkt_add_nghttp2)
  set(project_name "cpkt_nghttp2_project")
  set(prefix_dir "${CPKT_DEPENDENCY_BUILD_ROOT}/nghttp2")
  set(source_dir "${prefix_dir}/src")
  set(build_dir "${prefix_dir}/build")
  set(install_dir "${CPKT_EXTERNAL_ROOT}/nghttp2/install")
  set(stamp_dir "${prefix_dir}/stamp")
  set(tmp_dir "${prefix_dir}/tmp")
  cpkt_get_target_triple(autotools_host)
  cpkt_get_strip_dependency_install_command(strip_install_command "${install_dir}")
  file(MAKE_DIRECTORY "${install_dir}/include" "${install_dir}/lib")
  set(nghttp2_env_args
    CC=${CMAKE_C_COMPILER}
    AR=${CMAKE_AR}
    RANLIB=${CMAKE_RANLIB}
  )
  set(nghttp2_post_configure_command "")
  cpkt_get_external_c_flags(nghttp2_cflags)
  list(APPEND nghttp2_env_args CFLAGS=${nghttp2_cflags})
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_LINKER)
    set(nghttp2_post_configure_command
      COMMAND ${CMAKE_COMMAND}
        -DCPKT_DARWIN_INSTALL_NAME_FILE=${build_dir}/libtool
        -P ${CMAKE_SOURCE_DIR}/cmake/patch_darwin_generated_install_names.cmake)
  endif()
  cpkt_append_pinned_external_toolchain_env_args(nghttp2_env_args)

  if(CPKT_BUILD_DEPENDENCIES)
    cpkt_cached_external_project_add(${project_name}
      URL "https://github.com/nghttp2/nghttp2/releases/download/v${CPKT_NGHTTP2_VERSION}/nghttp2-${CPKT_NGHTTP2_VERSION}.tar.gz"
      URL_HASH "SHA256=aa317e2cf9dca6afa0aed68f8fad6ff303ec6982e25a78c75c0b65e2b9b3ded5"
      DOWNLOAD_NAME "nghttp2-${CPKT_NGHTTP2_VERSION}.tar.gz"
      PREFIX "${prefix_dir}"
      DOWNLOAD_DIR "${CPKT_DOWNLOAD_ROOT}"
      SOURCE_DIR "${source_dir}"
      BINARY_DIR "${build_dir}"
      STAMP_DIR "${stamp_dir}"
      TMP_DIR "${tmp_dir}"
      TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_TIMEOUT}
      INACTIVITY_TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_INACTIVITY_TIMEOUT}
      CONFIGURE_COMMAND
        ${CMAKE_COMMAND} -E env
        ${nghttp2_env_args}
        "${source_dir}/configure"
        --prefix=${install_dir}
        --host=${autotools_host}
        --enable-shared
        --enable-static
        --with-pic
        --enable-lib-only
        ${nghttp2_post_configure_command}
      BUILD_COMMAND ${CMAKE_COMMAND} -E env ${nghttp2_env_args} make -C lib -j${CPKT_DEPENDENCY_BUILD_JOBS}
      INSTALL_COMMAND ${CMAKE_COMMAND} -E env ${nghttp2_env_args} make -C lib install
        COMMAND ${strip_install_command}
      BUILD_BYPRODUCTS
        "${install_dir}/lib/libnghttp2${CMAKE_STATIC_LIBRARY_SUFFIX}"
        "${install_dir}/lib/libnghttp2${CMAKE_SHARED_LIBRARY_SUFFIX}"
      BUILD_IN_SOURCE 0
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
  endif()

  add_library(cpkt::nghttp2_static STATIC IMPORTED GLOBAL)
  set_target_properties(cpkt::nghttp2_static
    PROPERTIES
      IMPORTED_LOCATION "${install_dir}/lib/libnghttp2${CMAKE_STATIC_LIBRARY_SUFFIX}"
      INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
  )
  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt::nghttp2_static ${project_name})
    cpkt_record_dependency_target(${project_name})
  else()
    cpkt_require_dependency_file("${install_dir}/lib/libnghttp2${CMAKE_STATIC_LIBRARY_SUFFIX}" "nghttp2 (static)")
  endif()

  add_library(cpkt::nghttp2_shared SHARED IMPORTED GLOBAL)
  set_target_properties(cpkt::nghttp2_shared
    PROPERTIES
      IMPORTED_LOCATION "${install_dir}/lib/libnghttp2${CMAKE_SHARED_LIBRARY_SUFFIX}"
      INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
  )
  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt::nghttp2_shared ${project_name})
  else()
    cpkt_require_dependency_file("${install_dir}/lib/libnghttp2${CMAKE_SHARED_LIBRARY_SUFFIX}" "nghttp2 (shared)")
  endif()

  set(CPKT_NGHTTP2_static_PREFIX "${install_dir}" PARENT_SCOPE)
  set(CPKT_NGHTTP2_shared_PREFIX "${install_dir}" PARENT_SCOPE)
endfunction()

function(cpkt_add_zlib)
  set(project_name "cpkt_zlib_project")
  set(prefix_dir "${CPKT_DEPENDENCY_BUILD_ROOT}/zlib")
  set(source_dir "${prefix_dir}/src")
  set(build_dir "${prefix_dir}/build")
  set(install_dir "${CPKT_EXTERNAL_ROOT}/zlib/install")
  set(stamp_dir "${prefix_dir}/stamp")
  set(tmp_dir "${prefix_dir}/tmp")
  cpkt_append_common_external_cmake_args(common_cmake_args)
  cpkt_get_external_cmake_step_commands(cmake_build_command cmake_install_command)
  cpkt_get_strip_dependency_install_command(strip_install_command "${install_dir}")
  file(MAKE_DIRECTORY "${install_dir}/include" "${install_dir}/lib")

  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(zlib_shared_library "${install_dir}/lib/libz.${CPKT_ZLIB_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(zlib_shared_soname "${install_dir}/lib/libz.1${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(zlib_shared_link "${install_dir}/lib/libz${CMAKE_SHARED_LIBRARY_SUFFIX}")
  else()
    set(zlib_shared_library "${install_dir}/lib/libz${CMAKE_SHARED_LIBRARY_SUFFIX}.${CPKT_ZLIB_VERSION}")
    set(zlib_shared_soname "${install_dir}/lib/libz${CMAKE_SHARED_LIBRARY_SUFFIX}.1")
    set(zlib_shared_link "${install_dir}/lib/libz${CMAKE_SHARED_LIBRARY_SUFFIX}")
  endif()
  set(zlib_static_library "${install_dir}/lib/libz${CMAKE_STATIC_LIBRARY_SUFFIX}")

  if(CPKT_BUILD_DEPENDENCIES)
    cpkt_cached_external_project_add(${project_name}
      URL
        "https://www.zlib.net/zlib-${CPKT_ZLIB_VERSION}.tar.gz"
        "https://zlib.net/fossils/zlib-${CPKT_ZLIB_VERSION}.tar.gz"
      URL_HASH "SHA256=bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16"
      DOWNLOAD_NAME "zlib-${CPKT_ZLIB_VERSION}.tar.gz"
      PREFIX "${prefix_dir}"
      DOWNLOAD_DIR "${CPKT_DOWNLOAD_ROOT}"
      SOURCE_DIR "${source_dir}"
      BINARY_DIR "${build_dir}"
      STAMP_DIR "${stamp_dir}"
      TMP_DIR "${tmp_dir}"
      TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_TIMEOUT}
      INACTIVITY_TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_INACTIVITY_TIMEOUT}
      PATCH_COMMAND ${CMAKE_COMMAND}
        -DCPKT_ZLIB_SOURCE_DIR=<SOURCE_DIR>
        -P ${CMAKE_SOURCE_DIR}/cmake/patch_zlib_single_pass.cmake
      CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${install_dir}
        -DCMAKE_INSTALL_LIBDIR=lib
        -DCMAKE_BUILD_TYPE=${CPKT_DEPENDENCY_BUILD_TYPE}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DZLIB_BUILD_SHARED=ON
        -DZLIB_BUILD_STATIC=ON
        -DZLIB_BUILD_TESTING=OFF
        -DZLIB_INSTALL=ON
        ${common_cmake_args}
      BUILD_COMMAND ${cmake_build_command}
      INSTALL_COMMAND ${cmake_install_command}
        COMMAND ${strip_install_command}
      BUILD_BYPRODUCTS
        "${zlib_static_library}"
        "${zlib_shared_library}"
        "${zlib_shared_soname}"
        "${zlib_shared_link}"
      BUILD_IN_SOURCE 0
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
  endif()

  add_library(cpkt::zlib_static STATIC IMPORTED GLOBAL)
  set_target_properties(cpkt::zlib_static
    PROPERTIES
      IMPORTED_LOCATION "${zlib_static_library}"
      INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
  )

  add_library(cpkt::zlib_shared SHARED IMPORTED GLOBAL)
  set_target_properties(cpkt::zlib_shared
    PROPERTIES
      IMPORTED_LOCATION "${zlib_shared_library}"
      INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
  )

  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt::zlib_static ${project_name})
    add_dependencies(cpkt::zlib_shared ${project_name})
    cpkt_record_dependency_target(${project_name})
  else()
    cpkt_require_dependency_file("${zlib_static_library}" "zlib static library")
    cpkt_require_dependency_file("${zlib_shared_library}" "zlib shared library")
    cpkt_require_dependency_file("${zlib_shared_soname}" "zlib shared-library SONAME")
    cpkt_require_dependency_file("${zlib_shared_link}" "zlib shared-library linker symlink")
    cpkt_require_dependency_file("${install_dir}/include/zlib.h" "zlib header")
    cpkt_require_dependency_file("${install_dir}/include/zconf.h" "zlib configuration header")
  endif()

  set(CPKT_ZLIB_PREFIX "${install_dir}" PARENT_SCOPE)
  set(CPKT_ZLIB_SHARED_LIBRARY "${zlib_shared_library}" PARENT_SCOPE)
endfunction()

function(cpkt_add_libssh2)
  set(project_name "cpkt_libssh2_project")
  set(openssl_project "")
  set(prefix_dir "${CPKT_DEPENDENCY_BUILD_ROOT}/libssh2")
  set(source_dir "${prefix_dir}/src")
  set(build_dir "${prefix_dir}/build")
  set(install_dir "${CPKT_EXTERNAL_ROOT}/libssh2/install")
  set(stamp_dir "${prefix_dir}/stamp")
  set(tmp_dir "${prefix_dir}/tmp")
  cpkt_append_common_external_cmake_args(common_cmake_args)
  cpkt_get_external_cmake_step_commands(cmake_build_command cmake_install_command)
  cpkt_get_strip_dependency_install_command(strip_install_command "${install_dir}")
  file(MAKE_DIRECTORY "${install_dir}/include" "${install_dir}/lib")

  set(libssh2_shared_library "${install_dir}/lib/libssh2${CMAKE_SHARED_LIBRARY_SUFFIX}")
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(libssh2_shared_library "${install_dir}/lib/libssh2.1${CMAKE_SHARED_LIBRARY_SUFFIX}")
  endif()
  set(libssh2_static_library "${install_dir}/lib/libssh2${CMAKE_STATIC_LIBRARY_SUFFIX}")
  if(NOT DEFINED CPKT_ZLIB_PREFIX OR "${CPKT_ZLIB_PREFIX}" STREQUAL "")
    message(FATAL_ERROR "libssh2 requires zlib to be configured first")
  endif()
  if(DEFINED CPKT_OPENSSL_shared_PREFIX AND NOT "${CPKT_OPENSSL_shared_PREFIX}" STREQUAL "")
    set(libssh2_openssl_prefix "${CPKT_OPENSSL_shared_PREFIX}")
    set(libssh2_openssl_build_variant "shared")
    set(openssl_project "cpkt_openssl_project")
    set(libssh2_openssl_ssl_library "${libssh2_openssl_prefix}/lib/libssl${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(libssh2_openssl_crypto_library "${libssh2_openssl_prefix}/lib/libcrypto${CMAKE_SHARED_LIBRARY_SUFFIX}")
  elseif(DEFINED CPKT_OPENSSL_static_PREFIX AND NOT "${CPKT_OPENSSL_static_PREFIX}" STREQUAL "")
    set(libssh2_openssl_prefix "${CPKT_OPENSSL_static_PREFIX}")
    set(libssh2_openssl_build_variant "static")
    set(openssl_project "cpkt_openssl_project")
    set(libssh2_openssl_ssl_library "${libssh2_openssl_prefix}/lib/libssl${CMAKE_STATIC_LIBRARY_SUFFIX}")
    set(libssh2_openssl_crypto_library "${libssh2_openssl_prefix}/lib/libcrypto${CMAKE_STATIC_LIBRARY_SUFFIX}")
  else()
    message(FATAL_ERROR "libssh2 requires OpenSSL to be configured first")
  endif()
  if(DEFINED CPKT_OPENSSL_static_PREFIX AND NOT "${CPKT_OPENSSL_static_PREFIX}" STREQUAL "")
    set(libssh2_openssl_link_variant "static")
  else()
    set(libssh2_openssl_link_variant "${libssh2_openssl_build_variant}")
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(libssh2_install_rpath "@loader_path")
    set(libssh2_platform_cmake_args "")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(libssh2_install_rpath "$ORIGIN")
    set(libssh2_platform_cmake_args
      -DCMAKE_SHARED_LINKER_FLAGS=-Wl,--enable-new-dtags)
  else()
    set(libssh2_install_rpath "")
    set(libssh2_platform_cmake_args "")
  endif()

  if(CPKT_BUILD_DEPENDENCIES)
    cpkt_cached_external_project_add(${project_name}
      URL "https://libssh2.org/download/libssh2-${CPKT_LIBSSH2_VERSION}.tar.gz"
      URL_HASH "SHA256=d9ec76cbe34db98eec3539fe2c899d26b0c837cb3eb466a56b0f109cabf658f7"
      DOWNLOAD_NAME "libssh2-${CPKT_LIBSSH2_VERSION}.tar.gz"
      PREFIX "${prefix_dir}"
      DOWNLOAD_DIR "${CPKT_DOWNLOAD_ROOT}"
      SOURCE_DIR "${source_dir}"
      BINARY_DIR "${build_dir}"
      STAMP_DIR "${stamp_dir}"
      TMP_DIR "${tmp_dir}"
      TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_TIMEOUT}
      INACTIVITY_TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_INACTIVITY_TIMEOUT}
      PATCH_COMMAND ${CMAKE_COMMAND}
        -DCPKT_LIBSSH2_SOURCE_DIR=<SOURCE_DIR>
        -P ${CMAKE_SOURCE_DIR}/cmake/patch_libssh2_single_pass.cmake
      CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${install_dir}
        -DCMAKE_INSTALL_LIBDIR=lib
        -DCMAKE_BUILD_TYPE=${CPKT_DEPENDENCY_BUILD_TYPE}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DCMAKE_FIND_PACKAGE_PREFER_CONFIG=ON
        -DCMAKE_INSTALL_RPATH=${libssh2_install_rpath}
        -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=OFF
        -DCMAKE_BUILD_RPATH=
        -DCMAKE_SKIP_INSTALL_RPATH=OFF
        ${libssh2_platform_cmake_args}
        -DBUILD_STATIC_LIBS=ON
        -DBUILD_SHARED_LIBS=ON
        -DBUILD_EXAMPLES=OFF
        -DBUILD_TESTING=OFF
        -DENABLE_ZLIB_COMPRESSION=ON
        -DCRYPTO_BACKEND=OpenSSL
        -DOpenSSL_DIR=${libssh2_openssl_prefix}/lib/cmake/OpenSSL
        -DZLIB_ROOT=${CPKT_ZLIB_PREFIX}
        -DZLIB_DIR=${CPKT_ZLIB_PREFIX}/lib/cmake/zlib
        -DZLIB_INCLUDE_DIRS=${CPKT_ZLIB_PREFIX}/include
        -DZLIB_LIBRARIES=${CPKT_ZLIB_SHARED_LIBRARY}
        ${common_cmake_args}
      DEPENDS
        ${openssl_project}
        cpkt_zlib_project
      BUILD_COMMAND ${cmake_build_command}
      INSTALL_COMMAND ${cmake_install_command}
        COMMAND ${strip_install_command}
      BUILD_BYPRODUCTS
        "${libssh2_static_library}"
        "${libssh2_shared_library}"
      BUILD_IN_SOURCE 0
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
  endif()

  add_library(cpkt::libssh2_static STATIC IMPORTED GLOBAL)
  set_target_properties(cpkt::libssh2_static
    PROPERTIES
      IMPORTED_LOCATION "${libssh2_static_library}"
      INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
      INTERFACE_LINK_LIBRARIES "cpkt::openssl_crypto_${libssh2_openssl_link_variant};cpkt::zlib_static"
  )

  add_library(cpkt::libssh2_shared SHARED IMPORTED GLOBAL)
  set_target_properties(cpkt::libssh2_shared
    PROPERTIES
      IMPORTED_LOCATION "${libssh2_shared_library}"
      INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
      INTERFACE_LINK_LIBRARIES "cpkt::zlib_shared"
  )

  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt::libssh2_static ${project_name})
    add_dependencies(cpkt::libssh2_shared ${project_name})
    cpkt_record_dependency_target(${project_name})
  else()
    cpkt_require_dependency_file("${libssh2_static_library}" "libssh2 static library")
    cpkt_require_dependency_file("${libssh2_shared_library}" "libssh2 shared library")
    cpkt_require_dependency_file("${install_dir}/include/libssh2.h" "libssh2 header")
    cpkt_require_dependency_file("${install_dir}/include/libssh2_publickey.h" "libssh2 publickey header")
    cpkt_require_dependency_file("${install_dir}/include/libssh2_sftp.h" "libssh2 sftp header")
  endif()

  set(CPKT_LIBSSH2_PREFIX "${install_dir}" PARENT_SCOPE)
endfunction()

function(cpkt_get_curl_platform_cmake_args out_var)
  set(_args "")
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND _args
      -DENABLE_THREADED_RESOLVER=OFF
      -DUSE_APPLE_SECTRUST=ON)
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Cross-compiling disables curl's CA bundle/path auto-detection.  Ask its
    # OpenSSL backend to load the target system's configured default trust
    # locations when callers do not supply CURLOPT_CAINFO or CURLOPT_CAPATH.
    list(APPEND _args
      -DCMAKE_SHARED_LINKER_FLAGS=-Wl,--enable-new-dtags
      -DCURL_CA_FALLBACK=ON)
  endif()
  set(${out_var} "${_args}" PARENT_SCOPE)
endfunction()

function(cpkt_get_curl_static_platform_libs out_var)
  set(_libs "")
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND _libs
      "-framework SystemConfiguration"
      "-framework Security"
      "-framework CoreFoundation"
      "-framework CoreServices")
  endif()
  set(${out_var} "${_libs}" PARENT_SCOPE)
endfunction()

function(cpkt_add_curl)
  set(project_name "cpkt_curl_project")
  set(openssl_project "cpkt_openssl_project")
  set(nghttp2_project "cpkt_nghttp2_project")
  set(libssh2_project "cpkt_libssh2_project")
  set(zlib_project "cpkt_zlib_project")
  set(prefix_dir "${CPKT_DEPENDENCY_BUILD_ROOT}/curl")
  set(install_dir "${CPKT_EXTERNAL_ROOT}/curl/install")
  set(openssl_prefix "${CPKT_OPENSSL_shared_PREFIX}")
  set(nghttp2_prefix "${CPKT_NGHTTP2_shared_PREFIX}")
  set(libssh2_prefix "${CPKT_LIBSSH2_PREFIX}")
  set(curl_download_name "curl-${CPKT_CURL_VERSION}.tar.xz")
  set(curl_openssl_ssl_library "${openssl_prefix}/lib/libssl${CMAKE_SHARED_LIBRARY_SUFFIX}")
  set(curl_openssl_crypto_library "${openssl_prefix}/lib/libcrypto${CMAKE_SHARED_LIBRARY_SUFFIX}")
  set(curl_nghttp2_library "${nghttp2_prefix}/lib/libnghttp2${CMAKE_SHARED_LIBRARY_SUFFIX}")
  set(curl_libssh2_library "${libssh2_prefix}/lib/libssh2${CMAKE_SHARED_LIBRARY_SUFFIX}")
  set(curl_zlib_library "${CPKT_ZLIB_PREFIX}/lib/libz${CMAKE_SHARED_LIBRARY_SUFFIX}")
  set(source_dir "${prefix_dir}/src")
  set(build_dir "${prefix_dir}/build")
  set(stamp_dir "${prefix_dir}/stamp")
  set(tmp_dir "${prefix_dir}/tmp")
  cpkt_append_common_external_cmake_args(common_cmake_args)
  cpkt_get_external_cmake_step_commands(cmake_build_command cmake_install_command)
  cpkt_get_strip_dependency_install_command(strip_install_command "${install_dir}")
  file(MAKE_DIRECTORY "${install_dir}/include" "${install_dir}/lib")
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(curl_install_rpath "@loader_path")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(curl_install_rpath "$ORIGIN")
  else()
    set(curl_install_rpath "")
  endif()
  cpkt_get_curl_platform_cmake_args(curl_platform_cmake_args)
  cpkt_get_curl_static_platform_libs(curl_static_platform_libs)
  cpkt_get_external_cmake_configure_command(cmake_configure_command)
  set(curl_cmake_args
    -DCMAKE_INSTALL_PREFIX=${install_dir}
    -DCMAKE_INSTALL_LIBDIR=lib
    -DCMAKE_DEBUG_POSTFIX=
    -DCMAKE_BUILD_TYPE=${CPKT_DEPENDENCY_BUILD_TYPE}
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DCMAKE_INSTALL_RPATH=${curl_install_rpath}
    -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=OFF
    -DCMAKE_BUILD_RPATH=
    -DCMAKE_SKIP_INSTALL_RPATH=OFF
    ${curl_platform_cmake_args}
    -DBUILD_SHARED_LIBS=ON
    -DBUILD_STATIC_LIBS=ON
    -DSHARE_LIB_OBJECT=ON
    -DBUILD_CURL_EXE=OFF
    -DBUILD_EXAMPLES=OFF
    -DBUILD_LIBCURL_DOCS=OFF
    -DBUILD_MISC_DOCS=OFF
    -DBUILD_TESTING=OFF
    -DCURL_DISABLE_INSTALL=OFF
    -DCURL_USE_PKGCONFIG=OFF
    -DCURL_USE_OPENSSL=ON
    -DCURL_USE_LIBSSH2=ON
    -DCURL_USE_LIBSSH=OFF
    -DUSE_NGHTTP2=ON
    # libpq's OAuth support requires libcurl asynchronous DNS.  Keep this
    # resolver self-contained rather than introducing c-ares as another
    # bundled dependency.
    -DENABLE_THREADED_RESOLVER=ON
    -DCURL_DISABLE_LDAP=ON
    -DCURL_DISABLE_LDAPS=ON
    -DCURL_ZLIB=ON
    -DCURL_BROTLI=OFF
    -DCURL_ZSTD=OFF
    -DCURL_USE_LIBPSL=OFF
    -DUSE_LIBRTMP=OFF
    -DUSE_LIBIDN2=OFF
    -DZLIB_ROOT=${CPKT_ZLIB_PREFIX}
    -DZLIB_INCLUDE_DIR=${CPKT_ZLIB_PREFIX}/include
    -DZLIB_LIBRARY=${curl_zlib_library}
    -DOPENSSL_ROOT_DIR=${openssl_prefix}
    -DOPENSSL_INCLUDE_DIR=${openssl_prefix}/include
    -DOPENSSL_SSL_LIBRARY=${curl_openssl_ssl_library}
    -DOPENSSL_CRYPTO_LIBRARY=${curl_openssl_crypto_library}
    -DNGHTTP2_INCLUDE_DIR=${nghttp2_prefix}/include
    -DNGHTTP2_LIBRARY=${curl_nghttp2_library}
    -DLIBSSH2_INCLUDE_DIR=${libssh2_prefix}/include
    -DLIBSSH2_LIBRARY=${curl_libssh2_library}
    ${common_cmake_args}
  )

  if(CPKT_BUILD_DEPENDENCIES)
    cpkt_cached_external_project_add(${project_name}
      URL "https://curl.se/download/curl-${CPKT_CURL_VERSION}.tar.xz"
      URL_HASH "SHA256=f7ef3ae8a22e521f289803fe93543eb64c329b58aa73a9e224dfd915a2a5f4f7"
      DOWNLOAD_NAME "${curl_download_name}"
      PREFIX "${prefix_dir}"
      DOWNLOAD_DIR "${CPKT_DOWNLOAD_ROOT}"
      SOURCE_DIR "${source_dir}"
      BINARY_DIR "${build_dir}"
      STAMP_DIR "${stamp_dir}"
      TMP_DIR "${tmp_dir}"
      TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_TIMEOUT}
      INACTIVITY_TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_INACTIVITY_TIMEOUT}
      DEPENDS
        ${zlib_project}
        ${openssl_project}
        ${nghttp2_project}
        ${libssh2_project}
      CONFIGURE_COMMAND ${cmake_configure_command} ${curl_cmake_args}
      BUILD_COMMAND ${cmake_build_command}
      INSTALL_COMMAND ${cmake_install_command}
        COMMAND ${strip_install_command}
      BUILD_BYPRODUCTS
        "${install_dir}/lib/libcurl${CMAKE_STATIC_LIBRARY_SUFFIX}"
        "${install_dir}/lib/libcurl${CMAKE_SHARED_LIBRARY_SUFFIX}"
      BUILD_IN_SOURCE 0
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
  endif()

  add_library(cpkt::curl_static STATIC IMPORTED GLOBAL)
  set_target_properties(cpkt::curl_static
    PROPERTIES
      IMPORTED_LOCATION "${install_dir}/lib/libcurl${CMAKE_STATIC_LIBRARY_SUFFIX}"
      INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
      INTERFACE_LINK_LIBRARIES "cpkt::libssh2_static;cpkt::nghttp2_static;cpkt::openssl_ssl_static;cpkt::openssl_crypto_static;cpkt::zlib_static;${CMAKE_DL_LIBS};Threads::Threads;${curl_static_platform_libs}"
  )
  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt::curl_static ${project_name})
    cpkt_record_dependency_target(${project_name})
  else()
    cpkt_require_dependency_file("${install_dir}/lib/libcurl${CMAKE_STATIC_LIBRARY_SUFFIX}" "curl (static)")
  endif()

  add_library(cpkt::curl_shared SHARED IMPORTED GLOBAL)
  set_target_properties(cpkt::curl_shared
    PROPERTIES
      IMPORTED_LOCATION "${install_dir}/lib/libcurl${CMAKE_SHARED_LIBRARY_SUFFIX}"
      INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
      INTERFACE_LINK_LIBRARIES "cpkt::openssl_ssl_shared;cpkt::openssl_crypto_shared;cpkt::nghttp2_shared;cpkt::libssh2_shared;cpkt::zlib_shared;${CMAKE_DL_LIBS};Threads::Threads"
  )
  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt::curl_shared ${project_name})
  else()
    cpkt_require_dependency_file("${install_dir}/lib/libcurl${CMAKE_SHARED_LIBRARY_SUFFIX}" "curl (shared)")
  endif()

  set(CPKT_CURL_PREFIX "${install_dir}" PARENT_SCOPE)

endfunction()

function(cpkt_add_libxml2)
  set(project_name_shared "cpkt_libxml2_shared_project")
  set(project_name_static "cpkt_libxml2_static_project")
  set(prefix_dir "${CPKT_DEPENDENCY_BUILD_ROOT}/libxml2")
  set(source_dir "${prefix_dir}/src")
  set(shared_build_dir "${prefix_dir}/build-shared")
  set(static_build_dir "${prefix_dir}/build-static")
  set(install_dir "${CPKT_EXTERNAL_ROOT}/libxml2/install")
  set(stamp_dir "${prefix_dir}/stamp")
  set(tmp_dir "${prefix_dir}/tmp")
  cpkt_append_common_external_cmake_args(common_cmake_args)
  cpkt_get_external_cmake_configure_command(cmake_configure_command)
  cpkt_get_external_cmake_step_commands(cmake_build_command cmake_install_command)
  cpkt_get_strip_dependency_install_command(strip_install_command "${install_dir}")
  find_package(Iconv REQUIRED)
  set(libxml2_static_iconv_link_libraries Iconv::Iconv)
  set(libxml2_shared_iconv_link_libraries Iconv::Iconv)
  if(CPKT_TARGET_ID STREQUAL "arm64-apple-darwin")
    list(APPEND libxml2_static_iconv_link_libraries iconv)
    list(APPEND libxml2_shared_iconv_link_libraries iconv)
  endif()
  file(MAKE_DIRECTORY "${install_dir}/include/libxml2" "${install_dir}/lib")

  set(libxml2_static_library "${install_dir}/lib/libxml2${CMAKE_STATIC_LIBRARY_SUFFIX}")
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(libxml2_shared_library "${install_dir}/lib/libxml2.16${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(libxml2_shared_link "${install_dir}/lib/libxml2${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(libxml2_install_rpath "@loader_path")
    set(libxml2_platform_cmake_args
      -DCMAKE_SHARED_LINKER_FLAGS=-liconv)
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(libxml2_shared_library "${install_dir}/lib/libxml2${CMAKE_SHARED_LIBRARY_SUFFIX}.16.1.4")
    set(libxml2_shared_link "${install_dir}/lib/libxml2${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(libxml2_install_rpath "$ORIGIN")
    set(libxml2_platform_cmake_args
      -DCMAKE_SHARED_LINKER_FLAGS=-Wl,--enable-new-dtags)
  else()
    set(libxml2_shared_library "${install_dir}/lib/libxml2${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(libxml2_shared_link "${libxml2_shared_library}")
    set(libxml2_install_rpath "")
    set(libxml2_platform_cmake_args "")
  endif()

  set(libxml2_common_cmake_args
    -DCMAKE_INSTALL_PREFIX=${install_dir}
    -DCMAKE_INSTALL_LIBDIR=lib
    -DCMAKE_INSTALL_SYSCONFDIR=/etc
    -DCMAKE_BUILD_TYPE=${CPKT_DEPENDENCY_BUILD_TYPE}
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DCMAKE_FIND_PACKAGE_PREFER_CONFIG=ON
    -DCMAKE_INSTALL_RPATH=${libxml2_install_rpath}
    -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=OFF
    -DCMAKE_BUILD_RPATH=
    -DCMAKE_SKIP_INSTALL_RPATH=OFF
    ${libxml2_platform_cmake_args}
    -DLIBXML2_WITH_CATALOG=ON
    -DLIBXML2_WITH_C14N=ON
    -DLIBXML2_WITH_DEBUG=ON
    -DLIBXML2_WITH_DOCS=OFF
    -DLIBXML2_WITH_HTML=ON
    -DLIBXML2_WITH_HTTP=OFF
    -DLIBXML2_WITH_ICONV=ON
    -DLIBXML2_WITH_ICU=OFF
    -DLIBXML2_WITH_LEGACY=OFF
    -DLIBXML2_WITH_MODULES=ON
    -DLIBXML2_WITH_OUTPUT=ON
    -DLIBXML2_WITH_PATTERN=ON
    -DLIBXML2_WITH_PROGRAMS=OFF
    -DLIBXML2_WITH_PUSH=ON
    -DLIBXML2_WITH_PYTHON=OFF
    -DLIBXML2_WITH_READLINE=OFF
    -DLIBXML2_WITH_READER=ON
    -DLIBXML2_WITH_REGEXPS=ON
    -DLIBXML2_WITH_RELAXNG=ON
    -DLIBXML2_WITH_SAX1=ON
    -DLIBXML2_WITH_SCHEMAS=ON
    -DLIBXML2_WITH_SCHEMATRON=ON
    -DLIBXML2_WITH_TESTS=OFF
    -DLIBXML2_WITH_THREADS=ON
    -DLIBXML2_WITH_THREAD_ALLOC=ON
    -DLIBXML2_WITH_TLS=ON
    -DLIBXML2_WITH_VALID=ON
    -DLIBXML2_WITH_WRITER=ON
    -DLIBXML2_WITH_XINCLUDE=ON
    -DLIBXML2_WITH_XPATH=ON
    -DLIBXML2_WITH_XPTR=ON
    -DLIBXML2_WITH_ZLIB=ON
    -DZLIB_ROOT=${CPKT_ZLIB_PREFIX}
    -DZLIB_DIR=${CPKT_ZLIB_PREFIX}/lib/cmake/zlib
    -DZLIB_INCLUDE_DIR=${CPKT_ZLIB_PREFIX}/include
    ${common_cmake_args}
  )

  if(CPKT_BUILD_DEPENDENCIES)
    cpkt_cached_external_project_add(${project_name_shared}
      URL "https://download.gnome.org/sources/libxml2/2.15/libxml2-${CPKT_LIBXML2_VERSION}.tar.xz"
      URL_HASH "SHA256=98087fd181d9070724f3fbc65c7377db03038eb92bd882374daff44940138821"
      DOWNLOAD_NAME "libxml2-${CPKT_LIBXML2_VERSION}.tar.xz"
      PREFIX "${prefix_dir}"
      DOWNLOAD_DIR "${CPKT_DOWNLOAD_ROOT}"
      SOURCE_DIR "${source_dir}"
      BINARY_DIR "${shared_build_dir}"
      STAMP_DIR "${stamp_dir}/shared"
      TMP_DIR "${tmp_dir}"
      TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_TIMEOUT}
      INACTIVITY_TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_INACTIVITY_TIMEOUT}
      DEPENDS cpkt_zlib_project
      CONFIGURE_COMMAND ${cmake_configure_command}
        -DBUILD_SHARED_LIBS=ON
        ${libxml2_common_cmake_args}
        -DZLIB_LIBRARY=${CPKT_ZLIB_SHARED_LIBRARY}
      BUILD_COMMAND ${cmake_build_command}
      INSTALL_COMMAND ${cmake_install_command}
      BUILD_BYPRODUCTS "${libxml2_shared_library}"
      BUILD_IN_SOURCE 0
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    cpkt_cached_external_project_add(${project_name_static}
      URL "https://download.gnome.org/sources/libxml2/2.15/libxml2-${CPKT_LIBXML2_VERSION}.tar.xz"
      URL_HASH "SHA256=98087fd181d9070724f3fbc65c7377db03038eb92bd882374daff44940138821"
      DOWNLOAD_NAME "libxml2-${CPKT_LIBXML2_VERSION}.tar.xz"
      PREFIX "${prefix_dir}"
      DOWNLOAD_DIR "${CPKT_DOWNLOAD_ROOT}"
      SOURCE_DIR "${source_dir}"
      BINARY_DIR "${static_build_dir}"
      STAMP_DIR "${stamp_dir}/static"
      TMP_DIR "${tmp_dir}"
      TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_TIMEOUT}
      INACTIVITY_TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_INACTIVITY_TIMEOUT}
      DEPENDS ${project_name_shared}
      CONFIGURE_COMMAND ${cmake_configure_command}
        -DBUILD_SHARED_LIBS=OFF
        ${libxml2_common_cmake_args}
        -DZLIB_LIBRARY=${CPKT_ZLIB_PREFIX}/lib/libz${CMAKE_STATIC_LIBRARY_SUFFIX}
      BUILD_COMMAND ${cmake_build_command}
      INSTALL_COMMAND ${cmake_install_command}
        COMMAND ${strip_install_command}
      BUILD_BYPRODUCTS "${libxml2_static_library}"
      BUILD_IN_SOURCE 0
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
  endif()

  add_library(cpkt::libxml2_static STATIC IMPORTED GLOBAL)
  set_target_properties(cpkt::libxml2_static
    PROPERTIES
      IMPORTED_LOCATION "${libxml2_static_library}"
      INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include/libxml2"
      INTERFACE_LINK_LIBRARIES "cpkt::zlib_static;${libxml2_static_iconv_link_libraries};${CMAKE_DL_LIBS};Threads::Threads;m"
  )

  add_library(cpkt::libxml2_shared SHARED IMPORTED GLOBAL)
  set_target_properties(cpkt::libxml2_shared
    PROPERTIES
      IMPORTED_LOCATION "${libxml2_shared_library}"
      INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include/libxml2"
      INTERFACE_LINK_LIBRARIES "cpkt::zlib_shared;${libxml2_shared_iconv_link_libraries};${CMAKE_DL_LIBS};Threads::Threads;m"
  )

  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt::libxml2_static ${project_name_static})
    add_dependencies(cpkt::libxml2_shared ${project_name_shared})
    cpkt_record_dependency_target(${project_name_static})
  else()
    cpkt_require_dependency_file("${libxml2_static_library}" "libxml2 static library")
    cpkt_require_dependency_file("${libxml2_shared_library}" "libxml2 shared library")
    cpkt_require_dependency_file("${libxml2_shared_link}" "libxml2 shared-library linker symlink")
    cpkt_require_dependency_file("${install_dir}/include/libxml2/libxml/parser.h" "libxml2 parser header")
  endif()

  set(CPKT_LIBXML2_PREFIX "${install_dir}" PARENT_SCOPE)
endfunction()

function(cpkt_add_lua)
  set(project_name "cpkt_lua_project")
  set(prefix_dir "${CPKT_DEPENDENCY_BUILD_ROOT}/lua")
  set(source_dir "${prefix_dir}/src")
  set(install_dir "${CPKT_EXTERNAL_ROOT}/lua/install")
  set(stamp_dir "${prefix_dir}/stamp")
  set(tmp_dir "${prefix_dir}/tmp")
  cpkt_get_strip_dependency_install_command(strip_install_command "${install_dir}")
  file(MAKE_DIRECTORY "${install_dir}/include" "${install_dir}/lib")

  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(lua_shared_library "liblua.${CPKT_LUA_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(lua_shared_soname "liblua.5.5${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(lua_shared_link "liblua${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(lua_shared_link_flags -dynamiclib -Wl,-install_name,@rpath/${lua_shared_soname})
    set(lua_shared_libs -lm)
  else()
    set(lua_shared_library "liblua${CMAKE_SHARED_LIBRARY_SUFFIX}.${CPKT_LUA_VERSION}")
    set(lua_shared_soname "liblua${CMAKE_SHARED_LIBRARY_SUFFIX}.5.5")
    set(lua_shared_link "liblua${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(lua_shared_link_flags -shared -Wl,-soname,${lua_shared_soname})
    set(lua_shared_libs -lm)
    if(CMAKE_DL_LIBS)
      list(APPEND lua_shared_libs "-l${CMAKE_DL_LIBS}")
    endif()
  endif()
  set(lua_static_library "${install_dir}/lib/liblua${CMAKE_STATIC_LIBRARY_SUFFIX}")
  set(lua_shared_library_path "${install_dir}/lib/${lua_shared_library}")

  cpkt_get_external_c_flags(lua_external_cflags)
  set(lua_my_cflags "${lua_external_cflags} -fPIC -DLUA_USE_POSIX -DLUA_USE_DLOPEN")
  set(lua_env_args "")
  cpkt_append_pinned_external_toolchain_env_args(lua_env_args)
  set(lua_shared_extra_link_flags "")
  if(CMAKE_SHARED_LINKER_FLAGS)
    separate_arguments(lua_shared_extra_link_flags NATIVE_COMMAND "${CMAKE_SHARED_LINKER_FLAGS}")
  endif()

  set(lua_base_objects
    "${source_dir}/src/lapi.o"
    "${source_dir}/src/lcode.o"
    "${source_dir}/src/lctype.o"
    "${source_dir}/src/ldebug.o"
    "${source_dir}/src/ldo.o"
    "${source_dir}/src/ldump.o"
    "${source_dir}/src/lfunc.o"
    "${source_dir}/src/lgc.o"
    "${source_dir}/src/llex.o"
    "${source_dir}/src/lmem.o"
    "${source_dir}/src/lobject.o"
    "${source_dir}/src/lopcodes.o"
    "${source_dir}/src/lparser.o"
    "${source_dir}/src/lstate.o"
    "${source_dir}/src/lstring.o"
    "${source_dir}/src/ltable.o"
    "${source_dir}/src/ltm.o"
    "${source_dir}/src/lundump.o"
    "${source_dir}/src/lvm.o"
    "${source_dir}/src/lzio.o"
    "${source_dir}/src/lauxlib.o"
    "${source_dir}/src/lbaselib.o"
    "${source_dir}/src/lcorolib.o"
    "${source_dir}/src/ldblib.o"
    "${source_dir}/src/liolib.o"
    "${source_dir}/src/lmathlib.o"
    "${source_dir}/src/loadlib.o"
    "${source_dir}/src/loslib.o"
    "${source_dir}/src/lstrlib.o"
    "${source_dir}/src/ltablib.o"
    "${source_dir}/src/lutf8lib.o"
    "${source_dir}/src/linit.o"
  )

  if(CPKT_BUILD_DEPENDENCIES)
    cpkt_cached_external_project_add(${project_name}
      URL "https://lua.org/ftp/lua-${CPKT_LUA_VERSION}.tar.gz"
      URL_HASH "SHA256=1c4b4068d67061f2a2231ad2b5422e77acea1487ea9890f6320af614f4373dce"
      DOWNLOAD_NAME "lua-${CPKT_LUA_VERSION}.tar.gz"
      PREFIX "${prefix_dir}"
      DOWNLOAD_DIR "${CPKT_DOWNLOAD_ROOT}"
      SOURCE_DIR "${source_dir}"
      STAMP_DIR "${stamp_dir}"
      TMP_DIR "${tmp_dir}"
      TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_TIMEOUT}
      INACTIVITY_TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_INACTIVITY_TIMEOUT}
      CONFIGURE_COMMAND ${CMAKE_COMMAND} -E true
      BUILD_COMMAND
        ${CMAKE_COMMAND} -E env ${lua_env_args} MAKEFLAGS= make -C "${source_dir}/src" clean
        COMMAND
          ${CMAKE_COMMAND} -E env ${lua_env_args} MAKEFLAGS= make -C "${source_dir}/src" a -j1
            CC=${CMAKE_C_COMPILER}
            AR=${CMAKE_AR}\ rcu
            RANLIB=${CMAKE_RANLIB}
            MYCFLAGS=${lua_my_cflags}
        COMMAND
          ${CMAKE_COMMAND} -E env ${lua_env_args}
          ${CMAKE_C_COMPILER}
          ${lua_shared_link_flags}
          ${lua_shared_extra_link_flags}
          -o "${source_dir}/src/${lua_shared_library}"
          ${lua_base_objects}
          ${lua_shared_libs}
      INSTALL_COMMAND
        ${CMAKE_COMMAND}
          -DCPKT_LUA_SOURCE_DIR=${source_dir}
          -DCPKT_LUA_INSTALL_DIR=${install_dir}
          -DCPKT_LUA_VERSION=${CPKT_LUA_VERSION}
          -DCPKT_LUA_SHARED_LIBRARY=${lua_shared_library}
          -DCPKT_LUA_SHARED_SONAME=${lua_shared_soname}
          -DCPKT_LUA_SHARED_LINK=${lua_shared_link}
          -P ${CMAKE_SOURCE_DIR}/cmake/install_lua.cmake
        COMMAND ${strip_install_command}
      BUILD_BYPRODUCTS
        "${lua_static_library}"
        "${lua_shared_library_path}"
      BUILD_IN_SOURCE 1
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
  endif()

  add_library(cpkt::lua_static STATIC IMPORTED GLOBAL)
  set_target_properties(cpkt::lua_static
    PROPERTIES
      IMPORTED_LOCATION "${lua_static_library}"
      INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
      INTERFACE_LINK_LIBRARIES "m;${CMAKE_DL_LIBS}"
  )

  add_library(cpkt::lua_shared SHARED IMPORTED GLOBAL)
  set_target_properties(cpkt::lua_shared
    PROPERTIES
      IMPORTED_LOCATION "${lua_shared_library_path}"
      INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
      INTERFACE_LINK_LIBRARIES "m;${CMAKE_DL_LIBS}"
  )

  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt::lua_static ${project_name})
    add_dependencies(cpkt::lua_shared ${project_name})
    cpkt_record_dependency_target(${project_name})
  else()
    cpkt_require_dependency_file("${lua_static_library}" "Lua static library")
    cpkt_require_dependency_file("${lua_shared_library_path}" "Lua shared library")
    cpkt_require_dependency_file("${install_dir}/include/lua.h" "Lua header")
    cpkt_require_dependency_file("${install_dir}/include/lauxlib.h" "Lua auxiliary header")
    cpkt_require_dependency_file("${install_dir}/include/lualib.h" "Lua standard library header")
  endif()

  set(CPKT_LUA_PREFIX "${install_dir}" PARENT_SCOPE)
endfunction()

function(cpkt_add_mqttc)
  set(project_name "cpkt_mqttc_project")
  set(prefix_dir "${CPKT_DEPENDENCY_BUILD_ROOT}/mqtt-c")
  set(source_dir "${prefix_dir}/src")
  set(build_dir "${prefix_dir}/build")
  set(install_dir "${CPKT_EXTERNAL_ROOT}/mqtt-c/install")
  set(stamp_dir "${prefix_dir}/stamp")
  set(tmp_dir "${prefix_dir}/tmp")
  cpkt_get_strip_dependency_install_command(strip_install_command "${install_dir}")
  file(MAKE_DIRECTORY "${install_dir}/include" "${install_dir}/lib" "${build_dir}")

  set(mqttc_static_library "${install_dir}/lib/libmqttc${CMAKE_STATIC_LIBRARY_SUFFIX}")
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(mqttc_shared_library_name "libmqttc.${CPKT_MQTTC_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(mqttc_shared_soname "libmqttc.1${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(mqttc_shared_link "libmqttc${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(mqttc_shared_library "${install_dir}/lib/${mqttc_shared_library_name}")
    set(mqttc_shared_link_flags
      -dynamiclib
      -Wl,-install_name,@rpath/${mqttc_shared_soname}
    )
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(mqttc_shared_library_name "libmqttc${CMAKE_SHARED_LIBRARY_SUFFIX}.${CPKT_MQTTC_VERSION}")
    set(mqttc_shared_soname "libmqttc${CMAKE_SHARED_LIBRARY_SUFFIX}.1")
    set(mqttc_shared_link "libmqttc${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(mqttc_shared_library "${install_dir}/lib/${mqttc_shared_library_name}")
    set(mqttc_shared_link_flags
      -shared
      -Wl,--enable-new-dtags
      -Wl,-rpath,\$ORIGIN
      -Wl,-soname,${mqttc_shared_soname}
    )
  else()
    set(mqttc_shared_library_name "libmqttc${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(mqttc_shared_soname "")
    set(mqttc_shared_link "")
    set(mqttc_shared_library "${install_dir}/lib/${mqttc_shared_library_name}")
    set(mqttc_shared_link_flags -shared)
  endif()

  cpkt_get_external_c_flags(mqttc_external_cflags)
  separate_arguments(mqttc_compile_flags NATIVE_COMMAND "${mqttc_external_cflags}")
  list(APPEND mqttc_compile_flags -fPIC -I "${source_dir}/include")
  set(mqttc_env_args "")
  cpkt_append_pinned_external_toolchain_env_args(mqttc_env_args)
  set(mqttc_shared_extra_link_flags "")
  if(CMAKE_SHARED_LINKER_FLAGS)
    separate_arguments(mqttc_shared_extra_link_flags NATIVE_COMMAND "${CMAKE_SHARED_LINKER_FLAGS}")
  endif()
  set(mqttc_link_libraries Threads::Threads)
  set(mqttc_link_flags "")
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND mqttc_link_flags -pthread)
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND mqttc_link_flags -pthread)
  endif()

  if(CPKT_BUILD_DEPENDENCIES)
    cpkt_cached_external_project_add(${project_name}
      URL "https://github.com/LiamBindle/MQTT-C/archive/${CPKT_MQTTC_COMMIT}.tar.gz"
      URL_HASH "SHA256=985898405912dbddf50d8b446226763696e6390fbd6f38b66cede6f38e703086"
      DOWNLOAD_NAME "mqtt-c-${CPKT_MQTTC_COMMIT}.tar.gz"
      PREFIX "${prefix_dir}"
      DOWNLOAD_DIR "${CPKT_DOWNLOAD_ROOT}"
      SOURCE_DIR "${source_dir}"
      BINARY_DIR "${build_dir}"
      STAMP_DIR "${stamp_dir}"
      TMP_DIR "${tmp_dir}"
      TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_TIMEOUT}
      INACTIVITY_TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_INACTIVITY_TIMEOUT}
      CONFIGURE_COMMAND ${CMAKE_COMMAND} -E make_directory "${build_dir}" "${install_dir}/include" "${install_dir}/lib"
      BUILD_COMMAND
        ${CMAKE_COMMAND} -E copy_directory "${source_dir}/include" "${install_dir}/include"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${build_dir}"
        COMMAND ${CMAKE_COMMAND} -E env ${mqttc_env_args}
          ${CMAKE_C_COMPILER} ${mqttc_compile_flags} -c "${source_dir}/src/mqtt.c" -o "${build_dir}/mqtt.c.o"
        COMMAND ${CMAKE_COMMAND} -E env ${mqttc_env_args}
          ${CMAKE_C_COMPILER} ${mqttc_compile_flags} -c "${source_dir}/src/mqtt_pal.c" -o "${build_dir}/mqtt_pal.c.o"
        COMMAND ${CMAKE_COMMAND} -E rm -f "${mqttc_static_library}"
        COMMAND ${CMAKE_AR} qc "${mqttc_static_library}" "${build_dir}/mqtt.c.o" "${build_dir}/mqtt_pal.c.o"
        COMMAND ${CMAKE_RANLIB} "${mqttc_static_library}"
        COMMAND ${CMAKE_COMMAND} -E rm -f "${mqttc_shared_library}"
        COMMAND ${CMAKE_COMMAND} -E env ${mqttc_env_args}
          ${CMAKE_C_COMPILER} ${mqttc_shared_link_flags} ${mqttc_shared_extra_link_flags} -o "${mqttc_shared_library}" "${build_dir}/mqtt.c.o" "${build_dir}/mqtt_pal.c.o" ${mqttc_link_flags}
      INSTALL_COMMAND
        ${CMAKE_COMMAND} -E true
        COMMAND ${CMAKE_COMMAND} -E rm -f "${install_dir}/lib/${mqttc_shared_soname}" "${install_dir}/lib/${mqttc_shared_link}"
        COMMAND ${CMAKE_COMMAND} -E create_symlink "${mqttc_shared_library_name}" "${install_dir}/lib/${mqttc_shared_soname}"
        COMMAND ${CMAKE_COMMAND} -E create_symlink "${mqttc_shared_soname}" "${install_dir}/lib/${mqttc_shared_link}"
        COMMAND ${strip_install_command}
      BUILD_BYPRODUCTS
        "${mqttc_static_library}"
        "${mqttc_shared_library}"
      BUILD_IN_SOURCE 0
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
  endif()

  add_library(cpkt::mqttc_static STATIC IMPORTED GLOBAL)
  set_target_properties(cpkt::mqttc_static
    PROPERTIES
      IMPORTED_LOCATION "${mqttc_static_library}"
      INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
      INTERFACE_LINK_LIBRARIES "${mqttc_link_libraries}"
  )

  add_library(cpkt::mqttc_shared SHARED IMPORTED GLOBAL)
  set_target_properties(cpkt::mqttc_shared
    PROPERTIES
      IMPORTED_LOCATION "${mqttc_shared_library}"
      INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
      INTERFACE_LINK_LIBRARIES "${mqttc_link_libraries}"
  )

  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt::mqttc_static ${project_name})
    add_dependencies(cpkt::mqttc_shared ${project_name})
    cpkt_record_dependency_target(${project_name})
  else()
    cpkt_require_dependency_file("${mqttc_static_library}" "MQTT-C static library")
    cpkt_require_dependency_file("${mqttc_shared_library}" "MQTT-C shared library")
    cpkt_require_dependency_file("${install_dir}/include/mqtt.h" "MQTT-C header")
    cpkt_require_dependency_file("${install_dir}/include/mqtt_pal.h" "MQTT-C PAL header")
  endif()

  set(CPKT_MQTTC_SOURCE_DIR "${source_dir}" PARENT_SCOPE)
  set(CPKT_MQTTC_PREFIX "${install_dir}" PARENT_SCOPE)
endfunction()

function(cpkt_add_miniaudio)
  set(project_name "cpkt_miniaudio_project")
  set(prefix_dir "${CPKT_DEPENDENCY_BUILD_ROOT}/miniaudio")
  set(source_dir "${prefix_dir}/src")
  set(build_dir "${prefix_dir}/build")
  set(install_dir "${CPKT_EXTERNAL_ROOT}/miniaudio/install")
  set(stamp_dir "${prefix_dir}/stamp")
  set(tmp_dir "${prefix_dir}/tmp")
  cpkt_get_strip_dependency_install_command(strip_install_command "${install_dir}")
  file(MAKE_DIRECTORY "${install_dir}/include/miniaudio" "${install_dir}/lib")

  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(miniaudio_shared_library "${install_dir}/lib/libminiaudio${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(miniaudio_shared_link_flags
      -dynamiclib
      -Wl,-install_name,@rpath/libminiaudio${CMAKE_SHARED_LIBRARY_SUFFIX}
    )
    if(CMAKE_LINKER)
      list(APPEND miniaudio_shared_link_flags "--ld-path=${CMAKE_LINKER}")
    endif()
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(miniaudio_shared_library "${install_dir}/lib/libminiaudio${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(miniaudio_shared_link_flags
      -shared
      -Wl,--enable-new-dtags
      -Wl,-rpath,\$ORIGIN
      -Wl,-soname,libminiaudio${CMAKE_SHARED_LIBRARY_SUFFIX}
    )
  else()
    set(miniaudio_shared_library "${install_dir}/lib/libminiaudio${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(miniaudio_shared_link_flags -shared)
  endif()
  set(miniaudio_static_library "${install_dir}/lib/libminiaudio${CMAKE_STATIC_LIBRARY_SUFFIX}")

  cpkt_get_external_c_flags(miniaudio_external_cflags)
  separate_arguments(miniaudio_compile_flags NATIVE_COMMAND "${miniaudio_external_cflags}")
  list(APPEND miniaudio_compile_flags
    -fPIC
    -DMA_NO_RESOURCE_MANAGER
    -DMA_NO_NODE_GRAPH
    -DMA_NO_ENGINE
    -DMA_NO_GENERATION
  )
  set(miniaudio_object "${build_dir}/miniaudio.c.o")
  set(miniaudio_link_libraries -lm -pthread)
  if(CMAKE_DL_LIBS)
    list(APPEND miniaudio_link_libraries "-l${CMAKE_DL_LIBS}")
  endif()
  set(miniaudio_env_args "")
  cpkt_append_pinned_external_toolchain_env_args(miniaudio_env_args)

  if(CPKT_BUILD_DEPENDENCIES)
    cpkt_cached_external_project_add(${project_name}
      URL "https://github.com/mackron/miniaudio/archive/refs/tags/${CPKT_MINIAUDIO_VERSION}.tar.gz"
      URL_HASH "SHA256=b900edcffe979816e2560a0580b9b1216d674b4f17fbadeca8f777a7f8ab0274"
      DOWNLOAD_NAME "miniaudio-${CPKT_MINIAUDIO_VERSION}.tar.gz"
      PREFIX "${prefix_dir}"
      DOWNLOAD_DIR "${CPKT_DOWNLOAD_ROOT}"
      SOURCE_DIR "${source_dir}"
      BINARY_DIR "${build_dir}"
      STAMP_DIR "${stamp_dir}"
      TMP_DIR "${tmp_dir}"
      TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_TIMEOUT}
      INACTIVITY_TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_INACTIVITY_TIMEOUT}
      CONFIGURE_COMMAND
        ${CMAKE_COMMAND} -E make_directory
          "${build_dir}"
          "${install_dir}/include/miniaudio"
          "${install_dir}/lib"
      BUILD_COMMAND
        ${CMAKE_COMMAND} -E env ${miniaudio_env_args}
          ${CMAKE_C_COMPILER}
          ${miniaudio_compile_flags}
          -c "${source_dir}/miniaudio.c"
          -o "${miniaudio_object}"
        COMMAND ${CMAKE_COMMAND} -E rm -f "${miniaudio_static_library}"
        COMMAND ${CMAKE_AR} qc "${miniaudio_static_library}" "${miniaudio_object}"
        COMMAND ${CMAKE_RANLIB} "${miniaudio_static_library}"
        COMMAND ${CMAKE_COMMAND} -E rm -f "${miniaudio_shared_library}"
        COMMAND ${CMAKE_COMMAND} -E env ${miniaudio_env_args}
          ${CMAKE_C_COMPILER}
          ${miniaudio_shared_link_flags}
          -o "${miniaudio_shared_library}"
          "${miniaudio_object}"
          ${miniaudio_link_libraries}
      INSTALL_COMMAND
        ${CMAKE_COMMAND} -E copy_if_different
          "${source_dir}/miniaudio.h"
          "${install_dir}/include/miniaudio/miniaudio.h"
        COMMAND ${strip_install_command}
      BUILD_BYPRODUCTS
        "${miniaudio_static_library}"
        "${miniaudio_shared_library}"
      BUILD_IN_SOURCE 0
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
  endif()

  add_library(cpkt::miniaudio_static STATIC IMPORTED GLOBAL)
  set_target_properties(cpkt::miniaudio_static
    PROPERTIES
      IMPORTED_LOCATION "${miniaudio_static_library}"
      INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include/miniaudio"
      INTERFACE_LINK_LIBRARIES "m;Threads::Threads"
  )

  add_library(cpkt::miniaudio_shared SHARED IMPORTED GLOBAL)
  set_target_properties(cpkt::miniaudio_shared
    PROPERTIES
      IMPORTED_LOCATION "${miniaudio_shared_library}"
      INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include/miniaudio"
      INTERFACE_LINK_LIBRARIES "m;Threads::Threads"
  )

  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt::miniaudio_static ${project_name})
    add_dependencies(cpkt::miniaudio_shared ${project_name})
    cpkt_record_dependency_target(${project_name})
  else()
    cpkt_require_dependency_file("${miniaudio_static_library}" "miniaudio static library")
    cpkt_require_dependency_file("${miniaudio_shared_library}" "miniaudio shared library")
    cpkt_require_dependency_file("${install_dir}/include/miniaudio/miniaudio.h" "miniaudio header")
  endif()
endfunction()

function(cpkt_add_whisper)
  if(NOT CPKT_SUS_CPU_ONLY)
    message(FATAL_ERROR "cpkt_sus currently supports only CPU-only whisper.cpp dependency builds")
  endif()

  set(project_name_shared "cpkt_whisper_shared_project")
  set(project_name_static "cpkt_whisper_static_project")
  set(prefix_dir "${CPKT_DEPENDENCY_BUILD_ROOT}/whisper")
  set(source_dir "${prefix_dir}/src")
  set(shared_build_dir "${prefix_dir}/build-shared")
  set(static_build_dir "${prefix_dir}/build-static")
  set(install_dir "${CPKT_EXTERNAL_ROOT}/whisper/install")
  set(stamp_dir "${prefix_dir}/stamp")
  set(tmp_dir "${prefix_dir}/tmp")
  cpkt_append_common_external_cxx_cmake_args(common_cmake_args)
  cpkt_get_external_cmake_step_commands(cmake_build_command cmake_install_command)
  cpkt_get_strip_dependency_install_command(strip_install_command "${install_dir}")
  file(MAKE_DIRECTORY "${install_dir}/include" "${install_dir}/lib")

  set(whisper_static_library "${install_dir}/lib/libwhisper${CMAKE_STATIC_LIBRARY_SUFFIX}")
  set(whisper_shared_library "${install_dir}/lib/libwhisper${CMAKE_SHARED_LIBRARY_SUFFIX}")
  set(ggml_static_libraries
    "${install_dir}/lib/libggml${CMAKE_STATIC_LIBRARY_SUFFIX}"
    "${install_dir}/lib/libggml-base${CMAKE_STATIC_LIBRARY_SUFFIX}"
    "${install_dir}/lib/libggml-cpu${CMAKE_STATIC_LIBRARY_SUFFIX}")
  set(ggml_shared_libraries
    "${install_dir}/lib/libggml${CMAKE_SHARED_LIBRARY_SUFFIX}"
    "${install_dir}/lib/libggml-base${CMAKE_SHARED_LIBRARY_SUFFIX}"
    "${install_dir}/lib/libggml-cpu${CMAKE_SHARED_LIBRARY_SUFFIX}")
  set(whisper_install_rpath "")
  set(whisper_shared_linker_flags "")
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(whisper_install_rpath "$ORIGIN")
    if(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
      set(whisper_shared_linker_flags "-static-libstdc++ -static-libgcc")
    endif()
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(whisper_install_rpath "@loader_path")
  endif()

  set(whisper_common_cmake_args
    -DCMAKE_INSTALL_PREFIX=${install_dir}
    -DCMAKE_INSTALL_LIBDIR=lib
    -DCMAKE_BUILD_TYPE=${CPKT_DEPENDENCY_BUILD_TYPE}
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DCMAKE_INSTALL_RPATH=${whisper_install_rpath}
    -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=OFF
    -DCMAKE_BUILD_RPATH=
    -DCMAKE_SKIP_INSTALL_RPATH=OFF
    -DCMAKE_SHARED_LINKER_FLAGS=${whisper_shared_linker_flags}
    -DCMAKE_MODULE_LINKER_FLAGS=${whisper_shared_linker_flags}
    -DWHISPER_BUILD_TESTS=OFF
    -DWHISPER_BUILD_EXAMPLES=OFF
    -DWHISPER_BUILD_SERVER=OFF
    -DWHISPER_CURL=OFF
    -DWHISPER_SDL2=OFF
    -DWHISPER_COREML=OFF
    -DWHISPER_COREML_ALLOW_FALLBACK=OFF
    -DWHISPER_OPENVINO=OFF
    -DWHISPER_ALL_WARNINGS=OFF
    -DWHISPER_ALL_WARNINGS_3RD_PARTY=OFF
    -DWHISPER_FATAL_WARNINGS=OFF
    -DGGML_NATIVE=OFF
    -DGGML_OPENMP=OFF
    -DGGML_METAL=OFF
    -DGGML_BLAS=OFF
    -DGGML_ACCELERATE=OFF
    -DGGML_CUDA=OFF
    -DGGML_HIP=OFF
    -DGGML_VULKAN=OFF
    -DGGML_OPENCL=OFF
    -DGGML_SYCL=OFF
    -DGGML_RPC=OFF
    -DGGML_BACKEND_DL=OFF
    -DGGML_CPU_ALL_VARIANTS=OFF
    -DGGML_CCACHE=OFF
    ${common_cmake_args}
  )

  if(CPKT_BUILD_DEPENDENCIES)
    cpkt_cached_external_project_add(${project_name_shared}
      URL "https://github.com/ggml-org/whisper.cpp/archive/refs/tags/${CPKT_WHISPER_VERSION}.tar.gz"
      URL_HASH "SHA256=57e280cee375ab02425b806ad5146b99f6eb9357e3c2b31357c8a6af2e2e44ae"
      DOWNLOAD_NAME "whisper.cpp-${CPKT_WHISPER_VERSION}.tar.gz"
      PREFIX "${prefix_dir}"
      DOWNLOAD_DIR "${CPKT_DOWNLOAD_ROOT}"
      SOURCE_DIR "${source_dir}"
      BINARY_DIR "${shared_build_dir}"
      STAMP_DIR "${stamp_dir}/shared"
      TMP_DIR "${tmp_dir}"
      TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_TIMEOUT}
      INACTIVITY_TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_INACTIVITY_TIMEOUT}
      PATCH_COMMAND
        ${CMAKE_COMMAND}
          -DWHISPER_SOURCE_DIR=<SOURCE_DIR>
          -P ${CMAKE_SOURCE_DIR}/cmake/patch_whisper_buildinfo.cmake
      CMAKE_ARGS
        -DBUILD_SHARED_LIBS=ON
        ${whisper_common_cmake_args}
      BUILD_COMMAND ${cmake_build_command}
      INSTALL_COMMAND ${cmake_install_command}
      BUILD_BYPRODUCTS
        "${whisper_shared_library}"
        ${ggml_shared_libraries}
      BUILD_IN_SOURCE 0
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    cpkt_cached_external_project_add(${project_name_static}
      URL "https://github.com/ggml-org/whisper.cpp/archive/refs/tags/${CPKT_WHISPER_VERSION}.tar.gz"
      URL_HASH "SHA256=57e280cee375ab02425b806ad5146b99f6eb9357e3c2b31357c8a6af2e2e44ae"
      DOWNLOAD_NAME "whisper.cpp-${CPKT_WHISPER_VERSION}.tar.gz"
      PREFIX "${prefix_dir}"
      DOWNLOAD_DIR "${CPKT_DOWNLOAD_ROOT}"
      SOURCE_DIR "${source_dir}"
      BINARY_DIR "${static_build_dir}"
      STAMP_DIR "${stamp_dir}/static"
      TMP_DIR "${tmp_dir}"
      TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_TIMEOUT}
      INACTIVITY_TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_INACTIVITY_TIMEOUT}
      DEPENDS ${project_name_shared}
      PATCH_COMMAND
        ${CMAKE_COMMAND}
          -DWHISPER_SOURCE_DIR=<SOURCE_DIR>
          -P ${CMAKE_SOURCE_DIR}/cmake/patch_whisper_buildinfo.cmake
      CMAKE_ARGS
        -DBUILD_SHARED_LIBS=OFF
        ${whisper_common_cmake_args}
      BUILD_COMMAND ${cmake_build_command}
      INSTALL_COMMAND ${cmake_install_command}
        COMMAND ${strip_install_command}
      BUILD_BYPRODUCTS
        "${whisper_static_library}"
        ${ggml_static_libraries}
      BUILD_IN_SOURCE 0
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
  endif()

  add_library(cpkt::whisper_static STATIC IMPORTED GLOBAL)
  set_target_properties(cpkt::whisper_static
    PROPERTIES
      IMPORTED_LOCATION "${whisper_static_library}"
      INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
      INTERFACE_LINK_LIBRARIES "${ggml_static_libraries};Threads::Threads;m"
  )

  add_library(cpkt::whisper_shared SHARED IMPORTED GLOBAL)
  set_target_properties(cpkt::whisper_shared
    PROPERTIES
      IMPORTED_LOCATION "${whisper_shared_library}"
      INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
      INTERFACE_LINK_LIBRARIES "${ggml_shared_libraries};Threads::Threads"
  )

  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt::whisper_static ${project_name_static})
    add_dependencies(cpkt::whisper_shared ${project_name_shared})
    cpkt_record_dependency_target(${project_name_static})
  else()
    cpkt_require_dependency_file("${whisper_static_library}" "whisper.cpp static library")
    cpkt_require_dependency_file("${whisper_shared_library}" "whisper.cpp shared library")
    cpkt_require_dependency_file("${install_dir}/include/whisper.h" "whisper.cpp header")
  endif()
endfunction()

function(cpkt_add_open62541)
  set(project_name_shared "cpkt_open62541_shared_project")
  set(project_name_static "cpkt_open62541_static_project")
  set(prefix_dir "${CPKT_DEPENDENCY_BUILD_ROOT}/open62541")
  set(source_dir "${prefix_dir}/src")
  set(shared_build_dir "${prefix_dir}/build-shared")
  set(static_build_dir "${prefix_dir}/build-static")
  set(install_dir "${CPKT_EXTERNAL_ROOT}/open62541/install")
  set(stamp_dir "${prefix_dir}/stamp")
  set(tmp_dir "${prefix_dir}/tmp")
  cpkt_append_common_external_cmake_args(common_cmake_args)
  cpkt_get_external_cmake_step_commands(cmake_build_command cmake_install_command)
  cpkt_get_strip_dependency_install_command(strip_install_command "${install_dir}")
  file(MAKE_DIRECTORY "${install_dir}/include" "${install_dir}/lib")

  set(open62541_static_library "${install_dir}/lib/libopen62541${CMAKE_STATIC_LIBRARY_SUFFIX}")
  set(open62541_static_system_libs "m")
  set(open62541_static_pc_private_libs "-lm")
  set(open62541_interface_compile_definitions "_GNU_SOURCE")
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(open62541_shared_library "${install_dir}/lib/libopen62541.${CPKT_OPEN62541_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(open62541_install_rpath "@loader_path")
    set(open62541_platform_cmake_args "")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(open62541_shared_library "${install_dir}/lib/libopen62541${CMAKE_SHARED_LIBRARY_SUFFIX}.${CPKT_OPEN62541_VERSION}")
    set(open62541_install_rpath "$ORIGIN")
    set(open62541_platform_cmake_args
      -DCMAKE_SHARED_LINKER_FLAGS=-Wl,--enable-new-dtags)
    list(APPEND open62541_static_system_libs rt)
    string(APPEND open62541_static_pc_private_libs " -lrt")
  else()
    set(open62541_shared_library "${install_dir}/lib/libopen62541${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(open62541_install_rpath "")
    set(open62541_platform_cmake_args "")
  endif()

  set(open62541_common_cmake_args
    -DCMAKE_INSTALL_PREFIX=${install_dir}
    -DCMAKE_INSTALL_LIBDIR=lib
    -DCMAKE_BUILD_TYPE=${CPKT_DEPENDENCY_BUILD_TYPE}
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DCMAKE_FIND_PACKAGE_PREFER_CONFIG=ON
    -DCMAKE_INSTALL_RPATH=${open62541_install_rpath}
    -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=OFF
    -DCMAKE_BUILD_RPATH=
    -DCMAKE_SKIP_INSTALL_RPATH=OFF
    ${open62541_platform_cmake_args}
    -DOPEN62541_VERSION=v${CPKT_OPEN62541_VERSION}
    -DGIT_EXECUTABLE=GIT_EXECUTABLE-NOTFOUND
    -DUA_ARCHITECTURE=posix
    -DUA_NAMESPACE_ZERO=REDUCED
    -DUA_ENABLE_AMALGAMATION=OFF
    -DUA_ENABLE_ENCRYPTION=OPENSSL
    -DUA_ENABLE_MQTT=ON
    -DUA_FILE_MQTT=${source_dir}/deps/mqtt-c/src/mqtt.c
    -DUA_ENABLE_JSON_ENCODING=ON
    -DUA_ENABLE_XML_ENCODING=ON
    -DUA_ENABLE_DIAGNOSTICS=ON
    -DUA_ENABLE_METHODCALLS=ON
    -DUA_ENABLE_SUBSCRIPTIONS=ON
    -DUA_ENABLE_SUBSCRIPTIONS_EVENTS=ON
    -DUA_ENABLE_HISTORIZING=ON
    -DUA_ENABLE_DISCOVERY=ON
    -DUA_ENABLE_DISCOVERY_MULTICAST=OFF
    -DUA_ENABLE_NODEMANAGEMENT=ON
    -DUA_ENABLE_PUBSUB=ON
    -DUA_ENABLE_PUBSUB_INFORMATIONMODEL=ON
    -DUA_ENABLE_TYPEDESCRIPTION=ON
    -DUA_ENABLE_STATUSCODE_DESCRIPTIONS=ON
    -DUA_BUILD_EXAMPLES=OFF
    -DUA_BUILD_TOOLS=OFF
    -DUA_BUILD_UNIT_TESTS=OFF
    -DOPENSSL_ROOT_DIR=${CPKT_OPENSSL_static_PREFIX}
    -DOPENSSL_INCLUDE_DIR=${CPKT_OPENSSL_static_PREFIX}/include
    ${common_cmake_args}
  )

  if(CPKT_BUILD_DEPENDENCIES)
    cpkt_cached_external_project_add(${project_name_shared}
      URL "https://github.com/open62541/open62541/archive/refs/tags/v${CPKT_OPEN62541_VERSION}.tar.gz"
      URL_HASH "SHA256=cf7951baf253c0537b3397e4ce3ff13930542abcb6ffc3b9cb082af88f95c300"
      DOWNLOAD_NAME "open62541-${CPKT_OPEN62541_VERSION}.tar.gz"
      PREFIX "${prefix_dir}"
      DOWNLOAD_DIR "${CPKT_DOWNLOAD_ROOT}"
      SOURCE_DIR "${source_dir}"
      BINARY_DIR "${shared_build_dir}"
      STAMP_DIR "${stamp_dir}/shared"
      TMP_DIR "${tmp_dir}"
      TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_TIMEOUT}
      INACTIVITY_TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_INACTIVITY_TIMEOUT}
      DEPENDS cpkt_openssl_project cpkt_mqttc_project
      PATCH_COMMAND
        ${CMAKE_COMMAND} -E copy_directory
          "${CPKT_MQTTC_SOURCE_DIR}"
          "${source_dir}/deps/mqtt-c"
        COMMAND ${CMAKE_COMMAND}
          -DCPKT_PATCH_WORKING_DIRECTORY=${source_dir}
          -DCPKT_PATCH_SERIES=${CMAKE_SOURCE_DIR}/vendor/open62541/patches/series
          -P ${CMAKE_SOURCE_DIR}/cmake/apply_patch_series.cmake
      CMAKE_ARGS
        -DBUILD_SHARED_LIBS=ON
        -DOPENSSL_SSL_LIBRARY=${CPKT_OPENSSL_shared_PREFIX}/lib/libssl${CMAKE_SHARED_LIBRARY_SUFFIX}
        -DOPENSSL_CRYPTO_LIBRARY=${CPKT_OPENSSL_shared_PREFIX}/lib/libcrypto${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${open62541_common_cmake_args}
      BUILD_COMMAND ${cmake_build_command}
      INSTALL_COMMAND ${cmake_install_command}
      BUILD_BYPRODUCTS "${open62541_shared_library}"
      BUILD_IN_SOURCE 0
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    cpkt_cached_external_project_add(${project_name_static}
      URL "https://github.com/open62541/open62541/archive/refs/tags/v${CPKT_OPEN62541_VERSION}.tar.gz"
      URL_HASH "SHA256=cf7951baf253c0537b3397e4ce3ff13930542abcb6ffc3b9cb082af88f95c300"
      DOWNLOAD_NAME "open62541-${CPKT_OPEN62541_VERSION}.tar.gz"
      PREFIX "${prefix_dir}"
      DOWNLOAD_DIR "${CPKT_DOWNLOAD_ROOT}"
      SOURCE_DIR "${source_dir}"
      BINARY_DIR "${static_build_dir}"
      STAMP_DIR "${stamp_dir}/static"
      TMP_DIR "${tmp_dir}"
      TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_TIMEOUT}
      INACTIVITY_TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_INACTIVITY_TIMEOUT}
      DEPENDS ${project_name_shared}
      PATCH_COMMAND
        ${CMAKE_COMMAND} -E copy_directory
          "${CPKT_MQTTC_SOURCE_DIR}"
          "${source_dir}/deps/mqtt-c"
        COMMAND ${CMAKE_COMMAND}
          -DCPKT_PATCH_WORKING_DIRECTORY=${source_dir}
          -DCPKT_PATCH_SERIES=${CMAKE_SOURCE_DIR}/vendor/open62541/patches/series
          -P ${CMAKE_SOURCE_DIR}/cmake/apply_patch_series.cmake
      CMAKE_ARGS
        -DBUILD_SHARED_LIBS=OFF
        -DCMAKE_INTERPROCEDURAL_OPTIMIZATION:BOOL=OFF
        -DOPENSSL_SSL_LIBRARY=${CPKT_OPENSSL_static_PREFIX}/lib/libssl${CMAKE_STATIC_LIBRARY_SUFFIX}
        -DOPENSSL_CRYPTO_LIBRARY=${CPKT_OPENSSL_static_PREFIX}/lib/libcrypto${CMAKE_STATIC_LIBRARY_SUFFIX}
        ${open62541_common_cmake_args}
      BUILD_COMMAND ${cmake_build_command}
      INSTALL_COMMAND ${cmake_install_command}
        COMMAND ${strip_install_command}
      BUILD_BYPRODUCTS "${open62541_static_library}"
      BUILD_IN_SOURCE 0
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
  endif()

  add_library(cpkt::open62541_static STATIC IMPORTED GLOBAL)
  set_target_properties(cpkt::open62541_static
    PROPERTIES
      IMPORTED_LOCATION "${open62541_static_library}"
      INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
      INTERFACE_LINK_LIBRARIES "cpkt::openssl_ssl_static;cpkt::openssl_crypto_static;${open62541_static_system_libs}"
      INTERFACE_COMPILE_DEFINITIONS "${open62541_interface_compile_definitions}"
  )

  add_library(cpkt::open62541_shared SHARED IMPORTED GLOBAL)
  set_target_properties(cpkt::open62541_shared
    PROPERTIES
      IMPORTED_LOCATION "${open62541_shared_library}"
      INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
      INTERFACE_LINK_LIBRARIES "cpkt::openssl_ssl_shared;cpkt::openssl_crypto_shared"
      INTERFACE_COMPILE_DEFINITIONS "${open62541_interface_compile_definitions}"
  )

  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt::open62541_static ${project_name_static})
    add_dependencies(cpkt::open62541_shared ${project_name_shared})
    cpkt_record_dependency_target(${project_name_static})
  else()
    cpkt_require_dependency_file("${open62541_static_library}" "open62541 static library")
    cpkt_require_dependency_file("${open62541_shared_library}" "open62541 shared library")
    cpkt_require_dependency_file("${install_dir}/include/open62541/server.h" "open62541 server header")
    cpkt_require_dependency_file("${install_dir}/include/open62541/client.h" "open62541 client header")
  endif()
endfunction()

function(cpkt_add_krb5)
  set(project_name_shared "cpkt_krb5_shared_project")
  set(project_name_static "cpkt_krb5_static_project")
  set(prefix_dir "${CPKT_DEPENDENCY_BUILD_ROOT}/krb5")
  set(source_dir "${prefix_dir}/src")
  set(shared_build_dir "${prefix_dir}/build-shared")
  set(static_build_dir "${prefix_dir}/build-static")
  set(install_dir "${CPKT_EXTERNAL_ROOT}/krb5/install")
  set(stage_dir "${install_dir}/stage")
  set(stamp_dir "${prefix_dir}/stamp")
  set(tmp_dir "${prefix_dir}/tmp")
  set(gssapi_static_library "${install_dir}/lib/libgssapi_krb5${CMAKE_STATIC_LIBRARY_SUFFIX}")
  set(krb5_static_library "${install_dir}/lib/libkrb5${CMAKE_STATIC_LIBRARY_SUFFIX}")
  set(k5crypto_static_library "${install_dir}/lib/libk5crypto${CMAKE_STATIC_LIBRARY_SUFFIX}")
  set(com_err_static_library "${install_dir}/lib/libcom_err${CMAKE_STATIC_LIBRARY_SUFFIX}")
  set(krb5support_static_library "${install_dir}/lib/libkrb5support${CMAKE_STATIC_LIBRARY_SUFFIX}")
  set(profile_static_library "${install_dir}/lib/libprofile${CMAKE_STATIC_LIBRARY_SUFFIX}")
  set(verto_static_library "${install_dir}/lib/libverto${CMAKE_STATIC_LIBRARY_SUFFIX}")
  set(gssapi_shared_library "${install_dir}/lib/libgssapi_krb5${CMAKE_SHARED_LIBRARY_SUFFIX}")
  set(krb5_static_platform_libraries "")
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND krb5_static_platform_libraries resolv)
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND krb5_static_platform_libraries resolv "-Wl,-framework,Kerberos")
  endif()
  cpkt_get_target_triple(target_triple)
  cpkt_get_external_c_flags(external_cflags)
  cpkt_get_autotools_link_flags(external_ldflags)
  set(env_args "")
  cpkt_append_pinned_external_toolchain_env_args(env_args)
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if(NOT EXISTS "${CPKT_DARWIN_HOST_MIG}" OR NOT EXISTS "${CPKT_DARWIN_HOST_MIGCOM}")
      message(FATAL_ERROR "Darwin Kerberos requires the ready pinned host MIG toolchain")
    endif()
    get_filename_component(darwin_host_mig_bin_dir "${CPKT_DARWIN_HOST_MIG}" DIRECTORY)
    list(APPEND env_args
      "PATH=${darwin_host_mig_bin_dir}:${CPKT_OSXCROSS_BIN_DIR}:$ENV{PATH}"
      "MIGCC=${CMAKE_C_COMPILER}"
      "MIGCOM=${CPKT_DARWIN_HOST_MIGCOM}"
      "SDKROOT=${CMAKE_OSX_SYSROOT}")
  endif()
  list(APPEND env_args
    # Kerberos static archives are part of the public GSSAPI closure.
    "CFLAGS=${external_cflags} -fPIC"
    "LDFLAGS=${external_ldflags}"
    # Keep Kerberos defaults independent of the disposable build/install root.
    # Applications may override all three with the standard environment knobs.
    "DEFCCNAME=FILE:/tmp/krb5cc_%{uid}"
    "DEFKTNAME=FILE:/etc/krb5.keytab"
    "DEFCKTNAME=FILE:/var/lib/krb5/user/%{euid}/client.keytab")
  if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    # MIT Kerberos otherwise enables a GCC-version-specific warning-as-error
    # profile which is not valid with the pinned GCC 15 toolchain.
    list(APPEND env_args
      "WARN_CFLAGS=-Wno-error=discarded-qualifiers"
      "WARN_CXXFLAGS=-Wno-error=discarded-qualifiers")
  endif()
  if(CMAKE_CROSSCOMPILING)
    # The pinned GCC targets implement both attributes.  MIT Kerberos cannot
    # execute its otherwise straightforward probe while cross compiling.
    list(APPEND env_args
      "krb5_cv_attr_constructor_destructor=yes,yes"
      "ac_cv_printf_positional=yes")
  endif()
  # Static GSSAPI consumers are permitted to link into shared libraries.
  set(static_env_args ${env_args})
  cpkt_get_strip_dependency_install_command(strip_install_command "${install_dir}")
  set(krb5_darwin_install_name_normalize_command ${CMAKE_COMMAND} -E true)
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(krb5_darwin_install_name_normalize_command
      ${CMAKE_COMMAND}
        -DCPKT_DARWIN_LIBRARY_DIR=${install_dir}/lib
        -DCPKT_DARWIN_INSTALL_NAME_TOOL=${CMAKE_INSTALL_NAME_TOOL}
        -DCPKT_DARWIN_OTOOL=${CPKT_OTOOL}
        -P ${CMAKE_SOURCE_DIR}/cmake/normalize_darwin_dylib_install_names.cmake)
  endif()
  file(MAKE_DIRECTORY
    "${install_dir}/include"
    "${install_dir}/include/gssapi"
    "${install_dir}/include/krb5"
    "${install_dir}/lib")

  if(CPKT_BUILD_DEPENDENCIES)
    cpkt_cached_external_project_add(${project_name_static}
      URL "https://web.mit.edu/kerberos/dist/krb5/1.22/krb5-${CPKT_KRB5_VERSION}.tar.gz"
      URL_HASH "SHA256=3243ffbc8ea4d4ac22ddc7dd2a1dc54c57874c40648b60ff97009763554eaf13"
      DOWNLOAD_NAME "krb5-${CPKT_KRB5_VERSION}.tar.gz"
      PREFIX "${prefix_dir}"
      DOWNLOAD_DIR "${CPKT_DOWNLOAD_ROOT}"
      SOURCE_DIR "${source_dir}"
      BINARY_DIR "${static_build_dir}"
      STAMP_DIR "${stamp_dir}/static"
      TMP_DIR "${tmp_dir}"
      TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_TIMEOUT}
      INACTIVITY_TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_INACTIVITY_TIMEOUT}
      CONFIGURE_COMMAND ${CMAKE_COMMAND} -E chdir "${static_build_dir}"
        ${CMAKE_COMMAND} -E env ${static_env_args}
        "${source_dir}/src/configure"
        --host=${target_triple}
        --prefix=/usr
        --libdir=/usr/lib
        --includedir=/usr/include
        --localstatedir=/var
        --disable-shared
        --enable-static
        --disable-rpath
        --disable-nls
        --disable-pkinit
        --without-ldap
        --without-readline
      BUILD_COMMAND ${CMAKE_COMMAND} -E chdir "${static_build_dir}"
        ${CMAKE_COMMAND} -E env ${static_env_args} make -C util/support -j${CPKT_DEPENDENCY_BUILD_JOBS}
        COMMAND ${CMAKE_COMMAND} -E chdir "${static_build_dir}"
        ${CMAKE_COMMAND} -E env ${static_env_args} make -C util/et -j${CPKT_DEPENDENCY_BUILD_JOBS}
        COMMAND ${CMAKE_COMMAND} -E chdir "${static_build_dir}"
        ${CMAKE_COMMAND} -E env ${static_env_args} make -C include -j${CPKT_DEPENDENCY_BUILD_JOBS}
        COMMAND ${CMAKE_COMMAND} -E chdir "${static_build_dir}"
        ${CMAKE_COMMAND} -E env ${static_env_args} make -C util -j${CPKT_DEPENDENCY_BUILD_JOBS}
        COMMAND ${CMAKE_COMMAND} -E chdir "${static_build_dir}"
        ${CMAKE_COMMAND} -E env ${static_env_args} make -C lib/crypto -j${CPKT_DEPENDENCY_BUILD_JOBS}
        COMMAND ${CMAKE_COMMAND} -E chdir "${static_build_dir}"
        ${CMAKE_COMMAND} -E env ${static_env_args} make -C lib/krb5 -j${CPKT_DEPENDENCY_BUILD_JOBS}
        COMMAND ${CMAKE_COMMAND} -E chdir "${static_build_dir}"
        ${CMAKE_COMMAND} -E env ${static_env_args} make -C lib/gssapi -j${CPKT_DEPENDENCY_BUILD_JOBS}
      INSTALL_COMMAND ${CMAKE_COMMAND} -E remove_directory "${install_dir}"
        COMMAND ${CMAKE_COMMAND} -E make_directory
          "${stage_dir}/usr/include"
          "${stage_dir}/usr/include/kadm5"
          "${stage_dir}/usr/include/krb5"
          "${stage_dir}/usr/include/gssapi"
          "${stage_dir}/usr/include/gssrpc"
          "${stage_dir}/usr/lib"
        COMMAND ${CMAKE_COMMAND} -E chdir "${static_build_dir}"
        ${CMAKE_COMMAND} -E env ${static_env_args} make -C include install DESTDIR=${stage_dir}
        COMMAND ${CMAKE_COMMAND} -E chdir "${static_build_dir}"
        ${CMAKE_COMMAND} -E env ${static_env_args} make -C util/support install-libs DESTDIR=${stage_dir}
        COMMAND ${CMAKE_COMMAND} -E chdir "${static_build_dir}"
        ${CMAKE_COMMAND} -E env ${static_env_args} make -C util/et install-libs DESTDIR=${stage_dir}
        COMMAND ${CMAKE_COMMAND} -E chdir "${static_build_dir}"
        ${CMAKE_COMMAND} -E env ${static_env_args} make -C util/profile install-libs DESTDIR=${stage_dir}
        COMMAND ${CMAKE_COMMAND} -E chdir "${static_build_dir}"
        ${CMAKE_COMMAND} -E env ${static_env_args} make -C util/verto install-libs DESTDIR=${stage_dir}
        COMMAND ${CMAKE_COMMAND} -E chdir "${static_build_dir}"
        ${CMAKE_COMMAND} -E env ${static_env_args} make -C lib/crypto install DESTDIR=${stage_dir}
        COMMAND ${CMAKE_COMMAND} -E chdir "${static_build_dir}"
        ${CMAKE_COMMAND} -E env ${static_env_args} make -C lib/krb5 install DESTDIR=${stage_dir}
        COMMAND ${CMAKE_COMMAND} -E chdir "${static_build_dir}"
        ${CMAKE_COMMAND} -E env ${static_env_args} make -C lib/gssapi install DESTDIR=${stage_dir}
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${stage_dir}/usr/include" "${install_dir}/include"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${stage_dir}/usr/lib" "${install_dir}/lib"
        COMMAND ${strip_install_command}
      BUILD_BYPRODUCTS
        "${gssapi_static_library}"
        "${krb5_static_library}"
        "${k5crypto_static_library}"
        "${com_err_static_library}"
        "${krb5support_static_library}"
        "${profile_static_library}"
        "${verto_static_library}"
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

    cpkt_cached_external_project_add(${project_name_shared}
      URL "https://web.mit.edu/kerberos/dist/krb5/1.22/krb5-${CPKT_KRB5_VERSION}.tar.gz"
      URL_HASH "SHA256=3243ffbc8ea4d4ac22ddc7dd2a1dc54c57874c40648b60ff97009763554eaf13"
      DOWNLOAD_NAME "krb5-${CPKT_KRB5_VERSION}.tar.gz"
      PREFIX "${prefix_dir}"
      DOWNLOAD_DIR "${CPKT_DOWNLOAD_ROOT}"
      SOURCE_DIR "${source_dir}"
      BINARY_DIR "${shared_build_dir}"
      STAMP_DIR "${stamp_dir}/shared"
      TMP_DIR "${tmp_dir}"
      TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_TIMEOUT}
      INACTIVITY_TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_INACTIVITY_TIMEOUT}
      DEPENDS ${project_name_static}
      CONFIGURE_COMMAND ${CMAKE_COMMAND} -E chdir "${shared_build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args}
        "${source_dir}/src/configure"
        --host=${target_triple}
        --prefix=/usr
        --libdir=/usr/lib
        --includedir=/usr/include
        --localstatedir=/var
        --disable-static
        --enable-shared
        --disable-rpath
        --disable-nls
        --disable-pkinit
        --without-ldap
        --without-readline
      BUILD_COMMAND ${CMAKE_COMMAND} -E chdir "${shared_build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C util/support -j${CPKT_DEPENDENCY_BUILD_JOBS}
        COMMAND ${CMAKE_COMMAND} -E chdir "${shared_build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C util/et -j${CPKT_DEPENDENCY_BUILD_JOBS}
        COMMAND ${CMAKE_COMMAND} -E chdir "${shared_build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C include -j${CPKT_DEPENDENCY_BUILD_JOBS}
        COMMAND ${CMAKE_COMMAND} -E chdir "${shared_build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C util -j${CPKT_DEPENDENCY_BUILD_JOBS}
        COMMAND ${CMAKE_COMMAND} -E chdir "${shared_build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C lib/crypto -j${CPKT_DEPENDENCY_BUILD_JOBS}
        COMMAND ${CMAKE_COMMAND} -E chdir "${shared_build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C lib/krb5 -j${CPKT_DEPENDENCY_BUILD_JOBS}
        COMMAND ${CMAKE_COMMAND} -E chdir "${shared_build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C lib/gssapi -j${CPKT_DEPENDENCY_BUILD_JOBS}
      INSTALL_COMMAND ${CMAKE_COMMAND} -E chdir "${shared_build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C include install DESTDIR=${stage_dir}
        COMMAND ${CMAKE_COMMAND} -E chdir "${shared_build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C util/support install-libs DESTDIR=${stage_dir}
        COMMAND ${CMAKE_COMMAND} -E chdir "${shared_build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C util/et install-libs DESTDIR=${stage_dir}
        COMMAND ${CMAKE_COMMAND} -E chdir "${shared_build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C util/profile install-libs DESTDIR=${stage_dir}
        COMMAND ${CMAKE_COMMAND} -E chdir "${shared_build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C util/verto install-libs DESTDIR=${stage_dir}
        COMMAND ${CMAKE_COMMAND} -E chdir "${shared_build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C lib/crypto install DESTDIR=${stage_dir}
        COMMAND ${CMAKE_COMMAND} -E chdir "${shared_build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C lib/krb5 install DESTDIR=${stage_dir}
        COMMAND ${CMAKE_COMMAND} -E chdir "${shared_build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C lib/gssapi install DESTDIR=${stage_dir}
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${stage_dir}/usr/include" "${install_dir}/include"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${stage_dir}/usr/lib" "${install_dir}/lib"
        COMMAND ${krb5_darwin_install_name_normalize_command}
        COMMAND ${strip_install_command}
      BUILD_BYPRODUCTS "${gssapi_shared_library}"
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  endif()

  add_library(cpkt::gssapi_krb5_static STATIC IMPORTED GLOBAL)
  set_target_properties(cpkt::gssapi_krb5_static PROPERTIES
    IMPORTED_LOCATION "${gssapi_static_library}"
    INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
    INTERFACE_LINK_LIBRARIES "${krb5_static_library};${k5crypto_static_library};${com_err_static_library};${krb5support_static_library};${profile_static_library};${verto_static_library};${CMAKE_DL_LIBS};Threads::Threads;${krb5_static_platform_libraries}")
  add_library(cpkt::gssapi_krb5_shared SHARED IMPORTED GLOBAL)
  set_target_properties(cpkt::gssapi_krb5_shared PROPERTIES
    IMPORTED_LOCATION "${gssapi_shared_library}"
    INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include")
  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt::gssapi_krb5_static ${project_name_static})
    add_dependencies(cpkt::gssapi_krb5_shared ${project_name_shared})
    cpkt_record_dependency_target(${project_name_shared})
  else()
    cpkt_require_dependency_file("${gssapi_static_library}" "MIT Kerberos GSSAPI static library")
    cpkt_require_dependency_file("${gssapi_shared_library}" "MIT Kerberos GSSAPI shared library")
    cpkt_require_dependency_file("${install_dir}/include/gssapi/gssapi.h" "MIT Kerberos GSSAPI header")
  endif()
  set(CPKT_KRB5_PREFIX "${install_dir}" PARENT_SCOPE)
endfunction()

function(cpkt_add_cyrus_sasl)
  set(project_name "cpkt_cyrus_sasl_project")
  set(prefix_dir "${CPKT_DEPENDENCY_BUILD_ROOT}/cyrus-sasl")
  set(source_dir "${prefix_dir}/src")
  set(build_dir "${prefix_dir}/build")
  set(install_dir "${CPKT_EXTERNAL_ROOT}/cyrus-sasl/install")
  set(stage_dir "${install_dir}/stage")
  set(stamp_dir "${prefix_dir}/stamp")
  set(tmp_dir "${prefix_dir}/tmp")
  set(static_library "${install_dir}/lib/libsasl2${CMAKE_STATIC_LIBRARY_SUFFIX}")
  set(shared_library "${install_dir}/lib/libsasl2${CMAKE_SHARED_LIBRARY_SUFFIX}")
  cpkt_get_target_triple(target_triple)
  cpkt_get_external_c_flags(external_cflags)
  cpkt_get_autotools_link_flags(external_ldflags)
  set(env_args "")
  cpkt_append_pinned_external_toolchain_env_args(env_args)
  list(APPEND env_args
    # Cyrus SASL contributes static objects to cpkt::postgres.
    "CFLAGS=${external_cflags} -fPIC"
    # Cyrus SASL 2.1.28 defaults its bundled MD5 code to K&R declarations
    # unless the build tells it that the compiler has ANSI prototypes.
    "CPPFLAGS=-DPROTOTYPES=1 -DHAVE_TIME_H=1 -I${CPKT_KRB5_PREFIX}/include -I${CPKT_OPENSSL_shared_PREFIX}/include"
    "LDFLAGS=-L${CPKT_KRB5_PREFIX}/lib -L${CPKT_OPENSSL_shared_PREFIX}/lib ${external_ldflags}"
    # Cyrus's CMU_HAVE_OPENSSL macro normally adds an absolute rpath for its
    # OpenSSL prefix.  This cache value keeps the link search path while
    # leaving the installed library relocatable.
    "andrew_cv_runpath_switch=none")
  if(CMAKE_CROSSCOMPILING)
    # MIT Kerberos provides SPNEGO; Cyrus SASL's configure script otherwise
    # attempts to execute this capability probe for every cross target.
    list(APPEND env_args "ac_cv_gssapi_supports_spnego=yes")
  endif()
  cpkt_get_strip_dependency_install_command(strip_install_command "${install_dir}")
  set(cyrus_sasl_platform_configure_args "")
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # Cyrus SASL enables a system-wide /Library/Frameworks install hook by
    # default on Darwin.  It ignores DESTDIR, so disable it and stage only the
    # headers and libraries that belong in the SDK bundle.
    list(APPEND cyrus_sasl_platform_configure_args --disable-macos-framework)
  endif()
  set(cyrus_sasl_rpath_rewrite_command ${CMAKE_COMMAND} -E true)
  set(cyrus_sasl_darwin_install_name_normalize_command ${CMAKE_COMMAND} -E true)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(cyrus_sasl_rpath_rewrite_command
      ${CMAKE_COMMAND}
        -DCPKT_AUTOTOOLS_LIBTOOL=${build_dir}/libtool
        -P ${CMAKE_SOURCE_DIR}/cmake/disable_autotools_absolute_rpath.cmake)
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(cyrus_sasl_darwin_install_name_normalize_command
      ${CMAKE_COMMAND}
        -DCPKT_DARWIN_LIBRARY_DIR=${install_dir}/lib
        -DCPKT_DARWIN_INSTALL_NAME_TOOL=${CMAKE_INSTALL_NAME_TOOL}
        -DCPKT_DARWIN_OTOOL=${CPKT_OTOOL}
        -P ${CMAKE_SOURCE_DIR}/cmake/normalize_darwin_dylib_install_names.cmake)
  endif()
  file(MAKE_DIRECTORY "${install_dir}/include" "${install_dir}/lib")
  if(CPKT_BUILD_DEPENDENCIES)
    cpkt_cached_external_project_add(${project_name}
      URL "https://github.com/cyrusimap/cyrus-sasl/releases/download/cyrus-sasl-${CPKT_CYRUS_SASL_VERSION}/cyrus-sasl-${CPKT_CYRUS_SASL_VERSION}.tar.gz"
      URL_HASH "SHA256=7ccfc6abd01ed67c1a0924b353e526f1b766b21f42d4562ee635a8ebfc5bb38c"
      DOWNLOAD_NAME "cyrus-sasl-${CPKT_CYRUS_SASL_VERSION}.tar.gz"
      PREFIX "${prefix_dir}"
      DOWNLOAD_DIR "${CPKT_DOWNLOAD_ROOT}"
      SOURCE_DIR "${source_dir}"
      BINARY_DIR "${build_dir}"
      STAMP_DIR "${stamp_dir}"
      TMP_DIR "${tmp_dir}"
      TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_TIMEOUT}
      INACTIVITY_TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_INACTIVITY_TIMEOUT}
      DEPENDS cpkt_krb5_shared_project cpkt_openssl_project
      CONFIGURE_COMMAND ${CMAKE_COMMAND} -E chdir "${build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args}
        "${source_dir}/configure"
        --host=${target_triple}
        --prefix=/usr
        --libdir=/usr/lib
        --includedir=/usr/include
        --sysconfdir=/etc
        --enable-static
        --enable-shared
        --disable-sample
        --disable-obsolete_cram_attr
        --disable-obsolete_digest_attr
        --disable-checkapop
        --disable-cram
        --disable-digest
        --disable-scram
        --disable-otp
        --disable-plain
        --disable-anon
        --without-saslauthd
        --enable-gssapi=${CPKT_KRB5_PREFIX}
        --with-openssl=${CPKT_OPENSSL_shared_PREFIX}
        ${cyrus_sasl_platform_configure_args}
      BUILD_COMMAND ${cyrus_sasl_rpath_rewrite_command}
        COMMAND ${CMAKE_COMMAND} -E chdir "${build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C include -j${CPKT_DEPENDENCY_BUILD_JOBS}
        COMMAND ${CMAKE_COMMAND} -E chdir "${build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C common -j${CPKT_DEPENDENCY_BUILD_JOBS}
        COMMAND ${CMAKE_COMMAND} -E chdir "${build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C lib -j${CPKT_DEPENDENCY_BUILD_JOBS}
      INSTALL_COMMAND ${CMAKE_COMMAND} -E remove_directory "${install_dir}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${stage_dir}/usr/include" "${stage_dir}/usr/lib"
        COMMAND ${CMAKE_COMMAND} -E chdir "${build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C include install DESTDIR=${stage_dir}
        COMMAND ${CMAKE_COMMAND} -E chdir "${build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C lib install DESTDIR=${stage_dir}
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${stage_dir}/usr/include" "${install_dir}/include"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${stage_dir}/usr/lib" "${install_dir}/lib"
        COMMAND ${cyrus_sasl_darwin_install_name_normalize_command}
        COMMAND ${strip_install_command}
      BUILD_BYPRODUCTS "${static_library}" "${shared_library}"
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  endif()
  add_library(cpkt::cyrus_sasl_static STATIC IMPORTED GLOBAL)
  set_target_properties(cpkt::cyrus_sasl_static PROPERTIES
    IMPORTED_LOCATION "${static_library}"
    INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
    INTERFACE_LINK_LIBRARIES "cpkt::gssapi_krb5_static;cpkt::openssl_ssl_static;cpkt::openssl_crypto_static;${CMAKE_DL_LIBS};Threads::Threads")
  add_library(cpkt::cyrus_sasl_shared SHARED IMPORTED GLOBAL)
  set_target_properties(cpkt::cyrus_sasl_shared PROPERTIES
    IMPORTED_LOCATION "${shared_library}"
    INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
    INTERFACE_LINK_LIBRARIES "cpkt::gssapi_krb5_shared;cpkt::openssl_ssl_shared;cpkt::openssl_crypto_shared")
  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt::cyrus_sasl_static ${project_name})
    add_dependencies(cpkt::cyrus_sasl_shared ${project_name})
    cpkt_record_dependency_target(${project_name})
  else()
    cpkt_require_dependency_file("${static_library}" "Cyrus SASL static library")
    cpkt_require_dependency_file("${shared_library}" "Cyrus SASL shared library")
    cpkt_require_dependency_file("${install_dir}/include/sasl/sasl.h" "Cyrus SASL header")
  endif()
  set(CPKT_CYRUS_SASL_PREFIX "${install_dir}" PARENT_SCOPE)
endfunction()

function(cpkt_add_openldap)
  set(project_name "cpkt_openldap_project")
  set(prefix_dir "${CPKT_DEPENDENCY_BUILD_ROOT}/openldap")
  set(source_dir "${prefix_dir}/src")
  set(build_dir "${prefix_dir}/build")
  set(install_dir "${CPKT_EXTERNAL_ROOT}/openldap/install")
  set(stage_dir "${install_dir}/stage")
  set(stamp_dir "${prefix_dir}/stamp")
  set(tmp_dir "${prefix_dir}/tmp")
  set(ldap_static_library "${install_dir}/lib/libldap${CMAKE_STATIC_LIBRARY_SUFFIX}")
  set(lber_static_library "${install_dir}/lib/liblber${CMAKE_STATIC_LIBRARY_SUFFIX}")
  set(lutil_static_library "${install_dir}/lib/liblutil${CMAKE_STATIC_LIBRARY_SUFFIX}")
  set(ldap_shared_library "${install_dir}/lib/libldap${CMAKE_SHARED_LIBRARY_SUFFIX}")
  cpkt_get_target_triple(target_triple)
  cpkt_get_external_c_flags(external_cflags)
  cpkt_get_autotools_link_flags(external_ldflags)
  set(env_args "")
  cpkt_append_pinned_external_toolchain_env_args(env_args)
  list(APPEND env_args
    # OpenLDAP contributes static objects to cpkt::postgres.
    "CFLAGS=${external_cflags} -fPIC"
    "CPPFLAGS=-I${CPKT_CYRUS_SASL_PREFIX}/include -I${CPKT_OPENSSL_shared_PREFIX}/include -I${CPKT_KRB5_PREFIX}/include"
    "LDFLAGS=-L${CPKT_CYRUS_SASL_PREFIX}/lib -L${CPKT_OPENSSL_shared_PREFIX}/lib -L${CPKT_KRB5_PREFIX}/lib ${external_ldflags}")
  cpkt_get_strip_dependency_install_command(strip_install_command "${install_dir}")
  set(openldap_rpath_rewrite_command ${CMAKE_COMMAND} -E true)
  set(openldap_darwin_install_name_normalize_command ${CMAKE_COMMAND} -E true)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(openldap_rpath_rewrite_command
      ${CMAKE_COMMAND}
        -DCPKT_AUTOTOOLS_LIBTOOL=${build_dir}/libtool
        -P ${CMAKE_SOURCE_DIR}/cmake/disable_autotools_absolute_rpath.cmake)
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(openldap_darwin_install_name_normalize_command
      ${CMAKE_COMMAND}
        -DCPKT_DARWIN_LIBRARY_DIR=${install_dir}/lib
        -DCPKT_DARWIN_INSTALL_NAME_TOOL=${CMAKE_INSTALL_NAME_TOOL}
        -DCPKT_DARWIN_OTOOL=${CPKT_OTOOL}
        -P ${CMAKE_SOURCE_DIR}/cmake/normalize_darwin_dylib_install_names.cmake)
  endif()
  file(MAKE_DIRECTORY "${install_dir}/include" "${install_dir}/lib")
  if(CPKT_BUILD_DEPENDENCIES)
    cpkt_cached_external_project_add(${project_name}
      URL "https://www.openldap.org/software/download/OpenLDAP/openldap-release/openldap-${CPKT_OPENLDAP_VERSION}.tgz"
      URL_HASH "SHA256=bc91225dbfc50354033b1303bc91d1a7f6ddd1dc32fac950d79c28fe66d6bca8"
      DOWNLOAD_NAME "openldap-${CPKT_OPENLDAP_VERSION}.tgz"
      PREFIX "${prefix_dir}"
      DOWNLOAD_DIR "${CPKT_DOWNLOAD_ROOT}"
      SOURCE_DIR "${source_dir}"
      BINARY_DIR "${build_dir}"
      STAMP_DIR "${stamp_dir}"
      TMP_DIR "${tmp_dir}"
      TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_TIMEOUT}
      INACTIVITY_TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_INACTIVITY_TIMEOUT}
      DEPENDS cpkt_cyrus_sasl_project cpkt_openssl_project cpkt_krb5_shared_project
      PATCH_COMMAND ${CMAKE_COMMAND}
        -DCPKT_OPENLDAP_SOURCE_DIR=${source_dir}
        -P ${CMAKE_SOURCE_DIR}/cmake/patch_openldap_lutil_link.cmake
      CONFIGURE_COMMAND ${CMAKE_COMMAND} -E chdir "${build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args}
        "${source_dir}/configure"
        --host=${target_triple}
        --prefix=/usr
        --libdir=/usr/lib
        --includedir=/usr/include
        --sysconfdir=/etc/openldap
        --localstatedir=/var
        --enable-static
        --enable-shared
        --disable-fast-install
        --disable-slapd
        --disable-syslog
        --with-tls=openssl
        --with-cyrus-sasl
        --with-yielding_select=no
      BUILD_COMMAND ${openldap_rpath_rewrite_command}
        COMMAND ${CMAKE_COMMAND} -E chdir "${build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C include -j${CPKT_DEPENDENCY_BUILD_JOBS}
        COMMAND ${CMAKE_COMMAND} -E chdir "${build_dir}"
        # OpenLDAP's default all target also links upstream diagnostic
        # executables. They are not bundled, and its Darwin static-link recipe
        # is not valid under osxcross. Build production libraries explicitly;
        # staged artifact checks below prove the shipped set.
        ${CMAKE_COMMAND} -E env ${env_args} make -C libraries/liblutil liblutil.a -j${CPKT_DEPENDENCY_BUILD_JOBS}
        COMMAND ${CMAKE_COMMAND} -E chdir "${build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C libraries/liblber liblber.la -j${CPKT_DEPENDENCY_BUILD_JOBS}
        COMMAND ${CMAKE_COMMAND} -E chdir "${build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C libraries/libldap libldap.la -j${CPKT_DEPENDENCY_BUILD_JOBS}
      INSTALL_COMMAND ${CMAKE_COMMAND} -E remove_directory "${install_dir}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${stage_dir}/usr/include" "${stage_dir}/usr/lib"
        COMMAND ${CMAKE_COMMAND} -E chdir "${build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C include install DESTDIR=${stage_dir}
        COMMAND ${CMAKE_COMMAND}
          -DCPKT_AUTOTOOLS_LIBRARY_SOURCE_DIR=${build_dir}/libraries/liblutil
          -DCPKT_AUTOTOOLS_LIBRARY_DESTINATION_DIR=${stage_dir}/usr/lib
          -DCPKT_AUTOTOOLS_LIBRARY_BASENAME=liblutil
          -P ${CMAKE_SOURCE_DIR}/cmake/copy_autotools_library_artifacts.cmake
        COMMAND ${CMAKE_COMMAND}
          -DCPKT_AUTOTOOLS_LIBRARY_SOURCE_DIR=${build_dir}/libraries/liblber/.libs
          -DCPKT_AUTOTOOLS_LIBRARY_DESTINATION_DIR=${stage_dir}/usr/lib
          -DCPKT_AUTOTOOLS_LIBRARY_BASENAME=liblber
          -P ${CMAKE_SOURCE_DIR}/cmake/copy_autotools_library_artifacts.cmake
        COMMAND ${CMAKE_COMMAND}
          -DCPKT_AUTOTOOLS_LIBRARY_SOURCE_DIR=${build_dir}/libraries/libldap/.libs
          -DCPKT_AUTOTOOLS_LIBRARY_DESTINATION_DIR=${stage_dir}/usr/lib
          -DCPKT_AUTOTOOLS_LIBRARY_BASENAME=libldap
          -P ${CMAKE_SOURCE_DIR}/cmake/copy_autotools_library_artifacts.cmake
        # OpenLDAP's Darwin libtool archive embeds liblutil.a as a nested
        # member.  Darwin's linker rejects that archive; liblutil.a is staged
        # and exported separately in the supported static closure.
        COMMAND ${CMAKE_COMMAND}
          -DCPKT_STATIC_ARCHIVE=${stage_dir}/usr/lib/libldap${CMAKE_STATIC_LIBRARY_SUFFIX}
          -DCPKT_STATIC_ARCHIVE_MEMBER=liblutil${CMAKE_STATIC_LIBRARY_SUFFIX}
          -DCPKT_STATIC_ARCHIVER=${CMAKE_AR}
          -DCPKT_STATIC_RANLIB=${CMAKE_RANLIB}
          -P ${CMAKE_SOURCE_DIR}/cmake/remove_static_archive_member.cmake
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${stage_dir}/usr/include" "${install_dir}/include"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${stage_dir}/usr/lib" "${install_dir}/lib"
        COMMAND ${openldap_darwin_install_name_normalize_command}
        COMMAND ${strip_install_command}
      BUILD_BYPRODUCTS "${ldap_static_library}" "${lber_static_library}" "${lutil_static_library}" "${ldap_shared_library}"
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  endif()
  add_library(cpkt::openldap_static STATIC IMPORTED GLOBAL)
  set_target_properties(cpkt::openldap_static PROPERTIES
    IMPORTED_LOCATION "${ldap_static_library}"
    INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
    INTERFACE_LINK_LIBRARIES "${lber_static_library};${lutil_static_library};cpkt::cyrus_sasl_static;cpkt::openssl_ssl_static;cpkt::openssl_crypto_static;cpkt::gssapi_krb5_static;${CMAKE_DL_LIBS};Threads::Threads")
  add_library(cpkt::openldap_shared SHARED IMPORTED GLOBAL)
  set_target_properties(cpkt::openldap_shared PROPERTIES
    IMPORTED_LOCATION "${ldap_shared_library}"
    INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
    INTERFACE_LINK_LIBRARIES "cpkt::cyrus_sasl_shared;cpkt::openssl_ssl_shared;cpkt::openssl_crypto_shared;cpkt::gssapi_krb5_shared")
  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt::openldap_static ${project_name})
    add_dependencies(cpkt::openldap_shared ${project_name})
    cpkt_record_dependency_target(${project_name})
  else()
    cpkt_require_dependency_file("${ldap_static_library}" "OpenLDAP static library")
    cpkt_require_dependency_file("${lber_static_library}" "OpenLDAP LBER static library")
    cpkt_require_dependency_file("${ldap_shared_library}" "OpenLDAP shared library")
    cpkt_require_dependency_file("${install_dir}/include/ldap.h" "OpenLDAP header")
  endif()
  set(CPKT_OPENLDAP_PREFIX "${install_dir}" PARENT_SCOPE)
endfunction()

function(cpkt_add_postgresql)
  set(project_name "cpkt_postgresql_project")
  set(prefix_dir "${CPKT_DEPENDENCY_BUILD_ROOT}/postgresql")
  set(source_dir "${prefix_dir}/src")
  set(build_dir "${prefix_dir}/build")
  set(install_dir "${CPKT_EXTERNAL_ROOT}/postgresql/install")
  set(stage_dir "${install_dir}/stage")
  set(stamp_dir "${prefix_dir}/stamp")
  set(tmp_dir "${prefix_dir}/tmp")
  set(static_library "${install_dir}/lib/libpq${CMAKE_STATIC_LIBRARY_SUFFIX}")
  set(common_static_library "${install_dir}/lib/libpgcommon_shlib${CMAKE_STATIC_LIBRARY_SUFFIX}")
  set(port_static_library "${install_dir}/lib/libpgport${CMAKE_STATIC_LIBRARY_SUFFIX}")
  set(shared_library "${install_dir}/lib/libpq${CMAKE_SHARED_LIBRARY_SUFFIX}")
  string(REGEX MATCH "^[0-9]+" postgresql_major_version "${CPKT_POSTGRESQL_VERSION}")
  set(oauth_static_library "${install_dir}/lib/libpq-oauth${CMAKE_STATIC_LIBRARY_SUFFIX}")
  # PostgreSQL defines libpq-oauth as an internal shared module with no
  # install-name/SONAME.  Keep its static archive in libpq's static closure,
  # but do not ship an unsupported runtime library without ABI identity.
  set(oauth_shared_library "${install_dir}/lib/libpq-oauth-${postgresql_major_version}${CMAKE_SHARED_LIBRARY_SUFFIX}")
  cpkt_get_target_triple(target_triple)
  cpkt_get_external_c_flags(external_cflags)
  cpkt_get_autotools_link_flags(external_ldflags)
  set(postgresql_ldflags "-L${CPKT_ZLIB_PREFIX}/lib -L${CPKT_OPENSSL_shared_PREFIX}/lib -L${CPKT_CURL_PREFIX}/lib -L${CPKT_KRB5_PREFIX}/lib -L${CPKT_OPENLDAP_PREFIX}/lib -L${CPKT_CYRUS_SASL_PREFIX}/lib ${external_ldflags}")
  set(postgresql_curl_libs "-L${CPKT_CURL_PREFIX}/lib -lcurl")
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    string(APPEND postgresql_ldflags
      " -Wl,-rpath-link,${CPKT_OPENSSL_shared_PREFIX}/lib"
      " -Wl,-rpath-link,${CPKT_LIBSSH2_PREFIX}/lib"
      " -Wl,-rpath-link,${CPKT_NGHTTP2_shared_PREFIX}/lib"
      " -Wl,-rpath-link,${CPKT_CYRUS_SASL_PREFIX}/lib")
    string(APPEND postgresql_curl_libs
      " -Wl,-rpath-link,${CPKT_OPENSSL_shared_PREFIX}/lib"
      " -Wl,-rpath-link,${CPKT_LIBSSH2_PREFIX}/lib"
      " -Wl,-rpath-link,${CPKT_NGHTTP2_shared_PREFIX}/lib"
      " -Wl,-rpath-link,${CPKT_CYRUS_SASL_PREFIX}/lib")
  endif()
  set(postgresql_darwin_install_name_normalize_command ${CMAKE_COMMAND} -E true)
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(postgresql_darwin_install_name_normalize_command
      ${CMAKE_COMMAND}
        -DCPKT_DARWIN_LIBRARY_DIR=${install_dir}/lib
        -DCPKT_DARWIN_INSTALL_NAME_TOOL=${CMAKE_INSTALL_NAME_TOOL}
        -DCPKT_DARWIN_OTOOL=${CPKT_OTOOL}
        -P ${CMAKE_SOURCE_DIR}/cmake/normalize_darwin_dylib_install_names.cmake)
  endif()
  set(env_args "")
  cpkt_append_pinned_external_toolchain_env_args(env_args)
  list(APPEND env_args
    # libpq static archives are part of the supported shared-consumer closure.
    "CFLAGS=${external_cflags} -fPIC"
    "CPPFLAGS=-I${CPKT_ZLIB_PREFIX}/include -I${CPKT_OPENSSL_shared_PREFIX}/include -I${CPKT_CURL_PREFIX}/include -I${CPKT_KRB5_PREFIX}/include -I${CPKT_OPENLDAP_PREFIX}/include -I${CPKT_CYRUS_SASL_PREFIX}/include"
    "LDFLAGS=${postgresql_ldflags}"
    # OpenLDAP keeps liblutil as a private static archive.  It is required
    # after libldap for PostgreSQL's LDAP configure and final link checks.
    "LIBS=-llutil"
    "LIBCURL_CFLAGS=-I${CPKT_CURL_PREFIX}/include"
    "LIBCURL_LIBS=${postgresql_curl_libs}")
  set(postgresql_configure_env_args ${env_args})
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # PostgreSQL executes a libcurl feature probe while configuring.  Bundled
    # shared libraries deliberately use $ORIGIN, so make their build-tree
    # locations available only to that probe; do not leak an absolute runtime
    # search path into the installed artifacts.
    list(APPEND postgresql_configure_env_args
      "LD_LIBRARY_PATH=${CPKT_CURL_PREFIX}/lib:${CPKT_ZLIB_PREFIX}/lib:${CPKT_OPENSSL_shared_PREFIX}/lib:${CPKT_LIBSSH2_PREFIX}/lib:${CPKT_NGHTTP2_shared_PREFIX}/lib:${CPKT_KRB5_PREFIX}/lib:${CPKT_OPENLDAP_PREFIX}/lib:${CPKT_CYRUS_SASL_PREFIX}/lib")
  endif()
  set(postgresql_post_configure_command
    COMMAND ${CMAKE_COMMAND}
      -DCPKT_POSTGRESQL_SOURCE_DIR=${source_dir}
      -P ${CMAKE_SOURCE_DIR}/cmake/patch_postgresql_buildinfo.cmake)
  cpkt_get_strip_dependency_install_command(strip_install_command "${install_dir}")
  file(MAKE_DIRECTORY "${install_dir}/include" "${install_dir}/lib")
  if(CPKT_BUILD_DEPENDENCIES)
    cpkt_cached_external_project_add(${project_name}
      URL "https://ftp.postgresql.org/pub/source/v${CPKT_POSTGRESQL_VERSION}/postgresql-${CPKT_POSTGRESQL_VERSION}.tar.bz2"
      URL_HASH "SHA256=555610c24d53e4316da5b7d3fc25c279d96856d5e0e23ee308c328c5fa881d9f"
      DOWNLOAD_NAME "postgresql-${CPKT_POSTGRESQL_VERSION}.tar.bz2"
      PREFIX "${prefix_dir}"
      DOWNLOAD_DIR "${CPKT_DOWNLOAD_ROOT}"
      SOURCE_DIR "${source_dir}"
      BINARY_DIR "${build_dir}"
      STAMP_DIR "${stamp_dir}"
      TMP_DIR "${tmp_dir}"
      TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_TIMEOUT}
      INACTIVITY_TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_INACTIVITY_TIMEOUT}
      DEPENDS cpkt_openldap_project cpkt_cyrus_sasl_project cpkt_krb5_shared_project cpkt_curl_project cpkt_zlib_project cpkt_openssl_project
      CONFIGURE_COMMAND ${CMAKE_COMMAND} -E chdir "${build_dir}"
        ${CMAKE_COMMAND} -E env ${postgresql_configure_env_args}
        "${source_dir}/configure"
        --host=${target_triple}
        --prefix=/usr
        --libdir=/usr/lib
        --includedir=/usr/include
        --sysconfdir=/etc
        --disable-rpath
        --disable-nls
        --without-icu
        --without-readline
        --with-ssl=openssl
        --with-gssapi
        --with-ldap
        --with-libcurl
        ${postgresql_post_configure_command}
      BUILD_COMMAND ${CMAKE_COMMAND} -E chdir "${build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C src/common -j${CPKT_DEPENDENCY_BUILD_JOBS}
        COMMAND ${CMAKE_COMMAND} -E chdir "${build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C src/port -j${CPKT_DEPENDENCY_BUILD_JOBS}
        COMMAND ${CMAKE_COMMAND} -E chdir "${build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C src/interfaces/libpq -j${CPKT_DEPENDENCY_BUILD_JOBS}
        COMMAND ${CMAKE_COMMAND} -E chdir "${build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C src/interfaces/libpq-oauth -j${CPKT_DEPENDENCY_BUILD_JOBS}
      INSTALL_COMMAND ${CMAKE_COMMAND} -E remove_directory "${install_dir}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${stage_dir}/usr/include" "${stage_dir}/usr/lib"
        COMMAND ${CMAKE_COMMAND} -E chdir "${build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C src/interfaces/libpq install DESTDIR=${stage_dir}
        COMMAND ${CMAKE_COMMAND} -E chdir "${build_dir}"
        ${CMAKE_COMMAND} -E env ${env_args} make -C src/interfaces/libpq-oauth install DESTDIR=${stage_dir}
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${stage_dir}/usr/include" "${install_dir}/include"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${stage_dir}/usr/lib" "${install_dir}/lib"
        COMMAND ${CMAKE_COMMAND} -E rm -f "${oauth_shared_library}"
        COMMAND ${postgresql_darwin_install_name_normalize_command}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${build_dir}/src/common/libpgcommon_shlib${CMAKE_STATIC_LIBRARY_SUFFIX}"
          "${common_static_library}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${build_dir}/src/port/libpgport${CMAKE_STATIC_LIBRARY_SUFFIX}"
          "${port_static_library}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${source_dir}/src/include/postgres_ext.h"
          "${install_dir}/include/postgres_ext.h"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${install_dir}/share"
        COMMAND ${strip_install_command}
      BUILD_BYPRODUCTS "${static_library}" "${common_static_library}" "${port_static_library}" "${oauth_static_library}" "${shared_library}"
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  endif()
  add_library(cpkt::postgresql_static STATIC IMPORTED GLOBAL)
  set_target_properties(cpkt::postgresql_static PROPERTIES
    IMPORTED_LOCATION "${static_library}"
    INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
    INTERFACE_LINK_LIBRARIES "${oauth_static_library};${common_static_library};${port_static_library};cpkt::openldap_static;cpkt::gssapi_krb5_static;cpkt::cyrus_sasl_static;cpkt::curl_static;cpkt::openssl_ssl_static;cpkt::openssl_crypto_static;cpkt::zlib_static;${CMAKE_DL_LIBS};Threads::Threads;m")
  add_library(cpkt::postgresql_shared SHARED IMPORTED GLOBAL)
  set_target_properties(cpkt::postgresql_shared PROPERTIES
    IMPORTED_LOCATION "${shared_library}"
    INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
    INTERFACE_LINK_LIBRARIES "cpkt::openldap_shared;cpkt::gssapi_krb5_shared;cpkt::cyrus_sasl_shared;cpkt::curl_shared;cpkt::openssl_ssl_shared;cpkt::openssl_crypto_shared;cpkt::zlib_shared")
  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt::postgresql_static ${project_name})
    add_dependencies(cpkt::postgresql_shared ${project_name})
    cpkt_record_dependency_target(${project_name})
  else()
    cpkt_require_dependency_file("${static_library}" "PostgreSQL libpq static library")
    cpkt_require_dependency_file("${common_static_library}" "PostgreSQL frontend common static library")
    cpkt_require_dependency_file("${port_static_library}" "PostgreSQL frontend port static library")
    cpkt_require_dependency_file("${oauth_static_library}" "PostgreSQL OAuth static library")
    cpkt_require_dependency_file("${shared_library}" "PostgreSQL libpq shared library")
    cpkt_require_dependency_file("${install_dir}/include/libpq-fe.h" "PostgreSQL libpq header")
  endif()
  set(CPKT_POSTGRESQL_PREFIX "${install_dir}" PARENT_SCOPE)
endfunction()

function(cpkt_add_cmocka)
  set(project_name "cpkt_cmocka_project")
  set(prefix_dir "${CPKT_DEPENDENCY_BUILD_ROOT}/cmocka")
  set(source_dir "${prefix_dir}/src")
  set(build_dir "${prefix_dir}/build")
  set(install_dir "${CPKT_EXTERNAL_ROOT}/cmocka/install")
  set(stamp_dir "${prefix_dir}/stamp")
  set(tmp_dir "${prefix_dir}/tmp")
  cpkt_append_common_external_cmake_args(common_cmake_args)
  cpkt_get_external_cmake_step_commands(cmake_build_command cmake_install_command)
  cpkt_get_strip_dependency_install_command(strip_install_command "${install_dir}")
  file(MAKE_DIRECTORY "${install_dir}/include" "${install_dir}/lib")

  if(CPKT_BUILD_DEPENDENCIES)
    cpkt_cached_external_project_add(${project_name}
      URL "https://cmocka.org/files/2.0/cmocka-${CPKT_CMOCKA_VERSION}.tar.xz"
      URL_HASH "SHA256=39f92f366bdf3f1a02af4da75b4a5c52df6c9f7e736c7d65de13283f9f0ef416"
      PREFIX "${prefix_dir}"
      DOWNLOAD_DIR "${CPKT_DOWNLOAD_ROOT}"
      SOURCE_DIR "${source_dir}"
      BINARY_DIR "${build_dir}"
      STAMP_DIR "${stamp_dir}"
      TMP_DIR "${tmp_dir}"
      TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_TIMEOUT}
      INACTIVITY_TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_INACTIVITY_TIMEOUT}
      CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${install_dir}
        -DCMAKE_BUILD_TYPE=${CPKT_DEPENDENCY_BUILD_TYPE}
        -DBUILD_SHARED_LIBS=OFF
        -DBUILD_TESTING=OFF
        -DWITH_EXAMPLES=OFF
        -DPICKY_DEVELOPER=OFF
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        ${common_cmake_args}
      BUILD_COMMAND ${cmake_build_command}
      INSTALL_COMMAND ${cmake_install_command}
        COMMAND ${strip_install_command}
      BUILD_BYPRODUCTS
        "${install_dir}/lib/libcmocka${CMAKE_STATIC_LIBRARY_SUFFIX}"
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
  endif()

  add_library(cpkt::cmocka STATIC IMPORTED GLOBAL)
  set_target_properties(cpkt::cmocka
    PROPERTIES
      IMPORTED_LOCATION "${install_dir}/lib/libcmocka${CMAKE_STATIC_LIBRARY_SUFFIX}"
      INTERFACE_INCLUDE_DIRECTORIES "${install_dir}/include"
  )
  if(CPKT_BUILD_DEPENDENCIES)
    add_dependencies(cpkt::cmocka ${project_name})
    cpkt_record_dependency_target(${project_name})
  else()
    cpkt_require_dependency_file("${install_dir}/lib/libcmocka${CMAKE_STATIC_LIBRARY_SUFFIX}" "cmocka")
  endif()
endfunction()

function(cpkt_configure_dependencies)
  cpkt_add_openssl()
  cpkt_add_zlib()
  cpkt_add_libssh2()
  cpkt_add_nghttp2()
  cpkt_add_curl()
  cpkt_add_libxml2()
  cpkt_add_lua()
  cpkt_add_miniaudio()
  cpkt_add_whisper()
  cpkt_add_mqttc()
  cpkt_add_open62541()
  cpkt_add_krb5()
  cpkt_add_cyrus_sasl()
  cpkt_add_openldap()
  cpkt_add_postgresql()

  if(CPKT_BUILD_TESTS AND NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    cpkt_add_cmocka()
  endif()

  if(CPKT_BUILD_DEPENDENCIES)
    get_property(dep_targets GLOBAL PROPERTY CPKT_DEPENDENCY_TARGETS)
    if(dep_targets)
      add_custom_target(cpkt_deps DEPENDS ${dep_targets})
    endif()
  endif()
endfunction()
