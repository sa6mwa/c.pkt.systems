#!/usr/bin/env bash
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

help_output=$(make -C "$repo_root" help)

require_help_target() {
  target=$1
  if ! printf '%s\n' "$help_output" | grep -Eq "^[[:space:]]+$target[[:space:]]"; then
    printf 'make help does not list lifecycle target: %s\n' "$target" >&2
    exit 1
  fi
}

require_script() {
  path=$1
  if [ ! -x "$repo_root/$path" ]; then
    printf 'missing executable lifecycle script: %s\n' "$path" >&2
    exit 1
  fi
  bash -n "$repo_root/$path"
}

require_file_contains() {
  path=$1
  pattern=$2
  description=$3
  if ! grep -Eq "$pattern" "$repo_root/$path"; then
    printf '%s does not contain required lifecycle contract: %s\n' "$path" "$description" >&2
    exit 1
  fi
}

for target in \
  help deps-debug deps-release deps-cross build build-debug build-release \
  build-host cross-build test test-debug test-host test-cross cross-test test-all \
  test-install-tree asan tsan msan fuzz-smoke fuzz package package-source \
  package-source-smoke package-checksums package-verify verify-release-archives \
  verify-release-privacy release-matrix finalize-slice prerelease \
  prerelease-hardening release print-release-version format clean clean-dist; do
  require_help_target "$target"
done

for script in \
  scripts/build.sh \
  scripts/configure-preset.sh \
  scripts/test.sh \
  scripts/package.sh \
  scripts/run_linux_release_matrix.sh \
  scripts/clean.sh \
  scripts/release-version.sh \
  scripts/package-source.sh \
  scripts/source-archive-verify.sh \
  scripts/package-verify.sh; do
  require_script "$script"
done

grep -Eq '^/dist/$' "$repo_root/.gitignore"
grep -Eq '^/VERSION$' "$repo_root/.gitignore"
grep -Eq '^release-matrix:.*package-checksums.*package-verify' "$repo_root/Makefile"
require_file_contains \
  scripts/package.sh \
  'x86_64-linux-gnu-release x86_64-linux-musl-release aarch64-linux-gnu-release aarch64-linux-musl-release armhf-linux-gnu-release armhf-linux-musl-release' \
  'full Linux release preset matrix'
require_file_contains \
  scripts/package.sh \
  'package-arm64-apple-darwin-release' \
  'optional arm64 Darwin package target'
require_file_contains \
  scripts/package-verify.sh \
  'x86_64-linux-gnu x86_64-linux-musl aarch64-linux-gnu aarch64-linux-musl armhf-linux-gnu armhf-linux-musl' \
  'full Linux package verification matrix'
require_file_contains \
  CMakeLists.txt \
  'static_archive_pic_link' \
  'build-tree static archive PIC smoke'
require_file_contains \
  scripts/package-install-smoke.sh \
  'cpkt_add_static_archive_pic_smoke' \
  'install-tree static archive PIC smoke'
