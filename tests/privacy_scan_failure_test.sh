#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

work_root=$(mktemp -d "${TMPDIR:-/tmp}/cpkt-privacy-scan-test.XXXXXXXXXX")
cleanup() {
  rm -rf "$work_root"
}
trap cleanup EXIT HUP INT TERM

expect_privacy_failure() {
  artifact_path=$1
  expected_message=$2
  stderr_path="$work_root/$(basename -- "$artifact_path").stderr"

  if (
      cd "$work_root"
      cmake \
        -DCPKT_ROOT="$repo_root" \
        -DCPKT_SCAN_LABEL="privacy fixture" \
        -DCPKT_SCAN_PATHS="$artifact_path" \
        -P "$repo_root/tests/privacy_scan.cmake"
    ) >"$work_root/privacy.stdout" 2>"$stderr_path"; then
    printf 'privacy scan accepted private trace fixture: %s\n' "$artifact_path" >&2
    exit 1
  fi
  if ! grep -F -- "$expected_message" "$stderr_path" >/dev/null 2>&1; then
    printf 'privacy scan failure did not include expected message: %s\n' "$expected_message" >&2
    printf 'actual stderr:\n' >&2
    cat "$stderr_path" >&2
    exit 1
  fi
}

clean_file="$work_root/clean.txt"
printf 'release payload without workstation paths\n' > "$clean_file"
(
  cd "$work_root"
  cmake \
    -DCPKT_ROOT="$repo_root" \
    -DCPKT_SCAN_LABEL="privacy clean fixture" \
    -DCPKT_SCAN_PATHS="$clean_file" \
    -P "$repo_root/tests/privacy_scan.cmake"
)

raw_leak="$work_root/raw-leak.txt"
printf 'private source path: %s\n' "$repo_root" > "$raw_leak"
expect_privacy_failure "$raw_leak" "release artifact contains private trace CPKT_ROOT"

if [ -n "${HOME:-}" ]; then
  home_leak="$work_root/home-leak.txt"
  printf 'private home path: %s\n' "$HOME" > "$home_leak"
  expect_privacy_failure "$home_leak" "release artifact contains private trace HOME"
fi

root_file_url_leak="$work_root/root-file-url-leak.txt"
printf 'private source file URL: file://%s/model.bin\n' "$repo_root" \
  > "$root_file_url_leak"
expect_privacy_failure "$root_file_url_leak" \
  "release artifact contains private trace CPKT_ROOT_FILE_URL"

if [ -n "${HOME:-}" ]; then
  home_file_url_leak="$work_root/home-file-url-leak.txt"
  printf 'private home file URL: file://%s/cache.bin\n' "$HOME" \
    > "$home_file_url_leak"
  expect_privacy_failure "$home_file_url_leak" \
    "release artifact contains private trace HOME_FILE_URL"
fi

symlink_leak="$work_root/symlink-leak"
ln -s "$repo_root/private-target" "$symlink_leak"
expect_privacy_failure "$symlink_leak" "release artifact contains private trace CPKT_ROOT"

if [ -n "${HOME:-}" ]; then
  symlink_home_url_leak="$work_root/symlink-home-file-url-leak"
  ln -s "file://$HOME/private-target" "$symlink_home_url_leak"
  expect_privacy_failure "$symlink_home_url_leak" \
    "release artifact contains private trace HOME_FILE_URL"
fi

archive_stage="$work_root/archive-stage"
mkdir -p "$archive_stage/c.pkt.systems-privacy-fixture"
printf 'nested private source path: %s\n' "$repo_root" \
  > "$archive_stage/c.pkt.systems-privacy-fixture/leak.txt"
archive_leak="$work_root/archive-leak.tar.gz"
(
  cd "$archive_stage"
  tar -czf "$archive_leak" -- c.pkt.systems-privacy-fixture
)
expect_privacy_failure "$archive_leak" "release artifact contains private trace CPKT_ROOT"

archive_file_url_stage="$work_root/archive-file-url-stage"
mkdir -p "$archive_file_url_stage/c.pkt.systems-privacy-fixture"
printf 'nested private source file URL: file://%s/archive.bin\n' "$repo_root" \
  > "$archive_file_url_stage/c.pkt.systems-privacy-fixture/file-url-leak.txt"
archive_file_url_leak="$work_root/archive-file-url-leak.tar.gz"
(
  cd "$archive_file_url_stage"
  tar -czf "$archive_file_url_leak" -- c.pkt.systems-privacy-fixture
)
expect_privacy_failure "$archive_file_url_leak" \
  "release artifact contains private trace CPKT_ROOT_FILE_URL"

nested_archive_stage="$work_root/nested-archive-stage"
nested_inner_stage="$work_root/nested-inner-stage"
mkdir -p "$nested_archive_stage/c.pkt.systems-privacy-fixture"
mkdir -p "$nested_inner_stage/private-inner"
printf 'deep private source path: %s\n' "$repo_root" \
  > "$nested_inner_stage/private-inner/deep-leak.txt"
nested_inner_archive="$nested_archive_stage/c.pkt.systems-privacy-fixture/inner.tar.gz"
(
  cd "$nested_inner_stage"
  tar -czf "$nested_inner_archive" -- private-inner
)
nested_archive_leak="$work_root/nested-archive-leak.tar.gz"
(
  cd "$nested_archive_stage"
  tar -czf "$nested_archive_leak" -- c.pkt.systems-privacy-fixture
)
expect_privacy_failure "$nested_archive_leak" \
  "release artifact contains private trace CPKT_ROOT"

printf '[test] privacy scan failure modes passed\n'
