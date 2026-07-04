#include <cpkt/audio.h>
#include <cpkt/sus.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

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
  int capture;
  int backend;
  cpkt_sus_segment_mode segment_mode;
  int verbose;
  unsigned long threads;
  unsigned long read_frames;
  unsigned long seconds;
  unsigned long length_ms;
  unsigned long hang_ms;
  unsigned long prebuffer_ms;
  unsigned long buffer_ms;
  unsigned long period_ms;
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

struct cpktxscribe_capture_run {
  const struct cpktxscribe_options *options;
  cpkt_sus_model *model;
  struct cpktxscribe_stream *stream;
};

static void cpktxscribe_defaults(struct cpktxscribe_options *options) {
  memset(options, 0, sizeof(*options));
  options->model = "tiny";
  options->language = "auto";
  options->encoding = CPKT_AUDIO_ENCODING_UNKNOWN;
  options->cpu_only = 1;
  options->keep_context = 1;
  options->final_newline = 1;
  options->segment_mode = CPKT_SUS_SEGMENT_MODE_CONTINUOUS;
  options->read_frames = CPKTXSCRIBE_DEFAULT_READ_FRAMES;
  options->seconds = 0UL;
  options->length_ms = 0UL;
  options->hang_ms = 1500UL;
  options->prebuffer_ms = 50UL;
  options->buffer_ms = 2000UL;
  options->period_ms = 20UL;
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

static int cpktxscribe_parse_backend(const char *text, int *out) {
  if (text == NULL || out == NULL) {
    return 0;
  }
  if (strcmp(text, "auto") == 0) {
    *out = CPKT_AUDIO_DEVICE_BACKEND_AUTO;
  } else if (strcmp(text, "process") == 0) {
    *out = CPKT_AUDIO_DEVICE_BACKEND_PROCESS;
  } else if (strcmp(text, "coreaudio") == 0) {
    *out = CPKT_AUDIO_DEVICE_BACKEND_COREAUDIO;
  } else {
    return 0;
  }
  return 1;
}

static int cpktxscribe_is_url(const char *text) {
  const char *cursor;

  if (text == NULL) {
    return 0;
  }
  cursor = strstr(text, "://");
  return cursor != NULL && cursor != text;
}

static void cpktxscribe_sus_log_verbose(const cpkt_sus_log_event *event,
                                        void *user) {
  (void)user;
  if (event != NULL && event->message != NULL) {
    fputs(event->message, stderr);
  }
}

static const char *cpktxscribe_cache_phase_name(int phase) {
  switch (phase) {
  case CPKT_SUS_CACHE_STATUS_LOOKUP:
    return "lookup";
  case CPKT_SUS_CACHE_STATUS_HIT:
    return "cached";
  case CPKT_SUS_CACHE_STATUS_MISS:
    return "missing";
  case CPKT_SUS_CACHE_STATUS_DOWNLOAD_BEGIN:
    return "download";
  case CPKT_SUS_CACHE_STATUS_DOWNLOAD_COMPLETE:
    return "downloaded";
  case CPKT_SUS_CACHE_STATUS_VERIFY_BEGIN:
    return "verify";
  case CPKT_SUS_CACHE_STATUS_VERIFY_COMPLETE:
    return "verified";
  case CPKT_SUS_CACHE_STATUS_LOAD_BEGIN:
    return "load";
  default:
    return "unknown";
  }
}

static int cpktxscribe_cache_status_sink(
    const cpkt_sus_cache_status_event *event, void *user) {
  (void)user;
  if (event == NULL) {
    return 0;
  }
  if (event->source_url != NULL) {
    fprintf(stderr, "status model_cache=%s model=%s cache=%s source=%s\n",
            cpktxscribe_cache_phase_name(event->phase),
            event->model != NULL ? event->model : "(unknown)",
            event->cache_path != NULL ? event->cache_path : "(unresolved)",
            event->source_url);
  } else {
    fprintf(stderr, "status model_cache=%s model=%s cache=%s\n",
            cpktxscribe_cache_phase_name(event->phase),
            event->model != NULL ? event->model : "(unknown)",
            event->cache_path != NULL ? event->cache_path : "(unresolved)");
  }
  return 0;
}

static char *cpktxscribe_join2(const char *left, const char *right) {
  char *joined;
  size_t left_len;
  size_t right_len;
  size_t needs_slash;

  if (left == NULL || left[0] == '\0' || right == NULL || right[0] == '\0') {
    return NULL;
  }
  left_len = strlen(left);
  right_len = strlen(right);
  needs_slash = left[left_len - 1U] == '/' ? 0U : 1U;
  if (left_len > ((size_t)-1) - needs_slash - right_len - 1U) {
    return NULL;
  }
  joined = (char *)malloc(left_len + needs_slash + right_len + 1U);
  if (joined == NULL) {
    return NULL;
  }
  memcpy(joined, left, left_len);
  if (needs_slash) {
    joined[left_len] = '/';
  }
  memcpy(joined + left_len + needs_slash, right, right_len);
  joined[left_len + needs_slash + right_len] = '\0';
  return joined;
}

static char *cpktxscribe_default_cache_dir(void) {
  const char *xdg_cache_home;
  const char *home;
  char *base;
  char *path;

  xdg_cache_home = getenv("XDG_CACHE_HOME");
  if (xdg_cache_home != NULL && xdg_cache_home[0] != '\0') {
    return cpktxscribe_join2(xdg_cache_home, "cpkt/susurro/models");
  }
  home = getenv("HOME");
  if (home == NULL || home[0] == '\0') {
    return NULL;
  }
  base = cpktxscribe_join2(home, ".cache");
  if (base == NULL) {
    return NULL;
  }
  path = cpktxscribe_join2(base, "cpkt/susurro/models");
  free(base);
  return path;
}

static int cpktxscribe_file_exists(const char *path) {
  struct stat st;

  if (path == NULL || path[0] == '\0') {
    return 0;
  }
  if (stat(path, &st) != 0) {
    return 0;
  }
  return S_ISREG(st.st_mode) ? 1 : 0;
}

static const char *
cpktxscribe_source_label(const struct cpktxscribe_options *options) {
  if (options->capture) {
    return "default-capture";
  }
  if (options->url != NULL) {
    return options->url;
  }
  return options->input_path != NULL ? options->input_path : "(none)";
}

static void
cpktxscribe_print_status(const struct cpktxscribe_options *options) {
  cpkt_sus_model_entry entry;
  const char *cache_state;
  const char *cache_dir_value;
  char *cache_dir;
  char *cache_path;

  if (options->model_path != NULL) {
    fprintf(stderr, "status source=%s model_path=%s\n",
            cpktxscribe_source_label(options), options->model_path);
    return;
  }

  cache_dir = NULL;
  cache_path = NULL;
  if (cpkt_sus_model_catalog_find(options->model, &entry) == CPKT_SUS_OK &&
      entry.filename != NULL) {
    cache_dir_value = options->cache_dir;
    if (cache_dir_value == NULL || cache_dir_value[0] == '\0') {
      cache_dir = cpktxscribe_default_cache_dir();
      cache_dir_value = cache_dir;
    }
    if (cache_dir_value != NULL) {
      cache_path = cpktxscribe_join2(cache_dir_value, entry.filename);
    }
  }

  if (cache_path != NULL) {
    cache_state = cpktxscribe_file_exists(cache_path)
                      ? "cached"
                      : (options->offline ? "missing-offline" : "download");
    fprintf(stderr, "status source=%s model=%s cache=%s cache_state=%s\n",
            cpktxscribe_source_label(options),
            options->model != NULL ? options->model : "tiny", cache_path,
            cache_state);
  } else {
    fprintf(stderr, "status source=%s model=%s cache=(unresolved)\n",
            cpktxscribe_source_label(options),
            options->model != NULL ? options->model : "tiny");
  }
  free(cache_path);
  free(cache_dir);
}

static void cpktxscribe_usage(FILE *out) {
  fprintf(out, "usage: cpktxscribe [options] input-audio\n\n");
  fprintf(out, "Streams committed transcript text to stdout as VOX segments ");
  fprintf(out, "arrive.\n\n");
  fprintf(out, "Input:\n");
  fprintf(out, "  --capture                   Open the default capture device.\n");
  fprintf(out, "  --url URL                    Stream a libcurl-supported URL.\n");
  fprintf(out, "  --encoding auto|wav|flac|mp3 Input hint; default auto.\n");
  fprintf(out, "  --backend NAME               Capture backend: auto, process, coreaudio.\n");
  fprintf(out, "  --seconds N                  Capture duration; default 0, until killed.\n");
  fprintf(out, "  --buffer-ms N                Device ring buffer; default 2000.\n");
  fprintf(out, "  --period-ms N                Device callback period; default 20.\n");
  fprintf(out, "\nModel:\n");
  fprintf(out, "  --model NAME                 Cached model name; default tiny.\n");
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
  fprintf(out, "  --hang-ms N                  Silence release time; default 1500.\n");
  fprintf(out, "  --prebuffer-ms N             VOX prebuffer; default 50.\n");
  fprintf(out, "  --segment-ms N               Segment budget; default 0, mode default.\n");
  fprintf(out, "  --simplex                    Use simplex turn mode instead of continuous.\n");
  fprintf(out, "  --read-frames N              Decoder read size; default 4096.\n");
  fprintf(out, "  --memory-spool-bytes N       RAM before spool; default 65536.\n");
  fprintf(out, "  --max-spool-bytes N          Max open VOX segment; default 1 GiB.\n");
  fprintf(out, "  --keep-context 0|1           Carry prior text prompt; default 1.\n");
  fprintf(out, "\nOutput:\n");
  fprintf(out, "  -v, --verbose                Print backend logs to stderr.\n");
  fprintf(out, "  --metrics                    Print stream metrics to stderr.\n");
  fprintf(out, "  --progress                   Print backend progress to stderr.\n");
  fprintf(out, "  --no-final-newline           Do not append a final newline.\n");
  fprintf(out, "\nExamples:\n");
  fprintf(out, "  cpktxscribe intro.mp3\n");
  fprintf(out, "  cpktxscribe --capture --simplex\n");
  fprintf(out, "  cpktxscribe https://pkt.systems/trajectory/assets/narration/intro/intro.mp3\n");
  fprintf(out, "  cpktxscribe --model small intro.wav\n");
  fprintf(out, "  cpktxscribe --model tiny.sv intro.mp3\n");
  fprintf(out, "  cpktxscribe --model-path ggml-small.bin intro.mp3\n");
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
    } else if (strcmp(argv[i], "--capture") == 0) {
      options->capture = 1;
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
    } else if (strcmp(argv[i], "--backend") == 0 && i + 1 < argc) {
      if (!cpktxscribe_parse_backend(argv[++i], &options->backend)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
      if (!cpktxscribe_parse_ulong(argv[++i], &options->seconds)) {
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
    } else if (strcmp(argv[i], "--prebuffer-ms") == 0 && i + 1 < argc) {
      if (!cpktxscribe_parse_ulong(argv[++i], &options->prebuffer_ms)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--segment-ms") == 0 && i + 1 < argc) {
      if (!cpktxscribe_parse_ulong(argv[++i], &options->length_ms)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--simplex") == 0) {
      options->segment_mode = CPKT_SUS_SEGMENT_MODE_SIMPLEX;
    } else if (strcmp(argv[i], "--read-frames") == 0 && i + 1 < argc) {
      if (!cpktxscribe_parse_ulong(argv[++i], &options->read_frames)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--buffer-ms") == 0 && i + 1 < argc) {
      if (!cpktxscribe_parse_ulong(argv[++i], &options->buffer_ms)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--period-ms") == 0 && i + 1 < argc) {
      if (!cpktxscribe_parse_ulong(argv[++i], &options->period_ms)) {
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
    } else if (strcmp(argv[i], "--verbose") == 0 ||
               strcmp(argv[i], "-v") == 0) {
      options->verbose = 1;
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
  if (!options->capture && options->url == NULL && options->input_path == NULL) {
    return 0;
  }
  if (options->capture &&
      (options->url != NULL || options->input_path != NULL)) {
    return 0;
  }
  if (options->url != NULL && options->input_path != NULL) {
    return 0;
  }
  return 1;
}

static int cpktxscribe_segmented_sink(const cpkt_sus_segmented_event *event,
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

static void cpktxscribe_sleep_ms(unsigned long ms) {
  struct timeval tv;

  tv.tv_sec = (long)(ms / 1000UL);
  tv.tv_usec = (long)((ms % 1000UL) * 1000UL);
  (void)select(0, NULL, NULL, NULL, &tv);
}

static int cpktxscribe_open_audio(cpkt_audio_decoder **out,
                                  const struct cpktxscribe_options *options) {
  cpkt_audio_decoder_config config;
  cpkt_audio_result result;

  memset(&config, 0, sizeof(config));
  config.encoding = options->encoding;
  if (options->url != NULL) {
    result = cpkt_audio_decoder_open_url(out, options->url, &config);
  } else if (cpktxscribe_is_url(options->input_path)) {
    result = cpkt_audio_decoder_open_url(out, options->input_path, &config);
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

static void cpktxscribe_fill_transcriber_config(
    cpkt_sus_transcriber_config *config,
    const struct cpktxscribe_options *options) {
  memset(config, 0, sizeof(*config));
  config->threads = (int)options->threads;
  config->cpu_only = options->cpu_only;
  config->language = options->language;
  config->translate = options->translate;
  config->timestamps = options->timestamps;
  config->initial_prompt = options->initial_prompt;
  if (options->progress) {
    config->progress_sink = cpktxscribe_progress_sink;
  }
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
    cache_config.status_sink = cpktxscribe_cache_status_sink;
    result = cpkt_sus_model_open_cached(out, &cache_config);
  }
  if (result != CPKT_SUS_OK) {
    fprintf(stderr, "model open failed: %s\n", cpkt_sus_result_string(result));
    return 0;
  }
  return 1;
}

static int cpktxscribe_capture_segment_sink(cpkt_audio_vox_segment *segment,
                                            void *user) {
  struct cpktxscribe_capture_run *run;
  cpkt_sus_transcriber *transcriber;
  cpkt_sus_transcriber_config transcriber_config;
  cpkt_sus_segmented_config segmented_config;
  cpkt_sus_result result;

  run = (struct cpktxscribe_capture_run *)user;
  if (run == NULL || run->model == NULL || run->stream == NULL ||
      segment == NULL) {
    return 1;
  }
  if (run->stream->metrics) {
    fprintf(stderr,
            "metric capture_segment=%lu frames=%lu seconds=%.3f hard=%d "
            "final=%d\n",
            segment->segment_index, (unsigned long)segment->frame_count,
            (double)segment->frame_count / 16000.0, segment->hard_cut,
            segment->is_final);
  }

  transcriber = NULL;
  cpktxscribe_fill_transcriber_config(&transcriber_config, run->options);
  result = run->model->create_transcriber(run->model, &transcriber,
                                          &transcriber_config);
  if (result != CPKT_SUS_OK) {
    return 1;
  }

  memset(&segmented_config, 0, sizeof(segmented_config));
  segmented_config.keep_context = -1;
  segmented_config.audio_ctx = run->options->audio_ctx;
  segmented_config.max_tokens = run->options->max_tokens;
  segmented_config.segmented_sink = cpktxscribe_segmented_sink;
  segmented_config.segmented_user = run->stream;
  run->stream->last_text_length = 0UL;
  result = transcriber->transcribe_audio_vox_segment(transcriber, segment,
                                                     &segmented_config);
  transcriber->destroy(transcriber);
  return result == CPKT_SUS_OK ? 0 : 1;
}

static int cpktxscribe_run_capture(const struct cpktxscribe_options *options,
                                   cpkt_sus_model *model,
                                   struct cpktxscribe_stream *stream) {
  cpkt_audio_capture *capture;
  cpkt_audio_vox *vox;
  cpkt_audio_capture_config capture_config;
  cpkt_audio_vox_config vox_config;
  struct cpktxscribe_capture_run run;
  float frames[1024];
  size_t frames_read;
  time_t end_time;
  cpkt_audio_result audio_result;
  int rc;

  capture = NULL;
  vox = NULL;
  rc = 1;
  memset(&run, 0, sizeof(run));
  run.options = options;
  run.model = model;
  run.stream = stream;

  memset(&capture_config, 0, sizeof(capture_config));
  capture_config.backend = options->backend;
  capture_config.buffer_ms = options->buffer_ms;
  capture_config.period_ms = options->period_ms;
  audio_result = cpkt_audio_capture_open_default(&capture, &capture_config);
  if (audio_result != CPKT_AUDIO_OK) {
    fprintf(stderr, "capture open failed: %s\n",
            cpkt_audio_result_string(audio_result));
    goto cleanup;
  }

  memset(&vox_config, 0, sizeof(vox_config));
  vox_config.threshold = options->vox_threshold;
  vox_config.release_silence_ms = options->hang_ms;
  vox_config.prebuffer_ms = options->prebuffer_ms;
  vox_config.max_segment_ms = options->length_ms;
  vox_config.min_segment_ms = 100UL;
  vox_config.memory_spool_bytes = options->memory_spool_bytes;
  vox_config.max_spool_bytes = options->max_spool_bytes;
  vox_config.segment_sink = cpktxscribe_capture_segment_sink;
  vox_config.segment_user = &run;
  audio_result = cpkt_audio_vox_open(&vox, &vox_config);
  if (audio_result != CPKT_AUDIO_OK) {
    fprintf(stderr, "vox open failed: %s\n", cpkt_audio_result_string(audio_result));
    goto cleanup;
  }

  audio_result = capture->start(capture);
  if (audio_result != CPKT_AUDIO_OK) {
    fprintf(stderr, "capture start failed: %s\n",
            cpkt_audio_result_string(audio_result));
    goto cleanup;
  }

  end_time = options->seconds != 0UL ? time(NULL) + (time_t)options->seconds : 0;
  while (options->seconds == 0UL || time(NULL) < end_time) {
    frames_read = 0U;
    audio_result = capture->read_f32_mono_16k(capture, frames,
                                              sizeof(frames) / sizeof(frames[0]),
                                              &frames_read);
    if (audio_result != CPKT_AUDIO_OK) {
      fprintf(stderr, "capture read failed: %s\n",
              cpkt_audio_result_string(audio_result));
      goto cleanup;
    }
    if (frames_read == 0U) {
      cpktxscribe_sleep_ms(5UL);
      continue;
    }
    audio_result = vox->push_f32_mono_16k(vox, frames, frames_read);
    if (audio_result != CPKT_AUDIO_OK) {
      fprintf(stderr, "vox push failed: %s\n",
              cpkt_audio_result_string(audio_result));
      goto cleanup;
    }
  }

  audio_result = capture->stop(capture);
  if (audio_result != CPKT_AUDIO_OK) {
    fprintf(stderr, "capture stop failed: %s\n",
            cpkt_audio_result_string(audio_result));
    goto cleanup;
  }
  audio_result = vox->flush(vox);
  if (audio_result != CPKT_AUDIO_OK) {
    fprintf(stderr, "vox flush failed: %s\n", cpkt_audio_result_string(audio_result));
    goto cleanup;
  }
  rc = 0;

cleanup:
  if (vox != NULL) {
    vox->destroy(vox);
  }
  if (capture != NULL) {
    (void)capture->stop(capture);
    capture->destroy(capture);
  }
  return rc;
}

static int cpktxscribe_run(const struct cpktxscribe_options *options) {
  cpkt_audio_decoder *decoder;
  cpkt_sus_model *model;
  cpkt_sus_transcriber *transcriber;
  cpkt_sus_transcriber_config transcriber_config;
  cpkt_sus_segmented_config segmented_config;
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

  cpktxscribe_print_status(options);
  if (options->metrics) {
    fprintf(stderr,
            "source=%s model=%s cache_dir=%s language=%s threshold=%g "
            "hang_ms=%lu segment_ms=%lu read_frames=%lu cpu_only=%d\n",
            cpktxscribe_source_label(options),
            options->model_path != NULL ? options->model_path : options->model,
            options->cache_dir != NULL ? options->cache_dir : "(default)",
            options->language != NULL ? options->language : "auto",
            (double)options->vox_threshold, options->hang_ms,
            options->length_ms, options->read_frames, options->cpu_only);
  }

  if (!cpktxscribe_open_model(&model, options)) {
    goto cleanup;
  }
  if (options->capture) {
    rc = cpktxscribe_run_capture(options, model, &stream);
    goto finish_output;
  }
  if (!cpktxscribe_open_audio(&decoder, options)) {
    goto cleanup;
  }

  cpktxscribe_fill_transcriber_config(&transcriber_config, options);
  result = model->create_transcriber(model, &transcriber, &transcriber_config);
  if (result != CPKT_SUS_OK) {
    fprintf(stderr, "transcriber create failed: %s\n",
            cpkt_sus_result_string(result));
    goto cleanup;
  }

  memset(&segmented_config, 0, sizeof(segmented_config));
  segmented_config.mode = options->segment_mode;
  segmented_config.read_frames = options->read_frames;
  segmented_config.length_ms = options->length_ms;
  segmented_config.keep_ms = options->hang_ms;
  segmented_config.keep_context = options->keep_context;
  segmented_config.vox_threshold = options->vox_threshold;
  segmented_config.memory_spool_bytes = options->memory_spool_bytes;
  segmented_config.max_spool_bytes = options->max_spool_bytes;
  segmented_config.audio_ctx = options->audio_ctx;
  segmented_config.max_tokens = options->max_tokens;
  segmented_config.segmented_sink = cpktxscribe_segmented_sink;
  segmented_config.segmented_user = &stream;

  result = transcriber->transcribe_audio_decoder_segmented(transcriber, decoder,
                                                           &segmented_config);
  if (result != CPKT_SUS_OK) {
    fprintf(stderr, "streaming transcription failed: %s\n",
            cpkt_sus_result_string(result));
    goto cleanup;
  }
  rc = 0;

finish_output:
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
  if (options.verbose) {
    cpkt_sus_log_set(cpktxscribe_sus_log_verbose, NULL);
  } else {
    cpkt_sus_log_set(NULL, NULL);
  }
  return cpktxscribe_run(&options);
}
