#include <cpkt/audio.h>
#include <cpkt/sus.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CPKT_EXAMPLE_WINDOW_FRAMES (16000UL * 30UL)
#define CPKT_EXAMPLE_READ_FRAMES 4096UL

static int cpkt_example_segment_sink(const cpkt_sus_segment *segment,
                                     void *user) {
  FILE *out;

  out = (FILE *)user;
  if (segment == NULL || segment->text == NULL || out == NULL) {
    return 1;
  }
  if (fwrite(segment->text, 1U, (size_t)segment->text_length, out) !=
      (size_t)segment->text_length) {
    return 1;
  }
  return 0;
}

static int cpkt_example_transcribe_window(cpkt_sus_transcriber *transcriber,
                                          const float *samples,
                                          unsigned long sample_count) {
  cpkt_sus_result result;

  if (sample_count == 0UL) {
    return 0;
  }
  result = transcriber->transcribe_f32_mono_16k(transcriber, samples,
                                                sample_count);
  return result == CPKT_SUS_OK ? 0 : 1;
}

static int cpkt_example_transcribe_file(const char *model_path,
                                        const char *audio_path) {
  cpkt_audio_decoder *decoder;
  cpkt_sus_model *model;
  cpkt_sus_transcriber *transcriber;
  cpkt_sus_model_config model_config;
  cpkt_sus_transcriber_config transcriber_config;
  float *window;
  float read_buffer[CPKT_EXAMPLE_READ_FRAMES];
  size_t frames_read;
  size_t window_used;
  cpkt_audio_result audio_result;
  int rc;

  decoder = NULL;
  model = NULL;
  transcriber = NULL;
  window = NULL;
  window_used = 0U;
  rc = 1;

  memset(&model_config, 0, sizeof(model_config));
  model_config.model_path = model_path;
  memset(&transcriber_config, 0, sizeof(transcriber_config));
  transcriber_config.segment_sink = cpkt_example_segment_sink;
  transcriber_config.segment_user = stdout;

  window = (float *)malloc(sizeof(float) * (size_t)CPKT_EXAMPLE_WINDOW_FRAMES);
  if (window == NULL) {
    goto cleanup;
  }
  if (cpkt_audio_decoder_open_file(&decoder, audio_path, NULL) !=
      CPKT_AUDIO_OK) {
    goto cleanup;
  }
  if (cpkt_sus_model_open_path(&model, &model_config) != CPKT_SUS_OK) {
    goto cleanup;
  }
  if (model->create_transcriber(model, &transcriber, &transcriber_config) !=
      CPKT_SUS_OK) {
    goto cleanup;
  }

  for (;;) {
    audio_result = decoder->read_f32_mono_16k(
        decoder, read_buffer, (size_t)CPKT_EXAMPLE_READ_FRAMES, &frames_read);
    if (audio_result != CPKT_AUDIO_OK && audio_result != CPKT_AUDIO_AT_END) {
      goto cleanup;
    }
    if (frames_read > 0U) {
      size_t offset;

      offset = 0U;
      while (offset < frames_read) {
        size_t available;
        size_t chunk;

        available = (size_t)CPKT_EXAMPLE_WINDOW_FRAMES - window_used;
        chunk = frames_read - offset;
        if (chunk > available) {
          chunk = available;
        }
        memcpy(window + window_used, read_buffer + offset,
               sizeof(float) * chunk);
        window_used += chunk;
        offset += chunk;
        if (window_used == (size_t)CPKT_EXAMPLE_WINDOW_FRAMES) {
          if (cpkt_example_transcribe_window(
                  transcriber, window, CPKT_EXAMPLE_WINDOW_FRAMES) != 0) {
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

  if (cpkt_example_transcribe_window(transcriber, window,
                                     (unsigned long)window_used) != 0) {
    goto cleanup;
  }
  rc = 0;

cleanup:
  if (transcriber != NULL) {
    transcriber->destroy(transcriber);
  }
  if (model != NULL) {
    model->destroy(model);
  }
  if (decoder != NULL) {
    decoder->destroy(decoder);
  }
  free(window);
  return rc;
}

static int cpkt_example_smoke(void) {
  cpkt_sus_model_entry entry;

  if (!cpkt_audio_format_can_decode(CPKT_AUDIO_FORMAT_WAV)) {
    return 1;
  }
  if (!cpkt_audio_format_can_encode(CPKT_AUDIO_FORMAT_WAV)) {
    return 2;
  }
  if (cpkt_sus_model_catalog_default(&entry) != CPKT_SUS_OK) {
    return 3;
  }
  if (entry.name == NULL || strcmp(entry.name, "small") != 0) {
    return 4;
  }
  return 0;
}

int main(int argc, char **argv) {
  if (argc == 1) {
    return cpkt_example_smoke();
  }
  if (argc != 3) {
    return 64;
  }
  return cpkt_example_transcribe_file(argv[1], argv[2]);
}
