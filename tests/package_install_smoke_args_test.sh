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

if ! grep -F -- 'pkg_config_compile_toolchain_flags="$pkg_config_toolchain_flags"' "$repo_root/scripts/package-install-smoke.sh" >/dev/null 2>&1; then
  printf 'package smoke no longer keeps Darwin compile toolchain flags separate\n' >&2
  exit 1
fi
if ! grep -F -- 'pkg_config_link_toolchain_flags="--ld-path=$osxcross_root/bin/$osxcross_host-ld $pkg_config_toolchain_flags"' "$repo_root/scripts/package-install-smoke.sh" >/dev/null 2>&1; then
  printf 'package smoke no longer keeps Darwin --ld-path in link-only flags\n' >&2
  exit 1
fi
if grep -F -- 'CPKT_EXAMPLE_CFLAGS="$pkg_config_toolchain_flags' "$repo_root/scripts/package-install-smoke.sh" >/dev/null 2>&1 ||
    grep -F -- '"$cc" $pkg_config_static_flag $pkg_config_toolchain_flags' "$repo_root/scripts/package-install-smoke.sh" >/dev/null 2>&1 ||
    grep -F -- '"$cc" $pkg_config_toolchain_flags' "$repo_root/scripts/package-install-smoke.sh" >/dev/null 2>&1; then
  printf 'package smoke still passes Darwin link-only toolchain flags to compile commands\n' >&2
  exit 1
fi

for expected_compile_options in \
  'target_compile_options("\${target_name}" PRIVATE -Wall -Wextra -Wpedantic -Werror)' \
  'target_compile_options(cpkt_cmake_all PRIVATE -Wall -Wextra -Wpedantic -Werror)' \
  'target_compile_options($executable_name PRIVATE -Wall -Wextra -Wpedantic -Werror)' \
  'COMPILE_OPTIONS "-std=c89;-Wall;-Wextra;-Wpedantic;-Werror")' \
  'COMPILE_OPTIONS "-std=c99;-Wall;-Wextra;-Wpedantic;-Werror")'
do
  if ! grep -F -- "$expected_compile_options" "$repo_root/scripts/package-install-smoke.sh" >/dev/null 2>&1; then
    printf 'package smoke no longer compiles facade strict sources with: %s\n' "$expected_compile_options" >&2
    exit 1
  fi
done

if ! grep -F -- 'installed_examples_dir="$prefix/share/doc/c.pkt.systems/examples"' \
    "$repo_root/scripts/package-install-smoke.sh" >/dev/null 2>&1; then
  printf 'package smoke no longer builds examples from the extracted SDK docs tree\n' >&2
  exit 1
fi
if grep -F -- '-S "$repo_root/examples/' "$repo_root/scripts/package-install-smoke.sh" >/dev/null 2>&1 ||
    grep -F -- '"$repo_root/examples/' "$repo_root/scripts/package-install-smoke.sh" >/dev/null 2>&1; then
  printf 'package smoke still builds installed-example checks from the source tree\n' >&2
  exit 1
fi

if grep -F -- 'UA_Variant|uint|int32|int64|long long' "$repo_root/scripts/package-install-smoke.sh" >/dev/null 2>&1; then
  printf 'package smoke must not reject C89-safe facade identifiers such as cpkt_opcua_uint64\n' >&2
  exit 1
fi
if ! grep -F -- 'uint64_t' "$repo_root/scripts/package-install-smoke.sh" >/dev/null 2>&1; then
  printf 'package smoke no longer rejects C99 fixed-width integer typedef leaks\n' >&2
  exit 1
fi
if ! grep -F -- 'cpkt_opcua_server_new_from_json(&server, json_config, sizeof(json_config) - 1, &status)' \
    "$repo_root/scripts/package-install-smoke.sh" >/dev/null 2>&1; then
  printf 'package smoke no longer exercises the OPC UA JSON server constructor from a strict C89 consumer\n' >&2
  exit 1
fi

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

prefix_fixture="$work_root/prefix-fixture"
mkdir -p "$prefix_fixture/c.pkt.systems-1.2.3-x86_64-linux-gnu"
prefix_archive="$work_root/prefix-fixture.tar.gz"
(cd "$prefix_fixture" && tar -czf "$prefix_archive" -- c.pkt.systems-1.2.3-x86_64-linux-gnu)

prefix_actual=$(
  CPKT_PACKAGE_INSTALL_SMOKE_PRINT_EXTRACTED_PREFIX=1 \
    bash "$repo_root/scripts/package-install-smoke.sh" \
      "$prefix_archive" \
      x86_64-linux-gnu \
      "$work_root/source.c"
)

if [ "$prefix_actual" != "c.pkt.systems-1.2.3-x86_64-linux-gnu" ]; then
  printf 'package smoke selected the wrong extracted prefix: %s\n' "$prefix_actual" >&2
  exit 1
fi

multi_root_fixture="$work_root/multi-root-fixture"
mkdir -p "$multi_root_fixture/sdk-one" "$multi_root_fixture/sdk-two"
multi_root_archive="$work_root/multi-root-fixture.tar.gz"
(cd "$multi_root_fixture" && tar -czf "$multi_root_archive" -- sdk-one sdk-two)

if CPKT_PACKAGE_INSTALL_SMOKE_PRINT_EXTRACTED_PREFIX=1 \
    bash "$repo_root/scripts/package-install-smoke.sh" \
      "$multi_root_archive" \
      x86_64-linux-gnu \
      "$work_root/source.c" >/dev/null 2>&1; then
  printf 'package smoke accepted an archive with multiple top-level SDK roots\n' >&2
  exit 1
fi
