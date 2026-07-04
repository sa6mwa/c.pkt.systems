#include <stddef.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cpkt/audio.h>
#include <cpkt/sus.h>

#define CPKT_SUS_TEST_READ_FRAMES 4096UL

struct cpkt_sus_test_segmented_events {
  char text[8192];
  const char *expected;
  unsigned long count;
  unsigned long final_count;
  int matched_expected;
};

struct cpkt_sus_test_progress {
  unsigned long count;
  int last;
};

struct cpkt_sus_test_vox_shape {
  unsigned long count;
  unsigned long hard_count;
  unsigned long final_count;
};

struct cpkt_sus_test_exact_step_decoder {
  cpkt_audio_decoder decoder;
  unsigned long total_frames;
  unsigned long cursor;
  float sample_value;
};

static int cpkt_sus_test_contains_expected(const char *actual_a,
                                           const char *actual_b,
                                           const char *expected);

static int cpkt_sus_test_enabled(void) {
  const char *enabled;

  enabled = getenv("CPKT_SUS_INTEGRATION_ENABLE");
  return enabled != NULL && strcmp(enabled, "1") == 0;
}

static const char *cpkt_sus_test_env(const char *name) {
  const char *value;

  value = getenv(name);
  if (value == NULL || value[0] == '\0') {
    return NULL;
  }
  return value;
}

static unsigned long cpkt_sus_test_env_ulong(const char *name,
                                             unsigned long fallback) {
  const char *value;
  char *end;
  unsigned long parsed;

  value = cpkt_sus_test_env(name);
  if (value == NULL) {
    return fallback;
  }
  parsed = strtoul(value, &end, 10);
  if (end == value || *end != '\0') {
    return fallback;
  }
  return parsed;
}

static float cpkt_sus_test_env_float(const char *name, float fallback) {
  const char *value;
  char *end;
  double parsed;

  value = cpkt_sus_test_env(name);
  if (value == NULL) {
    return fallback;
  }
  parsed = strtod(value, &end);
  if (end == value || *end != '\0' || parsed < 0.0) {
    return fallback;
  }
  return (float)parsed;
}

static int
cpkt_sus_test_segmented_sink(const cpkt_sus_segmented_event *event, void *user) {
  struct cpkt_sus_test_segmented_events *events;
  size_t copy_size;

  events = (struct cpkt_sus_test_segmented_events *)user;
  if (events == NULL || event == NULL || event->text == NULL) {
    return 1;
  }

  ++events->count;
  if (event->is_final) {
    ++events->final_count;
  }
  copy_size = (size_t)event->text_length;
  if (copy_size >= sizeof(events->text)) {
    copy_size = sizeof(events->text) - 1U;
  }
  memcpy(events->text, event->text, copy_size);
  events->text[copy_size] = '\0';
  if (cpkt_sus_test_contains_expected(events->text, NULL,
                                      events->expected)) {
    events->matched_expected = 1;
  }
  return 0;
}

static int
cpkt_sus_test_failing_segmented_sink(const cpkt_sus_segmented_event *event,
                                    void *user) {
  unsigned long *count;

  (void)event;
  count = (unsigned long *)user;
  if (count != NULL) {
    ++*count;
  }
  return 1;
}

static int cpkt_sus_test_progress_sink(int progress, void *user) {
  struct cpkt_sus_test_progress *state;

  state = (struct cpkt_sus_test_progress *)user;
  if (state == NULL) {
    return 1;
  }
  ++state->count;
  state->last = progress;
  return 0;
}

static int cpkt_sus_test_abort_now(void *user) {
  unsigned long *count;

  count = (unsigned long *)user;
  if (count != NULL) {
    ++*count;
  }
  return 1;
}

