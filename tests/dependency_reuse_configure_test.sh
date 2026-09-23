#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  printf 'usage: %s <source-dir> <cmake>\n' "$0" >&2
  exit 2
fi

source_dir=$1
cmake_command=$2
work_dir=$(mktemp -d "$source_dir/build/dependency-reuse.XXXXXXXX")
trap '"$cmake_command" -E remove_directory "$work_dir"' EXIT HUP INT TERM

cd "$source_dir"
"$cmake_command" --preset debug -B "$work_dir" \
  -DCPKT_BUILD_DEPENDENCIES=OFF > "$work_dir/configure.log" 2>&1 || {
    cat "$work_dir/configure.log" >&2
    exit 1
  }

if ! grep -Fq 'Generating done' "$work_dir/configure.log"; then
  cat "$work_dir/configure.log" >&2
  printf 'dependency-reuse configure did not generate a build graph\n' >&2
  exit 1
fi
