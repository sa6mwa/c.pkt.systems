#include <cpkt/audio.h>

#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define CPKT_VOX_INTRO_URL                                                     \
  "https://pkt.systems/trajectory/assets/narration/intro/intro.mp3"
#define CPKT_VOX_DUMP_DIR "build/vox-intro-dump"
#define CPKT_VOX_SAMPLE_RATE 16000UL
#define CPKT_VOX_READ_FRAMES 1024U
#define CPKT_VOX_PATH_MAX 4096U
#define CPKT_VOX_MEMORY_SPOOL_BYTES 65536UL
#define CPKT_VOX_MAX_SPOOL_BYTES (1024UL * 1024UL * 1024UL)

struct cpkt_vox_options {
  const char *audio_path;
  const char *url;
  const char *dump_dir;
  float threshold;
  unsigned long hang_ms;
  unsigned long budget_ms;
  unsigned long prebuffer_ms;
  unsigned long memory_spool_bytes;
  unsigned long max_spool_bytes;
};

struct cpkt_vox_run {
  const struct cpkt_vox_options *options;
  FILE *summary;
  size_t input_frames_seen;
  unsigned long segment_count;
  unsigned long hard_cut_count;
  unsigned long final_count;
  int failed;
};

static void cpkt_vox_defaults(struct cpkt_vox_options *options) {
  memset(options, 0, sizeof(*options));
  options->url = CPKT_VOX_INTRO_URL;
  options->dump_dir = CPKT_VOX_DUMP_DIR;
  options->threshold = 0.03f;
  options->hang_ms = 1500UL;
  options->budget_ms = 7000UL;
  options->prebuffer_ms = 50UL;
  options->memory_spool_bytes = CPKT_VOX_MEMORY_SPOOL_BYTES;
  options->max_spool_bytes = CPKT_VOX_MAX_SPOOL_BYTES;
}

static void cpkt_vox_emit(struct cpkt_vox_run *run, const char *fmt, ...) {
  va_list ap;
  va_list ap2;

  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  va_end(ap);
  if (run != NULL && run->summary != NULL) {
    va_start(ap2, fmt);
    vfprintf(run->summary, fmt, ap2);
    va_end(ap2);
  }
}

static int cpkt_vox_parse_ulong(const char *text, unsigned long *out) {
  char *end;
  unsigned long value;

  if (text == NULL || out == NULL || text[0] == '\0') {
    return 0;
  }
  errno = 0;
  end = NULL;
  value = strtoul(text, &end, 10);
  if (errno != 0 || end == text || *end != '\0') {
    return 0;
  }
  *out = value;
  return 1;
}

static int cpkt_vox_parse_float(const char *text, float *out) {
  char *end;
  double value;

  if (text == NULL || out == NULL || text[0] == '\0') {
    return 0;
  }
  errno = 0;
  end = NULL;
  value = strtod(text, &end);
  if (errno != 0 || end == text || *end != '\0' || value < 0.0) {
    return 0;
  }
  *out = (float)value;
  return 1;
}

static int cpkt_vox_ends_with(const char *text, const char *suffix) {
  size_t text_len;
  size_t suffix_len;

  if (text == NULL || suffix == NULL) {
    return 0;
  }
  text_len = strlen(text);
  suffix_len = strlen(suffix);
  if (suffix_len > text_len) {
    return 0;
  }
  return strcmp(text + text_len - suffix_len, suffix) == 0;
}

static int cpkt_vox_join_path(char *out, size_t out_size, const char *dir,
                              const char *name) {
  size_t dir_len;
  size_t name_len;
  int needs_slash;

  if (out == NULL || out_size == 0U || dir == NULL || name == NULL) {
    return 0;
  }
  dir_len = strlen(dir);
  name_len = strlen(name);
  needs_slash = dir_len > 0U && dir[dir_len - 1U] != '/';
  if (dir_len + (needs_slash ? 1U : 0U) + name_len + 1U > out_size) {
    return 0;
  }
  memcpy(out, dir, dir_len);
  if (needs_slash) {
    out[dir_len] = '/';
    ++dir_len;
  }
  memcpy(out + dir_len, name, name_len);
  out[dir_len + name_len] = '\0';
  return 1;
}

static int cpkt_vox_mkdir_p(const char *path) {
  char tmp[CPKT_VOX_PATH_MAX];
  size_t len;
  size_t i;

  if (path == NULL || path[0] == '\0') {
    return 0;
  }
  len = strlen(path);
  if (len >= sizeof(tmp)) {
    return 0;
  }
  memcpy(tmp, path, len + 1U);
  for (i = 1U; i < len; ++i) {
    if (tmp[i] == '/') {
      tmp[i] = '\0';
      if (tmp[0] != '\0' && mkdir(tmp, 0777) != 0 && errno != EEXIST) {
        return 0;
      }
      tmp[i] = '/';
    }
  }
  if (mkdir(tmp, 0777) != 0 && errno != EEXIST) {
    return 0;
  }
  return 1;
}

