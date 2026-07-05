#include <cpkt/audio.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

static unsigned long now_ms(void) {
  struct timeval tv;

  if (gettimeofday(&tv, NULL) != 0) {
    return 0UL;
  }
  return ((unsigned long)tv.tv_sec * 1000UL) +
         ((unsigned long)tv.tv_usec / 1000UL);
}

static size_t env_size(const char *name, size_t fallback) {
  const char *value;
  unsigned long parsed;

  value = getenv(name);
  if (value == NULL || value[0] == '\0') {
    return fallback;
  }
  parsed = strtoul(value, NULL, 10);
  if (parsed == 0UL) {
    return fallback;
  }
  return (size_t)parsed;
}

static unsigned long env_ulong(const char *name, unsigned long fallback) {
  const char *value;
  unsigned long parsed;

  value = getenv(name);
  if (value == NULL || value[0] == '\0') {
    return fallback;
  }
  parsed = strtoul(value, NULL, 10);
  if (parsed == 0UL) {
    return fallback;
  }
  return parsed;
}

static int write_segment(cpkt_audio_playback *playback, size_t frame_count,
                         unsigned long min_drain_ms) {
  float *frames;
  size_t frames_written;
  cpkt_audio_result result;
  size_t i;
  unsigned long start_ms;
  unsigned long end_ms;
  unsigned long elapsed_ms;

  frames = (float *)malloc(sizeof(float) * frame_count);
  if (frames == NULL) {
    fprintf(stderr, "playback probe allocation failed\n");
    return 0;
  }
  for (i = 0U; i < frame_count; ++i) {
    frames[i] = 0.1f;
  }
  frames_written = 0U;
  result = playback->write_f32_mono_16k(playback, frames, frame_count,
                                        &frames_written);
  free(frames);
  if (result != CPKT_AUDIO_OK || frames_written != frame_count) {
    fprintf(stderr, "playback write failed: %s frames=%lu\n",
            cpkt_audio_result_string(result), (unsigned long)frames_written);
    return 0;
  }
  start_ms = now_ms();
  result = playback->drain(playback);
  end_ms = now_ms();
  if (result != CPKT_AUDIO_OK) {
    fprintf(stderr, "playback drain failed: %s\n",
            cpkt_audio_result_string(result));
    return 0;
  }
  elapsed_ms = end_ms - start_ms;
  if (min_drain_ms > 0UL && elapsed_ms < min_drain_ms) {
    fprintf(stderr, "playback drain returned too early: elapsed=%lu min=%lu\n",
            elapsed_ms, min_drain_ms);
    return 0;
  }
  return 1;
}

int main(void) {
  cpkt_audio_playback *playback;
  cpkt_audio_playback_config config;
  cpkt_audio_result result;
  size_t frame_count;
  unsigned long min_drain_ms;
  int start_only;

  playback = NULL;
  frame_count = env_size("CPKT_AUDIO_PLAYBACK_PROBE_FRAMES", 160U);
  min_drain_ms = env_ulong("CPKT_AUDIO_PLAYBACK_PROBE_MIN_DRAIN_MS", 0UL);
  start_only = getenv("CPKT_AUDIO_PLAYBACK_PROBE_START_ONLY") != NULL ? 1 : 0;
  config.backend = CPKT_AUDIO_DEVICE_BACKEND_PROCESS;
  config.buffer_ms = 0UL;
  config.period_ms = 0UL;

  result = cpkt_audio_playback_open_default(&playback, &config);
  if (result != CPKT_AUDIO_OK) {
    fprintf(stderr, "playback open failed: %s\n",
            cpkt_audio_result_string(result));
    return 1;
  }
  result = playback->start(playback);
  if (result != CPKT_AUDIO_OK) {
    fprintf(stderr, "playback start failed: %s\n",
            cpkt_audio_result_string(result));
    playback->destroy(playback);
    return 1;
  }
  if (start_only) {
    playback->destroy(playback);
    return 0;
  }
  if (!write_segment(playback, frame_count, min_drain_ms) ||
      !write_segment(playback, frame_count, min_drain_ms)) {
    playback->destroy(playback);
    return 1;
  }
  playback->destroy(playback);
  return 0;
}
