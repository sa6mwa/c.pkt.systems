#!/usr/bin/env sh
set -eu

if [ "$#" -ne 1 ]; then
  printf 'usage: sus_segmented_policy_test.sh <source-dir>\n' >&2
  exit 2
fi

source_dir=$1
sus_header=$source_dir/include/cpkt/sus.h
sus_source=$source_dir/src/sus.c

require_file_contains() {
  file=$1
  needle=$2
  description=$3
  if ! grep -F -- "$needle" "$file" >/dev/null 2>&1; then
    printf 'missing segmented policy: %s\n' "$description" >&2
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
    printf 'forbidden segmented policy drift: %s\n' "$description" >&2
    printf 'unexpected text: %s\n' "$needle" >&2
    printf 'in: %s\n' "$file" >&2
    exit 1
  fi
}

require_file_contains "$sus_header" \
  'CPKT_SUS_SEGMENT_MODE_SIMPLEX = 0' \
  'zero-initialized segmented configs select simplex mode'
require_file_contains "$sus_header" \
  'CPKT_SUS_SEGMENT_MODE_CONTINUOUS = 1' \
  'continuous mode remains the explicit non-zero mode'
require_file_contains "$sus_header" \
  'length_ms disables the time cap and relies on VOX release or' \
  'simplex zero length disables the hard time cap'
require_file_contains "$sus_header" \
  'default segment budget, currently 7000 ms.' \
  'continuous zero length selects the documented 7000 ms budget'
require_file_contains "$sus_header" \
  'Deprecated compatibility field. The VOX path does not run fixed-step' \
  'step_ms cannot reintroduce fixed-step inference'
require_file_contains "$sus_header" \
  'Previous audio is never retranscribed; prompt tokens from the previous' \
  'streaming segmented transcription carries prompt tokens instead of audio'
require_file_contains "$sus_header" \
  'int preserve_initial_space_after_first_transcriber;' \
  'model instances expose opt-in cross-transcriber initial-spacing policy'
require_file_contains "$sus_header" \
  'reset_transcript_spacing' \
  'model instances expose reset for cross-transcriber initial-spacing state'
require_file_contains "$sus_header" \
  'cpkt_sus_result cpkt_sus_reset_transcript_spacing(cpkt_sus *sus);' \
  'preferred instance reset wrapper is part of the public API'
require_file_contains "$sus_header" \
  'void cpkt_sus_config_default(cpkt_sus_config *config);' \
  'path-open config has a public default initializer'
require_file_contains "$sus_header" \
  'void cpkt_sus_cache_config_default(cpkt_sus_cache_config *config);' \
  'cache-open config has a public default initializer'
require_file_contains "$sus_header" \
  'void cpkt_sus_transcriber_config_default(cpkt_sus_transcriber_config *config);' \
  'transcriber config has a public default initializer'
require_file_contains "$sus_header" \
  'void cpkt_sus_segmented_config_default(cpkt_sus_segmented_config *config);' \
  'segmented config has a public default initializer'

require_file_contains "$sus_source" \
  'config->mode == CPKT_SUS_SEGMENT_MODE_CONTINUOUS' \
  'implementation branches continuous mode explicitly'
require_file_contains "$sus_source" \
  'length_ms = 7000UL;' \
  'continuous zero length maps to 7000 ms'
require_file_contains "$sus_source" \
  'length_ms = 0UL;' \
  'simplex zero length leaves VOX uncapped by time'
require_file_contains "$sus_source" \
  'keep_ms = config != NULL && config->keep_ms != 0UL ? config->keep_ms : 1500UL;' \
  'zero keep_ms maps to the 1500 ms hang-time default'
require_file_contains "$sus_source" \
  ': 4096UL;' \
  'zero read_frames maps to bounded 4096-frame pulls'
require_file_contains "$sus_source" \
  ': 0.001f;' \
  'zero vox_threshold maps to the SUS VOX default'
require_file_contains "$sus_source" \
  'state.use_prompt = (config == NULL || config->keep_context >= 0) ? 1 : 0;' \
  'prompt carry is enabled by default and disabled only by negative keep_context'
require_file_contains "$sus_source" \
  'cpkt_sus_segmented_text_replace(state, event->text,' \
  'materialized segmented text must keep the latest cumulative event text'
require_file_contains "$sus_source" \
  'state->length = text_len;' \
  'materialized segmented text length must match the latest cumulative event'
require_file_contains "$sus_source" \
  'vox_config.max_segment_ms = length_ms;' \
  'segmented policy is delegated to cpktaudio VOX max_segment_ms'
require_file_contains "$sus_source" \
  'vox_config.prebuffer_ms = config != NULL ? config->prebuffer_ms : 0UL;' \
  'SUS passes prebuffer through and lets audio defaults apply on zero'
require_file_contains "$sus_source" \
  'vox_config.memory_spool_bytes =' \
  'SUS passes memory spool sizing into cpktaudio VOX'
require_file_contains "$sus_source" \
  'vox_config.max_spool_bytes = config != NULL ? config->max_spool_bytes : 0UL;' \
  'SUS passes the hard spool cap into cpktaudio VOX'
require_file_contains "$sus_source" \
  "impl->revised_length != 0U" \
  'segmented transcript normalization is scoped to the first emitted text only'
require_file_contains "$sus_source" \
  "text[0] == ' '" \
  'segmented transcript normalization removes only one absolute initial space'
require_file_contains "$sus_source" \
  'preserve_initial_space_after_first_transcriber' \
  'cross-transcriber initial-spacing policy is stored on the model instance'
require_file_contains "$sus_source" \
  'model_impl->segmented_initial_space_seen = 1;' \
  'first segmented stream consumes the instance-level initial-space trim'
require_file_contains "$sus_source" \
  'impl->segmented_initial_space_seen = 0;' \
  'reset clears the instance-level initial-spacing state'
require_file_lacks "$sus_source" \
  'cpkt_sus_trim_ascii_space' \
  'segmented transcription must not trim whitespace from every Whisper segment'
require_file_lacks "$sus_source" \
  'config->step_ms' \
  'step_ms must not drive repeated fixed-step whisper inference'
require_file_lacks "$sus_source" \
  'cpkt_sus_segmented_text_apply' \
  'materialized segmented text must not append cumulative event text'
