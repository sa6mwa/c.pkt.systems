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
  debug|release|asan|tsan|msan|fuzz|opcua-fuzz|x86_64-linux-gnu-release|x86_64-linux-musl-release|aarch64-linux-gnu-release|aarch64-linux-musl-release|armhf-linux-gnu-release|armhf-linux-musl-release|arm64-apple-darwin-release)
    ;;
  *)
    printf 'unknown configure preset: %s\n' "$preset" >&2
    exit 2
    ;;
esac

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir="$repo_root/build/$preset"

if [ "$fresh" -eq 1 ]; then
  rm -f "$build_dir/CMakeCache.txt"
  rm -rf "$build_dir/CMakeFiles"
fi

cmake --preset "$preset"
