#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

work_root=$(mktemp -d "${TMPDIR:-/tmp}/cpkt-package-smoke-args.XXXXXX")
trap 'rm -rf "$work_root"' EXIT

osxcross_root="$work_root/osxcross"
osxcross_host="arm64-apple-darwin-test"
mkdir -p "$osxcross_root/bin"
touch "$osxcross_root/bin/$osxcross_host-clang"
chmod +x "$osxcross_root/bin/$osxcross_host-clang"

actual=$(
  OSXCROSS_ROOT="$osxcross_root" \
  CPKT_OSXCROSS_HOST="$osxcross_host" \
  CPKT_MACOS_DEPLOYMENT_TARGET=14.2 \
  CPKT_PACKAGE_INSTALL_SMOKE_PRINT_CMAKE_TOOLCHAIN_ARGS=1 \
    bash "$repo_root/scripts/package-install-smoke.sh" \
      "$work_root/unused.tar.gz" \
      arm64-apple-darwin \
      "$repo_root/examples/abi_smoke.c"
)

assert_line() {
  expected=$1
  if ! printf '%s\n' "$actual" | grep -F -x -- "$expected" >/dev/null 2>&1; then
    printf 'expected CMake smoke configure argument was not emitted: %s\n' "$expected" >&2
    printf 'actual arguments:\n%s\n' "$actual" >&2
    exit 1
  fi
}

assert_line "-DCMAKE_TOOLCHAIN_FILE=$repo_root/cmake/toolchains/arm64-apple-darwin.cmake"
assert_line "-G"
assert_line "Unix Makefiles"
assert_line "-DCPKT_OSXCROSS_ROOT=$osxcross_root"
assert_line "-DCPKT_OSXCROSS_HOST=$osxcross_host"
assert_line "-DCPKT_MACOS_DEPLOYMENT_TARGET=14.2"

package_pc_dir="$work_root/package-pc"
host_pc_dir="$work_root/host-pc"
mkdir -p "$package_pc_dir" "$host_pc_dir"
cat > "$package_pc_dir/cpkt-smoke-isolation.pc" <<'EOF'
prefix=/package-prefix
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: cpkt-smoke-isolation
Description: package pkg-config isolation fixture
Version: 1.0.0
Cflags: -I${includedir}/package-fixture
Libs: -L${libdir} -lpackage-fixture
EOF
cat > "$host_pc_dir/cpkt-smoke-isolation.pc" <<'EOF'
prefix=/host-prefix
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: cpkt-smoke-isolation
Description: host pkg-config contamination fixture
Version: 1.0.0
Cflags: -I${includedir}/host-fixture
Libs: -L${libdir} -lhost-fixture
EOF
touch "$work_root/unused.tar.gz" "$work_root/source.c"

pkg_config_actual=$(
  PKG_CONFIG_PATH="$host_pc_dir" \
  CPKT_PACKAGE_INSTALL_SMOKE_PRINT_PKG_CONFIG_WORDS=1 \
  CPKT_PACKAGE_INSTALL_SMOKE_PKG_CONFIG_LIBDIR="$package_pc_dir" \
    bash "$repo_root/scripts/package-install-smoke.sh" \
      "$work_root/unused.tar.gz" \
      x86_64-linux-gnu \
      "$work_root/source.c"
)

if ! printf '%s\n' "$pkg_config_actual" | grep -F -- '-lpackage-fixture' >/dev/null 2>&1; then
  printf 'pkg-config smoke did not resolve package metadata with PKG_CONFIG_PATH set\n' >&2
  printf 'actual pkg-config words: %s\n' "$pkg_config_actual" >&2
  exit 1
fi
if printf '%s\n' "$pkg_config_actual" | grep -F -- '-lhost-fixture' >/dev/null 2>&1; then
  printf 'pkg-config smoke used host PKG_CONFIG_PATH metadata instead of isolated package metadata\n' >&2
  printf 'actual pkg-config words: %s\n' "$pkg_config_actual" >&2
  exit 1
fi