static void cpkt_vox_clear_dump_dir(const char *dump_dir) {
  DIR *dir;
  struct dirent *entry;
  char path[CPKT_VOX_PATH_MAX];

  dir = opendir(dump_dir);
  if (dir == NULL) {
    return;
  }
  while ((entry = readdir(dir)) != NULL) {
    if ((strncmp(entry->d_name, "segment-", 8U) == 0 &&
         cpkt_vox_ends_with(entry->d_name, ".wav")) ||
        strcmp(entry->d_name, "summary.txt") == 0) {
      if (cpkt_vox_join_path(path, sizeof(path), dump_dir, entry->d_name)) {
        (void)remove(path);
      }
    }
  }
  closedir(dir);
}

static unsigned long cpkt_vox_frames_to_ms(size_t frames) {
  return (unsigned long)(((unsigned long)frames * 1000UL) /
                         CPKT_VOX_SAMPLE_RATE);
}

static int cpkt_vox_write_segment(cpkt_audio_vox_segment *segment,
                                  const char *path, unsigned long *peak_milli,
                                  unsigned long *mean_abs_milli) {
  cpkt_audio_encoder *encoder;
  cpkt_audio_encoder_config config;
  float frames[CPKT_VOX_READ_FRAMES];
  cpkt_audio_result result;
  size_t frames_read;
  size_t frames_written;
  size_t offset;
  size_t i;
  double peak;
  double sum_abs;
  double sample;
  double abs_sample;
  int ok;

  encoder = NULL;
  peak = 0.0;
  sum_abs = 0.0;
  ok = 0;
  memset(&config, 0, sizeof(config));
  config.format = CPKT_AUDIO_FORMAT_WAV;
  config.sample_rate = CPKT_VOX_SAMPLE_RATE;
  config.channels = 1UL;
  if (cpkt_audio_encoder_open_file(&encoder, path, &config) != CPKT_AUDIO_OK) {
    return 0;
  }
  do {
    frames_read = 0U;
    result = segment->read_f32_mono_16k(segment, frames, CPKT_VOX_READ_FRAMES,
                                        &frames_read);
    if (result != CPKT_AUDIO_OK && result != CPKT_AUDIO_AT_END) {
      goto cleanup;
    }
    for (i = 0U; i < frames_read; ++i) {
      sample = (double)frames[i];
      abs_sample = sample < 0.0 ? -sample : sample;
      if (abs_sample > peak) {
        peak = abs_sample;
      }
      sum_abs += abs_sample;
    }
    offset = 0U;
    while (offset < frames_read) {
      frames_written = 0U;
      if (encoder->write_f32(encoder, frames + offset, frames_read - offset,
                             &frames_written) != CPKT_AUDIO_OK ||
          frames_written == 0U) {
        goto cleanup;
      }
      offset += frames_written;
    }
  } while (result != CPKT_AUDIO_AT_END);
  if (encoder->close(encoder) != CPKT_AUDIO_OK) {
    goto cleanup;
  }
  if (peak_milli != NULL) {
    *peak_milli = (unsigned long)(peak * 1000.0);
  }
  if (mean_abs_milli != NULL) {
    if (segment->frame_count == 0U) {
      *mean_abs_milli = 0UL;
    } else {
      *mean_abs_milli =
          (unsigned long)((sum_abs / (double)segment->frame_count) * 1000.0);
    }
  }
  ok = 1;

cleanup:
  if (encoder != NULL) {
    encoder->destroy(encoder);
  }
  return ok;
}

static int cpkt_vox_segment_sink(cpkt_audio_vox_segment *segment, void *user) {
  struct cpkt_vox_run *run;
  char filename[64];
  char path[CPKT_VOX_PATH_MAX];
  unsigned long duration_ms;
  unsigned long start_ms;
  unsigned long end_ms;
  unsigned long peak_milli;
  unsigned long mean_abs_milli;
  const char *reason;
  const char *fits;

  run = (struct cpkt_vox_run *)user;
  if (run == NULL || segment == NULL) {
    return 1;
  }
  sprintf(filename, "segment-%04lu.wav", segment->segment_index);
  if (!cpkt_vox_join_path(path, sizeof(path), run->options->dump_dir,
                          filename)) {
    run->failed = 1;
    return 1;
  }
  peak_milli = 0UL;
  mean_abs_milli = 0UL;
  if (!cpkt_vox_write_segment(segment, path, &peak_milli, &mean_abs_milli)) {
    run->failed = 1;
    return 1;
  }
  duration_ms = cpkt_vox_frames_to_ms(segment->frame_count);
  start_ms = (unsigned long)segment->t0 * 10UL;
  end_ms = (unsigned long)segment->t1 * 10UL;
  if (segment->hard_cut) {
    reason = "budget-or-spool-hard-cut";
    ++run->hard_cut_count;
  } else if (segment->is_final) {
    reason = "eof";
    ++run->final_count;
  } else {
    reason = "hang-release";
  }
  if (run->options->budget_ms == 0UL ||
      duration_ms <= run->options->budget_ms) {
    fits = "yes";
  } else {
    fits = "no";
  }
  cpkt_vox_emit(run,
                "segment=%lu start_ms=%lu end_ms=%lu "
                "duration_ms=%lu frames=%lu reason=%s hard_cut=%d final=%d "
                "fits_budget=%s peak_milli=%lu mean_abs_milli=%lu wav=%s\n",
                segment->segment_index, start_ms, end_ms, duration_ms,
                (unsigned long)segment->frame_count, reason, segment->hard_cut,
                segment->is_final, fits, peak_milli, mean_abs_milli, path);
  ++run->segment_count;
  return 0;
}

