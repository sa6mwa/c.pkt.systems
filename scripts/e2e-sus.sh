#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_bin=${1:-"$repo_root/build/debug/cpkt_sus_audio_integration_test"}
build_dir=${2:-"$repo_root/build/debug"}
audio_url=${CPKT_SUS_E2E_AUDIO_URL:-"https://pkt.systems/trajectory/assets/narration/intro/intro.mp3"}
index_url=${CPKT_SUS_E2E_INDEX_URL:-"https://pkt.systems/trajectory/index.html"}
cache_root=${CPKT_SUS_E2E_CACHE:-"${XDG_CACHE_HOME:-"$HOME/.cache"}/c.pkt.systems/e2e/sus"}
audio_path="$cache_root/intro.mp3"
index_path="$cache_root/index.html"
model_cache="$cache_root/models"

download_atomic() {
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

extract_expected_text() {
  perl -0ne '
    if (/<div class="intro-crawl-track">.*?<p>(.*?)<\/p>/s) {
      $t = $1;
      $t =~ s/<[^>]+>//g;
      $t =~ s/&lt;/</g;
      $t =~ s/&gt;/>/g;
      $t =~ s/&amp;/\&/g;
      $t =~ s/\s+/ /g;
      $t =~ s/^\s+|\s+$//g;
      if ($t =~ /^(.*?\.)/) {
        $t = $1;
      }
      print $t;
    }
  ' "$1"
}

if ! command -v curl >/dev/null 2>&1; then
  printf 'curl is required for sus e2e downloads\n' >&2
  exit 2
fi
if [ ! -x "$test_bin" ]; then
  printf 'sus integration test binary is not executable: %s\n' "$test_bin" >&2
  exit 2
fi
if [ ! -f "$build_dir/CMakeCache.txt" ]; then
  printf 'CMake cache was not found: %s/CMakeCache.txt\n' "$build_dir" >&2
  exit 2
fi

download_atomic "$index_url" "$index_path"
download_atomic "$audio_url" "$audio_path"
mkdir -p "$model_cache"

expected=$(extract_expected_text "$index_path")
if [ -z "$expected" ]; then
  printf 'failed to extract expected narration text from %s\n' "$index_path" >&2
  exit 2
fi

external_root=$(sed -n 's/^CPKT_EXTERNAL_ROOT:PATH=//p' "$build_dir/CMakeCache.txt" | tail -n 1)
if [ -z "$external_root" ]; then
  printf 'failed to read CPKT_EXTERNAL_ROOT from %s/CMakeCache.txt\n' "$build_dir" >&2
  exit 2
fi

library_path="$(dirname "$test_bin"):$external_root/whisper/install/lib:$external_root/curl/install/lib:$external_root/openssl/install/lib:$external_root/nghttp2/install/lib:$external_root/libssh2/install/lib:$external_root/zlib/install/lib"
if [ "$(uname -s)" = "Darwin" ]; then
  export DYLD_LIBRARY_PATH="$library_path${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
else
  export LD_LIBRARY_PATH="$library_path${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

printf '[e2e-sus] audio=%s\n' "$audio_path"
printf '[e2e-sus] model-cache=%s\n' "$model_cache"
printf '[e2e-sus] expected=%s\n' "$expected"
printf '[e2e-sus] realtime-step-ms=%s\n' "${CPKT_SUS_E2E_REALTIME_STEP_MS:-1000}"
printf '[e2e-sus] realtime-length-ms=%s\n' "${CPKT_SUS_E2E_REALTIME_LENGTH_MS:-7000}"
printf '[e2e-sus] realtime-keep-ms=%s\n' "${CPKT_SUS_E2E_REALTIME_KEEP_MS:-1500}"
printf '[e2e-sus] vox-threshold=%s\n' "${CPKT_SUS_E2E_VOX_THRESHOLD:-0.03}"
printf '[e2e-sus] vox-prebuffer-ms=%s\n' "${CPKT_SUS_E2E_VOX_PREBUFFER_MS:-50}"
printf '[e2e-sus] expected-vox-segments=%s\n' "${CPKT_SUS_E2E_EXPECTED_VOX_SEGMENTS:-7}"
printf '[e2e-sus] expected-vox-hard-cuts=%s\n' "${CPKT_SUS_E2E_EXPECTED_VOX_HARD_CUTS:-6}"
printf '[e2e-sus] expected-vox-final-segments=%s\n' "${CPKT_SUS_E2E_EXPECTED_VOX_FINAL_SEGMENTS:-1}"

CPKT_SUS_INTEGRATION_ENABLE=1 \
CPKT_SUS_INTEGRATION_OPEN_CACHED=1 \
CPKT_SUS_INTEGRATION_MODEL="${CPKT_SUS_E2E_MODEL:-tiny}" \
CPKT_SUS_INTEGRATION_CACHE_DIR="$model_cache" \
CPKT_SUS_INTEGRATION_AUDIO_PATH="$audio_path" \
CPKT_SUS_INTEGRATION_AUDIO_URL="$audio_url" \
CPKT_SUS_INTEGRATION_EXPECTED_TEXT="$expected" \
CPKT_SUS_INTEGRATION_LANGUAGE="${CPKT_SUS_E2E_LANGUAGE:-en}" \
CPKT_SUS_INTEGRATION_REALTIME_STEP_MS="${CPKT_SUS_E2E_REALTIME_STEP_MS:-1000}" \
CPKT_SUS_INTEGRATION_REALTIME_LENGTH_MS="${CPKT_SUS_E2E_REALTIME_LENGTH_MS:-7000}" \
CPKT_SUS_INTEGRATION_REALTIME_KEEP_MS="${CPKT_SUS_E2E_REALTIME_KEEP_MS:-1500}" \
CPKT_SUS_INTEGRATION_VOX_THRESHOLD="${CPKT_SUS_E2E_VOX_THRESHOLD:-0.03}" \
CPKT_SUS_INTEGRATION_VOX_PREBUFFER_MS="${CPKT_SUS_E2E_VOX_PREBUFFER_MS:-50}" \
CPKT_SUS_INTEGRATION_MEMORY_SPOOL_BYTES="${CPKT_SUS_E2E_MEMORY_SPOOL_BYTES:-65536}" \
CPKT_SUS_INTEGRATION_MAX_SPOOL_BYTES="${CPKT_SUS_E2E_MAX_SPOOL_BYTES:-1073741824}" \
CPKT_SUS_INTEGRATION_EXPECTED_VOX_SEGMENTS="${CPKT_SUS_E2E_EXPECTED_VOX_SEGMENTS:-7}" \
CPKT_SUS_INTEGRATION_EXPECTED_VOX_HARD_CUTS="${CPKT_SUS_E2E_EXPECTED_VOX_HARD_CUTS:-6}" \
CPKT_SUS_INTEGRATION_EXPECTED_VOX_FINAL_SEGMENTS="${CPKT_SUS_E2E_EXPECTED_VOX_FINAL_SEGMENTS:-1}" \
CPKT_SUS_INTEGRATION_CPU_ONLY=1 \
  "$test_bin"
