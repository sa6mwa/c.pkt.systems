#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

CMAKE=${CMAKE:-cmake}
CTEST=${CTEST:-ctest}
mode=${1:-release}

release_presets="x86_64-linux-gnu-release x86_64-linux-musl-release aarch64-linux-gnu-release aarch64-linux-musl-release armhf-linux-gnu-release armhf-linux-musl-release"

build_and_test_preset() {
  preset=$1
  "$CMAKE" --preset "$preset"
  "$CMAKE" --build --preset "$preset"
  "$CTEST" --preset "$preset"
}

cd "$repo_root"

case "$mode" in
  debug | host)
    build_and_test_preset debug
    ;;
  release | all)
    for preset in $release_presets; do
      build_and_test_preset "$preset"
    done
    ;;
  *)
    build_and_test_preset "$mode"
    ;;
esac
