#include <cpkt/audio.h>

#include <stdio.h>

static int write_segment(cpkt_audio_playback *playback) {
  float frames[160];
  size_t frames_written;
  cpkt_audio_result result;
  size_t i;

  for (i = 0U; i < sizeof(frames) / sizeof(frames[0]); ++i) {
    frames[i] = 0.1f;
  }
  frames_written = 0U;
  result = playback->write_f32_mono_16k(
      playback, frames, sizeof(frames) / sizeof(frames[0]), &frames_written);
  if (result != CPKT_AUDIO_OK ||
      frames_written != sizeof(frames) / sizeof(frames[0])) {
    fprintf(stderr, "playback write failed: %s frames=%lu\n",
            cpkt_audio_result_string(result), (unsigned long)frames_written);
    return 0;
  }
  result = playback->drain(playback);
  if (result != CPKT_AUDIO_OK) {
    fprintf(stderr, "playback drain failed: %s\n",
            cpkt_audio_result_string(result));
    return 0;
  }
  return 1;
}

int main(void) {
  cpkt_audio_playback *playback;
  cpkt_audio_playback_config config;
  cpkt_audio_result result;

  playback = NULL;
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
  if (!write_segment(playback) || !write_segment(playback)) {
    playback->destroy(playback);
    return 1;
  }
  playback->destroy(playback);
  return 0;
}
