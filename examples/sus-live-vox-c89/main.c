#include <cpkt/audio.h>
#include <cpkt/sus.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define CPKT_SUS_LIVE_READ_FRAMES 512U
#define CPKT_SUS_LIVE_PATH_MAX 4096

struct cpkt_sus_live_options {
  int ptt;
  int smoke;
  int backend;
  int cpu_only;
  int offline;
  unsigned long seconds;
  unsigned long threshold_milli;
  unsigned long hang_ms;
  unsigned long prebuffer_ms;
  unsigned long max_segment_ms;
  unsigned long buffer_ms;
  unsigned long period_ms;
  const char *dump_dir;
  const char *model;
  const char *model_path;
  const char *cache_dir;
  const char *source_url;
  const char *language;
};

struct cpkt_sus_live_run {
  const struct cpkt_sus_live_options *options;
  cpkt_sus_model *model;
  unsigned long current_segment_index;
  unsigned long pending_rx_segment;
  unsigned long segment_count;
  unsigned long hard_count;
  unsigned long final_count;
  int pending_rx;
  int pending_rx_has_segment;
  FILE *summary;
};

static struct termios cpkt_sus_live_saved_tty;
static int cpkt_sus_live_raw_tty = 0;
static volatile sig_atomic_t cpkt_sus_live_stop = 0;

static void cpkt_sus_live_restore_tty(void) {
  if (cpkt_sus_live_raw_tty) {
    (void)tcsetattr(STDIN_FILENO, TCSANOW, &cpkt_sus_live_saved_tty);
    cpkt_sus_live_raw_tty = 0;
  }
}

static void cpkt_sus_live_signal_stop(int signum) {
  (void)signum;
  cpkt_sus_live_stop = 1;
}

static int cpkt_sus_live_enable_raw_tty(void) {
  struct termios raw;

  if (!isatty(STDIN_FILENO)) {
    return 0;
  }
  if (tcgetattr(STDIN_FILENO, &cpkt_sus_live_saved_tty) != 0) {
    return 0;
  }
  raw = cpkt_sus_live_saved_tty;
  raw.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
    return 0;
  }
  cpkt_sus_live_raw_tty = 1;
  (void)atexit(cpkt_sus_live_restore_tty);
  (void)signal(SIGINT, cpkt_sus_live_signal_stop);
  return 1;
}

static void cpkt_sus_live_defaults(struct cpkt_sus_live_options *opts) {
  memset(opts, 0, sizeof(*opts));
  opts->cpu_only = 1;
  opts->seconds = 0UL;
  opts->threshold_milli = 60UL;
  opts->hang_ms = 1500UL;
  opts->prebuffer_ms = 50UL;
  opts->max_segment_ms = 0UL;
  opts->buffer_ms = 2000UL;
  opts->period_ms = 20UL;
  opts->dump_dir = NULL;
  opts->model = "tiny";
  opts->language = "en";
}

static float cpkt_sus_live_threshold(const struct cpkt_sus_live_options *opts) {
  return opts->threshold_milli != 0UL ? (float)opts->threshold_milli / 1000.0f
                                      : 0.06f;
}

static void cpkt_sus_live_sleep_ms(unsigned long ms) {
  struct timeval tv;

  tv.tv_sec = (long)(ms / 1000UL);
  tv.tv_usec = (long)((ms % 1000UL) * 1000UL);
  (void)select(0, NULL, NULL, NULL, &tv);
}

static int cpkt_sus_live_parse_ulong(const char *text, unsigned long *out) {
  char *end;
  unsigned long value;

  if (text == NULL || out == NULL || text[0] == '\0') {
    return 0;
  }
  end = NULL;
  value = strtoul(text, &end, 10);
  if (end == text || end == NULL || end[0] != '\0') {
    return 0;
  }
  *out = value;
  return 1;
}

