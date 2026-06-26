#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <arpa/inet.h>
#include <cmocka.h>
#include <limits.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cpkt/opcua.h>

#include <open62541/client.h>
#include <open62541/client_config_default.h>
#include <open62541/client_highlevel.h>
#include <open62541/client_highlevel_async.h>
#include <open62541/client_subscriptions.h>
#include <open62541/nodeids.h>
#include <open62541/plugin/accesscontrol.h>
#include <open62541/plugin/certificategroup.h>
#include <open62541/plugin/historydata/history_data_backend_memory.h>
#include <open62541/plugin/historydata/history_data_gathering_default.h>
#include <open62541/plugin/historydata/history_database_default.h>
#include <open62541/plugin/historydatabase.h>
#include <open62541/plugin/securitypolicy.h>
#include <open62541/pubsub.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>
#include <open62541/server_config_file_based.h>
#include <open62541/server_pubsub.h>
#include <open62541/util.h>

#include "opcua_c89_boundary_peer.h"

struct cpkt_facade_server_thread {
  cpkt_opcua_c89_server_peer *server;
  volatile int running;
};

struct cpkt_raw_facade_server_thread {
  cpkt_opcua_server *server;
  volatile int running;
};

struct cpkt_native_server_thread {
  UA_Server *server;
  volatile int running;
};

struct cpkt_native_event_seen {
  int saw_expected;
  unsigned long severity;
  size_t event_id_length;
  char message[64];
};

struct cpkt_native_value_seen {
  int saw_expected_variant;
  int saw_expected_data_value;
};

struct cpkt_history_values_seen {
  size_t count;
  long values[4];
  int saw_timestamp;
};

struct cpkt_pubsub_component_ids {
  cpkt_opcua_node_id connection_id;
  cpkt_opcua_node_id published_dataset_id;
  cpkt_opcua_node_id field_id;
  cpkt_opcua_node_id writer_group_id;
  cpkt_opcua_node_id data_set_writer_id;
  cpkt_opcua_node_id reader_group_id;
  cpkt_opcua_node_id data_set_reader_id;
};

static const unsigned char cpkt_native_guid_node_id[16] = {
    0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
    0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0};
static const unsigned char cpkt_native_byte_node_id[3] = {0xde, 0xad, 0xbe};
static const unsigned char cpkt_facade_guid_node_id[16] = {
    0x0f, 0xed, 0xcb, 0xa9, 0x87, 0x65, 0x43, 0x21,
    0x0f, 0xed, 0xcb, 0xa9, 0x87, 0x65, 0x43, 0x21};
static const unsigned char cpkt_facade_byte_node_id[4] = {0xca, 0xfe, 0xba, 0xbe};
static const unsigned char cpkt_native_bytes_value[4] = {0x01, 0x23, 0x45, 0x67};
static const unsigned char cpkt_facade_bytes_value[5] = {0x89, 0xab, 0xcd, 0xef, 0x01};
static const unsigned char cpkt_facade_bytes_updated[4] = {0x22, 0x44, 0x66, 0x88};

static UA_Guid cpkt_test_guid(const unsigned char guid[16]) {
  UA_Guid native_guid;

  native_guid.data1 =
      ((UA_UInt32)guid[0] << 24) |
      ((UA_UInt32)guid[1] << 16) |
      ((UA_UInt32)guid[2] << 8) |
      (UA_UInt32)guid[3];
  native_guid.data2 = (UA_UInt16)(((UA_UInt16)guid[4] << 8) | (UA_UInt16)guid[5]);
  native_guid.data3 = (UA_UInt16)(((UA_UInt16)guid[6] << 8) | (UA_UInt16)guid[7]);
  memcpy(native_guid.data4, guid + 8, 8);
  return native_guid;
}

static UA_NodeId cpkt_test_byte_string_node_id(
    unsigned short namespace_index,
    const unsigned char *identifier,
    size_t identifier_length) {
  UA_NodeId node_id;

  UA_NodeId_init(&node_id);
  node_id.namespaceIndex = (UA_UInt16)namespace_index;
  node_id.identifierType = UA_NODEIDTYPE_BYTESTRING;
  node_id.identifier.byteString.length = identifier_length;
  node_id.identifier.byteString.data = (UA_Byte *)identifier;
  return node_id;
}

static UA_NodeId cpkt_test_guid_node_id(unsigned short namespace_index, const unsigned char guid[16]) {
  UA_NodeId node_id;

  UA_NodeId_init(&node_id);
  node_id.namespaceIndex = (UA_UInt16)namespace_index;
  node_id.identifierType = UA_NODEIDTYPE_GUID;
  node_id.identifier.guid = cpkt_test_guid(guid);
  return node_id;
}

static int cpkt_test_event_field_matches(const UA_QualifiedName *key, const char *name) {
  const char *key_text;
  size_t key_length;
  size_t name_length;
  size_t offset;

  assert_non_null(key);
  assert_non_null(name);
  if (key->name.data == NULL) {
    return 0;
  }
  key_text = (const char *)key->name.data;
  key_length = key->name.length;
  name_length = strlen(name);
  if (key_length == name_length && memcmp(key_text, name, name_length) == 0) {
    return 1;
  }
  if (key_length > name_length && key_text[key_length - name_length - 1] == '/') {
    offset = key_length - name_length;
    return memcmp(key_text + offset, name, name_length) == 0;
  }
  return 0;
}

static void cpkt_native_event_callback(
    UA_Client *client,
    UA_UInt32 sub_id,
    void *sub_context,
    UA_UInt32 mon_id,
    void *mon_context,
    const UA_KeyValueMap event_fields) {
  struct cpkt_native_event_seen *seen;
  UA_KeyValuePair *pair;
  UA_ByteString *event_id;
  UA_LocalizedText *message;
  UA_UInt16 *severity;
  size_t copy_length;
  size_t i;

  (void)client;
  (void)sub_id;
  (void)sub_context;
  (void)mon_id;
  if (mon_context == NULL) {
    return;
  }
  seen = (struct cpkt_native_event_seen *)mon_context;
  for (i = 0; i < event_fields.mapSize; ++i) {
    pair = &event_fields.map[i];
    if (cpkt_test_event_field_matches(&pair->key, "EventId") &&
        UA_Variant_hasScalarType(&pair->value, &UA_TYPES[UA_TYPES_BYTESTRING])) {
      event_id = (UA_ByteString *)pair->value.data;
      seen->event_id_length = event_id->length;
    } else if (cpkt_test_event_field_matches(&pair->key, "Message") &&
               UA_Variant_hasScalarType(&pair->value, &UA_TYPES[UA_TYPES_LOCALIZEDTEXT])) {
      message = (UA_LocalizedText *)pair->value.data;
      copy_length = message->text.length;
      if (copy_length >= sizeof(seen->message)) {
        copy_length = sizeof(seen->message) - 1;
      }
      if (copy_length != 0) {
        memcpy(seen->message, message->text.data, copy_length);
      }
      seen->message[copy_length] = '\0';
    } else if (cpkt_test_event_field_matches(&pair->key, "Severity") &&
               UA_Variant_hasScalarType(&pair->value, &UA_TYPES[UA_TYPES_UINT16])) {
      severity = (UA_UInt16 *)pair->value.data;
      seen->severity = (unsigned long)*severity;
    }
  }
  if (seen->severity == 654UL && strcmp(seen->message, "facade server event") == 0 &&
      seen->event_id_length != 0) {
    seen->saw_expected = 1;
  }
}

static int cpkt_native_integer_variant_callback(const void *native_variant, void *user) {
  const UA_Variant *variant;
  struct cpkt_native_value_seen *seen;

  assert_non_null(native_variant);
  assert_non_null(user);
  variant = (const UA_Variant *)native_variant;
  seen = (struct cpkt_native_value_seen *)user;
  assert_true(UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_INT32]));
  assert_int_equal(*(UA_Int32 *)variant->data, 41);
  seen->saw_expected_variant = 1;
  return 0;
}

static int cpkt_native_integer_data_value_callback(const void *native_data_value, void *user) {
  const UA_DataValue *data_value;
  struct cpkt_native_value_seen *seen;

  assert_non_null(native_data_value);
  assert_non_null(user);
  data_value = (const UA_DataValue *)native_data_value;
  seen = (struct cpkt_native_value_seen *)user;
  assert_true(data_value->hasValue);
  assert_int_equal(data_value->status, UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasScalarType(&data_value->value, &UA_TYPES[UA_TYPES_INT32]));
  assert_int_equal(*(UA_Int32 *)data_value->value.data, 41);
  seen->saw_expected_data_value = 1;
  return 0;
}

static int cpkt_native_callback_returns_error(const void *native_value, void *user) {
  (void)native_value;
  (void)user;
  return 1;
}

static int cpkt_history_data_value_callback(
    const cpkt_opcua_data_value *data_value,
    int more_data_available,
    void *user) {
  struct cpkt_history_values_seen *seen;

  (void)more_data_available;
  assert_non_null(data_value);
  assert_non_null(user);
  seen = (struct cpkt_history_values_seen *)user;
  assert_true(seen->count < 4);
  assert_int_equal(data_value->has_value, 1);
  assert_int_equal(data_value->value.type, CPKT_OPCUA_VALUE_INTEGER);
  assert_int_equal(data_value->has_source_timestamp, 1);
  seen->values[seen->count] = data_value->value.integer_value;
  if (data_value->source_timestamp.high32 != 0 || data_value->source_timestamp.low32 != 0) {
    seen->saw_timestamp = 1;
  }
  ++seen->count;
  return 0;
}

static int cpkt_history_data_value_stops(
    const cpkt_opcua_data_value *data_value,
    int more_data_available,
    void *user) {
  (void)data_value;
  (void)more_data_available;
  (void)user;
  return 1;
}

static void cpkt_test_event_select_clause(
    UA_SimpleAttributeOperand *select_clause,
    UA_QualifiedName *browse_name,
    const char *field_name) {
  UA_SimpleAttributeOperand_init(select_clause);
  *browse_name = UA_QUALIFIEDNAME(0, (char *)field_name);
  select_clause->typeDefinitionId = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEEVENTTYPE);
  select_clause->browsePath = browse_name;
  select_clause->browsePathSize = 1;
  select_clause->attributeId = UA_ATTRIBUTEID_VALUE;
}

static void cpkt_test_browse_path_init(
    UA_BrowsePath *browse_path,
    UA_NodeId start_node_id,
    unsigned short namespace_index,
    const char **browse_names,
    size_t browse_name_count) {
  size_t i;

  assert_non_null(browse_path);
  assert_non_null(browse_names);
  assert_true(browse_name_count != 0);
  UA_BrowsePath_init(browse_path);
  assert_int_equal(UA_NodeId_copy(&start_node_id, &browse_path->startingNode), UA_STATUSCODE_GOOD);
  browse_path->relativePath.elements =
      (UA_RelativePathElement *)calloc(browse_name_count, sizeof(*browse_path->relativePath.elements));
  assert_non_null(browse_path->relativePath.elements);
  browse_path->relativePath.elementsSize = browse_name_count;
  for (i = 0; i < browse_name_count; ++i) {
    assert_non_null(browse_names[i]);
    UA_RelativePathElement_init(&browse_path->relativePath.elements[i]);
    browse_path->relativePath.elements[i].referenceTypeId =
        UA_NODEID_NUMERIC(0, UA_NS0ID_HIERARCHICALREFERENCES);
    browse_path->relativePath.elements[i].includeSubtypes = true;
    browse_path->relativePath.elements[i].isInverse = false;
    browse_path->relativePath.elements[i].targetName.namespaceIndex =
        (UA_UInt16)namespace_index;
    browse_path->relativePath.elements[i].targetName.name =
        UA_STRING_ALLOC((char *)browse_names[i]);
    assert_non_null(browse_path->relativePath.elements[i].targetName.name.data);
  }
}

static size_t cpkt_native_client_count_child_by_reference(
    UA_Client *client,
    UA_NodeId source_node_id,
    UA_NodeId reference_type_id,
    UA_NodeId target_node_id,
    const char *target_browse_name,
    UA_UInt32 node_class_mask) {
  UA_BrowseDescription browse_description;
  UA_BrowseResult browse_result;
  size_t count;
  size_t i;

  assert_non_null(client);
  assert_non_null(target_browse_name);
  UA_BrowseDescription_init(&browse_description);
  browse_description.nodeId = source_node_id;
  browse_description.browseDirection = UA_BROWSEDIRECTION_FORWARD;
  browse_description.referenceTypeId = reference_type_id;
  browse_description.includeSubtypes = true;
  browse_description.nodeClassMask = node_class_mask;
  browse_description.resultMask = UA_BROWSERESULTMASK_ALL;
  browse_result = UA_Client_browse(client, NULL, 0, &browse_description);
  assert_int_equal(browse_result.statusCode, UA_STATUSCODE_GOOD);
  count = 0;
  for (i = 0; i < browse_result.referencesSize; ++i) {
    if (UA_NodeId_equal(&browse_result.references[i].nodeId.nodeId, &target_node_id) &&
        browse_result.references[i].browseName.name.length == strlen(target_browse_name) &&
        memcmp(
            browse_result.references[i].browseName.name.data,
            target_browse_name,
            strlen(target_browse_name)) == 0) {
      ++count;
    }
  }
  UA_BrowseResult_clear(&browse_result);
  return count;
}

struct cpkt_native_reference_check {
  UA_NodeId source_node_id;
  UA_NodeId reference_type_id;
  UA_NodeId target_node_id;
  const char *target_browse_name;
  UA_UInt32 node_class_mask;
  size_t count;
};

static cpkt_opcua_status cpkt_native_client_counts_child_by_reference(void *native_client, void *user) {
  struct cpkt_native_reference_check *check;

  assert_non_null(native_client);
  assert_non_null(user);
  check = (struct cpkt_native_reference_check *)user;
  check->count = cpkt_native_client_count_child_by_reference(
      (UA_Client *)native_client,
      check->source_node_id,
      check->reference_type_id,
      check->target_node_id,
      check->target_browse_name,
      check->node_class_mask);
  return UA_STATUSCODE_GOOD;
}

static void cpkt_assert_client_reference_count(
    cpkt_opcua_client *client,
    unsigned short source_namespace,
    unsigned long source_id,
    unsigned long reference_type_id,
    unsigned short target_namespace,
    unsigned long target_id,
    UA_UInt32 node_class_mask,
    const char *target_browse_name,
    size_t expected_count) {
  struct cpkt_native_reference_check check;

  check.source_node_id = UA_NODEID_NUMERIC((UA_UInt16)source_namespace, (UA_UInt32)source_id);
  check.reference_type_id = UA_NODEID_NUMERIC(0, (UA_UInt32)reference_type_id);
  check.target_node_id = UA_NODEID_NUMERIC((UA_UInt16)target_namespace, (UA_UInt32)target_id);
  check.target_browse_name = target_browse_name;
  check.node_class_mask = node_class_mask;
  check.count = 0;
  assert_int_equal(
      cpkt_opcua_client_native(client, cpkt_native_client_counts_child_by_reference, &check),
      CPKT_OPCUA_OK);
  assert_int_equal(check.count, expected_count);
}

static void cpkt_native_client_read_method_arguments(
    UA_Client *client,
    UA_NodeId method_node_id,
    const char *property_name,
    UA_Variant *variant_out) {
  const char *browse_names[1];
  UA_BrowsePath browse_path;
  UA_BrowsePathResult browse_path_result;

  assert_non_null(client);
  assert_non_null(property_name);
  assert_non_null(variant_out);
  browse_names[0] = property_name;
  cpkt_test_browse_path_init(&browse_path, method_node_id, 0, browse_names, 1);
  browse_path_result = UA_Client_translateBrowsePathToNodeIds(client, &browse_path);
  assert_int_equal(browse_path_result.statusCode, UA_STATUSCODE_GOOD);
  assert_int_equal(browse_path_result.targetsSize, 1);
  assert_int_equal(browse_path_result.targets[0].targetId.serverIndex, 0);
  assert_int_equal(browse_path_result.targets[0].targetId.namespaceUri.length, 0);
  assert_int_equal(browse_path_result.targets[0].remainingPathIndex, (UA_UInt32)UINT_MAX);
  UA_Variant_init(variant_out);
  assert_int_equal(
      UA_Client_readValueAttribute(
          client,
          browse_path_result.targets[0].targetId.nodeId,
          variant_out),
      UA_STATUSCODE_GOOD);
  UA_BrowsePathResult_clear(&browse_path_result);
  UA_BrowsePath_clear(&browse_path);
}

static UA_StatusCode cpkt_native_increment_method(
    UA_Server *server,
    const UA_NodeId *session_id,
    void *session_context,
    const UA_NodeId *method_id,
    void *method_context,
    const UA_NodeId *object_id,
    void *object_context,
    size_t input_size,
    const UA_Variant *input,
    size_t output_size,
    UA_Variant *output) {
  UA_Int32 result;
  UA_LocalizedText message;

  (void)session_id;
  (void)session_context;
  (void)method_id;
  (void)method_context;
  (void)object_context;
  if (input_size != 1 || output_size != 1 ||
      !UA_Variant_hasScalarType(&input[0], &UA_TYPES[UA_TYPES_INT32])) {
    return UA_STATUSCODE_BADTYPEMISMATCH;
  }
  message = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native method event");
  (void)UA_Server_createEvent(
      server,
      *object_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEEVENTTYPE),
      321,
      message,
      NULL,
      NULL,
      NULL);
  result = *(UA_Int32 *)input[0].data + 10;
  UA_Variant_setScalarCopy(&output[0], &result, &UA_TYPES[UA_TYPES_INT32]);
  return output[0].data != NULL ? UA_STATUSCODE_GOOD : UA_STATUSCODE_BADOUTOFMEMORY;
}

static UA_StatusCode cpkt_native_multi_method(
    UA_Server *server,
    const UA_NodeId *session_id,
    void *session_context,
    const UA_NodeId *method_id,
    void *method_context,
    const UA_NodeId *object_id,
    void *object_context,
    size_t input_size,
    const UA_Variant *input,
    size_t output_size,
    UA_Variant *output) {
  UA_Int32 doubled;
  UA_Double as_double;

  (void)server;
  (void)session_id;
  (void)session_context;
  (void)method_id;
  (void)method_context;
  (void)object_id;
  (void)object_context;
  if (input_size != 1 || output_size != 2 ||
      !UA_Variant_hasScalarType(&input[0], &UA_TYPES[UA_TYPES_INT32])) {
    return UA_STATUSCODE_BADTYPEMISMATCH;
  }
  doubled = *(UA_Int32 *)input[0].data * 2;
  as_double = (UA_Double)*(UA_Int32 *)input[0].data + 0.5;
  UA_Variant_setScalarCopy(&output[0], &doubled, &UA_TYPES[UA_TYPES_INT32]);
  UA_Variant_setScalarCopy(&output[1], &as_double, &UA_TYPES[UA_TYPES_DOUBLE]);
  return output[0].data != NULL && output[1].data != NULL ?
      UA_STATUSCODE_GOOD :
      UA_STATUSCODE_BADOUTOFMEMORY;
}

static void cpkt_test_method_argument_init(UA_Argument *argument, const char *name, UA_NodeId data_type) {
  UA_Argument_init(argument);
  argument->description = UA_LOCALIZEDTEXT_ALLOC((char *)"en-US", (char *)name);
  argument->name = UA_STRING_ALLOC((char *)name);
  argument->dataType = data_type;
  argument->valueRank = UA_VALUERANK_SCALAR;
  assert_non_null(argument->description.text.data);
  assert_non_null(argument->name.data);
}

static unsigned short cpkt_test_free_port(void) {
  int fd;
  struct sockaddr_in addr;
  socklen_t addr_len;
  unsigned short port;

  fd = socket(AF_INET, SOCK_STREAM, 0);
  assert_true(fd >= 0);
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  assert_int_equal(bind(fd, (struct sockaddr *)&addr, sizeof(addr)), 0);
  addr_len = (socklen_t)sizeof(addr);
  assert_int_equal(getsockname(fd, (struct sockaddr *)&addr, &addr_len), 0);
  port = ntohs(addr.sin_port);
  close(fd);
  return port;
}

static void *cpkt_facade_server_loop(void *arg) {
  struct cpkt_facade_server_thread *thread = (struct cpkt_facade_server_thread *)arg;
  while (thread->running) {
    (void)cpkt_opcua_c89_server_peer_iterate(thread->server);
  }
  return NULL;
}

static void *cpkt_raw_facade_server_loop(void *arg) {
  struct cpkt_raw_facade_server_thread *thread = (struct cpkt_raw_facade_server_thread *)arg;
  unsigned short wait_ms;

  while (thread->running) {
    wait_ms = 0;
    (void)cpkt_opcua_server_iterate(thread->server, 1, &wait_ms);
  }
  return NULL;
}

static void *cpkt_native_server_loop(void *arg) {
  struct cpkt_native_server_thread *thread = (struct cpkt_native_server_thread *)arg;
  while (thread->running) {
    (void)UA_Server_run_iterate(thread->server, true);
  }
  return NULL;
}

static UA_StatusCode cpkt_native_client_connect_username_with_retry(
    UA_Client *client,
    const char *endpoint,
    const char *username,
    const char *password) {
  int attempt;
  UA_StatusCode status;

  status = UA_STATUSCODE_BADCONNECTIONCLOSED;
  for (attempt = 0; attempt < 100; ++attempt) {
    status = UA_Client_connectUsername(client, endpoint, username, password);
    if (status == UA_STATUSCODE_GOOD) {
      return status;
    }
    usleep(10000);
  }
  return status;
}

