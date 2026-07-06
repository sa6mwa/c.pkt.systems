#include <cpkt/audio.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>

static unsigned long ready_count = 0UL;
static size_t last_ready_frames = 0U;

static int capture_state_sink(const cpkt_audio_capture_state_event *event,
                              void *user) {
  (void)user;
  if (event == NULL) {
    return 1;
  }
  if (event->state == CPKT_AUDIO_CAPTURE_READY) {
    ++ready_count;
    last_ready_frames = event->frame_count;
  }
  return 0;
}

static void sleep_ms(unsigned long ms) {
  struct timeval tv;

  tv.tv_sec = (long)(ms / 1000UL);
  tv.tv_usec = (long)((ms % 1000UL) * 1000UL);
  (void)select(0, NULL, NULL, NULL, &tv);
}

static int read_one(cpkt_audio_capture *capture, int *sample_out) {
  float frames[256];
  size_t frames_read;
  int attempts;
  cpkt_audio_result result;

  if (capture == NULL || sample_out == NULL) {
    return 0;
  }
  for (attempts = 0; attempts < 100; ++attempts) {
    result = capture->wait_ready(capture, 100UL);
    if (result == CPKT_AUDIO_TIMEOUT) {
      continue;
    }
    if (result != CPKT_AUDIO_OK) {
      fprintf(stderr, "capture wait failed: %s\n",
              cpkt_audio_result_string(result));
      return 0;
    }
    frames_read = 0U;
    result = capture->read_f32_mono_16k(capture, frames,
                                        sizeof(frames) / sizeof(frames[0]),
                                        &frames_read);
    if (result != CPKT_AUDIO_OK) {
      fprintf(stderr, "capture read failed: %s\n",
              cpkt_audio_result_string(result));
      return 0;
    }
    if (frames_read > 0U) {
      *sample_out = (int)(frames[0] * 32768.0f);
      return 1;
    }
    sleep_ms(10UL);
  }
  fprintf(stderr, "capture read timed out\n");
  return 0;
}

int main(void) {
  cpkt_audio_capture *capture;
  cpkt_audio_capture_config config;
  cpkt_audio_result result;
  int expect_eof;
  int first_sample;
  int second_sample;

  capture = NULL;
  expect_eof = getenv("CPKT_AUDIO_EXPECT_CAPTURE_EOF") != NULL ? 1 : 0;
  first_sample = 0;
  second_sample = 0;
  config.backend = CPKT_AUDIO_DEVICE_BACKEND_PROCESS;
  config.buffer_ms = 0UL;
  config.period_ms = 0UL;
  config.state_sink = capture_state_sink;
  config.state_user = NULL;

  result = cpkt_audio_capture_open_default(&capture, &config);
  if (result != CPKT_AUDIO_OK) {
    fprintf(stderr, "capture open failed: %s\n",
            cpkt_audio_result_string(result));
    return 1;
  }
  result = capture->start(capture);
  if (result != CPKT_AUDIO_OK) {
    fprintf(stderr, "capture start failed: %s\n",
            cpkt_audio_result_string(result));
    capture->destroy(capture);
    return 1;
  }
  if (expect_eof) {
    float frames[16];
    size_t frames_read;

    frames_read = 0U;
    result = capture->wait_ready(capture, 1000UL);
    if (result != CPKT_AUDIO_OK && result != CPKT_AUDIO_TIMEOUT) {
      fprintf(stderr, "capture EOF wait failed too early: %s\n",
              cpkt_audio_result_string(result));
      capture->destroy(capture);
      return 1;
    }
    result = capture->read_f32_mono_16k(capture, frames,
                                        sizeof(frames) / sizeof(frames[0]),
                                        &frames_read);
    capture->destroy(capture);
    if (result != CPKT_AUDIO_ERR_IO || frames_read != 0U) {
      fprintf(stderr, "expected capture EOF to report I/O error, got %s frames=%lu\n",
              cpkt_audio_result_string(result), (unsigned long)frames_read);
      return 4;
    }
    return 0;
  }
  if (!read_one(capture, &first_sample)) {
    capture->destroy(capture);
    return 1;
  }
  sleep_ms(800UL);
  if (!read_one(capture, &second_sample)) {
    capture->destroy(capture);
    return 1;
  }
  (void)capture->stop(capture);
  capture->destroy(capture);

  printf("first=%d second=%d\n", first_sample, second_sample);
  if (ready_count < 2UL || last_ready_frames == 0U) {
    fprintf(stderr, "capture ready event missing\n");
    return 3;
  }
  return second_sample == first_sample ? 0 : 2;
}
