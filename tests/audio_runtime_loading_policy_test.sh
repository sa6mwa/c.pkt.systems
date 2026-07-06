#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 4 ]; then
  printf 'usage: audio_runtime_loading_policy_test.sh <libcpktaudio.a> <libcpktaudio.so> <audio.c> <CMakeLists.txt>\n' >&2
  exit 2
fi

audio_static=$1
audio_shared=$2
audio_source=$3
cmake_source=$4

require_tool() {
  tool=$1
  if ! command -v "$tool" >/dev/null 2>&1; then
    printf '%s is required to inspect audio runtime-loading policy\n' "$tool" >&2
    exit 1
  fi
}

require_file() {
  file_path=$1
  description=$2
  if [ ! -f "$file_path" ]; then
    printf 'missing %s: %s\n' "$description" "$file_path" >&2
    exit 1
  fi
}

assert_static_refs_dlopen() {
  file_path=$1

  if ! nm -u "$file_path" | grep -E '[[:space:]]U[[:space:]]+dlopen($|@)' >/dev/null 2>&1; then
    printf 'libcpktaudio.a does not reference dlopen; native runtime loading is not compiled into the static facade\n' >&2
    nm -u "$file_path" >&2
    exit 1
  fi
}

assert_shared_refs_dlopen() {
  file_path=$1

  if ! nm -D -u "$file_path" | grep -E '[[:space:]]U[[:space:]]+dlopen($|@)' >/dev/null 2>&1; then
    printf 'libcpktaudio.so does not reference dlopen; native runtime loading is not compiled into the shared facade\n' >&2
    nm -D -u "$file_path" >&2
    exit 1
  fi
}

assert_shared_lacks_needed() {
  file_path=$1
  forbidden=$2
  description=$3

  if readelf -d "$file_path" | grep -E '\(NEEDED\).*\['"$forbidden"'\]' >/dev/null 2>&1; then
    printf 'libcpktaudio.so has forbidden direct %s dependency matching %s\n' "$description" "$forbidden" >&2
    readelf -d "$file_path" >&2
    exit 1
  fi
}

assert_source_contains() {
  file_path=$1
  needle=$2
  description=$3

  if ! grep -F "$needle" "$file_path" >/dev/null 2>&1; then
    printf 'audio runtime-loading policy source check failed: %s\n' "$description" >&2
    printf 'missing: %s\n' "$needle" >&2
    exit 1
  fi
}

assert_source_lacks() {
  file_path=$1
  needle=$2
  description=$3

  if grep -F "$needle" "$file_path" >/dev/null 2>&1; then
    printf 'audio runtime-loading policy source check failed: %s\n' "$description" >&2
    printf 'unexpected: %s\n' "$needle" >&2
    exit 1
  fi
}

require_tool nm
require_tool readelf
require_tool grep
require_file "$audio_static" "libcpktaudio static archive"
require_file "$audio_shared" "libcpktaudio shared library"
require_file "$audio_source" "audio facade implementation"
require_file "$cmake_source" "CMake project file"

assert_static_refs_dlopen "$audio_static"
assert_shared_refs_dlopen "$audio_shared"
assert_shared_lacks_needed "$audio_shared" 'libasound\.so[^]]*' ALSA
assert_shared_lacks_needed "$audio_shared" 'libpulse[^]]*\.so[^]]*' PulseAudio
assert_shared_lacks_needed "$audio_shared" 'libjack[^]]*\.so[^]]*' JACK
assert_source_contains "$audio_source" \
  'cpkt_audio_playback_wait_turn_elapsed(impl, impl->drain_latency_ms)' \
  'native playback drain must wait for configured backend latency'
assert_source_contains "$audio_source" \
  'if (!impl->started) {' \
  'native playback writes must fail before start instead of waiting for ring-buffer space forever'
assert_source_contains "$audio_source" \
  ': 2000UL) +' \
  'native playback drain latency must include default or configured buffer duration'
assert_source_contains "$audio_source" \
  'cpkt_audio_device_period_ms(config != NULL ? config->period_ms : 0UL)' \
  'native playback drain latency must include configured period duration'
assert_source_contains "$audio_source" \
  'CPKT_AUDIO_AUTO_PROCESS_DEVICE_IO' \
  'static process-device builds must have a compile-time AUTO-to-process policy'
assert_source_contains "$audio_source" \
  'CPKT_AUDIO_DEVICE_MODE_PROCESS' \
  'process backend mode must be backend-neutral for capture and playback'
assert_source_contains "$audio_source" \
  '#if !defined(__APPLE__)' \
  'SIGPIPE drain must not call sigtimedwait on Darwin'
assert_source_contains "$audio_source" \
  '(void)sigtimedwait(&blocked, NULL, &timeout)' \
  'Linux process playback must consume generated SIGPIPE after EPIPE'
assert_source_lacks "$audio_source" \
  'CPKT_AUDIO_DEVICE_MODE_PROCESS_ARECORD' \
  'process backend mode must not retain capture-specific naming'
assert_source_lacks "$audio_source" \
  'last_read_ms' \
  'process capture must not retain unused readiness timestamp state'
assert_source_contains "$cmake_source" \
  'CPKT_AUDIO_AUTO_PROCESS_DEVICE_IO' \
  'Linux static cpktaudio builds must select process device IO for AUTO'