static cpkt_opcua_result cpkt_facade_client_connect_username_with_retry(
    const char *endpoint,
    const char *username,
    const char *password,
    cpkt_opcua_client **client_out,
    cpkt_opcua_status *status_out) {
  cpkt_opcua_client *client;
  cpkt_opcua_result result;
  int attempt;

  if (client_out != NULL) {
    *client_out = NULL;
  }
  if (endpoint == NULL || username == NULL || password == NULL || client_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = CPKT_OPCUA_ERR_UPSTREAM;
  for (attempt = 0; attempt < 100; ++attempt) {
    client = NULL;
    assert_int_equal(cpkt_opcua_client_new(&client), CPKT_OPCUA_OK);
    result = cpkt_opcua_client_connect_username(client, endpoint, username, password, status_out);
    if (result == CPKT_OPCUA_OK) {
      *client_out = client;
      return CPKT_OPCUA_OK;
    }
    cpkt_opcua_client_free(client);
    usleep(10000);
  }
  return result;
}

static int cpkt_browse_stop_callback(const cpkt_opcua_browse_entry *entry, void *user) {
  (void)entry;
  (void)user;
  return 1;
}

static cpkt_opcua_result cpkt_bad_method_callback(
    const cpkt_opcua_value *inputs,
    size_t input_count,
    cpkt_opcua_value *output,
    void *user) {
  (void)inputs;
  (void)input_count;
  (void)output;
  (void)user;
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_bad_method_many_callback(
    const cpkt_opcua_value *inputs,
    size_t input_count,
    cpkt_opcua_value *outputs,
    size_t output_count,
    void *user) {
  (void)inputs;
  (void)input_count;
  (void)outputs;
  (void)output_count;
  (void)user;
  return CPKT_OPCUA_OK;
}

static void cpkt_bad_event_callback(
    cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_monitored_item_id monitored_item_id,
    const cpkt_opcua_event *event,
    cpkt_opcua_status status,
    void *user) {
  (void)subscription_id;
  (void)monitored_item_id;
  (void)event;
  (void)status;
  (void)user;
}

static void cpkt_bad_data_change_callback(
    cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_monitored_item_id monitored_item_id,
    const cpkt_opcua_value *value,
    cpkt_opcua_status status,
    void *user) {
  (void)subscription_id;
  (void)monitored_item_id;
  (void)value;
  (void)status;
  (void)user;
}

static void cpkt_bad_event_fields_callback(
    cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_monitored_item_id monitored_item_id,
    const cpkt_opcua_event_field *fields,
    size_t field_count,
    cpkt_opcua_status status,
    void *user) {
  (void)subscription_id;
  (void)monitored_item_id;
  (void)fields;
  (void)field_count;
  (void)status;
  (void)user;
}

static int cpkt_bad_string_array_callback(size_t index, const char *data, size_t length, void *user) {
  (void)index;
  (void)data;
  (void)length;
  (void)user;
  return 1;
}

static int cpkt_bad_byte_string_array_callback(
    size_t index,
    const unsigned char *data,
    size_t length,
    void *user) {
  (void)index;
  (void)data;
  (void)length;
  (void)user;
  return 1;
}

struct cpkt_async_seen {
  int done_count;
  int browse_seen;
  int status_ok;
  int node_ok;
  cpkt_opcua_result result;
  cpkt_opcua_status status;
  long integer_value;
  double double_value;
  cpkt_opcua_request_id request_id;
};

static void cpkt_async_value_callback(
    cpkt_opcua_request_id request_id,
    cpkt_opcua_result result,
    const cpkt_opcua_value *value,
    cpkt_opcua_status status,
    void *user) {
  struct cpkt_async_seen *seen;

  seen = (struct cpkt_async_seen *)user;
  seen->done_count += 1;
  seen->request_id = request_id;
  seen->result = result;
  seen->status = status;
  if (result == CPKT_OPCUA_OK && value != NULL && value->type == CPKT_OPCUA_VALUE_INTEGER) {
    seen->integer_value = value->integer_value;
  }
}

static void cpkt_async_status_callback(
    cpkt_opcua_request_id request_id,
    cpkt_opcua_result result,
    cpkt_opcua_status status,
    void *user) {
  struct cpkt_async_seen *seen;

  seen = (struct cpkt_async_seen *)user;
  seen->done_count += 1;
  seen->request_id = request_id;
  seen->result = result;
  seen->status = status;
  seen->status_ok = result == CPKT_OPCUA_OK ? 1 : 0;
}

static int cpkt_async_browse_entry_callback(const cpkt_opcua_browse_entry *entry, void *user) {
  struct cpkt_async_seen *seen;

  seen = (struct cpkt_async_seen *)user;
  if (entry != NULL && entry->browse_name != NULL &&
      (strcmp(entry->browse_name, "asyncClientObject") == 0 ||
       strcmp(entry->browse_name, "nativeInteger") == 0)) {
    seen->browse_seen += 1;
  }
  return 0;
}

static void cpkt_async_browse_done_callback(
    cpkt_opcua_request_id request_id,
    cpkt_opcua_result result,
    cpkt_opcua_status status,
    void *user) {
  struct cpkt_async_seen *seen;

  seen = (struct cpkt_async_seen *)user;
  seen->done_count += 1;
  seen->request_id = request_id;
  seen->result = result;
  seen->status = status;
}

static void cpkt_async_call_callback(
    cpkt_opcua_request_id request_id,
    cpkt_opcua_result result,
    const cpkt_opcua_value *outputs,
    size_t output_count,
    cpkt_opcua_status status,
    void *user) {
  struct cpkt_async_seen *seen;

  seen = (struct cpkt_async_seen *)user;
  seen->done_count += 1;
  seen->request_id = request_id;
  seen->result = result;
  seen->status = status;
  if (result == CPKT_OPCUA_OK && outputs != NULL && output_count == 2) {
    if (outputs[0].type == CPKT_OPCUA_VALUE_INTEGER) {
      seen->integer_value = outputs[0].integer_value;
    }
    if (outputs[1].type == CPKT_OPCUA_VALUE_DOUBLE) {
      seen->double_value = outputs[1].double_value;
    }
  }
}

static void cpkt_async_node_callback(
    cpkt_opcua_request_id request_id,
    cpkt_opcua_result result,
    const cpkt_opcua_node_id *node_id,
    cpkt_opcua_status status,
    void *user) {
  struct cpkt_async_seen *seen;

  seen = (struct cpkt_async_seen *)user;
  seen->done_count += 1;
  seen->request_id = request_id;
  seen->result = result;
  seen->status = status;
  if (result == CPKT_OPCUA_OK && node_id != NULL &&
      node_id->identifier_type == CPKT_OPCUA_NODE_ID_NUMERIC) {
    seen->node_ok = 1;
  }
}

static void cpkt_wait_for_async(cpkt_opcua_client *client, struct cpkt_async_seen *seen) {
  cpkt_opcua_status status;
  int attempt;

  for (attempt = 0; attempt < 100 && seen->done_count == 0; ++attempt) {
    (void)cpkt_opcua_client_run_iterate(client, 50, &status);
  }
  assert_true(seen->done_count != 0);
}

static void cpkt_facade_client_reads_and_writes_native_server(void **state) {
  unsigned short port;
  char endpoint[80];
  UA_Server *server;
  UA_ServerConfig *config;
  UA_StatusCode native_status;
  UA_VariableAttributes attr;
  UA_Int32 native_value;
  UA_Int32 native_array_values[3];
  UA_Boolean native_bool_array_values[2];
  UA_Double native_double_array_values[2];
  UA_String native_string_array_values[2];
  UA_ByteString native_bytes_array_values[2];
  unsigned char native_bytes_array_0[2];
  unsigned char native_bytes_array_1[3];
  UA_Guid native_guid_value;
  UA_Guid native_guid_array_values[2];
  UA_StatusCode native_status_value;
  UA_StatusCode native_status_array_values[2];
  UA_UInt64 native_uint64_value;
  UA_UInt64 native_uint64_array_values[2];
  UA_DateTime native_datetime_value;
  UA_DateTime native_datetime_array_values[3];
  UA_QualifiedName native_qualified_name_value;
  UA_QualifiedName native_qualified_name_array_values[2];
  UA_LocalizedText native_localized_text_value;
  UA_LocalizedText native_localized_text_array_values[2];
  UA_UInt32 native_array_dimensions[1];
  UA_Boolean native_bool;
  UA_String native_string;
  UA_ByteString native_bytes;
  UA_ObjectAttributes object_attr;
  UA_ReferenceTypeAttributes reference_type_attr;
  UA_ViewAttributes view_attr;
  UA_HistoryDataGathering history_gathering;
  UA_HistorizingNodeIdSettings history_setting;
  UA_MethodAttributes method_attr;
  UA_Argument native_input_argument;
  UA_Argument native_output_argument;
  UA_Argument native_multi_output_arguments[2];
  UA_NodeId native_node_id;
  struct cpkt_native_server_thread thread;
  pthread_t thread_id;
  cpkt_opcua_client *facade_client;
  struct cpkt_native_value_seen native_value_seen;
  struct cpkt_history_values_seen history_seen;
  struct cpkt_async_seen async_seen;
  cpkt_opcua_value history_value;
  cpkt_opcua_value async_value;
  cpkt_opcua_value async_inputs[1];
  cpkt_opcua_value async_outputs[2];
  cpkt_opcua_node_id async_node_out;
  cpkt_opcua_request_id async_request_id;
  cpkt_opcua_status async_status;
  cpkt_opcua_datetime history_start;
  cpkt_opcua_datetime history_end;
  int c89_line;

  (void)state;

  port = cpkt_test_free_port();
  snprintf(endpoint, sizeof(endpoint), "opc.tcp://127.0.0.1:%u", (unsigned)port);
  server = UA_Server_new();
  assert_non_null(server);
  config = UA_Server_getConfig(server);
  assert_non_null(config);
  assert_int_equal(UA_ServerConfig_setMinimal(config, port, NULL), UA_STATUSCODE_GOOD);
  history_gathering = UA_HistoryDataGathering_Default(1);
  config->historyDatabase = UA_HistoryDatabase_default(history_gathering);
  assert_true(UA_Server_addNamespace(server, CPKT_OPCUA_NATIVE_NAMESPACE_URI) != 0);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.writeMask = UA_WRITEMASK_ACCESSLEVEL | UA_WRITEMASK_HISTORIZING | UA_WRITEMASK_DISPLAYNAME |
                   UA_WRITEMASK_DESCRIPTION;
  attr.userWriteMask = UA_WRITEMASK_ACCESSLEVEL | UA_WRITEMASK_HISTORIZING | UA_WRITEMASK_DISPLAYNAME |
                       UA_WRITEMASK_DESCRIPTION;
  attr.dataType = UA_TYPES[UA_TYPES_INT32].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native integer");
  attr.valueRank = UA_VALUERANK_SCALAR;
  attr.historizing = false;
  native_value = 41;
  UA_Variant_setScalar(&attr.value, &native_value, &UA_TYPES[UA_TYPES_INT32]);
  native_status = UA_Server_addVariableNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_INT_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeInteger"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE |
                     UA_ACCESSLEVELMASK_HISTORYREAD | UA_ACCESSLEVELMASK_HISTORYWRITE |
                     UA_ACCESSLEVELMASK_STATUSWRITE | UA_ACCESSLEVELMASK_TIMESTAMPWRITE;
  attr.dataType = UA_TYPES[UA_TYPES_INT32].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native history value");
  attr.valueRank = UA_VALUERANK_SCALAR;
  attr.historizing = true;
  native_value = 100;
  UA_Variant_setScalar(&attr.value, &native_value, &UA_TYPES[UA_TYPES_INT32]);
  native_status = UA_Server_addVariableNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_HISTORY_VALUE_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeHistoryValue"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);
  history_setting.historizingBackend = UA_HistoryDataBackend_Memory(1, 16);
  history_setting.maxHistoryDataResponseSize = 16;
  history_setting.historizingUpdateStrategy = UA_HISTORIZINGUPDATESTRATEGY_VALUESET;
  history_setting.pollingInterval = 0;
  history_setting.userContext = NULL;
  native_node_id = UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_HISTORY_VALUE_ID);
  assert_int_equal(
      history_gathering.registerNodeId(server, history_gathering.context, &native_node_id, history_setting),
      UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.writeMask = UA_WRITEMASK_VALUERANK | UA_WRITEMASK_ARRAYDIMENSIONS | UA_WRITEMASK_DISPLAYNAME;
  attr.userWriteMask = attr.writeMask;
  attr.dataType = UA_TYPES[UA_TYPES_INT32].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native array dimensions");
  attr.valueRank = 1;
  native_array_dimensions[0] = 3;
  attr.arrayDimensions = native_array_dimensions;
  attr.arrayDimensionsSize = 1;
  native_array_values[0] = 1;
  native_array_values[1] = 2;
  native_array_values[2] = 3;
  UA_Variant_setArray(&attr.value, native_array_values, 3, &UA_TYPES[UA_TYPES_INT32]);
  native_status = UA_Server_addVariableNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_ARRAY_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeArrayDimensions"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.dataType = UA_TYPES[UA_TYPES_INT32].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native async integer");
  attr.valueRank = UA_VALUERANK_SCALAR;
  native_value = 100;
  UA_Variant_setScalar(&attr.value, &native_value, &UA_TYPES[UA_TYPES_INT32]);
  native_status = UA_Server_addVariableNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, 7799),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeAsyncInteger"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.dataType = UA_TYPES[UA_TYPES_BOOLEAN].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native boolean array");
  attr.valueRank = 1;
  native_array_dimensions[0] = 2;
  attr.arrayDimensions = native_array_dimensions;
  attr.arrayDimensionsSize = 1;
  native_bool_array_values[0] = true;
  native_bool_array_values[1] = false;
  UA_Variant_setArray(&attr.value, native_bool_array_values, 2, &UA_TYPES[UA_TYPES_BOOLEAN]);
  native_status = UA_Server_addVariableNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_BOOL_ARRAY_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeBooleanArray"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.dataType = UA_TYPES[UA_TYPES_DOUBLE].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native double array");
  attr.valueRank = 1;
  native_array_dimensions[0] = 2;
  attr.arrayDimensions = native_array_dimensions;
  attr.arrayDimensionsSize = 1;
  native_double_array_values[0] = 1.25;
  native_double_array_values[1] = 2.5;
  UA_Variant_setArray(&attr.value, native_double_array_values, 2, &UA_TYPES[UA_TYPES_DOUBLE]);
  native_status = UA_Server_addVariableNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_DOUBLE_ARRAY_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeDoubleArray"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.dataType = UA_TYPES[UA_TYPES_STRING].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native string array");
  attr.valueRank = 1;
  native_array_dimensions[0] = 2;
  attr.arrayDimensions = native_array_dimensions;
  attr.arrayDimensionsSize = 1;
  native_string_array_values[0] = UA_STRING((char *)"alpha");
  native_string_array_values[1] = UA_STRING((char *)"beta");
  UA_Variant_setArray(&attr.value, native_string_array_values, 2, &UA_TYPES[UA_TYPES_STRING]);
  native_status = UA_Server_addVariableNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_STRING_ARRAY_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeStringArray"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.dataType = UA_TYPES[UA_TYPES_BYTESTRING].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native byte string array");
  attr.valueRank = 1;
  native_array_dimensions[0] = 2;
  attr.arrayDimensions = native_array_dimensions;
  attr.arrayDimensionsSize = 1;
  native_bytes_array_0[0] = 0x10U;
  native_bytes_array_0[1] = 0x11U;
  native_bytes_array_1[0] = 0x20U;
  native_bytes_array_1[1] = 0x21U;
  native_bytes_array_1[2] = 0x22U;
  native_bytes_array_values[0] = UA_BYTESTRING_NULL;
  native_bytes_array_values[0].data = native_bytes_array_0;
  native_bytes_array_values[0].length = sizeof(native_bytes_array_0);
  native_bytes_array_values[1] = UA_BYTESTRING_NULL;
  native_bytes_array_values[1].data = native_bytes_array_1;
  native_bytes_array_values[1].length = sizeof(native_bytes_array_1);
  UA_Variant_setArray(&attr.value, native_bytes_array_values, 2, &UA_TYPES[UA_TYPES_BYTESTRING]);
  native_status = UA_Server_addVariableNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_BYTES_ARRAY_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeByteStringArray"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.dataType = UA_TYPES[UA_TYPES_GUID].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native guid value");
  attr.valueRank = UA_VALUERANK_SCALAR;
  native_guid_value = cpkt_test_guid(cpkt_native_guid_node_id);
  UA_Variant_setScalar(&attr.value, &native_guid_value, &UA_TYPES[UA_TYPES_GUID]);
  native_status = UA_Server_addVariableNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_GUID_VALUE_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeGuidValue"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.dataType = UA_TYPES[UA_TYPES_GUID].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native guid array");
  attr.valueRank = 1;
  native_array_dimensions[0] = 2;
  attr.arrayDimensions = native_array_dimensions;
  attr.arrayDimensionsSize = 1;
  native_guid_array_values[0] = cpkt_test_guid(cpkt_native_guid_node_id);
  native_guid_array_values[1] = cpkt_test_guid(cpkt_facade_guid_node_id);
  UA_Variant_setArray(&attr.value, native_guid_array_values, 2, &UA_TYPES[UA_TYPES_GUID]);
  native_status = UA_Server_addVariableNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_GUID_ARRAY_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeGuidArray"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.dataType = UA_TYPES[UA_TYPES_STATUSCODE].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native status value");
  attr.valueRank = UA_VALUERANK_SCALAR;
  native_status_value = UA_STATUSCODE_BADNOTFOUND;
  UA_Variant_setScalar(&attr.value, &native_status_value, &UA_TYPES[UA_TYPES_STATUSCODE]);
  native_status = UA_Server_addVariableNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_STATUS_VALUE_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeStatusValue"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.dataType = UA_TYPES[UA_TYPES_STATUSCODE].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native status array");
  attr.valueRank = 1;
  native_array_dimensions[0] = 2;
  attr.arrayDimensions = native_array_dimensions;
  attr.arrayDimensionsSize = 1;
  native_status_array_values[0] = UA_STATUSCODE_GOOD;
  native_status_array_values[1] = UA_STATUSCODE_BADNOTFOUND;
  UA_Variant_setArray(&attr.value, native_status_array_values, 2, &UA_TYPES[UA_TYPES_STATUSCODE]);
  native_status = UA_Server_addVariableNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_STATUS_ARRAY_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeStatusArray"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.dataType = UA_TYPES[UA_TYPES_UINT64].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native uint64 value");
  attr.valueRank = UA_VALUERANK_SCALAR;
  native_uint64_value = ((UA_UInt64)0x12345678U << 32) | (UA_UInt64)0x9abcdef0U;
  UA_Variant_setScalar(&attr.value, &native_uint64_value, &UA_TYPES[UA_TYPES_UINT64]);
  native_status = UA_Server_addVariableNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_UINT64_VALUE_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeUInt64Value"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.dataType = UA_TYPES[UA_TYPES_UINT64].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native uint64 array");
  attr.valueRank = 1;
  native_array_dimensions[0] = 2;
  attr.arrayDimensions = native_array_dimensions;
  attr.arrayDimensionsSize = 1;
  native_uint64_array_values[0] = ((UA_UInt64)0x11111111U << 32) | (UA_UInt64)0x22222222U;
  native_uint64_array_values[1] = ((UA_UInt64)0x33333333U << 32) | (UA_UInt64)0x44444444U;
  UA_Variant_setArray(&attr.value, native_uint64_array_values, 2, &UA_TYPES[UA_TYPES_UINT64]);
  native_status = UA_Server_addVariableNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_UINT64_ARRAY_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeUInt64Array"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.dataType = UA_TYPES[UA_TYPES_DATETIME].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native datetime value");
  attr.valueRank = UA_VALUERANK_SCALAR;
  native_datetime_value = (UA_DateTime)(((UA_Int64)0x01234567U << 32) | (UA_Int64)0x89abcdefU);
  UA_Variant_setScalar(&attr.value, &native_datetime_value, &UA_TYPES[UA_TYPES_DATETIME]);
  native_status = UA_Server_addVariableNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_DATETIME_VALUE_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeDateTimeValue"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.dataType = UA_TYPES[UA_TYPES_DATETIME].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native datetime array");
  attr.valueRank = 1;
  native_array_dimensions[0] = 3;
  attr.arrayDimensions = native_array_dimensions;
  attr.arrayDimensionsSize = 1;
  native_datetime_array_values[0] = (UA_DateTime)(((UA_Int64)0x11111111U << 32) | (UA_Int64)0x22222222U);
  native_datetime_array_values[1] = (UA_DateTime)((UA_Int64)-2 * ((UA_Int64)0xffffffffU + 1) + 0x33333333);
  native_datetime_array_values[2] =
      (UA_DateTime)(((UA_Int64)(-2147483647L - 1L) * ((UA_Int64)0xffffffffU + 1)) + 0x44444444);
  UA_Variant_setArray(&attr.value, native_datetime_array_values, 3, &UA_TYPES[UA_TYPES_DATETIME]);
  native_status = UA_Server_addVariableNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_DATETIME_ARRAY_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeDateTimeArray"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.dataType = UA_TYPES[UA_TYPES_QUALIFIEDNAME].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native qualified name value");
  attr.valueRank = UA_VALUERANK_SCALAR;
  native_qualified_name_value = UA_QUALIFIEDNAME(CPKT_OPCUA_TEST_NS, (char *)"nativeQualified");
  UA_Variant_setScalar(&attr.value, &native_qualified_name_value, &UA_TYPES[UA_TYPES_QUALIFIEDNAME]);
  native_status = UA_Server_addVariableNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_QUALIFIED_NAME_VALUE_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeQualifiedNameValue"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.dataType = UA_TYPES[UA_TYPES_QUALIFIEDNAME].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native qualified name array");
  attr.valueRank = 1;
  native_array_dimensions[0] = 2;
  attr.arrayDimensions = native_array_dimensions;
  attr.arrayDimensionsSize = 1;
  native_qualified_name_array_values[0] = UA_QUALIFIEDNAME(CPKT_OPCUA_TEST_NS, (char *)"alphaName");
  native_qualified_name_array_values[1] = UA_QUALIFIEDNAME(CPKT_OPCUA_TEST_NS, (char *)"betaName");
  UA_Variant_setArray(
      &attr.value,
      native_qualified_name_array_values,
      2,
      &UA_TYPES[UA_TYPES_QUALIFIEDNAME]);
  native_status = UA_Server_addVariableNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_QUALIFIED_NAME_ARRAY_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeQualifiedNameArray"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.dataType = UA_TYPES[UA_TYPES_LOCALIZEDTEXT].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native localized text value");
  attr.valueRank = UA_VALUERANK_SCALAR;
  native_localized_text_value = UA_LOCALIZEDTEXT((char *)"sv-SE", (char *)"infoddt text");
  UA_Variant_setScalar(&attr.value, &native_localized_text_value, &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]);
  native_status = UA_Server_addVariableNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_LOCALIZED_TEXT_VALUE_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeLocalizedTextValue"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.dataType = UA_TYPES[UA_TYPES_LOCALIZEDTEXT].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native localized text array");
  attr.valueRank = 1;
  native_array_dimensions[0] = 2;
  attr.arrayDimensions = native_array_dimensions;
  attr.arrayDimensionsSize = 1;
  native_localized_text_array_values[0] = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"alpha text");
  native_localized_text_array_values[1] = UA_LOCALIZEDTEXT((char *)"sv-SE", (char *)"beta text");
  UA_Variant_setArray(
      &attr.value,
      native_localized_text_array_values,
      2,
      &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]);
  native_status = UA_Server_addVariableNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_LOCALIZED_TEXT_ARRAY_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeLocalizedTextArray"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  reference_type_attr = UA_ReferenceTypeAttributes_default;
  reference_type_attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"Native Related To");
  reference_type_attr.inverseName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"Native Related From");
  reference_type_attr.writeMask =
      UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_INVERSENAME | UA_WRITEMASK_ISABSTRACT;
  reference_type_attr.userWriteMask =
      UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_INVERSENAME | UA_WRITEMASK_ISABSTRACT;
  reference_type_attr.isAbstract = false;
  reference_type_attr.symmetric = false;
  native_status = UA_Server_addReferenceTypeNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_REFERENCE_TYPE_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_REFERENCES),
      UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
      UA_QUALIFIEDNAME(1, (char *)"nativeRelatedTo"),
      reference_type_attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  view_attr = UA_ViewAttributes_default;
  view_attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"Native View");
  view_attr.containsNoLoops = true;
  native_status = UA_Server_addViewNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_VIEW_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_VIEWSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeView"),
      view_attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.dataType = UA_TYPES[UA_TYPES_BOOLEAN].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native boolean");
  attr.valueRank = UA_VALUERANK_SCALAR;
  native_bool = true;
  UA_Variant_setScalar(&attr.value, &native_bool, &UA_TYPES[UA_TYPES_BOOLEAN]);
  native_status = UA_Server_addVariableNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_BOOL_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeBoolean"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.dataType = UA_TYPES[UA_TYPES_STRING].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native string");
  attr.valueRank = UA_VALUERANK_SCALAR;
  native_string = UA_STRING((char *)"native");
  UA_Variant_setScalar(&attr.value, &native_string, &UA_TYPES[UA_TYPES_STRING]);
  native_status = UA_Server_addVariableNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_STRING_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeString"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.dataType = UA_TYPES[UA_TYPES_STRING].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native string node");
  attr.valueRank = UA_VALUERANK_SCALAR;
  native_string = UA_STRING((char *)"native string id");
  UA_Variant_setScalar(&attr.value, &native_string, &UA_TYPES[UA_TYPES_STRING]);
  native_status = UA_Server_addVariableNode(
      server,
      UA_NODEID_STRING(CPKT_OPCUA_TEST_NS, (char *)CPKT_OPCUA_NATIVE_STRING_NODE_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeStringNode"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.dataType = UA_TYPES[UA_TYPES_BYTESTRING].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native bytes");
  attr.valueRank = UA_VALUERANK_SCALAR;
  native_bytes = UA_BYTESTRING_NULL;
  native_bytes.data = (UA_Byte *)cpkt_native_bytes_value;
  native_bytes.length = sizeof(cpkt_native_bytes_value);
  UA_Variant_setScalar(&attr.value, &native_bytes, &UA_TYPES[UA_TYPES_BYTESTRING]);
  native_status = UA_Server_addVariableNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_BYTES_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeBytes"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.dataType = UA_TYPES[UA_TYPES_INT32].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native guid node");
  attr.valueRank = UA_VALUERANK_SCALAR;
  native_value = 314;
  UA_Variant_setScalar(&attr.value, &native_value, &UA_TYPES[UA_TYPES_INT32]);
  native_node_id = cpkt_test_guid_node_id(CPKT_OPCUA_TEST_NS, cpkt_native_guid_node_id);
  native_status = UA_Server_addVariableNode(
      server,
      native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeGuidNode"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.dataType = UA_TYPES[UA_TYPES_INT32].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native byte node");
  attr.valueRank = UA_VALUERANK_SCALAR;
  native_value = 315;
  UA_Variant_setScalar(&attr.value, &native_value, &UA_TYPES[UA_TYPES_INT32]);
  native_node_id =
      cpkt_test_byte_string_node_id(CPKT_OPCUA_TEST_NS, cpkt_native_byte_node_id, sizeof(cpkt_native_byte_node_id));
  native_status = UA_Server_addVariableNode(
      server,
      native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeByteNode"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  object_attr = UA_ObjectAttributes_default;
  object_attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"Native Object");
  object_attr.description = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"Native object description");
  object_attr.writeMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION;
  object_attr.userWriteMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION;
  object_attr.eventNotifier = UA_EVENTNOTIFIER_SUBSCRIBE_TO_EVENT;
  native_status = UA_Server_addObjectNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_OBJECT_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeObject"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
      object_attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.dataType = UA_TYPES[UA_TYPES_INT32].typeId;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"native object child");
  attr.valueRank = UA_VALUERANK_SCALAR;
  native_value = 7;
  UA_Variant_setScalar(&attr.value, &native_value, &UA_TYPES[UA_TYPES_INT32]);
  native_status = UA_Server_addVariableNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_CHILD_ID),
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_OBJECT_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, (char *)"nativeObjectChild"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  method_attr = UA_MethodAttributes_default;
  method_attr.executable = true;
  method_attr.userExecutable = true;
  method_attr.writeMask = UA_WRITEMASK_EXECUTABLE | UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION;
  method_attr.userWriteMask = UA_WRITEMASK_EXECUTABLE | UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION;
  method_attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"Native Increment");
  cpkt_test_method_argument_init(&native_input_argument, "input1", UA_NODEID_NUMERIC(0, UA_NS0ID_INT32));
  cpkt_test_method_argument_init(&native_output_argument, "output1", UA_NODEID_NUMERIC(0, UA_NS0ID_INT32));
  native_status = UA_Server_addMethodNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_METHOD_ID),
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_OBJECT_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
      UA_QUALIFIEDNAME(1, (char *)"nativeIncrement"),
      method_attr,
      cpkt_native_increment_method,
      1,
      &native_input_argument,
      1,
      &native_output_argument,
      NULL,
      NULL);
  UA_Argument_clear(&native_input_argument);
  UA_Argument_clear(&native_output_argument);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  method_attr = UA_MethodAttributes_default;
  method_attr.executable = true;
  method_attr.userExecutable = true;
  method_attr.writeMask = UA_WRITEMASK_EXECUTABLE | UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION;
  method_attr.userWriteMask = UA_WRITEMASK_EXECUTABLE | UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION;
  method_attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"Native Multi");
  cpkt_test_method_argument_init(&native_input_argument, "input1", UA_NODEID_NUMERIC(0, UA_NS0ID_INT32));
  cpkt_test_method_argument_init(&native_multi_output_arguments[0], "output1", UA_NODEID_NUMERIC(0, UA_NS0ID_INT32));
  cpkt_test_method_argument_init(&native_multi_output_arguments[1], "output2", UA_NODEID_NUMERIC(0, UA_NS0ID_DOUBLE));
  native_status = UA_Server_addMethodNode(
      server,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_MULTI_METHOD_ID),
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_OBJECT_ID),
      UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
      UA_QUALIFIEDNAME(1, (char *)"nativeMulti"),
      method_attr,
      cpkt_native_multi_method,
      1,
      &native_input_argument,
      2,
      native_multi_output_arguments,
      NULL,
      NULL);
  UA_Argument_clear(&native_input_argument);
  UA_Argument_clear(&native_multi_output_arguments[0]);
  UA_Argument_clear(&native_multi_output_arguments[1]);
  assert_int_equal(native_status, UA_STATUSCODE_GOOD);

  assert_int_equal(UA_Server_run_startup(server), UA_STATUSCODE_GOOD);
  thread.server = server;
  thread.running = 1;
  assert_int_equal(pthread_create(&thread_id, NULL, cpkt_native_server_loop, &thread), 0);

  facade_client = NULL;
  assert_int_equal(cpkt_opcua_client_new(&facade_client), CPKT_OPCUA_OK);
  assert_int_equal(cpkt_opcua_client_connect(facade_client, endpoint, NULL), CPKT_OPCUA_OK);
  native_value_seen.saw_expected_variant = 0;
  native_value_seen.saw_expected_data_value = 0;
  assert_int_equal(
      cpkt_opcua_client_read_native_variant(
          facade_client,
          cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_INT_ID),
          cpkt_native_integer_variant_callback,
          &native_value_seen,
          NULL),
      CPKT_OPCUA_OK);
  assert_int_equal(native_value_seen.saw_expected_variant, 1);
  assert_int_equal(
      cpkt_opcua_client_read_native_data_value(
          facade_client,
          cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_INT_ID),
          cpkt_native_integer_data_value_callback,
          &native_value_seen,
          NULL),
      CPKT_OPCUA_OK);
  assert_int_equal(native_value_seen.saw_expected_data_value, 1);
  assert_int_equal(
      cpkt_opcua_client_read_native_variant(
          facade_client,
          cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_INT_ID),
          cpkt_native_callback_returns_error,
          NULL,
          NULL),
      CPKT_OPCUA_ERR_CALLBACK);
  memset(&async_seen, 0, sizeof(async_seen));
  cpkt_opcua_value_integer(&async_value, 333);
  assert_int_equal(
      cpkt_opcua_client_write_async(
          facade_client,
          cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS, 7799),
          &async_value,
          cpkt_async_status_callback,
          &async_seen,
          &async_request_id,
          &async_status),
      CPKT_OPCUA_OK);
  assert_true(async_request_id != 0);
  cpkt_wait_for_async(facade_client, &async_seen);
  assert_int_equal(async_seen.result, CPKT_OPCUA_OK);
  assert_int_equal(async_seen.status, UA_STATUSCODE_GOOD);
  memset(&async_seen, 0, sizeof(async_seen));
  assert_int_equal(
      cpkt_opcua_client_read_async(
          facade_client,
          cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS, 7799),
          cpkt_async_value_callback,
          &async_seen,
          &async_request_id,
          NULL,
          0,
          NULL,
          &async_status),
      CPKT_OPCUA_OK);
  assert_true(async_request_id != 0);
  cpkt_wait_for_async(facade_client, &async_seen);
  assert_int_equal(async_seen.result, CPKT_OPCUA_OK);
  assert_int_equal(async_seen.integer_value, 333);
  memset(&async_seen, 0, sizeof(async_seen));
  assert_int_equal(
      cpkt_opcua_client_add_object_async(
          facade_client,
          cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS, 7800),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          "asyncClientObject",
          "Async Client Object",
          cpkt_async_node_callback,
          &async_seen,
          &async_request_id,
          &async_node_out,
          NULL,
          0,
          NULL,
          &async_status),
      CPKT_OPCUA_OK);
  cpkt_wait_for_async(facade_client, &async_seen);
  assert_int_equal(async_seen.result, CPKT_OPCUA_OK);
  assert_int_equal(async_seen.node_ok, 1);
  assert_int_equal(async_node_out.numeric, 7800);
  cpkt_assert_client_reference_count(
      facade_client,
      0,
      UA_NS0ID_OBJECTSFOLDER,
      UA_NS0ID_ORGANIZES,
      CPKT_OPCUA_TEST_NS,
      7800,
      UA_NODECLASS_OBJECT,
      "asyncClientObject",
      1);
  cpkt_assert_client_reference_count(
      facade_client,
      0,
      UA_NS0ID_OBJECTSFOLDER,
      UA_NS0ID_HASCOMPONENT,
      CPKT_OPCUA_TEST_NS,
      7800,
      UA_NODECLASS_OBJECT,
      "asyncClientObject",
      0);
  cpkt_opcua_value_integer(&async_value, 444);
  memset(&async_seen, 0, sizeof(async_seen));
  assert_int_equal(
      cpkt_opcua_client_add_variable_async(
          facade_client,
          cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS, 7801),
          cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS, 7800),
          "asyncClientVariable",
          "Async Client Variable",
          &async_value,
          cpkt_async_node_callback,
          &async_seen,
          &async_request_id,
          &async_node_out,
          NULL,
          0,
          NULL,
          &async_status),
      CPKT_OPCUA_OK);
  cpkt_wait_for_async(facade_client, &async_seen);
  assert_int_equal(async_seen.result, CPKT_OPCUA_OK);
  assert_int_equal(async_node_out.numeric, 7801);
  cpkt_assert_client_reference_count(
      facade_client,
      CPKT_OPCUA_TEST_NS,
      7800,
      UA_NS0ID_HASCOMPONENT,
      CPKT_OPCUA_TEST_NS,
      7801,
      UA_NODECLASS_VARIABLE,
      "asyncClientVariable",
      1);
  cpkt_assert_client_reference_count(
      facade_client,
      CPKT_OPCUA_TEST_NS,
      7800,
      UA_NS0ID_ORGANIZES,
      CPKT_OPCUA_TEST_NS,
      7801,
      UA_NODECLASS_VARIABLE,
      "asyncClientVariable",
      0);
  memset(&async_seen, 0, sizeof(async_seen));
  assert_int_equal(
      cpkt_opcua_client_browse_children_async(
          facade_client,
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          NULL,
          cpkt_async_browse_entry_callback,
          cpkt_async_browse_done_callback,
          &async_seen,
          &async_request_id,
          &async_status),
      CPKT_OPCUA_OK);
  cpkt_wait_for_async(facade_client, &async_seen);
  assert_int_equal(async_seen.result, CPKT_OPCUA_OK);
  assert_true(async_seen.browse_seen >= 1);
  cpkt_opcua_value_integer(&async_inputs[0], 9);
  memset(async_outputs, 0, sizeof(async_outputs));
  memset(&async_seen, 0, sizeof(async_seen));
  assert_int_equal(
      cpkt_opcua_client_call_method_async(
          facade_client,
          cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_OBJECT_ID),
          cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_MULTI_METHOD_ID),
          async_inputs,
          1,
          2,
          cpkt_async_call_callback,
          &async_seen,
          &async_request_id,
          async_outputs,
          NULL,
          NULL,
          NULL,
          &async_status),
      CPKT_OPCUA_OK);
  cpkt_wait_for_async(facade_client, &async_seen);
  assert_int_equal(async_seen.result, CPKT_OPCUA_OK);
  assert_int_equal(async_seen.integer_value, 18);
  assert_true(async_seen.double_value > 9.4 && async_seen.double_value < 9.6);
  cpkt_opcua_value_integer(&history_value, 111);
  assert_int_equal(
      cpkt_opcua_client_write(
          facade_client,
          cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_HISTORY_VALUE_ID),
          &history_value,
          NULL),
      CPKT_OPCUA_OK);
  usleep(10000);
  cpkt_opcua_value_integer(&history_value, 222);
  assert_int_equal(
      cpkt_opcua_client_write(
          facade_client,
          cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_HISTORY_VALUE_ID),
          &history_value,
          NULL),
      CPKT_OPCUA_OK);
  usleep(10000);
  memset(&history_seen, 0, sizeof(history_seen));
  history_start.high32 = 0;
  history_start.low32 = 0;
  history_end.high32 = 0x7fffffffL;
  history_end.low32 = 0xffffffffUL;
  assert_int_equal(
      cpkt_opcua_client_history_read_raw(
          facade_client,
          cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_HISTORY_VALUE_ID),
          history_start,
          history_end,
          NULL,
          0,
          16,
          cpkt_history_data_value_callback,
          &history_seen,
          NULL,
          0,
          NULL,
          NULL),
      CPKT_OPCUA_OK);
  assert_true(history_seen.count >= 2);
  assert_int_equal(history_seen.values[history_seen.count - 2], 111);
  assert_int_equal(history_seen.values[history_seen.count - 1], 222);
  assert_int_equal(history_seen.saw_timestamp, 1);
  assert_int_equal(
      cpkt_opcua_client_history_read_raw(
          facade_client,
          cpkt_opcua_node_id_numeric(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_NATIVE_HISTORY_VALUE_ID),
          history_start,
          history_end,
          NULL,
          0,
          16,
          cpkt_history_data_value_stops,
          NULL,
          NULL,
          0,
          NULL,
          NULL),
      CPKT_OPCUA_ERR_CALLBACK);
  cpkt_opcua_client_free(facade_client);

  c89_line = cpkt_opcua_c89_client_exercise_native_server(endpoint);
  assert_int_equal(c89_line, 0);

  thread.running = 0;
  assert_int_equal(pthread_join(thread_id, NULL), 0);
  assert_int_equal(UA_Server_run_shutdown(server), UA_STATUSCODE_GOOD);
  UA_Server_delete(server);
  UA_HistoryDataBackend_Memory_clear(&history_setting.historizingBackend);
}

