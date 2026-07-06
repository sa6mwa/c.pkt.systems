#include <cpkt/audio.h>
#include <cpkt/lua_runtime.h>
#include <cpkt/opcua.h>
#include <cpkt/sus.h>

int cpkt_static_archive_pic_smoke_probe(void) {
  cpkt_opcua_node_id null_node;
  cpkt_sus_config sus_config;

  if (cpkt_lua_runtime_facade_version() == 0) {
    return 1;
  }
  if (cpkt_audio_result_string(CPKT_AUDIO_OK) == 0 ||
      !cpkt_audio_format_can_decode(CPKT_AUDIO_FORMAT_WAV)) {
    return 2;
  }
  cpkt_sus_config_default(&sus_config);
  if (cpkt_sus_backend_capabilities() == 0 ||
      cpkt_sus_facade_version() == 0 ||
      cpkt_sus_result_string(CPKT_SUS_OK) == 0) {
    return 3;
  }
  null_node = cpkt_opcua_node_id_null();
  if (null_node.identifier_type != CPKT_OPCUA_NODE_ID_NULL ||
      cpkt_opcua_facade_version() == 0 ||
      cpkt_opcua_result_string(CPKT_OPCUA_OK) == 0) {
    return 4;
  }
  if (sus_config.cpu_only != 0) {
    return 5;
  }
  return 0;
}
