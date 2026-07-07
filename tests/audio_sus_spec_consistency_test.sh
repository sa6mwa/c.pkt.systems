#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
  printf 'usage: audio_sus_spec_consistency_test.sh <source-dir>\n' >&2
  exit 2
fi

source_dir=$1
readme=$source_dir/README.md
spec=$source_dir/docs/audio-sus-facade-spec.md
package_assertions=$source_dir/cmake/package_assertions.cmake
package_bundle=$source_dir/cmake/package_bundle.cmake
package_smoke=$source_dir/scripts/package-install-smoke.sh
small_default_catalog_fragment='ggml-small.bin\t'
small_default_flag_fragment='\tf16\t1'
tiny_catalog_fragment='ggml-tiny.bin\thttps://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.bin'

assert_file_contains() {
  file=$1
  needle=$2
  description=$3

  if ! grep -F "$needle" "$file" >/dev/null 2>&1; then
    printf '%s does not contain required audio/sus policy: %s\n' "$description" "$needle" >&2
    exit 1
  fi
}

if grep -F 'cpktsus` remains independent from `cpktaudio` at link time' "$spec" >/dev/null 2>&1; then
  printf 'audio/sus spec still claims cpktsus is link-independent from cpktaudio\n' >&2
  exit 1
fi

if ! grep -F 'public dependency on `libcpktaudio`' "$spec" >/dev/null 2>&1; then
  printf 'audio/sus spec does not document the cpktsus public libcpktaudio dependency\n' >&2
  exit 1
fi

if grep -F 'default `small`' "$spec" >/dev/null 2>&1; then
  printf 'audio/sus spec documents small as a default model\n' >&2
  exit 1
fi
if grep -F 'strcmp(entry.name, "small")' "$package_smoke" >/dev/null 2>&1; then
  printf 'package smoke expects small as the default model\n' >&2
  exit 1
fi
if grep -F "$small_default_catalog_fragment" "$package_assertions" | grep -F "$small_default_flag_fragment" >/dev/null 2>&1; then
  printf 'package assertions expect small as the default model\n' >&2
  exit 1
fi

assert_file_contains "$spec" 'The shell and the library resolver both default to `tiny`' 'audio/sus spec'
assert_file_contains "$spec" 'The default cached model is `tiny`' 'audio/sus spec'
assert_file_contains "$readme" 'NULL or empty cached-model name defaults to' 'README'
assert_file_contains "$readme" '`tiny`' 'README'
assert_file_contains "$readme" '`cpktxscribe` shell exposes the same' 'README'
assert_file_contains "$readme" '`--cache-dir DIR`' 'README'
assert_file_contains "$readme" '`cpkt_audio` is a C89 facade over miniaudio' 'README'
assert_file_contains "$readme" '`cpkt_sus` is a C89 facade over a CPU-only whisper.cpp/ggml build' 'README'
assert_file_contains "$package_smoke" 'strcmp(entry.name, "tiny")' 'package install smoke'
assert_file_contains "$package_assertions" "$tiny_catalog_fragment" 'package assertion script'
assert_file_contains "$spec" '`cpkt_sus_log_set`, which adapts whisper.cpp' 'audio/sus spec'

if ! grep -F 'assert_words_count "$sus_words" "-lcpktaudio" 1' "$package_smoke" >/dev/null 2>&1; then
  printf 'package smoke does not verify cpkt-sus.pc emits -lcpktaudio exactly once\n' >&2
  exit 1
fi

if ! grep -F 'assert_file_contains "$cmake_link_dir/cpkt_cmake_sus_facade.dir/link.txt" "$prefix/lib/libcpktaudio.a"' "$package_smoke" >/dev/null 2>&1; then
  printf 'package smoke does not verify cpkt::sus CMake consumers link libcpktaudio\n' >&2
  exit 1
fi

assert_file_contains "$spec" 'miniaudio license text when miniaudio source or binaries are bundled' 'audio/sus spec'
assert_file_contains "$spec" 'whisper.cpp/ggml license text when whisper.cpp/ggml source or binaries are' 'audio/sus spec'
assert_file_contains "$spec" 'Apache-2.0 license/provenance for KBLab model-cache entries' 'audio/sus spec'

assert_file_contains "$package_bundle" 'cpkt_stage_license("miniaudio"' 'package bundle script'
assert_file_contains "$package_bundle" 'cpkt_stage_license("whisper.cpp"' 'package bundle script'
assert_file_contains "$package_bundle" 'docs/third_party/kblab-whisper-models/LICENSE' 'package bundle script'
assert_file_contains "$package_bundle" 'docs/third_party/kblab-whisper-models/PROVENANCE.md' 'package bundle script'
assert_file_contains "$package_bundle" 'docs/sus-model-catalog.tsv' 'package bundle script'

assert_file_contains "$package_assertions" 'share/doc/c.pkt.systems/third_party/miniaudio/LICENSE' 'package assertion script'
assert_file_contains "$package_assertions" 'share/doc/c.pkt.systems/third_party/whisper.cpp/LICENSE' 'package assertion script'
assert_file_contains "$package_assertions" 'share/doc/c.pkt.systems/third_party/kblab-whisper-models/LICENSE' 'package assertion script'
assert_file_contains "$package_assertions" 'share/doc/c.pkt.systems/third_party/kblab-whisper-models/PROVENANCE.md' 'package assertion script'
assert_file_contains "$package_assertions" 'share/c.pkt.systems/sus-model-catalog.tsv' 'package assertion script'

assert_file_contains "$package_smoke" 'assert_package_file "share/doc/c.pkt.systems/third_party/miniaudio/LICENSE"' 'package install smoke'
assert_file_contains "$package_smoke" 'assert_package_file "share/doc/c.pkt.systems/third_party/whisper.cpp/LICENSE"' 'package install smoke'
assert_file_contains "$package_smoke" 'assert_package_file "share/doc/c.pkt.systems/third_party/kblab-whisper-models/LICENSE"' 'package install smoke'
assert_file_contains "$package_smoke" 'assert_package_file "share/doc/c.pkt.systems/third_party/kblab-whisper-models/PROVENANCE.md"' 'package install smoke'
assert_file_contains "$package_smoke" 'assert_file_contains "$prefix/share/doc/c.pkt.systems/third_party/kblab-whisper-models/PROVENANCE.md" "KBLab/kb-whisper-*"' 'package install smoke'
