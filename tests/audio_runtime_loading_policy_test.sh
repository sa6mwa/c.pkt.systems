#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 2 ]; then
  printf 'usage: audio_runtime_loading_policy_test.sh <libcpktaudio.a> <libcpktaudio.so>\n' >&2
  exit 2
fi

audio_static=$1
audio_shared=$2

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

require_tool nm
require_tool readelf
require_tool grep
require_file "$audio_static" "libcpktaudio static archive"
require_file "$audio_shared" "libcpktaudio shared library"

assert_static_refs_dlopen "$audio_static"
assert_shared_refs_dlopen "$audio_shared"
assert_shared_lacks_needed "$audio_shared" 'libasound\.so[^]]*' ALSA
assert_shared_lacks_needed "$audio_shared" 'libpulse[^]]*\.so[^]]*' PulseAudio
assert_shared_lacks_needed "$audio_shared" 'libjack[^]]*\.so[^]]*' JACK
