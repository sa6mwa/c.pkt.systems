#!/usr/bin/env sh
set -eu

if [ "$#" -ne 1 ]; then
  printf 'usage: cpktxscribe_nodevice_policy_test.sh <source-dir>\n' >&2
  exit 2
fi

source_dir=$1
top_cmake=$source_dir/CMakeLists.txt
tool_source=$source_dir/tools/cpktxscribe.c
string_check=$source_dir/cmake/check_binary_absent_strings.cmake

require_file_contains() {
  file=$1
  needle=$2
  description=$3

  if ! grep -F -- "$needle" "$file" >/dev/null 2>&1; then
    printf 'missing cpktxscribe no-device policy: %s\n' "$description" >&2
    printf 'expected to find: %s\n' "$needle" >&2
    printf 'in: %s\n' "$file" >&2
    exit 1
  fi
}

require_file_lacks() {
  file=$1
  needle=$2
  description=$3

  if grep -F -- "$needle" "$file" >/dev/null 2>&1; then
    printf 'forbidden cpktxscribe device dependency drift: %s\n' "$description" >&2
    printf 'unexpected text: %s\n' "$needle" >&2
    printf 'in: %s\n' "$file" >&2
    exit 1
  fi
}

require_file_contains "$top_cmake" \
  'add_library(cpkt_audio_nodevice_static STATIC src/audio.c)' \
  'cpktaudio no-device static variant must exist'
require_file_contains "$top_cmake" \
  'CPKT_AUDIO_NO_DEVICE_IO' \
  'cpktaudio no-device static variant must compile out device I/O'
require_file_contains "$top_cmake" \
  'add_library(cpkt_sus_nodevice_static STATIC src/sus.c)' \
  'cpktsus no-device static variant must exist for CLI use'
require_file_contains "$top_cmake" \
  'cpkt_audio_nodevice_static' \
  'cpktsus no-device variant must link the no-device audio facade'
require_file_contains "$top_cmake" \
  'target_link_libraries(cpktxscribe PRIVATE cpkt_sus_nodevice_static)' \
  'cpktxscribe must link the no-device speech facade'
require_file_contains "$top_cmake" \
  'NAME cpktxscribe_no_device_backend_strings' \
  'cpktxscribe binary must be scanned for device backend strings'
require_file_contains "$top_cmake" \
  'cmake/check_binary_absent_strings.cmake' \
  'cpktxscribe no-device string scanner must be registered'

require_file_contains "$string_check" 'libasound' 'binary scanner rejects ALSA linkage'
require_file_contains "$string_check" 'libpulse' 'binary scanner rejects PulseAudio linkage'
require_file_contains "$string_check" 'libjack' 'binary scanner rejects JACK linkage'
require_file_contains "$string_check" 'CoreAudio\\.framework' 'binary scanner rejects CoreAudio linkage'

require_file_contains "$tool_source" \
  'cpkt_audio_decoder_open_url' \
  'cpktxscribe supports URL decoder input'
require_file_contains "$tool_source" \
  'cpkt_audio_decoder_open_file' \
  'cpktxscribe supports file decoder input'
require_file_lacks "$tool_source" \
  'cpkt_audio_capture_open_default' \
  'cpktxscribe must not open capture devices'
require_file_lacks "$tool_source" \
  'cpkt_audio_playback_open_default' \
  'cpktxscribe must not open playback devices'
require_file_lacks "$tool_source" \
  'cpkt_audio_capture' \
  'cpktxscribe must not depend on capture handle types'
require_file_lacks "$tool_source" \
  'cpkt_audio_playback' \
  'cpktxscribe must not depend on playback handle types'
