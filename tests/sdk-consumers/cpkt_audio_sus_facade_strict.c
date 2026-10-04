#include <cpkt/audio.h>
#include <cpkt/sus.h>

#include <string.h>

int main(void) {
  cpkt_audio_decoder *decoder;
  cpkt_sus *model;
  cpkt_sus_config model_config;
  cpkt_sus_segmented_config segmented_config;
  cpkt_sus_segmented_event event;

  if (cpkt_audio_format_can_decode(CPKT_AUDIO_FORMAT_MP3) == 0) {
    return 1;
  }
  decoder = (cpkt_audio_decoder *)1;
  if (cpkt_audio_decoder_open_url(&decoder, "", 0) != CPKT_AUDIO_ERR_ARG) {
    return 2;
  }
  if (decoder != 0) {
    return 3;
  }
  if (cpkt_sus_backend_version() == 0 || cpkt_sus_backend_capabilities() == 0) {
    return 4;
  }
  if (strcmp(cpkt_sus_backend_capabilities(), "cpu") != 0) {
    return 5;
  }
  memset(&model_config, 0, sizeof(model_config));
  memset(&segmented_config, 0, sizeof(segmented_config));
  memset(&event, 0, sizeof(event));
  segmented_config.mode = CPKT_SUS_SEGMENT_MODE_CONTINUOUS;
  segmented_config.step_ms = 1000UL;
  segmented_config.length_ms = 7000UL;
  segmented_config.keep_ms = 1500UL;
  segmented_config.vox_threshold = 0.03f;
  segmented_config.prebuffer_ms = 50UL;
  segmented_config.memory_spool_bytes = 1024UL * 1024UL;
  segmented_config.max_spool_bytes = 1024UL * 1024UL * 1024UL;
  event.step_index = 1UL;
  if (sizeof(((cpkt_sus_transcriber *)0)->transcribe_audio_decoder_segmented) ==
          0 ||
      sizeof(((cpkt_sus_transcriber *)0)
                 ->transcribe_audio_decoder_segmented_text) == 0 ||
      sizeof(((cpkt_sus_transcriber *)0)->revised_text) == 0) {
    return 8;
  }
  if (segmented_config.mode != CPKT_SUS_SEGMENT_MODE_CONTINUOUS ||
      segmented_config.prebuffer_ms != 50UL || event.step_index != 1UL) {
    return 9;
  }
  model_config.model_path = "";
  model = (cpkt_sus *)1;
  if (cpkt_sus_open_path(&model, &model_config) != CPKT_SUS_ERR_ARG) {
    return 6;
  }
  if (model != 0) {
    return 7;
  }
  return 0;
}