static void cpkt_native_client_reads_and_writes_facade_server(void **state) {
  unsigned short port;
  cpkt_opcua_c89_server_peer *server;
  struct cpkt_facade_server_thread thread;
  pthread_t thread_id;
  char endpoint[80];
  UA_Client *client;
  UA_Client *rejected_client;
  UA_Variant native_variant;
  UA_Variant argument_variant;
  UA_Variant native_input;
  UA_Variant native_string_input;
  UA_Variant *native_outputs;
  UA_Argument *native_arguments;
  UA_Int32 native_int;
  UA_Int32 native_method_input;
  UA_Double native_double;
  UA_UInt64 native_uint64;
  UA_DateTime native_datetime;
  UA_String native_echo_input;
  UA_ByteString native_bytes;
  UA_QualifiedName native_qualified_name_input;
  UA_LocalizedText native_localized_text_input;
  UA_Guid expected_guid;
  UA_UInt32 *native_dimensions;
  UA_Boolean *native_bool_array;
  UA_Int32 *native_int_array;
  UA_Guid *native_guid_array;
  UA_StatusCode *native_status_array;
  UA_UInt64 *native_uint64_array;
  UA_DateTime *native_datetime_array;
  UA_Double *native_double_array;
  UA_QualifiedName *native_qualified_name_array;
  UA_LocalizedText *native_localized_text_array;
  UA_String *native_string_array;
  UA_ByteString *native_byte_string_array;
  UA_String namespace_uri;
  UA_UInt16 namespace_index;
  UA_EndpointDescription *endpoints;
  UA_ApplicationDescription *servers;
  size_t endpoint_count;
  size_t server_count;
  size_t native_output_count;
  size_t native_dimension_count;
  const char *browse_path_names[2];
  UA_BrowsePath browse_path;
  UA_BrowsePathResult browse_path_result;
  UA_BrowseDescription browse_description;
  UA_BrowseResult browse_result;
  UA_BrowseResult browse_next_result;
  UA_CreateSubscriptionRequest subscription_request;
  UA_CreateSubscriptionResponse subscription_response;
  UA_MonitoredItemCreateRequest event_request;
  UA_MonitoredItemCreateResult event_result;
  UA_EventFilter event_filter;
  UA_SimpleAttributeOperand event_select_clauses[4];
  UA_QualifiedName event_browse_names[4];
  UA_QualifiedName browse_name;
  UA_LocalizedText display_name;
  UA_LocalizedText description;
  UA_LocalizedText inverse_name;
  UA_NodeClass node_class;
  UA_NodeId data_type;
  UA_NodeId expected_data_type;
  UA_NodeId native_node_id;
  UA_NodeId reference_source_node_id;
  UA_NodeId reference_type_id;
  UA_NodeId reference_target_node_id;
  UA_Int32 value_rank;
  UA_Byte access_level;
  UA_UInt32 access_level_ex;
  UA_UInt32 write_mask;
  UA_Double minimum_sampling_interval;
  UA_Byte event_notifier;
  UA_Boolean executable;
  UA_Boolean historizing;
  UA_Boolean boolean_attribute;
  struct cpkt_native_event_seen event_seen;
  int c89_line;
  int attempt;

  (void)state;

  port = cpkt_test_free_port();
  server = NULL;
  c89_line = cpkt_opcua_c89_server_peer_new(port, &server, endpoint, sizeof(endpoint));
  assert_int_equal(c89_line, 0);
  c89_line = cpkt_opcua_c89_server_peer_start(server);
  assert_int_equal(c89_line, 0);
  thread.server = server;
  thread.running = 1;
  assert_int_equal(pthread_create(&thread_id, NULL, cpkt_facade_server_loop, &thread), 0);

  client = UA_Client_new();
  assert_non_null(client);
  assert_int_equal(UA_ClientConfig_setDefault(UA_Client_getConfig(client)), UA_STATUSCODE_GOOD);
  endpoints = NULL;
  endpoint_count = 0;
  assert_int_equal(UA_Client_getEndpoints(client, endpoint, &endpoint_count, &endpoints), UA_STATUSCODE_GOOD);
  assert_true(endpoint_count > 0);
  assert_true(endpoints != NULL);
  assert_true(endpoints[0].endpointUrl.length > strlen("opc.tcp://"));
  assert_memory_equal(endpoints[0].endpointUrl.data, "opc.tcp://", strlen("opc.tcp://"));
  UA_Array_delete(endpoints, endpoint_count, &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);
  servers = NULL;
  server_count = 0;
  assert_int_equal(
      UA_Client_findServers(client, endpoint, 0, NULL, 0, NULL, &server_count, &servers),
      UA_STATUSCODE_GOOD);
  assert_true(server_count > 0);
  assert_true(servers != NULL);
  assert_int_equal(servers[0].applicationUri.length, strlen(CPKT_OPCUA_FACADE_APPLICATION_URI));
  assert_memory_equal(
      servers[0].applicationUri.data,
      CPKT_OPCUA_FACADE_APPLICATION_URI,
      strlen(CPKT_OPCUA_FACADE_APPLICATION_URI));
  assert_int_equal(servers[0].productUri.length, strlen(CPKT_OPCUA_FACADE_PRODUCT_URI));
  assert_memory_equal(
      servers[0].productUri.data,
      CPKT_OPCUA_FACADE_PRODUCT_URI,
      strlen(CPKT_OPCUA_FACADE_PRODUCT_URI));
  assert_int_equal(servers[0].applicationName.text.length, strlen(CPKT_OPCUA_FACADE_APPLICATION_NAME));
  assert_memory_equal(
      servers[0].applicationName.text.data,
      CPKT_OPCUA_FACADE_APPLICATION_NAME,
      strlen(CPKT_OPCUA_FACADE_APPLICATION_NAME));
  UA_Array_delete(servers, server_count, &UA_TYPES[UA_TYPES_APPLICATIONDESCRIPTION]);

  rejected_client = UA_Client_new();
  assert_non_null(rejected_client);
  assert_int_equal(UA_ClientConfig_setDefault(UA_Client_getConfig(rejected_client)), UA_STATUSCODE_GOOD);
  UA_Client_getConfig(rejected_client)->allowNonePolicyPassword = true;
  assert_int_not_equal(
      UA_Client_connectUsername(rejected_client, endpoint, "c89-user", "wrong"),
      UA_STATUSCODE_GOOD);
  UA_Client_delete(rejected_client);

  UA_Client_getConfig(client)->allowNonePolicyPassword = true;
  assert_int_equal(
      cpkt_native_client_connect_username_with_retry(client, endpoint, "c89-user", "c89-secret"),
      UA_STATUSCODE_GOOD);
  namespace_index = 0;
  assert_int_equal(
      UA_Client_getNamespaceIndex(
          client,
          UA_STRING(CPKT_OPCUA_FACADE_NAMESPACE_URI),
          &namespace_index),
      UA_STATUSCODE_GOOD);
  assert_true(namespace_index != 0);
  UA_String_init(&namespace_uri);
  assert_int_equal(UA_Client_getNamespaceUri(client, namespace_index, &namespace_uri), UA_STATUSCODE_GOOD);
  assert_int_equal(namespace_uri.length, strlen(CPKT_OPCUA_FACADE_NAMESPACE_URI));
  assert_memory_equal(
      namespace_uri.data,
      CPKT_OPCUA_FACADE_NAMESPACE_URI,
      strlen(CPKT_OPCUA_FACADE_NAMESPACE_URI));
  UA_String_clear(&namespace_uri);

  browse_path_names[0] = "facadeObject";
  browse_path_names[1] = "facadeObjectChild";
  cpkt_test_browse_path_init(
      &browse_path,
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      CPKT_OPCUA_TEST_NS,
      browse_path_names,
      2);
  browse_path_result = UA_Client_translateBrowsePathToNodeIds(client, &browse_path);
  assert_int_equal(browse_path_result.statusCode, UA_STATUSCODE_GOOD);
  assert_int_equal(browse_path_result.targetsSize, 1);
  assert_int_equal(browse_path_result.targets[0].targetId.serverIndex, 0);
  native_node_id = UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_CHILD_ID);
  assert_true(
      UA_NodeId_equal(
          &browse_path_result.targets[0].targetId.nodeId,
          &native_node_id));
  UA_BrowsePathResult_clear(&browse_path_result);
  UA_BrowsePath_clear(&browse_path);

  reference_source_node_id = UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_OBJECT_ID);
  reference_type_id = UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT);
  assert_int_equal(
      cpkt_native_client_count_child_by_reference(
          client,
          reference_source_node_id,
          reference_type_id,
          native_node_id,
          "facadeObjectChild",
          UA_NODECLASS_VARIABLE),
      1);
  reference_target_node_id = UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_INT_ID);
  assert_int_equal(
      UA_Client_addReference(
          client,
          reference_source_node_id,
          reference_type_id,
          true,
          UA_STRING_NULL,
          UA_EXPANDEDNODEID_NODEID(reference_target_node_id),
          UA_NODECLASS_VARIABLE),
      UA_STATUSCODE_GOOD);
  assert_int_equal(
      cpkt_native_client_count_child_by_reference(
          client,
          reference_source_node_id,
          reference_type_id,
          reference_target_node_id,
          "facadeInteger",
          UA_NODECLASS_VARIABLE),
      1);
  UA_BrowseDescription_init(&browse_description);
  browse_description.nodeId = reference_source_node_id;
  browse_description.browseDirection = UA_BROWSEDIRECTION_FORWARD;
  browse_description.referenceTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_HIERARCHICALREFERENCES);
  browse_description.includeSubtypes = true;
  browse_description.nodeClassMask = UA_NODECLASS_VARIABLE;
  browse_description.resultMask = UA_BROWSERESULTMASK_ALL;
  browse_result = UA_Client_browse(client, NULL, 1, &browse_description);
  assert_int_equal(browse_result.statusCode, UA_STATUSCODE_GOOD);
  assert_int_equal(browse_result.referencesSize, 1);
  assert_true(browse_result.continuationPoint.length > 0);
  native_node_id = UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_CHILD_ID);
  assert_true(
      UA_NodeId_equal(&browse_result.references[0].nodeId.nodeId, &native_node_id) ||
      UA_NodeId_equal(&browse_result.references[0].nodeId.nodeId, &reference_target_node_id));
  browse_next_result = UA_Client_browseNext(client, false, browse_result.continuationPoint);
  assert_int_equal(browse_next_result.statusCode, UA_STATUSCODE_GOOD);
  assert_int_equal(browse_next_result.referencesSize, 1);
  assert_int_equal(browse_next_result.continuationPoint.length, 0);
  assert_true(
      UA_NodeId_equal(&browse_next_result.references[0].nodeId.nodeId, &native_node_id) ||
      UA_NodeId_equal(&browse_next_result.references[0].nodeId.nodeId, &reference_target_node_id));
  assert_false(
      UA_NodeId_equal(
          &browse_result.references[0].nodeId.nodeId,
          &browse_next_result.references[0].nodeId.nodeId));
  UA_BrowseResult_clear(&browse_next_result);
  UA_ByteString_init(&browse_result.continuationPoint);
  UA_BrowseResult_clear(&browse_result);
  assert_int_equal(
      UA_Client_deleteReference(
          client,
          reference_source_node_id,
          reference_type_id,
          true,
          UA_EXPANDEDNODEID_NODEID(reference_target_node_id),
          true),
      UA_STATUSCODE_GOOD);
  assert_int_equal(
      cpkt_native_client_count_child_by_reference(
          client,
          reference_source_node_id,
          reference_type_id,
          reference_target_node_id,
          "facadeInteger",
          UA_NODECLASS_VARIABLE),
      0);

  UA_Variant_init(&native_variant);
  assert_int_equal(
      UA_Client_readValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_INT_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasScalarType(&native_variant, &UA_TYPES[UA_TYPES_INT32]));
  assert_int_equal(*(UA_Int32 *)native_variant.data, 5);
  UA_Variant_clear(&native_variant);
  UA_Variant_init(&native_variant);
  native_node_id = cpkt_test_guid_node_id(CPKT_OPCUA_TEST_NS, cpkt_facade_guid_node_id);
  assert_int_equal(
      UA_Client_readValueAttribute(client, native_node_id, &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasScalarType(&native_variant, &UA_TYPES[UA_TYPES_INT32]));
  assert_int_equal(*(UA_Int32 *)native_variant.data, 414);
  UA_Variant_clear(&native_variant);
  UA_Variant_init(&native_variant);
  native_node_id =
      cpkt_test_byte_string_node_id(CPKT_OPCUA_TEST_NS, cpkt_facade_byte_node_id, sizeof(cpkt_facade_byte_node_id));
  assert_int_equal(
      UA_Client_readValueAttribute(client, native_node_id, &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasScalarType(&native_variant, &UA_TYPES[UA_TYPES_INT32]));
  assert_int_equal(*(UA_Int32 *)native_variant.data, 415);
  UA_Variant_clear(&native_variant);
  UA_NodeId_init(&data_type);
  assert_int_equal(
      UA_Client_readDataTypeAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_INT_ID),
          &data_type),
      UA_STATUSCODE_GOOD);
  expected_data_type = UA_NODEID_NUMERIC(0, UA_NS0ID_INT32);
  assert_true(UA_NodeId_equal(&data_type, &expected_data_type));
  UA_NodeId_clear(&data_type);
  UA_NodeId_init(&data_type);
  assert_int_equal(
      UA_Client_readDataTypeAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_BYTES_ID),
          &data_type),
      UA_STATUSCODE_GOOD);
  expected_data_type = UA_NODEID_NUMERIC(0, UA_NS0ID_BYTESTRING);
  assert_true(UA_NodeId_equal(&data_type, &expected_data_type));
  UA_NodeId_clear(&data_type);
  assert_int_equal(
      UA_Client_readValueRankAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_INT_ID),
          &value_rank),
      UA_STATUSCODE_GOOD);
  assert_int_equal(value_rank, UA_VALUERANK_SCALAR);
  native_dimensions = NULL;
  native_dimension_count = 0;
  assert_int_equal(
      UA_Client_readArrayDimensionsAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_ARRAY_ID),
          &native_dimension_count,
          &native_dimensions),
      UA_STATUSCODE_GOOD);
  assert_int_equal(native_dimension_count, 1);
  assert_non_null(native_dimensions);
  assert_int_equal(native_dimensions[0], 3);
  UA_Array_delete(native_dimensions, native_dimension_count, &UA_TYPES[UA_TYPES_UINT32]);
  UA_Variant_init(&native_variant);
  assert_int_equal(
      UA_Client_readValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_ARRAY_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasArrayType(&native_variant, &UA_TYPES[UA_TYPES_INT32]));
  assert_int_equal(native_variant.arrayLength, 3);
  native_int_array = (UA_Int32 *)native_variant.data;
  assert_int_equal(native_int_array[0], 510);
  assert_int_equal(native_int_array[1], 610);
  assert_int_equal(native_int_array[2], 611);
  UA_Variant_clear(&native_variant);
  UA_Variant_init(&native_variant);
  assert_int_equal(
      UA_Client_readValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_BOOL_ARRAY_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasArrayType(&native_variant, &UA_TYPES[UA_TYPES_BOOLEAN]));
  assert_int_equal(native_variant.arrayLength, 2);
  native_bool_array = (UA_Boolean *)native_variant.data;
  assert_true(native_bool_array[0]);
  assert_false(native_bool_array[1]);
  UA_Variant_clear(&native_variant);
  UA_Variant_init(&native_variant);
  assert_int_equal(
      UA_Client_readValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_DOUBLE_ARRAY_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasArrayType(&native_variant, &UA_TYPES[UA_TYPES_DOUBLE]));
  assert_int_equal(native_variant.arrayLength, 2);
  native_double_array = (UA_Double *)native_variant.data;
  assert_true(native_double_array[0] > 7.24 && native_double_array[0] < 7.26);
  assert_true(native_double_array[1] > 8.49 && native_double_array[1] < 8.51);
  UA_Variant_clear(&native_variant);
  UA_Variant_init(&native_variant);
  assert_int_equal(
      UA_Client_readValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_STRING_ARRAY_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasArrayType(&native_variant, &UA_TYPES[UA_TYPES_STRING]));
  assert_int_equal(native_variant.arrayLength, 2);
  native_string_array = (UA_String *)native_variant.data;
  assert_int_equal(native_string_array[0].length, strlen("alpha"));
  assert_memory_equal(native_string_array[0].data, "alpha", strlen("alpha"));
  assert_int_equal(native_string_array[1].length, strlen("beta"));
  assert_memory_equal(native_string_array[1].data, "beta", strlen("beta"));
  UA_Variant_clear(&native_variant);
  UA_Variant_init(&native_variant);
  assert_int_equal(
      UA_Client_readValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_BYTES_ARRAY_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasArrayType(&native_variant, &UA_TYPES[UA_TYPES_BYTESTRING]));
  assert_int_equal(native_variant.arrayLength, 2);
  native_byte_string_array = (UA_ByteString *)native_variant.data;
  assert_int_equal(native_byte_string_array[0].length, 2);
  assert_int_equal(native_byte_string_array[0].data[0], 0x10U);
  assert_int_equal(native_byte_string_array[0].data[1], 0x11U);
  assert_int_equal(native_byte_string_array[1].length, 3);
  assert_int_equal(native_byte_string_array[1].data[0], 0x20U);
  assert_int_equal(native_byte_string_array[1].data[1], 0x21U);
  assert_int_equal(native_byte_string_array[1].data[2], 0x22U);
  UA_Variant_clear(&native_variant);
  UA_Variant_init(&native_variant);
  assert_int_equal(
      UA_Client_readValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_GUID_VALUE_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasScalarType(&native_variant, &UA_TYPES[UA_TYPES_GUID]));
  expected_guid = cpkt_test_guid(cpkt_facade_guid_node_id);
  assert_true(UA_Guid_equal((UA_Guid *)native_variant.data, &expected_guid));
  UA_Variant_clear(&native_variant);
  UA_Variant_init(&native_variant);
  assert_int_equal(
      UA_Client_readValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_GUID_ARRAY_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasArrayType(&native_variant, &UA_TYPES[UA_TYPES_GUID]));
  assert_int_equal(native_variant.arrayLength, 2);
  native_guid_array = (UA_Guid *)native_variant.data;
  expected_guid = cpkt_test_guid(cpkt_native_guid_node_id);
  assert_true(UA_Guid_equal(&native_guid_array[0], &expected_guid));
  expected_guid = cpkt_test_guid(cpkt_facade_guid_node_id);
  assert_true(UA_Guid_equal(&native_guid_array[1], &expected_guid));
  UA_Variant_clear(&native_variant);
  UA_Variant_init(&native_variant);
  assert_int_equal(
      UA_Client_readValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_STATUS_VALUE_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasScalarType(&native_variant, &UA_TYPES[UA_TYPES_STATUSCODE]));
  assert_int_equal(*(UA_StatusCode *)native_variant.data, UA_STATUSCODE_GOOD);
  UA_Variant_clear(&native_variant);
  UA_Variant_init(&native_variant);
  assert_int_equal(
      UA_Client_readValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_STATUS_ARRAY_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasArrayType(&native_variant, &UA_TYPES[UA_TYPES_STATUSCODE]));
  assert_int_equal(native_variant.arrayLength, 2);
  native_status_array = (UA_StatusCode *)native_variant.data;
  assert_int_equal(native_status_array[0], UA_STATUSCODE_GOOD);
  assert_int_equal(native_status_array[1], UA_STATUSCODE_BADNOTFOUND);
  UA_Variant_clear(&native_variant);
  UA_Variant_init(&native_variant);
  assert_int_equal(
      UA_Client_readValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_UINT64_VALUE_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasScalarType(&native_variant, &UA_TYPES[UA_TYPES_UINT64]));
  native_uint64 = *(UA_UInt64 *)native_variant.data;
  assert_int_equal((UA_UInt32)(native_uint64 >> 32), 0xffffffffU);
  assert_int_equal((UA_UInt32)(native_uint64 & (UA_UInt64)0xffffffffU), 0xffffffffU);
  UA_Variant_clear(&native_variant);
  UA_Variant_init(&native_variant);
  assert_int_equal(
      UA_Client_readValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_UINT64_ARRAY_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasArrayType(&native_variant, &UA_TYPES[UA_TYPES_UINT64]));
  assert_int_equal(native_variant.arrayLength, 2);
  native_uint64_array = (UA_UInt64 *)native_variant.data;
  assert_int_equal((UA_UInt32)(native_uint64_array[0] >> 32), 0x11111111U);
  assert_int_equal((UA_UInt32)(native_uint64_array[0] & (UA_UInt64)0xffffffffU), 0x22222222U);
  assert_int_equal((UA_UInt32)(native_uint64_array[1] >> 32), 0x33333333U);
  assert_int_equal((UA_UInt32)(native_uint64_array[1] & (UA_UInt64)0xffffffffU), 0x44444444U);
  UA_Variant_clear(&native_variant);
  UA_Variant_init(&native_variant);
  assert_int_equal(
      UA_Client_readValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_DATETIME_VALUE_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasScalarType(&native_variant, &UA_TYPES[UA_TYPES_DATETIME]));
  native_datetime = *(UA_DateTime *)native_variant.data;
  assert_int_equal((UA_UInt32)(((UA_UInt64)native_datetime) >> 32), 0x01234567U);
  assert_int_equal((UA_UInt32)(((UA_UInt64)native_datetime) & (UA_UInt64)0xffffffffU), 0x76543210U);
  UA_Variant_clear(&native_variant);
  UA_Variant_init(&native_variant);
  assert_int_equal(
      UA_Client_readValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_DATETIME_ARRAY_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasArrayType(&native_variant, &UA_TYPES[UA_TYPES_DATETIME]));
  assert_int_equal(native_variant.arrayLength, 3);
  native_datetime_array = (UA_DateTime *)native_variant.data;
  assert_int_equal((UA_UInt32)(((UA_UInt64)native_datetime_array[0]) >> 32), 0x11111111U);
  assert_int_equal((UA_UInt32)(((UA_UInt64)native_datetime_array[0]) & (UA_UInt64)0xffffffffU), 0x22222222U);
  assert_int_equal((UA_UInt32)(((UA_UInt64)native_datetime_array[1]) >> 32), 0xfffffffeU);
  assert_int_equal((UA_UInt32)(((UA_UInt64)native_datetime_array[1]) & (UA_UInt64)0xffffffffU), 0x33333333U);
  assert_int_equal((UA_UInt32)(((UA_UInt64)native_datetime_array[2]) >> 32), 0x80000000U);
  assert_int_equal((UA_UInt32)(((UA_UInt64)native_datetime_array[2]) & (UA_UInt64)0xffffffffU), 0x44444444U);
  UA_Variant_clear(&native_variant);
  UA_Variant_init(&native_variant);
  assert_int_equal(
      UA_Client_readValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_QUALIFIED_NAME_VALUE_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasScalarType(&native_variant, &UA_TYPES[UA_TYPES_QUALIFIEDNAME]));
  assert_int_equal(((UA_QualifiedName *)native_variant.data)->namespaceIndex, CPKT_OPCUA_TEST_NS);
  assert_int_equal(((UA_QualifiedName *)native_variant.data)->name.length, strlen("facadeQualified"));
  assert_memory_equal(
      ((UA_QualifiedName *)native_variant.data)->name.data,
      "facadeQualified",
      strlen("facadeQualified"));
  UA_Variant_clear(&native_variant);
  UA_Variant_init(&native_variant);
  assert_int_equal(
      UA_Client_readValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_QUALIFIED_NAME_ARRAY_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasArrayType(&native_variant, &UA_TYPES[UA_TYPES_QUALIFIEDNAME]));
  assert_int_equal(native_variant.arrayLength, 2);
  native_qualified_name_array = (UA_QualifiedName *)native_variant.data;
  assert_int_equal(native_qualified_name_array[0].namespaceIndex, CPKT_OPCUA_TEST_NS);
  assert_int_equal(native_qualified_name_array[0].name.length, strlen("alphaName"));
  assert_memory_equal(native_qualified_name_array[0].name.data, "alphaName", strlen("alphaName"));
  assert_int_equal(native_qualified_name_array[1].namespaceIndex, CPKT_OPCUA_TEST_NS);
  assert_int_equal(native_qualified_name_array[1].name.length, strlen("betaName"));
  assert_memory_equal(native_qualified_name_array[1].name.data, "betaName", strlen("betaName"));
  UA_Variant_clear(&native_variant);
  UA_Variant_init(&native_variant);
  assert_int_equal(
      UA_Client_readValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_LOCALIZED_TEXT_VALUE_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasScalarType(&native_variant, &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]));
  assert_int_equal(((UA_LocalizedText *)native_variant.data)->locale.length, strlen("en-US"));
  assert_memory_equal(((UA_LocalizedText *)native_variant.data)->locale.data, "en-US", strlen("en-US"));
  assert_int_equal(((UA_LocalizedText *)native_variant.data)->text.length, strlen("facade text"));
  assert_memory_equal(
      ((UA_LocalizedText *)native_variant.data)->text.data,
      "facade text",
      strlen("facade text"));
  UA_Variant_clear(&native_variant);
  UA_Variant_init(&native_variant);
  assert_int_equal(
      UA_Client_readValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_LOCALIZED_TEXT_ARRAY_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasArrayType(&native_variant, &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]));
  assert_int_equal(native_variant.arrayLength, 2);
  native_localized_text_array = (UA_LocalizedText *)native_variant.data;
  assert_int_equal(native_localized_text_array[0].locale.length, strlen("en-US"));
  assert_memory_equal(native_localized_text_array[0].locale.data, "en-US", strlen("en-US"));
  assert_int_equal(native_localized_text_array[0].text.length, strlen("alpha text"));
  assert_memory_equal(native_localized_text_array[0].text.data, "alpha text", strlen("alpha text"));
  assert_int_equal(native_localized_text_array[1].locale.length, strlen("sv-SE"));
  assert_memory_equal(native_localized_text_array[1].locale.data, "sv-SE", strlen("sv-SE"));
  assert_int_equal(native_localized_text_array[1].text.length, strlen("beta text"));
  assert_memory_equal(native_localized_text_array[1].text.data, "beta text", strlen("beta text"));
  UA_Variant_clear(&native_variant);
  assert_int_equal(
      UA_Client_readAccessLevelAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_INT_ID),
          &access_level),
      UA_STATUSCODE_GOOD);
  assert_true((access_level & UA_ACCESSLEVELMASK_READ) != 0);
  assert_true((access_level & UA_ACCESSLEVELMASK_WRITE) != 0);
  write_mask = 0;
  assert_int_equal(
      UA_Client_readWriteMaskAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_INT_ID),
          &write_mask),
      UA_STATUSCODE_GOOD);
  assert_true((write_mask & UA_WRITEMASK_ACCESSLEVEL) != 0);
  assert_true((write_mask & UA_WRITEMASK_HISTORIZING) != 0);
  access_level_ex = 0;
  assert_int_equal(
      UA_Client_readAccessLevelExAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_INT_ID),
          &access_level_ex),
      UA_STATUSCODE_GOOD);
  assert_true((access_level_ex & UA_ACCESSLEVELMASK_READ) != 0);
  assert_true((access_level_ex & UA_ACCESSLEVELMASK_WRITE) != 0);
  minimum_sampling_interval = 0.0;
  assert_int_equal(
      UA_Client_readMinimumSamplingIntervalAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_INT_ID),
          &minimum_sampling_interval),
      UA_STATUSCODE_GOOD);
  assert_true(minimum_sampling_interval > 8.4 && minimum_sampling_interval < 8.6);
  access_level = UA_ACCESSLEVELMASK_READ;
  assert_int_equal(
      UA_Client_writeAccessLevelAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_INT_ID),
          &access_level),
      UA_STATUSCODE_GOOD);
  assert_int_equal(
      UA_Client_readAccessLevelAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_INT_ID),
          &access_level),
      UA_STATUSCODE_GOOD);
  assert_true((access_level & UA_ACCESSLEVELMASK_READ) != 0);
  assert_true((access_level & UA_ACCESSLEVELMASK_WRITE) == 0);
  access_level = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  assert_int_equal(
      UA_Client_writeAccessLevelAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_INT_ID),
          &access_level),
      UA_STATUSCODE_GOOD);
  historizing = false;
  assert_int_equal(
      UA_Client_readHistorizingAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_INT_ID),
          &historizing),
      UA_STATUSCODE_GOOD);
  assert_false(historizing);
  historizing = true;
  assert_int_equal(
      UA_Client_writeHistorizingAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_INT_ID),
          &historizing),
      UA_STATUSCODE_GOOD);
  historizing = false;
  assert_int_equal(
      UA_Client_readHistorizingAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_INT_ID),
          &historizing),
      UA_STATUSCODE_GOOD);
  assert_true(historizing);
  historizing = false;
  assert_int_equal(
      UA_Client_writeHistorizingAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_INT_ID),
          &historizing),
      UA_STATUSCODE_GOOD);

  native_int = 6;
  UA_Variant_setScalarCopy(&native_variant, &native_int, &UA_TYPES[UA_TYPES_INT32]);
  assert_int_equal(
      UA_Client_writeValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_INT_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  UA_Variant_clear(&native_variant);
  UA_Variant_init(&native_variant);
  assert_int_equal(
      UA_Client_readValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_INT_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasScalarType(&native_variant, &UA_TYPES[UA_TYPES_INT32]));
  assert_int_equal(*(UA_Int32 *)native_variant.data, 6);
  UA_Variant_clear(&native_variant);

  native_double = 7.25;
  UA_Variant_setScalarCopy(&native_variant, &native_double, &UA_TYPES[UA_TYPES_DOUBLE]);
  assert_int_equal(
      UA_Client_writeValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_DOUBLE_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  UA_Variant_clear(&native_variant);
  UA_Variant_init(&native_variant);
  assert_int_equal(
      UA_Client_readValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_DOUBLE_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasScalarType(&native_variant, &UA_TYPES[UA_TYPES_DOUBLE]));
  assert_true(*(UA_Double *)native_variant.data > 7.24 && *(UA_Double *)native_variant.data < 7.26);
  UA_Variant_clear(&native_variant);

  UA_Variant_init(&native_variant);
  assert_int_equal(
      UA_Client_readValueAttribute(
          client,
          UA_NODEID_STRING(CPKT_OPCUA_TEST_NS, (char *)CPKT_OPCUA_FACADE_STRING_NODE_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasScalarType(&native_variant, &UA_TYPES[UA_TYPES_STRING]));
  assert_int_equal(((UA_String *)native_variant.data)->length, strlen("facade string id"));
  assert_memory_equal(
      ((UA_String *)native_variant.data)->data,
      "facade string id",
      strlen("facade string id"));
  UA_Variant_clear(&native_variant);

  UA_Variant_init(&native_variant);
  assert_int_equal(
      UA_Client_readValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_BYTES_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasScalarType(&native_variant, &UA_TYPES[UA_TYPES_BYTESTRING]));
  assert_int_equal(((UA_ByteString *)native_variant.data)->length, sizeof(cpkt_facade_bytes_value));
  assert_memory_equal(
      ((UA_ByteString *)native_variant.data)->data,
      cpkt_facade_bytes_value,
      sizeof(cpkt_facade_bytes_value));
  UA_Variant_clear(&native_variant);
  native_bytes = UA_BYTESTRING_NULL;
  native_bytes.data = (UA_Byte *)cpkt_facade_bytes_updated;
  native_bytes.length = sizeof(cpkt_facade_bytes_updated);
  UA_Variant_setScalarCopy(&native_variant, &native_bytes, &UA_TYPES[UA_TYPES_BYTESTRING]);
  assert_int_equal(
      UA_Client_writeValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_BYTES_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  UA_Variant_clear(&native_variant);
  UA_Variant_init(&native_variant);
  assert_int_equal(
      UA_Client_readValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_BYTES_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasScalarType(&native_variant, &UA_TYPES[UA_TYPES_BYTESTRING]));
  assert_int_equal(((UA_ByteString *)native_variant.data)->length, sizeof(cpkt_facade_bytes_updated));
  assert_memory_equal(
      ((UA_ByteString *)native_variant.data)->data,
      cpkt_facade_bytes_updated,
      sizeof(cpkt_facade_bytes_updated));
  UA_Variant_clear(&native_variant);

  UA_Variant_init(&native_variant);
  assert_int_equal(
      UA_Client_readValueAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_CHILD_ID),
          &native_variant),
      UA_STATUSCODE_GOOD);
  assert_true(UA_Variant_hasScalarType(&native_variant, &UA_TYPES[UA_TYPES_INT32]));
  assert_int_equal(*(UA_Int32 *)native_variant.data, 11);
  UA_Variant_clear(&native_variant);

  UA_QualifiedName_init(&browse_name);
  assert_int_equal(
      UA_Client_readBrowseNameAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_OBJECT_ID),
          &browse_name),
      UA_STATUSCODE_GOOD);
  assert_int_equal(browse_name.name.length, strlen("facadeObject"));
  assert_memory_equal(browse_name.name.data, "facadeObject", strlen("facadeObject"));
  UA_QualifiedName_clear(&browse_name);

  node_class = UA_NODECLASS_UNSPECIFIED;
  assert_int_equal(
      UA_Client_readNodeClassAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_OBJECT_ID),
          &node_class),
      UA_STATUSCODE_GOOD);
  assert_int_equal(node_class, UA_NODECLASS_OBJECT);
  assert_int_equal(
      UA_Client_readNodeClassAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_OBJECT_TYPE_ID),
          &node_class),
      UA_STATUSCODE_GOOD);
  assert_int_equal(node_class, UA_NODECLASS_OBJECTTYPE);
  assert_int_equal(
      UA_Client_readNodeClassAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_VARIABLE_TYPE_ID),
          &node_class),
      UA_STATUSCODE_GOOD);
  assert_int_equal(node_class, UA_NODECLASS_VARIABLETYPE);
  assert_int_equal(
      UA_Client_readNodeClassAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_ARRAY_VARIABLE_TYPE_ID),
          &node_class),
      UA_STATUSCODE_GOOD);
  assert_int_equal(node_class, UA_NODECLASS_VARIABLETYPE);
  assert_int_equal(
      UA_Client_readValueRankAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_ARRAY_VARIABLE_TYPE_ID),
          &value_rank),
      UA_STATUSCODE_GOOD);
  assert_int_equal(value_rank, 1);
  native_dimensions = NULL;
  native_dimension_count = 0;
  assert_int_equal(
      UA_Client_readArrayDimensionsAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_ARRAY_VARIABLE_TYPE_ID),
          &native_dimension_count,
          &native_dimensions),
      UA_STATUSCODE_GOOD);
  assert_int_equal(native_dimension_count, 1);
  assert_non_null(native_dimensions);
  assert_int_equal(native_dimensions[0], 3);
  UA_Array_delete(native_dimensions, native_dimension_count, &UA_TYPES[UA_TYPES_UINT32]);
  assert_int_equal(
      UA_Client_readNodeClassAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_REFERENCE_TYPE_ID),
          &node_class),
      UA_STATUSCODE_GOOD);
  assert_int_equal(node_class, UA_NODECLASS_REFERENCETYPE);
  assert_int_equal(
      UA_Client_readNodeClassAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_DATA_TYPE_ID),
          &node_class),
      UA_STATUSCODE_GOOD);
  assert_int_equal(node_class, UA_NODECLASS_DATATYPE);
  assert_int_equal(
      UA_Client_readNodeClassAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_VIEW_ID),
          &node_class),
      UA_STATUSCODE_GOOD);
  assert_int_equal(node_class, UA_NODECLASS_VIEW);
  boolean_attribute = false;
  assert_int_equal(
      UA_Client_readIsAbstractAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_OBJECT_TYPE_ID),
          &boolean_attribute),
      UA_STATUSCODE_GOOD);
  assert_true(boolean_attribute);
  boolean_attribute = false;
  assert_int_equal(
      UA_Client_readIsAbstractAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_DATA_TYPE_ID),
          &boolean_attribute),
      UA_STATUSCODE_GOOD);
  assert_true(boolean_attribute);
  UA_LocalizedText_init(&inverse_name);
  assert_int_equal(
      UA_Client_readInverseNameAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_REFERENCE_TYPE_ID),
          &inverse_name),
      UA_STATUSCODE_GOOD);
  assert_int_equal(inverse_name.text.length, strlen("Facade Reference Type Inverse"));
  assert_memory_equal(
      inverse_name.text.data,
      "Facade Reference Type Inverse",
      strlen("Facade Reference Type Inverse"));
  UA_LocalizedText_clear(&inverse_name);
  boolean_attribute = false;
  assert_int_equal(
      UA_Client_readContainsNoLoopsAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_VIEW_ID),
          &boolean_attribute),
      UA_STATUSCODE_GOOD);
  assert_true(boolean_attribute);
  event_notifier = 99;
  assert_int_equal(
      UA_Client_readEventNotifierAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_VIEW_ID),
          &event_notifier),
      UA_STATUSCODE_GOOD);
  assert_int_equal(event_notifier, 0);
  UA_LocalizedText_init(&display_name);
  assert_int_equal(
      UA_Client_readDisplayNameAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_OBJECT_ID),
          &display_name),
      UA_STATUSCODE_GOOD);
  assert_int_equal(display_name.text.length, strlen("Facade Object Updated"));
  assert_memory_equal(display_name.text.data, "Facade Object Updated", strlen("Facade Object Updated"));
  UA_LocalizedText_clear(&display_name);
  display_name = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"Facade Object Native");
  assert_int_equal(
      UA_Client_writeDisplayNameAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_OBJECT_ID),
          &display_name),
      UA_STATUSCODE_GOOD);
  UA_LocalizedText_init(&display_name);
  assert_int_equal(
      UA_Client_readDisplayNameAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_OBJECT_ID),
          &display_name),
      UA_STATUSCODE_GOOD);
  assert_int_equal(display_name.text.length, strlen("Facade Object Native"));
  assert_memory_equal(display_name.text.data, "Facade Object Native", strlen("Facade Object Native"));
  UA_LocalizedText_clear(&display_name);
  UA_LocalizedText_init(&description);
  assert_int_equal(
      UA_Client_readDescriptionAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_OBJECT_ID),
          &description),
      UA_STATUSCODE_GOOD);
  assert_int_equal(description.text.length, strlen("Facade object description"));
  assert_memory_equal(
      description.text.data,
      "Facade object description",
      strlen("Facade object description"));
  UA_LocalizedText_clear(&description);
  event_notifier = 99;
  assert_int_equal(
      UA_Client_readEventNotifierAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_OBJECT_ID),
          &event_notifier),
      UA_STATUSCODE_GOOD);
  assert_int_equal(event_notifier, 0);
  event_notifier = UA_EVENTNOTIFIER_SUBSCRIBE_TO_EVENT;
  assert_int_equal(
      UA_Client_writeEventNotifierAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_OBJECT_ID),
          &event_notifier),
      UA_STATUSCODE_GOOD);
  subscription_request = UA_CreateSubscriptionRequest_default();
  subscription_request.requestedPublishingInterval = 25.0;
  subscription_response =
      UA_Client_Subscriptions_create(client, subscription_request, NULL, NULL, NULL);
  assert_int_equal(subscription_response.responseHeader.serviceResult, UA_STATUSCODE_GOOD);
  assert_true(subscription_response.subscriptionId != 0);
  event_request =
      UA_MonitoredItemCreateRequest_default(UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_OBJECT_ID));
  event_request.itemToMonitor.attributeId = UA_ATTRIBUTEID_EVENTNOTIFIER;
  event_request.requestedParameters.samplingInterval = 10.0;
  UA_EventFilter_init(&event_filter);
  cpkt_test_event_select_clause(&event_select_clauses[0], &event_browse_names[0], "EventId");
  cpkt_test_event_select_clause(&event_select_clauses[1], &event_browse_names[1], "SourceName");
  cpkt_test_event_select_clause(&event_select_clauses[2], &event_browse_names[2], "Message");
  cpkt_test_event_select_clause(&event_select_clauses[3], &event_browse_names[3], "Severity");
  event_filter.selectClauses = event_select_clauses;
  event_filter.selectClausesSize = 4;
  event_request.requestedParameters.filter.encoding = UA_EXTENSIONOBJECT_DECODED;
  event_request.requestedParameters.filter.content.decoded.type = &UA_TYPES[UA_TYPES_EVENTFILTER];
  event_request.requestedParameters.filter.content.decoded.data = &event_filter;
  memset(&event_seen, 0, sizeof(event_seen));
  event_result = UA_Client_MonitoredItems_createEvent(
      client,
      subscription_response.subscriptionId,
      UA_TIMESTAMPSTORETURN_BOTH,
      event_request,
      &event_seen,
      cpkt_native_event_callback,
      NULL);
  assert_int_equal(event_result.statusCode, UA_STATUSCODE_GOOD);
  assert_true(event_result.monitoredItemId != 0);
  c89_line = cpkt_opcua_c89_server_peer_trigger_event(server);
  assert_int_equal(c89_line, 0);
  for (attempt = 0; attempt < 100 && !event_seen.saw_expected; ++attempt) {
    (void)UA_Client_run_iterate(client, 50);
  }
  assert_true(event_seen.saw_expected);
  assert_int_equal(event_seen.severity, 654);
  assert_true(event_seen.event_id_length != 0);
  assert_int_equal(
      UA_Client_MonitoredItems_deleteSingle(
          client,
          subscription_response.subscriptionId,
          event_result.monitoredItemId),
      UA_STATUSCODE_GOOD);
  assert_int_equal(
      UA_Client_Subscriptions_deleteSingle(client, subscription_response.subscriptionId),
      UA_STATUSCODE_GOOD);
  event_notifier = 0;
  assert_int_equal(
      UA_Client_writeEventNotifierAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_OBJECT_ID),
          &event_notifier),
      UA_STATUSCODE_GOOD);
  assert_int_equal(
      UA_Client_readExecutableAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_METHOD_ID),
          &executable),
      UA_STATUSCODE_GOOD);
  assert_true(executable);
  executable = false;
  assert_int_equal(
      UA_Client_writeExecutableAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_METHOD_ID),
          &executable),
      UA_STATUSCODE_GOOD);
  assert_int_equal(
      UA_Client_readExecutableAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_METHOD_ID),
          &executable),
      UA_STATUSCODE_GOOD);
  assert_false(executable);
  executable = true;
  assert_int_equal(
      UA_Client_writeExecutableAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_METHOD_ID),
          &executable),
      UA_STATUSCODE_GOOD);
  assert_int_equal(
      UA_Client_readUserExecutableAttribute(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_METHOD_ID),
          &executable),
      UA_STATUSCODE_GOOD);
  assert_true(executable);

  cpkt_native_client_read_method_arguments(
      client,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_METHOD_ID),
      "InputArguments",
      &argument_variant);
  assert_true(UA_Variant_hasArrayType(&argument_variant, &UA_TYPES[UA_TYPES_ARGUMENT]));
  assert_int_equal(argument_variant.arrayLength, 1);
  native_arguments = (UA_Argument *)argument_variant.data;
  assert_int_equal(native_arguments[0].name.length, strlen("input1"));
  assert_memory_equal(native_arguments[0].name.data, "input1", strlen("input1"));
  expected_data_type = UA_NODEID_NUMERIC(0, UA_NS0ID_INT32);
  assert_true(UA_NodeId_equal(&native_arguments[0].dataType, &expected_data_type));
  assert_int_equal(native_arguments[0].valueRank, UA_VALUERANK_SCALAR);
  UA_Variant_clear(&argument_variant);

  cpkt_native_client_read_method_arguments(
      client,
      UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_MULTI_METHOD_ID),
      "OutputArguments",
      &argument_variant);
  assert_true(UA_Variant_hasArrayType(&argument_variant, &UA_TYPES[UA_TYPES_ARGUMENT]));
  assert_int_equal(argument_variant.arrayLength, 2);
  native_arguments = (UA_Argument *)argument_variant.data;
  assert_int_equal(native_arguments[0].name.length, strlen("output1"));
  assert_memory_equal(native_arguments[0].name.data, "output1", strlen("output1"));
  expected_data_type = UA_NODEID_NUMERIC(0, UA_NS0ID_INT32);
  assert_true(UA_NodeId_equal(&native_arguments[0].dataType, &expected_data_type));
  assert_int_equal(native_arguments[0].valueRank, UA_VALUERANK_SCALAR);
  assert_int_equal(native_arguments[1].name.length, strlen("output2"));
  assert_memory_equal(native_arguments[1].name.data, "output2", strlen("output2"));
  expected_data_type = UA_NODEID_NUMERIC(0, UA_NS0ID_DOUBLE);
  assert_true(UA_NodeId_equal(&native_arguments[1].dataType, &expected_data_type));
  assert_int_equal(native_arguments[1].valueRank, UA_VALUERANK_SCALAR);
  UA_Variant_clear(&argument_variant);

  UA_Variant_init(&native_input);
  native_method_input = 7;
  UA_Variant_setScalarCopy(&native_input, &native_method_input, &UA_TYPES[UA_TYPES_INT32]);
  native_output_count = 0;
  native_outputs = NULL;
  assert_int_equal(
      UA_Client_call(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_OBJECT_ID),
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_METHOD_ID),
          1,
          &native_input,
          &native_output_count,
          &native_outputs),
      UA_STATUSCODE_GOOD);
  assert_int_equal(native_output_count, 1);
  assert_true(UA_Variant_hasScalarType(&native_outputs[0], &UA_TYPES[UA_TYPES_INT32]));
  assert_int_equal(*(UA_Int32 *)native_outputs[0].data, 21);
  UA_Array_delete(native_outputs, native_output_count, &UA_TYPES[UA_TYPES_VARIANT]);
  UA_Variant_clear(&native_input);

  UA_Variant_init(&native_input);
  native_method_input = 8;
  UA_Variant_setScalarCopy(&native_input, &native_method_input, &UA_TYPES[UA_TYPES_INT32]);
  native_output_count = 0;
  native_outputs = NULL;
  assert_int_equal(
      UA_Client_call(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_OBJECT_ID),
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_MULTI_METHOD_ID),
          1,
          &native_input,
          &native_output_count,
          &native_outputs),
      UA_STATUSCODE_GOOD);
  assert_int_equal(native_output_count, 2);
  assert_true(UA_Variant_hasScalarType(&native_outputs[0], &UA_TYPES[UA_TYPES_INT32]));
  assert_int_equal(*(UA_Int32 *)native_outputs[0].data, 9);
  assert_true(UA_Variant_hasScalarType(&native_outputs[1], &UA_TYPES[UA_TYPES_DOUBLE]));
  assert_true(*(UA_Double *)native_outputs[1].data > 8.24 && *(UA_Double *)native_outputs[1].data < 8.26);
  UA_Array_delete(native_outputs, native_output_count, &UA_TYPES[UA_TYPES_VARIANT]);
  UA_Variant_clear(&native_input);

  UA_Variant_init(&native_string_input);
  native_echo_input = UA_STRING((char *)"facade echo input");
  UA_Variant_setScalarCopy(&native_string_input, &native_echo_input, &UA_TYPES[UA_TYPES_STRING]);
  native_output_count = 0;
  native_outputs = NULL;
  assert_int_equal(
      UA_Client_call(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_OBJECT_ID),
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_ECHO_METHOD_ID),
          1,
          &native_string_input,
          &native_output_count,
          &native_outputs),
      UA_STATUSCODE_GOOD);
  assert_int_equal(native_output_count, 1);
  assert_true(UA_Variant_hasScalarType(&native_outputs[0], &UA_TYPES[UA_TYPES_STRING]));
  assert_int_equal(((UA_String *)native_outputs[0].data)->length, strlen("facade echo input"));
  assert_memory_equal(
      ((UA_String *)native_outputs[0].data)->data,
      "facade echo input",
      strlen("facade echo input"));
  UA_Array_delete(native_outputs, native_output_count, &UA_TYPES[UA_TYPES_VARIANT]);
  UA_Variant_clear(&native_string_input);

  UA_Variant_init(&native_input);
  native_bytes = UA_BYTESTRING_NULL;
  native_bytes.data = (UA_Byte *)cpkt_facade_bytes_updated;
  native_bytes.length = sizeof(cpkt_facade_bytes_updated);
  UA_Variant_setScalarCopy(&native_input, &native_bytes, &UA_TYPES[UA_TYPES_BYTESTRING]);
  native_output_count = 0;
  native_outputs = NULL;
  assert_int_equal(
      UA_Client_call(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_OBJECT_ID),
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_BYTES_METHOD_ID),
          1,
          &native_input,
          &native_output_count,
          &native_outputs),
      UA_STATUSCODE_GOOD);
  assert_int_equal(native_output_count, 1);
  assert_true(UA_Variant_hasScalarType(&native_outputs[0], &UA_TYPES[UA_TYPES_BYTESTRING]));
  assert_int_equal(((UA_ByteString *)native_outputs[0].data)->length, sizeof(cpkt_facade_bytes_updated));
  assert_memory_equal(
      ((UA_ByteString *)native_outputs[0].data)->data,
      cpkt_facade_bytes_updated,
      sizeof(cpkt_facade_bytes_updated));
  UA_Array_delete(native_outputs, native_output_count, &UA_TYPES[UA_TYPES_VARIANT]);
  UA_Variant_clear(&native_input);

  UA_Variant_init(&native_input);
  native_qualified_name_input = UA_QUALIFIEDNAME(CPKT_OPCUA_TEST_NS, (char *)"facadeQualifiedEcho");
  UA_Variant_setScalarCopy(&native_input, &native_qualified_name_input, &UA_TYPES[UA_TYPES_QUALIFIEDNAME]);
  native_output_count = 0;
  native_outputs = NULL;
  assert_int_equal(
      UA_Client_call(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_OBJECT_ID),
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_QUALIFIED_NAME_METHOD_ID),
          1,
          &native_input,
          &native_output_count,
          &native_outputs),
      UA_STATUSCODE_GOOD);
  assert_int_equal(native_output_count, 1);
  assert_true(UA_Variant_hasScalarType(&native_outputs[0], &UA_TYPES[UA_TYPES_QUALIFIEDNAME]));
  assert_int_equal(((UA_QualifiedName *)native_outputs[0].data)->namespaceIndex, CPKT_OPCUA_TEST_NS);
  assert_int_equal(((UA_QualifiedName *)native_outputs[0].data)->name.length, strlen("facadeQualifiedEcho"));
  assert_memory_equal(
      ((UA_QualifiedName *)native_outputs[0].data)->name.data,
      "facadeQualifiedEcho",
      strlen("facadeQualifiedEcho"));
  UA_Array_delete(native_outputs, native_output_count, &UA_TYPES[UA_TYPES_VARIANT]);
  UA_Variant_clear(&native_input);

  UA_Variant_init(&native_input);
  native_localized_text_input = UA_LOCALIZEDTEXT((char *)"en-US", (char *)"facade localized echo");
  UA_Variant_setScalarCopy(&native_input, &native_localized_text_input, &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]);
  native_output_count = 0;
  native_outputs = NULL;
  assert_int_equal(
      UA_Client_call(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_OBJECT_ID),
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_LOCALIZED_TEXT_METHOD_ID),
          1,
          &native_input,
          &native_output_count,
          &native_outputs),
      UA_STATUSCODE_GOOD);
  assert_int_equal(native_output_count, 1);
  assert_true(UA_Variant_hasScalarType(&native_outputs[0], &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]));
  assert_int_equal(((UA_LocalizedText *)native_outputs[0].data)->locale.length, strlen("en-US"));
  assert_memory_equal(((UA_LocalizedText *)native_outputs[0].data)->locale.data, "en-US", strlen("en-US"));
  assert_int_equal(((UA_LocalizedText *)native_outputs[0].data)->text.length, strlen("facade localized echo"));
  assert_memory_equal(
      ((UA_LocalizedText *)native_outputs[0].data)->text.data,
      "facade localized echo",
      strlen("facade localized echo"));
  UA_Array_delete(native_outputs, native_output_count, &UA_TYPES[UA_TYPES_VARIANT]);
  UA_Variant_clear(&native_input);

  native_output_count = 0;
  native_outputs = NULL;
  assert_int_equal(
      UA_Client_call(
          client,
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_OBJECT_ID),
          UA_NODEID_NUMERIC(CPKT_OPCUA_TEST_NS, CPKT_OPCUA_FACADE_EMPTY_METHOD_ID),
          0,
          NULL,
          &native_output_count,
          &native_outputs),
      UA_STATUSCODE_GOOD);
  assert_int_equal(native_output_count, 1);
  assert_true(UA_Variant_isEmpty(&native_outputs[0]));
  UA_Array_delete(native_outputs, native_output_count, &UA_TYPES[UA_TYPES_VARIANT]);

  assert_int_equal(UA_Client_disconnect(client), UA_STATUSCODE_GOOD);
  UA_Client_delete(client);
  thread.running = 0;
  assert_int_equal(pthread_join(thread_id, NULL), 0);
  c89_line = cpkt_opcua_c89_server_peer_shutdown(server);
  assert_int_equal(c89_line, 0);
  cpkt_opcua_c89_server_peer_free(server);
}

