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

symlink_leak="$work_root/symlink-leak"
ln -s "$repo_root/private-target" "$symlink_leak"
expect_privacy_failure "$symlink_leak" "release artifact contains private trace CPKT_ROOT"

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

printf '[test] privacy scan failure modes passed\n'
