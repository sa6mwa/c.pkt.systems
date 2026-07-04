#!/usr/bin/env bash
set -euo pipefail

tool="${1:?cpktxscribe binary is required}"
build_dir="${2:?build directory is required}"
url="${CPKTXSCRIBE_E2E_URL:-https://pkt.systems/trajectory/assets/narration/intro/intro.mp3}"
out_dir="$build_dir/cpktxscribe-e2e"
stdout_path="$out_dir/stdout.txt"
stderr_path="$out_dir/stderr.txt"

mkdir -p "$out_dir"

"$tool" \
  --model tiny \
  --language en \
  --vox-threshold 0.03 \
  --hang-ms 500 \
  --segment-ms 5000 \
  --metrics \
  "$url" >"$stdout_path" 2>"$stderr_path"

if ! grep -qi 'organizations' "$stdout_path"; then
  printf 'cpktxscribe e2e missing expected transcript token: organizations\n' >&2
  printf 'stdout: %s\nstderr: %s\n' "$stdout_path" "$stderr_path" >&2
  exit 1
fi

if ! grep -qi 'accelerating' "$stdout_path"; then
  printf 'cpktxscribe e2e missing expected transcript token: accelerating\n' >&2
  printf 'stdout: %s\nstderr: %s\n' "$stdout_path" "$stderr_path" >&2
  exit 1
fi

printf '[cpktxscribe-e2e] ok url=%s stdout=%s stderr=%s\n' \
  "$url" "$stdout_path" "$stderr_path"