static int cpkt_trigger_facade_server_event(void *user) {
  return cpkt_opcua_c89_server_peer_trigger_event((cpkt_opcua_c89_server_peer *)user);
}

static void cpkt_facade_client_reads_and_writes_facade_server(void **state) {
  unsigned short port;
  cpkt_opcua_c89_server_peer *server;
  struct cpkt_facade_server_thread thread;
  pthread_t thread_id;
  char endpoint[80];
  int c89_line;

  (void)state;

  port = cpkt_test_free_port();
  server = NULL;
  c89_line = cpkt_opcua_c89_server_peer_new(port, &server, endpoint, sizeof(endpoint));
  assert_int_equal(c89_line, 0);
  c89_line = cpkt_opcua_c89_server_peer_start(server);
  assert_int_equal(c89_line, 0);
  thread.server = server;
  thread.running = 1;
  assert_int_equal(pthread_create(&thread_id, NULL, cpkt_facade_server_loop, &thread), 0);

  c89_line = cpkt_opcua_c89_client_exercise_facade_server(endpoint);
  assert_int_equal(c89_line, 0);
  c89_line = cpkt_opcua_c89_client_monitor_facade_server_events(
      endpoint,
      cpkt_trigger_facade_server_event,
      server);
  assert_int_equal(c89_line, 0);

  thread.running = 0;
  assert_int_equal(pthread_join(thread_id, NULL), 0);
  c89_line = cpkt_opcua_c89_server_peer_shutdown(server);
  assert_int_equal(c89_line, 0);
  cpkt_opcua_c89_server_peer_free(server);
}

