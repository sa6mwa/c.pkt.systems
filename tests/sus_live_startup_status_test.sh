#!/usr/bin/env sh
set -eu

if [ "$#" -ne 1 ]; then
  printf 'usage: sus_live_startup_status_test.sh <cpkt_sus_live_vox_c89_example>\n' >&2
  exit 2
fi

example=$1
missing_model=${TMPDIR:-/tmp}/cpkt-sus-live-startup-missing-model.bin
output=${TMPDIR:-/tmp}/cpkt-sus-live-startup-status.out
cache_dir=${TMPDIR:-/tmp}/cpkt-sus-live-startup-cache.$$
cache_output=${TMPDIR:-/tmp}/cpkt-sus-live-startup-cache-status.out

cleanup() {
  rm -rf "$cache_dir" "$missing_model" "$output" "$cache_output"
}
trap cleanup EXIT HUP INT TERM

rm -rf "$cache_dir" "$missing_model" "$output" "$cache_output"
mkdir -p "$cache_dir"

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

set +e
"$example" --offline --cache-dir "$cache_dir" --model tiny --seconds 0 \
  >"$cache_output" 2>&1
rc=$?
set -e

if [ "$rc" -eq 0 ]; then
  printf 'sus live example unexpectedly succeeded with a missing offline cache\n' >&2
  cat "$cache_output" >&2
  exit 1
fi

if ! grep -F -- "status mode=vox model=tiny cache=$cache_dir/ggml-tiny.bin cache_state=missing-offline language=en threshold=0.060 hang_ms=1500 prebuffer_ms=50" "$cache_output" >/dev/null 2>&1; then
  printf 'sus live example did not print the initial cached-model status line\n' >&2
  cat "$cache_output" >&2
  exit 1
fi

if ! grep -F -- "status model_cache=lookup model=tiny cache=$cache_dir/ggml-tiny.bin" "$cache_output" >/dev/null 2>&1; then
  printf 'sus live example did not print the cache lookup status line\n' >&2
  cat "$cache_output" >&2
  exit 1
fi

if ! grep -F -- "status model_cache=missing model=tiny cache=$cache_dir/ggml-tiny.bin" "$cache_output" >/dev/null 2>&1; then
  printf 'sus live example did not print the cache miss status line\n' >&2
  cat "$cache_output" >&2
  exit 1
fi

if grep -F -- 'RX' "$cache_output" >/dev/null 2>&1; then
  printf 'sus live example reached capture state before resolving cached model\n' >&2
  cat "$cache_output" >&2
  exit 1
fi
