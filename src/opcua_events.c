/* Handwritten OPC UA facade: events. */
#include "opcua_facade_internal.h"
#include <stdlib.h>

struct cpkt_opcua_server_event {
  cpkt_opcua_node_id source_node_id;
  cpkt_opcua_node_id event_type_id;
  struct cpkt_owned_node_id_memory source_memory;
  struct cpkt_owned_node_id_memory event_type_memory;
  unsigned long severity;
  char *message;
  UA_KeyValueMap fields;
};

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_server_event_free(cpkt_opcua_server_event *event) {
  if (event != NULL) {
    UA_KeyValueMap_clear(&event->fields);
    cpkt_owned_node_id_memory_clear(&event->source_memory);
    cpkt_owned_node_id_memory_clear(&event->event_type_memory);
    free(event->message);
    free(event);
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_create_event(
    cpkt_opcua_node_id source_node_id, cpkt_opcua_node_id event_type_id,
    unsigned long severity, const char *message,
    cpkt_opcua_server_event **event_out, cpkt_opcua_status *status_out) {
  cpkt_opcua_server_event *event;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (event_out != NULL) {
    *event_out = NULL;
  }
  if (event_out == NULL || message == NULL ||
      severity > (unsigned long)USHRT_MAX ||
      !cpkt_valid_node_id(source_node_id) ||
      !cpkt_valid_node_id(event_type_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  event = (cpkt_opcua_server_event *)calloc(1, sizeof(*event));
  if (event == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  event->severity = severity;
  event->message = cpkt_strdup_c89(message);
  if (event->message == NULL) {
    cpkt_opcua_server_event_free(event);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  result = cpkt_copy_facade_node_id(source_node_id, &event->source_node_id,
                                    &event->source_memory);
  if (result != CPKT_OPCUA_OK) {
    cpkt_opcua_server_event_free(event);
    return result;
  }
  result = cpkt_copy_facade_node_id(event_type_id, &event->event_type_id,
                                    &event->event_type_memory);
  if (result != CPKT_OPCUA_OK) {
    cpkt_opcua_server_event_free(event);
    return result;
  }
  event->fields.mapSize = 0;
  event->fields.map = NULL;
  *event_out = event;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_event_set_field(
    cpkt_opcua_server_event *event, unsigned short namespace_index,
    const char *field_name, const cpkt_opcua_value *value,
    cpkt_opcua_status *status_out) {
  UA_QualifiedName key;
  UA_Variant variant;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (event == NULL || field_name == NULL || field_name[0] == '\0' ||
      value == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  UA_Variant_init(&variant);
  result = cpkt_set_variant(&variant, value);
  if (result != CPKT_OPCUA_OK) {
    UA_Variant_clear(&variant);
    return result;
  }
  key = UA_QUALIFIEDNAME(namespace_index, (char *)field_name);
  status = UA_KeyValueMap_set(&event->fields, key, &variant);
  UA_Variant_clear(&variant);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_event_trigger(
    cpkt_opcua_server *server, cpkt_opcua_server_event *event,
    unsigned char *event_id_buffer, size_t event_id_buffer_size,
    size_t *required_event_id_size_out, cpkt_opcua_status *status_out) {
  UA_NodeId native_source_node_id;
  UA_NodeId native_event_type_id;
  UA_LocalizedText native_message;
  UA_ByteString event_id;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_event_id_size_out != NULL) {
    *required_event_id_size_out = 0;
  }
  if (server == NULL || server->server == NULL || event == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_source_node_id = cpkt_make_node_id(event->source_node_id);
  native_event_type_id = cpkt_make_node_id(event->event_type_id);
  native_message = UA_LOCALIZEDTEXT((char *)"en-US", event->message);
  UA_ByteString_init(&event_id);
  status =
      UA_Server_createEvent(server->server, native_source_node_id,
                            native_event_type_id, (UA_UInt16)event->severity,
                            native_message, &event->fields, NULL, &event_id);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_ByteString_clear(&event_id);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_ua_byte_string_to_buffer(&event_id, event_id_buffer,
                                              event_id_buffer_size,
                                              required_event_id_size_out);
  UA_ByteString_clear(&event_id);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_trigger_event(
    cpkt_opcua_server *server, cpkt_opcua_node_id source_node_id,
    cpkt_opcua_node_id event_type_id, unsigned long severity,
    const char *message, unsigned char *event_id_buffer,
    size_t event_id_buffer_size, size_t *required_event_id_size_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_source_node_id;
  UA_NodeId native_event_type_id;
  UA_LocalizedText native_message;
  UA_ByteString event_id;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_event_id_size_out != NULL) {
    *required_event_id_size_out = 0;
  }
  if (server == NULL || server->server == NULL || message == NULL ||
      severity > (unsigned long)USHRT_MAX ||
      !cpkt_valid_node_id(source_node_id) ||
      !cpkt_valid_node_id(event_type_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_source_node_id = cpkt_make_node_id(source_node_id);
  native_event_type_id = cpkt_make_node_id(event_type_id);
  native_message = UA_LOCALIZEDTEXT((char *)"en-US", (char *)message);
  UA_ByteString_init(&event_id);
  status = UA_Server_createEvent(server->server, native_source_node_id,
                                 native_event_type_id, (UA_UInt16)severity,
                                 native_message, NULL, NULL, &event_id);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_ByteString_clear(&event_id);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_ua_byte_string_to_buffer(&event_id, event_id_buffer,
                                              event_id_buffer_size,
                                              required_event_id_size_out);
  UA_ByteString_clear(&event_id);
  return result;
}