static cpkt_audio_result
cpkt_sus_test_exact_step_read(cpkt_audio_decoder *decoder, float *frames,
                              size_t frame_capacity, size_t *frames_read) {
  struct cpkt_sus_test_exact_step_decoder *state;
  unsigned long remaining;
  size_t to_write;
  size_t i;

  if (decoder == NULL || decoder->impl == NULL || frames == NULL ||
      frames_read == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  state = (struct cpkt_sus_test_exact_step_decoder *)decoder->impl;
  if (state->cursor >= state->total_frames) {
    *frames_read = 0U;
    return CPKT_AUDIO_AT_END;
  }
  remaining = state->total_frames - state->cursor;
  to_write = frame_capacity;
  if ((unsigned long)to_write > remaining) {
    to_write = (size_t)remaining;
  }
  for (i = 0U; i < to_write; ++i) {
    frames[i] = state->sample_value;
  }
  state->cursor += (unsigned long)to_write;
  *frames_read = to_write;
  return CPKT_AUDIO_OK;
}

static int cpkt_sus_test_open_audio_decoder(cpkt_audio_decoder **out,
                                            const char *audio_path,
                                            const char *audio_url) {
  cpkt_audio_decoder_config audio_config;
  cpkt_audio_result audio_result;

  if (out == NULL) {
    return 1;
  }
  *out = NULL;
  if (audio_path != NULL) {
    audio_result = cpkt_audio_decoder_open_file(out, audio_path, NULL);
    if (audio_result != CPKT_AUDIO_OK) {
      fprintf(stderr, "failed to open audio file decoder: %s\n",
              cpkt_audio_result_string(audio_result));
      return 1;
    }
  } else if (audio_url != NULL) {
    memset(&audio_config, 0, sizeof(audio_config));
    audio_config.encoding = CPKT_AUDIO_ENCODING_MP3;
    audio_result = cpkt_audio_decoder_open_url(out, audio_url, &audio_config);
    if (audio_result != CPKT_AUDIO_OK) {
      fprintf(stderr, "failed to open audio URL decoder: %s\n",
              cpkt_audio_result_string(audio_result));
      return 1;
    }
  }
  return 0;
}

static void cpkt_sus_test_segmented_config(cpkt_sus_segmented_config *config) {
  memset(config, 0, sizeof(*config));
  config->read_frames = cpkt_sus_test_env_ulong(
      "CPKT_SUS_INTEGRATION_SEGMENTED_READ_FRAMES", CPKT_SUS_TEST_READ_FRAMES);
  config->step_ms =
      cpkt_sus_test_env_ulong("CPKT_SUS_INTEGRATION_SEGMENTED_STEP_MS", 1000UL);
  config->length_ms = cpkt_sus_test_env_ulong(
      "CPKT_SUS_INTEGRATION_SEGMENTED_LENGTH_MS", 7000UL);
  config->keep_ms =
      cpkt_sus_test_env_ulong("CPKT_SUS_INTEGRATION_SEGMENTED_KEEP_MS", 1500UL);
  config->vox_threshold =
      cpkt_sus_test_env_float("CPKT_SUS_INTEGRATION_VOX_THRESHOLD", 0.03f);
  config->memory_spool_bytes = cpkt_sus_test_env_ulong(
      "CPKT_SUS_INTEGRATION_MEMORY_SPOOL_BYTES", 65536UL);
  config->max_spool_bytes = cpkt_sus_test_env_ulong(
      "CPKT_SUS_INTEGRATION_MAX_SPOOL_BYTES", 1024UL * 1024UL * 1024UL);
}

static void cpkt_sus_test_normalize_text(char *out, size_t out_size,
                                         const char *text) {
  size_t used;
  int pending_space;

  if (out == NULL || out_size == 0U) {
    return;
  }
  out[0] = '\0';
  if (text == NULL) {
    return;
  }
  used = 0U;
  pending_space = 0;
  while (*text != '\0' && used + 1U < out_size) {
    unsigned char ch;

    ch = (unsigned char)*text;
    if (isalnum(ch)) {
      if (pending_space && used > 0U && used + 1U < out_size) {
        out[used++] = ' ';
      }
      out[used++] = (char)tolower(ch);
      pending_space = 0;
    } else if (used > 0U) {
      pending_space = 1;
    }
    ++text;
  }
  out[used] = '\0';
}

static int cpkt_sus_test_contains_expected(const char *actual_a,
                                           const char *actual_b,
                                           const char *expected) {
  char normalized_actual_a[8192];
  char normalized_actual_b[8192];
  char normalized_expected[1024];

  if (expected == NULL) {
    return 1;
  }
  if ((actual_a != NULL && strstr(actual_a, expected) != NULL) ||
      (actual_b != NULL && strstr(actual_b, expected) != NULL)) {
    return 1;
  }
  cpkt_sus_test_normalize_text(normalized_actual_a,
                               sizeof(normalized_actual_a), actual_a);
  cpkt_sus_test_normalize_text(normalized_actual_b,
                               sizeof(normalized_actual_b), actual_b);
  cpkt_sus_test_normalize_text(normalized_expected,
                               sizeof(normalized_expected), expected);
  if (normalized_expected[0] == '\0') {
    return 1;
  }
  return strstr(normalized_actual_a, normalized_expected) != NULL ||
         strstr(normalized_actual_b, normalized_expected) != NULL;
}

static int cpkt_sus_test_vox_shape_sink(cpkt_audio_vox_segment *segment,
                                        void *user) {
  struct cpkt_sus_test_vox_shape *shape;
  float frames[512];
  size_t frames_read;
  cpkt_audio_result audio_result;

  shape = (struct cpkt_sus_test_vox_shape *)user;
  if (shape == NULL || segment == NULL ||
      segment->read_f32_mono_16k == NULL) {
    return 1;
  }
  ++shape->count;
  if (segment->hard_cut) {
    ++shape->hard_count;
  }
  if (segment->is_final) {
    ++shape->final_count;
  }
  do {
    frames_read = 0U;
    audio_result =
        segment->read_f32_mono_16k(segment, frames, 512U, &frames_read);
    if (audio_result != CPKT_AUDIO_OK && audio_result != CPKT_AUDIO_AT_END) {
      return 1;
    }
  } while (audio_result != CPKT_AUDIO_AT_END);
  return 0;
}

static int cpkt_sus_test_assert_vox_shape(
    const char *audio_path, const char *audio_url,
    const cpkt_sus_segmented_config *segmented_config) {
  cpkt_audio_decoder *decoder;
  cpkt_audio_vox *vox;
  cpkt_audio_vox_config vox_config;
  struct cpkt_sus_test_vox_shape shape;
  float frames[CPKT_SUS_TEST_READ_FRAMES];
  size_t frames_read;
  unsigned long read_frames;
  unsigned long expected_segments;
  unsigned long expected_hard_cuts;
  unsigned long expected_final_segments;
  cpkt_audio_result audio_result;
  int rc;

  expected_segments = cpkt_sus_test_env_ulong(
      "CPKT_SUS_INTEGRATION_EXPECTED_VOX_SEGMENTS", 0UL);
  expected_hard_cuts = cpkt_sus_test_env_ulong(
      "CPKT_SUS_INTEGRATION_EXPECTED_VOX_HARD_CUTS", 0UL);
  expected_final_segments = cpkt_sus_test_env_ulong(
      "CPKT_SUS_INTEGRATION_EXPECTED_VOX_FINAL_SEGMENTS", 0UL);
  if (expected_segments == 0UL) {
    return 0;
  }

  decoder = NULL;
  vox = NULL;
  rc = 1;
  memset(&shape, 0, sizeof(shape));
  if (cpkt_sus_test_open_audio_decoder(&decoder, audio_path, audio_url) != 0) {
    goto cleanup;
  }
  memset(&vox_config, 0, sizeof(vox_config));
  vox_config.threshold = segmented_config->vox_threshold;
  vox_config.release_silence_ms = segmented_config->keep_ms;
  vox_config.max_segment_ms = segmented_config->length_ms;
  vox_config.min_segment_ms = 100UL;
  vox_config.memory_spool_bytes = segmented_config->memory_spool_bytes;
  vox_config.max_spool_bytes = segmented_config->max_spool_bytes;
  vox_config.segment_sink = cpkt_sus_test_vox_shape_sink;
  vox_config.segment_user = &shape;
  audio_result = cpkt_audio_vox_open(&vox, &vox_config);
  if (audio_result != CPKT_AUDIO_OK) {
    fprintf(stderr, "failed to open e2e VOX shape segmenter: %s\n",
            cpkt_audio_result_string(audio_result));
    goto cleanup;
  }
  read_frames = segmented_config->read_frames != 0UL
                    ? segmented_config->read_frames
                    : CPKT_SUS_TEST_READ_FRAMES;
  if (read_frames > CPKT_SUS_TEST_READ_FRAMES) {
    read_frames = CPKT_SUS_TEST_READ_FRAMES;
  }
  do {
    frames_read = 0U;
    audio_result =
        decoder->read_f32_mono_16k(decoder, frames, (size_t)read_frames,
                                   &frames_read);
    if (audio_result != CPKT_AUDIO_OK && audio_result != CPKT_AUDIO_AT_END) {
      fprintf(stderr, "failed to decode e2e VOX shape audio: %s\n",
              cpkt_audio_result_string(audio_result));
      goto cleanup;
    }
    if (frames_read > 0U &&
        vox->push_f32_mono_16k(vox, frames, frames_read) != CPKT_AUDIO_OK) {
      fprintf(stderr, "failed to push e2e VOX shape audio\n");
      goto cleanup;
    }
  } while (audio_result != CPKT_AUDIO_AT_END);
  if (vox->flush(vox) != CPKT_AUDIO_OK) {
    fprintf(stderr, "failed to flush e2e VOX shape audio\n");
    goto cleanup;
  }
  if (shape.count != expected_segments ||
      shape.hard_count != expected_hard_cuts ||
      shape.final_count != expected_final_segments) {
    fprintf(stderr,
            "unexpected e2e VOX shape: segments=%lu hard_cuts=%lu "
            "final_segments=%lu\n",
            shape.count, shape.hard_count, shape.final_count);
    fprintf(stderr,
            "expected e2e VOX shape: segments=%lu hard_cuts=%lu "
            "final_segments=%lu\n",
            expected_segments, expected_hard_cuts, expected_final_segments);
    goto cleanup;
  }
  printf("e2e VOX shape: segments=%lu hard_cuts=%lu final_segments=%lu\n",
         shape.count, shape.hard_count, shape.final_count);
  rc = 0;

cleanup:
  if (vox != NULL) {
    vox->destroy(vox);
  }
  if (decoder != NULL) {
    decoder->destroy(decoder);
  }
  return rc;
}

static int cpkt_sus_test_open_model(cpkt_sus_model **out) {
  const char *model_path;
  cpkt_sus_model_config path_config;
  cpkt_sus_cache_config cache_config;
  const char *cached_model;
  const char *cache_dir;
  const char *sha256;
  const char *source_url;

  model_path = cpkt_sus_test_env("CPKT_SUS_INTEGRATION_MODEL_PATH");
  if (model_path != NULL) {
    memset(&path_config, 0, sizeof(path_config));
    path_config.model_path = model_path;
    path_config.cpu_only =
        cpkt_sus_test_env("CPKT_SUS_INTEGRATION_CPU_ONLY") != NULL ? 1 : 0;
    return cpkt_sus_model_open_path(out, &path_config) == CPKT_SUS_OK ? 0 : 1;
  }

  if (cpkt_sus_test_env("CPKT_SUS_INTEGRATION_OPEN_CACHED") == NULL) {
    printf("SKIP: set CPKT_SUS_INTEGRATION_MODEL_PATH or "
           "CPKT_SUS_INTEGRATION_OPEN_CACHED=1\n");
    return 77;
  }

  memset(&cache_config, 0, sizeof(cache_config));
  cached_model = cpkt_sus_test_env("CPKT_SUS_INTEGRATION_MODEL");
  cache_dir = cpkt_sus_test_env("CPKT_SUS_INTEGRATION_CACHE_DIR");
  sha256 = cpkt_sus_test_env("CPKT_SUS_INTEGRATION_SHA256");
  source_url = cpkt_sus_test_env("CPKT_SUS_INTEGRATION_SOURCE_URL");
  cache_config.model = cached_model != NULL ? cached_model : "tiny";
  cache_config.cache_dir = cache_dir;
  cache_config.sha256 = sha256;
  cache_config.source_url = source_url;
  cache_config.cpu_only =
      cpkt_sus_test_env("CPKT_SUS_INTEGRATION_CPU_ONLY") != NULL ? 1 : 0;
  return cpkt_sus_model_open_cached(out, &cache_config) == CPKT_SUS_OK ? 0 : 1;
}

static int cpkt_sus_test_run_segmented(cpkt_sus_model *model,
                                      const char *audio_path,
                                      const char *audio_url,
                                      struct cpkt_sus_test_segmented_events *events,
                                      struct cpkt_sus_test_progress *progress,
                                      char **text_out) {
  cpkt_audio_decoder *decoder;
  cpkt_sus_transcriber *transcriber;
  cpkt_sus_transcriber_config config;
  cpkt_sus_segmented_config segmented_config;
  cpkt_sus_result sus_result;
  int rc;

  decoder = NULL;
  transcriber = NULL;
  *text_out = NULL;
  rc = 1;

  if (cpkt_sus_test_open_audio_decoder(&decoder, audio_path, audio_url) != 0) {
    goto cleanup;
  }

  memset(&config, 0, sizeof(config));
  config.language = cpkt_sus_test_env("CPKT_SUS_INTEGRATION_LANGUAGE");
  config.progress_sink = cpkt_sus_test_progress_sink;
  config.progress_user = progress;
  if (model->create_transcriber(model, &transcriber, &config) != CPKT_SUS_OK) {
    fprintf(stderr, "failed to create segmented transcriber\n");
    goto cleanup;
  }

  cpkt_sus_test_segmented_config(&segmented_config);
  segmented_config.segmented_sink = cpkt_sus_test_segmented_sink;
  segmented_config.segmented_user = events;
  sus_result = transcriber->transcribe_audio_decoder_segmented(
      transcriber, decoder, &segmented_config);
  if (sus_result != CPKT_SUS_OK) {
    fprintf(stderr, "failed to transcribe segmented audio: %s\n",
            cpkt_sus_result_string(sus_result));
    goto cleanup;
  }
  sus_result = transcriber->revised_text(transcriber, text_out);
  if (sus_result != CPKT_SUS_OK || *text_out == NULL) {
    fprintf(stderr, "failed to retrieve segmented revised text: %s\n",
            cpkt_sus_result_string(sus_result));
    goto cleanup;
  }

  rc = 0;

cleanup:
  if (transcriber != NULL) {
    transcriber->destroy(transcriber);
  }
  if (decoder != NULL) {
    decoder->destroy(decoder);
  }
  return rc;
}

static int cpkt_sus_test_run_segmented_exact_step_final_event(
    cpkt_sus_model *model) {
  struct cpkt_sus_test_exact_step_decoder decoder_state;
  struct cpkt_sus_test_segmented_events events;
  cpkt_sus_transcriber *transcriber;
  cpkt_sus_transcriber_config config;
  cpkt_sus_segmented_config segmented_config;
  cpkt_sus_result result;
  int rc;

  memset(&decoder_state, 0, sizeof(decoder_state));
  decoder_state.decoder.impl = &decoder_state;
  decoder_state.decoder.read_f32_mono_16k = cpkt_sus_test_exact_step_read;
  decoder_state.total_frames = 16000UL;
  decoder_state.sample_value = 0.2f;

  memset(&events, 0, sizeof(events));
  transcriber = NULL;
  rc = 1;

  memset(&config, 0, sizeof(config));
  config.language = cpkt_sus_test_env("CPKT_SUS_INTEGRATION_LANGUAGE");
  if (model->create_transcriber(model, &transcriber, &config) != CPKT_SUS_OK) {
    goto cleanup;
  }

  memset(&segmented_config, 0, sizeof(segmented_config));
  segmented_config.read_frames = CPKT_SUS_TEST_READ_FRAMES;
  segmented_config.step_ms = 1000UL;
  segmented_config.length_ms = 1000UL;
  segmented_config.keep_ms = 0UL;
  segmented_config.segmented_sink = cpkt_sus_test_segmented_sink;
  segmented_config.segmented_user = &events;

  result = transcriber->transcribe_audio_decoder_segmented(
      transcriber, &decoder_state.decoder, &segmented_config);
  if (result != CPKT_SUS_OK || events.count == 0UL ||
      events.final_count == 0UL) {
    fprintf(stderr, "exact-step segmented EOF did not emit a final event\n");
    goto cleanup;
  }

  rc = 0;

cleanup:
  if (transcriber != NULL) {
    transcriber->destroy(transcriber);
  }
  return rc;
}

static int cpkt_sus_test_run_segmented_callback_failure(
    cpkt_sus_model *model, const char *audio_path, const char *audio_url) {
  cpkt_audio_decoder *decoder;
  cpkt_sus_transcriber *transcriber;
  cpkt_sus_transcriber_config config;
  cpkt_sus_segmented_config segmented_config;
  unsigned long callback_count;
  cpkt_sus_result result;
  int rc;

  decoder = NULL;
  transcriber = NULL;
  callback_count = 0UL;
  rc = 1;

  if (cpkt_sus_test_open_audio_decoder(&decoder, audio_path, audio_url) != 0) {
    goto cleanup;
  }

  memset(&config, 0, sizeof(config));
  config.language = cpkt_sus_test_env("CPKT_SUS_INTEGRATION_LANGUAGE");
  if (model->create_transcriber(model, &transcriber, &config) != CPKT_SUS_OK) {
    goto cleanup;
  }

  cpkt_sus_test_segmented_config(&segmented_config);
  segmented_config.segmented_sink = cpkt_sus_test_failing_segmented_sink;
  segmented_config.segmented_user = &callback_count;
  result = transcriber->transcribe_audio_decoder_segmented(
      transcriber, decoder, &segmented_config);
  if (result != CPKT_SUS_ERR_CALLBACK || callback_count == 0UL) {
    fprintf(stderr, "segmented callback failure was not reported\n");
    goto cleanup;
  }
  rc = 0;

cleanup:
  if (transcriber != NULL) {
    transcriber->destroy(transcriber);
  }
  if (decoder != NULL) {
    decoder->destroy(decoder);
  }
  return rc;
}

static int cpkt_sus_test_run_segmented_abort(cpkt_sus_model *model,
                                            const char *audio_path,
                                            const char *audio_url) {
  cpkt_audio_decoder *decoder;
  cpkt_sus_transcriber *transcriber;
  cpkt_sus_transcriber_config config;
  cpkt_sus_segmented_config segmented_config;
  unsigned long abort_count;
  cpkt_sus_result result;
  int rc;

  decoder = NULL;
  transcriber = NULL;
  abort_count = 0UL;
  rc = 1;

  if (cpkt_sus_test_open_audio_decoder(&decoder, audio_path, audio_url) != 0) {
    goto cleanup;
  }

  memset(&config, 0, sizeof(config));
  config.language = cpkt_sus_test_env("CPKT_SUS_INTEGRATION_LANGUAGE");
  config.abort = cpkt_sus_test_abort_now;
  config.abort_user = &abort_count;
  if (model->create_transcriber(model, &transcriber, &config) != CPKT_SUS_OK) {
    goto cleanup;
  }

  cpkt_sus_test_segmented_config(&segmented_config);
  result = transcriber->transcribe_audio_decoder_segmented(
      transcriber, decoder, &segmented_config);
  if (result != CPKT_SUS_ABORTED || abort_count == 0UL) {
    fprintf(stderr, "segmented abort was not reported\n");
    goto cleanup;
  }
  rc = 0;

cleanup:
  if (transcriber != NULL) {
    transcriber->destroy(transcriber);
  }
  if (decoder != NULL) {
    decoder->destroy(decoder);
  }
  return rc;
}

int main(void) {
  cpkt_sus_model *model;
  struct cpkt_sus_test_segmented_events events;
  struct cpkt_sus_test_progress progress;
  cpkt_sus_segmented_config shape_segmented_config;
  const char *audio_path;
  const char *audio_url;
  const char *expected;
  char *text;
  unsigned long expected_segments;
  unsigned long expected_final_segments;
  int open_result;
  int rc;

  if (!cpkt_sus_test_enabled()) {
    printf("SKIP: set CPKT_SUS_INTEGRATION_ENABLE=1 to run real whisper "
           "audio integration\n");
    return 0;
  }

  audio_path = cpkt_sus_test_env("CPKT_SUS_INTEGRATION_AUDIO_PATH");
  audio_url = cpkt_sus_test_env("CPKT_SUS_INTEGRATION_AUDIO_URL");
  if (audio_path == NULL && audio_url == NULL) {
    fprintf(stderr, "CPKT_SUS_INTEGRATION_AUDIO_PATH or "
                    "CPKT_SUS_INTEGRATION_AUDIO_URL is required\n");
    return 2;
  }

  cpkt_sus_test_segmented_config(&shape_segmented_config);
  if (cpkt_sus_test_assert_vox_shape(audio_path, audio_url,
                                     &shape_segmented_config) != 0) {
    fprintf(stderr, "e2e VOX shape assertion failed\n");
    return 3;
  }

  model = NULL;
  open_result = cpkt_sus_test_open_model(&model);
  if (open_result == 77) {
    return 0;
  }
  if (open_result != 0 || model == NULL) {
    fprintf(stderr, "failed to open integration model\n");
    return 4;
  }

  memset(&events, 0, sizeof(events));
  memset(&progress, 0, sizeof(progress));
  expected = cpkt_sus_test_env("CPKT_SUS_INTEGRATION_EXPECTED_TEXT");
  events.expected = expected;
  text = NULL;
  rc = 1;

  if (cpkt_sus_test_run_segmented(model, audio_path, audio_url, &events,
                                 &progress, &text) != 0) {
    fprintf(stderr, "segmented audio transcription failed\n");
    goto cleanup;
  }
  if (events.count == 0UL || events.final_count == 0UL) {
    fprintf(stderr, "segmented transcript callback was not invoked\n");
    goto cleanup;
  }
  expected_segments = cpkt_sus_test_env_ulong(
      "CPKT_SUS_INTEGRATION_EXPECTED_VOX_SEGMENTS", 0UL);
  expected_final_segments = cpkt_sus_test_env_ulong(
      "CPKT_SUS_INTEGRATION_EXPECTED_VOX_FINAL_SEGMENTS", 0UL);
  if (expected_segments != 0UL && events.count != expected_segments) {
    fprintf(stderr, "unexpected segmented event count: expected=%lu actual=%lu\n",
            expected_segments, events.count);
    goto cleanup;
  }
  if (expected_final_segments != 0UL &&
      events.final_count != expected_final_segments) {
    fprintf(stderr,
            "unexpected segmented final event count: expected=%lu actual=%lu\n",
            expected_final_segments, events.final_count);
    goto cleanup;
  }
  if (progress.count == 0UL) {
    fprintf(stderr, "progress callback was not invoked\n");
    goto cleanup;
  }

  if (cpkt_sus_test_run_segmented_callback_failure(model, audio_path,
                                                  audio_url) != 0) {
    fprintf(stderr, "segmented callback failure transcription failed\n");
    goto cleanup;
  }

  if (cpkt_sus_test_run_segmented_abort(model, audio_path, audio_url) != 0) {
    fprintf(stderr, "segmented abort transcription failed\n");
    goto cleanup;
  }

  if (cpkt_sus_test_run_segmented_exact_step_final_event(model) != 0) {
    fprintf(stderr, "exact-step segmented final event test failed\n");
    goto cleanup;
  }

  if (!events.matched_expected ||
      !cpkt_sus_test_contains_expected(text, NULL, expected)) {
    fprintf(stderr, "expected transcript text was not found\n");
    fprintf(stderr, "latest segmented transcript: %s\n", events.text);
    fprintf(stderr, "segmented text: %s\n", text);
    goto cleanup;
  }

  rc = 0;

cleanup:
  cpkt_sus_string_free(text);
  if (model != NULL) {
    model->destroy(model);
  }
  return rc;
}