static cpkt_opcua_status cpkt_native_callback_succeeds(void *native_server, void *user) {
  assert_non_null(native_server);
  assert_non_null(user);
  *(int *)user = 1;
  return 0;
}

static cpkt_opcua_status cpkt_native_callback_fails(void *native_server, void *user) {
  (void)native_server;
  (void)user;
  return UA_STATUSCODE_BADINTERNALERROR;
}

static cpkt_opcua_status cpkt_native_server_pubsub_succeeds(void *native_server, void *user) {
  UA_Server *server;

  assert_non_null(native_server);
  assert_non_null(user);
  server = (UA_Server *)native_server;
  assert_non_null(server);
  assert_true(sizeof(UA_PubSubConnectionConfig) > 0);
  assert_true(sizeof(UA_PubSubConfiguration) > 0);
  *(int *)user = 1;
  return UA_STATUSCODE_GOOD;
}

static cpkt_opcua_status cpkt_native_server_pubsub_components_exist(void *native_server, void *user) {
  UA_Server *server;
  struct cpkt_pubsub_component_ids *ids;
  UA_PubSubConnectionConfig connection_config;
  UA_PublishedDataSetConfig dataset_config;
  UA_WriterGroupConfig writer_group_config;
  UA_DataSetWriterConfig data_set_writer_config;
  UA_DataSetReaderConfig reader_config;

  assert_non_null(native_server);
  assert_non_null(user);
  server = (UA_Server *)native_server;
  ids = (struct cpkt_pubsub_component_ids *)user;
  memset(&connection_config, 0, sizeof(connection_config));
  assert_int_equal(
      UA_Server_getPubSubConnectionConfig(
          server,
          UA_NODEID_NUMERIC(ids->connection_id.namespace_index, (UA_UInt32)ids->connection_id.numeric),
          &connection_config),
      UA_STATUSCODE_GOOD);
  assert_int_equal(connection_config.publisherId.idType, UA_PUBLISHERIDTYPE_UINT32);
  assert_int_equal(connection_config.publisherId.id.uint32, 42);
  UA_PubSubConnectionConfig_clear(&connection_config);

  memset(&dataset_config, 0, sizeof(dataset_config));
  assert_int_equal(
      UA_Server_getPublishedDataSetConfig(
          server,
          UA_NODEID_NUMERIC(
              ids->published_dataset_id.namespace_index,
              (UA_UInt32)ids->published_dataset_id.numeric),
          &dataset_config),
      UA_STATUSCODE_GOOD);
  assert_int_equal(dataset_config.publishedDataSetType, UA_PUBSUB_DATASET_PUBLISHEDITEMS);
  UA_PublishedDataSetConfig_clear(&dataset_config);

  memset(&writer_group_config, 0, sizeof(writer_group_config));
  assert_int_equal(
      UA_Server_getWriterGroupConfig(
          server,
          UA_NODEID_NUMERIC(ids->writer_group_id.namespace_index, (UA_UInt32)ids->writer_group_id.numeric),
          &writer_group_config),
      UA_STATUSCODE_GOOD);
  assert_int_equal(writer_group_config.writerGroupId, 77);
  UA_WriterGroupConfig_clear(&writer_group_config);

  memset(&data_set_writer_config, 0, sizeof(data_set_writer_config));
  assert_int_equal(
      UA_Server_getDataSetWriterConfig(
          server,
          UA_NODEID_NUMERIC(
              ids->data_set_writer_id.namespace_index,
              (UA_UInt32)ids->data_set_writer_id.numeric),
          &data_set_writer_config),
      UA_STATUSCODE_GOOD);
  assert_int_equal(data_set_writer_config.dataSetWriterId, 88);
  UA_DataSetWriterConfig_clear(&data_set_writer_config);

  memset(&reader_config, 0, sizeof(reader_config));
  assert_int_equal(
      UA_Server_getDataSetReaderConfig(
          server,
          UA_NODEID_NUMERIC(ids->data_set_reader_id.namespace_index, (UA_UInt32)ids->data_set_reader_id.numeric),
          &reader_config),
      UA_STATUSCODE_GOOD);
  assert_int_equal(reader_config.writerGroupId, 77);
  assert_int_equal(reader_config.dataSetWriterId, 88);
  UA_DataSetReaderConfig_clear(&reader_config);
  return UA_STATUSCODE_GOOD;
}

static cpkt_opcua_status cpkt_native_server_history_succeeds(void *native_server, void *user) {
  UA_Server *server;

  assert_non_null(native_server);
  assert_non_null(user);
  server = (UA_Server *)native_server;
  assert_non_null(server);
  assert_true(sizeof(UA_HistoryDatabase) > 0);
  *(int *)user = 1;
  return UA_STATUSCODE_GOOD;
}

static cpkt_opcua_status cpkt_native_client_async_succeeds(void *native_client, void *user) {
  UA_Client *client;

  assert_non_null(native_client);
  assert_non_null(user);
  client = (UA_Client *)native_client;
  assert_non_null(client);
  assert_true(sizeof(UA_ClientAsyncReadValueAttributeCallback) > 0);
  assert_true(sizeof(UA_ClientAsyncCallCallback) > 0);
  *(int *)user = 1;
  return UA_STATUSCODE_GOOD;
}

static cpkt_opcua_status cpkt_native_client_history_succeeds(void *native_client, void *user) {
  UA_Client *client;

  assert_non_null(native_client);
  assert_non_null(user);
  client = (UA_Client *)native_client;
  assert_non_null(client);
  assert_true(sizeof(UA_HistoryReadRequest) > 0);
  assert_true(sizeof(UA_HistoryReadResponse) > 0);
  *(int *)user = 1;
  return UA_STATUSCODE_GOOD;
}

static cpkt_opcua_status cpkt_native_server_config_sets_tcp_buffer(void *native_server_config, void *user) {
  UA_ServerConfig *config;

  assert_non_null(native_server_config);
  assert_non_null(user);
  config = (UA_ServerConfig *)native_server_config;
  config->tcpBufSize = 32768;
  *(int *)user = (config->tcpBufSize == 32768) ? 1 : 0;
  return UA_STATUSCODE_GOOD;
}

static cpkt_opcua_status cpkt_native_server_file_config_succeeds(void *native_server_config, void *user) {
  UA_ServerConfig *config;
  UA_StatusCode (*update_from_file)(UA_ServerConfig *, const UA_ByteString);

  assert_non_null(native_server_config);
  assert_non_null(user);
  config = (UA_ServerConfig *)native_server_config;
  assert_non_null(config);
  update_from_file = UA_ServerConfig_updateFromFile;
  assert_non_null(update_from_file);
  *(int *)user = 1;
  return UA_STATUSCODE_GOOD;
}

struct cpkt_json_config_seen {
  const char *application_uri;
  UA_UInt32 tcp_buffer_size;
  int saw_expected;
};

static int cpkt_ua_string_equals_cstr(const UA_String *value, const char *expected) {
  size_t expected_length;

  assert_non_null(value);
  assert_non_null(expected);
  expected_length = strlen(expected);
  return value->length == expected_length &&
         (expected_length == 0 || memcmp(value->data, expected, expected_length) == 0);
}

static cpkt_opcua_status cpkt_native_server_json_config_matches(void *native_server_config, void *user) {
  UA_ServerConfig *config;
  struct cpkt_json_config_seen *seen;

  assert_non_null(native_server_config);
  assert_non_null(user);
  config = (UA_ServerConfig *)native_server_config;
  seen = (struct cpkt_json_config_seen *)user;
  if (cpkt_ua_string_equals_cstr(
          &config->applicationDescription.applicationUri,
          seen->application_uri) &&
      config->tcpBufSize == seen->tcp_buffer_size) {
    seen->saw_expected = 1;
  }
  return UA_STATUSCODE_GOOD;
}

static cpkt_opcua_status cpkt_native_server_security_plugin_succeeds(void *native_server_config, void *user) {
  UA_ServerConfig *config;

  assert_non_null(native_server_config);
  assert_non_null(user);
  config = (UA_ServerConfig *)native_server_config;
  assert_non_null(config);
  assert_true(sizeof(UA_SecurityPolicy) > 0);
  assert_true(sizeof(UA_CertificateGroup) > 0);
  assert_true(sizeof(UA_AccessControl) > 0);
  *(int *)user = 1;
  return UA_STATUSCODE_GOOD;
}

static cpkt_opcua_status cpkt_native_server_config_fails(void *native_server_config, void *user) {
  assert_non_null(native_server_config);
  (void)user;
  return UA_STATUSCODE_BADCONFIGURATIONERROR;
}

static void cpkt_facade_json_config_server_constructors(void **state) {
  static const unsigned char json_bytes[] =
      "{"
      " applicationDescription: {"
      "  applicationUri: \"urn:cpkt:opcua:json-bytes\","
      "  productUri: \"urn:cpkt:opcua:json-product\","
      "  applicationName: { locale: \"en-US\", text: \"cpkt json bytes\" },"
      "  applicationType: 0"
      " },"
      " tcp: { tcpBufSize: 32768 }"
      "}";
  static const char json_file[] =
      "{"
      " applicationDescription: {"
      "  applicationUri: \"urn:cpkt:opcua:json-file\","
      "  productUri: \"urn:cpkt:opcua:json-product\","
      "  applicationName: { locale: \"en-US\", text: \"cpkt json file\" },"
      "  applicationType: 0"
      " },"
      " tcp: { tcpBufSize: 49152 }"
      "}";
  cpkt_opcua_server *server;
  cpkt_opcua_status status;
  struct cpkt_json_config_seen seen;
  char path[128];
  FILE *file;

  (void)state;

  server = NULL;
  assert_int_equal(
      cpkt_opcua_server_new_from_json(&server, json_bytes, sizeof(json_bytes) - 1, &status),
      CPKT_OPCUA_OK);
  assert_non_null(server);
  seen.application_uri = "urn:cpkt:opcua:json-bytes";
  seen.tcp_buffer_size = 32768;
  seen.saw_expected = 0;
  assert_int_equal(
      cpkt_opcua_server_native_config(
          server,
          cpkt_native_server_json_config_matches,
          &seen,
          &status),
      CPKT_OPCUA_OK);
  assert_int_equal(seen.saw_expected, 1);
  cpkt_opcua_server_free(server);

  snprintf(path, sizeof(path), "/tmp/cpkt-opcua-json-config-%ld.json5", (long)getpid());
  file = fopen(path, "wb");
  assert_non_null(file);
  assert_int_equal(fwrite(json_file, 1, sizeof(json_file) - 1, file), sizeof(json_file) - 1);
  assert_int_equal(fclose(file), 0);

  server = NULL;
  assert_int_equal(
      cpkt_opcua_server_new_from_json_file(&server, path, &status),
      CPKT_OPCUA_OK);
  assert_non_null(server);
  seen.application_uri = "urn:cpkt:opcua:json-file";
  seen.tcp_buffer_size = 49152;
  seen.saw_expected = 0;
  assert_int_equal(
      cpkt_opcua_server_native_config(
          server,
          cpkt_native_server_json_config_matches,
          &seen,
          &status),
      CPKT_OPCUA_OK);
  assert_int_equal(seen.saw_expected, 1);
  cpkt_opcua_server_free(server);
  assert_int_equal(remove(path), 0);
}

static cpkt_opcua_status cpkt_native_client_config_sets_timeout(void *native_client_config, void *user) {
  UA_ClientConfig *config;

  assert_non_null(native_client_config);
  assert_non_null(user);
  config = (UA_ClientConfig *)native_client_config;
  config->timeout = 1234;
  *(int *)user = (config->timeout == 1234) ? 1 : 0;
  return UA_STATUSCODE_GOOD;
}

static cpkt_opcua_status cpkt_native_client_security_plugin_succeeds(void *native_client_config, void *user) {
  UA_ClientConfig *config;

  assert_non_null(native_client_config);
  assert_non_null(user);
  config = (UA_ClientConfig *)native_client_config;
  assert_non_null(config);
  assert_true(sizeof(UA_SecurityPolicy) > 0);
  assert_true(sizeof(UA_CertificateGroup) > 0);
  *(int *)user = 1;
  return UA_STATUSCODE_GOOD;
}

static cpkt_opcua_status cpkt_native_client_config_fails(void *native_client_config, void *user) {
  assert_non_null(native_client_config);
  (void)user;
  return UA_STATUSCODE_BADCONFIGURATIONERROR;
}

struct cpkt_login_callback_seen {
  int calls;
  size_t username_length;
  size_t password_length;
};

static cpkt_opcua_status cpkt_facade_login_callback(
    const char *username,
    size_t username_length,
    const unsigned char *password,
    size_t password_length,
    void *user) {
  struct cpkt_login_callback_seen *seen;

  seen = (struct cpkt_login_callback_seen *)user;
  assert_non_null(seen);
  seen->calls += 1;
  seen->username_length = username_length;
  seen->password_length = password_length;
  if (username != NULL && password != NULL &&
      username_length == strlen("callback-user") &&
      password_length == strlen("callback-secret") &&
      memcmp(username, "callback-user", username_length) == 0 &&
      memcmp(password, "callback-secret", password_length) == 0) {
    return UA_STATUSCODE_GOOD;
  }
  return UA_STATUSCODE_BADUSERACCESSDENIED;
}

static void cpkt_facade_username_access_control(void **state) {
  unsigned short port;
  char endpoint[80];
  cpkt_opcua_server *server;
  cpkt_opcua_client *client;
  cpkt_opcua_client *rejected_client;
  struct cpkt_raw_facade_server_thread thread;
  pthread_t thread_id;
  cpkt_opcua_status status;

  (void)state;

  port = cpkt_test_free_port();
  snprintf(endpoint, sizeof(endpoint), "opc.tcp://127.0.0.1:%u", (unsigned)port);
  server = NULL;
  client = NULL;
  rejected_client = NULL;
  assert_int_equal(cpkt_opcua_server_new(&server, port), CPKT_OPCUA_OK);
  assert_int_equal(
      cpkt_opcua_server_set_access_control(server, 0, "cpkt-user", "cpkt-secret", &status),
      CPKT_OPCUA_OK);
  assert_int_equal(status, UA_STATUSCODE_GOOD);
  assert_int_equal(cpkt_opcua_server_startup(server, &status), CPKT_OPCUA_OK);
  thread.server = server;
  thread.running = 1;
  assert_int_equal(pthread_create(&thread_id, NULL, cpkt_raw_facade_server_loop, &thread), 0);

  assert_int_equal(
      cpkt_facade_client_connect_username_with_retry(
          endpoint,
          "cpkt-user",
          "cpkt-secret",
          &client,
          &status),
      CPKT_OPCUA_OK);
  assert_non_null(client);
  assert_int_equal(cpkt_opcua_client_disconnect(client, &status), CPKT_OPCUA_OK);
  cpkt_opcua_client_free(client);

  assert_int_equal(cpkt_opcua_client_new(&rejected_client), CPKT_OPCUA_OK);
  assert_int_equal(cpkt_opcua_client_connect(rejected_client, endpoint, &status), CPKT_OPCUA_ERR_UPSTREAM);
  cpkt_opcua_client_free(rejected_client);
  rejected_client = NULL;

  assert_int_equal(cpkt_opcua_client_new(&rejected_client), CPKT_OPCUA_OK);
  assert_int_equal(
      cpkt_opcua_client_connect_username(rejected_client, endpoint, "cpkt-user", "wrong", &status),
      CPKT_OPCUA_ERR_UPSTREAM);
  cpkt_opcua_client_free(rejected_client);

  thread.running = 0;
  assert_int_equal(pthread_join(thread_id, NULL), 0);
  assert_int_equal(cpkt_opcua_server_shutdown(server, &status), CPKT_OPCUA_OK);
  cpkt_opcua_server_free(server);
}

