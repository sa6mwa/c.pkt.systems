#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  printf 'usage: dependency_cache_disabled_test.sh <source-dir> <external-root>\n' >&2
  exit 2
fi

source_dir=$1
external_root=$2
work_dir=$(mktemp -d "$source_dir/build/dependency-cache-disabled.XXXXXXXXXX")
trap 'rm -rf "$work_dir"' EXIT HUP INT TERM

env -u HOME -u XDG_CACHE_HOME -u CPKT_DEPENDENCY_CACHE \
  cmake \
    -S "$source_dir" \
    -B "$work_dir" \
    -DCPKT_BUILD_DEPENDENCIES=OFF \
    -DCPKT_BUILD_TESTS=OFF \
    -DCPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE=ON \
    -DCPKT_EXTERNAL_ROOT="$external_root"
