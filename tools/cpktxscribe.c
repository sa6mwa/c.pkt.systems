#include <cpkt/audio.h>
#include <cpkt/sus.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CPKTXSCRIBE_DEFAULT_READ_FRAMES 4096UL
#define CPKTXSCRIBE_DEFAULT_MEMORY_SPOOL_BYTES 65536UL
#define CPKTXSCRIBE_DEFAULT_MAX_SPOOL_BYTES (1024UL * 1024UL * 1024UL)

struct cpktxscribe_options {
  const char *input_path;
  const char *url;
  const char *model;
  const char *model_path;
  const char *cache_dir;
  const char *sha256;
  const char *source_url;
  const char *language;
  const char *initial_prompt;
  int encoding;
  int cpu_only;
  int translate;
  int timestamps;
  int offline;
  int insecure_no_checksum;
  int keep_context;
  int metrics;
  int progress;
  int final_newline;
  int list_models;
  unsigned long threads;
  unsigned long read_frames;
  unsigned long length_ms;
  unsigned long hang_ms;
  unsigned long memory_spool_bytes;
  unsigned long max_spool_bytes;
  unsigned long audio_ctx;
  unsigned long max_tokens;
  float vox_threshold;
};

struct cpktxscribe_stream {
  clock_t started;
  unsigned long events;
  unsigned long finals;
  unsigned long last_text_length;
  int saw_output;
  int metrics;
};

static void cpktxscribe_defaults(struct cpktxscribe_options *options) {
  memset(options, 0, sizeof(*options));
  options->model = "small";
  options->language = "auto";
  options->encoding = CPKT_AUDIO_ENCODING_WAV;
  options->cpu_only = 1;
  options->keep_context = 1;
  options->final_newline = 1;
  options->read_frames = CPKTXSCRIBE_DEFAULT_READ_FRAMES;
  options->length_ms = 5000UL;
  options->hang_ms = 500UL;
  options->memory_spool_bytes = CPKTXSCRIBE_DEFAULT_MEMORY_SPOOL_BYTES;
  options->max_spool_bytes = CPKTXSCRIBE_DEFAULT_MAX_SPOOL_BYTES;
  options->vox_threshold = 0.03f;
}

static unsigned long cpktxscribe_elapsed_ms(clock_t started) {
  clock_t now;

  now = clock();
  if (now == (clock_t)-1 || started == (clock_t)-1 || now < started) {
    return 0UL;
  }
  return (unsigned long)(((double)(now - started) * 1000.0) /
                         (double)CLOCKS_PER_SEC);
}

