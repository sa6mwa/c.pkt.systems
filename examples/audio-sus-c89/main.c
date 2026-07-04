#include <cpkt/audio.h>
#include <cpkt/sus.h>

#include <stdio.h>
#include <string.h>

#define CPKT_EXAMPLE_READ_FRAMES 4096UL

static int cpkt_example_realtime_sink(const cpkt_sus_realtime_event *event,
                                      void *user) {
  FILE *out;

  out = (FILE *)user;
  if (event == NULL || event->text == NULL || out == NULL) {
    return 1;
  }
  if (fwrite(event->text, 1U, (size_t)event->text_length, out) !=
      (size_t)event->text_length) {
    return 1;
  }
  if (fputc('\n', out) == EOF) {
    return 1;
  }
  return 0;
}

static int cpkt_example_transcribe_file(const char *model_path,
                                        const char *audio_path) {
  cpkt_audio_decoder *decoder;
  cpkt_sus_model *model;
  cpkt_sus_transcriber *transcriber;
  cpkt_sus_model_config model_config;
  cpkt_sus_transcriber_config transcriber_config;
  cpkt_sus_realtime_config realtime_config;
  char *text;
  int rc;

  decoder = NULL;
  model = NULL;
  transcriber = NULL;
  text = NULL;
  rc = 1;

  memset(&model_config, 0, sizeof(model_config));
  model_config.model_path = model_path;
  memset(&transcriber_config, 0, sizeof(transcriber_config));
  memset(&realtime_config, 0, sizeof(realtime_config));
  realtime_config.read_frames = CPKT_EXAMPLE_READ_FRAMES;
  realtime_config.step_ms = 1000UL;
  realtime_config.length_ms = 5000UL;
  realtime_config.keep_ms = 200UL;
  realtime_config.realtime_sink = cpkt_example_realtime_sink;
  realtime_config.realtime_user = stdout;

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
  if (transcriber->transcribe_audio_decoder_realtime_text(
          transcriber, decoder, &realtime_config, &text) != CPKT_SUS_OK) {
    goto cleanup;
  }
  if (fputs(text, stdout) == EOF) {
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
  cpkt_sus_string_free(text);
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
