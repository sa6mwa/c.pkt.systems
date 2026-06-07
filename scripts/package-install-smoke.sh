#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

if [ "$#" -ne 3 ]; then
  printf 'usage: %s <archive> <target-id> <source-file>\n' "$0" >&2
  exit 2
fi

archive=$1
target_id=$2
source_file=$3
run_consumers=1
pkg_config_static_flag=-static
pkg_config_toolchain_flags=
cmake_generator="Unix Makefiles"
cmake_toolchain_file=
cmake_toolchain_args=()

case "$target_id" in
  x86_64-linux-gnu)
    cc=${CC:-/usr/bin/cc}
    run_prefix=
    static_extra_libs=
    ;;
  x86_64-linux-musl)
    cc=${CC:-/usr/bin/musl-gcc}
    run_prefix=
    static_extra_libs=
    ;;
  aarch64-linux-gnu)
    cc=${CC:-/usr/bin/aarch64-linux-gnu-gcc}
    run_prefix="/usr/bin/qemu-aarch64 -L /usr/aarch64-linux-gnu"
    static_extra_libs=
    ;;
  armhf-linux-gnu)
    cc=${CC:-/usr/bin/arm-linux-gnueabihf-gcc}
    run_prefix="/usr/bin/qemu-arm -L /usr/arm-linux-gnueabihf"
    static_extra_libs=-latomic
    ;;
  aarch64-linux-musl)
    musl_prefix=${CPKT_AARCH64_MUSL_PREFIX:-"$HOME/.local/cross/aarch64-linux-musl"}
    cc=${CC:-"$musl_prefix/bin/aarch64-linux-musl-gcc"}
    run_prefix="/usr/bin/qemu-aarch64 -L $musl_prefix/aarch64-linux-musl"
    static_extra_libs=
    ;;
  armhf-linux-musl)
    musl_prefix=${CPKT_ARMHF_MUSL_PREFIX:-"$HOME/.local/cross/arm-linux-musleabihf"}
    cc=${CC:-"$musl_prefix/bin/arm-linux-musleabihf-gcc"}
    run_prefix="/usr/bin/qemu-arm -L $musl_prefix/arm-linux-musleabihf"
    static_extra_libs=-latomic
    ;;
  arm64-apple-darwin)
    osxcross_root=${OSXCROSS_ROOT:-"$HOME/.local/cross/osxcross"}
    osxcross_host=${CPKT_OSXCROSS_HOST:-arm64-apple-darwin25}
    cc=${CC:-"$osxcross_root/bin/$osxcross_host-clang"}
    run_prefix=
    run_consumers=0
    static_extra_libs=
    pkg_config_static_flag=
    macos_deployment_target=${CPKT_MACOS_DEPLOYMENT_TARGET:-15.0}
    pkg_config_toolchain_flags="--ld-path=$osxcross_root/bin/$osxcross_host-ld -mmacosx-version-min=$macos_deployment_target"
    cmake_toolchain_file="$repo_root/cmake/toolchains/arm64-apple-darwin.cmake"
    cmake_toolchain_args=(
      "-DCPKT_OSXCROSS_ROOT=$osxcross_root"
      "-DCPKT_OSXCROSS_HOST=$osxcross_host"
      "-DCPKT_MACOS_DEPLOYMENT_TARGET=$macos_deployment_target"
    )
    ;;
  *)
    printf 'package install smoke only supports Linux and arm64-apple-darwin targets, got: %s\n' "$target_id" >&2
    exit 2
    ;;
esac

if [ ! -x "$cc" ]; then
  printf 'compiler for %s is not executable: %s\n' "$target_id" "$cc" >&2
  exit 1
fi

