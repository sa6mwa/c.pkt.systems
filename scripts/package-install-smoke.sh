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
pkg_config_compile_toolchain_flags=
pkg_config_link_toolchain_flags=
example_runtime_ldflags=
cmake_generator="Unix Makefiles"
cmake_toolchain_file=
cmake_toolchain_args=()

cpkt_infer_cxx_from_cc() {
  case "$1" in
    *gcc) printf '%sg++\n' "${1%gcc}" ;;
    *cc)
      if [ -x "${1%cc}c++" ]; then
        printf '%sc++\n' "${1%cc}"
      else
        printf 'c++\n'
      fi
      ;;
    *clang) printf '%sclang++\n' "${1%clang}" ;;
    *) printf 'c++\n' ;;
  esac
}

cpkt_ensure_toolchain() {
  "$repo_root/scripts/ensure-toolchain.sh" "$1"
}

cpkt_toolchain_complete() {
  toolchain_root=$1
  toolchain_prefix=$2
  toolchain_sysroot=$3

  [ -x "$toolchain_root/bin/$toolchain_prefix-gcc" ] &&
    [ -x "$toolchain_root/bin/$toolchain_prefix-g++" ] &&
    [ -x "$toolchain_root/bin/$toolchain_prefix-ar" ] &&
    [ -x "$toolchain_root/bin/$toolchain_prefix-ranlib" ] &&
    [ -f "$toolchain_sysroot/include/stdio.h" ]
}

case "$target_id" in
  x86_64-linux-gnu)
    cc=${CC:-/usr/bin/cc}
    run_prefix=
    pkg_config_static_flag=
    static_extra_libs=
    ;;
  x86_64-linux-musl)
    if [ -n "${CPKT_X86_64_MUSL_PREFIX:-}" ] &&
        cpkt_toolchain_complete "$CPKT_X86_64_MUSL_PREFIX" x86_64-linux-musl "$CPKT_X86_64_MUSL_PREFIX/x86_64-linux-musl"; then
      toolchain_root=$CPKT_X86_64_MUSL_PREFIX
      cc=${CC:-"$toolchain_root/bin/x86_64-linux-musl-gcc"}
    else
      toolchain_root=$(cpkt_ensure_toolchain x86_64-linux-musl)
      cc=${CC:-"$toolchain_root/bin/x86_64-linux-gcc"}
    fi
    run_prefix=
    cmake_toolchain_file="$repo_root/cmake/toolchains/x86_64-linux-musl.cmake"
    static_extra_libs=
    ;;
  aarch64-linux-gnu)
    if [ -n "${CPKT_AARCH64_GNU_PREFIX:-}" ] &&
        cpkt_toolchain_complete "$CPKT_AARCH64_GNU_PREFIX" aarch64-linux-gnu "$CPKT_AARCH64_GNU_PREFIX/aarch64-linux-gnu"; then
      toolchain_root=$CPKT_AARCH64_GNU_PREFIX
      cc=${CC:-"$toolchain_root/bin/aarch64-linux-gnu-gcc"}
      run_prefix="/usr/bin/qemu-aarch64 -L $toolchain_root/aarch64-linux-gnu"
    elif cpkt_toolchain_complete /usr aarch64-linux-gnu /usr/aarch64-linux-gnu; then
      toolchain_root=/usr
      cc=${CC:-/usr/bin/aarch64-linux-gnu-gcc}
      run_prefix="/usr/bin/qemu-aarch64 -L /usr/aarch64-linux-gnu"
    else
      toolchain_root=$(cpkt_ensure_toolchain aarch64-linux-gnu)
      cc=${CC:-"$toolchain_root/bin/aarch64-linux-gcc"}
      run_prefix="/usr/bin/qemu-aarch64 -L $toolchain_root/aarch64-buildroot-linux-gnu/sysroot"
    fi
    cmake_toolchain_file="$repo_root/cmake/toolchains/aarch64-linux-gnu.cmake"
    pkg_config_static_flag=
    static_extra_libs=
    ;;
  armhf-linux-gnu)
    if [ -n "${CPKT_ARMHF_GNU_PREFIX:-}" ] &&
        cpkt_toolchain_complete "$CPKT_ARMHF_GNU_PREFIX" arm-linux-gnueabihf "$CPKT_ARMHF_GNU_PREFIX/arm-linux-gnueabihf"; then
      toolchain_root=$CPKT_ARMHF_GNU_PREFIX
      cc=${CC:-"$toolchain_root/bin/arm-linux-gnueabihf-gcc"}
      run_prefix="/usr/bin/qemu-arm -L $toolchain_root/arm-linux-gnueabihf"
    elif cpkt_toolchain_complete /usr arm-linux-gnueabihf /usr/arm-linux-gnueabihf; then
      toolchain_root=/usr
      cc=${CC:-/usr/bin/arm-linux-gnueabihf-gcc}
      run_prefix="/usr/bin/qemu-arm -L /usr/arm-linux-gnueabihf"
    else
      toolchain_root=$(cpkt_ensure_toolchain armhf-linux-gnu)
      cc=${CC:-"$toolchain_root/bin/arm-linux-gcc"}
      run_prefix="/usr/bin/qemu-arm -L $toolchain_root/arm-buildroot-linux-gnueabihf/sysroot"
    fi
    cmake_toolchain_file="$repo_root/cmake/toolchains/armhf-linux-gnu.cmake"
    pkg_config_static_flag=
    static_extra_libs=-latomic
    ;;
  aarch64-linux-musl)
    if [ -n "${CPKT_AARCH64_MUSL_PREFIX:-}" ]; then
      toolchain_root=$CPKT_AARCH64_MUSL_PREFIX
      cc=${CC:-"$toolchain_root/bin/aarch64-linux-musl-gcc"}
      run_prefix="/usr/bin/qemu-aarch64 -L $toolchain_root/aarch64-linux-musl"
    elif [ -n "${HOME:-}" ] && [ -x "$HOME/.local/cross/aarch64-linux-musl/bin/aarch64-linux-musl-gcc" ]; then
      toolchain_root=$HOME/.local/cross/aarch64-linux-musl
      cc=${CC:-"$toolchain_root/bin/aarch64-linux-musl-gcc"}
      run_prefix="/usr/bin/qemu-aarch64 -L $toolchain_root/aarch64-linux-musl"
    else
      toolchain_root=$(cpkt_ensure_toolchain aarch64-linux-musl)
      cc=${CC:-"$toolchain_root/bin/aarch64-linux-gcc"}
      run_prefix="/usr/bin/qemu-aarch64 -L $toolchain_root/aarch64-buildroot-linux-musl/sysroot"
    fi
    cmake_toolchain_file="$repo_root/cmake/toolchains/aarch64-linux-musl.cmake"
    static_extra_libs=
    ;;
  armhf-linux-musl)
    if [ -n "${CPKT_ARMHF_MUSL_PREFIX:-}" ]; then
      toolchain_root=$CPKT_ARMHF_MUSL_PREFIX
      cc=${CC:-"$toolchain_root/bin/arm-linux-musleabihf-gcc"}
      run_prefix="/usr/bin/qemu-arm -L $toolchain_root/arm-linux-musleabihf"
    elif [ -n "${HOME:-}" ] && [ -x "$HOME/.local/cross/arm-linux-musleabihf/bin/arm-linux-musleabihf-gcc" ]; then
      toolchain_root=$HOME/.local/cross/arm-linux-musleabihf
      cc=${CC:-"$toolchain_root/bin/arm-linux-musleabihf-gcc"}
      run_prefix="/usr/bin/qemu-arm -L $toolchain_root/arm-linux-musleabihf"
    else
      toolchain_root=$(cpkt_ensure_toolchain armhf-linux-musl)
      cc=${CC:-"$toolchain_root/bin/arm-linux-gcc"}
      run_prefix="/usr/bin/qemu-arm -L $toolchain_root/arm-buildroot-linux-musleabihf/sysroot"
    fi
    cmake_toolchain_file="$repo_root/cmake/toolchains/armhf-linux-musl.cmake"
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
    pkg_config_toolchain_flags="-mmacosx-version-min=$macos_deployment_target"
    pkg_config_compile_toolchain_flags="$pkg_config_toolchain_flags"
    pkg_config_link_toolchain_flags="--ld-path=$osxcross_root/bin/$osxcross_host-ld $pkg_config_toolchain_flags"
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
cxx=${CXX:-$(cpkt_infer_cxx_from_cc "$cc")}
case "$target_id" in
  *-linux-*)
    if [ ! -x "$cxx" ]; then
      printf 'C++ compiler for %s is not executable: %s\n' "$target_id" "$cxx" >&2
      exit 1
    fi
    ;;
esac

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
diagnostic_dir="$work_root/diagnostics"
mkdir -p "$diagnostic_dir"

cpkt_safe_log_name() {
  printf '%s\n' "$1" | tr -c 'A-Za-z0-9_.-' '_'
}

cpkt_run_checked() {
  description=$1
  shift
  log_file="$diagnostic_dir/$(cpkt_safe_log_name "$description").log"

  if ! "$@" >"$log_file" 2>&1; then
    printf '%s failed\n' "$description" >&2
    cat "$log_file" >&2
    exit 1
  fi
  if grep -E '(^|[[:space:]:])warning:' "$log_file" >/dev/null 2>&1; then
    printf '%s emitted warnings\n' "$description" >&2
    cat "$log_file" >&2
    exit 1
  fi
  cat "$log_file"
}

cpkt_run_shell_checked() {
  description=$1
  command_text=$2
  log_file="$diagnostic_dir/$(cpkt_safe_log_name "$description").log"

  if ! sh -c "$command_text" >"$log_file" 2>&1; then
    printf '%s failed\n' "$description" >&2
    cat "$log_file" >&2
    exit 1
  fi
  if grep -E '(^|[[:space:]:])warning:' "$log_file" >/dev/null 2>&1; then
    printf '%s emitted warnings\n' "$description" >&2
    cat "$log_file" >&2
    exit 1
  fi
  cat "$log_file"
}