static int cpkt_sus_live_parse_backend(const char *text, int *out) {
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

static char *cpkt_sus_live_join2(const char *left, const char *right) {
  char *out;
  size_t left_len;
  size_t right_len;
  int needs_slash;

  if (left == NULL || right == NULL) {
    return NULL;
  }
  left_len = strlen(left);
  right_len = strlen(right);
  needs_slash = left_len > 0U && left[left_len - 1U] != '/';
  out = (char *)malloc(left_len + (needs_slash ? 1U : 0U) + right_len + 1U);
  if (out == NULL) {
    return NULL;
  }
  memcpy(out, left, left_len);
  if (needs_slash) {
    out[left_len] = '/';
    ++left_len;
  }
  memcpy(out + left_len, right, right_len);
  out[left_len + right_len] = '\0';
  return out;
}

static char *cpkt_sus_live_default_cache_dir(void) {
  const char *xdg_cache_home;
  const char *home;
  char *base;
  char *path;

  xdg_cache_home = getenv("XDG_CACHE_HOME");
  if (xdg_cache_home != NULL && xdg_cache_home[0] != '\0') {
    return cpkt_sus_live_join2(xdg_cache_home, "cpkt/susurro/models");
  }
  home = getenv("HOME");
  if (home == NULL || home[0] == '\0') {
    return NULL;
  }
  base = cpkt_sus_live_join2(home, ".cache");
  if (base == NULL) {
    return NULL;
  }
  path = cpkt_sus_live_join2(base, "cpkt/susurro/models");
  free(base);
  return path;
}

static int cpkt_sus_live_file_exists(const char *path) {
  struct stat st;

  if (path == NULL || stat(path, &st) != 0) {
    return 0;
  }
  return S_ISREG(st.st_mode) ? 1 : 0;
}

static const char *cpkt_sus_live_cache_phase_name(int phase) {
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

static int
cpkt_sus_live_cache_status_sink(const cpkt_sus_cache_status_event *event,
                                void *user) {
  (void)user;
  if (event == NULL) {
    return 0;
  }
  if (event->source_url != NULL) {
    fprintf(stderr, "status model_cache=%s model=%s cache=%s source=%s\n",
            cpkt_sus_live_cache_phase_name(event->phase),
            event->model != NULL ? event->model : "(unknown)",
            event->cache_path != NULL ? event->cache_path : "(unresolved)",
            event->source_url);
  } else {
    fprintf(stderr, "status model_cache=%s model=%s cache=%s\n",
            cpkt_sus_live_cache_phase_name(event->phase),
            event->model != NULL ? event->model : "(unknown)",
            event->cache_path != NULL ? event->cache_path : "(unresolved)");
  }
  return 0;
}

static void
cpkt_sus_live_print_startup(const struct cpkt_sus_live_options *opts) {
  cpkt_sus_model_entry entry;
  const char *cache_dir_value;
  const char *cache_state;
  char *cache_dir;
  char *cache_path;

  if (opts == NULL) {
    return;
  }
  if (opts->model_path != NULL) {
    fprintf(stderr,
            "status source=default-capture mode=%s model_path=%s "
            "language=%s threshold=%.3f hang_ms=%lu prebuffer_ms=%lu\n",
            opts->ptt ? "ptt" : "vox", opts->model_path,
            opts->language != NULL ? opts->language : "auto",
            (double)cpkt_sus_live_threshold(opts), opts->hang_ms,
            opts->prebuffer_ms);
    return;
  }

  cache_dir = NULL;
  cache_path = NULL;
  if (cpkt_sus_model_catalog_find(opts->model, &entry) == CPKT_SUS_OK &&
      entry.filename != NULL) {
    cache_dir_value = opts->cache_dir;
    if (cache_dir_value == NULL || cache_dir_value[0] == '\0') {
      cache_dir = cpkt_sus_live_default_cache_dir();
      cache_dir_value = cache_dir;
    }
    if (cache_dir_value != NULL) {
      cache_path = cpkt_sus_live_join2(cache_dir_value, entry.filename);
    }
  }

  if (cache_path != NULL) {
    if (cpkt_sus_live_file_exists(cache_path)) {
      cache_state = "cached";
    } else {
      cache_state = opts->offline ? "missing-offline" : "download";
    }
    fprintf(stderr,
            "status source=default-capture mode=%s model=%s cache=%s "
            "cache_state=%s language=%s threshold=%.3f hang_ms=%lu "
            "prebuffer_ms=%lu\n",
            opts->ptt ? "ptt" : "vox", opts->model != NULL ? opts->model : "tiny",
            cache_path, cache_state,
            opts->language != NULL ? opts->language : "auto",
            (double)cpkt_sus_live_threshold(opts), opts->hang_ms,
            opts->prebuffer_ms);
    if (strcmp(cache_state, "download") == 0) {
      fprintf(stderr, "status model_cache=download model=%s cache=%s source=%s\n",
              opts->model != NULL ? opts->model : "tiny", cache_path,
              opts->source_url != NULL ? opts->source_url
                                       : (entry.source_url != NULL
                                              ? entry.source_url
                                              : "(unknown)"));
    }
  } else {
    fprintf(stderr,
            "status source=default-capture mode=%s model=%s "
            "cache=(unresolved) language=%s threshold=%.3f hang_ms=%lu "
            "prebuffer_ms=%lu\n",
            opts->ptt ? "ptt" : "vox", opts->model != NULL ? opts->model : "tiny",
            opts->language != NULL ? opts->language : "auto",
            (double)cpkt_sus_live_threshold(opts), opts->hang_ms,
            opts->prebuffer_ms);
  }

  free(cache_path);
  free(cache_dir);
}

static int cpkt_sus_live_join_path(char *out, size_t out_size, const char *dir,
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
  }
  memcpy(out + dir_len + (needs_slash ? 1U : 0U), name, name_len);
  out[dir_len + (needs_slash ? 1U : 0U) + name_len] = '\0';
  return 1;
}

static void cpkt_sus_live_emit(struct cpkt_sus_live_run *run,
                               const char *text) {
  fputs(text, stdout);
  fflush(stdout);
  if (run != NULL && run->summary != NULL) {
    fputs(text, run->summary);
    fflush(run->summary);
  }
}

static int cpkt_sus_live_state_sink(const cpkt_audio_vox_state_event *event,
                                    void *user) {
  struct cpkt_sus_live_run *run;
  char line[160];

  run = (struct cpkt_sus_live_run *)user;
  if (run == NULL || event == NULL) {
    return 1;
  }
  if (event->state == CPKT_AUDIO_VOX_TX_ON) {
    run->pending_rx = 0;
    run->pending_rx_has_segment = 0;
    sprintf(line, "TX segment=%lu threshold=%.3f\n", event->segment_index,
            (double)event->threshold);
  } else if (event->state == CPKT_AUDIO_VOX_TX_OFF) {
    run->pending_rx = 1;
    run->pending_rx_has_segment = 1;
    run->pending_rx_segment = event->segment_index;
    return 0;
  } else if (event->state == CPKT_AUDIO_VOX_HARD_CUT) {
    sprintf(line, "TX hard-cut segment=%lu max_segment_ms=%lu\n",
            event->segment_index, run->options->max_segment_ms);
  } else {
    sprintf(line, "VOX state=%d segment=%lu\n", event->state,
            event->segment_index);
  }
  cpkt_sus_live_emit(run, line);
  return 0;
}

static int cpkt_sus_live_segmented_sink(const cpkt_sus_segmented_event *event,
                                       void *user) {
  struct cpkt_sus_live_run *run;
  char line[256];

  run = (struct cpkt_sus_live_run *)user;
  if (run == NULL || event == NULL || event->text == NULL) {
    return 1;
  }
  sprintf(line, "TXT segment=%lu t0=%ld t1=%ld final=%d chars=%lu: ",
          run->current_segment_index, event->t0, event->t1, event->is_final,
          event->text_length);
  cpkt_sus_live_emit(run, line);
  cpkt_sus_live_emit(run, event->text);
  cpkt_sus_live_emit(run, "\n");
  return 0;
}

static int cpkt_sus_live_segment_sink(cpkt_audio_vox_segment *segment,
                                      void *user) {
  struct cpkt_sus_live_run *run;
  char line[256];
  cpkt_sus_transcriber *transcriber;
  cpkt_sus_transcriber_config transcriber_config;
  cpkt_sus_segmented_config segmented_config;
  cpkt_sus_result result;

  run = (struct cpkt_sus_live_run *)user;
  if (run == NULL || run->model == NULL || segment == NULL) {
    return 1;
  }
  sprintf(line,
          "segment index=%lu t0=%ld t1=%ld frames=%lu seconds=%.3f hard=%d "
          "final=%d\n",
          segment->segment_index, segment->t0, segment->t1,
          (unsigned long)segment->frame_count,
          (double)segment->frame_count / 16000.0,
          segment->hard_cut, segment->is_final);
  cpkt_sus_live_emit(run, line);

  transcriber = NULL;
  memset(&transcriber_config, 0, sizeof(transcriber_config));
  transcriber_config.language = run->options->language;
  transcriber_config.cpu_only = run->options->cpu_only;
  result = run->model->create_transcriber(run->model, &transcriber,
                                          &transcriber_config);
  if (result != CPKT_SUS_OK) {
    return 1;
  }

  memset(&segmented_config, 0, sizeof(segmented_config));
  segmented_config.keep_context = -1;
  segmented_config.segmented_sink = cpkt_sus_live_segmented_sink;
  segmented_config.segmented_user = run;
  run->current_segment_index = segment->segment_index;
  result = transcriber->transcribe_audio_vox_segment(transcriber, segment,
                                                     &segmented_config);
  transcriber->destroy(transcriber);
  if (result != CPKT_SUS_OK) {
    return 1;
  }
  ++run->segment_count;
  if (segment->hard_cut) {
    ++run->hard_count;
  }
  if (segment->is_final) {
    ++run->final_count;
  }
  return 0;
}

static int cpkt_sus_live_capture_state_sink(
    const cpkt_audio_capture_state_event *event, void *user) {
  struct cpkt_sus_live_run *run;
  char line[80];

  run = (struct cpkt_sus_live_run *)user;
  if (run == NULL || event == NULL) {
    return 1;
  }
  if (event->state != CPKT_AUDIO_CAPTURE_READY || !run->pending_rx) {
    return 0;
  }
  if (run->pending_rx_has_segment) {
    sprintf(line, "RX segment=%lu\n", run->pending_rx_segment);
  } else {
    sprintf(line, "RX\n");
  }
  cpkt_sus_live_emit(run, line);
  run->pending_rx = 0;
  run->pending_rx_has_segment = 0;
  return 0;
}

static cpkt_audio_result cpkt_sus_live_ptt_toggle(cpkt_audio_ptt *ptt,
                                                  int *tx) {
  if (ptt == NULL || tx == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  if (*tx) {
    *tx = 0;
    return ptt->release(ptt);
  }
  *tx = 1;
  return ptt->press(ptt);
}

static cpkt_audio_result cpkt_sus_live_ptt_poll(cpkt_audio_ptt *ptt, int *tx) {
  unsigned char ch;
  ssize_t count;
  cpkt_audio_result result;

  for (;;) {
    count = read(STDIN_FILENO, &ch, 1U);
    if (count <= 0) {
      return CPKT_AUDIO_OK;
    }
    if (ch == 'q' || ch == 'Q') {
      cpkt_sus_live_stop = 1;
      return CPKT_AUDIO_OK;
    }
    if (ch == ' ' || ch == 'p' || ch == 'P') {
      result = cpkt_sus_live_ptt_toggle(ptt, tx);
      if (result != CPKT_AUDIO_OK) {
        return result;
      }
    }
  }
}

static int cpkt_sus_live_open_model(cpkt_sus_model **out,
                                    const struct cpkt_sus_live_options *opts) {
  cpkt_sus_model_config path_config;
  cpkt_sus_cache_config cache_config;
  cpkt_sus_result result;

  if (opts->model_path != NULL) {
    memset(&path_config, 0, sizeof(path_config));
    path_config.model_path = opts->model_path;
    path_config.cpu_only = opts->cpu_only;
    result = cpkt_sus_model_open_path(out, &path_config);
  } else {
    memset(&cache_config, 0, sizeof(cache_config));
    cache_config.model = opts->model;
    cache_config.cache_dir = opts->cache_dir;
    cache_config.source_url = opts->source_url;
    cache_config.cpu_only = opts->cpu_only;
    cache_config.offline = opts->offline;
    cache_config.status_sink = cpkt_sus_live_cache_status_sink;
    result = cpkt_sus_model_open_cached(out, &cache_config);
  }
  if (result != CPKT_SUS_OK) {
    fprintf(stderr, "model open failed: %s\n", cpkt_sus_result_string(result));
    return 0;
  }
  return 1;
}

static int cpkt_sus_live_run(const struct cpkt_sus_live_options *opts) {
  cpkt_audio_capture *capture;
  cpkt_audio_vox *vox;
  cpkt_audio_ptt *ptt;
  cpkt_sus_model *model;
  cpkt_audio_capture_config capture_config;
  cpkt_audio_vox_config vox_config;
  cpkt_audio_ptt_config ptt_config;
  struct cpkt_sus_live_run run;
  char summary_path[CPKT_SUS_LIVE_PATH_MAX];
  float frames[CPKT_SUS_LIVE_READ_FRAMES];
  size_t frames_read;
  time_t end_time;
  cpkt_audio_result audio_result;
  int rc;
  int ptt_tx;

  capture = NULL;
  vox = NULL;
  ptt = NULL;
  model = NULL;
  rc = 1;
  ptt_tx = 0;
  memset(&run, 0, sizeof(run));
  run.options = opts;

  cpkt_sus_log_set(NULL, NULL);
  if (opts->dump_dir != NULL) {
    (void)mkdir(opts->dump_dir, 0700);
    if (!cpkt_sus_live_join_path(summary_path, sizeof(summary_path),
                                 opts->dump_dir, "summary.txt")) {
      fprintf(stderr, "dump path is too long\n");
      goto cleanup;
    }
    run.summary = fopen(summary_path, "wb");
    if (run.summary == NULL) {
      fprintf(stderr, "failed to open summary: %s\n", summary_path);
      goto cleanup;
    }
  }

  cpkt_sus_live_print_startup(opts);
  if (!cpkt_sus_live_open_model(&model, opts)) {
    goto cleanup;
  }
  run.model = model;

  memset(&capture_config, 0, sizeof(capture_config));
  capture_config.backend = opts->backend;
  capture_config.buffer_ms = opts->buffer_ms;
  capture_config.period_ms = opts->period_ms;
  capture_config.state_sink = cpkt_sus_live_capture_state_sink;
  capture_config.state_user = &run;
  audio_result = cpkt_audio_capture_open_default(&capture, &capture_config);
  if (audio_result != CPKT_AUDIO_OK) {
    fprintf(stderr, "capture open failed: %s\n",
            cpkt_audio_result_string(audio_result));
    goto cleanup;
  }

  if (opts->ptt) {
    if (!cpkt_sus_live_enable_raw_tty()) {
      fprintf(stderr, "--ptt requires stdin to be a tty\n");
      goto cleanup;
    }
    memset(&ptt_config, 0, sizeof(ptt_config));
    ptt_config.max_segment_ms = opts->max_segment_ms;
    ptt_config.min_segment_ms = 100UL;
    ptt_config.segment_sink = cpkt_sus_live_segment_sink;
    ptt_config.segment_user = &run;
    ptt_config.state_sink = cpkt_sus_live_state_sink;
    ptt_config.state_user = &run;
    audio_result = cpkt_audio_ptt_open(&ptt, &ptt_config);
    if (audio_result != CPKT_AUDIO_OK) {
      fprintf(stderr, "ptt open failed: %s\n",
              cpkt_audio_result_string(audio_result));
      goto cleanup;
    }
  } else {
    memset(&vox_config, 0, sizeof(vox_config));
    vox_config.threshold = cpkt_sus_live_threshold(opts);
    vox_config.release_silence_ms = opts->hang_ms;
    vox_config.prebuffer_ms = opts->prebuffer_ms;
    vox_config.max_segment_ms = opts->max_segment_ms;
    vox_config.min_segment_ms = 100UL;
    vox_config.segment_sink = cpkt_sus_live_segment_sink;
    vox_config.segment_user = &run;
    vox_config.state_sink = cpkt_sus_live_state_sink;
    vox_config.state_user = &run;
    audio_result = cpkt_audio_vox_open(&vox, &vox_config);
    if (audio_result != CPKT_AUDIO_OK) {
      fprintf(stderr, "vox open failed: %s\n",
              cpkt_audio_result_string(audio_result));
      goto cleanup;
    }
  }

  run.pending_rx = 1;
  run.pending_rx_has_segment = 0;
  if (opts->ptt) {
    cpkt_sus_live_emit(&run, "PTT ready: space/p toggles TX, q quits\n");
  }
  audio_result = capture->start(capture);
  if (audio_result != CPKT_AUDIO_OK) {
    fprintf(stderr, "capture start failed: %s\n",
            cpkt_audio_result_string(audio_result));
    goto cleanup;
  }

  end_time = opts->seconds != 0UL ? time(NULL) + (time_t)opts->seconds : 0;
  while (!cpkt_sus_live_stop &&
         (opts->seconds == 0UL || time(NULL) < end_time)) {
    if (opts->ptt) {
      audio_result = cpkt_sus_live_ptt_poll(ptt, &ptt_tx);
      if (audio_result != CPKT_AUDIO_OK) {
        fprintf(stderr, "ptt control failed: %s\n",
                cpkt_audio_result_string(audio_result));
        goto cleanup;
      }
    }
    frames_read = 0U;
    audio_result = capture->read_f32_mono_16k(
        capture, frames, CPKT_SUS_LIVE_READ_FRAMES, &frames_read);
    if (audio_result != CPKT_AUDIO_OK) {
      fprintf(stderr, "capture read failed: %s\n",
              cpkt_audio_result_string(audio_result));
      goto cleanup;
    }
    if (frames_read == 0U) {
      cpkt_sus_live_sleep_ms(5UL);
      continue;
    }
    audio_result = opts->ptt ? ptt->push_f32_mono_16k(ptt, frames, frames_read)
                             : vox->push_f32_mono_16k(vox, frames, frames_read);
    if (audio_result != CPKT_AUDIO_OK) {
      fprintf(stderr, "%s push failed: %s\n", opts->ptt ? "ptt" : "vox",
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
  audio_result = opts->ptt ? ptt->flush(ptt) : vox->flush(vox);
  if (audio_result != CPKT_AUDIO_OK) {
    fprintf(stderr, "%s flush failed: %s\n", opts->ptt ? "ptt" : "vox",
            cpkt_audio_result_string(audio_result));
    goto cleanup;
  }

  if (opts->dump_dir != NULL) {
    sprintf(summary_path,
            "summary segments=%lu hard=%lu final=%lu dump_dir=%s\n",
            run.segment_count, run.hard_count, run.final_count, opts->dump_dir);
  } else {
    sprintf(summary_path, "summary segments=%lu hard=%lu final=%lu\n",
            run.segment_count, run.hard_count, run.final_count);
  }
  cpkt_sus_live_emit(&run, summary_path);
  rc = 0;

cleanup:
  if (vox != NULL) {
    vox->destroy(vox);
  }
  if (ptt != NULL) {
    ptt->destroy(ptt);
  }
  cpkt_sus_live_restore_tty();
  if (capture != NULL) {
    (void)capture->stop(capture);
    capture->destroy(capture);
  }
  if (model != NULL) {
    model->destroy(model);
  }
  if (run.summary != NULL) {
    fclose(run.summary);
  }
  return rc;
}

static void cpkt_sus_live_usage(FILE *out) {
  fprintf(out, "usage: cpkt_sus_live_vox_c89_example [options]\n\n");
  fprintf(out, "No arguments open the default capture device, segment by VOX, "
               "and transcribe each segment.\n\n");
  fprintf(out, "Options:\n");
  fprintf(out, "  --ptt                       Use push-to-talk instead of VOX; "
               "space/p toggles TX, q quits.\n");
  fprintf(out, "  --seconds N                 Capture duration; default 0, run "
               "until terminated.\n");
  fprintf(out,
          "  --threshold-milli N         VOX threshold * 1000; default 60.\n");
  fprintf(out, "  --hang-ms N                 VOX hang-time; default 1500.\n");
  fprintf(out,
          "  --prebuffer-ms N            VOX prebuffer; default 50.\n");
  fprintf(out,
          "  --max-segment-ms N          Hard cut budget; default 0, disabled.\n");
  fprintf(out,
          "  --buffer-ms N               Device ring buffer; default 2000.\n");
  fprintf(
      out,
      "  --period-ms N               Device callback period; default 20.\n");
  fprintf(out, "  --backend NAME              auto, process, coreaudio.\n");
  fprintf(out,
          "  --model NAME                Cached model name; default tiny.\n");
  fprintf(out, "  --model-path PATH           Load an explicit model file.\n");
  fprintf(out, "  --cache-dir DIR             Model cache directory.\n");
  fprintf(out, "  --source-url URL            Override cached model source URL.\n");
  fprintf(out, "  --offline                   Do not download missing cached models.\n");
  fprintf(out, "  --language CODE             Language code; default en.\n");
  fprintf(
      out,
      "  --cpu-only N                1 for CPU-only, 0 for backend default.\n");
  fprintf(out,
          "  --dump-dir DIR              Optional summary dump directory.\n");
  fprintf(out, "  --smoke                     Run no-device smoke check.\n");
}

static int cpkt_sus_live_parse_options(int argc, char **argv,
                                       struct cpkt_sus_live_options *opts) {
  int i;
  unsigned long parsed;

  for (i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--help") == 0) {
      cpkt_sus_live_usage(stdout);
      exit(0);
    } else if (strcmp(argv[i], "--live") == 0) {
      /* Compatibility no-op: live mode is the default. */
    } else if (strcmp(argv[i], "--ptt") == 0) {
      opts->ptt = 1;
    } else if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
      if (!cpkt_sus_live_parse_ulong(argv[++i], &opts->seconds)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--threshold-milli") == 0 && i + 1 < argc) {
      if (!cpkt_sus_live_parse_ulong(argv[++i], &opts->threshold_milli)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--hang-ms") == 0 && i + 1 < argc) {
      if (!cpkt_sus_live_parse_ulong(argv[++i], &opts->hang_ms)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--prebuffer-ms") == 0 && i + 1 < argc) {
      if (!cpkt_sus_live_parse_ulong(argv[++i], &opts->prebuffer_ms)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--max-segment-ms") == 0 && i + 1 < argc) {
      if (!cpkt_sus_live_parse_ulong(argv[++i], &opts->max_segment_ms)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--buffer-ms") == 0 && i + 1 < argc) {
      if (!cpkt_sus_live_parse_ulong(argv[++i], &opts->buffer_ms)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--period-ms") == 0 && i + 1 < argc) {
      if (!cpkt_sus_live_parse_ulong(argv[++i], &opts->period_ms)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--backend") == 0 && i + 1 < argc) {
      if (!cpkt_sus_live_parse_backend(argv[++i], &opts->backend)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) {
      opts->model = argv[++i];
    } else if (strcmp(argv[i], "--model-path") == 0 && i + 1 < argc) {
      opts->model_path = argv[++i];
    } else if (strcmp(argv[i], "--cache-dir") == 0 && i + 1 < argc) {
      opts->cache_dir = argv[++i];
    } else if (strcmp(argv[i], "--source-url") == 0 && i + 1 < argc) {
      opts->source_url = argv[++i];
    } else if (strcmp(argv[i], "--offline") == 0) {
      opts->offline = 1;
    } else if (strcmp(argv[i], "--language") == 0 && i + 1 < argc) {
      opts->language = argv[++i];
    } else if (strcmp(argv[i], "--cpu-only") == 0 && i + 1 < argc) {
      if (!cpkt_sus_live_parse_ulong(argv[++i], &parsed)) {
        return 0;
      }
      opts->cpu_only = parsed != 0UL ? 1 : 0;
    } else if (strcmp(argv[i], "--dump-dir") == 0 && i + 1 < argc) {
      opts->dump_dir = argv[++i];
    } else if (strcmp(argv[i], "--smoke") == 0) {
      opts->smoke = 1;
    } else {
      return 0;
    }
  }
  return 1;
}

static int cpkt_sus_live_smoke(void) {
  cpkt_sus_model_entry entry;

  if (cpkt_sus_model_catalog_find("tiny", &entry) != CPKT_SUS_OK) {
    return 1;
  }
  if (entry.name == NULL || strcmp(entry.name, "tiny") != 0) {
    return 2;
  }
  return 0;
}

int main(int argc, char **argv) {
  struct cpkt_sus_live_options opts;

  cpkt_sus_live_defaults(&opts);
  if (!cpkt_sus_live_parse_options(argc, argv, &opts)) {
    cpkt_sus_live_usage(stderr);
    return 64;
  }
  if (opts.smoke) {
    return cpkt_sus_live_smoke();
  }
  if (opts.ptt && !isatty(STDIN_FILENO)) {
    fprintf(stderr, "--ptt requires stdin to be a tty\n");
    return 64;
  }
  return cpkt_sus_live_run(&opts);
}
