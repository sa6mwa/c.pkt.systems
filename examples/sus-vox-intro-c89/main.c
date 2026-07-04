#include <cpkt/audio.h>
#include <cpkt/sus.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CPKT_SUS_VOX_INTRO_URL                                               \
  "https://pkt.systems/trajectory/assets/narration/intro/intro.mp3"
#define CPKT_SUS_VOX_READ_FRAMES 4096UL
#define CPKT_SUS_VOX_MEMORY_SPOOL_BYTES 65536UL
#define CPKT_SUS_VOX_MAX_SPOOL_BYTES (1024UL * 1024UL * 1024UL)

struct cpkt_sus_vox_options {
  const char *audio_path;
  const char *url;
  const char *model_path;
  const char *model;
  const char *cache_dir;
  const char *language;
  float threshold;
  unsigned long hang_ms;
  unsigned long budget_ms;
  unsigned long read_frames;
  unsigned long memory_spool_bytes;
  unsigned long max_spool_bytes;
  int cpu_only;
};

struct cpkt_sus_vox_events {
  clock_t started;
  unsigned long event_count;
  unsigned long final_count;
  unsigned long last_text_length;
};

static void cpkt_sus_vox_defaults(struct cpkt_sus_vox_options *options) {
  memset(options, 0, sizeof(*options));
  options->url = CPKT_SUS_VOX_INTRO_URL;
  options->model = "tiny";
  options->language = "en";
  options->threshold = 0.03f;
  options->hang_ms = 500UL;
  options->budget_ms = 7000UL;
  options->read_frames = CPKT_SUS_VOX_READ_FRAMES;
  options->memory_spool_bytes = CPKT_SUS_VOX_MEMORY_SPOOL_BYTES;
  options->max_spool_bytes = CPKT_SUS_VOX_MAX_SPOOL_BYTES;
  options->cpu_only = 1;
}

static int cpkt_sus_vox_parse_ulong(const char *text, unsigned long *out) {
  char *end;
  unsigned long value;

  if (text == NULL || out == NULL || text[0] == '\0') {
    return 0;
  }
  end = NULL;
  value = strtoul(text, &end, 10);
  if (end == text || *end != '\0') {
    return 0;
  }
  *out = value;
  return 1;
}

static int cpkt_sus_vox_parse_float(const char *text, float *out) {
  char *end;
  double value;

  if (text == NULL || out == NULL || text[0] == '\0') {
    return 0;
  }
  end = NULL;
  value = strtod(text, &end);
  if (end == text || *end != '\0' || value < 0.0) {
    return 0;
  }
  *out = (float)value;
  return 1;
}

static void cpkt_sus_vox_print_text(const char *text, unsigned long length) {
  unsigned long i;
  int ch;

  fputc('"', stdout);
  for (i = 0UL; i < length; ++i) {
    ch = (unsigned char)text[i];
    if (ch == '\\' || ch == '"') {
      fputc('\\', stdout);
      fputc(ch, stdout);
    } else if (ch == '\n' || ch == '\r' || ch == '\t') {
      fputc(' ', stdout);
    } else if (ch >= 32 && ch < 127) {
      fputc(ch, stdout);
    } else {
      fputc('?', stdout);
    }
  }
  fputc('"', stdout);
}

static unsigned long cpkt_sus_vox_elapsed_ms(clock_t started) {
  clock_t now;

  now = clock();
  if (now == (clock_t)-1 || started == (clock_t)-1 || now < started) {
    return 0UL;
  }
  return (unsigned long)(((double)(now - started) * 1000.0) /
                         (double)CLOCKS_PER_SEC);
}

static int cpkt_sus_vox_realtime_sink(const cpkt_sus_realtime_event *event,
                                      void *user) {
  struct cpkt_sus_vox_events *events;
  unsigned long delta_offset;
  unsigned long delta_length;

  events = (struct cpkt_sus_vox_events *)user;
  if (events == NULL || event == NULL || event->text == NULL) {
    return 1;
  }
  ++events->event_count;
  if (event->is_final) {
    ++events->final_count;
  }
  delta_offset = events->last_text_length;
  if (delta_offset > event->text_length) {
    delta_offset = 0UL;
  }
  delta_length = event->text_length - delta_offset;
  printf("stream segment=%lu final=%d elapsed_ms=%lu total_chars=%lu "
         "delta_chars=%lu text=",
         event->step_index, event->is_final,
         cpkt_sus_vox_elapsed_ms(events->started), event->text_length,
         delta_length);
  cpkt_sus_vox_print_text(event->text + delta_offset, delta_length);
  printf("\n");
  events->last_text_length = event->text_length;
  return 0;
}

static int cpkt_sus_vox_progress_sink(int progress, void *user) {
  (void)user;
  printf("progress percent=%d\n", progress);
  return 0;
}