(cd "$work_root" && cmake -E tar xf "$archive")
prefix_count=0
prefix=
for candidate in "$work_root"/*; do
  if [ ! -d "$candidate" ] || [ "$candidate" = "$diagnostic_dir" ]; then
    continue
  fi
  prefix_count=$((prefix_count + 1))
  prefix=$candidate
done
if [ "$prefix_count" -ne 1 ]; then
  printf 'archive must extract to exactly one top-level SDK prefix, found %s: %s\n' "$prefix_count" "$archive" >&2
  exit 1
fi
if [ "${CPKT_PACKAGE_INSTALL_SMOKE_PRINT_EXTRACTED_PREFIX:-0}" = 1 ]; then
  basename -- "$prefix"
  exit 0
fi
case "$target_id" in
  *-linux-gnu)
    example_runtime_ldflags="-Wl,-rpath,$prefix/lib"
    ;;
esac

mkdir -p "$work_root/bin"

common_flags="-std=c99 -Wall -Wextra -Wpedantic -isystem $prefix/include"
common_c89_flags="-std=c89 -Wall -Wextra -Wpedantic -isystem $prefix/include"
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

assert_file_not_contains() {
  file_path=$1
  unexpected=$2
  description=$3
  if grep -F -- "$unexpected" "$file_path" >/dev/null 2>&1; then
    printf '%s contains unexpected metadata-propagated item: %s\n' "$description" "$unexpected" >&2
    printf 'inspected file: %s\n' "$file_path" >&2
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
assert_package_file "lib/cmake/libxml2/libxml2-config.cmake"
assert_package_file "lib/cmake/libxml2/libxml2-config-version.cmake"
assert_package_file "lib/cmake/Lua/LuaConfig.cmake"
assert_package_file "lib/cmake/Lua/LuaConfigVersion.cmake"
assert_package_file "lib/cmake/mqtt-c/mqtt-cConfig.cmake"
assert_package_file "lib/cmake/mqtt-c/mqtt-cConfigVersion.cmake"
assert_package_file "lib/cmake/CpktLuaRuntime/CpktLuaRuntimeConfig.cmake"
assert_package_file "lib/cmake/CpktLuaRuntime/CpktLuaRuntimeConfigVersion.cmake"
assert_package_file "lib/cmake/CpktOpcUa/CpktOpcUaConfig.cmake"
assert_package_file "lib/cmake/CpktOpcUa/CpktOpcUaConfigVersion.cmake"
assert_package_file "lib/cmake/open62541/open62541Config.cmake"
assert_package_file "lib/cmake/open62541/open62541ConfigVersion.cmake"
assert_package_file "share/c.pkt.systems/manifest.txt"
assert_package_file "share/c.pkt.systems/sus-model-catalog.tsv"
assert_package_file "share/doc/c.pkt.systems/LICENSE"
assert_package_file "share/doc/c.pkt.systems/README.md"
assert_package_file "share/doc/c.pkt.systems/docs/audio-sus-facade-spec.md"
assert_package_file "share/doc/c.pkt.systems/docs/opcua-c89-facade-spec.md"
assert_package_file "share/doc/c.pkt.systems/docs/sus-model-catalog.tsv"
assert_package_file "share/doc/c.pkt.systems/examples/abi_smoke.c"
assert_package_file "share/doc/c.pkt.systems/examples/audio-sus-c89/CMakeLists.txt"
assert_package_file "share/doc/c.pkt.systems/examples/audio-sus-c89/build-pkg-config.sh"
assert_package_file "share/doc/c.pkt.systems/examples/audio-sus-c89/main.c"
assert_package_file "share/doc/c.pkt.systems/examples/audio-vox-intro-c89/CMakeLists.txt"
assert_package_file "share/doc/c.pkt.systems/examples/audio-vox-intro-c89/build-pkg-config.sh"
assert_package_file "share/doc/c.pkt.systems/examples/audio-vox-intro-c89/main.c"
assert_package_file "share/doc/c.pkt.systems/examples/sus-vox-intro-c89/CMakeLists.txt"
assert_package_file "share/doc/c.pkt.systems/examples/sus-vox-intro-c89/build-pkg-config.sh"
assert_package_file "share/doc/c.pkt.systems/examples/sus-vox-intro-c89/main.c"
assert_package_file "share/doc/c.pkt.systems/examples/cmake-consumer/CMakeLists.txt"
assert_package_file "share/doc/c.pkt.systems/examples/lua-runtime-c89/CMakeLists.txt"
assert_package_file "share/doc/c.pkt.systems/examples/lua-runtime-c89/build-pkg-config.sh"
assert_package_file "share/doc/c.pkt.systems/examples/lua-runtime-c89/host_module.c"
assert_package_file "share/doc/c.pkt.systems/examples/lua-runtime-c89/main.c"
assert_package_file "share/doc/c.pkt.systems/examples/mqttc_smoke.c"
assert_package_file "share/doc/c.pkt.systems/examples/opcua-c89/CMakeLists.txt"
assert_package_file "share/doc/c.pkt.systems/examples/opcua-c89/build-pkg-config.sh"
assert_package_file "share/doc/c.pkt.systems/examples/opcua-c89/main.c"
assert_package_file "share/doc/c.pkt.systems/examples/pkg-config-consumer/build.sh"
assert_package_file "share/doc/c.pkt.systems/third_party/kblab-whisper-models/LICENSE"
assert_package_file "share/doc/c.pkt.systems/third_party/kblab-whisper-models/PROVENANCE.md"
assert_file_contains "$prefix/lib/cmake/libxml2/libxml2-config.cmake" "find_dependency(Iconv REQUIRED)" "libxml2 CMake config"
assert_file_contains "$prefix/lib/cmake/libxml2/libxml2-config.cmake" "Iconv::Iconv" "libxml2 CMake config"
assert_file_contains "$prefix/lib/cmake/open62541/open62541Config.cmake" "find_dependency(OpenSSL CONFIG REQUIRED)" "open62541 CMake config"
assert_file_contains "$prefix/lib/cmake/mqtt-c/mqtt-cConfig.cmake" "find_dependency(Threads REQUIRED)" "mqtt-c CMake config"
assert_file_contains "$prefix/share/c.pkt.systems/manifest.txt" "sus_backend_capabilities=cpu" "package manifest"
assert_file_contains "$prefix/share/c.pkt.systems/sus-model-catalog.tsv" "kb-whisper-small" "sus model catalog metadata"
assert_file_contains "$prefix/share/c.pkt.systems/sus-model-catalog.tsv" "large-v3-turbo:q5_0" "sus model catalog metadata"
assert_file_contains "$prefix/share/doc/c.pkt.systems/third_party/kblab-whisper-models/PROVENANCE.md" "KBLab/kb-whisper-*" "KBLab model provenance"
assert_package_dir_absent "lib/cmake/ZLIB"
assert_package_dir_absent "lib/cmake/Libssh2"

if grep -E 'lua\.h|lauxlib\.h|lualib\.h|lua_State|lua_Integer|lua_Number|lua_Unsigned|long long|inline' \
    "$prefix/include/cpkt/lua_runtime.h" >/dev/null 2>&1; then
  printf 'Lua runtime facade header is not C89-clean\n' >&2
  exit 1
fi
if grep -E 'open62541/|UA_Client|UA_Server|UA_StatusCode|UA_NodeId|UA_Variant|stdint\.h|stdbool\.h|uint8_t|uint16_t|uint32_t|uint64_t|int8_t|int16_t|int32_t|int64_t|long long|inline' \
    "$prefix/include/cpkt/opcua.h" >/dev/null 2>&1; then
  printf 'OPC UA facade header is not C89-clean\n' >&2
  exit 1
fi
installed_examples_dir="$prefix/share/doc/c.pkt.systems/examples"

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
cat > "$cmake_source_dir/cpkt_libxml2.c" <<'EOF'
#include <libxml/parser.h>

int main(void) {
  xmlDocPtr doc = xmlReadMemory("<root/>", 7, "memory.xml", 0, 0);
  if (doc == 0) {
    return 1;
  }
  xmlFreeDoc(doc);
  xmlCleanupParser();
  return 0;
}
EOF
cat > "$cmake_source_dir/cpkt_lua.c" <<'EOF'
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

int main(void) {
  lua_State *state = luaL_newstate();
  if (state == 0) {
    return 1;
  }
  luaL_openlibs(state);
  lua_close(state);
  return 0;
}
EOF
cat > "$cmake_source_dir/cpkt_open62541.c" <<'EOF'
#include <open62541/client.h>
#include <open62541/server.h>

int main(void) {
  UA_Client *client = UA_Client_new();
  UA_Server *server;

  if (client == 0) {
    return 1;
  }
  UA_Client_delete(client);

  server = UA_Server_new();
  if (server == 0) {
    return 2;
  }
  UA_Server_delete(server);
  return 0;
}
EOF
cat > "$cmake_source_dir/cpkt_mqttc.c" <<'EOF'
#include <mqtt.h>

int main(void) {
  const char *message = mqtt_error_str(MQTT_ERROR_NULLPTR);
  return message == 0 || message[0] == '\0';
}
EOF
cat > "$cmake_source_dir/cpkt_sus_facade_strict.c" <<'EOF'
#include <cpkt/sus.h>

#include <string.h>

int main(void) {
  cpkt_sus_model_entry entry;
  cpkt_sus_model *model;
  cpkt_sus_model_config config;
  cpkt_sus_realtime_config realtime_config;
  cpkt_sus_realtime_event event;

  if (cpkt_sus_backend_version() == 0 ||
      cpkt_sus_backend_system_info() == 0 ||
      cpkt_sus_backend_capabilities() == 0 ||
      cpkt_sus_facade_version() == 0) {
    return 1;
  }
  if (strcmp(cpkt_sus_backend_capabilities(), "cpu") != 0) {
    return 2;
  }
  if (cpkt_sus_result_string(CPKT_SUS_ERR_MODEL) == 0) {
    return 3;
  }
  if (cpkt_sus_result_string(CPKT_SUS_ABORTED) == 0) {
    return 11;
  }
  if (cpkt_sus_model_catalog_count() == 0) {
    return 4;
  }
  if (cpkt_sus_model_catalog_default(&entry) != CPKT_SUS_OK) {
    return 5;
  }
  if (entry.name == 0 || strcmp(entry.name, "small") != 0) {
    return 6;
  }
  if (cpkt_sus_model_catalog_find("kb-whisper-small", &entry) != CPKT_SUS_OK) {
    return 7;
  }
  if (entry.provider == 0 || strcmp(entry.provider, "KBLab/kb-whisper-small") != 0) {
    return 8;
  }
  memset(&config, 0, sizeof(config));
  memset(&realtime_config, 0, sizeof(realtime_config));
  memset(&event, 0, sizeof(event));
  realtime_config.step_ms = 1000UL;
  realtime_config.length_ms = 7000UL;
  realtime_config.keep_ms = 1500UL;
  realtime_config.memory_spool_bytes = 1024UL * 1024UL;
  realtime_config.max_spool_bytes = 1024UL * 1024UL * 1024UL;
  event.is_final = 1;
  if (realtime_config.step_ms != 1000UL || event.is_final == 0) {
    return 12;
  }
  if (sizeof(((cpkt_sus_transcriber *)0)->transcribe_audio_decoder_realtime_text) == 0 ||
      sizeof(((cpkt_sus_transcriber *)0)->revised_text) == 0) {
    return 13;
  }
  config.model_path = "";
  model = (cpkt_sus_model *)1;
  if (cpkt_sus_model_open_path(&model, &config) != CPKT_SUS_ERR_ARG) {
    return 9;
  }
  if (model != 0) {
    return 10;
  }
  cpkt_sus_string_free(0);
  return 0;
}
EOF
cat > "$cmake_source_dir/cpkt_audio_facade_strict.c" <<'EOF'
#include <cpkt/audio.h>

int main(void) {
  cpkt_audio_decoder *decoder;

  if (cpkt_audio_format_can_decode(CPKT_AUDIO_FORMAT_MP3) == 0) {
    return 1;
  }
  if (cpkt_audio_format_can_encode(CPKT_AUDIO_FORMAT_WAV) == 0) {
    return 2;
  }
  decoder = (cpkt_audio_decoder *)1;
  if (cpkt_audio_decoder_open_url(&decoder, "", 0) != CPKT_AUDIO_ERR_ARG) {
    return 3;
  }
  if (decoder != 0) {
    return 4;
  }
  return 0;
}
EOF
cat > "$cmake_source_dir/cpkt_audio_sus_facade_strict.c" <<'EOF'
#include <cpkt/audio.h>
#include <cpkt/sus.h>

#include <string.h>

int main(void) {
  cpkt_audio_decoder *decoder;
  cpkt_sus_model *model;
  cpkt_sus_model_config model_config;
  cpkt_sus_realtime_config realtime_config;
  cpkt_sus_realtime_event event;

  if (cpkt_audio_format_can_decode(CPKT_AUDIO_FORMAT_MP3) == 0) {
    return 1;
  }
  decoder = (cpkt_audio_decoder *)1;
  if (cpkt_audio_decoder_open_url(&decoder, "", 0) != CPKT_AUDIO_ERR_ARG) {
    return 2;
  }
  if (decoder != 0) {
    return 3;
  }
  if (cpkt_sus_backend_version() == 0 ||
      cpkt_sus_backend_capabilities() == 0) {
    return 4;
  }
  if (strcmp(cpkt_sus_backend_capabilities(), "cpu") != 0) {
    return 5;
  }
  memset(&model_config, 0, sizeof(model_config));
  memset(&realtime_config, 0, sizeof(realtime_config));
  memset(&event, 0, sizeof(event));
  realtime_config.step_ms = 1000UL;
  realtime_config.length_ms = 7000UL;
  realtime_config.keep_ms = 1500UL;
  realtime_config.memory_spool_bytes = 1024UL * 1024UL;
  realtime_config.max_spool_bytes = 1024UL * 1024UL * 1024UL;
  event.step_index = 1UL;
  if (sizeof(((cpkt_sus_transcriber *)0)->transcribe_audio_decoder_realtime) == 0 ||
      sizeof(((cpkt_sus_transcriber *)0)->transcribe_audio_decoder_realtime_text) == 0 ||
      sizeof(((cpkt_sus_transcriber *)0)->revised_text) == 0) {
    return 8;
  }
  if (event.step_index != 1UL) {
    return 9;
  }
  model_config.model_path = "";
  model = (cpkt_sus_model *)1;
  if (cpkt_sus_model_open_path(&model, &model_config) != CPKT_SUS_ERR_ARG) {
    return 6;
  }
  if (model != 0) {
    return 7;
  }
  return 0;
}
EOF
cat > "$cmake_source_dir/cpkt_opcua_facade_strict.c" <<'EOF'
#include <cpkt/opcua.h>

#include <string.h>

struct cpkt_strict_browse_seen {
  int object_seen;
  int child_seen;
};

static int cpkt_strict_browse(const cpkt_opcua_browse_entry *entry, void *user) {
  struct cpkt_strict_browse_seen *seen;

  seen = (struct cpkt_strict_browse_seen *)user;
  if (entry == 0 || seen == 0) {
    return 1;
  }
  if (entry->browse_name != 0 && strcmp(entry->browse_name, "strictObject") == 0) {
    seen->object_seen = 1;
  }
  if (entry->browse_name != 0 && strcmp(entry->browse_name, "strictChild") == 0) {
    seen->child_seen = 1;
  }
  return 0;
}

static cpkt_opcua_result cpkt_strict_method(
    const cpkt_opcua_value *inputs,
    size_t input_count,
    cpkt_opcua_value *output,
    void *user) {
  long factor;

  if (inputs == 0 || input_count != 1 || output == 0 || user == 0 ||
      inputs[0].type != CPKT_OPCUA_VALUE_INTEGER) {
    return CPKT_OPCUA_ERR_ARG;
  }
  factor = *(long *)user;
  cpkt_opcua_value_integer(output, inputs[0].integer_value * factor);
  return CPKT_OPCUA_OK;
}

static void cpkt_strict_data_change(
    cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_monitored_item_id monitored_item_id,
    const cpkt_opcua_value *value,
    cpkt_opcua_status status,
    void *user) {
  (void)subscription_id;
  (void)monitored_item_id;
  (void)value;
  (void)status;
  (void)user;
}

int main(void) {
  static const unsigned char json_config[] =
      "{ applicationDescription: { applicationUri: \"urn:cpkt:package:opcua-json\" } }";
  cpkt_opcua_server *server;
  cpkt_opcua_node_id node_id;
  cpkt_opcua_node_id object_id;
  cpkt_opcua_node_id child_id;
  cpkt_opcua_node_id method_id;
  cpkt_opcua_value value;
  cpkt_opcua_value out;
  cpkt_opcua_status status;
  cpkt_opcua_subscription_id subscription_id;
  cpkt_opcua_monitored_item_id monitored_item_id;
  struct cpkt_strict_browse_seen browse_seen;
  char endpoint[64];
  size_t required;
  int method_input_types[1];
  long method_factor;

  if (cpkt_opcua_open62541_version() == 0 || cpkt_opcua_facade_version() == 0) {
    return 1;
  }
  if (cpkt_opcua_server_new_from_json(&server, json_config, sizeof(json_config) - 1, &status) !=
      CPKT_OPCUA_OK) {
    return 2;
  }
  if (cpkt_opcua_server_set_endpoint(server, "127.0.0.1", 4840) != CPKT_OPCUA_OK) {
    cpkt_opcua_server_free(server);
    return 16;
  }
  node_id = cpkt_opcua_node_id_numeric(1, 7001);
  object_id = cpkt_opcua_node_id_numeric(1, 7002);
  child_id = cpkt_opcua_node_id_numeric(1, 7003);
  method_id = cpkt_opcua_node_id_numeric(1, 7004);
  method_input_types[0] = CPKT_OPCUA_VALUE_INTEGER;
  method_factor = 2;
  cpkt_opcua_value_integer(&value, 11);
  if (cpkt_opcua_server_add_variable(server, node_id, "strictValue", "Strict Value", &value, &status) !=
      CPKT_OPCUA_OK) {
    cpkt_opcua_server_free(server);
    return 3;
  }
  if (cpkt_opcua_server_add_object(
          server,
          object_id,
          cpkt_opcua_node_id_numeric(0, 85),
          "strictObject",
          "Strict Object",
          &status) != CPKT_OPCUA_OK) {
    cpkt_opcua_server_free(server);
    return 4;
  }
  if (cpkt_opcua_server_add_variable_under(
          server,
          child_id,
          object_id,
          "strictChild",
          "Strict Child",
          &value,
          &status) != CPKT_OPCUA_OK) {
    cpkt_opcua_server_free(server);
    return 5;
  }
  if (cpkt_opcua_server_add_method(
          server,
          method_id,
          object_id,
          "strictMethod",
          "Strict Method",
          method_input_types,
          1,
          CPKT_OPCUA_VALUE_INTEGER,
          cpkt_strict_method,
          &method_factor,
          &status) != CPKT_OPCUA_OK) {
    cpkt_opcua_server_free(server);
    return 6;
  }
  browse_seen.object_seen = 0;
  browse_seen.child_seen = 0;
  if (cpkt_opcua_server_browse_children(
          server,
          cpkt_opcua_node_id_numeric(0, 85),
          cpkt_strict_browse,
          &browse_seen,
          &status) != CPKT_OPCUA_OK || browse_seen.object_seen == 0) {
    cpkt_opcua_server_free(server);
    return 7;
  }
  browse_seen.object_seen = 0;
  browse_seen.child_seen = 0;
  if (cpkt_opcua_server_browse_children(
          server,
          object_id,
          cpkt_strict_browse,
          &browse_seen,
          &status) != CPKT_OPCUA_OK || browse_seen.child_seen == 0) {
    cpkt_opcua_server_free(server);
    return 8;
  }
  cpkt_opcua_value_integer(&value, 12);
  if (cpkt_opcua_server_write(server, node_id, &value, &status) != CPKT_OPCUA_OK) {
    cpkt_opcua_server_free(server);
    return 9;
  }
  if (cpkt_opcua_server_read(server, node_id, &out, 0, 0, 0, &status) != CPKT_OPCUA_OK) {
    cpkt_opcua_server_free(server);
    return 10;
  }
  if (out.type != CPKT_OPCUA_VALUE_INTEGER || out.integer_value != 12) {
    cpkt_opcua_server_free(server);
    return 11;
  }
  if (cpkt_opcua_server_endpoint_url(server, endpoint, sizeof(endpoint), &required) != CPKT_OPCUA_OK) {
    cpkt_opcua_server_free(server);
    return 12;
  }
  if (required == 0 || strstr(endpoint, "opc.tcp://127.0.0.1:") != endpoint) {
    cpkt_opcua_server_free(server);
    return 13;
  }
  if (cpkt_opcua_client_create_subscription(0, 1.0, &subscription_id, &status) != CPKT_OPCUA_ERR_ARG) {
    cpkt_opcua_server_free(server);
    return 14;
  }
  if (cpkt_opcua_client_monitor_value(
          0,
          1,
          node_id,
          1.0,
          cpkt_strict_data_change,
          0,
          &monitored_item_id,
          &status) != CPKT_OPCUA_ERR_ARG) {
    cpkt_opcua_server_free(server);
    return 15;
  }
  cpkt_opcua_server_free(server);
  return 0;
}
EOF
cat > "$cmake_source_dir/cpkt_lua_runtime_strict.c" <<'EOF'
#include <cpkt/lua_runtime.h>

#include <stdlib.h>
#include <string.h>

int cpkt_strict_host_open(void *lua_state);

struct cpkt_strict_allocator {
  size_t alloc_calls;
  size_t realloc_calls;
  size_t free_calls;
  size_t bytes_live;
};

static void *cpkt_strict_alloc(void *user, size_t size) {
  struct cpkt_strict_allocator *allocator;
  void *ptr;

  allocator = (struct cpkt_strict_allocator *)user;
  allocator->alloc_calls += 1;
  ptr = malloc(size);
  if (ptr != 0) {
    allocator->bytes_live += size;
  }
  return ptr;
}

static void *cpkt_strict_realloc(void *user, void *ptr, size_t old_size, size_t new_size) {
  struct cpkt_strict_allocator *allocator;
  void *next;

  allocator = (struct cpkt_strict_allocator *)user;
  allocator->realloc_calls += 1;
  next = realloc(ptr, new_size);
  if (next != 0) {
    if (allocator->bytes_live >= old_size) {
      allocator->bytes_live -= old_size;
    } else {
      allocator->bytes_live = 0;
    }
    allocator->bytes_live += new_size;
  }
  return next;
}

static void cpkt_strict_free(void *user, void *ptr, size_t size) {
  struct cpkt_strict_allocator *allocator;

  allocator = (struct cpkt_strict_allocator *)user;
  allocator->free_calls += 1;
  if (allocator->bytes_live >= size) {
    allocator->bytes_live -= size;
  } else {
    allocator->bytes_live = 0;
  }
  free(ptr);
}

static int cpkt_check(cpkt_lua_runtime_status status) {
  return status == CPKT_LUA_RUNTIME_OK ? 0 : 1;
}

int main(int argc, char **argv) {
  static const unsigned char preload_source[] = "return {value = 'chunk'}";
  static const unsigned char buffer_source[] =
      "local h = require('host')\n"
      "local c = require('chunkmod')\n"
      "if h.context ~= 'strict-context' then error('bad context') end\n"
      "if c.value ~= 'chunk' then error('bad chunk') end\n"
      "if arg[0] ~= 'buffer.lua' then error('bad arg0') end\n"
      "if arg[1] ~= 'one' or arg[2] ~= 'two' then error('bad argv') end\n";
  static const unsigned char side_effect_source[] = "required_side_effect = 'ok'; return true";
  static const unsigned char limit_source[] = "while true do end";
  const char *script_argv[2];
  struct cpkt_strict_allocator allocator;
  cpkt_lua_runtime_allocator_config allocator_config;
  cpkt_lua_runtime *runtime;
  cpkt_lua_runtime *limited_runtime;
  cpkt_lua_runtime_status status;
  const char *limit_error;

  if (argc != 2) {
    return 2;
  }

  if (cpkt_lua_runtime_lua_version() == 0 || cpkt_lua_runtime_facade_version() == 0) {
    return 3;
  }

  memset(&allocator, 0, sizeof(allocator));
  memset(&allocator_config, 0, sizeof(allocator_config));
  allocator_config.user = &allocator;
  allocator_config.alloc_fn = cpkt_strict_alloc;
  allocator_config.realloc_fn = cpkt_strict_realloc;
  allocator_config.free_fn = cpkt_strict_free;
  allocator_config.max_bytes = 16 * 1024 * 1024;
  status = cpkt_lua_runtime_new_with_allocator(&runtime, &allocator_config);
  if (status != CPKT_LUA_RUNTIME_OK) {
    return 4;
  }

  cpkt_lua_runtime_set_context(runtime, (void *)"strict-context");
  if (cpkt_check(cpkt_lua_runtime_open_libs(
          runtime,
          CPKT_LUA_RUNTIME_LIB_BASE |
              CPKT_LUA_RUNTIME_LIB_PACKAGE |
              CPKT_LUA_RUNTIME_LIB_TABLE |
              CPKT_LUA_RUNTIME_LIB_STRING |
              CPKT_LUA_RUNTIME_LIB_MATH)) != 0) {
    cpkt_lua_runtime_free(runtime);
    return 5;
  }
  if (cpkt_lua_runtime_context(runtime) == 0) {
    cpkt_lua_runtime_free(runtime);
    return 6;
  }
  if (cpkt_check(cpkt_lua_runtime_set_traceback(runtime, 1)) != 0) {
    cpkt_lua_runtime_free(runtime);
    return 7;
  }
  if (cpkt_check(cpkt_lua_runtime_set_package_path(runtime, "first/?.lua")) != 0) {
    cpkt_lua_runtime_free(runtime);
    return 8;
  }
  if (cpkt_check(cpkt_lua_runtime_prepend_package_path(runtime, "zero/?.lua")) != 0) {
    cpkt_lua_runtime_free(runtime);
    return 9;
  }
  if (cpkt_check(cpkt_lua_runtime_set_package_cpath(runtime, "first/?.so")) != 0) {
    cpkt_lua_runtime_free(runtime);
    return 10;
  }
  if (cpkt_check(cpkt_lua_runtime_prepend_package_cpath(runtime, "zero/?.so")) != 0) {
    cpkt_lua_runtime_free(runtime);
    return 11;
  }
  if (cpkt_check(cpkt_lua_runtime_set_global_string(runtime, "strict_name", "facade")) != 0) {
    cpkt_lua_runtime_free(runtime);
    return 12;
  }
  if (cpkt_check(cpkt_lua_runtime_set_global_boolean(runtime, "strict_flag", 1)) != 0) {
    cpkt_lua_runtime_free(runtime);
    return 13;
  }
  if (cpkt_check(cpkt_lua_runtime_set_global_integer(runtime, "strict_count", 7)) != 0) {
    cpkt_lua_runtime_free(runtime);
    return 14;
  }
  if (cpkt_check(cpkt_lua_runtime_set_global_number(runtime, "strict_number", 2.5)) != 0) {
    cpkt_lua_runtime_free(runtime);
    return 15;
  }
  if (cpkt_check(cpkt_lua_runtime_register_c_module(runtime, "host", cpkt_strict_host_open)) != 0) {
    cpkt_lua_runtime_free(runtime);
    return 16;
  }
  if (cpkt_check(cpkt_lua_runtime_register_lua_module(
          runtime,
          "chunkmod",
          preload_source,
          sizeof(preload_source) - 1,
          "chunkmod.lua")) != 0) {
    cpkt_lua_runtime_free(runtime);
    return 17;
  }
  if (cpkt_check(cpkt_lua_runtime_register_lua_module(
          runtime,
          "sideeffect",
          side_effect_source,
          sizeof(side_effect_source) - 1,
          "sideeffect.lua")) != 0) {
    cpkt_lua_runtime_free(runtime);
    return 18;
  }
  if (cpkt_check(cpkt_lua_runtime_require(runtime, "sideeffect")) != 0) {
    cpkt_lua_runtime_free(runtime);
    return 19;
  }

  script_argv[0] = "one";
  script_argv[1] = "two";
  if (cpkt_check(cpkt_lua_runtime_run_buffer(
          runtime,
          buffer_source,
          sizeof(buffer_source) - 1,
          "buffer.lua",
          2,
          script_argv,
          0)) != 0) {
    cpkt_lua_runtime_free(runtime);
    return 20;
  }
  if (cpkt_check(cpkt_lua_runtime_run_file(runtime, argv[1], 2, script_argv, 0)) != 0) {
    cpkt_lua_runtime_free(runtime);
    return 21;
  }

  cpkt_lua_runtime_free(runtime);
  if (allocator.alloc_calls == 0 || allocator.free_calls == 0) {
    return 22;
  }

  status = cpkt_lua_runtime_new(&limited_runtime);
  if (status != CPKT_LUA_RUNTIME_OK) {
    return 23;
  }
  if (cpkt_check(cpkt_lua_runtime_set_traceback(limited_runtime, 1)) != 0) {
    cpkt_lua_runtime_free(limited_runtime);
    return 24;
  }
  if (cpkt_check(cpkt_lua_runtime_set_instruction_limit(limited_runtime, 1000)) != 0) {
    cpkt_lua_runtime_free(limited_runtime);
    return 25;
  }
  status = cpkt_lua_runtime_run_buffer(
      limited_runtime,
      limit_source,
      sizeof(limit_source) - 1,
      "limit.lua",
      0,
      0,
      0);
  if (status != CPKT_LUA_RUNTIME_ERR_LIMIT) {
    cpkt_lua_runtime_free(limited_runtime);
    return 26;
  }
  limit_error = cpkt_lua_runtime_error(limited_runtime);
  if (limit_error == 0 || strstr(limit_error, "instruction limit") == 0) {
    cpkt_lua_runtime_free(limited_runtime);
    return 27;
  }
  if (cpkt_check(cpkt_lua_runtime_clear_instruction_limit(limited_runtime)) != 0) {
    cpkt_lua_runtime_free(limited_runtime);
    return 28;
  }
  cpkt_lua_runtime_clear_error(limited_runtime);
  if (cpkt_lua_runtime_error(limited_runtime)[0] != '\0') {
    cpkt_lua_runtime_free(limited_runtime);
    return 29;
  }
  cpkt_lua_runtime_free(limited_runtime);

  return 0;
}
EOF
cat > "$cmake_source_dir/cpkt_lua_runtime_module.c" <<'EOF'
#include <cpkt/lua_runtime.h>

#include <lua.h>

int cpkt_strict_host_open(void *lua_state) {
  lua_State *state = (lua_State *)lua_state;
  const char *context = (const char *)cpkt_lua_runtime_context_from_state(lua_state);

  lua_newtable(state);
  lua_pushstring(state, context != 0 ? context : "");
  lua_setfield(state, -2, "context");
  return 1;
}
EOF
cat > "$cmake_source_dir/CMakeLists.txt" <<EOF
cmake_minimum_required(VERSION 3.21)
project(cpkt_package_cmake_static_smoke LANGUAGES C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(OpenSSL CONFIG REQUIRED)
find_package(ZLIB CONFIG REQUIRED)
find_package(nghttp2 CONFIG REQUIRED)
find_package(Libssh2 CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(libxml2 CONFIG REQUIRED)
find_package(Lua CONFIG REQUIRED)
find_package(miniaudio CONFIG REQUIRED)
find_package(mqtt-c CONFIG REQUIRED)
find_package(CpktLuaRuntime CONFIG REQUIRED)
find_package(CpktAudio CONFIG REQUIRED)
find_package(CpktOpcUa CONFIG REQUIRED)
find_package(open62541 CONFIG REQUIRED)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  find_package(CpktSus CONFIG REQUIRED)
endif()

function(cpkt_add_static_smoke target_name source_name link_target)
  add_executable("\${target_name}" "\${source_name}")
  target_compile_options("\${target_name}" PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_link_libraries("\${target_name}" PRIVATE "\${link_target}")
endfunction()

cpkt_add_static_smoke(cpkt_cmake_zlib cpkt_zlib.c ZLIB::ZLIB)
cpkt_add_static_smoke(cpkt_cmake_nghttp2 cpkt_nghttp2.c nghttp2::nghttp2)
cpkt_add_static_smoke(cpkt_cmake_crypto cpkt_crypto.c OpenSSL::Crypto)
cpkt_add_static_smoke(cpkt_cmake_ssl cpkt_ssl.c OpenSSL::SSL)
cpkt_add_static_smoke(cpkt_cmake_libssh2 cpkt_libssh2.c Libssh2::libssh2)
cpkt_add_static_smoke(cpkt_cmake_curl cpkt_curl.c CURL::libcurl)
cpkt_add_static_smoke(cpkt_cmake_libxml2 cpkt_libxml2.c LibXml2::LibXml2)
cpkt_add_static_smoke(cpkt_cmake_lua cpkt_lua.c Lua::Lua)
cpkt_add_static_smoke(cpkt_cmake_mqttc cpkt_mqttc.c MQTT-C::mqttc)
cpkt_add_static_smoke(cpkt_cmake_open62541 cpkt_open62541.c open62541::open62541)
cpkt_add_static_smoke(cpkt_cmake_audio_facade cpkt_audio_facade_strict.c cpkt::audio)
cpkt_add_static_smoke(cpkt_cmake_opcua_facade cpkt_opcua_facade_strict.c cpkt::opcua)
set_source_files_properties(cpkt_audio_facade_strict.c PROPERTIES
  COMPILE_OPTIONS "-std=c89;-Wall;-Wextra;-Wpedantic;-Werror")
set_source_files_properties(cpkt_opcua_facade_strict.c PROPERTIES
  COMPILE_OPTIONS "-std=c89;-Wall;-Wextra;-Wpedantic;-Werror")
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  cpkt_add_static_smoke(cpkt_cmake_sus_facade cpkt_sus_facade_strict.c cpkt::sus)
  add_executable(cpkt_cmake_audio_sus_facade cpkt_audio_sus_facade_strict.c)
  target_link_libraries(cpkt_cmake_audio_sus_facade PRIVATE cpkt::audio cpkt::sus)
  set_source_files_properties(cpkt_sus_facade_strict.c PROPERTIES
    COMPILE_OPTIONS "-std=c89;-Wall;-Wextra;-Wpedantic;-Werror")
  set_source_files_properties(cpkt_audio_sus_facade_strict.c PROPERTIES
    COMPILE_OPTIONS "-std=c89;-Wall;-Wextra;-Wpedantic;-Werror")
endif()
add_executable(cpkt_cmake_all cpkt_all.c)
target_compile_options(cpkt_cmake_all PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(cpkt_cmake_all PRIVATE CURL::libcurl LibXml2::LibXml2 Lua::Lua miniaudio::miniaudio open62541::open62541)

file(WRITE "\${CMAKE_CURRENT_BINARY_DIR}/strict_file.lua"
  "local h = require('host')\\n"
  "local c = require('chunkmod')\\n"
  "if h.context ~= 'strict-context' then error('bad file context') end\\n"
  "if c.value ~= 'chunk' then error('bad file chunk') end\\n"
  "if package.path ~= 'zero/?.lua;first/?.lua' then error('bad file package path') end\\n"
  "if package.cpath ~= 'zero/?.so;first/?.so' then error('bad file package cpath') end\\n"
  "if strict_name ~= 'facade' then error('bad file string global') end\\n"
  "if strict_flag ~= true then error('bad file boolean global') end\\n"
  "if strict_count ~= 7 then error('bad file integer global') end\\n"
  "if strict_number ~= 2.5 then error('bad file number global') end\\n"
  "if required_side_effect ~= 'ok' then error('bad file require side effect') end\\n"
  "if arg[0] ~= '\${CMAKE_CURRENT_BINARY_DIR}/strict_file.lua' then error('bad file arg0') end\\n"
  "if arg[1] ~= 'one' or arg[2] ~= 'two' then error('bad file argv') end\\n")
add_executable(cpkt_cmake_lua_runtime_strict
  cpkt_lua_runtime_strict.c
  cpkt_lua_runtime_module.c)
set_source_files_properties(cpkt_lua_runtime_strict.c PROPERTIES
  COMPILE_OPTIONS "-std=c89;-Wall;-Wextra;-Wpedantic;-Werror")
set_source_files_properties(cpkt_lua_runtime_module.c PROPERTIES
  COMPILE_OPTIONS "-std=c99;-Wall;-Wextra;-Wpedantic;-Werror")
target_link_libraries(cpkt_cmake_lua_runtime_strict PRIVATE cpkt::lua_runtime)
target_compile_definitions(cpkt_cmake_lua_runtime_strict PRIVATE
  CPKT_STRICT_FILE="\${CMAKE_CURRENT_BINARY_DIR}/strict_file.lua")
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
  -Dlibxml2_DIR="$prefix/lib/cmake/libxml2" \
  -DLua_DIR="$prefix/lib/cmake/Lua" \
  -Dmqtt-c_DIR="$prefix/lib/cmake/mqtt-c" \
  -DCpktLuaRuntime_DIR="$prefix/lib/cmake/CpktLuaRuntime" \
  -DCpktSus_DIR="$prefix/lib/cmake/CpktSus" \
  -DCpktOpcUa_DIR="$prefix/lib/cmake/CpktOpcUa" \
  -Dopen62541_DIR="$prefix/lib/cmake/open62541" \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
)
if [ -n "$cmake_toolchain_file" ]; then
  cmake_args+=("-DCMAKE_TOOLCHAIN_FILE=$cmake_toolchain_file")
  cmake_args+=("${cmake_toolchain_args[@]}")
  cmake_args+=("-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH")
fi
cpkt_run_checked "cmake aggregate consumer configure" cmake "${cmake_args[@]}"
cpkt_run_checked "cmake aggregate consumer build" cmake --build "$cmake_build_dir"

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

assert_words_count() {
  words=$1
  expected_word=$2
  expected_count=$3
  description=$4
  actual_count=0
  for word in $words; do
    if [ "$word" = "$expected_word" ]; then
      actual_count=$((actual_count + 1))
    fi
  done
  if [ "$actual_count" -ne "$expected_count" ]; then
    printf '%s expected %s occurrence(s) of %s, got %s\n' \
      "$description" "$expected_count" "$expected_word" "$actual_count" >&2
    printf 'actual words: %s\n' "$words" >&2
    exit 1
  fi
}

cmake_link_dir="$cmake_build_dir/CMakeFiles"
assert_file_contains "$cmake_link_dir/cpkt_cmake_libssh2.dir/link.txt" "$prefix/lib/libcrypto.a" "Libssh2::libssh2 link line"
assert_file_contains "$cmake_link_dir/cpkt_cmake_libssh2.dir/link.txt" "$prefix/lib/libz.a" "Libssh2::libssh2 link line"
assert_file_contains "$cmake_link_dir/cpkt_cmake_curl.dir/link.txt" "$prefix/lib/libssh2.a" "CURL::libcurl link line"
assert_file_contains "$cmake_link_dir/cpkt_cmake_curl.dir/link.txt" "$prefix/lib/libnghttp2.a" "CURL::libcurl link line"
assert_file_contains "$cmake_link_dir/cpkt_cmake_curl.dir/link.txt" "$prefix/lib/libssl.a" "CURL::libcurl link line"
assert_file_contains "$cmake_link_dir/cpkt_cmake_curl.dir/link.txt" "$prefix/lib/libcrypto.a" "CURL::libcurl link line"
assert_file_contains "$cmake_link_dir/cpkt_cmake_curl.dir/link.txt" "$prefix/lib/libz.a" "CURL::libcurl link line"
assert_file_contains "$cmake_link_dir/cpkt_cmake_libxml2.dir/link.txt" "$prefix/lib/libz.a" "LibXml2::LibXml2 link line"
assert_file_contains "$cmake_link_dir/cpkt_cmake_mqttc.dir/link.txt" "$prefix/lib/libmqttc.a" "MQTT-C::mqttc link line"
assert_file_contains "$cmake_link_dir/cpkt_cmake_open62541.dir/link.txt" "$prefix/lib/libssl.a" "open62541::open62541 link line"
assert_file_contains "$cmake_link_dir/cpkt_cmake_open62541.dir/link.txt" "$prefix/lib/libcrypto.a" "open62541::open62541 link line"
assert_file_contains "$cmake_link_dir/cpkt_cmake_audio_facade.dir/link.txt" "$prefix/lib/libcpktaudio.a" "cpkt::audio link line"
assert_file_contains "$cmake_link_dir/cpkt_cmake_audio_facade.dir/link.txt" "$prefix/lib/libcurl.a" "cpkt::audio link line"
assert_file_not_contains "$cmake_link_dir/cpkt_cmake_audio_facade.dir/link.txt" "$prefix/lib/libminiaudio.a" "cpkt::audio link line"
assert_file_not_contains "$cmake_link_dir/cpkt_cmake_audio_facade.dir/link.txt" "$prefix/lib/libcpktsus.a" "cpkt::audio link line"
assert_file_not_contains "$cmake_link_dir/cpkt_cmake_audio_facade.dir/link.txt" "$prefix/lib/libwhisper.a" "cpkt::audio link line"
assert_file_not_contains "$cmake_link_dir/cpkt_cmake_audio_facade.dir/link.txt" "$prefix/lib/libggml.a" "cpkt::audio link line"
assert_file_contains "$cmake_link_dir/cpkt_cmake_opcua_facade.dir/link.txt" "$prefix/lib/libcpkt_opcua.a" "cpkt::opcua link line"
assert_file_contains "$cmake_link_dir/cpkt_cmake_opcua_facade.dir/link.txt" "$prefix/lib/libopen62541.a" "cpkt::opcua link line"
case "$target_id" in
  *-linux-gnu)
    assert_file_contains "$cmake_link_dir/cpkt_cmake_crypto.dir/link.txt" "-ldl" "OpenSSL::Crypto link line"
    assert_file_contains "$cmake_link_dir/cpkt_cmake_curl.dir/link.txt" "-ldl" "CURL::libcurl link line"
    assert_file_contains "$cmake_link_dir/cpkt_cmake_libxml2.dir/link.txt" "-ldl" "LibXml2::LibXml2 link line"
    assert_file_contains "$cmake_link_dir/cpkt_cmake_lua.dir/link.txt" "-ldl" "Lua::Lua link line"
    assert_file_contains "$cmake_link_dir/cpkt_cmake_open62541.dir/link.txt" "-lrt" "open62541::open62541 link line"
    ;;
  arm64-apple-darwin)
    assert_file_contains "$cmake_link_dir/cpkt_cmake_curl.dir/link.txt" "CoreFoundation" "CURL::libcurl link line"
    assert_file_contains "$cmake_link_dir/cpkt_cmake_curl.dir/link.txt" "SystemConfiguration" "CURL::libcurl link line"
    assert_file_contains "$cmake_link_dir/cpkt_cmake_libxml2.dir/link.txt" "iconv" "LibXml2::LibXml2 link line"
    ;;
esac
case "$target_id" in
  *-linux-*)
    assert_file_contains "$cmake_link_dir/cpkt_cmake_sus_facade.dir/link.txt" "$prefix/lib/libcpktsus.a" "cpkt::sus link line"
    assert_file_contains "$cmake_link_dir/cpkt_cmake_sus_facade.dir/link.txt" "$prefix/lib/libcpktaudio.a" "cpkt::sus link line"
    assert_file_contains "$cmake_link_dir/cpkt_cmake_sus_facade.dir/link.txt" "$prefix/lib/libwhisper.a" "cpkt::sus link line"
    assert_file_contains "$cmake_link_dir/cpkt_cmake_sus_facade.dir/link.txt" "$prefix/lib/cpkt-cxx/libstdc++.a" "cpkt::sus link line"
    assert_file_contains "$cmake_link_dir/cpkt_cmake_sus_facade.dir/link.txt" "$prefix/lib/cpkt-cxx/libgcc.a" "cpkt::sus link line"
    assert_file_not_contains "$cmake_link_dir/cpkt_cmake_sus_facade.dir/link.txt" "$prefix/lib/libminiaudio.a" "cpkt::sus link line"
    assert_file_contains "$cmake_link_dir/cpkt_cmake_audio_sus_facade.dir/link.txt" "$prefix/lib/libcpktaudio.a" "cpkt::audio+sus link line"
    assert_file_contains "$cmake_link_dir/cpkt_cmake_audio_sus_facade.dir/link.txt" "$prefix/lib/libcpktsus.a" "cpkt::audio+sus link line"
    assert_file_contains "$cmake_link_dir/cpkt_cmake_audio_sus_facade.dir/link.txt" "$prefix/lib/libcurl.a" "cpkt::audio+sus link line"
    assert_file_contains "$cmake_link_dir/cpkt_cmake_audio_sus_facade.dir/link.txt" "$prefix/lib/cpkt-cxx/libstdc++.a" "cpkt::audio+sus link line"
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
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package($package_name CONFIG REQUIRED)

add_executable($executable_name "$source_name")
target_compile_options($executable_name PRIVATE -Wall -Wextra -Wpedantic -Werror)
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
  cpkt_run_checked "cmake direct ${package_name} configure" cmake "${direct_cmake_args[@]}"
  cpkt_run_checked "cmake direct ${package_name} build" cmake --build "$direct_build_dir"
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

cpkt_cmake_direct_dir_smoke libxml2 "$prefix/lib/cmake/libxml2" cpkt_direct_libxml2 cpkt_libxml2.c LibXml2::LibXml2
direct_libxml2_link_dir="$work_root/cmake-direct-libxml2-build/CMakeFiles/cpkt_direct_libxml2.dir"
assert_file_contains "$direct_libxml2_link_dir/link.txt" "$prefix/lib/libz.a" "direct libxml2_DIR link line"
case "$target_id" in
  arm64-apple-darwin)
    assert_file_contains "$direct_libxml2_link_dir/link.txt" "iconv" "direct libxml2_DIR link line"
    ;;
esac

cpkt_cmake_direct_dir_smoke Lua "$prefix/lib/cmake/Lua" cpkt_direct_lua cpkt_lua.c Lua::Lua
cpkt_cmake_direct_dir_smoke mqtt-c "$prefix/lib/cmake/mqtt-c" cpkt_direct_mqttc cpkt_mqttc.c MQTT-C::mqttc
cpkt_cmake_direct_dir_smoke open62541 "$prefix/lib/cmake/open62541" cpkt_direct_open62541 cpkt_open62541.c open62541::open62541
direct_open62541_link_dir="$work_root/cmake-direct-open62541-build/CMakeFiles/cpkt_direct_open62541.dir"
assert_file_contains "$direct_open62541_link_dir/link.txt" "$prefix/lib/libssl.a" "direct open62541_DIR link line"
assert_file_contains "$direct_open62541_link_dir/link.txt" "$prefix/lib/libcrypto.a" "direct open62541_DIR link line"
example_cmake_build_dir="$work_root/example-cmake-consumer-build"
example_cmake_args=(
  -G "$cmake_generator" \
  -S "$installed_examples_dir/cmake-consumer" \
  -B "$example_cmake_build_dir" \
  -DCMAKE_C_COMPILER="$cc" \
  "-DCMAKE_C_FLAGS=-Werror" \
  -DCMAKE_PREFIX_PATH="$prefix" \
  -DCURL_DIR="$prefix/lib/cmake/CURL" \
  -Dlibxml2_DIR="$prefix/lib/cmake/libxml2" \
  -DLua_DIR="$prefix/lib/cmake/Lua" \
  -Dopen62541_DIR="$prefix/lib/cmake/open62541" \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
)
if [ -n "$cmake_toolchain_file" ]; then
  example_cmake_args+=("-DCMAKE_TOOLCHAIN_FILE=$cmake_toolchain_file")
  example_cmake_args+=("${cmake_toolchain_args[@]}")
  example_cmake_args+=("-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH")
fi
cpkt_run_checked "example cmake consumer configure" cmake "${example_cmake_args[@]}"
cpkt_run_checked "example cmake consumer build" cmake --build "$example_cmake_build_dir"

lua_runtime_example_cmake_build_dir="$work_root/example-lua-runtime-c89-cmake-build"
lua_runtime_example_cmake_args=(
  -G "$cmake_generator" \
  -S "$installed_examples_dir/lua-runtime-c89" \
  -B "$lua_runtime_example_cmake_build_dir" \
  -DCMAKE_C_COMPILER="$cc" \
  "-DCMAKE_C_FLAGS=-Werror" \
  -DCMAKE_PREFIX_PATH="$prefix" \
  -DCpktLuaRuntime_DIR="$prefix/lib/cmake/CpktLuaRuntime" \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
)
if [ -n "$cmake_toolchain_file" ]; then
  lua_runtime_example_cmake_args+=("-DCMAKE_TOOLCHAIN_FILE=$cmake_toolchain_file")
  lua_runtime_example_cmake_args+=("${cmake_toolchain_args[@]}")
  lua_runtime_example_cmake_args+=("-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH")
fi
cpkt_run_checked "lua runtime cmake example configure" cmake "${lua_runtime_example_cmake_args[@]}"
cpkt_run_checked "lua runtime cmake example build" cmake --build "$lua_runtime_example_cmake_build_dir"

opcua_example_cmake_build_dir="$work_root/example-opcua-c89-cmake-build"
opcua_example_cmake_args=(
  -G "$cmake_generator" \
  -S "$installed_examples_dir/opcua-c89" \
  -B "$opcua_example_cmake_build_dir" \
  -DCMAKE_C_COMPILER="$cc" \
  "-DCMAKE_C_FLAGS=-Werror" \
  -DCMAKE_PREFIX_PATH="$prefix" \
  -DCpktOpcUa_DIR="$prefix/lib/cmake/CpktOpcUa" \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
)
if [ -n "$cmake_toolchain_file" ]; then
  opcua_example_cmake_args+=("-DCMAKE_TOOLCHAIN_FILE=$cmake_toolchain_file")
  opcua_example_cmake_args+=("${cmake_toolchain_args[@]}")
  opcua_example_cmake_args+=("-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH")
fi
cpkt_run_checked "opcua cmake example configure" cmake "${opcua_example_cmake_args[@]}"
cpkt_run_checked "opcua cmake example build" cmake --build "$opcua_example_cmake_build_dir"

audio_sus_example_cmake_build_dir="$work_root/example-audio-sus-c89-cmake-build"
audio_sus_example_cmake_args=(
  -G "$cmake_generator" \
  -S "$installed_examples_dir/audio-sus-c89" \
  -B "$audio_sus_example_cmake_build_dir" \
  -DCMAKE_C_COMPILER="$cc" \
  "-DCMAKE_C_FLAGS=-Werror" \
  -DCMAKE_PREFIX_PATH="$prefix" \
  -DCpktAudio_DIR="$prefix/lib/cmake/CpktAudio" \
  -DCpktSus_DIR="$prefix/lib/cmake/CpktSus" \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
)
if [ -n "$cmake_toolchain_file" ]; then
  audio_sus_example_cmake_args+=("-DCMAKE_TOOLCHAIN_FILE=$cmake_toolchain_file")
  audio_sus_example_cmake_args+=("${cmake_toolchain_args[@]}")
  audio_sus_example_cmake_args+=("-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH")
fi
cpkt_run_checked "audio sus cmake example configure" cmake "${audio_sus_example_cmake_args[@]}"
cpkt_run_checked "audio sus cmake example build" cmake --build "$audio_sus_example_cmake_build_dir"

audio_vox_example_cmake_build_dir="$work_root/example-audio-vox-intro-c89-cmake-build"
audio_vox_example_cmake_args=(
  -G "$cmake_generator" \
  -S "$installed_examples_dir/audio-vox-intro-c89" \
  -B "$audio_vox_example_cmake_build_dir" \
  -DCMAKE_C_COMPILER="$cc" \
  "-DCMAKE_C_FLAGS=-Werror" \
  -DCMAKE_PREFIX_PATH="$prefix" \
  -DCpktAudio_DIR="$prefix/lib/cmake/CpktAudio" \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
)
if [ -n "$cmake_toolchain_file" ]; then
  audio_vox_example_cmake_args+=("-DCMAKE_TOOLCHAIN_FILE=$cmake_toolchain_file")
  audio_vox_example_cmake_args+=("${cmake_toolchain_args[@]}")
  audio_vox_example_cmake_args+=("-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH")
fi
cpkt_run_checked "audio vox intro cmake example configure" cmake "${audio_vox_example_cmake_args[@]}"
cpkt_run_checked "audio vox intro cmake example build" cmake --build "$audio_vox_example_cmake_build_dir"

sus_vox_example_cmake_build_dir="$work_root/example-sus-vox-intro-c89-cmake-build"
sus_vox_example_cmake_args=(
  -G "$cmake_generator" \
  -S "$installed_examples_dir/sus-vox-intro-c89" \
  -B "$sus_vox_example_cmake_build_dir" \
  -DCMAKE_C_COMPILER="$cc" \
  "-DCMAKE_C_FLAGS=-Werror" \
  -DCMAKE_PREFIX_PATH="$prefix" \
  -DCpktAudio_DIR="$prefix/lib/cmake/CpktAudio" \
  -DCpktSus_DIR="$prefix/lib/cmake/CpktSus" \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
)
if [ -n "$cmake_toolchain_file" ]; then
  sus_vox_example_cmake_args+=("-DCMAKE_TOOLCHAIN_FILE=$cmake_toolchain_file")
  sus_vox_example_cmake_args+=("${cmake_toolchain_args[@]}")
  sus_vox_example_cmake_args+=("-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH")
fi
cpkt_run_checked "sus vox intro cmake example configure" cmake "${sus_vox_example_cmake_args[@]}"
cpkt_run_checked "sus vox intro cmake example build" cmake --build "$sus_vox_example_cmake_build_dir"

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
libxml2_words=$(pkg_config_words libxml-2.0)
lua_words=$(pkg_config_words lua)
mqttc_words=$(pkg_config_words mqtt-c)
lua_runtime_words=$(pkg_config_words cpkt-lua-runtime)
audio_words=$(pkg_config_words cpkt-audio)
open62541_words=$(pkg_config_words open62541)
opcua_words=$(pkg_config_words cpkt-opcua)
sus_words=$(pkg_config_words cpkt-sus)
openssl_default_words=$(pkg_config_default_words openssl)

case "$target_id" in
  *-linux-gnu)
    assert_words_contain "$libcrypto_words" "-ldl" "libcrypto.pc --static output"
    assert_words_contain "$libssl_words" "-ldl" "libssl.pc --static output"
    assert_words_contain "$openssl_words" "-ldl" "openssl.pc --static output"
    assert_words_contain "$libcurl_words" "-ldl" "libcurl.pc --static output"
    assert_words_contain "$libxml2_words" "-ldl" "libxml-2.0.pc --static output"
    assert_words_contain "$lua_words" "-ldl" "lua.pc --static output"
    assert_words_contain "$lua_runtime_words" "-ldl" "cpkt-lua-runtime.pc --static output"
    assert_words_contain "$opcua_words" "-lrt" "cpkt-opcua.pc --static output"
    assert_words_contain "$mqttc_words" "-pthread" "mqtt-c.pc --static output"
    assert_words_contain "$open62541_words" "-lrt" "open62541.pc --static output"
    ;;
  arm64-apple-darwin)
    assert_words_contain "$libcurl_words" "CoreFoundation" "libcurl.pc --static output"
    assert_words_contain "$libcurl_words" "SystemConfiguration" "libcurl.pc --static output"
    assert_words_contain "$libxml2_words" "-liconv" "libxml-2.0.pc --static output"
    assert_words_not_contain "$libcrypto_words" "-ldl" "libcrypto.pc --static output"
    assert_words_not_contain "$libssl_words" "-ldl" "libssl.pc --static output"
    assert_words_not_contain "$openssl_words" "-ldl" "openssl.pc --static output"
    assert_words_not_contain "$libcurl_words" "-ldl" "libcurl.pc --static output"
    assert_words_not_contain "$libxml2_words" "-ldl" "libxml-2.0.pc --static output"
    assert_words_not_contain "$lua_words" "-ldl" "lua.pc --static output"
    assert_words_not_contain "$lua_runtime_words" "-ldl" "cpkt-lua-runtime.pc --static output"
    assert_words_not_contain "$opcua_words" "-ldl" "cpkt-opcua.pc --static output"
    assert_words_not_contain "$open62541_words" "-ldl" "open62541.pc --static output"
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
assert_words_contain "$libxml2_words" "-lz" "libxml-2.0.pc --static output"
assert_words_contain "$libxml2_words" "-lm" "libxml-2.0.pc --static output"
assert_words_contain "$lua_words" "-lm" "lua.pc --static output"
assert_words_contain "$lua_runtime_words" "-llua" "cpkt-lua-runtime.pc --static output"
assert_words_contain "$lua_runtime_words" "-lm" "cpkt-lua-runtime.pc --static output"
assert_words_contain "$audio_words" "-lcpktaudio" "cpkt-audio.pc --static output"
assert_words_not_contain "$audio_words" "-lminiaudio" "cpkt-audio.pc --static output"
assert_words_contain "$audio_words" "-lcurl" "cpkt-audio.pc --static output"
assert_words_contain "$audio_words" "-lssh2" "cpkt-audio.pc --static output"
assert_words_contain "$audio_words" "-lnghttp2" "cpkt-audio.pc --static output"
assert_words_contain "$audio_words" "-lssl" "cpkt-audio.pc --static output"
assert_words_contain "$audio_words" "-lcrypto" "cpkt-audio.pc --static output"
assert_words_contain "$audio_words" "-lz" "cpkt-audio.pc --static output"
assert_words_not_contain "$audio_words" "-lcpktsus" "cpkt-audio.pc --static output"
assert_words_not_contain "$audio_words" "-lwhisper" "cpkt-audio.pc --static output"
assert_words_not_contain "$audio_words" "-lggml" "cpkt-audio.pc --static output"
assert_words_not_contain "$audio_words" "-lggml-base" "cpkt-audio.pc --static output"
assert_words_not_contain "$audio_words" "-lggml-cpu" "cpkt-audio.pc --static output"
assert_words_contain "$mqttc_words" "-lmqttc" "mqtt-c.pc --static output"
assert_words_contain "$open62541_words" "-lssl" "open62541.pc --static output"
assert_words_contain "$open62541_words" "-lcrypto" "open62541.pc --static output"
assert_words_contain "$open62541_words" "-lm" "open62541.pc --static output"
assert_words_contain "$opcua_words" "-lcpkt_opcua" "cpkt-opcua.pc --static output"
assert_words_contain "$opcua_words" "-lopen62541" "cpkt-opcua.pc --static output"
assert_words_contain "$opcua_words" "-lssl" "cpkt-opcua.pc --static output"
assert_words_contain "$opcua_words" "-lcrypto" "cpkt-opcua.pc --static output"
assert_words_contain "$opcua_words" "-lm" "cpkt-opcua.pc --static output"
assert_words_contain "$sus_words" "-lcpktsus" "cpkt-sus.pc --static output"
assert_words_contain "$sus_words" "-lwhisper" "cpkt-sus.pc --static output"
assert_words_contain "$sus_words" "-lggml" "cpkt-sus.pc --static output"
assert_words_contain "$sus_words" "-lggml-base" "cpkt-sus.pc --static output"
assert_words_contain "$sus_words" "-lggml-cpu" "cpkt-sus.pc --static output"
assert_words_contain "$sus_words" "-lcurl" "cpkt-sus.pc --static output"
assert_words_contain "$sus_words" "-lcrypto" "cpkt-sus.pc --static output"
assert_words_contain "$sus_words" "-lcpktaudio" "cpkt-sus.pc --static output"
assert_words_count "$sus_words" "-lcpktaudio" 1 "cpkt-sus.pc --static output"
assert_words_not_contain "$sus_words" "-lminiaudio" "cpkt-sus.pc --static output"
case "$target_id" in
  *-linux-*)
    assert_words_contain "$sus_words" "$pkg_config_libdir/../../lib/cpkt-cxx/libstdc++.a" "cpkt-sus.pc --static output"
    assert_words_contain "$sus_words" "$pkg_config_libdir/../../lib/cpkt-cxx/libgcc.a" "cpkt-sus.pc --static output"
    ;;
esac

cpkt_pkg_config_static_smoke() {
  pc_name=$1
  source_name=$2
  output_path="$work_root/bin/cpkt_pkg_${pc_name}"
  source_flags=$common_flags
  case "$source_name" in
    cpkt_audio_facade_strict.c|cpkt_audio_sus_facade_strict.c|cpkt_opcua_facade_strict.c|cpkt_sus_facade_strict.c)
      source_flags=$common_c89_flags
      ;;
  esac
  pkg_config_link_words=$(cpkt_pkg_config --static --cflags --libs "$pc_name")
  case "$target_id" in
    *-linux-gnu)
      pkg_config_link_words=$(printf '%s\n' "$pkg_config_link_words" | awk '
        BEGIN {
          bundled["-lcrypto"] = 1
          bundled["-lssl"] = 1
          bundled["-lz"] = 1
          bundled["-lnghttp2"] = 1
          bundled["-lssh2"] = 1
          bundled["-lcurl"] = 1
          bundled["-lxml2"] = 1
          bundled["-llua"] = 1
          bundled["-lcpkt_lua_runtime"] = 1
          bundled["-lmqttc"] = 1
          bundled["-lwhisper"] = 1
          bundled["-lggml"] = 1
          bundled["-lggml-base"] = 1
          bundled["-lggml-cpu"] = 1
          bundled["-lcpktsus"] = 1
          bundled["-lopen62541"] = 1
          bundled["-lcpkt_opcua"] = 1
        }
        {
          for (i = 1; i <= NF; ++i) {
            if ($i in bundled) {
              printf " -Wl,-Bstatic %s", $i
            } else if ($i ~ /^-l/ || $i == "-pthread") {
              printf " -Wl,-Bdynamic %s", $i
            } else {
              printf " %s", $i
            }
          }
          printf " -Wl,-Bdynamic"
        }')
      ;;
  esac
  cpkt_run_shell_checked "pkg-config static ${pc_name} build" \
    "\"$cc\" $pkg_config_static_flag $pkg_config_compile_toolchain_flags $source_flags \"$cmake_source_dir/$source_name\" \
    -o "$output_path" \
    $pkg_config_link_toolchain_flags \
    $pkg_config_link_words \
    $static_extra_libs"
}

cpkt_pkg_config_static_multi_smoke() {
  output_name=$1
  source_name=$2
  shift 2
  output_path="$work_root/bin/$output_name"
  source_flags=$common_flags
  case "$source_name" in
    cpkt_audio_facade_strict.c|cpkt_audio_sus_facade_strict.c|cpkt_opcua_facade_strict.c|cpkt_sus_facade_strict.c)
      source_flags=$common_c89_flags
      ;;
  esac
  pkg_config_link_words=$(cpkt_pkg_config --static --cflags --libs "$@")
  case "$target_id" in
    *-linux-gnu)
      pkg_config_link_words=$(printf '%s\n' "$pkg_config_link_words" | awk '
        BEGIN {
          bundled["-lcpktaudio"] = 1
          bundled["-lminiaudio"] = 1
          bundled["-lcrypto"] = 1
          bundled["-lssl"] = 1
          bundled["-lz"] = 1
          bundled["-lnghttp2"] = 1
          bundled["-lssh2"] = 1
          bundled["-lcurl"] = 1
          bundled["-lwhisper"] = 1
          bundled["-lggml"] = 1
          bundled["-lggml-base"] = 1
          bundled["-lggml-cpu"] = 1
          bundled["-lcpktsus"] = 1
        }
        {
          for (i = 1; i <= NF; ++i) {
            if ($i in bundled) {
              printf " -Wl,-Bstatic %s", $i
            } else if ($i ~ /^-l/ || $i == "-pthread") {
              printf " -Wl,-Bdynamic %s", $i
            } else {
              printf " %s", $i
            }
          }
          printf " -Wl,-Bdynamic"
        }')
      ;;
  esac
  cpkt_run_shell_checked "pkg-config static $output_name C-only build" \
    "\"$cc\" $pkg_config_static_flag $pkg_config_compile_toolchain_flags $source_flags \"$cmake_source_dir/$source_name\" \
    -o \"$output_path\" \
    $pkg_config_link_toolchain_flags \
    $pkg_config_link_words \
    $static_extra_libs"
}

cpkt_pkg_config_static_mixed_cxx_smoke() {
  output_path="$work_root/bin/cpkt_pkg_sus_mixed_cxx"
  c_source="$work_root/cpkt_sus_mixed_c_main.c"
  c_object="$work_root/cpkt_sus_mixed_c_main.o"
  cxx_source="$work_root/cpkt_sus_mixed_cxx_probe.cpp"
  cxx_object="$work_root/cpkt_sus_mixed_cxx_probe.o"
  pkg_config_link_words=$(cpkt_pkg_config --static --cflags --libs cpkt-sus)
  case "$target_id" in
    *-linux-gnu)
      pkg_config_link_words=$(printf '%s\n' "$pkg_config_link_words" | awk '
        BEGIN {
          bundled["-lcrypto"] = 1
          bundled["-lssl"] = 1
          bundled["-lz"] = 1
          bundled["-lnghttp2"] = 1
          bundled["-lssh2"] = 1
          bundled["-lcurl"] = 1
          bundled["-lwhisper"] = 1
          bundled["-lggml"] = 1
          bundled["-lggml-base"] = 1
          bundled["-lggml-cpu"] = 1
          bundled["-lcpktsus"] = 1
        }
        {
          for (i = 1; i <= NF; ++i) {
            if ($i in bundled) {
              printf " -Wl,-Bstatic %s", $i
            } else if ($i ~ /^-l/ || $i == "-pthread") {
              printf " -Wl,-Bdynamic %s", $i
            } else {
              printf " %s", $i
            }
          }
          printf " -Wl,-Bdynamic"
        }')
      ;;
  esac
  cat > "$c_source" <<'EOF'
#include <string.h>

#include <cpkt/sus.h>

int cpkt_sus_mixed_cxx_value(void);

int main(void) {
  const char *capabilities;

  capabilities = cpkt_sus_backend_capabilities();
  if (capabilities == 0 || strcmp(capabilities, "cpu") != 0) {
    return 1;
  }
  return cpkt_sus_mixed_cxx_value() == 8 ? 0 : 2;
}
EOF
  cat > "$cxx_source" <<'EOF'
#include <string>

extern "C" int cpkt_sus_mixed_cxx_value(void) {
  std::string value("cpkt-sus");
  return static_cast<int>(value.size());
}
EOF
  cpkt_run_shell_checked "pkg-config static cpkt-sus mixed C compile" \
    "\"$cc\" $pkg_config_compile_toolchain_flags $common_flags -c \"$c_source\" -o \"$c_object\" \
    $(cpkt_pkg_config --cflags cpkt-sus)"
  cpkt_run_shell_checked "pkg-config static cpkt-sus mixed C++ compile" \
    "\"$cxx\" $pkg_config_compile_toolchain_flags -std=c++11 -Wall -Wextra -Wpedantic -Werror -c \"$cxx_source\" -o \"$cxx_object\" \
    $(cpkt_pkg_config --cflags cpkt-sus)"
  cpkt_run_shell_checked "pkg-config static cpkt-sus mixed C-final link" \
    "\"$cc\" $pkg_config_static_flag $pkg_config_link_toolchain_flags \
    \"$c_object\" \"$cxx_object\" \
    -o \"$output_path\" \
    $pkg_config_link_words \
    $static_extra_libs"
}

cpkt_pkg_config_static_smoke zlib cpkt_zlib.c
cpkt_pkg_config_static_smoke libnghttp2 cpkt_nghttp2.c
cpkt_pkg_config_static_smoke libcrypto cpkt_crypto.c
cpkt_pkg_config_static_smoke libssl cpkt_ssl.c
cpkt_pkg_config_static_smoke openssl cpkt_ssl.c
cpkt_pkg_config_static_smoke libssh2 cpkt_libssh2.c
cpkt_pkg_config_static_smoke libcurl cpkt_curl.c
cpkt_pkg_config_static_smoke libxml-2.0 cpkt_libxml2.c
cpkt_pkg_config_static_smoke lua cpkt_lua.c
cpkt_pkg_config_static_smoke mqtt-c cpkt_mqttc.c
cpkt_pkg_config_static_smoke open62541 cpkt_open62541.c
cpkt_pkg_config_static_smoke cpkt-opcua cpkt_opcua_facade_strict.c
case "$target_id" in
  *-linux-*)
    cpkt_pkg_config_static_smoke cpkt-audio cpkt_audio_facade_strict.c
    cpkt_pkg_config_static_smoke cpkt-sus cpkt_sus_facade_strict.c
    cpkt_pkg_config_static_multi_smoke cpkt_pkg_audio_sus_facade cpkt_audio_sus_facade_strict.c cpkt-audio cpkt-sus
    cpkt_pkg_config_static_mixed_cxx_smoke
    ;;
esac

example_pkg_config_output="$work_root/bin/cpkt_example_pkg_config_consumer"
CPKT_SDK_PREFIX="$prefix" \
CC="$cc" \
CPKT_EXAMPLE_CFLAGS="$pkg_config_static_flag $pkg_config_compile_toolchain_flags $common_flags" \
CPKT_EXAMPLE_LDFLAGS="$pkg_config_link_toolchain_flags $example_runtime_ldflags $static_extra_libs" \
  cpkt_run_checked "example pkg-config consumer build" \
    "$installed_examples_dir/pkg-config-consumer/build.sh" "$example_pkg_config_output"

lua_runtime_example_pkg_file="$work_root/lua-runtime-c89-pkg-file.lua"
cat > "$lua_runtime_example_pkg_file" <<EOF
local host = require('example_host')
local chunk = require('example_chunk')
if host.context ~= 'example-context' then error('bad file context') end
if chunk.value ~= 'chunk' then error('bad file chunk') end
if package.path ~= 'zero/?.lua;first/?.lua' then error('bad package path') end
if package.cpath ~= 'zero/?.so;first/?.so' then error('bad package cpath') end
if example_name ~= 'facade' then error('bad string global') end
if example_flag ~= true then error('bad boolean global') end
if example_count ~= 7 then error('bad integer global') end
if example_number ~= 2.5 then error('bad number global') end
if required_side_effect ~= 'ok' then error('bad require side effect') end
if arg[0] ~= '$lua_runtime_example_pkg_file' then error('bad arg0') end
if arg[1] ~= 'one' or arg[2] ~= 'two' then error('bad argv') end
EOF
lua_runtime_example_pkg_config_output="$work_root/bin/cpkt_lua_runtime_c89_pkg_example"
CPKT_SDK_PREFIX="$prefix" \
CC="$cc" \
CPKT_EXAMPLE_CFLAGS="$pkg_config_compile_toolchain_flags" \
CPKT_EXAMPLE_LDFLAGS="$pkg_config_link_toolchain_flags $pkg_config_static_flag $example_runtime_ldflags $static_extra_libs" \
  cpkt_run_checked "lua runtime pkg-config example build" \
    "$installed_examples_dir/lua-runtime-c89/build-pkg-config.sh" "$lua_runtime_example_pkg_config_output"

opcua_example_pkg_config_output="$work_root/bin/cpkt_opcua_c89_pkg_example"
CPKT_SDK_PREFIX="$prefix" \
CC="$cc" \
CPKT_EXAMPLE_CFLAGS="$pkg_config_compile_toolchain_flags" \
CPKT_EXAMPLE_LDFLAGS="$pkg_config_link_toolchain_flags $pkg_config_static_flag $example_runtime_ldflags $static_extra_libs" \
  cpkt_run_checked "opcua pkg-config example build" \
    "$installed_examples_dir/opcua-c89/build-pkg-config.sh" "$opcua_example_pkg_config_output"

audio_sus_example_pkg_config_output="$work_root/bin/cpkt_audio_sus_c89_pkg_example"
CPKT_SDK_PREFIX="$prefix" \
CC="$cc" \
CPKT_EXAMPLE_CFLAGS="$pkg_config_compile_toolchain_flags" \
CPKT_EXAMPLE_LDFLAGS="$pkg_config_link_toolchain_flags $pkg_config_static_flag $example_runtime_ldflags $static_extra_libs" \
  cpkt_run_checked "audio sus pkg-config example build" \
    "$installed_examples_dir/audio-sus-c89/build-pkg-config.sh" "$audio_sus_example_pkg_config_output"

audio_vox_example_pkg_config_output="$work_root/bin/cpkt_audio_vox_intro_c89_pkg_example"
CPKT_SDK_PREFIX="$prefix" \
CC="$cc" \
CPKT_EXAMPLE_CFLAGS="$pkg_config_compile_toolchain_flags" \
CPKT_EXAMPLE_LDFLAGS="$pkg_config_link_toolchain_flags $pkg_config_static_flag $example_runtime_ldflags $static_extra_libs" \
  cpkt_run_checked "audio vox intro pkg-config example build" \
    "$installed_examples_dir/audio-vox-intro-c89/build-pkg-config.sh" "$audio_vox_example_pkg_config_output"

sus_vox_example_pkg_config_output="$work_root/bin/cpkt_sus_vox_intro_c89_pkg_example"
CPKT_SDK_PREFIX="$prefix" \
CC="$cc" \
CPKT_EXAMPLE_CFLAGS="$pkg_config_compile_toolchain_flags" \
CPKT_EXAMPLE_LDFLAGS="$pkg_config_link_toolchain_flags $pkg_config_static_flag $example_runtime_ldflags $static_extra_libs" \
  cpkt_run_checked "sus vox intro pkg-config example build" \
    "$installed_examples_dir/sus-vox-intro-c89/build-pkg-config.sh" "$sus_vox_example_pkg_config_output"

cpkt_pkg_config_smoke() {
  pc_name=$1
  source_name=$2
  output_path="$work_root/bin/cpkt_pkg_${pc_name}_default"
  cpkt_run_shell_checked "pkg-config default ${pc_name} build" \
    "\"$cc\" $pkg_config_compile_toolchain_flags $common_flags \"$cmake_source_dir/$source_name\" \
    -o "$output_path" \
    $pkg_config_link_toolchain_flags \
    $(cpkt_pkg_config --cflags --libs "$pc_name")"
}

cpkt_pkg_config_smoke openssl cpkt_ssl.c

if [ "$run_consumers" -eq 0 ]; then
  exit 0
fi

if [ -z "$run_prefix" ]; then
  case "$target_id" in
    *-linux-*)
      if [ -n "${LD_LIBRARY_PATH:-}" ]; then
        LD_LIBRARY_PATH="$prefix/lib:$LD_LIBRARY_PATH"
      else
        LD_LIBRARY_PATH="$prefix/lib"
      fi
      export LD_LIBRARY_PATH
      ;;
  esac
  "$cmake_build_dir/cpkt_cmake_zlib"
  "$cmake_build_dir/cpkt_cmake_nghttp2"
  "$cmake_build_dir/cpkt_cmake_crypto"
  "$cmake_build_dir/cpkt_cmake_ssl"
  "$cmake_build_dir/cpkt_cmake_libssh2"
  "$cmake_build_dir/cpkt_cmake_curl"
  "$cmake_build_dir/cpkt_cmake_libxml2"
  "$cmake_build_dir/cpkt_cmake_lua"
  "$cmake_build_dir/cpkt_cmake_mqttc"
  "$cmake_build_dir/cpkt_cmake_open62541"
  "$cmake_build_dir/cpkt_cmake_opcua_facade"
  "$cmake_build_dir/cpkt_cmake_lua_runtime_strict" "$cmake_build_dir/strict_file.lua"
  "$cmake_build_dir/cpkt_cmake_all"
  "$work_root/bin/cpkt_pkg_zlib"
  "$work_root/bin/cpkt_pkg_libnghttp2"
  "$work_root/bin/cpkt_pkg_libcrypto"
  "$work_root/bin/cpkt_pkg_libssl"
  "$work_root/bin/cpkt_pkg_openssl"
  "$work_root/bin/cpkt_pkg_libssh2"
  "$work_root/bin/cpkt_pkg_libcurl"
  "$work_root/bin/cpkt_pkg_libxml-2.0"
  "$work_root/bin/cpkt_pkg_lua"
  "$work_root/bin/cpkt_pkg_mqtt-c"
  "$work_root/bin/cpkt_pkg_open62541"
  "$work_root/bin/cpkt_pkg_cpkt-opcua"
  case "$target_id" in
    *-linux-*) "$work_root/bin/cpkt_pkg_sus_mixed_cxx" ;;
  esac
  "$example_cmake_build_dir/cpkt_bundle_cmake_consumer"
  "$example_pkg_config_output"
  "$lua_runtime_example_cmake_build_dir/cpkt_lua_runtime_c89_example" "$lua_runtime_example_cmake_build_dir/example_file.lua"
  "$lua_runtime_example_pkg_config_output" "$lua_runtime_example_pkg_file"
  "$opcua_example_cmake_build_dir/cpkt_opcua_c89_example"
  "$opcua_example_pkg_config_output"
  "$audio_sus_example_cmake_build_dir/cpkt_audio_sus_c89_example"
  "$audio_sus_example_pkg_config_output"
  "$audio_vox_example_cmake_build_dir/cpkt_audio_vox_intro_c89_example"
  "$audio_vox_example_pkg_config_output"
  "$sus_vox_example_cmake_build_dir/cpkt_sus_vox_intro_c89_example"
  "$sus_vox_example_pkg_config_output"
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
  $run_prefix "$cmake_build_dir/cpkt_cmake_libxml2"
  # shellcheck disable=SC2086
  $run_prefix "$cmake_build_dir/cpkt_cmake_lua"
  # shellcheck disable=SC2086
  $run_prefix "$cmake_build_dir/cpkt_cmake_mqttc"
  # shellcheck disable=SC2086
  $run_prefix "$cmake_build_dir/cpkt_cmake_open62541"
  # shellcheck disable=SC2086
  $run_prefix "$cmake_build_dir/cpkt_cmake_opcua_facade"
  # shellcheck disable=SC2086
  $run_prefix "$cmake_build_dir/cpkt_cmake_lua_runtime_strict" "$cmake_build_dir/strict_file.lua"
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
  $run_prefix "$work_root/bin/cpkt_pkg_libxml-2.0"
  # shellcheck disable=SC2086
  $run_prefix "$work_root/bin/cpkt_pkg_lua"
  # shellcheck disable=SC2086
  $run_prefix "$work_root/bin/cpkt_pkg_mqtt-c"
  # shellcheck disable=SC2086
  $run_prefix "$work_root/bin/cpkt_pkg_open62541"
  # shellcheck disable=SC2086
  $run_prefix "$work_root/bin/cpkt_pkg_cpkt-opcua"
  case "$target_id" in
    *-linux-*)
      # shellcheck disable=SC2086
      $run_prefix "$work_root/bin/cpkt_pkg_sus_mixed_cxx"
      ;;
  esac
  # shellcheck disable=SC2086
  $run_prefix "$example_cmake_build_dir/cpkt_bundle_cmake_consumer"
  # shellcheck disable=SC2086
  $run_prefix "$example_pkg_config_output"
  # shellcheck disable=SC2086
  $run_prefix "$lua_runtime_example_cmake_build_dir/cpkt_lua_runtime_c89_example" "$lua_runtime_example_cmake_build_dir/example_file.lua"
  # shellcheck disable=SC2086
  $run_prefix "$lua_runtime_example_pkg_config_output" "$lua_runtime_example_pkg_file"
  # shellcheck disable=SC2086
  $run_prefix "$opcua_example_cmake_build_dir/cpkt_opcua_c89_example"
  # shellcheck disable=SC2086
  $run_prefix "$opcua_example_pkg_config_output"
  # shellcheck disable=SC2086
  $run_prefix "$audio_sus_example_cmake_build_dir/cpkt_audio_sus_c89_example"
  # shellcheck disable=SC2086
  $run_prefix "$audio_sus_example_pkg_config_output"
  # shellcheck disable=SC2086
  $run_prefix "$audio_vox_example_cmake_build_dir/cpkt_audio_vox_intro_c89_example"
  # shellcheck disable=SC2086
  $run_prefix "$audio_vox_example_pkg_config_output"
  # shellcheck disable=SC2086
  $run_prefix "$sus_vox_example_cmake_build_dir/cpkt_sus_vox_intro_c89_example"
  # shellcheck disable=SC2086
  $run_prefix "$sus_vox_example_pkg_config_output"
fi