static void cpkt_facade_callback_access_control(void **state) {
  unsigned short port;
  char endpoint[80];
  cpkt_opcua_server *server;
  cpkt_opcua_client *client;
  cpkt_opcua_client *rejected_client;
  struct cpkt_raw_facade_server_thread thread;
  struct cpkt_login_callback_seen seen;
  pthread_t thread_id;
  cpkt_opcua_status status;

  (void)state;

  port = cpkt_test_free_port();
  snprintf(endpoint, sizeof(endpoint), "opc.tcp://127.0.0.1:%u", (unsigned)port);
  server = NULL;
  client = NULL;
  rejected_client = NULL;
  memset(&seen, 0, sizeof(seen));
  assert_int_equal(cpkt_opcua_server_new(&server, port), CPKT_OPCUA_OK);
  assert_int_equal(
      cpkt_opcua_server_set_access_control_callback(
          server,
          0,
          cpkt_facade_login_callback,
          &seen,
          &status),
      CPKT_OPCUA_OK);
  assert_int_equal(status, UA_STATUSCODE_GOOD);
  assert_int_equal(cpkt_opcua_server_startup(server, &status), CPKT_OPCUA_OK);
  thread.server = server;
  thread.running = 1;
  assert_int_equal(pthread_create(&thread_id, NULL, cpkt_raw_facade_server_loop, &thread), 0);

  assert_int_equal(
      cpkt_facade_client_connect_username_with_retry(
          endpoint,
          "callback-user",
          "callback-secret",
          &client,
          &status),
      CPKT_OPCUA_OK);
  assert_non_null(client);
  assert_true(seen.calls >= 1);
  assert_int_equal(seen.username_length, strlen("callback-user"));
  assert_int_equal(seen.password_length, strlen("callback-secret"));
  assert_int_equal(cpkt_opcua_client_disconnect(client, &status), CPKT_OPCUA_OK);
  cpkt_opcua_client_free(client);

  assert_int_equal(cpkt_opcua_client_new(&rejected_client), CPKT_OPCUA_OK);
  assert_int_equal(
      cpkt_opcua_client_connect_username(rejected_client, endpoint, "callback-user", "wrong", &status),
      CPKT_OPCUA_ERR_UPSTREAM);
  cpkt_opcua_client_free(rejected_client);

  thread.running = 0;
  assert_int_equal(pthread_join(thread_id, NULL), 0);
  assert_int_equal(cpkt_opcua_server_shutdown(server, &status), CPKT_OPCUA_OK);
  cpkt_opcua_server_free(server);
}

