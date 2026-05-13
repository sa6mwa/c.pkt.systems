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

function(cpkt_get_strip_dependency_install_command out_var install_dir)
  if(NOT CMAKE_STRIP)
    message(FATAL_ERROR "CMAKE_STRIP is required when building release dependencies")
  endif()

  set(_strip_static_archives ON)
  set(_darwin_fixup_args "")
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(_strip_static_archives OFF)
    if(NOT CMAKE_INSTALL_NAME_TOOL)
      message(FATAL_ERROR "CMAKE_INSTALL_NAME_TOOL is required when building Darwin dependencies")
    endif()
    if(NOT CPKT_OTOOL)
      message(FATAL_ERROR "CPKT_OTOOL is required when building Darwin dependencies")
    endif()
    set(_darwin_fixup_args
      -DCPKT_DARWIN_DEPENDENCY_ROOT=${CPKT_EXTERNAL_ROOT}
      -DCPKT_INSTALL_NAME_TOOL=${CMAKE_INSTALL_NAME_TOOL}
      -DCPKT_OTOOL=${CPKT_OTOOL}
    )
  endif()

  set(_command
    ${CMAKE_COMMAND}
      -DCPKT_STRIP_BIN=${CMAKE_STRIP}
      -DCPKT_STRIP_ROOT=${install_dir}
      -DCPKT_STRIP_STATIC_ARCHIVES=${_strip_static_archives}
      ${_darwin_fixup_args}
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

  cpkt_get_external_c_flags(_cpkt_external_c_flags)
  list(APPEND _args -DCMAKE_C_FLAGS=${_cpkt_external_c_flags})

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
  cpkt_get_external_c_flags(nghttp2_cflags)
  list(APPEND nghttp2_env_args CFLAGS=${nghttp2_cflags})
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_LINKER)
    list(APPEND nghttp2_env_args
      PATH=${CPKT_OSXCROSS_BIN_DIR}:$ENV{PATH}
      LDFLAGS=-fuse-ld=${CMAKE_LINKER}
    )
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
    set(libssh2_platform_linker_flags "")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(libssh2_install_rpath "$ORIGIN")
    set(libssh2_platform_linker_flags "-DCMAKE_SHARED_LINKER_FLAGS=-Wl,--enable-new-dtags")
  else()
    set(libssh2_install_rpath "")
    set(libssh2_platform_linker_flags "")
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
        ${libssh2_platform_linker_flags}
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
    set(curl_platform_linker_flags "")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(curl_install_rpath "$ORIGIN")
    set(curl_platform_linker_flags "-DCMAKE_SHARED_LINKER_FLAGS=-Wl,--enable-new-dtags")
  else()
    set(curl_install_rpath "")
    set(curl_platform_linker_flags "")
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
        ${curl_platform_linker_flags}
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
