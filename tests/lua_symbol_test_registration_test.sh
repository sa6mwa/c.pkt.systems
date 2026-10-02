#!/usr/bin/env bash
set -euo pipefail

repo_root=${1:?repository root is required}
build_dir="$repo_root/build/lua-symbol-test-registration"

cleanup() {
  cmake -E remove_directory "$build_dir"
}
trap cleanup EXIT INT TERM
cleanup

cd "$repo_root"
cmake --preset arm64-apple-darwin-release -B "$build_dir" >/dev/null
registered=$(ctest --test-dir "$build_dir" -N)
for test_name in lua_api_inventory lua_export_policy; do
  case "$registered" in
    *"$test_name"*) ;;
    *)
      printf 'Darwin does not register %s without a target runner\n' \
        "$test_name" >&2
      exit 1
      ;;
  esac
done
