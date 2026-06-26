#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

for checked_path in \
  scripts/source-archive-verify.sh \
  tests/source_archive_git_ignore_test.sh \
  tests/source_archive_git_parent_test.sh
do
  if grep -- '-printf' "$repo_root/$checked_path" >/dev/null 2>&1; then
    printf 'source archive verification must not depend on GNU find -printf: %s\n' "$checked_path" >&2
    exit 1
  fi
done

printf '[test] source archive portable file listing passed\n'
