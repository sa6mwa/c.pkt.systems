#include <stddef.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cpkt/audio.h>
#include <cpkt/sus.h>

#define CPKT_SUS_TEST_READ_FRAMES 4096UL

struct cpkt_sus_test_realtime_events {
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

struct cpkt_sus_test_exact_step_decoder {
  cpkt_audio_decoder decoder;
  unsigned long total_frames;
  unsigned long cursor;
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

static int
cpkt_sus_test_realtime_sink(const cpkt_sus_realtime_event *event, void *user) {
  struct cpkt_sus_test_realtime_events *events;
  size_t copy_size;

  events = (struct cpkt_sus_test_realtime_events *)user;
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
cpkt_sus_test_failing_realtime_sink(const cpkt_sus_realtime_event *event,
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
    frames[i] = 0.0f;
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
  if (audio_url != NULL) {
    memset(&audio_config, 0, sizeof(audio_config));
    audio_config.encoding = CPKT_AUDIO_ENCODING_MP3;
    audio_result = cpkt_audio_decoder_open_url(out, audio_url, &audio_config);
    if (audio_result != CPKT_AUDIO_OK) {
      fprintf(stderr, "failed to open audio URL decoder: %s\n",
              cpkt_audio_result_string(audio_result));
      return 1;
    }
  } else {
    audio_result = cpkt_audio_decoder_open_file(out, audio_path, NULL);
    if (audio_result != CPKT_AUDIO_OK) {
      fprintf(stderr, "failed to open audio file decoder: %s\n",
              cpkt_audio_result_string(audio_result));
      return 1;
    }
  }
  return 0;
}

static void cpkt_sus_test_realtime_config(cpkt_sus_realtime_config *config) {
  memset(config, 0, sizeof(*config));
  config->read_frames = cpkt_sus_test_env_ulong(
      "CPKT_SUS_INTEGRATION_REALTIME_READ_FRAMES", CPKT_SUS_TEST_READ_FRAMES);
  config->step_ms =
      cpkt_sus_test_env_ulong("CPKT_SUS_INTEGRATION_REALTIME_STEP_MS", 1000UL);
  config->length_ms = cpkt_sus_test_env_ulong(
      "CPKT_SUS_INTEGRATION_REALTIME_LENGTH_MS", 1000UL);
  config->keep_ms =
      cpkt_sus_test_env_ulong("CPKT_SUS_INTEGRATION_REALTIME_KEEP_MS", 200UL);
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

static int cpkt_sus_test_run_realtime(cpkt_sus_model *model,
                                      const char *audio_path,
                                      const char *audio_url,
                                      struct cpkt_sus_test_realtime_events *events,
                                      struct cpkt_sus_test_progress *progress,
                                      char **text_out) {
  cpkt_audio_decoder *decoder;
  cpkt_sus_transcriber *transcriber;
  cpkt_sus_transcriber_config config;
  cpkt_sus_realtime_config realtime_config;
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
    fprintf(stderr, "failed to create realtime transcriber\n");
    goto cleanup;
  }

  cpkt_sus_test_realtime_config(&realtime_config);
  realtime_config.realtime_sink = cpkt_sus_test_realtime_sink;
  realtime_config.realtime_user = events;
  sus_result = transcriber->transcribe_audio_decoder_realtime_text(
      transcriber, decoder, &realtime_config, text_out);
  if (sus_result != CPKT_SUS_OK || *text_out == NULL) {
    fprintf(stderr, "failed to transcribe realtime audio: %s\n",
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

static int cpkt_sus_test_run_realtime_exact_step_final_event(
    cpkt_sus_model *model) {
  struct cpkt_sus_test_exact_step_decoder decoder_state;
  struct cpkt_sus_test_realtime_events events;
  cpkt_sus_transcriber *transcriber;
  cpkt_sus_transcriber_config config;
  cpkt_sus_realtime_config realtime_config;
  cpkt_sus_result result;
  int rc;

  memset(&decoder_state, 0, sizeof(decoder_state));
  decoder_state.decoder.impl = &decoder_state;
  decoder_state.decoder.read_f32_mono_16k = cpkt_sus_test_exact_step_read;
  decoder_state.total_frames = 16000UL;

  memset(&events, 0, sizeof(events));
  transcriber = NULL;
  rc = 1;

  memset(&config, 0, sizeof(config));
  config.language = cpkt_sus_test_env("CPKT_SUS_INTEGRATION_LANGUAGE");
  if (model->create_transcriber(model, &transcriber, &config) != CPKT_SUS_OK) {
    goto cleanup;
  }

  memset(&realtime_config, 0, sizeof(realtime_config));
  realtime_config.read_frames = CPKT_SUS_TEST_READ_FRAMES;
  realtime_config.step_ms = 1000UL;
  realtime_config.length_ms = 1000UL;
  realtime_config.keep_ms = 0UL;
  realtime_config.realtime_sink = cpkt_sus_test_realtime_sink;
  realtime_config.realtime_user = &events;

  result = transcriber->transcribe_audio_decoder_realtime(
      transcriber, &decoder_state.decoder, &realtime_config);
  if (result != CPKT_SUS_OK || events.count == 0UL ||
      events.final_count == 0UL) {
    fprintf(stderr, "exact-step realtime EOF did not emit a final event\n");
    goto cleanup;
  }

  rc = 0;

cleanup:
  if (transcriber != NULL) {
    transcriber->destroy(transcriber);
  }
  return rc;
}

static int cpkt_sus_test_run_realtime_callback_failure(
    cpkt_sus_model *model, const char *audio_path, const char *audio_url) {
  cpkt_audio_decoder *decoder;
  cpkt_sus_transcriber *transcriber;
  cpkt_sus_transcriber_config config;
  cpkt_sus_realtime_config realtime_config;
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

  cpkt_sus_test_realtime_config(&realtime_config);
  realtime_config.realtime_sink = cpkt_sus_test_failing_realtime_sink;
  realtime_config.realtime_user = &callback_count;
  result = transcriber->transcribe_audio_decoder_realtime(
      transcriber, decoder, &realtime_config);
  if (result != CPKT_SUS_ERR_CALLBACK || callback_count == 0UL) {
    fprintf(stderr, "realtime callback failure was not reported\n");
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

static int cpkt_sus_test_run_realtime_abort(cpkt_sus_model *model,
                                            const char *audio_path,
                                            const char *audio_url) {
  cpkt_audio_decoder *decoder;
  cpkt_sus_transcriber *transcriber;
  cpkt_sus_transcriber_config config;
  cpkt_sus_realtime_config realtime_config;
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

  cpkt_sus_test_realtime_config(&realtime_config);
  result = transcriber->transcribe_audio_decoder_realtime(
      transcriber, decoder, &realtime_config);
  if (result != CPKT_SUS_ABORTED || abort_count == 0UL) {
    fprintf(stderr, "realtime abort was not reported\n");
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
  struct cpkt_sus_test_realtime_events events;
  struct cpkt_sus_test_progress progress;
  const char *audio_path;
  const char *audio_url;
  const char *expected;
  char *text;
  int open_result;
  int rc;

  if (!cpkt_sus_test_enabled()) {
    printf("SKIP: set CPKT_SUS_INTEGRATION_ENABLE=1 to run real whisper "
           "audio integration\n");
    return 0;
  }

  audio_path = cpkt_sus_test_env("CPKT_SUS_INTEGRATION_AUDIO_PATH");
  audio_url = cpkt_sus_test_env("CPKT_SUS_INTEGRATION_AUDIO_URL");
  if (audio_path == NULL) {
    fprintf(stderr, "CPKT_SUS_INTEGRATION_AUDIO_PATH is required\n");
    return 2;
  }

  model = NULL;
  open_result = cpkt_sus_test_open_model(&model);
  if (open_result == 77) {
    return 0;
  }
  if (open_result != 0 || model == NULL) {
    fprintf(stderr, "failed to open integration model\n");
    return 3;
  }

  memset(&events, 0, sizeof(events));
  memset(&progress, 0, sizeof(progress));
  expected = cpkt_sus_test_env("CPKT_SUS_INTEGRATION_EXPECTED_TEXT");
  events.expected = expected;
  text = NULL;
  rc = 1;

  if (cpkt_sus_test_run_realtime(model, audio_path, audio_url, &events,
                                 &progress, &text) != 0) {
    fprintf(stderr, "realtime audio transcription failed\n");
    goto cleanup;
  }
  if (events.count == 0UL || events.final_count == 0UL) {
    fprintf(stderr, "realtime hypothesis callback was not invoked\n");
    goto cleanup;
  }
  if (progress.count == 0UL) {
    fprintf(stderr, "progress callback was not invoked\n");
    goto cleanup;
  }

  if (cpkt_sus_test_run_realtime_callback_failure(model, audio_path,
                                                  audio_url) != 0) {
    fprintf(stderr, "realtime callback failure transcription failed\n");
    goto cleanup;
  }

  if (cpkt_sus_test_run_realtime_abort(model, audio_path, audio_url) != 0) {
    fprintf(stderr, "realtime abort transcription failed\n");
    goto cleanup;
  }

  if (cpkt_sus_test_run_realtime_exact_step_final_event(model) != 0) {
    fprintf(stderr, "exact-step realtime final event test failed\n");
    goto cleanup;
  }

  if (!events.matched_expected ||
      !cpkt_sus_test_contains_expected(text, NULL, expected)) {
    fprintf(stderr, "expected transcript text was not found\n");
    fprintf(stderr, "latest realtime hypothesis: %s\n", events.text);
    fprintf(stderr, "realtime text: %s\n", text);
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