static int cpktxscribe_parse_ulong(const char *text, unsigned long *out) {
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

static int cpktxscribe_parse_float(const char *text, float *out) {
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

static int cpktxscribe_parse_encoding(const char *text, int *out) {
  if (text == NULL || out == NULL) {
    return 0;
  }
  if (strcmp(text, "auto") == 0) {
    *out = CPKT_AUDIO_ENCODING_UNKNOWN;
  } else if (strcmp(text, "wav") == 0) {
    *out = CPKT_AUDIO_ENCODING_WAV;
  } else if (strcmp(text, "flac") == 0) {
    *out = CPKT_AUDIO_ENCODING_FLAC;
  } else if (strcmp(text, "mp3") == 0) {
    *out = CPKT_AUDIO_ENCODING_MP3;
  } else {
    return 0;
  }
  return 1;
}

static void cpktxscribe_usage(FILE *out) {
  fprintf(out, "usage: cpktxscribe [options] input.wav\n\n");
  fprintf(out, "Streams committed transcript text to stdout as VOX segments ");
  fprintf(out, "arrive.\n\n");
  fprintf(out, "Input:\n");
  fprintf(out, "  --url URL                    Stream HTTP(S) audio instead of a file.\n");
  fprintf(out, "  --encoding auto|wav|flac|mp3 Input hint; default wav.\n");
  fprintf(out, "\nModel:\n");
  fprintf(out, "  --model NAME                 Cached model name; default small.\n");
  fprintf(out, "  --model-path PATH            Load an explicit model file.\n");
  fprintf(out, "  --cache-dir DIR              Model cache directory.\n");
  fprintf(out, "  --offline                    Require an existing cached model.\n");
  fprintf(out, "  --source-url URL             Override cached model source URL.\n");
  fprintf(out, "  --sha256 HEX                 Override expected model checksum.\n");
  fprintf(out, "  --insecure-no-checksum       Disable model checksum enforcement.\n");
  fprintf(out, "  --list-models                Print curated cached models and exit.\n");
  fprintf(out, "\nTranscription:\n");
  fprintf(out, "  --language CODE              Language code; default auto.\n");
  fprintf(out, "  --threads N                  Backend thread count; default backend.\n");
  fprintf(out, "  --cpu-only 0|1               CPU-only runtime request; default 1.\n");
  fprintf(out, "  --translate                  Translate to English.\n");
  fprintf(out, "  --timestamps                 Enable backend timestamps.\n");
  fprintf(out, "  --initial-prompt TEXT        Initial backend prompt.\n");
  fprintf(out, "  --audio-ctx N                Backend audio context override.\n");
  fprintf(out, "  --max-tokens N               Backend max tokens override.\n");
  fprintf(out, "\nVOX:\n");
  fprintf(out, "  --vox-threshold VALUE        RMS threshold; default 0.03.\n");
  fprintf(out, "  --hang-ms N                  Silence release time; default 500.\n");
  fprintf(out, "  --segment-ms N               Segment budget; default 5000.\n");
  fprintf(out, "  --read-frames N              Decoder read size; default 4096.\n");
  fprintf(out, "  --memory-spool-bytes N       RAM before spool; default 65536.\n");
  fprintf(out, "  --max-spool-bytes N          Max open VOX segment; default 1 GiB.\n");
  fprintf(out, "  --keep-context 0|1           Carry prior text prompt; default 1.\n");
  fprintf(out, "\nOutput:\n");
  fprintf(out, "  --metrics                    Print stream metrics to stderr.\n");
  fprintf(out, "  --progress                   Print backend progress to stderr.\n");
  fprintf(out, "  --no-final-newline           Do not append a final newline.\n");
}

static int cpktxscribe_print_models(void) {
  unsigned long count;
  unsigned long i;
  cpkt_sus_model_entry entry;
  cpkt_sus_result result;

  count = cpkt_sus_model_catalog_count();
  for (i = 0UL; i < count; ++i) {
    result = cpkt_sus_model_catalog_entry(i, &entry);
    if (result != CPKT_SUS_OK) {
      fprintf(stderr, "model catalog failed: %s\n",
              cpkt_sus_result_string(result));
      return 1;
    }
    printf("%s\t%s\t%s\t%lu\t%s%s\n",
           entry.name != NULL ? entry.name : "",
           entry.provider != NULL ? entry.provider : "",
           entry.quantization != NULL ? entry.quantization : "",
           entry.size_bytes,
           entry.sha256 != NULL ? entry.sha256 : "",
           entry.is_default ? "\tdefault" : "");
  }
  return 0;
}

static int cpktxscribe_parse_options(int argc, char **argv,
                                     struct cpktxscribe_options *options) {
  int i;
  unsigned long value;

  for (i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      cpktxscribe_usage(stdout);
      exit(0);
    } else if (strcmp(argv[i], "--list-models") == 0) {
      options->list_models = 1;
    } else if (strcmp(argv[i], "--url") == 0 && i + 1 < argc) {
      options->url = argv[++i];
    } else if (strcmp(argv[i], "--encoding") == 0 && i + 1 < argc) {
      if (!cpktxscribe_parse_encoding(argv[++i], &options->encoding)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) {
      options->model = argv[++i];
    } else if (strcmp(argv[i], "--model-path") == 0 && i + 1 < argc) {
      options->model_path = argv[++i];
    } else if (strcmp(argv[i], "--cache-dir") == 0 && i + 1 < argc) {
      options->cache_dir = argv[++i];
    } else if (strcmp(argv[i], "--source-url") == 0 && i + 1 < argc) {
      options->source_url = argv[++i];
    } else if (strcmp(argv[i], "--sha256") == 0 && i + 1 < argc) {
      options->sha256 = argv[++i];
    } else if (strcmp(argv[i], "--offline") == 0) {
      options->offline = 1;
    } else if (strcmp(argv[i], "--insecure-no-checksum") == 0) {
      options->insecure_no_checksum = 1;
    } else if (strcmp(argv[i], "--language") == 0 && i + 1 < argc) {
      options->language = argv[++i];
    } else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
      if (!cpktxscribe_parse_ulong(argv[++i], &options->threads)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--cpu-only") == 0 && i + 1 < argc) {
      if (!cpktxscribe_parse_ulong(argv[++i], &value)) {
        return 0;
      }
      options->cpu_only = value != 0UL ? 1 : 0;
    } else if (strcmp(argv[i], "--translate") == 0) {
      options->translate = 1;
    } else if (strcmp(argv[i], "--timestamps") == 0) {
      options->timestamps = 1;
    } else if (strcmp(argv[i], "--initial-prompt") == 0 && i + 1 < argc) {
      options->initial_prompt = argv[++i];
    } else if (strcmp(argv[i], "--audio-ctx") == 0 && i + 1 < argc) {
      if (!cpktxscribe_parse_ulong(argv[++i], &options->audio_ctx)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--max-tokens") == 0 && i + 1 < argc) {
      if (!cpktxscribe_parse_ulong(argv[++i], &options->max_tokens)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--vox-threshold") == 0 && i + 1 < argc) {
      if (!cpktxscribe_parse_float(argv[++i], &options->vox_threshold)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--hang-ms") == 0 && i + 1 < argc) {
      if (!cpktxscribe_parse_ulong(argv[++i], &options->hang_ms)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--segment-ms") == 0 && i + 1 < argc) {
      if (!cpktxscribe_parse_ulong(argv[++i], &options->length_ms)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--read-frames") == 0 && i + 1 < argc) {
      if (!cpktxscribe_parse_ulong(argv[++i], &options->read_frames)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--memory-spool-bytes") == 0 &&
               i + 1 < argc) {
      if (!cpktxscribe_parse_ulong(argv[++i],
                                   &options->memory_spool_bytes)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--max-spool-bytes") == 0 &&
               i + 1 < argc) {
      if (!cpktxscribe_parse_ulong(argv[++i], &options->max_spool_bytes)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--keep-context") == 0 && i + 1 < argc) {
      if (!cpktxscribe_parse_ulong(argv[++i], &value)) {
        return 0;
      }
      options->keep_context = value != 0UL ? 1 : -1;
    } else if (strcmp(argv[i], "--metrics") == 0) {
      options->metrics = 1;
    } else if (strcmp(argv[i], "--progress") == 0) {
      options->progress = 1;
    } else if (strcmp(argv[i], "--no-final-newline") == 0) {
      options->final_newline = 0;
    } else if (argv[i][0] == '-') {
      return 0;
    } else if (options->input_path == NULL) {
      options->input_path = argv[i];
    } else {
      return 0;
    }
  }

  if (options->list_models) {
    return 1;
  }
  if (options->url == NULL && options->input_path == NULL) {
    return 0;
  }
  if (options->url != NULL && options->input_path != NULL) {
    return 0;
  }
  return 1;
}

static int cpktxscribe_realtime_sink(const cpkt_sus_realtime_event *event,
                                     void *user) {
  struct cpktxscribe_stream *stream;
  unsigned long offset;
  unsigned long length;
  const char *text;

  stream = (struct cpktxscribe_stream *)user;
  if (stream == NULL || event == NULL) {
    return 1;
  }
  text = event->text != NULL ? event->text : "";
  if (event->text == NULL && event->text_length != 0UL) {
    return 1;
  }

  offset = stream->last_text_length;
  if (offset > event->text_length) {
    offset = 0UL;
  }
  length = event->text_length - offset;
  if (length > 0UL) {
    if (fwrite(text + offset, 1, length, stdout) != length) {
      return 1;
    }
    if (fflush(stdout) != 0) {
      return 1;
    }
    stream->saw_output = 1;
  }

  ++stream->events;
  if (event->is_final) {
    ++stream->finals;
  }
  if (stream->metrics) {
    fprintf(stderr,
            "metric segment=%lu final=%d elapsed_ms=%lu total_chars=%lu "
            "delta_chars=%lu\n",
            event->step_index, event->is_final,
            cpktxscribe_elapsed_ms(stream->started), event->text_length,
            length);
  }
  stream->last_text_length = event->text_length;
  return 0;
}

static int cpktxscribe_progress_sink(int progress, void *user) {
  (void)user;
  fprintf(stderr, "progress percent=%d\n", progress);
  return 0;
}

static int cpktxscribe_open_audio(cpkt_audio_decoder **out,
                                  const struct cpktxscribe_options *options) {
  cpkt_audio_decoder_config config;
  cpkt_audio_result result;

  memset(&config, 0, sizeof(config));
  config.encoding = options->encoding;
  if (options->url != NULL) {
    result = cpkt_audio_decoder_open_url(out, options->url, &config);
  } else {
    result = cpkt_audio_decoder_open_file(out, options->input_path, &config);
  }
  if (result != CPKT_AUDIO_OK) {
    fprintf(stderr, "audio open failed: %s\n",
            cpkt_audio_result_string(result));
    return 0;
  }
  return 1;
}

static int cpktxscribe_open_model(cpkt_sus_model **out,
                                  const struct cpktxscribe_options *options) {
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
    cache_config.sha256 = options->sha256;
    cache_config.source_url = options->source_url;
    cache_config.insecure_no_checksum = options->insecure_no_checksum;
    cache_config.offline = options->offline;
    cache_config.cpu_only = options->cpu_only;
    result = cpkt_sus_model_open_cached(out, &cache_config);
  }
  if (result != CPKT_SUS_OK) {
    fprintf(stderr, "model open failed: %s\n", cpkt_sus_result_string(result));
    return 0;
  }
  return 1;
}

static int cpktxscribe_run(const struct cpktxscribe_options *options) {
  cpkt_audio_decoder *decoder;
  cpkt_sus_model *model;
  cpkt_sus_transcriber *transcriber;
  cpkt_sus_transcriber_config transcriber_config;
  cpkt_sus_realtime_config realtime_config;
  struct cpktxscribe_stream stream;
  cpkt_sus_result result;
  int rc;

  decoder = NULL;
  model = NULL;
  transcriber = NULL;
  rc = 1;
  memset(&stream, 0, sizeof(stream));
  stream.started = clock();
  stream.metrics = options->metrics;

  if (options->metrics) {
    fprintf(stderr,
            "source=%s model=%s cache_dir=%s language=%s threshold=%g "
            "hang_ms=%lu segment_ms=%lu read_frames=%lu cpu_only=%d\n",
            options->url != NULL ? options->url : options->input_path,
            options->model_path != NULL ? options->model_path : options->model,
            options->cache_dir != NULL ? options->cache_dir : "(default)",
            options->language != NULL ? options->language : "auto",
            (double)options->vox_threshold, options->hang_ms,
            options->length_ms, options->read_frames, options->cpu_only);
  }

  if (!cpktxscribe_open_audio(&decoder, options)) {
    goto cleanup;
  }
  if (!cpktxscribe_open_model(&model, options)) {
    goto cleanup;
  }

  memset(&transcriber_config, 0, sizeof(transcriber_config));
  transcriber_config.threads = (int)options->threads;
  transcriber_config.cpu_only = options->cpu_only;
  transcriber_config.language = options->language;
  transcriber_config.translate = options->translate;
  transcriber_config.timestamps = options->timestamps;
  transcriber_config.initial_prompt = options->initial_prompt;
  if (options->progress) {
    transcriber_config.progress_sink = cpktxscribe_progress_sink;
  }
  result = model->create_transcriber(model, &transcriber, &transcriber_config);
  if (result != CPKT_SUS_OK) {
    fprintf(stderr, "transcriber create failed: %s\n",
            cpkt_sus_result_string(result));
    goto cleanup;
  }

  memset(&realtime_config, 0, sizeof(realtime_config));
  realtime_config.read_frames = options->read_frames;
  realtime_config.length_ms = options->length_ms;
  realtime_config.keep_ms = options->hang_ms;
  realtime_config.keep_context = options->keep_context;
  realtime_config.vox_threshold = options->vox_threshold;
  realtime_config.memory_spool_bytes = options->memory_spool_bytes;
  realtime_config.max_spool_bytes = options->max_spool_bytes;
  realtime_config.audio_ctx = options->audio_ctx;
  realtime_config.max_tokens = options->max_tokens;
  realtime_config.realtime_sink = cpktxscribe_realtime_sink;
  realtime_config.realtime_user = &stream;

  result = transcriber->transcribe_audio_decoder_realtime(transcriber, decoder,
                                                          &realtime_config);
  if (result != CPKT_SUS_OK) {
    fprintf(stderr, "streaming transcription failed: %s\n",
            cpkt_sus_result_string(result));
    goto cleanup;
  }
  if (options->final_newline && stream.saw_output) {
    fputc('\n', stdout);
    fflush(stdout);
  }
  if (options->metrics) {
    fprintf(stderr,
            "summary events=%lu final_events=%lu elapsed_ms=%lu "
            "final_chars=%lu\n",
            stream.events, stream.finals,
            cpktxscribe_elapsed_ms(stream.started), stream.last_text_length);
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
  return rc;
}

int main(int argc, char **argv) {
  struct cpktxscribe_options options;

  cpktxscribe_defaults(&options);
  if (!cpktxscribe_parse_options(argc, argv, &options)) {
    cpktxscribe_usage(stderr);
    return 64;
  }
  if (options.list_models) {
    return cpktxscribe_print_models();
  }
  return cpktxscribe_run(&options);
}
