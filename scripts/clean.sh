#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

clean_one() {
  path=$1
  case "$path" in
    "$repo_root/build" | "$repo_root/.cache" | "$repo_root/dist")
      rm -rf "$path"
      ;;
    *)
      printf 'refusing to remove unsafe generated path: %s\n' "$path" >&2
      exit 1
      ;;
  esac
}

clean_legacy_package_assertions() {
  shopt -s nullglob
  for path in "$repo_root"/package-assertions-*; do
    if [ -d "$path" ]; then
      rm -rf -- "$path"
    fi
  done
}

mode=${1:-all}

case "$mode" in
  all)
    clean_legacy_package_assertions
    clean_one "$repo_root/build"
    clean_one "$repo_root/.cache"
    clean_one "$repo_root/dist"
    ;;
  dist)
    clean_one "$repo_root/dist"
    ;;
  *)
    printf 'usage: %s [all|dist]\n' "$0" >&2
    exit 2
    ;;
esac