static int cpkt_vox_print_usage(FILE *out) {
  fprintf(out, "usage: cpkt_audio_vox_intro_c89_example [options]\n");
  fprintf(out, "\n");
  fprintf(out, "No arguments run a cheap facade smoke test.\n");
  fprintf(out, "\n");
  fprintf(out, "Options:\n");
  fprintf(out, "  --audio PATH                 Decode a local audio file.\n");
  fprintf(out, "  --url URL                    Stream an HTTP(S) audio URL.\n");
  fprintf(out,
          "  --dump-dir DIR               Write summary and segment WAVs.\n");
  fprintf(out, "  --threshold VALUE            VOX open/keep threshold.\n");
  fprintf(out,
          "  --hang-ms N                  Silence hang-time before release.\n");
  fprintf(out,
          "  --budget-ms N                Max duration; 0 is unbounded.\n");
  fprintf(out,
          "  --prebuffer-ms N             Audio retained before VOX opens.\n");
  fprintf(out,
          "  --memory-spool-bytes N       RAM before VOX spills to disk.\n");
  fprintf(out,
          "  --max-spool-bytes N          Bytes before forced hard cut.\n");
  return 0;
}

static int cpkt_vox_parse_options(int argc, char **argv,
                                  struct cpkt_vox_options *options) {
  int i;

  for (i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--help") == 0) {
      cpkt_vox_print_usage(stdout);
      exit(0);
    } else if (strcmp(argv[i], "--audio") == 0 && i + 1 < argc) {
      options->audio_path = argv[++i];
    } else if (strcmp(argv[i], "--url") == 0 && i + 1 < argc) {
      options->url = argv[++i];
    } else if (strcmp(argv[i], "--dump-dir") == 0 && i + 1 < argc) {
      options->dump_dir = argv[++i];
    } else if (strcmp(argv[i], "--threshold") == 0 && i + 1 < argc) {
      if (!cpkt_vox_parse_float(argv[++i], &options->threshold)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--hang-ms") == 0 && i + 1 < argc) {
      if (!cpkt_vox_parse_ulong(argv[++i], &options->hang_ms)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--budget-ms") == 0 && i + 1 < argc) {
      if (!cpkt_vox_parse_ulong(argv[++i], &options->budget_ms)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--prebuffer-ms") == 0 && i + 1 < argc) {
      if (!cpkt_vox_parse_ulong(argv[++i], &options->prebuffer_ms)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--memory-spool-bytes") == 0 && i + 1 < argc) {
      if (!cpkt_vox_parse_ulong(argv[++i], &options->memory_spool_bytes)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--max-spool-bytes") == 0 && i + 1 < argc) {
      if (!cpkt_vox_parse_ulong(argv[++i], &options->max_spool_bytes)) {
        return 0;
      }
    } else {
      return 0;
    }
  }
  return 1;
}

static int cpkt_vox_smoke(void) {
  if (!cpkt_audio_format_can_decode(CPKT_AUDIO_FORMAT_MP3)) {
    return 1;
  }
  if (!cpkt_audio_format_can_encode(CPKT_AUDIO_FORMAT_WAV)) {
    return 2;
  }
  return 0;
}

static int cpkt_vox_run(const struct cpkt_vox_options *options) {
  cpkt_audio_decoder *decoder;
  cpkt_audio_vox *vox;
  cpkt_audio_decoder_config decoder_config;
  cpkt_audio_vox_config vox_config;
  cpkt_audio_result audio_result;
  struct cpkt_vox_run run;
  float frames[CPKT_VOX_READ_FRAMES];
  size_t frames_read;
  char summary_path[CPKT_VOX_PATH_MAX];
  const char *source;
  int rc;

  decoder = NULL;
  vox = NULL;
  rc = 1;
  memset(&run, 0, sizeof(run));
  run.options = options;
  if (!cpkt_vox_mkdir_p(options->dump_dir)) {
    fprintf(stderr, "failed to create dump dir: %s\n", options->dump_dir);
    return 1;
  }
  cpkt_vox_clear_dump_dir(options->dump_dir);
  if (!cpkt_vox_join_path(summary_path, sizeof(summary_path), options->dump_dir,
                          "summary.txt")) {
    fprintf(stderr, "dump dir path is too long: %s\n", options->dump_dir);
    return 1;
  }
  run.summary = fopen(summary_path, "wb");
  if (run.summary == NULL) {
    fprintf(stderr, "failed to open summary: %s\n", summary_path);
    return 1;
  }
  memset(&decoder_config, 0, sizeof(decoder_config));
  decoder_config.encoding = CPKT_AUDIO_ENCODING_MP3;
  if (options->audio_path != NULL) {
    source = options->audio_path;
    audio_result = cpkt_audio_decoder_open_file(&decoder, options->audio_path,
                                                &decoder_config);
  } else {
    source = options->url;
    audio_result =
        cpkt_audio_decoder_open_url(&decoder, options->url, &decoder_config);
  }
  if (audio_result != CPKT_AUDIO_OK) {
    fprintf(stderr, "failed to open audio source %s: %s\n", source,
            cpkt_audio_result_string(audio_result));
    goto cleanup;
  }
  memset(&vox_config, 0, sizeof(vox_config));
  vox_config.threshold = options->threshold;
  vox_config.release_silence_ms = options->hang_ms;
  vox_config.max_segment_ms = options->budget_ms;
  vox_config.min_segment_ms = 100UL;
  vox_config.prebuffer_ms = options->prebuffer_ms;
  vox_config.memory_spool_bytes = options->memory_spool_bytes;
  vox_config.max_spool_bytes = options->max_spool_bytes;
  vox_config.segment_sink = cpkt_vox_segment_sink;
  vox_config.segment_user = &run;
  audio_result = cpkt_audio_vox_open(&vox, &vox_config);
  if (audio_result != CPKT_AUDIO_OK) {
    fprintf(stderr, "failed to open VOX: %s\n",
            cpkt_audio_result_string(audio_result));
    goto cleanup;
  }
  cpkt_vox_emit(&run,
                "source=%s dump_dir=%s threshold=%g hang_ms=%lu "
                "budget_ms=%lu prebuffer_ms=%lu memory_spool_bytes=%lu "
                "max_spool_bytes=%lu\n",
                source, options->dump_dir, (double)options->threshold,
                options->hang_ms, options->budget_ms, options->prebuffer_ms,
                options->memory_spool_bytes, options->max_spool_bytes);
  do {
    frames_read = 0U;
    audio_result = decoder->read_f32_mono_16k(
        decoder, frames, CPKT_VOX_READ_FRAMES, &frames_read);
    if (audio_result != CPKT_AUDIO_OK && audio_result != CPKT_AUDIO_AT_END) {
      fprintf(stderr, "decode failed: %s\n",
              cpkt_audio_result_string(audio_result));
      goto cleanup;
    }
    if (frames_read > 0U) {
      run.input_frames_seen += frames_read;
      audio_result = vox->push_f32_mono_16k(vox, frames, frames_read);
      if (audio_result != CPKT_AUDIO_OK) {
        fprintf(stderr, "VOX push failed: %s\n",
                cpkt_audio_result_string(audio_result));
        goto cleanup;
      }
    }
  } while (audio_result != CPKT_AUDIO_AT_END);
  audio_result = vox->flush(vox);
  if (audio_result != CPKT_AUDIO_OK) {
    fprintf(stderr, "VOX flush failed: %s\n",
            cpkt_audio_result_string(audio_result));
    goto cleanup;
  }
  cpkt_vox_emit(&run,
                "summary segments=%lu hard_cuts=%lu final_segments=%lu "
                "decoded_ms=%lu summary=%s\n",
                run.segment_count, run.hard_cut_count, run.final_count,
                cpkt_vox_frames_to_ms(run.input_frames_seen), summary_path);
  if (run.failed || run.segment_count == 0UL) {
    goto cleanup;
  }
  rc = 0;

cleanup:
  if (vox != NULL) {
    vox->destroy(vox);
  }
  if (decoder != NULL) {
    decoder->destroy(decoder);
  }
  if (run.summary != NULL) {
    fclose(run.summary);
  }
  return rc;
}

int main(int argc, char **argv) {
  struct cpkt_vox_options options;

  if (argc == 1) {
    return cpkt_vox_smoke();
  }
  cpkt_vox_defaults(&options);
  if (!cpkt_vox_parse_options(argc, argv, &options)) {
    cpkt_vox_print_usage(stderr);
    return 64;
  }
  return cpkt_vox_run(&options);
}
