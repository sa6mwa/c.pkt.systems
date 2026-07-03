include(ExternalProject)

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
  if(NOT "${CMAKE_CXX_FLAGS}" STREQUAL "")
    set(_flags "${CMAKE_CXX_FLAGS} ${_flags}")
  endif()
  string(STRIP "${_flags}" _flags)
  set(${out_var} "${_flags}" PARENT_SCOPE)
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

function(cpkt_append_common_external_cmake_args out_var)
  set(_args
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_AR=${CMAKE_AR}
    -DCMAKE_RANLIB=${CMAKE_RANLIB}
    -Wno-dev
  )

  if(CMAKE_TOOLCHAIN_FILE)
    list(APPEND _args -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
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

function(cpkt_add_openssl)
  set(project_name "cpkt_openssl_project")
  set(prefix_dir "${CPKT_DEPENDENCY_BUILD_ROOT}/openssl")
  set(source_dir "${prefix_dir}/src")
  set(build_dir "${prefix_dir}/build")
  set(install_dir "${CPKT_EXTERNAL_ROOT}/openssl/install")
  set(stamp_dir "${prefix_dir}/stamp")
  set(tmp_dir "${prefix_dir}/tmp")
  cpkt_get_openssl_config_target(openssl_config_target)
  set(openssl_dir "/etc/ssl")
  set(config_args ${openssl_config_target} no-tests no-docs no-module no-apps no-makedepend)
  if(CPKT_TARGET_LIBC STREQUAL "musl")
    list(APPEND config_args no-secure-memory no-afalgeng)
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND config_args shared "-Wl,--enable-new-dtags,-rpath,\\$$ORIGIN")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND config_args shared "-Wl,-rpath,@loader_path")
  else()
    list(APPEND config_args shared)
  endif()

  cpkt_normalize_prefix(env_prefix "${install_dir}")
  file(MAKE_DIRECTORY "${install_dir}/include" "${install_dir}/lib")
  set(build_command make -j${CPKT_DEPENDENCY_BUILD_JOBS})
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
  list(APPEND openssl_env_args CFLAGS=${openssl_cflags})
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_LINKER)
    list(APPEND openssl_env_args LDFLAGS=-fuse-ld=${CMAKE_LINKER})
  endif()

  if(CPKT_BUILD_DEPENDENCIES)
    ExternalProject_Add(${project_name}
      URL "https://github.com/openssl/openssl/releases/download/openssl-${CPKT_OPENSSL_VERSION}/openssl-${CPKT_OPENSSL_VERSION}.tar.gz"
      URL_HASH "SHA256=aaf51a1fe064384f811daeaeb4ec4dce7340ec8bd893027eee676af31e83a04f"
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
      BUILD_COMMAND ${build_command}
      INSTALL_COMMAND ${install_command}
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
    list(APPEND nghttp2_env_args
      PATH=${CPKT_OSXCROSS_BIN_DIR}:$ENV{PATH}
      LDFLAGS=-fuse-ld=${CMAKE_LINKER}
    )
    set(nghttp2_post_configure_command
      COMMAND ${CMAKE_COMMAND}
        -DCPKT_DARWIN_INSTALL_NAME_FILE=${build_dir}/libtool
        -P ${CMAKE_SOURCE_DIR}/cmake/patch_darwin_generated_install_names.cmake)
  endif()

  if(CPKT_BUILD_DEPENDENCIES)
    ExternalProject_Add(${project_name}
      URL "https://github.com/nghttp2/nghttp2/releases/download/v${CPKT_NGHTTP2_VERSION}/nghttp2-${CPKT_NGHTTP2_VERSION}.tar.gz"
      URL_HASH "SHA256=c866b7477cbb7512ab6863a685027adbb1bb8da8fc3bab7429ed43d3281d5aa9"
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
        --enable-lib-only
        ${nghttp2_post_configure_command}
      BUILD_COMMAND make -C lib -j${CPKT_DEPENDENCY_BUILD_JOBS}
      INSTALL_COMMAND make -C lib install
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
    ExternalProject_Add(${project_name}
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
      BUILD_COMMAND ${CMAKE_COMMAND} --build . --parallel ${CPKT_DEPENDENCY_BUILD_JOBS}
      INSTALL_COMMAND ${CMAKE_COMMAND} --install .
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
    ExternalProject_Add(${project_name}
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
      BUILD_COMMAND ${CMAKE_COMMAND} --build . --parallel ${CPKT_DEPENDENCY_BUILD_JOBS}
      INSTALL_COMMAND ${CMAKE_COMMAND} --install .
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
  cpkt_get_strip_dependency_install_command(strip_install_command "${install_dir}")
  file(MAKE_DIRECTORY "${install_dir}/include" "${install_dir}/lib")
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(curl_install_rpath "@loader_path")
    set(curl_platform_cmake_args "")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(curl_install_rpath "$ORIGIN")
    set(curl_platform_cmake_args
      -DCMAKE_SHARED_LINKER_FLAGS=-Wl,--enable-new-dtags)
  else()
    set(curl_install_rpath "")
    set(curl_platform_cmake_args "")
  endif()
  set(curl_static_platform_libs "")
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND curl_static_platform_libs "-framework CoreFoundation" "-framework SystemConfiguration")
  endif()

  if(CPKT_BUILD_DEPENDENCIES)
    ExternalProject_Add(${project_name}
      URL "https://curl.se/download/curl-${CPKT_CURL_VERSION}.tar.xz"
      URL_HASH "SHA256=63fe2dc148ba0ceae89922ef838f7e5c946272c2e78b7c59fab4b79d3ce2b896"
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
      CMAKE_ARGS
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
      BUILD_COMMAND ${CMAKE_COMMAND} --build . --parallel ${CPKT_DEPENDENCY_BUILD_JOBS}
      INSTALL_COMMAND ${CMAKE_COMMAND} --install .
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
    set(libxml2_shared_library "${install_dir}/lib/libxml2${CMAKE_SHARED_LIBRARY_SUFFIX}.16.1.3")
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
    ExternalProject_Add(${project_name_shared}
      URL "https://download.gnome.org/sources/libxml2/2.15/libxml2-${CPKT_LIBXML2_VERSION}.tar.xz"
      URL_HASH "SHA256=78262a6e7ac170d6528ebfe2efccdf220191a5af6a6cd61ea4a9a9a5042c7a07"
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
      CMAKE_ARGS
        -DBUILD_SHARED_LIBS=ON
        ${libxml2_common_cmake_args}
        -DZLIB_LIBRARY=${CPKT_ZLIB_SHARED_LIBRARY}
      BUILD_COMMAND ${CMAKE_COMMAND} --build . --parallel ${CPKT_DEPENDENCY_BUILD_JOBS}
      INSTALL_COMMAND ${CMAKE_COMMAND} --install .
      BUILD_BYPRODUCTS "${libxml2_shared_library}"
      BUILD_IN_SOURCE 0
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    ExternalProject_Add(${project_name_static}
      URL "https://download.gnome.org/sources/libxml2/2.15/libxml2-${CPKT_LIBXML2_VERSION}.tar.xz"
      URL_HASH "SHA256=78262a6e7ac170d6528ebfe2efccdf220191a5af6a6cd61ea4a9a9a5042c7a07"
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
      CMAKE_ARGS
        -DBUILD_SHARED_LIBS=OFF
        ${libxml2_common_cmake_args}
        -DZLIB_LIBRARY=${CPKT_ZLIB_PREFIX}/lib/libz${CMAKE_STATIC_LIBRARY_SUFFIX}
      BUILD_COMMAND ${CMAKE_COMMAND} --build . --parallel ${CPKT_DEPENDENCY_BUILD_JOBS}
      INSTALL_COMMAND ${CMAKE_COMMAND} --install .
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
    set(lua_shared_library "liblua.5.5.0${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(lua_shared_soname "liblua.5.5${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(lua_shared_link "liblua${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(lua_shared_link_flags -dynamiclib -Wl,-install_name,@rpath/${lua_shared_soname})
    set(lua_shared_libs -lm)
  else()
    set(lua_shared_library "liblua${CMAKE_SHARED_LIBRARY_SUFFIX}.5.5.0")
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
    ExternalProject_Add(${project_name}
      URL "https://lua.org/ftp/lua-${CPKT_LUA_VERSION}.tar.gz"
      URL_HASH "SHA256=57ccc32bbbd005cab75bcc52444052535af691789dba2b9016d5c50640d68b3d"
      DOWNLOAD_NAME "lua-${CPKT_LUA_VERSION}.tar.gz"
      PREFIX "${prefix_dir}"
      DOWNLOAD_DIR "${CPKT_DOWNLOAD_ROOT}"
      SOURCE_DIR "${source_dir}"
      STAMP_DIR "${stamp_dir}"
      TMP_DIR "${tmp_dir}"
      TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_TIMEOUT}
      INACTIVITY_TIMEOUT ${CPKT_DEPENDENCY_DOWNLOAD_INACTIVITY_TIMEOUT}
      CONFIGURE_COMMAND ""
      BUILD_COMMAND
        ${CMAKE_COMMAND} -E env MAKEFLAGS= make -C "${source_dir}/src" clean
        COMMAND
          ${CMAKE_COMMAND} -E env MAKEFLAGS= make -C "${source_dir}/src" a -j1
            CC=${CMAKE_C_COMPILER}
            AR=${CMAKE_AR}\ rcu
            RANLIB=${CMAKE_RANLIB}
            MYCFLAGS=${lua_my_cflags}
        COMMAND
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
    ExternalProject_Add(${project_name}
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
        COMMAND ${CMAKE_C_COMPILER} ${mqttc_compile_flags} -c "${source_dir}/src/mqtt.c" -o "${build_dir}/mqtt.c.o"
        COMMAND ${CMAKE_C_COMPILER} ${mqttc_compile_flags} -c "${source_dir}/src/mqtt_pal.c" -o "${build_dir}/mqtt_pal.c.o"
        COMMAND ${CMAKE_COMMAND} -E rm -f "${mqttc_static_library}"
        COMMAND ${CMAKE_AR} qc "${mqttc_static_library}" "${build_dir}/mqtt.c.o" "${build_dir}/mqtt_pal.c.o"
        COMMAND ${CMAKE_RANLIB} "${mqttc_static_library}"
        COMMAND ${CMAKE_COMMAND} -E rm -f "${mqttc_shared_library}"
        COMMAND ${CMAKE_C_COMPILER} ${mqttc_shared_link_flags} ${mqttc_shared_extra_link_flags} -o "${mqttc_shared_library}" "${build_dir}/mqtt.c.o" "${build_dir}/mqtt_pal.c.o" ${mqttc_link_flags}
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
    -DMA_NO_DEVICE_IO
    -DMA_NO_RESOURCE_MANAGER
    -DMA_NO_NODE_GRAPH
    -DMA_NO_ENGINE
    -DMA_NO_GENERATION
    -DMA_NO_RUNTIME_LINKING
  )
  set(miniaudio_object "${build_dir}/miniaudio.c.o")
  set(miniaudio_link_libraries -lm -pthread)

  if(CPKT_BUILD_DEPENDENCIES)
    ExternalProject_Add(${project_name}
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
        ${CMAKE_C_COMPILER}
          ${miniaudio_compile_flags}
          -c "${source_dir}/miniaudio.c"
          -o "${miniaudio_object}"
        COMMAND ${CMAKE_COMMAND} -E rm -f "${miniaudio_static_library}"
        COMMAND ${CMAKE_AR} qc "${miniaudio_static_library}" "${miniaudio_object}"
        COMMAND ${CMAKE_RANLIB} "${miniaudio_static_library}"
        COMMAND ${CMAKE_COMMAND} -E rm -f "${miniaudio_shared_library}"
        COMMAND ${CMAKE_C_COMPILER}
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
    ExternalProject_Add(${project_name_shared}
      URL "https://github.com/ggml-org/whisper.cpp/archive/refs/tags/${CPKT_WHISPER_VERSION}.tar.gz"
      URL_HASH "SHA256=147267177eef7b22ec3d2476dd514d1b12e160e176230b740e3d1bd600118447"
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
      BUILD_COMMAND ${CMAKE_COMMAND} --build . --parallel ${CPKT_DEPENDENCY_BUILD_JOBS}
      INSTALL_COMMAND ${CMAKE_COMMAND} --install .
      BUILD_BYPRODUCTS
        "${whisper_shared_library}"
        ${ggml_shared_libraries}
      BUILD_IN_SOURCE 0
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    ExternalProject_Add(${project_name_static}
      URL "https://github.com/ggml-org/whisper.cpp/archive/refs/tags/${CPKT_WHISPER_VERSION}.tar.gz"
      URL_HASH "SHA256=147267177eef7b22ec3d2476dd514d1b12e160e176230b740e3d1bd600118447"
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
      BUILD_COMMAND ${CMAKE_COMMAND} --build . --parallel ${CPKT_DEPENDENCY_BUILD_JOBS}
      INSTALL_COMMAND ${CMAKE_COMMAND} --install .
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
    ExternalProject_Add(${project_name_shared}
      URL "https://github.com/open62541/open62541/archive/refs/tags/v${CPKT_OPEN62541_VERSION}.tar.gz"
      URL_HASH "SHA256=fb5aafc19c67a91368d1f71d9ee4acf0f4b47a0d65c66db4ed738691828779c7"
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
      BUILD_COMMAND ${CMAKE_COMMAND} --build . --parallel ${CPKT_DEPENDENCY_BUILD_JOBS}
      INSTALL_COMMAND ${CMAKE_COMMAND} --install .
      BUILD_BYPRODUCTS "${open62541_shared_library}"
      BUILD_IN_SOURCE 0
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    ExternalProject_Add(${project_name_static}
      URL "https://github.com/open62541/open62541/archive/refs/tags/v${CPKT_OPEN62541_VERSION}.tar.gz"
      URL_HASH "SHA256=fb5aafc19c67a91368d1f71d9ee4acf0f4b47a0d65c66db4ed738691828779c7"
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
      BUILD_COMMAND ${CMAKE_COMMAND} --build . --parallel ${CPKT_DEPENDENCY_BUILD_JOBS}
      INSTALL_COMMAND ${CMAKE_COMMAND} --install .
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

function(cpkt_add_cmocka)
  set(project_name "cpkt_cmocka_project")
  set(prefix_dir "${CPKT_DEPENDENCY_BUILD_ROOT}/cmocka")
  set(source_dir "${prefix_dir}/src")
  set(build_dir "${prefix_dir}/build")
  set(install_dir "${CPKT_EXTERNAL_ROOT}/cmocka/install")
  set(stamp_dir "${prefix_dir}/stamp")
  set(tmp_dir "${prefix_dir}/tmp")
  cpkt_append_common_external_cmake_args(common_cmake_args)
  cpkt_get_strip_dependency_install_command(strip_install_command "${install_dir}")
  file(MAKE_DIRECTORY "${install_dir}/include" "${install_dir}/lib")

  if(CPKT_BUILD_DEPENDENCIES)
    ExternalProject_Add(${project_name}
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
      BUILD_COMMAND ${CMAKE_COMMAND} --build . --parallel ${CPKT_DEPENDENCY_BUILD_JOBS}
      INSTALL_COMMAND ${CMAKE_COMMAND} --install .
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
