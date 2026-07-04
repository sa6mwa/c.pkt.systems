#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
  printf 'usage: audio_sus_spec_consistency_test.sh <source-dir>\n' >&2
  exit 2
fi

source_dir=$1
spec=$source_dir/docs/audio-sus-facade-spec.md
package_smoke=$source_dir/scripts/package-install-smoke.sh

if grep -F 'cpktsus` remains independent from `cpktaudio` at link time' "$spec" >/dev/null 2>&1; then
  printf 'audio/sus spec still claims cpktsus is link-independent from cpktaudio\n' >&2
  exit 1
fi

if ! grep -F 'public dependency on `libcpktaudio`' "$spec" >/dev/null 2>&1; then
  printf 'audio/sus spec does not document the cpktsus public libcpktaudio dependency\n' >&2
  exit 1
fi

if ! grep -F 'assert_words_count "$sus_words" "-lcpktaudio" 1' "$package_smoke" >/dev/null 2>&1; then
  printf 'package smoke does not verify cpkt-sus.pc emits -lcpktaudio exactly once\n' >&2
  exit 1
fi

if ! grep -F 'assert_file_contains "$cmake_link_dir/cpkt_cmake_sus_facade.dir/link.txt" "$prefix/lib/libcpktaudio.a"' "$package_smoke" >/dev/null 2>&1; then
  printf 'package smoke does not verify cpkt::sus CMake consumers link libcpktaudio\n' >&2
  exit 1
fi
