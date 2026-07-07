#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
example_bin=${1:-"$repo_root/build/debug/cpkt_sus_vox_intro_c89_example"}
build_dir=${2:-"$repo_root/build/debug"}
audio_url=${CPKT_SUS_VOX_INTRO_URL:-"https://pkt.systems/trajectory/assets/narration/intro/intro.mp3"}
cache_root=${CPKT_SUS_VOX_INTRO_CACHE:-"${XDG_CACHE_HOME:-"$HOME/.cache"}/c.pkt.systems/examples/sus-vox-intro"}
audio_path="$cache_root/intro.mp3"
dump_dir=${CPKT_SUS_VOX_INTRO_DUMP_DIR:-"$repo_root/build/sus-vox-intro-dump"}
model_cache=${CPKT_SUS_VOX_INTRO_MODEL_CACHE:-"$cache_root/models"}
model=${CPKT_SUS_VOX_INTRO_MODEL:-tiny}
language=${CPKT_SUS_VOX_INTRO_LANGUAGE:-en}
threshold=${CPKT_SUS_VOX_INTRO_THRESHOLD:-0.03}
hang_ms=${CPKT_SUS_VOX_INTRO_HANG_MS:-1500}
budget_ms=${CPKT_SUS_VOX_INTRO_BUDGET_MS:-0}
read_frames=${CPKT_SUS_VOX_INTRO_READ_FRAMES:-4096}
prebuffer_ms=${CPKT_SUS_VOX_INTRO_PREBUFFER_MS:-50}

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
  printf 'sus VOX intro example is not executable: %s\n' "$example_bin" >&2
  exit 2
fi
if [ ! -f "$build_dir/CMakeCache.txt" ]; then
  printf 'CMake cache was not found: %s/CMakeCache.txt\n' "$build_dir" >&2
  exit 2
fi
if ! command -v curl >/dev/null 2>&1; then
  printf 'curl is required to cache intro.mp3\n' >&2
  exit 2
fi

download_atomic "$audio_url" "$audio_path"
mkdir -p "$model_cache"
rm -rf "$dump_dir"
mkdir -p "$dump_dir"

external_root=$(sed -n 's/^CPKT_EXTERNAL_ROOT:PATH=//p' "$build_dir/CMakeCache.txt" | tail -n 1)
if [ -z "$external_root" ]; then
  printf 'failed to read CPKT_EXTERNAL_ROOT from %s/CMakeCache.txt\n' "$build_dir" >&2
  exit 2
fi

library_path="$(dirname "$example_bin"):$external_root/whisper/install/lib:$external_root/curl/install/lib:$external_root/openssl/install/lib:$external_root/nghttp2/install/lib:$external_root/libssh2/install/lib:$external_root/zlib/install/lib"
if [ "$(uname -s)" = "Darwin" ]; then
  export DYLD_LIBRARY_PATH="$library_path${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
else
  export LD_LIBRARY_PATH="$library_path${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

printf '[sus-vox-intro] audio=%s\n' "$audio_path"
printf '[sus-vox-intro] dump-dir=%s\n' "$dump_dir"
printf '[sus-vox-intro] model=%s model-cache=%s\n' "$model" "$model_cache"
printf '[sus-vox-intro] threshold=%s hang-ms=%s budget-ms=%s prebuffer-ms=%s\n' \
  "$threshold" "$hang_ms" "$budget_ms" "$prebuffer_ms"

"$example_bin" \
  --audio "$audio_path" \
  --dump-dir "$dump_dir" \
  --model "$model" \
  --cache-dir "$model_cache" \
  --language "$language" \
  --threshold "$threshold" \
  --hang-ms "$hang_ms" \
  --budget-ms "$budget_ms" \
  --read-frames "$read_frames" \
  --prebuffer-ms "$prebuffer_ms" \
  --cpu-only "${CPKT_SUS_VOX_INTRO_CPU_ONLY:-1}"
