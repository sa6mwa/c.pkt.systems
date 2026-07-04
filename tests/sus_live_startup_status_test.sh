#!/usr/bin/env sh
set -eu

if [ "$#" -ne 1 ]; then
  printf 'usage: sus_live_startup_status_test.sh <cpkt_sus_live_vox_c89_example>\n' >&2
  exit 2
fi

example=$1
missing_model=${TMPDIR:-/tmp}/cpkt-sus-live-startup-missing-model.bin
output=${TMPDIR:-/tmp}/cpkt-sus-live-startup-status.out

rm -f "$missing_model" "$output"

set +e
"$example" --model-path "$missing_model" --seconds 0 >"$output" 2>&1
rc=$?
set -e

if [ "$rc" -eq 0 ]; then
  printf 'sus live example unexpectedly succeeded with a missing model path\n' >&2
  cat "$output" >&2
  exit 1
fi

if ! grep -F -- "status mode=vox model_path=$missing_model language=en threshold=0.060 hang_ms=1500 prebuffer_ms=50" "$output" >/dev/null 2>&1; then
  printf 'sus live example did not print the startup status line before model load\n' >&2
  cat "$output" >&2
  exit 1
fi

if ! grep -F -- 'model open failed: model load failed' "$output" >/dev/null 2>&1; then
  printf 'sus live example did not report the expected missing model failure\n' >&2
  cat "$output" >&2
  exit 1
fi

if grep -F -- 'RX' "$output" >/dev/null 2>&1; then
  printf 'sus live example reached capture state before resolving the model\n' >&2
  cat "$output" >&2
  exit 1
fi