static void cpkt_facade_failure_modes(void **state) {
  cpkt_opcua_server *server;
  cpkt_opcua_server *json_server;
  cpkt_opcua_client *client;
  cpkt_opcua_value value;
  cpkt_opcua_value out;
  cpkt_opcua_data_value data_value;
  cpkt_opcua_node_id parsed_node;
  cpkt_opcua_expanded_node_id parsed_expanded_node;
  cpkt_opcua_expanded_node_id expected_expanded_node;
  cpkt_opcua_status status;
  cpkt_opcua_browse_options browse_options;
  cpkt_opcua_browse_path_element browse_path_elements[1];
  cpkt_opcua_string_view event_field_names[1];
  cpkt_opcua_monitor_options monitor_options;
  cpkt_opcua_mqtt_connection_options mqtt_options;
  cpkt_opcua_pubsub_writer_group_options writer_group_options;
  cpkt_opcua_pubsub_data_set_writer_options data_set_writer_options;
  cpkt_opcua_pubsub_reader_group_options reader_group_options;
  cpkt_opcua_pubsub_data_set_reader_options data_set_reader_options;
  struct cpkt_pubsub_component_ids pubsub_ids;
  cpkt_opcua_server_event *event;
  unsigned char parsed_guid[16];
  const char *parsed_locale;
  const char *parsed_text;
  char node_id_buffer[8];
  char node_id_large_buffer[128];
  char endpoint_buffer[80];
  unsigned char byte_node_buffer[8];
  unsigned char bad_certificate[1];
  unsigned char bad_private_key[1];
  unsigned char bytes_value[2];
  cpkt_opcua_byte_string_view bad_trust_list[1];
  size_t required;
  size_t parsed_length;
  size_t parsed_text_length;
  cpkt_opcua_request_id request_id;
  unsigned short parsed_namespace_index;
  long value_rank;
  int callback_seen;
  struct cpkt_native_value_seen native_value_seen;
  int method_input_types[1];
  int method_output_types[2];

  (void)state;

  json_server = NULL;
  assert_int_equal(cpkt_opcua_server_new(NULL, 0), CPKT_OPCUA_ERR_ARG);
  assert_int_equal(cpkt_opcua_client_new(NULL), CPKT_OPCUA_ERR_ARG);
  assert_non_null(cpkt_opcua_open62541_version());
  assert_non_null(cpkt_opcua_facade_version());
  event = NULL;
  assert_true(cpkt_opcua_status_name(UA_STATUSCODE_GOOD)[0] != '\0');
  assert_string_equal(cpkt_opcua_result_string(CPKT_OPCUA_ERR_ARG), "invalid argument");
  assert_int_equal(
      cpkt_opcua_node_id_print(
          cpkt_opcua_node_id_string(1, NULL),
          node_id_buffer,
          sizeof(node_id_buffer),
          &required),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_node_id_print(
          cpkt_opcua_node_id_numeric(1, 12345),
          node_id_buffer,
          sizeof(node_id_buffer),
          &required),
      CPKT_OPCUA_ERR_RANGE);
  assert_true(required > sizeof(node_id_buffer));
  assert_int_equal(
      cpkt_opcua_node_id_parse("ns=1;x=2", &parsed_node, node_id_buffer, sizeof(node_id_buffer), &required),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_node_id_parse(
          "ns=1;s=too-large",
          &parsed_node,
          node_id_buffer,
          sizeof(node_id_buffer),
          &required),
      CPKT_OPCUA_ERR_RANGE);
  assert_true(required > sizeof(node_id_buffer));
  assert_int_equal(
      cpkt_opcua_node_id_print(
          cpkt_opcua_node_id_null(),
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required),
      CPKT_OPCUA_OK);
  assert_string_equal(node_id_large_buffer, "i=0");
  assert_int_equal(
      cpkt_opcua_node_id_parse("i=0", &parsed_node, node_id_large_buffer, sizeof(node_id_large_buffer), &required),
      CPKT_OPCUA_OK);
  assert_int_equal(parsed_node.identifier_type, CPKT_OPCUA_NODE_ID_NULL);
  assert_int_equal(
      cpkt_opcua_node_id_print(
          cpkt_opcua_node_id_guid(1, cpkt_native_guid_node_id),
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required),
      CPKT_OPCUA_OK);
  assert_string_equal(node_id_large_buffer, "ns=1;g=12345678-9abc-def0-1234-56789abcdef0");
  assert_int_equal(
      cpkt_opcua_node_id_parse(
          "ns=1;g=12345678-9abc-def0-1234-56789abcdef0",
          &parsed_node,
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required),
      CPKT_OPCUA_OK);
  assert_true(cpkt_opcua_node_id_equal(parsed_node, cpkt_opcua_node_id_guid(1, cpkt_native_guid_node_id)));
  assert_int_equal(
      cpkt_opcua_node_id_parse("ns=1;b=3q2+", &parsed_node, (char *)byte_node_buffer, 2, &required),
      CPKT_OPCUA_ERR_RANGE);
  assert_int_equal(required, 3);
  assert_int_equal(
      cpkt_opcua_node_id_parse(
          "ns=1;b=3q2+",
          &parsed_node,
          (char *)byte_node_buffer,
          sizeof(byte_node_buffer),
          &required),
      CPKT_OPCUA_OK);
  assert_int_equal(required, 3);
  assert_true(
      cpkt_opcua_node_id_equal(
          parsed_node,
          cpkt_opcua_node_id_byte_string(1, cpkt_native_byte_node_id, sizeof(cpkt_native_byte_node_id))));
  assert_int_equal(
      cpkt_opcua_node_id_print(parsed_node, node_id_large_buffer, sizeof(node_id_large_buffer), &required),
      CPKT_OPCUA_OK);
  assert_string_equal(node_id_large_buffer, "ns=1;b=3q2+");
  assert_int_equal(
      cpkt_opcua_guid_print(cpkt_native_guid_node_id, node_id_buffer, sizeof(node_id_buffer), &required),
      CPKT_OPCUA_ERR_RANGE);
  assert_int_equal(required, 37);
  assert_int_equal(
      cpkt_opcua_guid_print(
          cpkt_native_guid_node_id,
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required),
      CPKT_OPCUA_OK);
  assert_string_equal(node_id_large_buffer, "12345678-9abc-def0-1234-56789abcdef0");
  assert_int_equal(cpkt_opcua_guid_parse(node_id_large_buffer, parsed_guid), CPKT_OPCUA_OK);
  assert_memory_equal(parsed_guid, cpkt_native_guid_node_id, sizeof(parsed_guid));
  assert_int_equal(cpkt_opcua_guid_parse("not-a-guid", parsed_guid), CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_qualified_name_print(
          CPKT_OPCUA_TEST_NS,
          "facadeQualified",
          strlen("facadeQualified"),
          node_id_buffer,
          sizeof(node_id_buffer),
          &required),
      CPKT_OPCUA_ERR_RANGE);
  assert_true(required > sizeof(node_id_buffer));
  assert_int_equal(
      cpkt_opcua_qualified_name_print(
          CPKT_OPCUA_TEST_NS,
          "facadeQualified",
          strlen("facadeQualified"),
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required),
      CPKT_OPCUA_OK);
  assert_string_equal(node_id_large_buffer, "ns=1;q=facadeQualified");
  assert_int_equal(
      cpkt_opcua_qualified_name_parse(
          node_id_large_buffer,
          &parsed_namespace_index,
          node_id_buffer,
          sizeof(node_id_buffer),
          &parsed_length,
          &required),
      CPKT_OPCUA_ERR_RANGE);
  assert_true(required > sizeof(node_id_buffer));
  assert_int_equal(
      cpkt_opcua_qualified_name_parse(
          node_id_large_buffer,
          &parsed_namespace_index,
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &parsed_length,
          &required),
      CPKT_OPCUA_OK);
  assert_int_equal(parsed_namespace_index, CPKT_OPCUA_TEST_NS);
  assert_int_equal(parsed_length, strlen("facadeQualified"));
  assert_string_equal(node_id_large_buffer, "facadeQualified");
  assert_int_equal(
      cpkt_opcua_qualified_name_parse(
          "ns=1;s=not-qualified",
          &parsed_namespace_index,
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &parsed_length,
          &required),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_localized_text_print(
          "en-US",
          strlen("en-US"),
          "facade text",
          strlen("facade text"),
          node_id_buffer,
          sizeof(node_id_buffer),
          &required),
      CPKT_OPCUA_ERR_RANGE);
  assert_true(required > sizeof(node_id_buffer));
  assert_int_equal(
      cpkt_opcua_localized_text_print(
          "en-US",
          strlen("en-US"),
          "facade text",
          strlen("facade text"),
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required),
      CPKT_OPCUA_OK);
  assert_string_equal(node_id_large_buffer, "locale=en-US;text=facade text");
  assert_int_equal(
      cpkt_opcua_localized_text_parse(
          node_id_large_buffer,
          node_id_buffer,
          sizeof(node_id_buffer),
          &parsed_locale,
          &parsed_length,
          &parsed_text,
          &parsed_text_length,
          &required),
      CPKT_OPCUA_ERR_RANGE);
  assert_true(required > sizeof(node_id_buffer));
  assert_int_equal(
      cpkt_opcua_localized_text_parse(
          node_id_large_buffer,
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &parsed_locale,
          &parsed_length,
          &parsed_text,
          &parsed_text_length,
          &required),
      CPKT_OPCUA_OK);
  assert_int_equal(parsed_length, strlen("en-US"));
  assert_string_equal(parsed_locale, "en-US");
  assert_int_equal(parsed_text_length, strlen("facade text"));
  assert_string_equal(parsed_text, "facade text");
  assert_int_equal(
      cpkt_opcua_localized_text_parse(
          "locale=en-US;bad=facade text",
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &parsed_locale,
          &parsed_length,
          &parsed_text,
          &parsed_text_length,
          &required),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_node_id_print(
          cpkt_opcua_node_id_byte_string(2, NULL, 0),
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required),
      CPKT_OPCUA_OK);
  assert_string_equal(node_id_large_buffer, "ns=2;b=");
  assert_int_equal(
      cpkt_opcua_node_id_parse("ns=2;b=", &parsed_node, NULL, 0, &required),
      CPKT_OPCUA_OK);
  assert_int_equal(required, 0);
  assert_int_equal(parsed_node.identifier_type, CPKT_OPCUA_NODE_ID_BYTE_STRING);
  assert_int_equal(parsed_node.namespace_index, 2);
  assert_int_equal(parsed_node.byte_string_length, 0);
  assert_null(parsed_node.byte_string);
  assert_int_equal(
      cpkt_opcua_expanded_node_id_print(
          cpkt_opcua_expanded_node_id_uri(
              "urn:expanded",
              strlen("urn:expanded"),
              cpkt_opcua_node_id_string(2, "expandedNode")),
          node_id_buffer,
          sizeof(node_id_buffer),
          &required),
      CPKT_OPCUA_ERR_RANGE);
  assert_true(required > sizeof(node_id_buffer));
  assert_int_equal(
      cpkt_opcua_expanded_node_id_print(
          cpkt_opcua_expanded_node_id_server_uri(
              3,
              "urn:expanded",
              strlen("urn:expanded"),
              cpkt_opcua_node_id_string(2, "expandedNode")),
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required),
      CPKT_OPCUA_OK);
  assert_string_equal(node_id_large_buffer, "svr=3;nsu=urn:expanded;ns=2;s=expandedNode");
  assert_int_equal(
      cpkt_opcua_expanded_node_id_parse(
          "svr=3;nsu=urn:expanded;ns=2;s=expandedNode",
          &parsed_expanded_node,
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required),
      CPKT_OPCUA_OK);
  assert_int_equal(required, strlen("urn:expanded") + 1 + strlen("expandedNode") + 1);
  expected_expanded_node = cpkt_opcua_expanded_node_id_server_uri(
      3,
      "urn:expanded",
      strlen("urn:expanded"),
      cpkt_opcua_node_id_string(2, "expandedNode"));
  assert_true(cpkt_opcua_expanded_node_id_equal(parsed_expanded_node, expected_expanded_node));
  assert_string_equal(parsed_expanded_node.namespace_uri, "urn:expanded");
  assert_string_equal(parsed_expanded_node.node_id.string, "expandedNode");
  assert_int_equal(
      cpkt_opcua_expanded_node_id_print(
          cpkt_opcua_expanded_node_id_server(7, cpkt_opcua_node_id_numeric(2, 42)),
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required),
      CPKT_OPCUA_OK);
  assert_string_equal(node_id_large_buffer, "svr=7;ns=2;i=42");
  assert_int_equal(
      cpkt_opcua_expanded_node_id_parse(
          "svr=7;ns=2;i=42",
          &parsed_expanded_node,
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required),
      CPKT_OPCUA_OK);
  expected_expanded_node = cpkt_opcua_expanded_node_id_server(7, cpkt_opcua_node_id_numeric(2, 42));
  assert_true(cpkt_opcua_expanded_node_id_equal(parsed_expanded_node, expected_expanded_node));
  assert_int_equal(parsed_expanded_node.server_index, 7);
  assert_null(parsed_expanded_node.namespace_uri);
  assert_int_equal(
      cpkt_opcua_expanded_node_id_parse(
          "nsu=urn:expanded;ns=2;s=expandedNode",
          &parsed_expanded_node,
          node_id_buffer,
          sizeof(node_id_buffer),
          &required),
      CPKT_OPCUA_ERR_RANGE);
  assert_true(required > sizeof(node_id_buffer));
  assert_int_equal(
      cpkt_opcua_expanded_node_id_parse(
          "svr=x;i=1",
          &parsed_expanded_node,
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_expanded_node_id_print(
          cpkt_opcua_expanded_node_id_uri(NULL, 1, cpkt_opcua_node_id_numeric(1, 1)),
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required),
      CPKT_OPCUA_ERR_ARG);
  bytes_value[0] = 0x01;
  bytes_value[1] = 0x02;
  cpkt_opcua_value_byte_string(&value, bytes_value, sizeof(bytes_value));
  assert_int_equal(value.type, CPKT_OPCUA_VALUE_BYTE_STRING);
  assert_int_equal(value.bytes_length, sizeof(bytes_value));
  assert_memory_equal(value.bytes_value, bytes_value, sizeof(bytes_value));

  assert_int_equal(cpkt_opcua_server_new(&server, cpkt_test_free_port()), CPKT_OPCUA_OK);
  assert_int_equal(cpkt_opcua_server_set_endpoint(NULL, "127.0.0.1", cpkt_test_free_port()), CPKT_OPCUA_ERR_ARG);
  assert_int_equal(cpkt_opcua_server_set_endpoint(server, NULL, cpkt_test_free_port()), CPKT_OPCUA_ERR_ARG);
  assert_int_equal(cpkt_opcua_server_set_endpoint(server, "", cpkt_test_free_port()), CPKT_OPCUA_ERR_ARG);
  assert_int_equal(cpkt_opcua_server_set_endpoint(server, "127.0.0.1", cpkt_test_free_port()), CPKT_OPCUA_OK);
  assert_int_equal(
      cpkt_opcua_server_endpoint_url(server, endpoint_buffer, sizeof(endpoint_buffer), &required),
      CPKT_OPCUA_OK);
  assert_true(strncmp(endpoint_buffer, "opc.tcp://127.0.0.1:", strlen("opc.tcp://127.0.0.1:")) == 0);
  assert_int_equal(cpkt_opcua_client_new(&client), CPKT_OPCUA_OK);
  cpkt_opcua_value_integer(&value, 41);
  assert_int_equal(
      cpkt_opcua_server_add_variable(
          server,
          cpkt_opcua_node_id_numeric(1, 5315),
          "nativeReadValue",
          "Native Read Value",
          &value,
          &status),
      CPKT_OPCUA_OK);
  cpkt_opcua_mqtt_connection_options_default(&mqtt_options);
  mqtt_options.name = "facade mqtt publisher";
  mqtt_options.broker_host = "127.0.0.1";
  mqtt_options.topic = "cpkt/opcua/facade";
  mqtt_options.publisher_id = 42;
  mqtt_options.validate_only = 1;
  mqtt_options.enabled = 0;
  assert_int_equal(
      cpkt_opcua_server_add_mqtt_pubsub_connection(
          server,
          &mqtt_options,
          &pubsub_ids.connection_id,
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required,
          &status),
      CPKT_OPCUA_OK);
  assert_int_equal(pubsub_ids.connection_id.identifier_type, CPKT_OPCUA_NODE_ID_NUMERIC);
  assert_int_equal(
      cpkt_opcua_server_add_published_dataset(
          server,
          "facade published data set",
          &pubsub_ids.published_dataset_id,
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required,
          &status),
      CPKT_OPCUA_OK);
  assert_int_equal(
      cpkt_opcua_server_add_published_variable(
          server,
          pubsub_ids.published_dataset_id,
          cpkt_opcua_node_id_numeric(1, 5315),
          "facadeField",
          &pubsub_ids.field_id,
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required,
          &status),
      CPKT_OPCUA_OK);
  cpkt_opcua_pubsub_writer_group_options_default(&writer_group_options);
  writer_group_options.name = "facade writer group";
  writer_group_options.writer_group_id = 77;
  writer_group_options.publishing_interval_ms = 100.0;
  writer_group_options.enabled = 0;
  assert_int_equal(
      cpkt_opcua_server_add_pubsub_writer_group(
          server,
          pubsub_ids.connection_id,
          &writer_group_options,
          &pubsub_ids.writer_group_id,
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required,
          &status),
      CPKT_OPCUA_OK);
  cpkt_opcua_pubsub_data_set_writer_options_default(&data_set_writer_options);
  data_set_writer_options.name = "facade data set writer";
  data_set_writer_options.data_set_writer_id = 88;
  data_set_writer_options.enabled = 0;
  assert_int_equal(
      cpkt_opcua_server_add_pubsub_data_set_writer(
          server,
          pubsub_ids.writer_group_id,
          pubsub_ids.published_dataset_id,
          &data_set_writer_options,
          &pubsub_ids.data_set_writer_id,
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required,
          &status),
      CPKT_OPCUA_OK);
  cpkt_opcua_pubsub_reader_group_options_default(&reader_group_options);
  reader_group_options.name = "facade reader group";
  reader_group_options.enabled = 0;
  assert_int_equal(
      cpkt_opcua_server_add_pubsub_reader_group(
          server,
          pubsub_ids.connection_id,
          &reader_group_options,
          &pubsub_ids.reader_group_id,
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required,
          &status),
      CPKT_OPCUA_OK);
  cpkt_opcua_pubsub_data_set_reader_options_default(&data_set_reader_options);
  data_set_reader_options.name = "facade data set reader";
  data_set_reader_options.publisher_id = 42;
  data_set_reader_options.writer_group_id = 77;
  data_set_reader_options.data_set_writer_id = 88;
  data_set_reader_options.enabled = 0;
  assert_int_equal(
      cpkt_opcua_server_add_pubsub_data_set_reader(
          server,
          pubsub_ids.reader_group_id,
          &data_set_reader_options,
          &pubsub_ids.data_set_reader_id,
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required,
          &status),
      CPKT_OPCUA_OK);
  assert_int_equal(
      cpkt_opcua_server_pubsub_native(
          server,
          cpkt_native_server_pubsub_components_exist,
          &pubsub_ids),
      CPKT_OPCUA_OK);
  assert_int_equal(
      cpkt_opcua_server_add_mqtt_pubsub_connection(
          server,
          NULL,
          &pubsub_ids.connection_id,
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_add_pubsub_writer_group(
          server,
          cpkt_opcua_node_id_numeric(1, 99999),
          &writer_group_options,
          &pubsub_ids.writer_group_id,
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required,
          &status),
      CPKT_OPCUA_ERR_UPSTREAM);
  native_value_seen.saw_expected_variant = 0;
  native_value_seen.saw_expected_data_value = 0;
  assert_int_equal(
      cpkt_opcua_server_read_native_variant(
          server,
          cpkt_opcua_node_id_numeric(1, 5315),
          cpkt_native_integer_variant_callback,
          &native_value_seen,
          &status),
      CPKT_OPCUA_OK);
  assert_int_equal(native_value_seen.saw_expected_variant, 1);
  assert_int_equal(
      cpkt_opcua_server_read_native_data_value(
          server,
          cpkt_opcua_node_id_numeric(1, 5315),
          cpkt_native_integer_data_value_callback,
          &native_value_seen,
          &status),
      CPKT_OPCUA_OK);
  assert_int_equal(native_value_seen.saw_expected_data_value, 1);
  assert_int_equal(
      cpkt_opcua_server_read_data_value(
          server,
          cpkt_opcua_node_id_numeric(1, 5315),
          &data_value,
          NULL,
          0,
          NULL,
          &status),
      CPKT_OPCUA_OK);
  assert_int_equal(data_value.has_value, 1);
  assert_int_equal(data_value.value.type, CPKT_OPCUA_VALUE_INTEGER);
  assert_int_equal(data_value.value.integer_value, 41);
  assert_int_equal(
      cpkt_opcua_server_read_native_variant(
          server,
          cpkt_opcua_node_id_numeric(1, 5315),
          cpkt_native_callback_returns_error,
          NULL,
          &status),
      CPKT_OPCUA_ERR_CALLBACK);
  assert_int_equal(
      cpkt_opcua_server_read_native_data_value(
          server,
          cpkt_opcua_node_id_numeric(1, 5315),
          cpkt_native_callback_returns_error,
          NULL,
          &status),
      CPKT_OPCUA_ERR_CALLBACK);
  assert_int_equal(
      cpkt_opcua_server_read_native_variant(
          server,
          cpkt_opcua_node_id_numeric(1, 5315),
          NULL,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_data_value(
          server,
          cpkt_opcua_node_id_numeric(1, 5315),
          NULL,
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_data_value(
          server,
          cpkt_opcua_node_id_string(1, NULL),
          &data_value,
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_read_native_variant(
          client,
          cpkt_opcua_node_id_numeric(1, 5315),
          cpkt_native_integer_variant_callback,
          &native_value_seen,
          &status),
      CPKT_OPCUA_ERR_UPSTREAM);
  assert_int_equal(
      cpkt_opcua_client_read_native_variant(
          client,
          cpkt_opcua_node_id_numeric(1, 5315),
          NULL,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_read_native_data_value(
          client,
          cpkt_opcua_node_id_numeric(1, 5315),
          cpkt_native_integer_data_value_callback,
          &native_value_seen,
          &status),
      CPKT_OPCUA_ERR_UPSTREAM);
  assert_int_equal(
      cpkt_opcua_client_read_native_data_value(
          client,
          cpkt_opcua_node_id_numeric(1, 5315),
          NULL,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_read_data_value(
          client,
          cpkt_opcua_node_id_numeric(1, 5315),
          NULL,
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_read_data_value(
          client,
          cpkt_opcua_node_id_string(1, NULL),
          &data_value,
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_set_application_identity(server, NULL, "product", "name"),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_new_from_json(NULL, (const unsigned char *)"{}", 2, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_new_from_json(&json_server, NULL, 2, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_null(json_server);
  assert_int_equal(
      cpkt_opcua_server_new_from_json(&json_server, (const unsigned char *)"{}", 0, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_null(json_server);
  assert_int_equal(
      cpkt_opcua_server_new_from_json(&json_server, (const unsigned char *)"{ invalid", 9, &status),
      CPKT_OPCUA_ERR_UPSTREAM);
  assert_null(json_server);
  assert_int_equal(status, UA_STATUSCODE_BADCONFIGURATIONERROR);
  assert_int_equal(
      cpkt_opcua_server_new_from_json_file(NULL, "/tmp/missing-cpkt-opcua-json-config.json5", &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_new_from_json_file(&json_server, NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_null(json_server);
  assert_int_equal(
      cpkt_opcua_server_new_from_json_file(&json_server, "", &status),
      CPKT_OPCUA_ERR_ARG);
  assert_null(json_server);
  assert_int_equal(
      cpkt_opcua_server_new_from_json_file(&json_server, "/tmp/missing-cpkt-opcua-json-config.json5", &status),
      CPKT_OPCUA_ERR_UPSTREAM);
  assert_null(json_server);
  assert_int_equal(status, UA_STATUSCODE_BADNOTFOUND);
  assert_int_equal(
      cpkt_opcua_server_native_config(server, NULL, NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  callback_seen = 0;
  assert_int_equal(
      cpkt_opcua_server_native_config(
          server,
          cpkt_native_server_config_sets_tcp_buffer,
          &callback_seen,
          &status),
      CPKT_OPCUA_OK);
  assert_int_equal(callback_seen, 1);
  assert_int_equal(status, UA_STATUSCODE_GOOD);
  callback_seen = 0;
  assert_int_equal(
      cpkt_opcua_server_file_config_native_config(
          server,
          cpkt_native_server_file_config_succeeds,
          &callback_seen,
          &status),
      CPKT_OPCUA_OK);
  assert_int_equal(callback_seen, 1);
  assert_int_equal(status, UA_STATUSCODE_GOOD);
  assert_int_equal(
      cpkt_opcua_server_file_config_native_config(server, NULL, NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  callback_seen = 0;
  assert_int_equal(
      cpkt_opcua_server_security_plugin_native_config(
          server,
          cpkt_native_server_security_plugin_succeeds,
          &callback_seen,
          &status),
      CPKT_OPCUA_OK);
  assert_int_equal(callback_seen, 1);
  assert_int_equal(status, UA_STATUSCODE_GOOD);
  assert_int_equal(
      cpkt_opcua_server_security_plugin_native_config(server, NULL, NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_native_config(server, cpkt_native_server_config_fails, NULL, &status),
      CPKT_OPCUA_ERR_UPSTREAM);
  assert_int_equal(status, UA_STATUSCODE_BADCONFIGURATIONERROR);
  bad_certificate[0] = 0;
  bad_private_key[0] = 0;
  bad_trust_list[0].data = NULL;
  bad_trust_list[0].length = 1;
  assert_int_equal(
      cpkt_opcua_server_set_default_security(
          server,
          0,
          NULL,
          0,
          bad_private_key,
          sizeof(bad_private_key),
          NULL,
          0,
          NULL,
          0,
          NULL,
          0,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_set_default_security(
          server,
          0,
          bad_certificate,
          sizeof(bad_certificate),
          bad_private_key,
          sizeof(bad_private_key),
          bad_trust_list,
          1,
          NULL,
          0,
          NULL,
          0,
          &status),
      CPKT_OPCUA_ERR_ARG);
  callback_seen = 0;
  assert_int_equal(
      cpkt_opcua_client_native_config(
          client,
          cpkt_native_client_config_sets_timeout,
          &callback_seen,
          &status),
      CPKT_OPCUA_OK);
  assert_int_equal(callback_seen, 1);
  assert_int_equal(status, UA_STATUSCODE_GOOD);
  callback_seen = 0;
  assert_int_equal(
      cpkt_opcua_client_security_plugin_native_config(
          client,
          cpkt_native_client_security_plugin_succeeds,
          &callback_seen,
          &status),
      CPKT_OPCUA_OK);
  assert_int_equal(callback_seen, 1);
  assert_int_equal(status, UA_STATUSCODE_GOOD);
  assert_int_equal(
      cpkt_opcua_client_security_plugin_native_config(client, NULL, NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  callback_seen = 0;
  assert_int_equal(
      cpkt_opcua_client_async_native(client, cpkt_native_client_async_succeeds, &callback_seen),
      CPKT_OPCUA_OK);
  assert_int_equal(callback_seen, 1);
  assert_int_equal(
      cpkt_opcua_client_async_native(client, NULL, NULL),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_read_async(
          client,
          cpkt_opcua_node_id_numeric(1, 5315),
          NULL,
          NULL,
          &request_id,
          NULL,
          0,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_write_async(
          client,
          cpkt_opcua_node_id_string(1, NULL),
          &value,
          cpkt_async_status_callback,
          NULL,
          &request_id,
          &status),
      CPKT_OPCUA_ERR_ARG);
#if ULONG_MAX > 4294967295UL
  cpkt_opcua_value_uint64(&value, ULONG_MAX, 0);
  assert_int_equal(
      cpkt_opcua_client_write_async(
          client,
          cpkt_opcua_node_id_numeric(1, 5315),
          &value,
          cpkt_async_status_callback,
          NULL,
          &request_id,
          &status),
      CPKT_OPCUA_ERR_RANGE);
#endif
  assert_int_equal(
      cpkt_opcua_client_browse_children_async(
          client,
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          NULL,
          NULL,
          cpkt_async_browse_done_callback,
          NULL,
          &request_id,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_call_method_async(
          client,
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          cpkt_opcua_node_id_numeric(1, 5315),
          NULL,
          0,
          1,
          cpkt_async_call_callback,
          NULL,
          &request_id,
          NULL,
          NULL,
          NULL,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_add_object_async(
          client,
          cpkt_opcua_node_id_numeric(1, 5400),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          "badAsyncObject",
          NULL,
          cpkt_async_node_callback,
          NULL,
          &request_id,
          NULL,
          NULL,
          0,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  callback_seen = 0;
  assert_int_equal(
      cpkt_opcua_client_history_native(client, cpkt_native_client_history_succeeds, &callback_seen),
      CPKT_OPCUA_OK);
  assert_int_equal(callback_seen, 1);
  assert_int_equal(
      cpkt_opcua_client_history_native(client, NULL, NULL),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_native_config(client, cpkt_native_client_config_fails, NULL, &status),
      CPKT_OPCUA_ERR_UPSTREAM);
  assert_int_equal(status, UA_STATUSCODE_BADCONFIGURATIONERROR);
  assert_int_equal(
      cpkt_opcua_client_native_config(client, NULL, NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_set_default_encryption(
          client,
          NULL,
          0,
          bad_private_key,
          sizeof(bad_private_key),
          NULL,
          0,
          NULL,
          0,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_set_default_encryption(
          client,
          bad_certificate,
          sizeof(bad_certificate),
          bad_private_key,
          sizeof(bad_private_key),
          bad_trust_list,
          1,
          NULL,
          0,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_set_access_control(server, 1, "user", NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_set_access_control_callback(server, 1, NULL, NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_set_access_control(server, 1, NULL, NULL, &status),
      CPKT_OPCUA_OK);
  assert_int_equal(status, UA_STATUSCODE_GOOD);
  assert_int_equal(
      cpkt_opcua_server_set_application_identity(server, "urn:test:app", "urn:test:product", "test app"),
      CPKT_OPCUA_OK);
  assert_int_equal(cpkt_opcua_server_startup(server, &status), CPKT_OPCUA_OK);
  assert_int_equal(
      cpkt_opcua_server_native_config(
          server,
          cpkt_native_server_config_sets_tcp_buffer,
          &callback_seen,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_file_config_native_config(
          server,
          cpkt_native_server_file_config_succeeds,
          &callback_seen,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_set_application_identity(server, "urn:test:late", "urn:test:late", "late"),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(cpkt_opcua_server_set_endpoint(server, "127.0.0.1", cpkt_test_free_port()), CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_set_access_control(server, 1, NULL, NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_set_access_control_callback(
          server,
          1,
          cpkt_facade_login_callback,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(cpkt_opcua_server_shutdown(server, &status), CPKT_OPCUA_OK);

  cpkt_opcua_value_clear(&value);
  assert_int_equal(
      cpkt_opcua_server_add_variable(
          server,
          cpkt_opcua_node_id_numeric(1, 5301),
          "emptyValue",
          "Empty Value",
          &value,
          &status),
      CPKT_OPCUA_OK);
  assert_int_equal(
      cpkt_opcua_server_read(
          server,
          cpkt_opcua_node_id_numeric(1, 5301),
          &out,
          NULL,
          0,
          NULL,
          &status),
      CPKT_OPCUA_OK);
  assert_int_equal(out.type, CPKT_OPCUA_VALUE_EMPTY);
  cpkt_opcua_value_uint64(&value, 0x12345678UL, 0x9abcdef0UL);
  cpkt_opcua_value_clear(&value);
  assert_int_equal(value.type, CPKT_OPCUA_VALUE_EMPTY);
  assert_int_equal(value.uint64_value.high32, 0);
  assert_int_equal(value.uint64_value.low32, 0);
  assert_null(value.uint64_array_values);
  assert_int_equal(value.uint64_array_length, 0);
  assert_null(value.guid_array_values);
  assert_int_equal(value.guid_array_length, 0);
  assert_null(value.qualified_name_array_values);
  assert_int_equal(value.qualified_name_array_length, 0);
  assert_null(value.localized_text_array_values);
  assert_int_equal(value.localized_text_array_length, 0);
  assert_null(value.status_array_values);
  assert_int_equal(value.status_array_length, 0);
  cpkt_opcua_value_uint64(&value, 0xffffffffUL, 0xffffffffUL);
  assert_int_equal(
      cpkt_opcua_server_add_variable(
          server,
          cpkt_opcua_node_id_numeric(1, 5320),
          "maxUInt64",
          "Max UInt64",
          &value,
          &status),
      CPKT_OPCUA_OK);
  cpkt_opcua_value_clear(&out);
  assert_int_equal(
      cpkt_opcua_server_read(
          server,
          cpkt_opcua_node_id_numeric(1, 5320),
          &out,
          NULL,
          0,
          NULL,
          &status),
      CPKT_OPCUA_OK);
  assert_int_equal(out.type, CPKT_OPCUA_VALUE_UINT64);
  assert_int_equal(out.uint64_value.high32, 0xffffffffUL);
  assert_int_equal(out.uint64_value.low32, 0xffffffffUL);
  cpkt_opcua_value_datetime(&value, -2L, 0xfedcba98UL);
  cpkt_opcua_value_clear(&value);
  assert_int_equal(value.type, CPKT_OPCUA_VALUE_EMPTY);
  assert_int_equal(value.datetime_value.high32, 0);
  assert_int_equal(value.datetime_value.low32, 0);
  assert_null(value.datetime_array_values);
  assert_int_equal(value.datetime_array_length, 0);
  cpkt_opcua_data_value_clear(&data_value);
  assert_int_equal(data_value.has_value, 0);
  assert_int_equal(data_value.status, 0);
  cpkt_opcua_value_integer(&data_value.value, 12);
  data_value.has_value = 1;
  data_value.has_status = 1;
  data_value.status = 1;
  data_value.has_source_timestamp = 1;
  data_value.source_timestamp.high32 = -1;
  data_value.source_timestamp.low32 = 1;
  data_value.has_server_timestamp = 1;
  data_value.server_timestamp.high32 = 2;
  data_value.server_timestamp.low32 = 3;
  cpkt_opcua_data_value_clear(&data_value);
  assert_int_equal(data_value.has_value, 0);
  assert_int_equal(data_value.value.type, CPKT_OPCUA_VALUE_EMPTY);
  assert_int_equal(data_value.has_status, 0);
  assert_int_equal(data_value.status, 0);
  assert_int_equal(data_value.has_source_timestamp, 0);
  assert_int_equal(data_value.source_timestamp.high32, 0);
  assert_int_equal(data_value.source_timestamp.low32, 0);
  assert_int_equal(data_value.has_server_timestamp, 0);
  assert_int_equal(data_value.server_timestamp.high32, 0);
  assert_int_equal(data_value.server_timestamp.low32, 0);
  cpkt_opcua_value_integer(&value, 44);
  assert_int_equal(
      cpkt_opcua_server_write(server, cpkt_opcua_node_id_numeric(1, 5301), &value, &status),
      CPKT_OPCUA_OK);
  assert_int_equal(
      cpkt_opcua_server_read(
          server,
          cpkt_opcua_node_id_numeric(1, 5301),
          &out,
          NULL,
          0,
          NULL,
          &status),
      CPKT_OPCUA_OK);
  assert_int_equal(out.type, CPKT_OPCUA_VALUE_INTEGER);
  assert_int_equal(out.integer_value, 44);
  cpkt_opcua_value_clear(&value);
  assert_int_equal(
      cpkt_opcua_server_write(server, cpkt_opcua_node_id_numeric(1, 5301), &value, &status),
      CPKT_OPCUA_OK);
  assert_int_equal(
      cpkt_opcua_server_read(
          server,
          cpkt_opcua_node_id_numeric(1, 5301),
          &out,
          NULL,
          0,
          NULL,
          &status),
      CPKT_OPCUA_OK);
  assert_int_equal(out.type, CPKT_OPCUA_VALUE_EMPTY);
  cpkt_opcua_value_byte_string(&value, NULL, 1);
  assert_int_equal(
      cpkt_opcua_server_add_variable(
          server,
          cpkt_opcua_node_id_numeric(1, 5304),
          "badBytes",
          "Bad Bytes",
          &value,
          &status),
      CPKT_OPCUA_ERR_ARG);
  cpkt_opcua_value_qualified_name(&value, 1, NULL, 1);
  assert_int_equal(
      cpkt_opcua_server_add_variable(
          server,
          cpkt_opcua_node_id_numeric(1, 5311),
          "badQualifiedName",
          "Bad Qualified Name",
          &value,
          &status),
      CPKT_OPCUA_ERR_ARG);
  cpkt_opcua_value_localized_text(&value, NULL, 1, "text", 4);
  assert_int_equal(
      cpkt_opcua_server_add_variable(
          server,
          cpkt_opcua_node_id_numeric(1, 5312),
          "badLocalizedText",
          "Bad Localized Text",
          &value,
          &status),
      CPKT_OPCUA_ERR_ARG);
  cpkt_opcua_value_localized_text_array(&value, NULL, 1);
  assert_int_equal(
      cpkt_opcua_server_add_variable(
          server,
          cpkt_opcua_node_id_numeric(1, 5324),
          "badLocalizedTextArray",
          "Bad Localized Text Array",
          &value,
          &status),
      CPKT_OPCUA_ERR_ARG);
  {
    cpkt_opcua_localized_text_view bad_localized_text_array[1];

    bad_localized_text_array[0].locale = "en-US";
    bad_localized_text_array[0].locale_length = strlen("en-US");
    bad_localized_text_array[0].text = NULL;
    bad_localized_text_array[0].text_length = 1;
    cpkt_opcua_value_localized_text_array(&value, bad_localized_text_array, 1);
    assert_int_equal(
        cpkt_opcua_server_add_variable(
            server,
            cpkt_opcua_node_id_numeric(1, 5325),
            "badLocalizedTextArrayElement",
            "Bad Localized Text Array Element",
            &value,
            &status),
        CPKT_OPCUA_ERR_ARG);
  }
  cpkt_opcua_value_guid_array(&value, NULL, 1);
  assert_int_equal(
      cpkt_opcua_server_add_variable(
          server,
          cpkt_opcua_node_id_numeric(1, 5321),
          "badGuidArray",
          "Bad GUID Array",
          &value,
          &status),
      CPKT_OPCUA_ERR_ARG);
  cpkt_opcua_value_qualified_name_array(&value, NULL, 1);
  assert_int_equal(
      cpkt_opcua_server_add_variable(
          server,
          cpkt_opcua_node_id_numeric(1, 5322),
          "badQualifiedNameArray",
          "Bad Qualified Name Array",
          &value,
          &status),
      CPKT_OPCUA_ERR_ARG);
  {
    cpkt_opcua_qualified_name_view bad_qualified_name_array[1];

    bad_qualified_name_array[0].namespace_index = 1;
    bad_qualified_name_array[0].name = NULL;
    bad_qualified_name_array[0].name_length = 1;
    cpkt_opcua_value_qualified_name_array(&value, bad_qualified_name_array, 1);
    assert_int_equal(
        cpkt_opcua_server_add_variable(
            server,
            cpkt_opcua_node_id_numeric(1, 5323),
            "badQualifiedNameArrayElement",
            "Bad Qualified Name Array Element",
            &value,
            &status),
        CPKT_OPCUA_ERR_ARG);
  }
#if ULONG_MAX > UINT_MAX
  cpkt_opcua_value_status(&value, (unsigned long)UINT_MAX + 1UL);
  assert_int_equal(
      cpkt_opcua_server_add_variable(
          server,
          cpkt_opcua_node_id_numeric(1, 5313),
          "badStatus",
          "Bad Status",
          &value,
          &status),
      CPKT_OPCUA_ERR_RANGE);
  {
    cpkt_opcua_status bad_status_array[1];

    bad_status_array[0] = (unsigned long)UINT_MAX + 1UL;
    cpkt_opcua_value_status_array(&value, bad_status_array, 1);
    assert_int_equal(
        cpkt_opcua_server_add_variable(
            server,
            cpkt_opcua_node_id_numeric(1, 5320),
            "badStatusArray",
            "Bad Status Array",
            &value,
            &status),
        CPKT_OPCUA_ERR_RANGE);
  }
#endif
#if LONG_MAX > 2147483647L
  cpkt_opcua_value_datetime(&value, LONG_MAX, 0);
  assert_int_equal(
      cpkt_opcua_server_add_variable(
          server,
          cpkt_opcua_node_id_numeric(1, 5314),
          "badDateTime",
          "Bad DateTime",
          &value,
          &status),
      CPKT_OPCUA_ERR_RANGE);
  cpkt_opcua_value_datetime(&value, LONG_MIN, 0);
  assert_int_equal(
      cpkt_opcua_server_add_variable(
          server,
          cpkt_opcua_node_id_numeric(1, 5317),
          "badDateTimeNegative",
          "Bad DateTime Negative",
          &value,
          &status),
      CPKT_OPCUA_ERR_RANGE);
  {
    cpkt_opcua_datetime bad_datetime_array[1];

    bad_datetime_array[0].high32 = LONG_MAX;
    bad_datetime_array[0].low32 = 0;
    cpkt_opcua_value_datetime_array(&value, bad_datetime_array, 1);
    assert_int_equal(
        cpkt_opcua_server_add_variable(
            server,
            cpkt_opcua_node_id_numeric(1, 5319),
            "badDateTimeArray",
            "Bad DateTime Array",
            &value,
            &status),
        CPKT_OPCUA_ERR_RANGE);
  }
#endif
#if ULONG_MAX > 4294967295UL
  cpkt_opcua_value_uint64(&value, ULONG_MAX, 0);
  assert_int_equal(
      cpkt_opcua_server_add_variable(
          server,
          cpkt_opcua_node_id_numeric(1, 5315),
          "badUInt64",
          "Bad UInt64",
          &value,
          &status),
      CPKT_OPCUA_ERR_RANGE);
  cpkt_opcua_value_uint64(&value, 0, ULONG_MAX);
  assert_int_equal(
      cpkt_opcua_server_add_variable(
          server,
          cpkt_opcua_node_id_numeric(1, 5316),
          "badUInt64Low",
          "Bad UInt64 Low",
          &value,
          &status),
      CPKT_OPCUA_ERR_RANGE);
  {
    cpkt_opcua_uint64 bad_uint64_array[1];

    bad_uint64_array[0].high32 = ULONG_MAX;
    bad_uint64_array[0].low32 = 0;
    cpkt_opcua_value_uint64_array(&value, bad_uint64_array, 1);
    assert_int_equal(
        cpkt_opcua_server_add_variable(
            server,
            cpkt_opcua_node_id_numeric(1, 5318),
            "badUInt64Array",
            "Bad UInt64 Array",
            &value,
            &status),
        CPKT_OPCUA_ERR_RANGE);
  }
#endif
  assert_int_equal(
      cpkt_opcua_server_add_object_type(
          server,
          cpkt_opcua_node_id_numeric(1, 5305),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_BASEOBJECTTYPE),
          NULL,
          "Missing Browse",
          0,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_add_variable_type(
          server,
          cpkt_opcua_node_id_numeric(1, 5306),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_BASEVARIABLETYPE),
          "missingValueType",
          "Missing Value Type",
          NULL,
          0,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_add_reference_type(
          server,
          cpkt_opcua_node_id_numeric(1, 5307),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_REFERENCES),
          "missingInverse",
          "Missing Inverse",
          NULL,
          0,
          0,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_add_view(
          server,
          cpkt_opcua_node_id_numeric(1, 5308),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_VIEWSFOLDER),
          "badNotifierView",
          "Bad Notifier View",
          0,
          256UL,
          &status),
      CPKT_OPCUA_ERR_RANGE);
#if LONG_MAX > INT_MAX
  cpkt_opcua_value_integer(&value, (long)INT_MAX + 1L);
  assert_int_equal(
      cpkt_opcua_server_add_variable(
          server,
          cpkt_opcua_node_id_numeric(1, 5302),
          "rangeValue",
          "Range Value",
          &value,
          &status),
      CPKT_OPCUA_ERR_RANGE);
#endif
  cpkt_opcua_value_integer(&value, 1);
  assert_int_equal(
      cpkt_opcua_server_write(
          server,
          cpkt_opcua_node_id_string(1, NULL),
          &value,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_boolean_array_range(
          NULL,
          cpkt_opcua_node_id_numeric(1, 5301),
          "0:0",
          NULL,
          0,
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_double_array_range(
          server,
          cpkt_opcua_node_id_numeric(1, 5301),
          NULL,
          NULL,
          0,
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_uint64_array_range(
          server,
          cpkt_opcua_node_id_numeric(1, 5301),
          "0:0",
          NULL,
          0,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_datetime_array_range(
          server,
          cpkt_opcua_node_id_numeric(1, 5301),
          "0:0",
          NULL,
          0,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_status_array_range(
          server,
          cpkt_opcua_node_id_numeric(1, 5301),
          "0:0",
          NULL,
          0,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_guid_array_range(
          server,
          cpkt_opcua_node_id_numeric(1, 5301),
          "0:0",
          NULL,
          0,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_qualified_name_array_range(
          server,
          cpkt_opcua_node_id_numeric(1, 5301),
          "0:0",
          NULL,
          NULL,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_localized_text_array_range(
          server,
          cpkt_opcua_node_id_numeric(1, 5301),
          "0:0",
          NULL,
          NULL,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_string_array_range(
          server,
          cpkt_opcua_node_id_numeric(1, 5301),
          "0:0",
          cpkt_bad_string_array_callback,
          NULL,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_byte_string_array_range(
          server,
          cpkt_opcua_node_id_numeric(1, 5301),
          "0:0",
          cpkt_bad_byte_string_array_callback,
          NULL,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_trigger_event(
          server,
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_BASEEVENTTYPE),
          (unsigned long)USHRT_MAX + 1UL,
          "bad severity",
          NULL,
          0,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_trigger_event(
          server,
          cpkt_opcua_node_id_string(1, NULL),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_BASEEVENTTYPE),
          1UL,
          "bad source",
          NULL,
          0,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_trigger_event(
          server,
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_BASEEVENTTYPE),
          1UL,
          NULL,
          NULL,
          0,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_create_event(
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_BASEEVENTTYPE),
          1UL,
          "bad event out",
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_create_event(
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_BASEEVENTTYPE),
          (unsigned long)USHRT_MAX + 1UL,
          "bad prepared severity",
          &event,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_null(event);
  assert_int_equal(
      cpkt_opcua_server_create_event(
          cpkt_opcua_node_id_string(1, NULL),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_BASEEVENTTYPE),
          1UL,
          "bad prepared source",
          &event,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_null(event);
  assert_int_equal(
      cpkt_opcua_server_create_event(
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_BASEEVENTTYPE),
          1UL,
          "prepared event",
          &event,
          &status),
      CPKT_OPCUA_OK);
  assert_non_null(event);
  cpkt_opcua_value_string(&value, "field", strlen("field"));
  assert_int_equal(cpkt_opcua_server_event_set_field(NULL, 0, "Message", &value, &status), CPKT_OPCUA_ERR_ARG);
  assert_int_equal(cpkt_opcua_server_event_set_field(event, 0, NULL, &value, &status), CPKT_OPCUA_ERR_ARG);
  assert_int_equal(cpkt_opcua_server_event_set_field(event, 0, "", &value, &status), CPKT_OPCUA_ERR_ARG);
  assert_int_equal(cpkt_opcua_server_event_set_field(event, 0, "Message", NULL, &status), CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_event_trigger(NULL, event, NULL, 0, NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_event_trigger(server, NULL, NULL, 0, NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  cpkt_opcua_server_event_free(event);
  event = NULL;
  assert_int_equal(
      cpkt_opcua_server_delete_node(server, cpkt_opcua_node_id_string(1, NULL), 1, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_add_reference(
          server,
          cpkt_opcua_node_id_string(1, NULL),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_HASCOMPONENT),
          1,
          cpkt_opcua_node_id_numeric(1, 5301),
          CPKT_OPCUA_NODE_CLASS_VARIABLE,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_add_reference(
          server,
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_HASCOMPONENT),
          1,
          cpkt_opcua_node_id_numeric(1, 5301),
          9999,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_add_reference_ex(
          server,
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_HASCOMPONENT),
          1,
          cpkt_opcua_expanded_node_id_uri(NULL, 1, cpkt_opcua_node_id_numeric(1, 5301)),
          CPKT_OPCUA_NODE_CLASS_VARIABLE,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_delete_reference(
          server,
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_HASCOMPONENT),
          1,
          cpkt_opcua_node_id_string(1, NULL),
          1,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_delete_reference_ex(
          server,
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_HASCOMPONENT),
          1,
          cpkt_opcua_expanded_node_id_uri(NULL, 1, cpkt_opcua_node_id_numeric(1, 5301)),
          1,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_write_description(server, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_write_display_name(server, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_data_type(server, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_node_id(
          server,
          cpkt_opcua_node_id_numeric(1, 1),
          NULL,
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_node_id(
          server,
          cpkt_opcua_node_id_string(1, NULL),
          &parsed_node,
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_write_data_type(
          server,
          cpkt_opcua_node_id_string(1, NULL),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_INT32),
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_write_data_type(
          server,
          cpkt_opcua_node_id_numeric(1, 1),
          cpkt_opcua_node_id_string(1, NULL),
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_write_mask(server, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_user_write_mask(server, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_write_write_mask(server, cpkt_opcua_node_id_string(1, NULL), 1UL, &status),
      CPKT_OPCUA_ERR_ARG);
#if ULONG_MAX > UINT_MAX
  assert_int_equal(
      cpkt_opcua_server_write_write_mask(server, cpkt_opcua_node_id_numeric(1, 1), ULONG_MAX, &status),
      CPKT_OPCUA_ERR_RANGE);
#endif
  assert_int_equal(
      cpkt_opcua_server_read_is_abstract(server, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_write_is_abstract(server, cpkt_opcua_node_id_string(1, NULL), 1, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_symmetric(server, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_write_symmetric(server, cpkt_opcua_node_id_string(1, NULL), 1, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_write_inverse_name(server, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_contains_no_loops(server, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_write_contains_no_loops(server, cpkt_opcua_node_id_string(1, NULL), 1, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_event_notifier(server, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_write_event_notifier(server, cpkt_opcua_node_id_string(1, NULL), 1UL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_write_event_notifier(server, cpkt_opcua_node_id_numeric(1, 1), 256UL, &status),
      CPKT_OPCUA_ERR_RANGE);
  assert_int_equal(
      cpkt_opcua_server_write_value_rank(server, cpkt_opcua_node_id_string(1, NULL), -1, &status),
      CPKT_OPCUA_ERR_ARG);
#if LONG_MAX > INT_MAX
  assert_int_equal(
      cpkt_opcua_server_write_value_rank(server, cpkt_opcua_node_id_numeric(1, 1), (long)INT_MAX + 1L, &status),
      CPKT_OPCUA_ERR_RANGE);
#endif
  assert_int_equal(
      cpkt_opcua_server_read_array_dimensions(
          server,
          cpkt_opcua_node_id_numeric(1, 1),
          NULL,
          0,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_write_array_dimensions(
          server,
          cpkt_opcua_node_id_string(1, NULL),
          NULL,
          0,
          &status),
      CPKT_OPCUA_ERR_ARG);
#if ULONG_MAX > UINT_MAX
  {
    unsigned long dimension_value = ULONG_MAX;

    assert_int_equal(
        cpkt_opcua_server_write_array_dimensions(
            server,
            cpkt_opcua_node_id_numeric(1, 1),
            &dimension_value,
            1,
            &status),
        CPKT_OPCUA_ERR_RANGE);
  }
#endif
  assert_int_equal(
      cpkt_opcua_server_read_access_level(server, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_user_access_level(server, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_write_access_level(server, cpkt_opcua_node_id_string(1, NULL), 1UL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_write_access_level(server, cpkt_opcua_node_id_numeric(1, 1), 256UL, &status),
      CPKT_OPCUA_ERR_RANGE);
  assert_int_equal(
      cpkt_opcua_server_read_access_level_ex(server, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_write_access_level_ex(server, cpkt_opcua_node_id_string(1, NULL), 1UL, &status),
      CPKT_OPCUA_ERR_ARG);
#if ULONG_MAX > UINT_MAX
  assert_int_equal(
      cpkt_opcua_server_write_access_level_ex(server, cpkt_opcua_node_id_numeric(1, 1), ULONG_MAX, &status),
      CPKT_OPCUA_ERR_RANGE);
#endif
  assert_int_equal(
      cpkt_opcua_server_read_minimum_sampling_interval(
          server,
          cpkt_opcua_node_id_numeric(1, 1),
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_write_minimum_sampling_interval(
          server,
          cpkt_opcua_node_id_string(1, NULL),
          1.0,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_historizing(server, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_write_historizing(server, cpkt_opcua_node_id_string(1, NULL), 1, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_write_executable(server, cpkt_opcua_node_id_string(1, NULL), 1, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_user_executable(server, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_method_argument_count(
          server,
          cpkt_opcua_node_id_string(1, NULL),
          CPKT_OPCUA_METHOD_ARGUMENT_INPUT,
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_method_argument_count(
          server,
          cpkt_opcua_node_id_numeric(1, 1),
          99,
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read_method_argument(
          server,
          cpkt_opcua_node_id_numeric(1, 1),
          CPKT_OPCUA_METHOD_ARGUMENT_INPUT,
          0,
          NULL,
          &value_rank,
          node_id_buffer,
          sizeof(node_id_buffer),
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_read(
          server,
          cpkt_opcua_node_id_numeric(1, 9999),
          &out,
          NULL,
          0,
          NULL,
          &status),
      CPKT_OPCUA_ERR_UPSTREAM);
  assert_int_equal(
      cpkt_opcua_server_browse_children(
          server,
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          cpkt_browse_stop_callback,
          NULL,
          &status),
      CPKT_OPCUA_ERR_CALLBACK);
  cpkt_opcua_browse_options_default(&browse_options);
  browse_options.browse_direction = 99;
  assert_int_equal(
      cpkt_opcua_server_browse_children_ex(
          server,
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          &browse_options,
          cpkt_browse_stop_callback,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
#if ULONG_MAX > UINT_MAX
  cpkt_opcua_browse_options_default(&browse_options);
  browse_options.result_mask = (unsigned long)UINT_MAX + 1UL;
  assert_int_equal(
      cpkt_opcua_server_browse_children_ex(
          server,
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          &browse_options,
          cpkt_browse_stop_callback,
          NULL,
          &status),
      CPKT_OPCUA_ERR_RANGE);
#endif
  assert_int_equal(
      cpkt_opcua_server_browse_children_page(
          server,
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          NULL,
          NULL,
          NULL,
          byte_node_buffer,
          sizeof(byte_node_buffer),
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_browse_next(
          server,
          NULL,
          1,
          0,
          cpkt_browse_stop_callback,
          NULL,
          byte_node_buffer,
          sizeof(byte_node_buffer),
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_browse_next(
          server,
          byte_node_buffer,
          0,
          0,
          NULL,
          NULL,
          byte_node_buffer,
          sizeof(byte_node_buffer),
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  browse_path_elements[0].namespace_index = CPKT_OPCUA_TEST_NS;
  browse_path_elements[0].browse_name = "child";
  assert_int_equal(
      cpkt_opcua_server_translate_browse_path(
          server,
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          browse_path_elements,
          1,
          NULL,
          node_id_buffer,
          sizeof(node_id_buffer),
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  browse_path_elements[0].browse_name = NULL;
  assert_int_equal(
      cpkt_opcua_server_translate_browse_path(
          server,
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          browse_path_elements,
          1,
          &parsed_node,
          node_id_buffer,
          sizeof(node_id_buffer),
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);

  callback_seen = 0;
  assert_int_equal(
      cpkt_opcua_server_native(server, cpkt_native_callback_succeeds, &callback_seen),
      CPKT_OPCUA_OK);
  assert_int_equal(callback_seen, 1);
  callback_seen = 0;
  assert_int_equal(
      cpkt_opcua_server_pubsub_native(server, cpkt_native_server_pubsub_succeeds, &callback_seen),
      CPKT_OPCUA_OK);
  assert_int_equal(callback_seen, 1);
  assert_int_equal(
      cpkt_opcua_server_pubsub_native(server, NULL, NULL),
      CPKT_OPCUA_ERR_ARG);
  callback_seen = 0;
  assert_int_equal(
      cpkt_opcua_server_history_native(server, cpkt_native_server_history_succeeds, &callback_seen),
      CPKT_OPCUA_OK);
  assert_int_equal(callback_seen, 1);
  assert_int_equal(
      cpkt_opcua_server_history_native(server, NULL, NULL),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_server_native(server, cpkt_native_callback_fails, NULL),
      CPKT_OPCUA_ERR_UPSTREAM);
  assert_int_equal(cpkt_opcua_server_native(server, NULL, NULL), CPKT_OPCUA_ERR_ARG);
  method_input_types[0] = CPKT_OPCUA_VALUE_INTEGER_ARRAY;
  assert_int_equal(
      cpkt_opcua_server_add_method(
          server,
          cpkt_opcua_node_id_numeric(1, 5301),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          "arrayInputMethod",
          "Array Input Method",
          method_input_types,
          1,
          CPKT_OPCUA_VALUE_EMPTY,
          cpkt_bad_method_callback,
          NULL,
          &status),
      CPKT_OPCUA_ERR_TYPE);
  method_input_types[0] = CPKT_OPCUA_VALUE_INTEGER;
  assert_int_equal(
      cpkt_opcua_server_add_method(
          server,
          cpkt_opcua_node_id_numeric(1, 5302),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          "arrayOutputMethod",
          "Array Output Method",
          method_input_types,
          1,
          CPKT_OPCUA_VALUE_INTEGER_ARRAY,
          cpkt_bad_method_callback,
          NULL,
          &status),
      CPKT_OPCUA_ERR_TYPE);
  method_output_types[0] = CPKT_OPCUA_VALUE_INTEGER_ARRAY;
  assert_int_equal(
      cpkt_opcua_server_add_method_many(
          server,
          cpkt_opcua_node_id_numeric(1, 5305),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          "arrayOutputMethodMany",
          "Array Output Method Many",
          method_input_types,
          1,
          method_output_types,
          1,
          cpkt_bad_method_many_callback,
          NULL,
          &status),
      CPKT_OPCUA_ERR_TYPE);
  method_input_types[0] = CPKT_OPCUA_VALUE_INTEGER;
  assert_int_equal(
      cpkt_opcua_server_add_method(
          server,
          cpkt_opcua_node_id_numeric(1, 5303),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          "badMethod",
          "Bad Method",
          method_input_types,
          1,
          CPKT_OPCUA_VALUE_EMPTY,
          cpkt_bad_method_callback,
          NULL,
          &status),
      CPKT_OPCUA_OK);
  assert_int_equal(
      cpkt_opcua_server_read_method_argument(
          server,
          cpkt_opcua_node_id_numeric(1, 5303),
          CPKT_OPCUA_METHOD_ARGUMENT_OUTPUT,
          0,
          &parsed_node,
          &value_rank,
          node_id_buffer,
          sizeof(node_id_buffer),
          &required,
          &status),
      CPKT_OPCUA_OK);
  assert_true(cpkt_opcua_node_id_equal(parsed_node, cpkt_opcua_node_id_numeric(0, UA_NS0ID_BASEDATATYPE)));
  assert_int_equal(value_rank, UA_VALUERANK_SCALAR);
  method_output_types[0] = CPKT_OPCUA_VALUE_INTEGER;
  method_output_types[1] = CPKT_OPCUA_VALUE_EMPTY;
  assert_int_equal(
      cpkt_opcua_server_add_method_many(
          server,
          cpkt_opcua_node_id_numeric(1, 5304),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          "badMethodMany",
          "Bad Method Many",
          method_input_types,
          1,
          method_output_types,
          2,
          NULL,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);

  assert_int_equal(cpkt_opcua_client_connect(client, NULL, &status), CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_get_endpoint_count(client, NULL, &required, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_get_endpoint_url(client, NULL, 0, node_id_buffer, sizeof(node_id_buffer), &required, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_find_server_count(client, NULL, &required, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_find_server_application_uri(
          client,
          NULL,
          0,
          node_id_buffer,
          sizeof(node_id_buffer),
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_find_server_application_name(
          client,
          NULL,
          0,
          node_id_buffer,
          sizeof(node_id_buffer),
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_read_node_id(
          client,
          cpkt_opcua_node_id_numeric(1, 1),
          NULL,
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_read_node_id(
          client,
          cpkt_opcua_node_id_string(1, NULL),
          &parsed_node,
          node_id_large_buffer,
          sizeof(node_id_large_buffer),
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_delete_node(client, cpkt_opcua_node_id_string(1, NULL), 1, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_add_reference(
          client,
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          cpkt_opcua_node_id_string(1, NULL),
          1,
          cpkt_opcua_node_id_numeric(1, 1),
          CPKT_OPCUA_NODE_CLASS_VARIABLE,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_add_reference(
          client,
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_HASCOMPONENT),
          1,
          cpkt_opcua_node_id_numeric(1, 1),
          9999,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_add_reference_ex(
          client,
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_HASCOMPONENT),
          1,
          cpkt_opcua_expanded_node_id_uri(NULL, 1, cpkt_opcua_node_id_numeric(1, 1)),
          CPKT_OPCUA_NODE_CLASS_VARIABLE,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_delete_reference(
          client,
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_HASCOMPONENT),
          1,
          cpkt_opcua_node_id_string(1, NULL),
          1,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_delete_reference_ex(
          client,
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_HASCOMPONENT),
          1,
          cpkt_opcua_expanded_node_id_uri(NULL, 1, cpkt_opcua_node_id_numeric(1, 1)),
          1,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_browse_children_page(
          client,
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          NULL,
          NULL,
          NULL,
          byte_node_buffer,
          sizeof(byte_node_buffer),
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_browse_next(
          client,
          NULL,
          1,
          0,
          cpkt_browse_stop_callback,
          NULL,
          byte_node_buffer,
          sizeof(byte_node_buffer),
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_browse_next(
          client,
          byte_node_buffer,
          0,
          0,
          NULL,
          NULL,
          byte_node_buffer,
          sizeof(byte_node_buffer),
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  cpkt_opcua_value_integer(&value, 1);
  assert_int_equal(
      cpkt_opcua_client_add_variable(
          client,
          cpkt_opcua_node_id_numeric(1, 1),
          NULL,
          "missing name",
          &value,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_add_variable(
          client,
          cpkt_opcua_node_id_numeric(1, 1),
          "missingValue",
          "Missing Value",
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_add_variable_under(
          client,
          cpkt_opcua_node_id_numeric(1, 1),
          cpkt_opcua_node_id_string(1, NULL),
          "badParent",
          "Bad Parent",
          &value,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_add_object_type(
          client,
          cpkt_opcua_node_id_numeric(1, 1),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_BASEOBJECTTYPE),
          NULL,
          "Missing Browse",
          0,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_add_variable_type(
          client,
          cpkt_opcua_node_id_numeric(1, 1),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_BASEVARIABLETYPE),
          "missingValueType",
          "Missing Value Type",
          NULL,
          0,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_add_reference_type(
          client,
          cpkt_opcua_node_id_numeric(1, 1),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_REFERENCES),
          "missingInverse",
          "Missing Inverse",
          NULL,
          0,
          0,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_add_view(
          client,
          cpkt_opcua_node_id_numeric(1, 1),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_VIEWSFOLDER),
          "badNotifierView",
          "Bad Notifier View",
          0,
          256UL,
          &status),
      CPKT_OPCUA_ERR_RANGE);
  assert_int_equal(
      cpkt_opcua_client_write_description(client, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_write_display_name(client, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_read_data_type(client, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_write_data_type(
          client,
          cpkt_opcua_node_id_string(1, NULL),
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_INT32),
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_write_data_type(
          client,
          cpkt_opcua_node_id_numeric(1, 1),
          cpkt_opcua_node_id_string(1, NULL),
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_read_write_mask(client, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_read_user_write_mask(client, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_write_write_mask(client, cpkt_opcua_node_id_string(1, NULL), 1UL, &status),
      CPKT_OPCUA_ERR_ARG);
#if ULONG_MAX > UINT_MAX
  assert_int_equal(
      cpkt_opcua_client_write_write_mask(client, cpkt_opcua_node_id_numeric(1, 1), ULONG_MAX, &status),
      CPKT_OPCUA_ERR_RANGE);
#endif
  assert_int_equal(
      cpkt_opcua_client_read_is_abstract(client, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_write_is_abstract(client, cpkt_opcua_node_id_string(1, NULL), 1, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_read_symmetric(client, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_write_symmetric(client, cpkt_opcua_node_id_string(1, NULL), 1, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_write_inverse_name(client, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_read_contains_no_loops(client, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_write_contains_no_loops(client, cpkt_opcua_node_id_string(1, NULL), 1, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_read_event_notifier(client, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_write_event_notifier(client, cpkt_opcua_node_id_string(1, NULL), 1UL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_write_event_notifier(client, cpkt_opcua_node_id_numeric(1, 1), 256UL, &status),
      CPKT_OPCUA_ERR_RANGE);
  assert_int_equal(
      cpkt_opcua_client_write_value_rank(client, cpkt_opcua_node_id_string(1, NULL), -1, &status),
      CPKT_OPCUA_ERR_ARG);
#if LONG_MAX > INT_MAX
  assert_int_equal(
      cpkt_opcua_client_write_value_rank(client, cpkt_opcua_node_id_numeric(1, 1), (long)INT_MAX + 1L, &status),
      CPKT_OPCUA_ERR_RANGE);
#endif
  assert_int_equal(
      cpkt_opcua_client_read_array_dimensions(
          client,
          cpkt_opcua_node_id_numeric(1, 1),
          NULL,
          0,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_write_array_dimensions(
          client,
          cpkt_opcua_node_id_string(1, NULL),
          NULL,
          0,
          &status),
      CPKT_OPCUA_ERR_ARG);
#if ULONG_MAX > UINT_MAX
  {
    unsigned long dimension_value = ULONG_MAX;

    assert_int_equal(
        cpkt_opcua_client_write_array_dimensions(
            client,
            cpkt_opcua_node_id_numeric(1, 1),
            &dimension_value,
            1,
            &status),
        CPKT_OPCUA_ERR_RANGE);
  }
#endif
  assert_int_equal(
      cpkt_opcua_client_read_executable(client, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_write_access_level(client, cpkt_opcua_node_id_string(1, NULL), 1UL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_write_access_level(client, cpkt_opcua_node_id_numeric(1, 1), 256UL, &status),
      CPKT_OPCUA_ERR_RANGE);
  assert_int_equal(
      cpkt_opcua_client_read_access_level_ex(client, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_write_access_level_ex(client, cpkt_opcua_node_id_string(1, NULL), 1UL, &status),
      CPKT_OPCUA_ERR_ARG);
#if ULONG_MAX > UINT_MAX
  assert_int_equal(
      cpkt_opcua_client_write_access_level_ex(client, cpkt_opcua_node_id_numeric(1, 1), ULONG_MAX, &status),
      CPKT_OPCUA_ERR_RANGE);
#endif
  assert_int_equal(
      cpkt_opcua_client_read_minimum_sampling_interval(
          client,
          cpkt_opcua_node_id_numeric(1, 1),
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_write_minimum_sampling_interval(
          client,
          cpkt_opcua_node_id_string(1, NULL),
          1.0,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_read_historizing(client, cpkt_opcua_node_id_numeric(1, 1), NULL, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_write_historizing(client, cpkt_opcua_node_id_string(1, NULL), 1, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_write_executable(client, cpkt_opcua_node_id_string(1, NULL), 1, &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_read_method_argument_count(
          client,
          cpkt_opcua_node_id_string(1, NULL),
          CPKT_OPCUA_METHOD_ARGUMENT_INPUT,
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_read_method_argument_count(
          client,
          cpkt_opcua_node_id_numeric(1, 1),
          99,
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_read_method_argument(
          client,
          cpkt_opcua_node_id_numeric(1, 1),
          CPKT_OPCUA_METHOD_ARGUMENT_INPUT,
          0,
          NULL,
          &value_rank,
          node_id_buffer,
          sizeof(node_id_buffer),
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_modify_subscription(client, 1, -1.0, &status),
      CPKT_OPCUA_ERR_ARG);
  cpkt_opcua_monitor_options_default(&monitor_options);
  assert_int_equal(
      cpkt_opcua_client_monitor_value_ex(
          client,
          1,
          cpkt_opcua_node_id_numeric(1, 1),
          &monitor_options,
          NULL,
          NULL,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  monitor_options.sampling_interval_ms = -1.0;
  assert_int_equal(
      cpkt_opcua_client_monitor_value_ex(
          client,
          1,
          cpkt_opcua_node_id_numeric(1, 1),
          &monitor_options,
          cpkt_bad_data_change_callback,
          NULL,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  cpkt_opcua_monitor_options_default(&monitor_options);
  monitor_options.deadband_type = 99;
  assert_int_equal(
      cpkt_opcua_client_monitor_value_ex(
          client,
          1,
          cpkt_opcua_node_id_numeric(1, 1),
          &monitor_options,
          cpkt_bad_data_change_callback,
          NULL,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  cpkt_opcua_monitor_options_default(&monitor_options);
  monitor_options.deadband_value = 1.0;
  assert_int_equal(
      cpkt_opcua_client_monitor_value_ex(
          client,
          1,
          cpkt_opcua_node_id_numeric(1, 1),
          &monitor_options,
          cpkt_bad_data_change_callback,
          NULL,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  cpkt_opcua_monitor_options_default(&monitor_options);
  monitor_options.deadband_type = CPKT_OPCUA_DEADBAND_PERCENT;
  monitor_options.deadband_value = 101.0;
  assert_int_equal(
      cpkt_opcua_client_monitor_value_ex(
          client,
          1,
          cpkt_opcua_node_id_numeric(1, 1),
          &monitor_options,
          cpkt_bad_data_change_callback,
          NULL,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_set_monitoring_mode(
          client,
          1,
          1,
          99,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_monitor_events(
          client,
          1,
          cpkt_opcua_node_id_numeric(1, 1),
          1.0,
          NULL,
          NULL,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
#if ULONG_MAX > UINT_MAX
  assert_int_equal(
      cpkt_opcua_client_monitor_events(
          client,
          (unsigned long)UINT_MAX + 1UL,
          cpkt_opcua_node_id_numeric(1, 1),
          1.0,
          cpkt_bad_event_callback,
          NULL,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
#endif
  assert_int_equal(
      cpkt_opcua_client_monitor_events(
          client,
          1,
          cpkt_opcua_node_id_numeric(1, 1),
          -1.0,
          cpkt_bad_event_callback,
          NULL,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  event_field_names[0].data = "Message";
  event_field_names[0].length = strlen("Message");
  assert_int_equal(
      cpkt_opcua_client_monitor_event_fields(
          client,
          1,
          cpkt_opcua_node_id_numeric(1, 1),
          1.0,
          NULL,
          1,
          cpkt_bad_event_fields_callback,
          NULL,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_monitor_event_fields(
          client,
          1,
          cpkt_opcua_node_id_numeric(1, 1),
          1.0,
          event_field_names,
          0,
          cpkt_bad_event_fields_callback,
          NULL,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_monitor_event_fields(
          client,
          1,
          cpkt_opcua_node_id_numeric(1, 1),
          1.0,
          event_field_names,
          1,
          NULL,
          NULL,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  event_field_names[0].data = "";
  event_field_names[0].length = 0;
  assert_int_equal(
      cpkt_opcua_client_monitor_event_fields(
          client,
          1,
          cpkt_opcua_node_id_numeric(1, 1),
          1.0,
          event_field_names,
          1,
          cpkt_bad_event_fields_callback,
          NULL,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  event_field_names[0].data = "Message";
  event_field_names[0].length = strlen("Message");
#if ULONG_MAX > UINT_MAX
  assert_int_equal(
      cpkt_opcua_client_monitor_event_fields(
          client,
          (unsigned long)UINT_MAX + 1UL,
          cpkt_opcua_node_id_numeric(1, 1),
          1.0,
          event_field_names,
          1,
          cpkt_bad_event_fields_callback,
          NULL,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
#endif
  assert_int_equal(
      cpkt_opcua_client_monitor_event_fields(
          client,
          1,
          cpkt_opcua_node_id_numeric(1, 1),
          -1.0,
          event_field_names,
          1,
          cpkt_bad_event_fields_callback,
          NULL,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_call_method_many(
          client,
          cpkt_opcua_node_id_numeric(1, 1),
          cpkt_opcua_node_id_numeric(1, 2),
          NULL,
          0,
          NULL,
          0,
          NULL,
          NULL,
          NULL,
          &status),
      CPKT_OPCUA_ERR_ARG);
  browse_path_elements[0].browse_name = "child";
  assert_int_equal(
      cpkt_opcua_client_translate_browse_path(
          client,
          cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
          browse_path_elements,
          1,
          NULL,
          node_id_buffer,
          sizeof(node_id_buffer),
          &required,
          &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(
      cpkt_opcua_client_connect_username(client, NULL, "user", "password", &status),
      CPKT_OPCUA_ERR_ARG);
  assert_int_equal(cpkt_opcua_client_connect(client, "opc.tcp://127.0.0.1:9", &status), CPKT_OPCUA_ERR_UPSTREAM);
  assert_int_not_equal(status, UA_STATUSCODE_GOOD);

  cpkt_opcua_client_free(client);
  cpkt_opcua_server_free(server);
}

int main(int argc, char **argv) {
  const struct CMUnitTest server_c99_client_c89_tests[] = {
      cmocka_unit_test(cpkt_facade_client_reads_and_writes_native_server),
  };
  const struct CMUnitTest server_c89_client_c99_tests[] = {
      cmocka_unit_test(cpkt_native_client_reads_and_writes_facade_server),
  };
  const struct CMUnitTest facade_client_server_tests[] = {
      cmocka_unit_test(cpkt_facade_client_reads_and_writes_facade_server),
  };
  const struct CMUnitTest failure_tests[] = {
      cmocka_unit_test(cpkt_facade_username_access_control),
      cmocka_unit_test(cpkt_facade_callback_access_control),
      cmocka_unit_test(cpkt_facade_failure_modes),
  };
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(cpkt_facade_client_reads_and_writes_native_server),
      cmocka_unit_test(cpkt_native_client_reads_and_writes_facade_server),
      cmocka_unit_test(cpkt_facade_client_reads_and_writes_facade_server),
      cmocka_unit_test(cpkt_facade_json_config_server_constructors),
      cmocka_unit_test(cpkt_facade_username_access_control),
      cmocka_unit_test(cpkt_facade_callback_access_control),
      cmocka_unit_test(cpkt_facade_failure_modes),
  };

  if (argc == 2 && strcmp(argv[1], "server-is-c99-and-client-is-c89") == 0) {
    return cmocka_run_group_tests(server_c99_client_c89_tests, NULL, NULL);
  }
  if (argc == 2 && strcmp(argv[1], "server-is-c89-and-client-is-c99") == 0) {
    return cmocka_run_group_tests(server_c89_client_c99_tests, NULL, NULL);
  }
  if (argc == 2 && strcmp(argv[1], "server-is-c89-and-client-is-c89") == 0) {
    return cmocka_run_group_tests(facade_client_server_tests, NULL, NULL);
  }
  if (argc == 2 && strcmp(argv[1], "failure-modes") == 0) {
    return cmocka_run_group_tests(failure_tests, NULL, NULL);
  }
  return cmocka_run_group_tests(tests, NULL, NULL);
}