static int cpkt_sus_vox_open_audio(cpkt_audio_decoder **out,
                                   const struct cpkt_sus_vox_options *options) {
  cpkt_audio_decoder_config config;
  cpkt_audio_result result;

  if (options->audio_path != NULL) {
    result = cpkt_audio_decoder_open_file(out, options->audio_path, NULL);
  } else {
    memset(&config, 0, sizeof(config));
    config.encoding = CPKT_AUDIO_ENCODING_MP3;
    result = cpkt_audio_decoder_open_url(out, options->url, &config);
  }
  if (result != CPKT_AUDIO_OK) {
    fprintf(stderr, "audio open failed: %s\n", cpkt_audio_result_string(result));
    return 0;
  }
  return 1;
}

static int cpkt_sus_vox_open_model(cpkt_sus_model **out,
                                   const struct cpkt_sus_vox_options *options) {
  cpkt_sus_model_config path_config;
  cpkt_sus_cache_config cache_config;
  cpkt_sus_result result;

  if (options->model_path != NULL) {
    memset(&path_config, 0, sizeof(path_config));
    path_config.model_path = options->model_path;
    path_config.cpu_only = options->cpu_only;
    result = cpkt_sus_model_open_path(out, &path_config);
  } else {
    memset(&cache_config, 0, sizeof(cache_config));
    cache_config.model = options->model;
    cache_config.cache_dir = options->cache_dir;
    cache_config.cpu_only = options->cpu_only;
    result = cpkt_sus_model_open_cached(out, &cache_config);
  }
  if (result != CPKT_SUS_OK) {
    fprintf(stderr, "model open failed: %s\n", cpkt_sus_result_string(result));
    return 0;
  }
  return 1;
}

static int cpkt_sus_vox_run(const struct cpkt_sus_vox_options *options) {
  cpkt_audio_decoder *decoder;
  cpkt_sus_model *model;
  cpkt_sus_transcriber *transcriber;
  cpkt_sus_transcriber_config transcriber_config;
  cpkt_sus_realtime_config realtime_config;
  struct cpkt_sus_vox_events events;
  cpkt_sus_result result;
  char *final_text;
  int rc;

  decoder = NULL;
  model = NULL;
  transcriber = NULL;
  final_text = NULL;
  rc = 1;
  memset(&events, 0, sizeof(events));
  events.started = clock();

  if (!cpkt_sus_vox_open_audio(&decoder, options)) {
    goto cleanup;
  }
  if (!cpkt_sus_vox_open_model(&model, options)) {
    goto cleanup;
  }

  memset(&transcriber_config, 0, sizeof(transcriber_config));
  transcriber_config.language = options->language;
  transcriber_config.cpu_only = options->cpu_only;
  transcriber_config.progress_sink = cpkt_sus_vox_progress_sink;
  result = model->create_transcriber(model, &transcriber, &transcriber_config);
  if (result != CPKT_SUS_OK) {
    fprintf(stderr, "transcriber create failed: %s\n",
            cpkt_sus_result_string(result));
    goto cleanup;
  }

  memset(&realtime_config, 0, sizeof(realtime_config));
  realtime_config.read_frames = options->read_frames;
  realtime_config.length_ms = options->budget_ms;
  realtime_config.keep_ms = options->hang_ms;
  realtime_config.vox_threshold = options->threshold;
  realtime_config.memory_spool_bytes = options->memory_spool_bytes;
  realtime_config.max_spool_bytes = options->max_spool_bytes;
  realtime_config.realtime_sink = cpkt_sus_vox_realtime_sink;
  realtime_config.realtime_user = &events;

  printf("source=%s model=%s cache_dir=%s language=%s threshold=%g hang_ms=%lu "
         "budget_ms=%lu read_frames=%lu memory_spool_bytes=%lu "
         "max_spool_bytes=%lu cpu_only=%d\n",
         options->audio_path != NULL ? options->audio_path : options->url,
         options->model_path != NULL ? options->model_path : options->model,
         options->cache_dir != NULL ? options->cache_dir : "(default)",
         options->language != NULL ? options->language : "auto",
         (double)options->threshold, options->hang_ms, options->budget_ms,
         options->read_frames, options->memory_spool_bytes,
         options->max_spool_bytes, options->cpu_only);

  result = transcriber->transcribe_audio_decoder_realtime(transcriber, decoder,
                                                          &realtime_config);
  if (result != CPKT_SUS_OK) {
    fprintf(stderr, "streaming transcription failed: %s\n",
            cpkt_sus_result_string(result));
    goto cleanup;
  }
  result = transcriber->revised_text(transcriber, &final_text);
  if (result != CPKT_SUS_OK || final_text == NULL) {
    fprintf(stderr, "final text retrieval failed: %s\n",
            cpkt_sus_result_string(result));
    goto cleanup;
  }

  printf("summary segments=%lu final_events=%lu elapsed_ms=%lu final_chars=%lu\n",
         events.event_count, events.final_count,
         cpkt_sus_vox_elapsed_ms(events.started),
         (unsigned long)strlen(final_text));
  printf("final_text=");
  cpkt_sus_vox_print_text(final_text, (unsigned long)strlen(final_text));
  printf("\n");
  rc = 0;

cleanup:
  cpkt_sus_string_free(final_text);
  if (transcriber != NULL) {
    transcriber->destroy(transcriber);
  }
  if (model != NULL) {
    model->destroy(model);
  }
  if (decoder != NULL) {
    decoder->destroy(decoder);
  }
  return rc;
}

