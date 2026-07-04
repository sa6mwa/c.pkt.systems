#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cpkt/audio.h>
#include <cpkt/sus.h>

#define CPKT_SUS_TEST_READ_FRAMES 4096U
#define CPKT_SUS_TEST_WINDOW_FRAMES (16000U * 30U)
#define CPKT_SUS_TEST_MAX_MATERIALIZED_SAMPLES (16000UL * 60UL * 10UL)

struct cpkt_sus_test_segments {
  char text[8192];
  size_t used;
  unsigned long count;
};

struct cpkt_sus_test_progress {
  unsigned long count;
  int last;
};

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

static int cpkt_sus_test_segment_sink(const cpkt_sus_segment *segment,
                                      void *user) {
  struct cpkt_sus_test_segments *segments;
  size_t available;
  size_t copy_size;

  segments = (struct cpkt_sus_test_segments *)user;
  if (segments == NULL || segment == NULL || segment->text == NULL) {
    return 1;
  }

  ++segments->count;
  if (segments->used + 1U >= sizeof(segments->text)) {
    return 0;
  }
  available = sizeof(segments->text) - segments->used - 1U;
  copy_size = (size_t)segment->text_length;
  if (copy_size > available) {
    copy_size = available;
  }
  memcpy(segments->text + segments->used, segment->text, copy_size);
  segments->used += copy_size;
  segments->text[segments->used] = '\0';
  return 0;
}

