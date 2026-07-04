#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
example_bin=${1:-"$repo_root/build/debug/cpkt_audio_vox_intro_c89_example"}
audio_url=${CPKT_AUDIO_VOX_INTRO_URL:-"https://pkt.systems/trajectory/assets/narration/intro/intro.mp3"}
cache_root=${CPKT_AUDIO_VOX_INTRO_CACHE:-"${XDG_CACHE_HOME:-"$HOME/.cache"}/c.pkt.systems/examples/audio-vox-intro"}
audio_path="$cache_root/intro.mp3"
dump_dir=${CPKT_AUDIO_VOX_INTRO_DUMP_DIR:-"$repo_root/build/vox-intro-dump"}
threshold=${CPKT_AUDIO_VOX_INTRO_THRESHOLD:-0.03}
hang_ms=${CPKT_AUDIO_VOX_INTRO_HANG_MS:-500}
budget_ms=${CPKT_AUDIO_VOX_INTRO_BUDGET_MS:-7000}
memory_spool_bytes=${CPKT_AUDIO_VOX_INTRO_MEMORY_SPOOL_BYTES:-65536}
max_spool_bytes=${CPKT_AUDIO_VOX_INTRO_MAX_SPOOL_BYTES:-1073741824}
assert_expected=${CPKT_AUDIO_VOX_INTRO_ASSERT:-auto}
expected_segments=${CPKT_AUDIO_VOX_INTRO_EXPECTED_SEGMENTS:-11}
expected_hard_cuts=${CPKT_AUDIO_VOX_INTRO_EXPECTED_HARD_CUTS:-0}
expected_final_segments=${CPKT_AUDIO_VOX_INTRO_EXPECTED_FINAL_SEGMENTS:-1}

download_atomic() {
  local url
  local path
  local tmp

  url=$1
  path=$2
  tmp="$path.tmp.$$"

  if [ -s "$path" ]; then
    return 0
  fi
  mkdir -p "$(dirname "$path")"
  curl -fL --retry 3 --connect-timeout 30 --output "$tmp" "$url"
  mv "$tmp" "$path"
}

if [ ! -x "$example_bin" ]; then
  printf 'audio VOX intro example is not executable: %s\n' "$example_bin" >&2
  exit 2
fi

if [ "${CPKT_AUDIO_VOX_INTRO_DIRECT:-0}" = "1" ]; then
  source_args=(--url "$audio_url")
else
  if ! command -v curl >/dev/null 2>&1; then
    printf 'curl is required to cache intro.mp3\n' >&2
    exit 2
  fi
  download_atomic "$audio_url" "$audio_path"
  source_args=(--audio "$audio_path")
fi

printf '[audio-vox-intro] dump-dir=%s\n' "$dump_dir"
printf '[audio-vox-intro] threshold=%s hang-ms=%s budget-ms=%s\n' \
  "$threshold" "$hang_ms" "$budget_ms"

"$example_bin" \
  "${source_args[@]}" \
  --dump-dir "$dump_dir" \
  --threshold "$threshold" \
  --hang-ms "$hang_ms" \
  --budget-ms "$budget_ms" \
  --memory-spool-bytes "$memory_spool_bytes" \
  --max-spool-bytes "$max_spool_bytes"

if [ "$assert_expected" = "auto" ]; then
  if [ "$threshold" = "0.03" ] && [ "$hang_ms" = "500" ] && [ "$budget_ms" = "7000" ]; then
    assert_expected=1
  else
    assert_expected=0
  fi
fi

if [ "$assert_expected" = "1" ]; then
  summary_line=$(grep '^summary ' "$dump_dir/summary.txt" | tail -n 1)
  case "$summary_line" in
    *"segments=$expected_segments "*\
*"hard_cuts=$expected_hard_cuts "*\
*"final_segments=$expected_final_segments "*)
      printf '[audio-vox-intro] deterministic summary matched: %s\n' "$summary_line"
      ;;
    *)
      printf '[audio-vox-intro] deterministic summary mismatch\n' >&2
      printf 'expected: segments=%s hard_cuts=%s final_segments=%s\n' \
        "$expected_segments" "$expected_hard_cuts" "$expected_final_segments" >&2
      printf 'actual:   %s\n' "$summary_line" >&2
      exit 1
      ;;
  esac
fi
