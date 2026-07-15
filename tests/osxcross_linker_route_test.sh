#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

if ! bash "$repo_root/scripts/osxcross_available.sh"; then
  printf '[test] osxcross linker route skipped: osxcross toolchain not available\n'
  exit 0
fi

osxcross_root=${OSXCROSS_ROOT:-"$HOME/.local/cross/osxcross"}
osxcross_host=${CPKT_OSXCROSS_HOST:-arm64-apple-darwin25}
osxcross_bin="$osxcross_root/bin"
compiler="$osxcross_bin/$osxcross_host-clang"
linker="$osxcross_bin/$osxcross_host-ld"
clean_path=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

work_dir=$(mktemp -d "${TMPDIR:-/tmp}/cpkt-osxcross-linker-route.XXXXXXXXXX")
cleanup() {
  rm -rf "$work_dir"
}
trap cleanup EXIT HUP INT TERM

source_file="$work_dir/main.c"
binary_file="$work_dir/main"
printf 'int main(void) { return 0; }\n' > "$source_file"

assert_contains() {
  haystack=$1
  needle=$2
  description=$3
  case "$haystack" in
    *"$needle"*) ;;
    *)
      printf 'osxcross linker route did not contain %s: %s\n%s\n' "$description" "$needle" "$haystack" >&2
      exit 1
      ;;
  esac
}

bad_route_output=$(PATH="$clean_path" "$compiler" -### "$source_file" -o "$binary_file" 2>&1)
assert_contains "$bad_route_output" '"/usr/bin/ld"' "the known host-linker failure route"

path_route_output=$(PATH="$osxcross_bin:$clean_path" "$compiler" -### "$source_file" -o "$binary_file" 2>&1)
assert_contains "$path_route_output" "\"$linker\"" "the target linker when osxcross bin is first on PATH"

ld_path_route_output=$(PATH="$clean_path" "$compiler" "--ld-path=$linker" -### "$source_file" -o "$binary_file" 2>&1)
assert_contains "$ld_path_route_output" "\"$linker\"" "the target linker when --ld-path is absolute"

if ! grep -F 'set(ENV{PATH} "${CPKT_OSXCROSS_BIN_DIR}:$ENV{PATH}")' \
    "$repo_root/cmake/toolchains/arm64-apple-darwin.cmake" >/dev/null 2>&1; then
  printf 'Darwin toolchain must prepend osxcross bin to PATH before configure/build probes\n' >&2
  exit 1
fi

if ! grep -F 'set(_cpkt_darwin_linker_flag "--ld-path=${CMAKE_LINKER}")' \
    "$repo_root/cmake/toolchains/arm64-apple-darwin.cmake" >/dev/null 2>&1; then
  printf 'Darwin toolchain must force an absolute target linker with --ld-path\n' >&2
  exit 1
fi

if ! grep -F 'string(REGEX REPLACE "(^| )--ld-path=[^ ]+" " " _cpkt_existing_linker_flags' \
    "$repo_root/cmake/toolchains/arm64-apple-darwin.cmake" >/dev/null 2>&1; then
  printf 'Darwin toolchain must remove stale --ld-path linker routes from existing CMake caches\n' >&2
  exit 1
fi

if ! grep -F 'string(REGEX REPLACE "(^| )-fuse-ld=[^ ]+" " " _cpkt_existing_linker_flags' \
    "$repo_root/cmake/toolchains/arm64-apple-darwin.cmake" >/dev/null 2>&1; then
  printf 'Darwin toolchain must remove its legacy -fuse-ld linker path from existing CMake caches\n' >&2
  exit 1
fi

printf '[test] osxcross linker route passed\n'
