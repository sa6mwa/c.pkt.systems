/* Handwritten OPC UA facade: history. */
#include "opcua_facade_internal.h"
#include <open62541/client_highlevel.h>

struct cpkt_opcua_history_read_context {
  cpkt_opcua_history_data_value_fn fn;
  void *user;
  char *string_buffer;
  size_t string_buffer_size;
  size_t *required_string_size_out;
  cpkt_opcua_result result;
};

static UA_Boolean cpkt_history_read_raw_callback(UA_Client *client,
                                                 const UA_NodeId *node_id,
                                                 UA_Boolean more_data_available,
                                                 const UA_ExtensionObject *data,
                                                 void *callback_context) {
  struct cpkt_opcua_history_read_context *context;
  UA_HistoryData *history_data;
  cpkt_opcua_data_value data_value;
  size_t i;

  (void)client;
  (void)node_id;
  context = (struct cpkt_opcua_history_read_context *)callback_context;
  if (context == NULL || context->fn == NULL || data == NULL) {
    return false;
  }
  if (data->encoding != UA_EXTENSIONOBJECT_DECODED ||
      data->content.decoded.type != &UA_TYPES[UA_TYPES_HISTORYDATA] ||
      data->content.decoded.data == NULL) {
    context->result = CPKT_OPCUA_ERR_TYPE;
    return false;
  }
  history_data = (UA_HistoryData *)data->content.decoded.data;
  for (i = 0; i < history_data->dataValuesSize; ++i) {
    cpkt_opcua_data_value_clear(&data_value);
    context->result = cpkt_get_data_value(
        &history_data->dataValues[i], &data_value, context->string_buffer,
        context->string_buffer_size, context->required_string_size_out);
    if (context->result != CPKT_OPCUA_OK) {
      return false;
    }
    if (context->fn(&data_value, more_data_available ? 1 : 0, context->user) !=
        0) {
      cpkt_opcua_data_value_clear(&data_value);
      context->result = CPKT_OPCUA_ERR_CALLBACK;
      return false;
    }
    cpkt_opcua_data_value_clear(&data_value);
  }
  return true;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_history_read_raw(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_datetime start_time, cpkt_opcua_datetime end_time,
    const char *index_range, int return_bounds,
    unsigned long values_per_response, cpkt_opcua_history_data_value_fn fn,
    void *user, char *string_buffer, size_t string_buffer_size,
    size_t *required_string_size_out, cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_String native_index_range;
  UA_StatusCode status;
  struct cpkt_opcua_history_read_context context;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_string_size_out != NULL) {
    *required_string_size_out = 0;
  }
  if (client == NULL || client->client == NULL || fn == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!cpkt_valid_datetime_words(start_time.high32, start_time.low32) ||
      !cpkt_valid_datetime_words(end_time.high32, end_time.low32) ||
      values_per_response > (unsigned long)UINT_MAX) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_index_range = UA_STRING_NULL;
  if (index_range != NULL) {
    native_index_range = UA_STRING((char *)index_range);
  }
  context.fn = fn;
  context.user = user;
  context.string_buffer = string_buffer;
  context.string_buffer_size = string_buffer_size;
  context.required_string_size_out = required_string_size_out;
  context.result = CPKT_OPCUA_OK;
  status = UA_Client_HistoryRead_raw(
      client->client, &native_node_id, cpkt_history_read_raw_callback,
      cpkt_make_datetime(start_time), cpkt_make_datetime(end_time),
      native_index_range, return_bounds ? true : false,
      (UA_UInt32)values_per_response, UA_TIMESTAMPSTORETURN_BOTH, &context);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (context.result != CPKT_OPCUA_OK) {
    return context.result;
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}
