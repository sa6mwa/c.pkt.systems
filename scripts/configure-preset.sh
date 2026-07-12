#!/usr/bin/env bash
set -eu

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
  printf 'usage: %s [--fresh] <preset>\n' "$0" >&2
  exit 2
fi

fresh=0
if [ "$1" = "--fresh" ]; then
  fresh=1
  shift
fi

preset=$1
case "$preset" in
  debug|release|valgrind|fuzz|opcua-fuzz|x86_64-linux-gnu-release|x86_64-linux-musl-release|aarch64-linux-gnu-release|aarch64-linux-musl-release|armhf-linux-gnu-release|armhf-linux-musl-release|arm64-apple-darwin-release)
    ;;
  *)
    printf 'unknown configure preset: %s\n' "$preset" >&2
    exit 2
    ;;
esac

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir="$repo_root/build/$preset"
cmake_args=(--preset "$preset")

remove_generated_glob() {
  for path in "$@"; do
    if [ -e "$path" ]; then
      rm -rf "$path"
    fi
  done
}

if [ "$fresh" -eq 1 ]; then
  rm -f "$build_dir/CMakeCache.txt"
  rm -rf "$build_dir/CMakeFiles"
  case "$preset" in
    fuzz|opcua-fuzz)
      remove_generated_glob \
        "$repo_root"/.cache/deps-build/x86_64-linux-gnu/*/AFL_* \
        "$repo_root"/.cache/deps/x86_64-linux-gnu/*/AFL_*
      ;;
  esac
fi

if [ "$preset" = "opcua-fuzz" ]; then
  debug_cache="$repo_root/build/debug/CMakeCache.txt"
  if [ ! -f "$debug_cache" ]; then
    cmake --preset debug
  fi
  external_root=$(sed -n 's/^CPKT_EXTERNAL_ROOT:PATH=//p' "$debug_cache" | tail -n 1)
  dependency_build_root=$(sed -n 's/^CPKT_DEPENDENCY_BUILD_ROOT:PATH=//p' "$debug_cache" | tail -n 1)
  if [ -z "$external_root" ] || [ -z "$dependency_build_root" ]; then
    printf 'failed to read dependency roots from %s\n' "$debug_cache" >&2
    exit 1
  fi
  cmake_args+=(
    -DCPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE=ON
    -DCPKT_BUILD_DEPENDENCIES=OFF
    -DCPKT_EXTERNAL_ROOT="$external_root"
    -DCPKT_DEPENDENCY_BUILD_ROOT="$dependency_build_root"
  )
fi

cmake "${cmake_args[@]}"