static int cpkt_sus_test_progress_sink(int progress, void *user) {
  struct cpkt_sus_test_progress *state;

  state = (struct cpkt_sus_test_progress *)user;
  if (state == NULL || progress < 0 || progress > 100) {
    return 1;
  }
  ++state->count;
  state->last = progress;
  return 0;
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

static int cpkt_sus_test_run_windowed(cpkt_sus_model *model,
                                      const char *audio_path,
                                      struct cpkt_sus_test_segments *segments,
                                      struct cpkt_sus_test_progress *progress) {
  cpkt_audio_decoder *decoder;
  cpkt_sus_transcriber *transcriber;
  cpkt_sus_transcriber_config config;
  float *window;
  float read_buffer[CPKT_SUS_TEST_READ_FRAMES];
  size_t frames_read;
  size_t window_used;
  cpkt_audio_result audio_result;
  cpkt_sus_result sus_result;
  int rc;

  decoder = NULL;
  transcriber = NULL;
  window = NULL;
  window_used = 0U;
  rc = 1;

  window = (float *)malloc(sizeof(float) * CPKT_SUS_TEST_WINDOW_FRAMES);
  if (window == NULL) {
    goto cleanup;
  }
  if (cpkt_audio_decoder_open_file(&decoder, audio_path, NULL) !=
      CPKT_AUDIO_OK) {
    goto cleanup;
  }

  memset(&config, 0, sizeof(config));
  config.language = cpkt_sus_test_env("CPKT_SUS_INTEGRATION_LANGUAGE");
  config.segment_sink = cpkt_sus_test_segment_sink;
  config.segment_user = segments;
  config.progress_sink = cpkt_sus_test_progress_sink;
  config.progress_user = progress;
  if (model->create_transcriber(model, &transcriber, &config) != CPKT_SUS_OK) {
    goto cleanup;
  }

  for (;;) {
    audio_result = decoder->read_f32_mono_16k(
        decoder, read_buffer, CPKT_SUS_TEST_READ_FRAMES, &frames_read);
    if (audio_result != CPKT_AUDIO_OK && audio_result != CPKT_AUDIO_AT_END) {
      goto cleanup;
    }
    if (frames_read > 0U) {
      size_t offset;

      offset = 0U;
      while (offset < frames_read) {
        size_t available;
        size_t chunk;

        available = CPKT_SUS_TEST_WINDOW_FRAMES - window_used;
        chunk = frames_read - offset;
        if (chunk > available) {
          chunk = available;
        }
        memcpy(window + window_used, read_buffer + offset,
               sizeof(float) * chunk);
        window_used += chunk;
        offset += chunk;
        if (window_used == CPKT_SUS_TEST_WINDOW_FRAMES) {
          sus_result = transcriber->transcribe_f32_mono_16k(
              transcriber, window, (unsigned long)window_used);
          if (sus_result != CPKT_SUS_OK) {
            goto cleanup;
          }
          window_used = 0U;
        }
      }
    }
    if (audio_result == CPKT_AUDIO_AT_END) {
      break;
    }
  }

  if (window_used > 0U) {
    sus_result = transcriber->transcribe_f32_mono_16k(
        transcriber, window, (unsigned long)window_used);
    if (sus_result != CPKT_SUS_OK) {
      goto cleanup;
    }
  }

  rc = 0;

cleanup:
  if (transcriber != NULL) {
    transcriber->destroy(transcriber);
  }
  if (decoder != NULL) {
    decoder->destroy(decoder);
  }
  free(window);
  return rc;
}

static int cpkt_sus_test_materialize_samples(float **out,
                                             unsigned long *sample_count_out,
                                             const char *audio_path) {
  cpkt_audio_decoder *decoder;
  float read_buffer[CPKT_SUS_TEST_READ_FRAMES];
  float *samples;
  float *grown;
  size_t capacity;
  size_t used;
  size_t frames_read;
  cpkt_audio_result audio_result;
  int rc;

  *out = NULL;
  *sample_count_out = 0UL;
  decoder = NULL;
  samples = NULL;
  capacity = 0U;
  used = 0U;
  rc = 1;

  if (cpkt_audio_decoder_open_file(&decoder, audio_path, NULL) !=
      CPKT_AUDIO_OK) {
    goto cleanup;
  }

  for (;;) {
    audio_result = decoder->read_f32_mono_16k(
        decoder, read_buffer, CPKT_SUS_TEST_READ_FRAMES, &frames_read);
    if (audio_result != CPKT_AUDIO_OK && audio_result != CPKT_AUDIO_AT_END) {
      goto cleanup;
    }
    if (frames_read > 0U) {
      if (used + frames_read > CPKT_SUS_TEST_MAX_MATERIALIZED_SAMPLES) {
        fprintf(stderr, "audio fixture exceeds integration materialization "
                        "limit\n");
        goto cleanup;
      }
      if (used + frames_read > capacity) {
        size_t next_capacity;

        next_capacity = capacity == 0U ? 16384U : capacity * 2U;
        while (next_capacity < used + frames_read) {
          next_capacity *= 2U;
        }
        grown = (float *)realloc(samples, sizeof(float) * next_capacity);
        if (grown == NULL) {
          goto cleanup;
        }
        samples = grown;
        capacity = next_capacity;
      }
      memcpy(samples + used, read_buffer, sizeof(float) * frames_read);
      used += frames_read;
    }
    if (audio_result == CPKT_AUDIO_AT_END) {
      break;
    }
  }

  *out = samples;
  *sample_count_out = (unsigned long)used;
  samples = NULL;
  rc = 0;

cleanup:
  if (decoder != NULL) {
    decoder->destroy(decoder);
  }
  free(samples);
  return rc;
}

static int cpkt_sus_test_run_materialized(cpkt_sus_model *model,
                                          const char *audio_path,
                                          char **text_out) {
  cpkt_sus_transcriber *transcriber;
  cpkt_sus_transcriber_config config;
  float *samples;
  unsigned long sample_count;
  cpkt_sus_result result;
  int rc;

  transcriber = NULL;
  samples = NULL;
  *text_out = NULL;
  rc = 1;

  if (cpkt_sus_test_materialize_samples(&samples, &sample_count, audio_path) !=
      0) {
    goto cleanup;
  }
  if (sample_count == 0UL) {
    goto cleanup;
  }

  memset(&config, 0, sizeof(config));
  config.language = cpkt_sus_test_env("CPKT_SUS_INTEGRATION_LANGUAGE");
  if (model->create_transcriber(model, &transcriber, &config) != CPKT_SUS_OK) {
    goto cleanup;
  }

  result = transcriber->transcribe_f32_mono_16k_text(transcriber, samples,
                                                     sample_count, text_out);
  if (result != CPKT_SUS_OK || *text_out == NULL) {
    goto cleanup;
  }
  rc = 0;

cleanup:
  if (transcriber != NULL) {
    transcriber->destroy(transcriber);
  }
  free(samples);
  return rc;
}

int main(void) {
  cpkt_sus_model *model;
  struct cpkt_sus_test_segments segments;
  struct cpkt_sus_test_progress progress;
  const char *audio_path;
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

  memset(&segments, 0, sizeof(segments));
  memset(&progress, 0, sizeof(progress));
  text = NULL;
  rc = 1;

  if (cpkt_sus_test_run_windowed(model, audio_path, &segments, &progress) !=
      0) {
    fprintf(stderr, "windowed audio transcription failed\n");
    goto cleanup;
  }
  if (progress.count == 0UL) {
    fprintf(stderr, "progress callback was not invoked\n");
    goto cleanup;
  }

  if (cpkt_sus_test_run_materialized(model, audio_path, &text) != 0) {
    fprintf(stderr, "materialized audio transcription failed\n");
    goto cleanup;
  }

  expected = cpkt_sus_test_env("CPKT_SUS_INTEGRATION_EXPECTED_TEXT");
  if (expected != NULL && strstr(segments.text, expected) == NULL &&
      strstr(text, expected) == NULL) {
    fprintf(stderr, "expected transcript text was not found\n");
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
