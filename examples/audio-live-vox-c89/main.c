#include <cpkt/audio.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define CPKT_LIVE_VOX_READ_FRAMES 512U
#define CPKT_LIVE_VOX_PATH_MAX 4096

struct cpkt_live_vox_options {
  int ptt;
  int playback;
  int smoke;
  int backend;
  unsigned long seconds;
  unsigned long threshold_milli;
  unsigned long hang_ms;
  unsigned long prebuffer_ms;
  unsigned long max_segment_ms;
  unsigned long buffer_ms;
  unsigned long period_ms;
  const char *dump_dir;
};

struct cpkt_live_vox_run {
  const struct cpkt_live_vox_options *options;
  cpkt_audio_playback *playback;
  unsigned long segment_count;
  unsigned long hard_count;
  unsigned long final_count;
  FILE *summary;
};

static struct termios cpkt_live_vox_saved_tty;
static int cpkt_live_vox_raw_tty = 0;
static volatile sig_atomic_t cpkt_live_vox_stop = 0;

static void cpkt_live_vox_restore_tty(void) {
  if (cpkt_live_vox_raw_tty) {
    (void)tcsetattr(STDIN_FILENO, TCSANOW, &cpkt_live_vox_saved_tty);
    cpkt_live_vox_raw_tty = 0;
  }
}

static void cpkt_live_vox_signal_stop(int signum) {
  (void)signum;
  cpkt_live_vox_stop = 1;
}

static int cpkt_live_vox_enable_raw_tty(void) {
  struct termios raw;

  if (!isatty(STDIN_FILENO)) {
    return 0;
  }
  if (tcgetattr(STDIN_FILENO, &cpkt_live_vox_saved_tty) != 0) {
    return 0;
  }
  raw = cpkt_live_vox_saved_tty;
  raw.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
    return 0;
  }
  cpkt_live_vox_raw_tty = 1;
  (void)atexit(cpkt_live_vox_restore_tty);
  (void)signal(SIGINT, cpkt_live_vox_signal_stop);
  return 1;
}

static float cpkt_live_vox_threshold(const struct cpkt_live_vox_options *opts) {
  return opts->threshold_milli != 0UL ? (float)opts->threshold_milli / 1000.0f
                                      : 0.06f;
}

static void cpkt_live_vox_defaults(struct cpkt_live_vox_options *opts) {
  memset(opts, 0, sizeof(*opts));
  opts->playback = 1;
  opts->seconds = 0UL;
  opts->threshold_milli = 60UL;
  opts->hang_ms = 1500UL;
  opts->prebuffer_ms = 50UL;
  opts->max_segment_ms = 0UL;
  opts->buffer_ms = 2000UL;
  opts->period_ms = 20UL;
  opts->dump_dir = NULL;
}

static void cpkt_live_vox_sleep_ms(unsigned long ms) {
  struct timeval tv;

  tv.tv_sec = (long)(ms / 1000UL);
  tv.tv_usec = (long)((ms % 1000UL) * 1000UL);
  (void)select(0, NULL, NULL, NULL, &tv);
}

