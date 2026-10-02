/* Handwritten OPC UA facade: client values. */
#include "opcua_facade_internal.h"
#include <open62541/client_highlevel.h>

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_value *value_out, char *string_buffer, size_t string_buffer_size,
    size_t *required_string_size_out, cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Variant variant;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || value_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_Variant_init(&variant);
  status =
      UA_Client_readValueAttribute(client->client, native_node_id, &variant);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_get_variant(&variant, value_out, string_buffer,
                            string_buffer_size, required_string_size_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_native_variant(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_native_variant_fn fn, void *user,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Variant variant;
  UA_StatusCode status;
  int callback_status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || fn == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_Variant_init(&variant);
  status =
      UA_Client_readValueAttribute(client->client, native_node_id, &variant);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_Variant_clear(&variant);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  callback_status = fn(&variant, user);
  UA_Variant_clear(&variant);
  return callback_status == 0 ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_CALLBACK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_native_data_value(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_native_data_value_fn fn, void *user,
    cpkt_opcua_status *status_out) {
  UA_ReadRequest request;
  UA_ReadResponse response;
  UA_ReadValueId read_value;
  UA_StatusCode service_status;
  int callback_status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || fn == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  UA_ReadValueId_init(&read_value);
  read_value.nodeId = cpkt_make_node_id(node_id);
  read_value.attributeId = UA_ATTRIBUTEID_VALUE;
  UA_ReadRequest_init(&request);
  request.timestampsToReturn = UA_TIMESTAMPSTORETURN_BOTH;
  request.nodesToRead = &read_value;
  request.nodesToReadSize = 1;
  response = UA_Client_Service_read(client->client, request);
  service_status = response.responseHeader.serviceResult;
  if (service_status != UA_STATUSCODE_GOOD || response.resultsSize != 1 ||
      response.results == NULL) {
    if (status_out != NULL) {
      *status_out = cpkt_status(service_status);
    }
    UA_ReadResponse_clear(&response);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (status_out != NULL) {
    *status_out = cpkt_status(response.results[0].status);
  }
  callback_status = fn(&response.results[0], user);
  UA_ReadResponse_clear(&response);
  return callback_status == 0 ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_CALLBACK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_data_value(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_data_value *data_value_out, char *string_buffer,
    size_t string_buffer_size, size_t *required_string_size_out,
    cpkt_opcua_status *status_out) {
  UA_ReadRequest request;
  UA_ReadResponse response;
  UA_ReadValueId read_value;
  UA_StatusCode service_status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_string_size_out != NULL) {
    *required_string_size_out = 0;
  }
  if (data_value_out != NULL) {
    cpkt_opcua_data_value_clear(data_value_out);
  }
  if (client == NULL || client->client == NULL || data_value_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  UA_ReadValueId_init(&read_value);
  read_value.nodeId = cpkt_make_node_id(node_id);
  read_value.attributeId = UA_ATTRIBUTEID_VALUE;
  UA_ReadRequest_init(&request);
  request.timestampsToReturn = UA_TIMESTAMPSTORETURN_BOTH;
  request.nodesToRead = &read_value;
  request.nodesToReadSize = 1;
  response = UA_Client_Service_read(client->client, request);
  service_status = response.responseHeader.serviceResult;
  if (service_status != UA_STATUSCODE_GOOD || response.resultsSize != 1 ||
      response.results == NULL) {
    if (status_out != NULL) {
      *status_out = cpkt_status(service_status);
    }
    UA_ReadResponse_clear(&response);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (status_out != NULL) {
    *status_out = cpkt_status(response.results[0].status);
  }
  result =
      cpkt_get_data_value(&response.results[0], data_value_out, string_buffer,
                          string_buffer_size, required_string_size_out);
  UA_ReadResponse_clear(&response);
  return result;
}

static cpkt_opcua_result cpkt_client_read_array_variant(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    UA_Variant *variant_out, cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (variant_out != NULL) {
    UA_Variant_init(variant_out);
  }
  if (client == NULL || client->client == NULL || variant_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status =
      UA_Client_readValueAttribute(client->client, native_node_id, variant_out);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_boolean_array(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id, int *values,
    size_t value_count, size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result =
      cpkt_client_read_array_variant(client, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_boolean_array_from_variant(&variant, values, value_count,
                                                required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_integer_array(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id, long *values,
    size_t value_count, size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result =
      cpkt_client_read_array_variant(client, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_integer_array_from_variant(&variant, values, value_count,
                                                required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_uint64_array(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_uint64 *values, size_t value_count,
    size_t *required_value_count_out, cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result =
      cpkt_client_read_array_variant(client, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_uint64_array_from_variant(&variant, values, value_count,
                                               required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_datetime_array(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_datetime *values, size_t value_count,
    size_t *required_value_count_out, cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result =
      cpkt_client_read_array_variant(client, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_datetime_array_from_variant(&variant, values, value_count,
                                                 required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_status_array(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_status *values, size_t value_count,
    size_t *required_value_count_out, cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result =
      cpkt_client_read_array_variant(client, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_status_array_from_variant(&variant, values, value_count,
                                               required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_guid_array(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_guid *values, size_t value_count,
    size_t *required_value_count_out, cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result =
      cpkt_client_read_array_variant(client, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_guid_array_from_variant(&variant, values, value_count,
                                             required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_qualified_name_array(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_qualified_name_array_fn fn, void *user, size_t *value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result =
      cpkt_client_read_array_variant(client, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_each_qualified_name_array_from_variant(&variant, fn, user,
                                                       value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_localized_text_array(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_localized_text_array_fn fn, void *user, size_t *value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result =
      cpkt_client_read_array_variant(client, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_each_localized_text_array_from_variant(&variant, fn, user,
                                                       value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_double_array(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id, double *values,
    size_t value_count, size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result =
      cpkt_client_read_array_variant(client, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_double_array_from_variant(&variant, values, value_count,
                                               required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_string_array(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_string_array_fn fn, void *user, size_t *value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result =
      cpkt_client_read_array_variant(client, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result =
      cpkt_each_string_array_from_variant(&variant, fn, user, value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_byte_string_array(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_byte_string_array_fn fn, void *user, size_t *value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result =
      cpkt_client_read_array_variant(client, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_each_byte_string_array_from_variant(&variant, fn, user,
                                                    value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

static cpkt_opcua_result cpkt_client_read_index_range_variant(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    const char *index_range, UA_Variant *variant_out,
    cpkt_opcua_status *status_out) {
  UA_ReadRequest request;
  UA_ReadValueId read_value;
  UA_ReadResponse response;
  UA_StatusCode copy_status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (variant_out != NULL) {
    UA_Variant_init(variant_out);
  }
  if (client == NULL || client->client == NULL || index_range == NULL ||
      variant_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  UA_ReadRequest_init(&request);
  UA_ReadValueId_init(&read_value);
  read_value.nodeId = cpkt_make_node_id(node_id);
  read_value.attributeId = UA_ATTRIBUTEID_VALUE;
  read_value.indexRange = UA_STRING((char *)index_range);
  request.nodesToRead = &read_value;
  request.nodesToReadSize = 1;
  response = UA_Client_Service_read(client->client, request);
  if (response.responseHeader.serviceResult != UA_STATUSCODE_GOOD) {
    if (status_out != NULL) {
      *status_out = cpkt_status(response.responseHeader.serviceResult);
    }
    UA_ReadResponse_clear(&response);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (response.resultsSize != 1) {
    if (status_out != NULL) {
      *status_out = cpkt_status(UA_STATUSCODE_BADINTERNALERROR);
    }
    UA_ReadResponse_clear(&response);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (status_out != NULL) {
    *status_out = cpkt_status(response.results[0].status);
  }
  if (response.results[0].status != UA_STATUSCODE_GOOD) {
    UA_ReadResponse_clear(&response);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  copy_status = UA_Variant_copy(&response.results[0].value, variant_out);
  UA_ReadResponse_clear(&response);
  return copy_status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK
                                           : CPKT_OPCUA_ERR_ALLOC;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_boolean_array_range(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    const char *index_range, int *values, size_t value_count,
    size_t *required_value_count_out, cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_client_read_index_range_variant(client, node_id, index_range,
                                                &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_boolean_array_from_variant(&variant, values, value_count,
                                                required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_integer_array_range(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    const char *index_range, long *values, size_t value_count,
    size_t *required_value_count_out, cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_client_read_index_range_variant(client, node_id, index_range,
                                                &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_integer_array_from_variant(&variant, values, value_count,
                                                required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_double_array_range(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    const char *index_range, double *values, size_t value_count,
    size_t *required_value_count_out, cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_client_read_index_range_variant(client, node_id, index_range,
                                                &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_double_array_from_variant(&variant, values, value_count,
                                               required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_uint64_array_range(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    const char *index_range, cpkt_opcua_uint64 *values, size_t value_count,
    size_t *required_value_count_out, cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_client_read_index_range_variant(client, node_id, index_range,
                                                &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_uint64_array_from_variant(&variant, values, value_count,
                                               required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_datetime_array_range(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    const char *index_range, cpkt_opcua_datetime *values, size_t value_count,
    size_t *required_value_count_out, cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_client_read_index_range_variant(client, node_id, index_range,
                                                &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_datetime_array_from_variant(&variant, values, value_count,
                                                 required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_status_array_range(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    const char *index_range, cpkt_opcua_status *values, size_t value_count,
    size_t *required_value_count_out, cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_client_read_index_range_variant(client, node_id, index_range,
                                                &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_status_array_from_variant(&variant, values, value_count,
                                               required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_guid_array_range(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    const char *index_range, cpkt_opcua_guid *values, size_t value_count,
    size_t *required_value_count_out, cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_client_read_index_range_variant(client, node_id, index_range,
                                                &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_guid_array_from_variant(&variant, values, value_count,
                                             required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_qualified_name_array_range(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    const char *index_range, cpkt_opcua_qualified_name_array_fn fn, void *user,
    size_t *value_count_out, cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (value_count_out != NULL) {
    *value_count_out = 0;
  }
  if (value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_client_read_index_range_variant(client, node_id, index_range,
                                                &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_each_qualified_name_range_from_variant(&variant, fn, user,
                                                       value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_localized_text_array_range(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    const char *index_range, cpkt_opcua_localized_text_array_fn fn, void *user,
    size_t *value_count_out, cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (value_count_out != NULL) {
    *value_count_out = 0;
  }
  if (value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_client_read_index_range_variant(client, node_id, index_range,
                                                &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_each_localized_text_range_from_variant(&variant, fn, user,
                                                       value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_string_array_range(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    const char *index_range, cpkt_opcua_string_array_fn fn, void *user,
    size_t *value_count_out, cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;
  cpkt_opcua_status local_status;
  cpkt_opcua_status *effective_status;
  size_t start;
  size_t count;

  if (value_count_out != NULL) {
    *value_count_out = 0;
  }
  if (value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  effective_status = status_out != NULL ? status_out : &local_status;
  result = cpkt_client_read_index_range_variant(client, node_id, index_range,
                                                &variant, effective_status);
  if (result != CPKT_OPCUA_OK) {
    if (result != CPKT_OPCUA_ERR_UPSTREAM ||
        *effective_status != cpkt_status(UA_STATUSCODE_BADINDEXRANGEINVALID)) {
      return result;
    }
    if (!cpkt_parse_simple_array_index_range(index_range, &start, &count)) {
      return result;
    }
    result = cpkt_client_read_array_variant(client, node_id, &variant,
                                            effective_status);
    if (result != CPKT_OPCUA_OK) {
      return result;
    }
    result = cpkt_each_string_array_slice_from_variant(
        &variant, start, count, fn, user, value_count_out);
    if (result == CPKT_OPCUA_ERR_RANGE && status_out != NULL) {
      *status_out = cpkt_status(UA_STATUSCODE_BADINDEXRANGENODATA);
    }
    UA_Variant_clear(&variant);
    return result;
  }
  result =
      cpkt_each_string_range_from_variant(&variant, fn, user, value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_byte_string_array_range(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    const char *index_range, cpkt_opcua_byte_string_array_fn fn, void *user,
    size_t *value_count_out, cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;
  cpkt_opcua_status local_status;
  cpkt_opcua_status *effective_status;
  size_t start;
  size_t count;

  if (value_count_out != NULL) {
    *value_count_out = 0;
  }
  if (value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  effective_status = status_out != NULL ? status_out : &local_status;
  result = cpkt_client_read_index_range_variant(client, node_id, index_range,
                                                &variant, effective_status);
  if (result != CPKT_OPCUA_OK) {
    if (result != CPKT_OPCUA_ERR_UPSTREAM ||
        *effective_status != cpkt_status(UA_STATUSCODE_BADINDEXRANGEINVALID)) {
      return result;
    }
    if (!cpkt_parse_simple_array_index_range(index_range, &start, &count)) {
      return result;
    }
    result = cpkt_client_read_array_variant(client, node_id, &variant,
                                            effective_status);
    if (result != CPKT_OPCUA_OK) {
      return result;
    }
    result = cpkt_each_byte_string_array_slice_from_variant(
        &variant, start, count, fn, user, value_count_out);
    if (result == CPKT_OPCUA_ERR_RANGE && status_out != NULL) {
      *status_out = cpkt_status(UA_STATUSCODE_BADINDEXRANGENODATA);
    }
    UA_Variant_clear(&variant);
    return result;
  }
  result = cpkt_each_byte_string_range_from_variant(&variant, fn, user,
                                                    value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_index_range(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    const char *index_range, const cpkt_opcua_value *value,
    cpkt_opcua_status *status_out) {
  UA_WriteRequest request;
  UA_WriteValue write_value;
  UA_WriteResponse response;
  UA_Variant variant;
  cpkt_opcua_result result;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || index_range == NULL ||
      value == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  UA_Variant_init(&variant);
  result = cpkt_set_variant(&variant, value);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  UA_WriteRequest_init(&request);
  UA_WriteValue_init(&write_value);
  write_value.nodeId = cpkt_make_node_id(node_id);
  write_value.attributeId = UA_ATTRIBUTEID_VALUE;
  write_value.indexRange = UA_STRING((char *)index_range);
  write_value.value.hasValue = true;
  write_value.value.value = variant;
  request.nodesToWrite = &write_value;
  request.nodesToWriteSize = 1;
  response = UA_Client_Service_write(client->client, request);
  status = response.responseHeader.serviceResult;
  if (status == UA_STATUSCODE_GOOD && response.resultsSize == 1) {
    status = response.results[0];
  } else if (status == UA_STATUSCODE_GOOD) {
    status = UA_STATUSCODE_BADINTERNALERROR;
  }
  UA_WriteResponse_clear(&response);
  UA_Variant_clear(&variant);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write(cpkt_opcua_client *client,
                                          cpkt_opcua_node_id node_id,
                                          const cpkt_opcua_value *value,
                                          cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Variant variant;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || value == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  UA_Variant_init(&variant);
  result = cpkt_set_variant(&variant, value);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status =
      UA_Client_writeValueAttribute(client->client, native_node_id, &variant);
  UA_Variant_clear(&variant);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}
