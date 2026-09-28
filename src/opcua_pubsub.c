/* Handwritten OPC UA facade: pubsub. */
#include "opcua_facade_internal.h"
#include <open62541/server_pubsub.h>
#include <string.h>

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_mqtt_connection_options_default(
    cpkt_opcua_mqtt_connection_options *options) {
  if (options == NULL) {
    return;
  }
  memset(options, 0, sizeof(*options));
  options->broker_port = 1883;
  options->keep_alive_seconds = 400;
  options->enabled = 1;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_pubsub_writer_group_options_default(
    cpkt_opcua_pubsub_writer_group_options *options) {
  if (options == NULL) {
    return;
  }
  memset(options, 0, sizeof(*options));
  options->enabled = 1;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_pubsub_data_set_writer_options_default(
    cpkt_opcua_pubsub_data_set_writer_options *options) {
  if (options == NULL) {
    return;
  }
  memset(options, 0, sizeof(*options));
  options->key_frame_count = 1;
  options->enabled = 1;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_pubsub_reader_group_options_default(
    cpkt_opcua_pubsub_reader_group_options *options) {
  if (options == NULL) {
    return;
  }
  memset(options, 0, sizeof(*options));
  options->enabled = 1;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_pubsub_data_set_reader_options_default(
    cpkt_opcua_pubsub_data_set_reader_options *options) {
  if (options == NULL) {
    return;
  }
  memset(options, 0, sizeof(*options));
  options->enabled = 1;
}

static UA_String cpkt_borrowed_string_or_null(const char *value) {
  if (value == NULL) {
    return UA_STRING_NULL;
  }
  return UA_STRING((char *)value);
}

static cpkt_opcua_result cpkt_pubsub_map_set_string(UA_KeyValueMap *map,
                                                    const char *key,
                                                    const char *value) {
  UA_String string_value;
  UA_StatusCode status;

  if (value == NULL) {
    return CPKT_OPCUA_OK;
  }
  string_value = UA_STRING((char *)value);
  status = UA_KeyValueMap_setScalar(map, UA_QUALIFIEDNAME(0, (char *)key),
                                    &string_value, &UA_TYPES[UA_TYPES_STRING]);
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_ALLOC;
}

static cpkt_opcua_result cpkt_pubsub_map_set_uint16(UA_KeyValueMap *map,
                                                    const char *key,
                                                    unsigned short value) {
  UA_UInt16 native_value;
  UA_StatusCode status;

  native_value = (UA_UInt16)value;
  status = UA_KeyValueMap_setScalar(map, UA_QUALIFIEDNAME(0, (char *)key),
                                    &native_value, &UA_TYPES[UA_TYPES_UINT16]);
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_ALLOC;
}

static cpkt_opcua_result
cpkt_pubsub_map_set_boolean(UA_KeyValueMap *map, const char *key, int value) {
  UA_Boolean native_value;
  UA_StatusCode status;

  native_value = value ? true : false;
  status = UA_KeyValueMap_setScalar(map, UA_QUALIFIEDNAME(0, (char *)key),
                                    &native_value, &UA_TYPES[UA_TYPES_BOOLEAN]);
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_ALLOC;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_mqtt_pubsub_connection(
    cpkt_opcua_server *server,
    const cpkt_opcua_mqtt_connection_options *options,
    cpkt_opcua_node_id *connection_id_out, char *node_id_buffer,
    size_t node_id_buffer_size, size_t *required_node_id_size_out,
    cpkt_opcua_status *status_out) {
  UA_PubSubConnectionConfig config;
  UA_NetworkAddressUrlDataType address;
  UA_NodeId connection_id;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (connection_id_out != NULL) {
    *connection_id_out = cpkt_opcua_node_id_null();
  }
  if (server == NULL || server->server == NULL || options == NULL ||
      connection_id_out == NULL || options->broker_host == NULL ||
      options->topic == NULL ||
      options->publisher_id > CPKT_OPCUA_UINT32_MAX_VALUE) {
    return CPKT_OPCUA_ERR_ARG;
  }
  memset(&config, 0, sizeof(config));
  memset(&address, 0, sizeof(address));
  UA_NodeId_init(&connection_id);
  config.name = cpkt_borrowed_string_or_null(options->name);
  config.enabled = options->enabled ? true : false;
  config.publisherId.idType = UA_PUBLISHERIDTYPE_UINT32;
  config.publisherId.id.uint32 = (UA_UInt32)options->publisher_id;
  config.transportProfileUri = UA_STRING(
      (char *)"http://opcfoundation.org/UA-Profile/Transport/pubsub-mqtt-uadp");
  address.url = UA_STRING((char *)options->topic);
  UA_Variant_setScalar(&config.address, &address,
                       &UA_TYPES[UA_TYPES_NETWORKADDRESSURLDATATYPE]);
  result = cpkt_pubsub_map_set_string(&config.connectionProperties, "address",
                                      options->broker_host);
  if (result == CPKT_OPCUA_OK) {
    result = cpkt_pubsub_map_set_uint16(&config.connectionProperties, "port",
                                        options->broker_port);
  }
  if (result == CPKT_OPCUA_OK) {
    result = cpkt_pubsub_map_set_string(&config.connectionProperties, "topic",
                                        options->topic);
  }
  if (result == CPKT_OPCUA_OK) {
    result = cpkt_pubsub_map_set_boolean(&config.connectionProperties,
                                         "subscribe", options->subscribe);
  }
  if (result == CPKT_OPCUA_OK) {
    result = cpkt_pubsub_map_set_string(&config.connectionProperties,
                                        "username", options->username);
  }
  if (result == CPKT_OPCUA_OK) {
    result = cpkt_pubsub_map_set_string(&config.connectionProperties,
                                        "password", options->password);
  }
  if (result == CPKT_OPCUA_OK) {
    result =
        cpkt_pubsub_map_set_uint16(&config.connectionProperties, "keep-alive",
                                   options->keep_alive_seconds);
  }
  if (result == CPKT_OPCUA_OK) {
    result = cpkt_pubsub_map_set_boolean(&config.connectionProperties,
                                         "validate", options->validate_only);
  }
  if (result != CPKT_OPCUA_OK) {
    UA_KeyValueMap_clear(&config.connectionProperties);
    return result;
  }
  status =
      UA_Server_addPubSubConnection(server->server, &config, &connection_id);
  UA_KeyValueMap_clear(&config.connectionProperties);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_NodeId_clear(&connection_id);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_native_node_id_to_output(
      &connection_id, connection_id_out, node_id_buffer, node_id_buffer_size,
      required_node_id_size_out);
  UA_NodeId_clear(&connection_id);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_published_dataset(
    cpkt_opcua_server *server, const char *name,
    cpkt_opcua_node_id *published_dataset_id_out, char *node_id_buffer,
    size_t node_id_buffer_size, size_t *required_node_id_size_out,
    cpkt_opcua_status *status_out) {
  UA_PublishedDataSetConfig config;
  UA_AddPublishedDataSetResult add_result;
  UA_NodeId published_dataset_id;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (published_dataset_id_out != NULL) {
    *published_dataset_id_out = cpkt_opcua_node_id_null();
  }
  if (server == NULL || server->server == NULL || name == NULL ||
      published_dataset_id_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  memset(&config, 0, sizeof(config));
  UA_NodeId_init(&published_dataset_id);
  config.name = UA_STRING((char *)name);
  config.publishedDataSetType = UA_PUBSUB_DATASET_PUBLISHEDITEMS;
  add_result = UA_Server_addPublishedDataSet(server->server, &config,
                                             &published_dataset_id);
  if (status_out != NULL) {
    *status_out = cpkt_status(add_result.addResult);
  }
  if (add_result.addResult != UA_STATUSCODE_GOOD) {
    UA_NodeId_clear(&published_dataset_id);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_native_node_id_to_output(
      &published_dataset_id, published_dataset_id_out, node_id_buffer,
      node_id_buffer_size, required_node_id_size_out);
  UA_NodeId_clear(&published_dataset_id);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_published_variable(
    cpkt_opcua_server *server, cpkt_opcua_node_id published_dataset_id,
    cpkt_opcua_node_id variable_node_id, const char *field_name,
    cpkt_opcua_node_id *field_id_out, char *node_id_buffer,
    size_t node_id_buffer_size, size_t *required_node_id_size_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_published_dataset_id;
  UA_NodeId native_variable_node_id;
  UA_NodeId field_id;
  UA_DataSetFieldConfig config;
  UA_DataSetFieldResult add_result;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (field_id_out != NULL) {
    *field_id_out = cpkt_opcua_node_id_null();
  }
  if (server == NULL || server->server == NULL || field_id_out == NULL ||
      !cpkt_valid_node_id(published_dataset_id) ||
      !cpkt_valid_node_id(variable_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_published_dataset_id = cpkt_make_node_id(published_dataset_id);
  native_variable_node_id = cpkt_make_node_id(variable_node_id);
  UA_NodeId_init(&field_id);
  memset(&config, 0, sizeof(config));
  config.dataSetFieldType = UA_PUBSUB_DATASETFIELD_VARIABLE;
  config.field.variable.fieldNameAlias =
      cpkt_borrowed_string_or_null(field_name);
  config.field.variable.publishParameters.publishedVariable =
      native_variable_node_id;
  config.field.variable.publishParameters.attributeId = UA_ATTRIBUTEID_VALUE;
  add_result = UA_Server_addDataSetField(
      server->server, native_published_dataset_id, &config, &field_id);
  if (status_out != NULL) {
    *status_out = cpkt_status(add_result.result);
  }
  if (add_result.result != UA_STATUSCODE_GOOD) {
    UA_NodeId_clear(&field_id);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_native_node_id_to_output(
      &field_id, field_id_out, node_id_buffer, node_id_buffer_size,
      required_node_id_size_out);
  UA_NodeId_clear(&field_id);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_pubsub_writer_group(
    cpkt_opcua_server *server, cpkt_opcua_node_id connection_id,
    const cpkt_opcua_pubsub_writer_group_options *options,
    cpkt_opcua_node_id *writer_group_id_out, char *node_id_buffer,
    size_t node_id_buffer_size, size_t *required_node_id_size_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_connection_id;
  UA_NodeId writer_group_id;
  UA_WriterGroupConfig config;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (writer_group_id_out != NULL) {
    *writer_group_id_out = cpkt_opcua_node_id_null();
  }
  if (server == NULL || server->server == NULL || options == NULL ||
      writer_group_id_out == NULL || !cpkt_valid_node_id(connection_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_connection_id = cpkt_make_node_id(connection_id);
  UA_NodeId_init(&writer_group_id);
  memset(&config, 0, sizeof(config));
  config.name = cpkt_borrowed_string_or_null(options->name);
  config.enabled = options->enabled ? true : false;
  config.writerGroupId = (UA_UInt16)options->writer_group_id;
  config.publishingInterval = options->publishing_interval_ms;
  config.encodingMimeType = options->json_encoding ? UA_PUBSUB_ENCODING_JSON
                                                   : UA_PUBSUB_ENCODING_UADP;
  status = UA_Server_addWriterGroup(server->server, native_connection_id,
                                    &config, &writer_group_id);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_NodeId_clear(&writer_group_id);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_native_node_id_to_output(
      &writer_group_id, writer_group_id_out, node_id_buffer,
      node_id_buffer_size, required_node_id_size_out);
  UA_NodeId_clear(&writer_group_id);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_pubsub_data_set_writer(
    cpkt_opcua_server *server, cpkt_opcua_node_id writer_group_id,
    cpkt_opcua_node_id published_dataset_id,
    const cpkt_opcua_pubsub_data_set_writer_options *options,
    cpkt_opcua_node_id *data_set_writer_id_out, char *node_id_buffer,
    size_t node_id_buffer_size, size_t *required_node_id_size_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_writer_group_id;
  UA_NodeId native_published_dataset_id;
  UA_NodeId data_set_writer_id;
  UA_DataSetWriterConfig config;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (data_set_writer_id_out != NULL) {
    *data_set_writer_id_out = cpkt_opcua_node_id_null();
  }
  if (server == NULL || server->server == NULL || options == NULL ||
      data_set_writer_id_out == NULL || !cpkt_valid_node_id(writer_group_id) ||
      !cpkt_valid_node_id(published_dataset_id) ||
      options->key_frame_count > CPKT_OPCUA_UINT32_MAX_VALUE) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_writer_group_id = cpkt_make_node_id(writer_group_id);
  native_published_dataset_id = cpkt_make_node_id(published_dataset_id);
  UA_NodeId_init(&data_set_writer_id);
  memset(&config, 0, sizeof(config));
  config.name = cpkt_borrowed_string_or_null(options->name);
  config.enabled = options->enabled ? true : false;
  config.dataSetWriterId = (UA_UInt16)options->data_set_writer_id;
  config.keyFrameCount = (UA_UInt32)options->key_frame_count;
  config.dataSetName = cpkt_borrowed_string_or_null(options->name);
  status = UA_Server_addDataSetWriter(server->server, native_writer_group_id,
                                      native_published_dataset_id, &config,
                                      &data_set_writer_id);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_NodeId_clear(&data_set_writer_id);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_native_node_id_to_output(
      &data_set_writer_id, data_set_writer_id_out, node_id_buffer,
      node_id_buffer_size, required_node_id_size_out);
  UA_NodeId_clear(&data_set_writer_id);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_pubsub_reader_group(
    cpkt_opcua_server *server, cpkt_opcua_node_id connection_id,
    const cpkt_opcua_pubsub_reader_group_options *options,
    cpkt_opcua_node_id *reader_group_id_out, char *node_id_buffer,
    size_t node_id_buffer_size, size_t *required_node_id_size_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_connection_id;
  UA_NodeId reader_group_id;
  UA_ReaderGroupConfig config;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (reader_group_id_out != NULL) {
    *reader_group_id_out = cpkt_opcua_node_id_null();
  }
  if (server == NULL || server->server == NULL || options == NULL ||
      reader_group_id_out == NULL || !cpkt_valid_node_id(connection_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_connection_id = cpkt_make_node_id(connection_id);
  UA_NodeId_init(&reader_group_id);
  memset(&config, 0, sizeof(config));
  config.name = cpkt_borrowed_string_or_null(options->name);
  config.enabled = options->enabled ? true : false;
  config.encodingMimeType = options->json_encoding ? UA_PUBSUB_ENCODING_JSON
                                                   : UA_PUBSUB_ENCODING_UADP;
  status = UA_Server_addReaderGroup(server->server, native_connection_id,
                                    &config, &reader_group_id);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_NodeId_clear(&reader_group_id);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_native_node_id_to_output(
      &reader_group_id, reader_group_id_out, node_id_buffer,
      node_id_buffer_size, required_node_id_size_out);
  UA_NodeId_clear(&reader_group_id);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_pubsub_data_set_reader(
    cpkt_opcua_server *server, cpkt_opcua_node_id reader_group_id,
    const cpkt_opcua_pubsub_data_set_reader_options *options,
    cpkt_opcua_node_id *data_set_reader_id_out, char *node_id_buffer,
    size_t node_id_buffer_size, size_t *required_node_id_size_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_reader_group_id;
  UA_NodeId data_set_reader_id;
  UA_DataSetReaderConfig config;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (data_set_reader_id_out != NULL) {
    *data_set_reader_id_out = cpkt_opcua_node_id_null();
  }
  if (server == NULL || server->server == NULL || options == NULL ||
      data_set_reader_id_out == NULL || !cpkt_valid_node_id(reader_group_id) ||
      options->publisher_id > CPKT_OPCUA_UINT32_MAX_VALUE) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_reader_group_id = cpkt_make_node_id(reader_group_id);
  UA_NodeId_init(&data_set_reader_id);
  memset(&config, 0, sizeof(config));
  config.name = cpkt_borrowed_string_or_null(options->name);
  config.enabled = options->enabled ? true : false;
  config.publisherId.idType = UA_PUBLISHERIDTYPE_UINT32;
  config.publisherId.id.uint32 = (UA_UInt32)options->publisher_id;
  config.writerGroupId = (UA_UInt16)options->writer_group_id;
  config.dataSetWriterId = (UA_UInt16)options->data_set_writer_id;
  config.messageReceiveTimeout = options->message_receive_timeout_ms;
  status = UA_Server_addDataSetReader(server->server, native_reader_group_id,
                                      &config, &data_set_reader_id);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_NodeId_clear(&data_set_reader_id);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_native_node_id_to_output(
      &data_set_reader_id, data_set_reader_id_out, node_id_buffer,
      node_id_buffer_size, required_node_id_size_out);
  UA_NodeId_clear(&data_set_reader_id);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_write_pubsub_configuration(
    cpkt_opcua_server *server, unsigned char *buffer, size_t buffer_size,
    size_t *required_size_out, cpkt_opcua_status *status_out) {
#ifdef UA_ENABLE_PUBSUB_FILE_CONFIG
  UA_ByteString bytes;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (server == NULL || server->server == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  bytes = UA_BYTESTRING_NULL;
  status =
      UA_Server_writePubSubConfigurationToByteString(server->server, &bytes);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_ByteString_clear(&bytes);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_ua_byte_string_to_buffer(&bytes, buffer, buffer_size,
                                              required_size_out);
  UA_ByteString_clear(&bytes);
  return result;
#else
  (void)server;
  (void)buffer;
  (void)buffer_size;
  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (status_out != NULL) {
    *status_out = 0;
  }
  return CPKT_OPCUA_ERR_UPSTREAM;
#endif
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_load_pubsub_configuration(
    cpkt_opcua_server *server, const unsigned char *buffer, size_t buffer_size,
    cpkt_opcua_status *status_out) {
#ifdef UA_ENABLE_PUBSUB_FILE_CONFIG
  UA_ByteString bytes;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL ||
      (buffer == NULL && buffer_size != 0)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  bytes = UA_BYTESTRING_NULL;
  bytes.data = (UA_Byte *)buffer;
  bytes.length = buffer_size;
  status = UA_Server_loadPubSubConfigFromByteString(server->server, bytes);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
#else
  (void)server;
  (void)buffer;
  (void)buffer_size;
  if (status_out != NULL) {
    *status_out = 0;
  }
  return CPKT_OPCUA_ERR_UPSTREAM;
#endif
}