static int cpkt_live_vox_parse_ulong(const char *text, unsigned long *out) {
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

static int cpkt_live_vox_parse_backend(const char *text, int *out) {
  if (text == NULL || out == NULL) {
    return 0;
  }
  if (strcmp(text, "auto") == 0) {
    *out = CPKT_AUDIO_DEVICE_BACKEND_AUTO;
  } else if (strcmp(text, "alsa") == 0) {
    *out = CPKT_AUDIO_DEVICE_BACKEND_ALSA;
  } else if (strcmp(text, "coreaudio") == 0) {
    *out = CPKT_AUDIO_DEVICE_BACKEND_COREAUDIO;
  } else {
    return 0;
  }
  return 1;
}

static int cpkt_live_vox_join_path(char *out, size_t out_size, const char *dir,
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

static void cpkt_live_vox_emit(struct cpkt_live_vox_run *run,
                               const char *text) {
  fputs(text, stdout);
  fflush(stdout);
  if (run != NULL && run->summary != NULL) {
    fputs(text, run->summary);
    fflush(run->summary);
  }
}

static int cpkt_live_vox_state_sink(const cpkt_audio_vox_state_event *event,
                                    void *user) {
  struct cpkt_live_vox_run *run;
  char line[160];

  run = (struct cpkt_live_vox_run *)user;
  if (run == NULL || event == NULL) {
    return 1;
  }
  if (event->state == CPKT_AUDIO_VOX_TX_ON) {
    sprintf(line, "TX segment=%lu threshold=%.3f\n", event->segment_index,
            (double)event->threshold);
  } else if (event->state == CPKT_AUDIO_VOX_TX_OFF) {
    if (run->playback != NULL) {
      return 0;
    }
    sprintf(line, "RX segment=%lu hang_ms=%lu\n", event->segment_index,
            run->options->hang_ms);
  } else if (event->state == CPKT_AUDIO_VOX_HARD_CUT) {
    sprintf(line, "TX hard-cut segment=%lu max_segment_ms=%lu\n",
            event->segment_index, run->options->max_segment_ms);
  } else {
    sprintf(line, "VOX state=%d segment=%lu\n", event->state,
            event->segment_index);
  }
  cpkt_live_vox_emit(run, line);
  return 0;
}

static int cpkt_live_vox_write_segment(cpkt_audio_vox_segment *segment,
                                       void *user) {
  struct cpkt_live_vox_run *run;
  cpkt_audio_encoder *encoder;
  cpkt_audio_encoder_config encoder_config;
  char name[64];
  char path[CPKT_LIVE_VOX_PATH_MAX];
  char status[160];
  float frames[CPKT_LIVE_VOX_READ_FRAMES];
  size_t frames_read;
  size_t frames_written;
  size_t total_frames;
  cpkt_audio_result result;
  int emit_rx;

  run = (struct cpkt_live_vox_run *)user;
  if (run == NULL || segment == NULL) {
    return 1;
  }

  sprintf(name, "segment-%04lu.wav", segment->segment_index);
  encoder = NULL;
  emit_rx = 0;
  path[0] = '\0';
  if (run->options->dump_dir != NULL) {
    if (!cpkt_live_vox_join_path(path, sizeof(path), run->options->dump_dir,
                                 name)) {
      return 1;
    }
    memset(&encoder_config, 0, sizeof(encoder_config));
    encoder_config.format = CPKT_AUDIO_FORMAT_WAV;
    encoder_config.sample_rate = 16000UL;
    encoder_config.channels = 1UL;
    if (cpkt_audio_encoder_open_file(&encoder, path, &encoder_config) !=
        CPKT_AUDIO_OK) {
      return 1;
    }
  }

  if (run->playback != NULL) {
    sprintf(status, "PLAYBACK segment=%lu\n", segment->segment_index);
    cpkt_live_vox_emit(run, status);
  }

  total_frames = 0U;
  do {
    frames_read = 0U;
    result = segment->read_f32_mono_16k(
        segment, frames, CPKT_LIVE_VOX_READ_FRAMES, &frames_read);
    if (result != CPKT_AUDIO_OK && result != CPKT_AUDIO_AT_END) {
      if (encoder != NULL) {
        encoder->destroy(encoder);
      }
      return 1;
    }
    if (frames_read > 0U) {
      frames_written = 0U;
      if (encoder != NULL) {
        if (encoder->write_f32(encoder, frames, frames_read, &frames_written) !=
                CPKT_AUDIO_OK ||
            frames_written != frames_read) {
          encoder->destroy(encoder);
          return 1;
        }
      }
      if (run->playback != NULL) {
        frames_written = 0U;
        if (run->playback->write_f32_mono_16k(run->playback, frames,
                                              frames_read, &frames_written) !=
                CPKT_AUDIO_OK ||
            frames_written != frames_read) {
          if (encoder != NULL) {
            encoder->destroy(encoder);
          }
          return 1;
        }
      }
      total_frames += frames_read;
    }
  } while (result != CPKT_AUDIO_AT_END);

  if (encoder != NULL && encoder->close(encoder) != CPKT_AUDIO_OK) {
    encoder->destroy(encoder);
    return 1;
  }
  if (encoder != NULL) {
    encoder->destroy(encoder);
  }
  if (run->playback != NULL) {
    if (run->playback->drain(run->playback) != CPKT_AUDIO_OK) {
      return 1;
    }
    if (!segment->hard_cut) {
      emit_rx = 1;
    }
  }

  ++run->segment_count;
  if (segment->hard_cut) {
    ++run->hard_count;
  }
  if (segment->is_final) {
    ++run->final_count;
  }
  if (path[0] != '\0') {
    fprintf(stdout,
            "segment index=%lu frames=%lu seconds=%.3f hard=%d final=%d "
            "wav=%s\n",
            segment->segment_index, (unsigned long)total_frames,
            (double)total_frames / 16000.0, segment->hard_cut,
            segment->is_final, path);
  } else {
    fprintf(stdout,
            "segment index=%lu frames=%lu seconds=%.3f hard=%d "
            "final=%d\n",
            segment->segment_index, (unsigned long)total_frames,
            (double)total_frames / 16000.0, segment->hard_cut,
            segment->is_final);
  }
  fflush(stdout);
  if (run->summary != NULL) {
    fprintf(run->summary,
            "segment index=%lu frames=%lu seconds=%.3f hard=%d final=%d "
            "wav=%s\n",
            segment->segment_index, (unsigned long)total_frames,
            (double)total_frames / 16000.0, segment->hard_cut,
            segment->is_final, path);
    fflush(run->summary);
  }
  if (emit_rx) {
    sprintf(status, "RX segment=%lu\n", segment->segment_index);
    cpkt_live_vox_emit(run, status);
  }
  return 0;
}

static cpkt_audio_result cpkt_live_vox_ptt_toggle(cpkt_audio_ptt *ptt,
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

static cpkt_audio_result cpkt_live_vox_ptt_poll(cpkt_audio_ptt *ptt, int *tx) {
  unsigned char ch;
  ssize_t count;
  cpkt_audio_result result;

  for (;;) {
    count = read(STDIN_FILENO, &ch, 1U);
    if (count <= 0) {
      return CPKT_AUDIO_OK;
    }
    if (ch == 'q' || ch == 'Q') {
      cpkt_live_vox_stop = 1;
      return CPKT_AUDIO_OK;
    }
    if (ch == ' ' || ch == 'p' || ch == 'P') {
      result = cpkt_live_vox_ptt_toggle(ptt, tx);
      if (result != CPKT_AUDIO_OK) {
        return result;
      }
    }
  }
}

static void cpkt_live_vox_usage(FILE *out) {
  fprintf(out, "usage: cpkt_audio_live_vox_c89_example [options]\n\n");
  fprintf(out, "No arguments open the default capture device, segment by VOX, "
               "and play each segment back.\n\n");
  fprintf(out, "Options:\n");
  fprintf(out, "  --ptt                       Use push-to-talk instead of VOX; "
               "space/p toggles TX, q quits.\n");
  fprintf(out,
          "  --no-playback               Do not play captured segments back.\n");
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
  fprintf(out, "  --backend NAME              auto, alsa, coreaudio.\n");
  fprintf(out, "  --dump-dir DIR              Optional WAV dump directory.\n");
  fprintf(out, "  --smoke                     Run no-device smoke check.\n");
}

static int cpkt_live_vox_parse_options(int argc, char **argv,
                                       struct cpkt_live_vox_options *opts) {
  int i;

  for (i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--help") == 0) {
      cpkt_live_vox_usage(stdout);
      exit(0);
    } else if (strcmp(argv[i], "--live") == 0) {
      /* Compatibility no-op: live mode is the default. */
    } else if (strcmp(argv[i], "--ptt") == 0) {
      opts->ptt = 1;
    } else if (strcmp(argv[i], "--no-playback") == 0) {
      opts->playback = 0;
    } else if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
      if (!cpkt_live_vox_parse_ulong(argv[++i], &opts->seconds)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--threshold-milli") == 0 && i + 1 < argc) {
      if (!cpkt_live_vox_parse_ulong(argv[++i], &opts->threshold_milli)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--hang-ms") == 0 && i + 1 < argc) {
      if (!cpkt_live_vox_parse_ulong(argv[++i], &opts->hang_ms)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--prebuffer-ms") == 0 && i + 1 < argc) {
      if (!cpkt_live_vox_parse_ulong(argv[++i], &opts->prebuffer_ms)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--max-segment-ms") == 0 && i + 1 < argc) {
      if (!cpkt_live_vox_parse_ulong(argv[++i], &opts->max_segment_ms)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--buffer-ms") == 0 && i + 1 < argc) {
      if (!cpkt_live_vox_parse_ulong(argv[++i], &opts->buffer_ms)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--period-ms") == 0 && i + 1 < argc) {
      if (!cpkt_live_vox_parse_ulong(argv[++i], &opts->period_ms)) {
        return 0;
      }
    } else if (strcmp(argv[i], "--backend") == 0 && i + 1 < argc) {
      if (!cpkt_live_vox_parse_backend(argv[++i], &opts->backend)) {
        return 0;
      }
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

static int cpkt_live_vox_run(const struct cpkt_live_vox_options *opts) {
  cpkt_audio_capture *capture;
  cpkt_audio_playback *playback;
  cpkt_audio_vox *vox;
  cpkt_audio_ptt *ptt;
  cpkt_audio_capture_config capture_config;
  cpkt_audio_playback_config playback_config;
  cpkt_audio_vox_config vox_config;
  cpkt_audio_ptt_config ptt_config;
  struct cpkt_live_vox_run run;
  char summary_path[CPKT_LIVE_VOX_PATH_MAX];
  float frames[CPKT_LIVE_VOX_READ_FRAMES];
  size_t frames_read;
  unsigned long captured_frames;
  time_t end_time;
  cpkt_audio_result result;
  int rc;
  int ptt_tx;

  capture = NULL;
  playback = NULL;
  vox = NULL;
  ptt = NULL;
  rc = 1;
  ptt_tx = 0;
  memset(&run, 0, sizeof(run));
  run.options = opts;

  if (opts->dump_dir != NULL) {
    (void)mkdir(opts->dump_dir, 0700);
    if (!cpkt_live_vox_join_path(summary_path, sizeof(summary_path),
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

  memset(&capture_config, 0, sizeof(capture_config));
  capture_config.backend = opts->backend;
  capture_config.buffer_ms = opts->buffer_ms;
  capture_config.period_ms = opts->period_ms;
  result = cpkt_audio_capture_open_default(&capture, &capture_config);
  if (result != CPKT_AUDIO_OK) {
    fprintf(stderr, "capture open failed: %s\n",
            cpkt_audio_result_string(result));
    goto cleanup;
  }

  if (opts->playback) {
    memset(&playback_config, 0, sizeof(playback_config));
    playback_config.backend = opts->backend;
    playback_config.buffer_ms = opts->buffer_ms;
    playback_config.period_ms = opts->period_ms;
    result = cpkt_audio_playback_open_default(&playback, &playback_config);
    if (result != CPKT_AUDIO_OK) {
      fprintf(stderr, "PLAYBACK unavailable: %s\n",
              cpkt_audio_result_string(result));
      playback = NULL;
    } else {
      result = playback->start(playback);
      if (result != CPKT_AUDIO_OK) {
        fprintf(stderr, "PLAYBACK unavailable: %s\n",
                cpkt_audio_result_string(result));
        playback->destroy(playback);
        playback = NULL;
      }
    }
    run.playback = playback;
  }

  if (opts->ptt) {
    if (!cpkt_live_vox_enable_raw_tty()) {
      fprintf(stderr, "--ptt requires stdin to be a tty\n");
      goto cleanup;
    }
    memset(&ptt_config, 0, sizeof(ptt_config));
    ptt_config.max_segment_ms = opts->max_segment_ms;
    ptt_config.min_segment_ms = 100UL;
    ptt_config.segment_sink = cpkt_live_vox_write_segment;
    ptt_config.segment_user = &run;
    ptt_config.state_sink = cpkt_live_vox_state_sink;
    ptt_config.state_user = &run;
    result = cpkt_audio_ptt_open(&ptt, &ptt_config);
    if (result != CPKT_AUDIO_OK) {
      fprintf(stderr, "ptt open failed: %s\n",
              cpkt_audio_result_string(result));
      goto cleanup;
    }
  } else {
    memset(&vox_config, 0, sizeof(vox_config));
    vox_config.threshold = cpkt_live_vox_threshold(opts);
    vox_config.release_silence_ms = opts->hang_ms;
    vox_config.prebuffer_ms = opts->prebuffer_ms;
    vox_config.max_segment_ms = opts->max_segment_ms;
    vox_config.min_segment_ms = 100UL;
    vox_config.segment_sink = cpkt_live_vox_write_segment;
    vox_config.segment_user = &run;
    vox_config.state_sink = cpkt_live_vox_state_sink;
    vox_config.state_user = &run;
    result = cpkt_audio_vox_open(&vox, &vox_config);
    if (result != CPKT_AUDIO_OK) {
      fprintf(stderr, "vox open failed: %s\n",
              cpkt_audio_result_string(result));
      goto cleanup;
    }
  }

  cpkt_live_vox_emit(&run, "RX\n");
  if (opts->ptt) {
    cpkt_live_vox_emit(&run, "PTT ready: space/p toggles TX, q quits\n");
  }
  result = capture->start(capture);
  if (result != CPKT_AUDIO_OK) {
    fprintf(stderr, "capture start failed: %s\n",
            cpkt_audio_result_string(result));
    goto cleanup;
  }

  captured_frames = 0UL;
  end_time = opts->seconds != 0UL ? time(NULL) + (time_t)opts->seconds : 0;
  while (!cpkt_live_vox_stop &&
         (opts->seconds == 0UL || time(NULL) < end_time)) {
    if (opts->ptt) {
      result = cpkt_live_vox_ptt_poll(ptt, &ptt_tx);
      if (result != CPKT_AUDIO_OK) {
        fprintf(stderr, "ptt control failed: %s\n",
                cpkt_audio_result_string(result));
        goto cleanup;
      }
    }
    frames_read = 0U;
    result = capture->read_f32_mono_16k(
        capture, frames, CPKT_LIVE_VOX_READ_FRAMES, &frames_read);
    if (result != CPKT_AUDIO_OK) {
      fprintf(stderr, "capture read failed: %s\n",
              cpkt_audio_result_string(result));
      goto cleanup;
    }
    if (frames_read == 0U) {
      cpkt_live_vox_sleep_ms(5UL);
      continue;
    }
    captured_frames += (unsigned long)frames_read;
    result = opts->ptt ? ptt->push_f32_mono_16k(ptt, frames, frames_read)
                       : vox->push_f32_mono_16k(vox, frames, frames_read);
    if (result != CPKT_AUDIO_OK) {
      fprintf(stderr, "%s push failed: %s\n", opts->ptt ? "ptt" : "vox",
              cpkt_audio_result_string(result));
      goto cleanup;
    }
  }

  result = capture->stop(capture);
  if (result != CPKT_AUDIO_OK) {
    fprintf(stderr, "capture stop failed: %s\n",
            cpkt_audio_result_string(result));
    goto cleanup;
  }
  result = opts->ptt ? ptt->flush(ptt) : vox->flush(vox);
  if (result != CPKT_AUDIO_OK) {
    fprintf(stderr, "%s flush failed: %s\n", opts->ptt ? "ptt" : "vox",
            cpkt_audio_result_string(result));
    goto cleanup;
  }
  if (playback != NULL) {
    (void)playback->drain(playback);
  }
  fprintf(stdout, "summary segments=%lu hard=%lu final=%lu", run.segment_count,
          run.hard_count, run.final_count);
  if (opts->dump_dir != NULL) {
    fprintf(stdout, " dump_dir=%s", opts->dump_dir);
  }
  fputc('\n', stdout);
  if (run.summary != NULL) {
    fprintf(run.summary,
            "summary segments=%lu hard=%lu final=%lu dump_dir=%s\n",
            run.segment_count, run.hard_count, run.final_count, opts->dump_dir);
  }
  rc = 0;

cleanup:
  if (playback != NULL) {
    (void)playback->stop(playback);
    playback->destroy(playback);
  }
  if (vox != NULL) {
    vox->destroy(vox);
  }
  if (ptt != NULL) {
    ptt->destroy(ptt);
  }
  cpkt_live_vox_restore_tty();
  if (capture != NULL) {
    (void)capture->stop(capture);
    capture->destroy(capture);
  }
  if (run.summary != NULL) {
    fclose(run.summary);
  }
  return rc;
}

int main(int argc, char **argv) {
  struct cpkt_live_vox_options opts;

  cpkt_live_vox_defaults(&opts);
  if (!cpkt_live_vox_parse_options(argc, argv, &opts)) {
    cpkt_live_vox_usage(stderr);
    return 2;
  }
  if (opts.smoke) {
    printf("cpkt_audio_live_vox_c89_example smoke ok\n");
    return 0;
  }
  if (opts.ptt && !isatty(STDIN_FILENO)) {
    fprintf(stderr, "--ptt requires stdin to be a tty\n");
    return 2;
  }
  return cpkt_live_vox_run(&opts);
}
