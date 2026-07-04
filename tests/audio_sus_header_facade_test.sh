#!/usr/bin/env bash
set -euo pipefail

repo_root=${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
include_dir="$repo_root/include"
audio_header="$include_dir/cpkt/audio.h"
sus_header="$include_dir/cpkt/sus.h"

cc=${CC:-cc}
cxx=${CXX:-c++}
work_root=$(mktemp -d "${TMPDIR:-/tmp}/cpkt-audio-sus-header.XXXXXX")
trap 'rm -rf "$work_root"' EXIT

for header in "$audio_header" "$sus_header"; do
  for forbidden in \
    'miniaudio' \
    'whisper' \
    'ggml' \
    'curl/' \
    'CURL' \
    'stdint.h' \
    'stdbool.h' \
    'uint8_t' \
    'uint16_t' \
    'uint32_t' \
    'uint64_t' \
    'int8_t' \
    'int16_t' \
    'int32_t' \
    'int64_t' \
    'long long' \
    'inline'
  do
    if grep -F -- "$forbidden" "$header" >/dev/null 2>&1; then
      printf 'audio/sus C89 facade header contains forbidden token: %s: %s\n' "$header" "$forbidden" >&2
      exit 1
    fi
  done
done

cat > "$work_root/audio_header_c89.c" <<'EOF'
#include <cpkt/audio.h>

int main(void) {
  cpkt_audio_decoder_config decoder_config;
  cpkt_audio_encoder_config encoder_config;
  cpkt_audio_vox_segment vox_segment;
  cpkt_audio_stream_info info;

  vox_segment.impl = 0;
  vox_segment.frame_count = 0U;
  vox_segment.t0 = 0;
  vox_segment.t1 = 0;
  vox_segment.segment_index = 0UL;
  vox_segment.hard_cut = 0;
  vox_segment.is_final = 0;
  vox_segment.read_f32_mono_16k = 0;
  decoder_config.encoding = CPKT_AUDIO_ENCODING_UNKNOWN;
  encoder_config.format = CPKT_AUDIO_FORMAT_WAV;
  encoder_config.sample_rate = 16000UL;
  encoder_config.channels = 1UL;
  info.source_format = encoder_config.format;
  info.output_sample_rate = encoder_config.sample_rate;
  info.output_channels = encoder_config.channels;
  info.output_frame_count = 0UL;
  return decoder_config.encoding + info.source_format + vox_segment.hard_cut;
}
EOF

"$cc" -std=c89 -Wall -Wextra -Wpedantic -Werror -I "$include_dir" \
  -c "$work_root/audio_header_c89.c" -o "$work_root/audio_header_c89.o"

cat > "$work_root/sus_header_c89.c" <<'EOF'
#include <cpkt/sus.h>

int main(void) {
  cpkt_sus_transcriber_config transcriber_config;
  cpkt_sus_segmented_config segmented_config;
  cpkt_sus_segmented_event event;
  cpkt_sus_model_entry entry;

  transcriber_config.threads = 1;
  transcriber_config.cpu_only = 1;
  transcriber_config.language = "sv";
  transcriber_config.translate = 0;
  transcriber_config.timestamps = 0;
  transcriber_config.initial_prompt = 0;
  transcriber_config.segment_sink = 0;
  transcriber_config.segment_user = 0;
  transcriber_config.progress_sink = 0;
  transcriber_config.progress_user = 0;
  transcriber_config.abort = 0;
  transcriber_config.abort_user = 0;

  segmented_config.mode = CPKT_SUS_SEGMENT_MODE_CONTINUOUS;
  segmented_config.read_frames = 4096UL;
  segmented_config.mode = CPKT_SUS_SEGMENT_MODE_SIMPLEX;
  segmented_config.step_ms = 1000UL;
  segmented_config.length_ms = 7000UL;
  segmented_config.keep_ms = 1500UL;
  segmented_config.keep_context = 1;
  segmented_config.vox_threshold = 0.03f;
  segmented_config.prebuffer_ms = 50UL;
  segmented_config.memory_spool_bytes = 1024UL * 1024UL;
  segmented_config.max_spool_bytes = 1024UL * 1024UL * 1024UL;
  segmented_config.audio_ctx = 0UL;
  segmented_config.max_tokens = 0UL;
  segmented_config.segmented_sink = 0;
  segmented_config.segmented_user = 0;

  event.text = "";
  event.text_length = 0UL;
  event.t0 = 0;
  event.t1 = 0;
  event.step_index = 0UL;
  event.is_final = 0;

  entry.name = "tiny";
  entry.provider = "ggml";
  entry.source_url = "";
  entry.filename = "";
  entry.sha256 = "";
  entry.size_bytes = 0UL;
  entry.license = "";
  entry.quantization = "f16";
  entry.is_default = 0;

  if (sizeof(((cpkt_sus_transcriber *)0)->revised_text) == 0) {
    return 1;
  }
  if (sizeof(((cpkt_sus_transcriber *)0)->transcribe_audio_decoder_segmented) == 0 ||
      sizeof(((cpkt_sus_transcriber *)0)->transcribe_audio_decoder_segmented_text) == 0 ||
      sizeof(((cpkt_sus_transcriber *)0)->transcribe_audio_vox_segment) == 0) {
    return 2;
  }

  return transcriber_config.threads + segmented_config.keep_context +
         event.is_final + entry.is_default;
}
EOF

"$cc" -std=c89 -Wall -Wextra -Wpedantic -Werror -I "$include_dir" \
  -c "$work_root/sus_header_c89.c" -o "$work_root/sus_header_c89.o"

cat > "$work_root/audio_then_sus_header_c89.c" <<'EOF'
#include <cpkt/audio.h>
#include <cpkt/sus.h>

int main(void) {
  cpkt_audio_decoder *decoder;
  cpkt_sus_transcriber *transcriber;
  decoder = 0;
  transcriber = 0;
  return decoder == 0 && transcriber == 0 ? 0 : 1;
}
EOF

"$cc" -std=c89 -Wall -Wextra -Wpedantic -Werror -I "$include_dir" \
  -c "$work_root/audio_then_sus_header_c89.c" -o "$work_root/audio_then_sus_header_c89.o"

cat > "$work_root/sus_then_audio_header_c89.c" <<'EOF'
#include <cpkt/sus.h>
#include <cpkt/audio.h>

int main(void) {
  cpkt_audio_decoder *decoder;
  cpkt_sus_model *model;
  decoder = 0;
  model = 0;
  return decoder == 0 && model == 0 ? 0 : 1;
}
EOF

"$cc" -std=c89 -Wall -Wextra -Wpedantic -Werror -I "$include_dir" \
  -c "$work_root/sus_then_audio_header_c89.c" -o "$work_root/sus_then_audio_header_c89.o"

cat > "$work_root/audio_sus_header_cpp98.cpp" <<'EOF'
#include <cpkt/audio.h>
#include <cpkt/sus.h>

int main() {
  cpkt_audio_encoder_config encoder_config;
  cpkt_sus_segmented_config segmented_config;
  encoder_config.format = CPKT_AUDIO_FORMAT_WAV;
  encoder_config.sample_rate = 16000UL;
  encoder_config.channels = 1UL;
  segmented_config.step_ms = 1000UL;
  segmented_config.length_ms = 7000UL;
  segmented_config.keep_ms = 1500UL;
  segmented_config.keep_context = 1;
  segmented_config.vox_threshold = 0.03f;
  segmented_config.prebuffer_ms = 50UL;
  segmented_config.memory_spool_bytes = 1024UL * 1024UL;
  segmented_config.max_spool_bytes = 1024UL * 1024UL * 1024UL;
  return encoder_config.format + segmented_config.keep_context;
}
EOF

"$cxx" -std=c++98 -Wall -Wextra -Wpedantic -Werror -I "$include_dir" \
  -c "$work_root/audio_sus_header_cpp98.cpp" -o "$work_root/audio_sus_header_cpp98.o"