static void cpkt_sus_vox_usage(FILE *out) {
  fprintf(out, "usage: cpkt_sus_vox_intro_c89_example [options]\n\n");
  fprintf(out, "No arguments run a cheap facade smoke test.\n\n");
  fprintf(out, "Options:\n");
  fprintf(out, "  --audio PATH                 Decode a local audio file.\n");
  fprintf(out, "  --url URL                    Stream an HTTP(S) audio URL.\n");
  fprintf(out, "  --model NAME                 Cached model name; default tiny.\n");
  fprintf(out, "  --model-path PATH            Load an explicit model file.\n");
  fprintf(out, "  --cache-dir DIR              Model cache directory.\n");
  fprintf(out, "  --language CODE              Language code; default en.\n");
  fprintf(out, "  --threshold VALUE            VOX threshold; default 0.03.\n");
  fprintf(out, "  --hang-ms N                  VOX hang-time; default 500.\n");
  fprintf(out, "  --budget-ms N                VOX segment budget; default 7000.\n");
  fprintf(out, "  --read-frames N              Decoder read size; default 4096.\n");
  fprintf(out, "  --cpu-only N                 1 for CPU-only, 0 for backend default.\n");
}

static int cpkt_sus_vox_parse_options(int argc, char **argv,
                                      struct cpkt_sus_vox_options *options) {
  int i;
  unsigned long parsed;

  for (i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--help") == 0) {
      cpkt_sus_vox_usage(stdout);
      exit(0);
    } else if (strcmp(argv[i], "--audio") == 0 && i + 1 < argc) {
      options->audio_path = argv[++i];
    } else if (strcmp(argv[i], "--url") == 0 && i + 1 < argc) {
      options->url = argv[++i];
    } else if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) {
      options->model = argv[++i];
    } else if (strcmp(argv[i], "--model-path") == 0 && i + 1 < argc) {
      options->model_path = argv[++i];
    } else if (strcmp(argv[i], "--cache-dir") == 0 && i + 1 < argc) {
      options->cache_dir = argv[++i];
    } else if (strcmp(argv[i], "--language") == 0 && i + 1 < argc) {
      options->language = argv[++i];
    } else if (strcmp(argv[i], "--threshold") == 0 && i + 1 < argc) {
      if (!cpkt_sus_vox_parse_float(argv[++i], &options->threshold)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--hang-ms") == 0 && i + 1 < argc) {
      if (!cpkt_sus_vox_parse_ulong(argv[++i], &options->hang_ms)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--budget-ms") == 0 && i + 1 < argc) {
      if (!cpkt_sus_vox_parse_ulong(argv[++i], &options->budget_ms)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--read-frames") == 0 && i + 1 < argc) {
      if (!cpkt_sus_vox_parse_ulong(argv[++i], &options->read_frames)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--cpu-only") == 0 && i + 1 < argc) {
      if (!cpkt_sus_vox_parse_ulong(argv[++i], &parsed)) {
        return 0;
      }
      options->cpu_only = parsed != 0UL ? 1 : 0;
    } else {
      return 0;
    }
  }
  return 1;
}

static int cpkt_sus_vox_smoke(void) {
  cpkt_sus_model_entry entry;

  if (!cpkt_audio_format_can_decode(CPKT_AUDIO_FORMAT_MP3)) {
    return 1;
  }
  if (cpkt_sus_model_catalog_find("tiny", &entry) != CPKT_SUS_OK) {
    return 2;
  }
  if (entry.name == NULL || strcmp(entry.name, "tiny") != 0) {
    return 3;
  }
  return 0;
}

int main(int argc, char **argv) {
  struct cpkt_sus_vox_options options;

  if (argc == 1) {
    return cpkt_sus_vox_smoke();
  }
  cpkt_sus_vox_defaults(&options);
  if (!cpkt_sus_vox_parse_options(argc, argv, &options)) {
    cpkt_sus_vox_usage(stderr);
    return 64;
  }
  return cpkt_sus_vox_run(&options);
}