if [ "${CPKT_PACKAGE_INSTALL_SMOKE_PRINT_CMAKE_TOOLCHAIN_ARGS:-0}" = 1 ]; then
  printf '%s\n' -G
  printf '%s\n' "$cmake_generator"
  if [ -n "$cmake_toolchain_file" ]; then
    printf '%s\n' "-DCMAKE_TOOLCHAIN_FILE=$cmake_toolchain_file"
    printf '%s\n' "${cmake_toolchain_args[@]}"
  fi
  exit 0
fi

if [ ! -f "$archive" ]; then
  printf 'archive does not exist: %s\n' "$archive" >&2
  exit 1
fi
case "$archive" in
  /*) ;;
  *) archive=$(CDPATH= cd -- "$(dirname -- "$archive")" && pwd)/$(basename -- "$archive") ;;
esac

if [ ! -f "$source_file" ]; then
  printf 'source file does not exist: %s\n' "$source_file" >&2
  exit 1
fi
case "$source_file" in
  /*) ;;
  *) source_file=$(CDPATH= cd -- "$(dirname -- "$source_file")" && pwd)/$(basename -- "$source_file") ;;
esac

if ! command -v pkg-config >/dev/null 2>&1; then
  printf 'pkg-config is required for package metadata smoke tests\n' >&2
  exit 1
fi

cpkt_pkg_config() {
  PKG_CONFIG_PATH= PKG_CONFIG_LIBDIR="$pkg_config_libdir" pkg-config "$@"
}

if [ "${CPKT_PACKAGE_INSTALL_SMOKE_PRINT_PKG_CONFIG_WORDS:-0}" = 1 ]; then
  pkg_config_libdir=${CPKT_PACKAGE_INSTALL_SMOKE_PKG_CONFIG_LIBDIR:-}
  if [ -z "$pkg_config_libdir" ]; then
    printf 'CPKT_PACKAGE_INSTALL_SMOKE_PKG_CONFIG_LIBDIR is required when printing pkg-config words\n' >&2
    exit 2
  fi
  cpkt_pkg_config --cflags --libs "${CPKT_PACKAGE_INSTALL_SMOKE_PKG_CONFIG_NAME:-cpkt-smoke-isolation}"
  exit 0
fi

work_root=$(mktemp -d "${TMPDIR:-/tmp}/cpkt-install-smoke.XXXXXX")
trap 'rm -rf "$work_root"' EXIT

(cd "$work_root" && cmake -E tar xf "$archive")
prefix=$(find "$work_root" -mindepth 1 -maxdepth 1 -type d | head -n1)
if [ -z "$prefix" ]; then
  printf 'archive did not extract to a top-level prefix: %s\n' "$archive" >&2
  exit 1
fi

mkdir -p "$work_root/bin"

common_flags="-std=c89 -Wall -Wextra -Wpedantic -isystem $prefix/include"
assert_package_file() {
  package_path=$1
  if [ ! -f "$prefix/$package_path" ]; then
    printf 'package metadata file is missing: %s\n' "$package_path" >&2
    exit 1
  fi
}

assert_package_dir_absent() {
  package_path=$1
  if [ -e "$prefix/$package_path" ]; then
    printf 'package contains non-upstream metadata path: %s\n' "$package_path" >&2
    exit 1
  fi
}

assert_package_file "lib/cmake/OpenSSL/OpenSSLConfig.cmake"
assert_package_file "lib/cmake/OpenSSL/OpenSSLConfigVersion.cmake"
assert_package_file "lib/cmake/zlib/ZLIBConfig.cmake"
assert_package_file "lib/cmake/zlib/ZLIBConfigVersion.cmake"
assert_package_file "lib/cmake/nghttp2/nghttp2Config.cmake"
assert_package_file "lib/cmake/nghttp2/nghttp2ConfigVersion.cmake"
assert_package_file "lib/cmake/libssh2/libssh2-config.cmake"
assert_package_file "lib/cmake/libssh2/libssh2-config-version.cmake"
assert_package_file "lib/cmake/CURL/CURLConfig.cmake"
assert_package_file "lib/cmake/CURL/CURLConfigVersion.cmake"
assert_package_dir_absent "lib/cmake/ZLIB"
assert_package_dir_absent "lib/cmake/Libssh2"

cmake_source_dir="$work_root/cmake-consumer-src"
cmake_build_dir="$work_root/cmake-consumer-build"
mkdir -p "$cmake_source_dir" "$cmake_build_dir"
cp "$source_file" "$cmake_source_dir/cpkt_all.c"
cat > "$cmake_source_dir/cpkt_zlib.c" <<'EOF'
#include <zlib.h>

int main(void) {
  return zlibVersion() == 0;
}
EOF
cat > "$cmake_source_dir/cpkt_nghttp2.c" <<'EOF'
#include <nghttp2/nghttp2.h>

int main(void) {
  nghttp2_info *info = nghttp2_version(NGHTTP2_VERSION_NUM);
  return info == 0 || info->version_str == 0;
}
EOF
cat > "$cmake_source_dir/cpkt_crypto.c" <<'EOF'
#include <openssl/crypto.h>

int main(void) {
  return OpenSSL_version(OPENSSL_VERSION) == 0;
}
EOF
cat > "$cmake_source_dir/cpkt_ssl.c" <<'EOF'
#include <openssl/ssl.h>

int main(void) {
  SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
  if (ctx == 0) {
    return 1;
  }
  SSL_CTX_free(ctx);
  return 0;
}
EOF
cat > "$cmake_source_dir/cpkt_libssh2.c" <<'EOF'
#include <libssh2.h>

int main(void) {
  return libssh2_version(0) == 0;
}
EOF
cat > "$cmake_source_dir/cpkt_curl.c" <<'EOF'
#include <curl/curl.h>

int main(void) {
  return curl_version_info(CURLVERSION_NOW)->version == 0;
}
EOF
cat > "$cmake_source_dir/CMakeLists.txt" <<EOF
cmake_minimum_required(VERSION 3.21)
project(cpkt_package_cmake_static_smoke LANGUAGES C)
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(OpenSSL CONFIG REQUIRED)
find_package(ZLIB CONFIG REQUIRED)
find_package(nghttp2 CONFIG REQUIRED)
find_package(Libssh2 CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)

function(cpkt_add_static_smoke target_name source_name link_target)
  add_executable("\${target_name}" "\${source_name}")
  target_compile_options("\${target_name}" PRIVATE -Wall -Wextra -Wpedantic)
  target_link_libraries("\${target_name}" PRIVATE "\${link_target}")
endfunction()

cpkt_add_static_smoke(cpkt_cmake_zlib cpkt_zlib.c ZLIB::ZLIB)
cpkt_add_static_smoke(cpkt_cmake_nghttp2 cpkt_nghttp2.c nghttp2::nghttp2)
cpkt_add_static_smoke(cpkt_cmake_crypto cpkt_crypto.c OpenSSL::Crypto)
cpkt_add_static_smoke(cpkt_cmake_ssl cpkt_ssl.c OpenSSL::SSL)
cpkt_add_static_smoke(cpkt_cmake_libssh2 cpkt_libssh2.c Libssh2::libssh2)
cpkt_add_static_smoke(cpkt_cmake_curl cpkt_curl.c CURL::libcurl)
cpkt_add_static_smoke(cpkt_cmake_all cpkt_all.c CURL::libcurl)
EOF
cmake_args=(
  -G "$cmake_generator" \
  -S "$cmake_source_dir" \
  -B "$cmake_build_dir" \
  -DCMAKE_C_COMPILER="$cc" \
  -DCMAKE_PREFIX_PATH="$prefix" \
  -DOpenSSL_DIR="$prefix/lib/cmake/OpenSSL" \
  -DZLIB_DIR="$prefix/lib/cmake/zlib" \
  -Dnghttp2_DIR="$prefix/lib/cmake/nghttp2" \
  -DLibssh2_DIR="$prefix/lib/cmake/libssh2" \
  -DCURL_DIR="$prefix/lib/cmake/CURL" \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
)
if [ -n "$cmake_toolchain_file" ]; then
  cmake_args+=("-DCMAKE_TOOLCHAIN_FILE=$cmake_toolchain_file")
  cmake_args+=("${cmake_toolchain_args[@]}")
  cmake_args+=("-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH")
fi
cmake "${cmake_args[@]}"
cmake --build "$cmake_build_dir"

assert_file_contains() {
  file_path=$1
  expected=$2
  description=$3
  if ! grep -F -- "$expected" "$file_path" >/dev/null 2>&1; then
    printf '%s does not contain expected metadata-propagated item: %s\n' "$description" "$expected" >&2
    printf 'inspected file: %s\n' "$file_path" >&2
    exit 1
  fi
}

assert_words_contain() {
  words=$1
  expected=$2
  description=$3
  case " $words " in
    *" $expected "*) ;;
    *)
      printf '%s does not contain expected metadata-propagated item: %s\n' "$description" "$expected" >&2
      printf 'actual words: %s\n' "$words" >&2
      exit 1
      ;;
  esac
}

assert_words_not_contain() {
  words=$1
  unexpected=$2
  description=$3
  case " $words " in
    *" $unexpected "*)
      printf '%s contains unexpected metadata-propagated item: %s\n' "$description" "$unexpected" >&2
      printf 'actual words: %s\n' "$words" >&2
      exit 1
      ;;
  esac
}

cmake_link_dir="$cmake_build_dir/CMakeFiles"
assert_file_contains "$cmake_link_dir/cpkt_cmake_libssh2.dir/link.txt" "$prefix/lib/libcrypto.a" "Libssh2::libssh2 link line"
assert_file_contains "$cmake_link_dir/cpkt_cmake_libssh2.dir/link.txt" "$prefix/lib/libz.a" "Libssh2::libssh2 link line"
assert_file_contains "$cmake_link_dir/cpkt_cmake_curl.dir/link.txt" "$prefix/lib/libssh2.a" "CURL::libcurl link line"
assert_file_contains "$cmake_link_dir/cpkt_cmake_curl.dir/link.txt" "$prefix/lib/libnghttp2.a" "CURL::libcurl link line"
assert_file_contains "$cmake_link_dir/cpkt_cmake_curl.dir/link.txt" "$prefix/lib/libssl.a" "CURL::libcurl link line"
assert_file_contains "$cmake_link_dir/cpkt_cmake_curl.dir/link.txt" "$prefix/lib/libcrypto.a" "CURL::libcurl link line"
assert_file_contains "$cmake_link_dir/cpkt_cmake_curl.dir/link.txt" "$prefix/lib/libz.a" "CURL::libcurl link line"
case "$target_id" in
  *-linux-gnu)
    assert_file_contains "$cmake_link_dir/cpkt_cmake_crypto.dir/link.txt" "-ldl" "OpenSSL::Crypto link line"
    assert_file_contains "$cmake_link_dir/cpkt_cmake_curl.dir/link.txt" "-ldl" "CURL::libcurl link line"
    ;;
  arm64-apple-darwin)
    assert_file_contains "$cmake_link_dir/cpkt_cmake_curl.dir/link.txt" "CoreFoundation" "CURL::libcurl link line"
    assert_file_contains "$cmake_link_dir/cpkt_cmake_curl.dir/link.txt" "SystemConfiguration" "CURL::libcurl link line"
    ;;
esac

cpkt_cmake_direct_dir_smoke() {
  package_name=$1
  package_dir=$2
  executable_name=$3
  source_name=$4
  link_target=$5
  direct_source_dir="$work_root/cmake-direct-$package_name-src"
  direct_build_dir="$work_root/cmake-direct-$package_name-build"
  mkdir -p "$direct_source_dir" "$direct_build_dir"
  cp "$cmake_source_dir/$source_name" "$direct_source_dir/$source_name"
  cat > "$direct_source_dir/CMakeLists.txt" <<EOF
cmake_minimum_required(VERSION 3.21)
project(cpkt_package_cmake_direct_${package_name}_smoke LANGUAGES C)
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package($package_name CONFIG REQUIRED)

add_executable($executable_name "$source_name")
target_compile_options($executable_name PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries($executable_name PRIVATE "$link_target")
EOF
  direct_cmake_args=(
    -G "$cmake_generator" \
    -S "$direct_source_dir" \
    -B "$direct_build_dir" \
    -DCMAKE_C_COMPILER="$cc" \
    "-D${package_name}_DIR=$package_dir" \
    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
  )
  if [ -n "$cmake_toolchain_file" ]; then
    direct_cmake_args+=("-DCMAKE_TOOLCHAIN_FILE=$cmake_toolchain_file")
    direct_cmake_args+=("${cmake_toolchain_args[@]}")
    direct_cmake_args+=("-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH")
  fi
  cmake "${direct_cmake_args[@]}"
  cmake --build "$direct_build_dir"
}

cpkt_cmake_direct_dir_smoke Libssh2 "$prefix/lib/cmake/libssh2" cpkt_direct_libssh2 cpkt_libssh2.c Libssh2::libssh2
direct_libssh2_link_dir="$work_root/cmake-direct-Libssh2-build/CMakeFiles/cpkt_direct_libssh2.dir"
assert_file_contains "$direct_libssh2_link_dir/link.txt" "$prefix/lib/libcrypto.a" "direct Libssh2_DIR link line"
assert_file_contains "$direct_libssh2_link_dir/link.txt" "$prefix/lib/libz.a" "direct Libssh2_DIR link line"

cpkt_cmake_direct_dir_smoke CURL "$prefix/lib/cmake/CURL" cpkt_direct_curl cpkt_curl.c CURL::libcurl
direct_curl_link_dir="$work_root/cmake-direct-CURL-build/CMakeFiles/cpkt_direct_curl.dir"
assert_file_contains "$direct_curl_link_dir/link.txt" "$prefix/lib/libssh2.a" "direct CURL_DIR link line"
assert_file_contains "$direct_curl_link_dir/link.txt" "$prefix/lib/libnghttp2.a" "direct CURL_DIR link line"
assert_file_contains "$direct_curl_link_dir/link.txt" "$prefix/lib/libssl.a" "direct CURL_DIR link line"
assert_file_contains "$direct_curl_link_dir/link.txt" "$prefix/lib/libcrypto.a" "direct CURL_DIR link line"
assert_file_contains "$direct_curl_link_dir/link.txt" "$prefix/lib/libz.a" "direct CURL_DIR link line"

example_cmake_build_dir="$work_root/example-cmake-consumer-build"
example_cmake_args=(
  -G "$cmake_generator" \
  -S "$repo_root/examples/cmake-consumer" \
  -B "$example_cmake_build_dir" \
  -DCMAKE_C_COMPILER="$cc" \
  -DCMAKE_PREFIX_PATH="$prefix" \
  -DCURL_DIR="$prefix/lib/cmake/CURL" \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
)
if [ -n "$cmake_toolchain_file" ]; then
  example_cmake_args+=("-DCMAKE_TOOLCHAIN_FILE=$cmake_toolchain_file")
  example_cmake_args+=("${cmake_toolchain_args[@]}")
  example_cmake_args+=("-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH")
fi
cmake "${example_cmake_args[@]}"
cmake --build "$example_cmake_build_dir"

pkg_config_libdir="$prefix/lib/pkgconfig"
pkg_config_words() {
  cpkt_pkg_config --static --cflags --libs "$1"
}

pkg_config_default_words() {
  cpkt_pkg_config --cflags --libs "$1"
}

libcrypto_words=$(pkg_config_words libcrypto)
libssl_words=$(pkg_config_words libssl)
openssl_words=$(pkg_config_words openssl)
zlib_words=$(pkg_config_words zlib)
nghttp2_words=$(pkg_config_words libnghttp2)
libssh2_words=$(pkg_config_words libssh2)
libcurl_words=$(pkg_config_words libcurl)
openssl_default_words=$(pkg_config_default_words openssl)

case "$target_id" in
  *-linux-gnu)
    assert_words_contain "$libcrypto_words" "-ldl" "libcrypto.pc --static output"
    assert_words_contain "$libssl_words" "-ldl" "libssl.pc --static output"
    assert_words_contain "$openssl_words" "-ldl" "openssl.pc --static output"
    assert_words_contain "$libcurl_words" "-ldl" "libcurl.pc --static output"
    ;;
  arm64-apple-darwin)
    assert_words_contain "$libcurl_words" "CoreFoundation" "libcurl.pc --static output"
    assert_words_contain "$libcurl_words" "SystemConfiguration" "libcurl.pc --static output"
    assert_words_not_contain "$libcrypto_words" "-ldl" "libcrypto.pc --static output"
    assert_words_not_contain "$libssl_words" "-ldl" "libssl.pc --static output"
    assert_words_not_contain "$openssl_words" "-ldl" "openssl.pc --static output"
    assert_words_not_contain "$libcurl_words" "-ldl" "libcurl.pc --static output"
    ;;
esac
case "$target_id" in
  armhf-*)
    assert_words_contain "$libcrypto_words" "-latomic" "libcrypto.pc --static output"
    assert_words_contain "$libcurl_words" "-latomic" "libcurl.pc --static output"
    ;;
esac
assert_words_contain "$libssl_words" "-lcrypto" "libssl.pc --static output"
assert_words_contain "$openssl_words" "-lssl" "openssl.pc --static output"
assert_words_contain "$openssl_words" "-lcrypto" "openssl.pc --static output"
assert_words_contain "$openssl_default_words" "-lssl" "openssl.pc output"
assert_words_contain "$openssl_default_words" "-lcrypto" "openssl.pc output"
assert_words_contain "$libssh2_words" "-lcrypto" "libssh2.pc --static output"
assert_words_contain "$libssh2_words" "-lz" "libssh2.pc --static output"
assert_words_contain "$libcurl_words" "-lssh2" "libcurl.pc --static output"
assert_words_contain "$libcurl_words" "-lnghttp2" "libcurl.pc --static output"
assert_words_contain "$libcurl_words" "-lssl" "libcurl.pc --static output"
assert_words_contain "$libcurl_words" "-lcrypto" "libcurl.pc --static output"
assert_words_contain "$libcurl_words" "-lz" "libcurl.pc --static output"

cpkt_pkg_config_static_smoke() {
  pc_name=$1
  source_name=$2
  output_path="$work_root/bin/cpkt_pkg_${pc_name}"
  "$cc" $pkg_config_static_flag $pkg_config_toolchain_flags $common_flags "$cmake_source_dir/$source_name" \
    -o "$output_path" \
    $(cpkt_pkg_config --static --cflags --libs "$pc_name") \
    $static_extra_libs
}

cpkt_pkg_config_static_smoke zlib cpkt_zlib.c
cpkt_pkg_config_static_smoke libnghttp2 cpkt_nghttp2.c
cpkt_pkg_config_static_smoke libcrypto cpkt_crypto.c
cpkt_pkg_config_static_smoke libssl cpkt_ssl.c
cpkt_pkg_config_static_smoke openssl cpkt_ssl.c
cpkt_pkg_config_static_smoke libssh2 cpkt_libssh2.c
cpkt_pkg_config_static_smoke libcurl cpkt_curl.c

example_pkg_config_output="$work_root/bin/cpkt_example_pkg_config_consumer"
CPKT_SDK_PREFIX="$prefix" \
CC="$cc" \
CPKT_EXAMPLE_CFLAGS="$pkg_config_static_flag $pkg_config_toolchain_flags $common_flags" \
CPKT_EXAMPLE_LDFLAGS="$static_extra_libs" \
  "$repo_root/examples/pkg-config-consumer/build.sh" "$example_pkg_config_output"

cpkt_pkg_config_smoke() {
  pc_name=$1
  source_name=$2
  output_path="$work_root/bin/cpkt_pkg_${pc_name}_default"
  "$cc" $pkg_config_toolchain_flags $common_flags "$cmake_source_dir/$source_name" \
    -o "$output_path" \
    $(cpkt_pkg_config --cflags --libs "$pc_name")
}

cpkt_pkg_config_smoke openssl cpkt_ssl.c

if [ "$run_consumers" -eq 0 ]; then
  exit 0
fi

if [ -z "$run_prefix" ]; then
  "$cmake_build_dir/cpkt_cmake_zlib"
  "$cmake_build_dir/cpkt_cmake_nghttp2"
  "$cmake_build_dir/cpkt_cmake_crypto"
  "$cmake_build_dir/cpkt_cmake_ssl"
  "$cmake_build_dir/cpkt_cmake_libssh2"
  "$cmake_build_dir/cpkt_cmake_curl"
  "$cmake_build_dir/cpkt_cmake_all"
  "$work_root/bin/cpkt_pkg_zlib"
  "$work_root/bin/cpkt_pkg_libnghttp2"
  "$work_root/bin/cpkt_pkg_libcrypto"
  "$work_root/bin/cpkt_pkg_libssl"
  "$work_root/bin/cpkt_pkg_openssl"
  "$work_root/bin/cpkt_pkg_libssh2"
  "$work_root/bin/cpkt_pkg_libcurl"
  "$example_cmake_build_dir/cpkt_bundle_cmake_consumer"
  "$example_pkg_config_output"
else
  # shellcheck disable=SC2086
  $run_prefix "$cmake_build_dir/cpkt_cmake_zlib"
  # shellcheck disable=SC2086
  $run_prefix "$cmake_build_dir/cpkt_cmake_nghttp2"
  # shellcheck disable=SC2086
  $run_prefix "$cmake_build_dir/cpkt_cmake_crypto"
  # shellcheck disable=SC2086
  $run_prefix "$cmake_build_dir/cpkt_cmake_ssl"
  # shellcheck disable=SC2086
  $run_prefix "$cmake_build_dir/cpkt_cmake_libssh2"
  # shellcheck disable=SC2086
  $run_prefix "$cmake_build_dir/cpkt_cmake_curl"
  # shellcheck disable=SC2086
  $run_prefix "$cmake_build_dir/cpkt_cmake_all"
  # shellcheck disable=SC2086
  $run_prefix "$work_root/bin/cpkt_pkg_zlib"
  # shellcheck disable=SC2086
  $run_prefix "$work_root/bin/cpkt_pkg_libnghttp2"
  # shellcheck disable=SC2086
  $run_prefix "$work_root/bin/cpkt_pkg_libcrypto"
  # shellcheck disable=SC2086
  $run_prefix "$work_root/bin/cpkt_pkg_libssl"
  # shellcheck disable=SC2086
  $run_prefix "$work_root/bin/cpkt_pkg_openssl"
  # shellcheck disable=SC2086
  $run_prefix "$work_root/bin/cpkt_pkg_libssh2"
  # shellcheck disable=SC2086
  $run_prefix "$work_root/bin/cpkt_pkg_libcurl"
  # shellcheck disable=SC2086
  $run_prefix "$example_cmake_build_dir/cpkt_bundle_cmake_consumer"
  # shellcheck disable=SC2086
  $run_prefix "$example_pkg_config_output"
fi
