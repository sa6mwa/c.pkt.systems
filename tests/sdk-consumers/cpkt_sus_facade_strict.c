#include <cpkt/sus.h>

#include <string.h>

int main(void) {
  cpkt_sus_model_entry entry;
  cpkt_sus *model;
  cpkt_sus_config config;
  cpkt_sus_segmented_config segmented_config;
  cpkt_sus_segmented_event event;

  if (cpkt_sus_backend_version() == 0 || cpkt_sus_backend_system_info() == 0 ||
      cpkt_sus_backend_capabilities() == 0 || cpkt_sus_facade_version() == 0) {
    return 1;
  }
  if (strcmp(cpkt_sus_backend_capabilities(), "cpu") != 0) {
    return 2;
  }
  if (cpkt_sus_result_string(CPKT_SUS_ERR_MODEL) == 0) {
    return 3;
  }
  if (cpkt_sus_result_string(CPKT_SUS_ABORTED) == 0) {
    return 11;
  }
  if (cpkt_sus_model_catalog_count() == 0) {
    return 4;
  }
  if (cpkt_sus_model_catalog_default(&entry) != CPKT_SUS_OK) {
    return 5;
  }
  if (entry.name == 0 || strcmp(entry.name, "tiny") != 0) {
    return 6;
  }
  if (cpkt_sus_model_catalog_find("kb-whisper-small", &entry) != CPKT_SUS_OK) {
    return 7;
  }
  if (entry.provider == 0 ||
      strcmp(entry.provider, "KBLab/kb-whisper-small") != 0) {
    return 8;
  }
  memset(&config, 0, sizeof(config));
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
  event.is_final = 1;
  if (segmented_config.mode != CPKT_SUS_SEGMENT_MODE_CONTINUOUS ||
      segmented_config.step_ms != 1000UL ||
      segmented_config.prebuffer_ms != 50UL || event.is_final == 0) {
    return 12;
  }
  if (sizeof(((cpkt_sus_transcriber *)0)
                 ->transcribe_audio_decoder_segmented_text) == 0 ||
      sizeof(((cpkt_sus_transcriber *)0)->revised_text) == 0) {
    return 13;
  }
  config.model_path = "";
  model = (cpkt_sus *)1;
  if (cpkt_sus_open_path(&model, &config) != CPKT_SUS_ERR_ARG) {
    return 9;
  }
  if (model != 0) {
    return 10;
  }
  cpkt_sus_string_free(0);
  return 0;
}
