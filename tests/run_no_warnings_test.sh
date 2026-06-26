#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

require_file_contains() {
  file_path=$1
  expected=$2
  description=$3

  if ! grep -F -- "$expected" "$file_path" >/dev/null 2>&1; then
    printf '%s\nmissing: %s\nin: %s\n' "$description" "$expected" "$file_path" >&2
    exit 1
  fi
}

require_file_lacks() {
  file_path=$1
  unexpected=$2
  description=$3

  if grep -F -- "$unexpected" "$file_path" >/dev/null 2>&1; then
    printf '%s\nunexpected: %s\nin: %s\n' "$description" "$unexpected" "$file_path" >&2
    exit 1
  fi
}

bash "$repo_root/scripts/run-no-warnings.sh" \
  "clean command" \
  sh -c 'printf "%s\n" "clean output"' >/dev/null

work_root=$(mktemp -d "${TMPDIR:-/tmp}/cpkt-no-warnings-test.XXXXXX")
trap 'rm -rf "$work_root"' EXIT

if bash "$repo_root/scripts/run-no-warnings.sh" \
    "warning command" \
    sh -c 'printf "%s\n" "ld: warning: duplicate symbol"' \
    >"$work_root/stdout" 2>"$work_root/stderr"; then
  printf 'warning command unexpectedly passed\n' >&2
  exit 1
fi

if ! grep -F 'warning command emitted warnings' "$work_root/stderr" >/dev/null 2>&1; then
  printf 'warning failure did not include the command description\n' >&2
  cat "$work_root/stderr" >&2
  exit 1
fi

require_file_lacks \
  "$repo_root/Makefile" \
  'bash ./scripts/run-no-warnings.sh "build $$preset" $(CMAKE) --build --preset "$$preset";' \
  'release/build targets must not scan full upstream dependency build logs for warnings'

require_file_lacks \
  "$repo_root/Makefile" \
  'bash ./scripts/run-no-warnings.sh "package $$preset" $(CMAKE) --build --preset "package-$$preset";' \
  'release/package targets must not scan full upstream dependency build logs for warnings'

require_file_lacks \
  "$repo_root/Makefile" \
  'bash ./scripts/run-no-warnings.sh "build arm64-apple-darwin-release" $(CMAKE) --build --preset arm64-apple-darwin-release;' \
  'Darwin release builds must not scan full upstream dependency build logs for warnings'

require_file_lacks \
  "$repo_root/Makefile" \
  'bash ./scripts/run-no-warnings.sh "package arm64-apple-darwin-release" $(CMAKE) --build --preset package-arm64-apple-darwin-release;' \
  'Darwin package builds must not scan full upstream dependency build logs for warnings'

require_file_contains \
  "$repo_root/CMakeLists.txt" \
  'target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -Werror)' \
  'repo-owned C targets must compile with warning errors on GCC/Clang'

require_file_contains \
  "$repo_root/CMakeLists.txt" \
  'target_link_options(${target_name} PRIVATE LINKER:--fatal-warnings)' \
  'repo-owned C targets must link with fatal linker warnings on ELF targets'

require_file_contains \
  "$repo_root/CMakeLists.txt" \
  'target_link_options(${target_name} PRIVATE LINKER:-fatal_warnings)' \
  'repo-owned C targets must link with fatal linker warnings on Darwin targets'

for facade_target in \
    cpkt_lua_runtime_static \
    cpkt_lua_runtime_shared \
    cpkt_opcua_static \
    cpkt_opcua_shared \
    cpkt_lua_runtime_test \
    cpkt_opcua_facade_test; do
  require_file_contains \
    "$repo_root/CMakeLists.txt" \
    "cpkt_add_repo_warning_errors($facade_target)" \
    "facade target must opt into repo-owned warning errors: $facade_target"
done

if ! grep -F 'ld: warning: duplicate symbol' "$work_root/stderr" >/dev/null 2>&1; then
  printf 'warning failure did not include command output\n' >&2
  cat "$work_root/stderr" >&2
  exit 1
fi
