#!/usr/bin/env bash
set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
workspace_parent="$repo_root/.cache/package-assertions"

mkdir -p "$workspace_parent"
workspace=$(mktemp -d "$workspace_parent/assertion.XXXXXXXX")

cleanup() {
  rm -rf -- "$workspace"
  rmdir "$workspace_parent" 2>/dev/null || true
}
trap cleanup EXIT HUP INT TERM

cmake \
  "$@" \
  -DCPKT_ASSERTION_WORK_ROOT="$workspace" \
  -P "$repo_root/cmake/package_assertions.cmake"
