#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
deps_file="$repo_root/cmake/CpktDependencies.cmake"

assert_contains() {
  needle=$1
  description=$2
  if ! grep -F -- "$needle" "$deps_file" >/dev/null 2>&1; then
    printf 'Darwin dependency metadata config is missing %s: %s\n' "$description" "$needle" >&2
    exit 1
  fi
}

assert_not_contains() {
  needle=$1
  description=$2
  if grep -F -- "$needle" "$deps_file" >/dev/null 2>&1; then
    printf 'Darwin dependency metadata config still contains %s: %s\n' "$description" "$needle" >&2
    exit 1
  fi
}

assert_contains "-DCPKT_STRIP_SHARED_LIBRARIES=\${_strip_shared_libraries}" "strip helper shared-library policy"
assert_contains "-DCMAKE_INSTALL_NAME_DIR=@rpath" "link-time install name"
assert_contains "-DCMAKE_BUILD_WITH_INSTALL_NAME_DIR=ON" "build-time install-name use"
assert_contains "-DCMAKE_BUILD_WITH_INSTALL_RPATH=ON" "build-time install rpath use"
assert_contains "-DCPKT_DARWIN_INSTALL_NAME_FILE=\${source_dir}/Makefile" "OpenSSL generated install-name patch"
assert_contains "-DCPKT_DARWIN_INSTALL_NAME_FILE=\${build_dir}/libtool" "nghttp2 generated install-name patch"
assert_contains "patch_darwin_generated_install_names.cmake" "generated install-name patch script"
assert_not_contains "CPKT_DARWIN_DEPENDENCY_ROOT" "post-install Darwin dependency rewrite"

printf '[test] Darwin dependency metadata config passed\n'
