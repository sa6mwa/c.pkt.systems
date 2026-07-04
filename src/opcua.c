#include <cpkt/opcua.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <open62541/client.h>
#include <open62541/client_config_default.h>
#include <open62541/client_highlevel.h>
#include <open62541/client_highlevel_async.h>
#include <open62541/client_subscriptions.h>
#include <open62541/nodeids.h>
#include <open62541/plugin/accesscontrol_default.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>
#include <open62541/server_config_file_based.h>
#include <open62541/server_pubsub.h>
#include <open62541/util.h>

#ifndef CPKT_OPCUA_FACADE_VERSION
#define CPKT_OPCUA_FACADE_VERSION "0"
#endif

#define CPKT_OPCUA_UINT32_MAX_VALUE 4294967295UL
#define CPKT_OPCUA_INT32_MIN_VALUE (-2147483647L - 1L)
#define CPKT_OPCUA_INT32_MAX_VALUE 2147483647L

typedef char cpkt_opcua_assert_upstream_uint64_is_64_bits
    [(sizeof(UA_UInt64) * CHAR_BIT == 64) ? 1 : -1];
typedef char cpkt_opcua_assert_upstream_datetime_is_64_bits
    [(sizeof(UA_DateTime) * CHAR_BIT == 64) ? 1 : -1];
typedef char cpkt_opcua_assert_upstream_status_is_32_bits
    [(sizeof(UA_StatusCode) * CHAR_BIT == 32) ? 1 : -1];

struct cpkt_opcua_monitor_context {
  struct cpkt_opcua_monitor_context *next;
  cpkt_opcua_client *owner;
  cpkt_opcua_subscription_id subscription_id;
  cpkt_opcua_monitored_item_id monitored_item_id;
  cpkt_opcua_data_change_fn fn;
  cpkt_opcua_event_fn event_fn;
  cpkt_opcua_event_fields_fn event_fields_fn;
  char **event_field_names;
  size_t event_field_count;
  void *user;
};

struct cpkt_opcua_async_context {
  struct cpkt_opcua_async_context *next;
  cpkt_opcua_client *owner;
  cpkt_opcua_async_value_fn value_fn;
  cpkt_opcua_async_status_fn status_fn;
  cpkt_opcua_browse_fn browse_fn;
  cpkt_opcua_async_browse_fn browse_done_fn;
  cpkt_opcua_async_call_fn call_fn;
  cpkt_opcua_async_node_fn node_fn;
  void *user;
  cpkt_opcua_value *outputs;
  size_t expected_output_count;
  char *string_buffer;
  size_t string_buffer_size;
  size_t *required_string_size_out;
  char **string_buffers;
  const size_t *string_buffer_sizes;
  size_t *required_string_sizes_out;
  cpkt_opcua_node_id *node_id_out;
  char *node_id_buffer;
  size_t node_id_buffer_size;
  size_t *required_node_id_size_out;
};

struct cpkt_opcua_method_context {
  struct cpkt_opcua_method_context *next;
  cpkt_opcua_method_many_fn fn;
  cpkt_opcua_method_fn single_fn;
  void *user;
  size_t input_count;
  size_t output_count;
  int *input_types;
  int *output_types;
};

struct cpkt_opcua_client {
  UA_Client *client;
  struct cpkt_opcua_monitor_context *monitors;
  struct cpkt_opcua_async_context *asyncs;
};

struct cpkt_opcua_server {
  UA_Server *server;
  unsigned short port;
  int started;
  struct cpkt_opcua_method_context *methods;
  char *endpoint_hostname;
  char *access_username;
  unsigned char *access_password;
  cpkt_opcua_login_fn access_login_fn;
  void *access_login_user;
};

struct cpkt_owned_node_id_memory {
  char *string;
  unsigned char *byte_string;
};

static void cpkt_async_link(cpkt_opcua_client *client, struct cpkt_opcua_async_context *context) {
  context->owner = client;
  context->next = client->asyncs;
  client->asyncs = context;
}

static void cpkt_async_unlink(struct cpkt_opcua_async_context *context) {
  cpkt_opcua_client *client;
  struct cpkt_opcua_async_context **slot;

  if (context == NULL || context->owner == NULL) {
    return;
  }
  client = context->owner;
  slot = &client->asyncs;
  while (*slot != NULL) {
    if (*slot == context) {
      *slot = context->next;
      context->next = NULL;
      context->owner = NULL;
      return;
    }
    slot = &(*slot)->next;
  }
  context->owner = NULL;
}

static void cpkt_async_finish(struct cpkt_opcua_async_context *context) {
  if (context == NULL) {
    return;
  }
  cpkt_async_unlink(context);
  free(context);
}

struct cpkt_opcua_history_read_context {
  cpkt_opcua_history_data_value_fn fn;
  void *user;
  char *string_buffer;
  size_t string_buffer_size;
  size_t *required_string_size_out;
  cpkt_opcua_result result;
};

struct cpkt_opcua_server_event {
  cpkt_opcua_node_id source_node_id;
  cpkt_opcua_node_id event_type_id;
  struct cpkt_owned_node_id_memory source_memory;
  struct cpkt_owned_node_id_memory event_type_memory;
  unsigned long severity;
  char *message;
  UA_KeyValueMap fields;
};

static void cpkt_byte_string_clear_malloc(UA_ByteString *bytes) {
  if (bytes == NULL) {
    return;
  }
  free(bytes->data);
  bytes->data = NULL;
  bytes->length = 0;
}

static cpkt_opcua_result cpkt_read_file_bytes(
    const char *path,
    UA_ByteString *bytes_out,
    cpkt_opcua_status *status_out) {
  FILE *file;
  long length;
  size_t read_count;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (bytes_out == NULL || path == NULL || path[0] == '\0') {
    return CPKT_OPCUA_ERR_ARG;
  }
  bytes_out->data = NULL;
  bytes_out->length = 0;
  file = fopen(path, "rb");
  if (file == NULL) {
    if (status_out != NULL) {
      *status_out = (cpkt_opcua_status)UA_STATUSCODE_BADNOTFOUND;
    }
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    if (status_out != NULL) {
      *status_out = (cpkt_opcua_status)UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  length = ftell(file);
  if (length <= 0) {
    fclose(file);
    if (status_out != NULL) {
      *status_out = (cpkt_opcua_status)UA_STATUSCODE_BADCONFIGURATIONERROR;
    }
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if ((unsigned long)length > (unsigned long)((size_t)-1)) {
    fclose(file);
    return CPKT_OPCUA_ERR_RANGE;
  }
  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    if (status_out != NULL) {
      *status_out = (cpkt_opcua_status)UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  bytes_out->data = (UA_Byte *)malloc((size_t)length);
  if (bytes_out->data == NULL) {
    fclose(file);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  read_count = fread(bytes_out->data, 1, (size_t)length, file);
  if (read_count != (size_t)length || ferror(file)) {
    cpkt_byte_string_clear_malloc(bytes_out);
    fclose(file);
    if (status_out != NULL) {
      *status_out = (cpkt_opcua_status)UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (fclose(file) != 0) {
    cpkt_byte_string_clear_malloc(bytes_out);
    if (status_out != NULL) {
      *status_out = (cpkt_opcua_status)UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  bytes_out->length = (size_t)length;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_variable_under(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    const cpkt_opcua_value *value,
    cpkt_opcua_status *status_out);

static cpkt_opcua_status cpkt_status(UA_StatusCode status) {
  return (cpkt_opcua_status)status;
}

static int cpkt_valid_uint64_words(unsigned long high32, unsigned long low32) {
  return high32 <= CPKT_OPCUA_UINT32_MAX_VALUE && low32 <= CPKT_OPCUA_UINT32_MAX_VALUE;
}

static int cpkt_valid_datetime_words(long high32, unsigned long low32) {
  return high32 >= CPKT_OPCUA_INT32_MIN_VALUE && high32 <= CPKT_OPCUA_INT32_MAX_VALUE &&
         low32 <= CPKT_OPCUA_UINT32_MAX_VALUE;
}

static UA_UInt64 cpkt_make_uint64(cpkt_opcua_uint64 value) {
  return ((UA_UInt64)value.high32 << 32) | (UA_UInt64)value.low32;
}

static UA_DateTime cpkt_make_datetime(cpkt_opcua_datetime value) {
  return (UA_DateTime)((UA_Int64)value.high32 * ((UA_Int64)CPKT_OPCUA_UINT32_MAX_VALUE + 1) +
                       (UA_Int64)value.low32);
}

static cpkt_opcua_uint64 cpkt_uint64_from_native(UA_UInt64 value) {
  cpkt_opcua_uint64 out;

  out.high32 = (unsigned long)(value >> 32);
  out.low32 = (unsigned long)(value & (UA_UInt64)CPKT_OPCUA_UINT32_MAX_VALUE);
  return out;
}

static cpkt_opcua_datetime cpkt_datetime_from_native(UA_DateTime value) {
  cpkt_opcua_datetime out;
  UA_UInt64 bits;
  unsigned long high32;

  bits = (UA_UInt64)value;
  high32 = (unsigned long)(bits >> 32);
  if (high32 <= (unsigned long)CPKT_OPCUA_INT32_MAX_VALUE) {
    out.high32 = (long)high32;
  } else {
    out.high32 = -1L - (long)(CPKT_OPCUA_UINT32_MAX_VALUE - high32);
  }
  out.low32 = (unsigned long)(bits & (UA_UInt64)CPKT_OPCUA_UINT32_MAX_VALUE);
  return out;
}

static UA_Guid cpkt_make_guid(const unsigned char guid[16]) {
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

static void cpkt_guid_from_native(UA_Guid native_guid, unsigned char guid[16]) {
  guid[0] = (unsigned char)((native_guid.data1 >> 24) & 0xffU);
  guid[1] = (unsigned char)((native_guid.data1 >> 16) & 0xffU);
  guid[2] = (unsigned char)((native_guid.data1 >> 8) & 0xffU);
  guid[3] = (unsigned char)(native_guid.data1 & 0xffU);
  guid[4] = (unsigned char)((native_guid.data2 >> 8) & 0xffU);
  guid[5] = (unsigned char)(native_guid.data2 & 0xffU);
  guid[6] = (unsigned char)((native_guid.data3 >> 8) & 0xffU);
  guid[7] = (unsigned char)(native_guid.data3 & 0xffU);
  memcpy(guid + 8, native_guid.data4, 8);
}

static UA_NodeId cpkt_make_node_id(cpkt_opcua_node_id node_id) {
  UA_NodeId native_node_id;

  UA_NodeId_init(&native_node_id);
  native_node_id.namespaceIndex = (UA_UInt16)node_id.namespace_index;
  if (node_id.identifier_type == CPKT_OPCUA_NODE_ID_NULL) {
    return native_node_id;
  }
  if (node_id.identifier_type == CPKT_OPCUA_NODE_ID_NUMERIC) {
    native_node_id.identifierType = UA_NODEIDTYPE_NUMERIC;
    native_node_id.identifier.numeric = (UA_UInt32)node_id.numeric;
    return native_node_id;
  }
  if (node_id.identifier_type == CPKT_OPCUA_NODE_ID_STRING) {
    native_node_id.identifierType = UA_NODEIDTYPE_STRING;
    native_node_id.identifier.string = UA_STRING((char *)node_id.string);
    return native_node_id;
  }
  if (node_id.identifier_type == CPKT_OPCUA_NODE_ID_GUID) {
    native_node_id.identifierType = UA_NODEIDTYPE_GUID;
    native_node_id.identifier.guid = cpkt_make_guid(node_id.guid);
    return native_node_id;
  }
  native_node_id.identifierType = UA_NODEIDTYPE_BYTESTRING;
  native_node_id.identifier.byteString.length = node_id.byte_string_length;
  native_node_id.identifier.byteString.data = (UA_Byte *)node_id.byte_string;
  return native_node_id;
}

static UA_ExpandedNodeId cpkt_make_expanded_node_id(cpkt_opcua_expanded_node_id node_id) {
  UA_ExpandedNodeId native_node_id;

  native_node_id = UA_EXPANDEDNODEID_NODEID(cpkt_make_node_id(node_id.node_id));
  native_node_id.serverIndex = (UA_UInt32)node_id.server_index;
  if (node_id.namespace_uri != NULL || node_id.namespace_uri_length != 0) {
    native_node_id.namespaceUri = UA_STRING_NULL;
    native_node_id.namespaceUri.data = (UA_Byte *)node_id.namespace_uri;
    native_node_id.namespaceUri.length = node_id.namespace_uri_length;
  }
  return native_node_id;
}

static UA_ByteString cpkt_make_borrowed_byte_string(
    const unsigned char *data,
    size_t length) {
  UA_ByteString bytes;

  bytes.data = (UA_Byte *)data;
  bytes.length = length;
  return bytes;
}

static cpkt_opcua_result cpkt_make_borrowed_byte_string_array(
    const cpkt_opcua_byte_string_view *views,
    size_t view_count,
    UA_ByteString **bytes_out) {
  UA_ByteString *bytes;
  size_t i;

  if (bytes_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  *bytes_out = NULL;
  if (view_count == 0) {
    return CPKT_OPCUA_OK;
  }
  if (views == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  bytes = (UA_ByteString *)calloc(view_count, sizeof(*bytes));
  if (bytes == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  for (i = 0; i < view_count; ++i) {
    if (views[i].data == NULL && views[i].length != 0) {
      free(bytes);
      return CPKT_OPCUA_ERR_ARG;
    }
    bytes[i] = cpkt_make_borrowed_byte_string(views[i].data, views[i].length);
  }
  *bytes_out = bytes;
  return CPKT_OPCUA_OK;
}

static int cpkt_valid_node_id(cpkt_opcua_node_id node_id) {
  if (node_id.identifier_type == CPKT_OPCUA_NODE_ID_NULL) {
    return node_id.namespace_index == 0;
  }
  if (node_id.identifier_type == CPKT_OPCUA_NODE_ID_NUMERIC) {
    return node_id.numeric <= (unsigned long)UINT_MAX;
  }
  if (node_id.identifier_type == CPKT_OPCUA_NODE_ID_STRING) {
    return node_id.string != NULL;
  }
  if (node_id.identifier_type == CPKT_OPCUA_NODE_ID_GUID) {
    return 1;
  }
  if (node_id.identifier_type == CPKT_OPCUA_NODE_ID_BYTE_STRING) {
    return node_id.byte_string != NULL || node_id.byte_string_length == 0;
  }
  return 0;
}

static int cpkt_valid_expanded_node_id(cpkt_opcua_expanded_node_id node_id) {
  if (!cpkt_valid_node_id(node_id.node_id) || node_id.server_index > (unsigned long)UINT_MAX) {
    return 0;
  }
  if (node_id.namespace_uri == NULL && node_id.namespace_uri_length != 0) {
    return 0;
  }
  return 1;
}

static int cpkt_valid_node_class(unsigned long node_class) {
  switch (node_class) {
    case CPKT_OPCUA_NODE_CLASS_UNSPECIFIED:
    case CPKT_OPCUA_NODE_CLASS_OBJECT:
    case CPKT_OPCUA_NODE_CLASS_VARIABLE:
    case CPKT_OPCUA_NODE_CLASS_METHOD:
    case CPKT_OPCUA_NODE_CLASS_OBJECT_TYPE:
    case CPKT_OPCUA_NODE_CLASS_VARIABLE_TYPE:
    case CPKT_OPCUA_NODE_CLASS_REFERENCE_TYPE:
    case CPKT_OPCUA_NODE_CLASS_DATA_TYPE:
    case CPKT_OPCUA_NODE_CLASS_VIEW:
      return 1;
    default:
      return 0;
  }
}

static int cpkt_parse_unsigned_long(
    const char *text,
    const char **end_out,
    unsigned long *value_out) {
  char *end;
  unsigned long value;

  if (text == NULL || end_out == NULL || value_out == NULL || *text < '0' || *text > '9') {
    return 0;
  }
  errno = 0;
  value = strtoul(text, &end, 10);
  if (errno == ERANGE || end == text) {
    return 0;
  }
  *end_out = end;
  *value_out = value;
  return 1;
}

static cpkt_opcua_result cpkt_copy_c_string_to_buffer(
    const char *value,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out) {
  size_t required;

  if (value == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  required = strlen(value) + 1;
  if (required_size_out != NULL) {
    *required_size_out = required;
  }
  if (buffer == NULL || buffer_size < required) {
    if (buffer != NULL && buffer_size != 0) {
      buffer[0] = '\0';
    }
    return CPKT_OPCUA_ERR_RANGE;
  }
  memcpy(buffer, value, required);
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_copy_counted_string_to_buffer(
    const char *value,
    size_t value_length,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out) {
  size_t required;

  if (value == NULL && value_length != 0) {
    return CPKT_OPCUA_ERR_ARG;
  }
  required = value_length + 1;
  if (required_size_out != NULL) {
    *required_size_out = required;
  }
  if (buffer == NULL || buffer_size < required) {
    if (buffer != NULL && buffer_size != 0) {
      buffer[0] = '\0';
    }
    return CPKT_OPCUA_ERR_RANGE;
  }
  if (value_length != 0) {
    memcpy(buffer, value, value_length);
  }
  buffer[value_length] = '\0';
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_copy_ua_byte_string_to_buffer(
    const UA_ByteString *value,
    unsigned char *buffer,
    size_t buffer_size,
    size_t *required_size_out) {
  size_t required;

  if (value == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  required = value->length;
  if (required_size_out != NULL) {
    *required_size_out = required;
  }
  if (required == 0) {
    return CPKT_OPCUA_OK;
  }
  if (buffer == NULL || buffer_size < required) {
    if (buffer != NULL && buffer_size != 0) {
      memset(buffer, 0, buffer_size);
    }
    return CPKT_OPCUA_ERR_RANGE;
  }
  memcpy(buffer, value->data, required);
  return CPKT_OPCUA_OK;
}

static int cpkt_native_node_id_to_facade(
    const UA_NodeId *native,
    cpkt_opcua_node_id *out,
    struct cpkt_owned_node_id_memory *owned_out) {
  char *owned;
  unsigned char *owned_bytes;

  if (native == NULL || out == NULL || owned_out == NULL) {
    return 0;
  }
  owned_out->string = NULL;
  owned_out->byte_string = NULL;
  if (native->identifierType == UA_NODEIDTYPE_NUMERIC && native->namespaceIndex == 0 &&
      native->identifier.numeric == 0) {
    *out = cpkt_opcua_node_id_null();
    return 1;
  }
  if (native->identifierType == UA_NODEIDTYPE_NUMERIC) {
    *out = cpkt_opcua_node_id_numeric(
        (unsigned short)native->namespaceIndex,
        (unsigned long)native->identifier.numeric);
    return 1;
  }
  if (native->identifierType == UA_NODEIDTYPE_STRING) {
    owned = (char *)malloc(native->identifier.string.length + 1);
    if (owned == NULL) {
      return 0;
    }
    if (native->identifier.string.length != 0) {
      memcpy(owned, native->identifier.string.data, native->identifier.string.length);
    }
    owned[native->identifier.string.length] = '\0';
    *out = cpkt_opcua_node_id_string((unsigned short)native->namespaceIndex, owned);
    owned_out->string = owned;
    return 1;
  }
  if (native->identifierType == UA_NODEIDTYPE_GUID) {
    *out = cpkt_opcua_node_id_null();
    out->namespace_index = (unsigned short)native->namespaceIndex;
    out->identifier_type = CPKT_OPCUA_NODE_ID_GUID;
    cpkt_guid_from_native(native->identifier.guid, out->guid);
    return 1;
  }
  if (native->identifierType == UA_NODEIDTYPE_BYTESTRING) {
    if (native->identifier.byteString.length != 0) {
      owned_bytes = (unsigned char *)malloc(native->identifier.byteString.length);
      if (owned_bytes == NULL) {
        return 0;
      }
      memcpy(owned_bytes, native->identifier.byteString.data, native->identifier.byteString.length);
    } else {
      owned_bytes = NULL;
    }
    *out = cpkt_opcua_node_id_byte_string(
        (unsigned short)native->namespaceIndex,
        owned_bytes,
        native->identifier.byteString.length);
    owned_out->byte_string = owned_bytes;
    return 1;
  }
  return 0;
}

static void cpkt_owned_node_id_memory_clear(struct cpkt_owned_node_id_memory *owned) {
  if (owned != NULL) {
    free(owned->string);
    free(owned->byte_string);
    owned->string = NULL;
    owned->byte_string = NULL;
  }
}

static char *cpkt_copy_ua_string(const UA_String *value) {
  char *copy;

  if (value == NULL) {
    return NULL;
  }
  copy = (char *)malloc(value->length + 1);
  if (copy == NULL) {
    return NULL;
  }
  if (value->length != 0) {
    memcpy(copy, value->data, value->length);
  }
  copy[value->length] = '\0';
  return copy;
}

static char *cpkt_strdup_c89(const char *value) {
  char *copy;
  size_t length;

  if (value == NULL) {
    return NULL;
  }
  length = strlen(value) + 1;
  copy = (char *)malloc(length);
  if (copy == NULL) {
    return NULL;
  }
  memcpy(copy, value, length);
  return copy;
}

static unsigned char *cpkt_memdup_c89(const unsigned char *value, size_t length) {
  unsigned char *copy;

  if (value == NULL && length != 0) {
    return NULL;
  }
  if (length == 0) {
    return NULL;
  }
  copy = (unsigned char *)malloc(length);
  if (copy == NULL) {
    return NULL;
  }
  memcpy(copy, value, length);
  return copy;
}

static cpkt_opcua_result cpkt_copy_facade_node_id(
    cpkt_opcua_node_id in,
    cpkt_opcua_node_id *out,
    struct cpkt_owned_node_id_memory *owned_out) {
  char *string_copy;
  unsigned char *bytes_copy;

  if (out == NULL || owned_out == NULL || !cpkt_valid_node_id(in)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  owned_out->string = NULL;
  owned_out->byte_string = NULL;
  *out = in;
  if (in.identifier_type == CPKT_OPCUA_NODE_ID_STRING) {
    string_copy = cpkt_strdup_c89(in.string);
    if (string_copy == NULL) {
      return CPKT_OPCUA_ERR_ALLOC;
    }
    out->string = string_copy;
    owned_out->string = string_copy;
  } else if (in.identifier_type == CPKT_OPCUA_NODE_ID_BYTE_STRING && in.byte_string_length != 0) {
    bytes_copy = cpkt_memdup_c89(in.byte_string, in.byte_string_length);
    if (bytes_copy == NULL) {
      return CPKT_OPCUA_ERR_ALLOC;
    }
    out->byte_string = bytes_copy;
    owned_out->byte_string = bytes_copy;
  }
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_copy_native_node_id_to_output(
    const UA_NodeId *native,
    cpkt_opcua_node_id *node_id_out,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out) {
  cpkt_opcua_node_id facade_node_id;
  struct cpkt_owned_node_id_memory owned;
  size_t required;

  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (node_id_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  *node_id_out = cpkt_opcua_node_id_null();
  if (!cpkt_native_node_id_to_facade(native, &facade_node_id, &owned)) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  if (facade_node_id.identifier_type == CPKT_OPCUA_NODE_ID_STRING) {
    required = strlen(facade_node_id.string) + 1;
    if (required_size_out != NULL) {
      *required_size_out = required;
    }
    if (buffer == NULL || buffer_size < required) {
      cpkt_owned_node_id_memory_clear(&owned);
      return CPKT_OPCUA_ERR_RANGE;
    }
    memcpy(buffer, facade_node_id.string, required);
    facade_node_id.string = buffer;
  } else if (facade_node_id.identifier_type == CPKT_OPCUA_NODE_ID_BYTE_STRING) {
    required = facade_node_id.byte_string_length;
    if (required_size_out != NULL) {
      *required_size_out = required;
    }
    if (required != 0 && (buffer == NULL || buffer_size < required)) {
      cpkt_owned_node_id_memory_clear(&owned);
      return CPKT_OPCUA_ERR_RANGE;
    }
    if (required != 0) {
      memcpy(buffer, facade_node_id.byte_string, required);
    }
    facade_node_id.byte_string = (const unsigned char *)buffer;
  }
  *node_id_out = facade_node_id;
  cpkt_owned_node_id_memory_clear(&owned);
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_copy_ua_string_to_buffer(
    const UA_String *value,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out) {
  size_t required;

  if (value == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  required = value->length + 1;
  if (required_size_out != NULL) {
    *required_size_out = required;
  }
  if (buffer == NULL || buffer_size < required) {
    if (buffer != NULL && buffer_size != 0) {
      buffer[0] = '\0';
    }
    return CPKT_OPCUA_ERR_RANGE;
  }
  if (value->length != 0) {
    memcpy(buffer, value->data, value->length);
  }
  buffer[value->length] = '\0';
  return CPKT_OPCUA_OK;
}

static int cpkt_ulong_fits_uint32(unsigned long value) {
  return value <= (unsigned long)UINT_MAX;
}

static int cpkt_long_fits_int32(long value) {
  return value >= (long)INT_MIN && value <= (long)INT_MAX;
}

static cpkt_opcua_result cpkt_copy_array_dimensions_to_buffer(
    const UA_UInt32 *native_dimensions,
    size_t native_dimension_count,
    unsigned long *dimensions,
    size_t dimension_count,
    size_t *required_dimension_count_out) {
  size_t i;

  if (required_dimension_count_out != NULL) {
    *required_dimension_count_out = native_dimension_count;
  }
  if (native_dimension_count == 0) {
    return CPKT_OPCUA_OK;
  }
  if (dimensions == NULL || dimension_count < native_dimension_count) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  for (i = 0; i < native_dimension_count; ++i) {
    dimensions[i] = (unsigned long)native_dimensions[i];
  }
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_make_array_dimensions(
    const unsigned long *dimensions,
    size_t dimension_count,
    UA_UInt32 **native_dimensions_out) {
  UA_UInt32 *native_dimensions;
  size_t i;

  if (native_dimensions_out != NULL) {
    *native_dimensions_out = NULL;
  }
  if (native_dimensions_out == NULL || (dimension_count != 0 && dimensions == NULL)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (dimension_count == 0) {
    return CPKT_OPCUA_OK;
  }
  native_dimensions = (UA_UInt32 *)calloc(dimension_count, sizeof(*native_dimensions));
  if (native_dimensions == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  for (i = 0; i < dimension_count; ++i) {
    if (!cpkt_ulong_fits_uint32(dimensions[i])) {
      free(native_dimensions);
      return CPKT_OPCUA_ERR_RANGE;
    }
    native_dimensions[i] = (UA_UInt32)dimensions[i];
  }
  *native_dimensions_out = native_dimensions;
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_apply_value_shape_to_variable_attributes(
    UA_VariableAttributes *attr,
    const cpkt_opcua_value *value) {
  size_t array_length;

  if (attr == NULL || value == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  array_length = 0;
  switch (value->type) {
    case CPKT_OPCUA_VALUE_BOOLEAN_ARRAY:
      array_length = value->boolean_array_length;
      break;
    case CPKT_OPCUA_VALUE_INTEGER_ARRAY:
      array_length = value->integer_array_length;
      break;
    case CPKT_OPCUA_VALUE_UINT64_ARRAY:
      array_length = value->uint64_array_length;
      break;
    case CPKT_OPCUA_VALUE_DATETIME_ARRAY:
      array_length = value->datetime_array_length;
      break;
    case CPKT_OPCUA_VALUE_STATUS_ARRAY:
      array_length = value->status_array_length;
      break;
    case CPKT_OPCUA_VALUE_GUID_ARRAY:
      array_length = value->guid_array_length;
      break;
    case CPKT_OPCUA_VALUE_QUALIFIED_NAME_ARRAY:
      array_length = value->qualified_name_array_length;
      break;
    case CPKT_OPCUA_VALUE_LOCALIZED_TEXT_ARRAY:
      array_length = value->localized_text_array_length;
      break;
    case CPKT_OPCUA_VALUE_DOUBLE_ARRAY:
      array_length = value->double_array_length;
      break;
    case CPKT_OPCUA_VALUE_STRING_ARRAY:
      array_length = value->string_array_length;
      break;
    case CPKT_OPCUA_VALUE_BYTE_STRING_ARRAY:
      array_length = value->byte_string_array_length;
      break;
    default:
      attr->valueRank = UA_VALUERANK_SCALAR;
      return CPKT_OPCUA_OK;
  }
  if (array_length > (size_t)UINT32_MAX) {
      return CPKT_OPCUA_ERR_RANGE;
  }
  attr->valueRank = 1;
  attr->arrayDimensions = (UA_UInt32 *)UA_Array_new(1, &UA_TYPES[UA_TYPES_UINT32]);
  if (attr->arrayDimensions == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  attr->arrayDimensionsSize = 1;
  attr->arrayDimensions[0] = (UA_UInt32)array_length;
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_apply_value_shape_to_variable_type_attributes(
    UA_VariableTypeAttributes *attr,
    const cpkt_opcua_value *value) {
  UA_VariableAttributes shape;
  cpkt_opcua_result result;

  if (attr == NULL || value == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  shape = UA_VariableAttributes_default;
  result = cpkt_apply_value_shape_to_variable_attributes(&shape, value);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  attr->valueRank = shape.valueRank;
  attr->arrayDimensions = shape.arrayDimensions;
  attr->arrayDimensionsSize = shape.arrayDimensionsSize;
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_copy_node_id_to_caller(
    const cpkt_opcua_node_id *source,
    const struct cpkt_owned_node_id_memory *owned,
    cpkt_opcua_node_id *target_out,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out) {
  size_t required;

  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (target_out != NULL) {
    *target_out = cpkt_opcua_node_id_null();
  }
  if (source == NULL || owned == NULL || target_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (source->identifier_type == CPKT_OPCUA_NODE_ID_STRING) {
    if (owned->string == NULL) {
      return CPKT_OPCUA_ERR_TYPE;
    }
    required = strlen(owned->string) + 1;
    if (required_size_out != NULL) {
      *required_size_out = required;
    }
    if (buffer == NULL || buffer_size < required) {
      if (buffer != NULL && buffer_size != 0) {
        buffer[0] = '\0';
      }
      return CPKT_OPCUA_ERR_RANGE;
    }
    memcpy(buffer, owned->string, required);
    *target_out = cpkt_opcua_node_id_string(source->namespace_index, buffer);
    return CPKT_OPCUA_OK;
  }
  if (source->identifier_type == CPKT_OPCUA_NODE_ID_BYTE_STRING) {
    required = source->byte_string_length;
    if (required_size_out != NULL) {
      *required_size_out = required;
    }
    if (required != 0 && (buffer == NULL || buffer_size < required)) {
      if (buffer != NULL && buffer_size != 0) {
        buffer[0] = '\0';
      }
      return CPKT_OPCUA_ERR_RANGE;
    }
    if (required != 0) {
      memcpy(buffer, owned->byte_string, required);
    }
    *target_out = cpkt_opcua_node_id_byte_string(
        source->namespace_index,
        (const unsigned char *)buffer,
        required);
    return CPKT_OPCUA_OK;
  }
  *target_out = *source;
  return CPKT_OPCUA_OK;
}

static const UA_DataType *cpkt_data_type_for_value_type(int type) {
  switch (type) {
    case CPKT_OPCUA_VALUE_BOOLEAN:
      return &UA_TYPES[UA_TYPES_BOOLEAN];
    case CPKT_OPCUA_VALUE_INTEGER:
      return &UA_TYPES[UA_TYPES_INT32];
    case CPKT_OPCUA_VALUE_UINT64:
      return &UA_TYPES[UA_TYPES_UINT64];
    case CPKT_OPCUA_VALUE_DATETIME:
      return &UA_TYPES[UA_TYPES_DATETIME];
    case CPKT_OPCUA_VALUE_DOUBLE:
      return &UA_TYPES[UA_TYPES_DOUBLE];
    case CPKT_OPCUA_VALUE_STRING:
      return &UA_TYPES[UA_TYPES_STRING];
    case CPKT_OPCUA_VALUE_BYTE_STRING:
      return &UA_TYPES[UA_TYPES_BYTESTRING];
    case CPKT_OPCUA_VALUE_GUID:
      return &UA_TYPES[UA_TYPES_GUID];
    case CPKT_OPCUA_VALUE_STATUS:
      return &UA_TYPES[UA_TYPES_STATUSCODE];
    case CPKT_OPCUA_VALUE_QUALIFIED_NAME:
      return &UA_TYPES[UA_TYPES_QUALIFIEDNAME];
    case CPKT_OPCUA_VALUE_LOCALIZED_TEXT:
      return &UA_TYPES[UA_TYPES_LOCALIZEDTEXT];
    default:
      return NULL;
  }
}

static UA_NodeId cpkt_data_type_node_id_for_value_type(int type) {
  switch (type) {
    case CPKT_OPCUA_VALUE_EMPTY:
      return UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATATYPE);
    case CPKT_OPCUA_VALUE_BOOLEAN:
    case CPKT_OPCUA_VALUE_BOOLEAN_ARRAY:
      return UA_NODEID_NUMERIC(0, UA_NS0ID_BOOLEAN);
    case CPKT_OPCUA_VALUE_INTEGER:
    case CPKT_OPCUA_VALUE_INTEGER_ARRAY:
      return UA_NODEID_NUMERIC(0, UA_NS0ID_INT32);
    case CPKT_OPCUA_VALUE_UINT64:
    case CPKT_OPCUA_VALUE_UINT64_ARRAY:
      return UA_NODEID_NUMERIC(0, UA_NS0ID_UINT64);
    case CPKT_OPCUA_VALUE_DATETIME:
    case CPKT_OPCUA_VALUE_DATETIME_ARRAY:
      return UA_NODEID_NUMERIC(0, UA_NS0ID_DATETIME);
    case CPKT_OPCUA_VALUE_DOUBLE:
    case CPKT_OPCUA_VALUE_DOUBLE_ARRAY:
      return UA_NODEID_NUMERIC(0, UA_NS0ID_DOUBLE);
    case CPKT_OPCUA_VALUE_STRING:
    case CPKT_OPCUA_VALUE_STRING_ARRAY:
      return UA_NODEID_NUMERIC(0, UA_NS0ID_STRING);
    case CPKT_OPCUA_VALUE_BYTE_STRING:
    case CPKT_OPCUA_VALUE_BYTE_STRING_ARRAY:
      return UA_NODEID_NUMERIC(0, UA_NS0ID_BYTESTRING);
    case CPKT_OPCUA_VALUE_GUID:
    case CPKT_OPCUA_VALUE_GUID_ARRAY:
      return UA_NODEID_NUMERIC(0, UA_NS0ID_GUID);
    case CPKT_OPCUA_VALUE_STATUS:
    case CPKT_OPCUA_VALUE_STATUS_ARRAY:
      return UA_NODEID_NUMERIC(0, UA_NS0ID_STATUSCODE);
    case CPKT_OPCUA_VALUE_QUALIFIED_NAME:
    case CPKT_OPCUA_VALUE_QUALIFIED_NAME_ARRAY:
      return UA_NODEID_NUMERIC(0, UA_NS0ID_QUALIFIEDNAME);
    case CPKT_OPCUA_VALUE_LOCALIZED_TEXT:
    case CPKT_OPCUA_VALUE_LOCALIZED_TEXT_ARRAY:
      return UA_NODEID_NUMERIC(0, UA_NS0ID_LOCALIZEDTEXT);
    default:
      return UA_NODEID_NUMERIC(0, 0);
  }
}

static int cpkt_valid_value_type(int type) {
  return type == CPKT_OPCUA_VALUE_EMPTY || cpkt_data_type_for_value_type(type) != NULL;
}

static int cpkt_value_type_is_array(int type) {
  return type == CPKT_OPCUA_VALUE_BOOLEAN_ARRAY ||
      type == CPKT_OPCUA_VALUE_INTEGER_ARRAY ||
      type == CPKT_OPCUA_VALUE_UINT64_ARRAY ||
      type == CPKT_OPCUA_VALUE_DATETIME_ARRAY ||
      type == CPKT_OPCUA_VALUE_STATUS_ARRAY ||
      type == CPKT_OPCUA_VALUE_GUID_ARRAY ||
      type == CPKT_OPCUA_VALUE_QUALIFIED_NAME_ARRAY ||
      type == CPKT_OPCUA_VALUE_LOCALIZED_TEXT_ARRAY ||
      type == CPKT_OPCUA_VALUE_DOUBLE_ARRAY ||
      type == CPKT_OPCUA_VALUE_STRING_ARRAY ||
      type == CPKT_OPCUA_VALUE_BYTE_STRING_ARRAY;
}

static int cpkt_valid_method_value_type(int type) {
  return cpkt_valid_value_type(type) && !cpkt_value_type_is_array(type);
}

static int cpkt_value_type_needs_buffer(int type) {
  return type == CPKT_OPCUA_VALUE_STRING ||
      type == CPKT_OPCUA_VALUE_BYTE_STRING ||
      type == CPKT_OPCUA_VALUE_QUALIFIED_NAME ||
      type == CPKT_OPCUA_VALUE_LOCALIZED_TEXT;
}

static cpkt_opcua_result cpkt_browse_result_each(
    UA_BrowseResult *browse_result,
    cpkt_opcua_browse_fn fn,
    void *user,
    cpkt_opcua_status *status_out) {
  size_t i;
  cpkt_opcua_browse_entry entry;
  UA_ReferenceDescription *reference;
  struct cpkt_owned_node_id_memory node_id_memory;
  char *browse_name;
  char *display_name;

  if (status_out != NULL) {
    *status_out = cpkt_status(browse_result->statusCode);
  }
  if (browse_result->statusCode != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }

  for (i = 0; i < browse_result->referencesSize; ++i) {
    reference = &browse_result->references[i];
    node_id_memory.string = NULL;
    node_id_memory.byte_string = NULL;
    browse_name = NULL;
    display_name = NULL;
    if (reference->nodeId.serverIndex != 0 ||
        !cpkt_native_node_id_to_facade(&reference->nodeId.nodeId, &entry.target_node_id, &node_id_memory)) {
      cpkt_owned_node_id_memory_clear(&node_id_memory);
      return CPKT_OPCUA_ERR_TYPE;
    }
    browse_name = cpkt_copy_ua_string(&reference->browseName.name);
    if (browse_name == NULL) {
      cpkt_owned_node_id_memory_clear(&node_id_memory);
      return CPKT_OPCUA_ERR_ALLOC;
    }
    display_name = cpkt_copy_ua_string(&reference->displayName.text);
    if (display_name == NULL) {
      free(browse_name);
      cpkt_owned_node_id_memory_clear(&node_id_memory);
      return CPKT_OPCUA_ERR_ALLOC;
    }
    entry.node_class = (unsigned long)reference->nodeClass;
    entry.browse_name_namespace_index = (unsigned short)reference->browseName.namespaceIndex;
    entry.browse_name = browse_name;
    entry.display_name = display_name;
    entry.is_forward = reference->isForward ? 1 : 0;
    if (fn(&entry, user) != 0) {
      free(display_name);
      free(browse_name);
      cpkt_owned_node_id_memory_clear(&node_id_memory);
      return CPKT_OPCUA_ERR_CALLBACK;
    }
    free(display_name);
    free(browse_name);
    cpkt_owned_node_id_memory_clear(&node_id_memory);
  }

  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_browse_result_page(
    UA_BrowseResult *browse_result,
    cpkt_opcua_browse_fn fn,
    void *user,
    unsigned char *continuation_point_buffer,
    size_t continuation_point_buffer_size,
    size_t *required_continuation_point_size_out,
    cpkt_opcua_status *status_out) {
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = cpkt_status(browse_result->statusCode);
  }
  if (required_continuation_point_size_out != NULL) {
    *required_continuation_point_size_out = 0;
  }
  if (browse_result->statusCode != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_ua_byte_string_to_buffer(
      &browse_result->continuationPoint,
      continuation_point_buffer,
      continuation_point_buffer_size,
      required_continuation_point_size_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  if (fn == NULL) {
    return CPKT_OPCUA_OK;
  }
  return cpkt_browse_result_each(browse_result, fn, user, status_out);
}

static void cpkt_server_release_browse_continuation_point(
    UA_Server *server,
    const UA_ByteString *continuation_point) {
  UA_BrowseResult release_result;

  if (server == NULL || continuation_point == NULL || continuation_point->length == 0) {
    return;
  }
  release_result = UA_Server_browseNext(server, true, continuation_point);
  UA_BrowseResult_clear(&release_result);
}

static void cpkt_client_release_browse_continuation_point(
    UA_Client *client,
    UA_ByteString continuation_point) {
  UA_BrowseResult release_result;

  if (client == NULL || continuation_point.length == 0) {
    return;
  }
  release_result = UA_Client_browseNext(client, true, continuation_point);
  UA_BrowseResult_clear(&release_result);
}

static cpkt_opcua_result cpkt_fill_browse_description(
    UA_BrowseDescription *browse_description,
    cpkt_opcua_node_id parent_node_id,
    const cpkt_opcua_browse_options *options) {
  UA_NodeId reference_type_id;

  if (browse_description == NULL || !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (options != NULL) {
    if (options->browse_direction != CPKT_OPCUA_BROWSE_FORWARD &&
        options->browse_direction != CPKT_OPCUA_BROWSE_INVERSE &&
        options->browse_direction != CPKT_OPCUA_BROWSE_BOTH) {
      return CPKT_OPCUA_ERR_ARG;
    }
    if (options->has_reference_type && !cpkt_valid_node_id(options->reference_type_id)) {
      return CPKT_OPCUA_ERR_ARG;
    }
    if (options->max_references > (unsigned long)UINT_MAX ||
        options->node_class_mask > (unsigned long)UINT_MAX ||
        options->result_mask > (unsigned long)UINT_MAX) {
      return CPKT_OPCUA_ERR_RANGE;
    }
  }

  UA_BrowseDescription_init(browse_description);
  browse_description->nodeId = cpkt_make_node_id(parent_node_id);
  browse_description->browseDirection = UA_BROWSEDIRECTION_FORWARD;
  browse_description->referenceTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_HIERARCHICALREFERENCES);
  browse_description->includeSubtypes = true;
  browse_description->nodeClassMask = 0;
  browse_description->resultMask = UA_BROWSERESULTMASK_ALL;

  if (options == NULL) {
    return CPKT_OPCUA_OK;
  }
  if (options->browse_direction == CPKT_OPCUA_BROWSE_INVERSE) {
    browse_description->browseDirection = UA_BROWSEDIRECTION_INVERSE;
  } else if (options->browse_direction == CPKT_OPCUA_BROWSE_BOTH) {
    browse_description->browseDirection = UA_BROWSEDIRECTION_BOTH;
  }
  if (options->has_reference_type) {
    reference_type_id = cpkt_make_node_id(options->reference_type_id);
    browse_description->referenceTypeId = reference_type_id;
  }
  browse_description->includeSubtypes = options->include_subtypes ? true : false;
  browse_description->nodeClassMask = (UA_UInt32)options->node_class_mask;
  browse_description->resultMask = (UA_UInt32)options->result_mask;
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_fill_browse_path(
    UA_BrowsePath *browse_path,
    cpkt_opcua_node_id start_node_id,
    const cpkt_opcua_browse_path_element *elements,
    size_t element_count) {
  UA_NodeId native_start_node_id;
  size_t i;

  if (browse_path == NULL || elements == NULL || element_count == 0 ||
      !cpkt_valid_node_id(start_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  UA_BrowsePath_init(browse_path);
  native_start_node_id = cpkt_make_node_id(start_node_id);
  if (UA_NodeId_copy(&native_start_node_id, &browse_path->startingNode) != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  browse_path->relativePath.elements =
      (UA_RelativePathElement *)calloc(element_count, sizeof(*browse_path->relativePath.elements));
  if (browse_path->relativePath.elements == NULL) {
    UA_BrowsePath_clear(browse_path);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  browse_path->relativePath.elementsSize = element_count;
  for (i = 0; i < element_count; ++i) {
    if (elements[i].browse_name == NULL) {
      UA_BrowsePath_clear(browse_path);
      return CPKT_OPCUA_ERR_ARG;
    }
    UA_RelativePathElement_init(&browse_path->relativePath.elements[i]);
    browse_path->relativePath.elements[i].referenceTypeId =
        UA_NODEID_NUMERIC(0, UA_NS0ID_HIERARCHICALREFERENCES);
    browse_path->relativePath.elements[i].isInverse = false;
    browse_path->relativePath.elements[i].includeSubtypes = true;
    browse_path->relativePath.elements[i].targetName.namespaceIndex =
        (UA_UInt16)elements[i].namespace_index;
    browse_path->relativePath.elements[i].targetName.name =
        UA_STRING_ALLOC((char *)elements[i].browse_name);
    if (elements[i].browse_name[0] != '\0' &&
        browse_path->relativePath.elements[i].targetName.name.data == NULL) {
      UA_BrowsePath_clear(browse_path);
      return CPKT_OPCUA_ERR_ALLOC;
    }
  }
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_translate_browse_path_result_to_target(
    UA_BrowsePathResult *browse_path_result,
    cpkt_opcua_node_id *target_node_id_out,
    char *target_buffer,
    size_t target_buffer_size,
    size_t *required_target_size_out,
    cpkt_opcua_status *status_out) {
  struct cpkt_owned_node_id_memory target_memory;
  cpkt_opcua_node_id target;
  cpkt_opcua_result result;

  if (required_target_size_out != NULL) {
    *required_target_size_out = 0;
  }
  if (target_node_id_out != NULL) {
    *target_node_id_out = cpkt_opcua_node_id_null();
  }
  if (browse_path_result == NULL || target_node_id_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (status_out != NULL) {
    *status_out = cpkt_status(browse_path_result->statusCode);
  }
  if (browse_path_result->statusCode != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (browse_path_result->targetsSize == 0) {
    if (status_out != NULL) {
      *status_out = cpkt_status(UA_STATUSCODE_BADNOMATCH);
    }
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (browse_path_result->targets[0].targetId.serverIndex != 0 ||
      browse_path_result->targets[0].targetId.namespaceUri.length != 0 ||
      browse_path_result->targets[0].remainingPathIndex != (UA_UInt32)UINT_MAX) {
    return CPKT_OPCUA_ERR_TYPE;
  }
  target_memory.string = NULL;
  target_memory.byte_string = NULL;
  if (!cpkt_native_node_id_to_facade(
          &browse_path_result->targets[0].targetId.nodeId,
          &target,
          &target_memory)) {
    cpkt_owned_node_id_memory_clear(&target_memory);
    return CPKT_OPCUA_ERR_TYPE;
  }
  result = cpkt_copy_node_id_to_caller(
      &target,
      &target_memory,
      target_node_id_out,
      target_buffer,
      target_buffer_size,
      required_target_size_out);
  cpkt_owned_node_id_memory_clear(&target_memory);
  return result;
}

static void cpkt_remove_monitor_context(
    cpkt_opcua_client *client,
    struct cpkt_opcua_monitor_context *context) {
  struct cpkt_opcua_monitor_context **slot;

  if (client == NULL || context == NULL) {
    return;
  }
  slot = &client->monitors;
  while (*slot != NULL) {
    if (*slot == context) {
      *slot = context->next;
      return;
    }
    slot = &(*slot)->next;
  }
}

static void cpkt_monitor_context_free(struct cpkt_opcua_monitor_context *context) {
  size_t i;

  if (context == NULL) {
    return;
  }
  if (context->event_field_names != NULL) {
    for (i = 0; i < context->event_field_count; ++i) {
      free(context->event_field_names[i]);
    }
    free(context->event_field_names);
  }
  free(context);
}

static cpkt_opcua_result cpkt_set_variant(UA_Variant *variant, const cpkt_opcua_value *value) {
  UA_Boolean boolean_value;
  UA_Boolean *boolean_array_values;
  UA_Int32 integer_value;
  UA_Int32 *integer_array_values;
  UA_Double double_value;
  UA_Double *double_array_values;
  UA_String string_value;
  UA_String *string_array_values;
  UA_ByteString bytes_value;
  UA_ByteString *byte_string_array_values;
  UA_Guid guid_value;
  UA_Guid *guid_array_values;
  UA_QualifiedName qualified_name_value;
  UA_QualifiedName *qualified_name_array_values;
  UA_LocalizedText localized_text_value;
  UA_LocalizedText *localized_text_array_values;
  UA_StatusCode status;
  UA_StatusCode status_value;
  UA_StatusCode *status_array_values;
  UA_UInt64 uint64_value;
  UA_UInt64 *uint64_array_values;
  UA_DateTime datetime_value;
  UA_DateTime *datetime_array_values;
  size_t i;

  if (variant == NULL || value == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }

  switch (value->type) {
    case CPKT_OPCUA_VALUE_EMPTY:
      break;
    case CPKT_OPCUA_VALUE_BOOLEAN:
      boolean_value = value->boolean_value ? true : false;
      UA_Variant_setScalarCopy(variant, &boolean_value, &UA_TYPES[UA_TYPES_BOOLEAN]);
      break;
    case CPKT_OPCUA_VALUE_BOOLEAN_ARRAY:
      if (value->boolean_array_values == NULL && value->boolean_array_length != 0) {
        return CPKT_OPCUA_ERR_ARG;
      }
      boolean_array_values = NULL;
      if (value->boolean_array_length != 0) {
        boolean_array_values =
            (UA_Boolean *)calloc(value->boolean_array_length, sizeof(*boolean_array_values));
        if (boolean_array_values == NULL) {
          return CPKT_OPCUA_ERR_ALLOC;
        }
      }
      for (i = 0; i < value->boolean_array_length; ++i) {
        boolean_array_values[i] = value->boolean_array_values[i] ? true : false;
      }
      status = UA_Variant_setArrayCopy(
          variant,
          boolean_array_values,
          value->boolean_array_length,
          &UA_TYPES[UA_TYPES_BOOLEAN]);
      free(boolean_array_values);
      if (status != UA_STATUSCODE_GOOD) {
        return CPKT_OPCUA_ERR_ALLOC;
      }
      break;
    case CPKT_OPCUA_VALUE_INTEGER:
      if (value->integer_value < (long)INT_MIN || value->integer_value > (long)INT_MAX) {
        return CPKT_OPCUA_ERR_RANGE;
      }
      integer_value = (UA_Int32)value->integer_value;
      UA_Variant_setScalarCopy(variant, &integer_value, &UA_TYPES[UA_TYPES_INT32]);
      break;
    case CPKT_OPCUA_VALUE_UINT64:
      if (!cpkt_valid_uint64_words(value->uint64_value.high32, value->uint64_value.low32)) {
        return CPKT_OPCUA_ERR_RANGE;
      }
      uint64_value = cpkt_make_uint64(value->uint64_value);
      UA_Variant_setScalarCopy(variant, &uint64_value, &UA_TYPES[UA_TYPES_UINT64]);
      break;
    case CPKT_OPCUA_VALUE_UINT64_ARRAY:
      if (value->uint64_array_values == NULL && value->uint64_array_length != 0) {
        return CPKT_OPCUA_ERR_ARG;
      }
      uint64_array_values = NULL;
      if (value->uint64_array_length != 0) {
        uint64_array_values =
            (UA_UInt64 *)calloc(value->uint64_array_length, sizeof(*uint64_array_values));
        if (uint64_array_values == NULL) {
          return CPKT_OPCUA_ERR_ALLOC;
        }
      }
      for (i = 0; i < value->uint64_array_length; ++i) {
        if (!cpkt_valid_uint64_words(
                value->uint64_array_values[i].high32,
                value->uint64_array_values[i].low32)) {
          free(uint64_array_values);
          return CPKT_OPCUA_ERR_RANGE;
        }
        uint64_array_values[i] = cpkt_make_uint64(value->uint64_array_values[i]);
      }
      status = UA_Variant_setArrayCopy(
          variant,
          uint64_array_values,
          value->uint64_array_length,
          &UA_TYPES[UA_TYPES_UINT64]);
      free(uint64_array_values);
      if (status != UA_STATUSCODE_GOOD) {
        return CPKT_OPCUA_ERR_ALLOC;
      }
      break;
    case CPKT_OPCUA_VALUE_DATETIME:
      if (!cpkt_valid_datetime_words(value->datetime_value.high32, value->datetime_value.low32)) {
        return CPKT_OPCUA_ERR_RANGE;
      }
      datetime_value = cpkt_make_datetime(value->datetime_value);
      UA_Variant_setScalarCopy(variant, &datetime_value, &UA_TYPES[UA_TYPES_DATETIME]);
      break;
    case CPKT_OPCUA_VALUE_DATETIME_ARRAY:
      if (value->datetime_array_values == NULL && value->datetime_array_length != 0) {
        return CPKT_OPCUA_ERR_ARG;
      }
      datetime_array_values = NULL;
      if (value->datetime_array_length != 0) {
        datetime_array_values =
            (UA_DateTime *)calloc(value->datetime_array_length, sizeof(*datetime_array_values));
        if (datetime_array_values == NULL) {
          return CPKT_OPCUA_ERR_ALLOC;
        }
      }
      for (i = 0; i < value->datetime_array_length; ++i) {
        if (!cpkt_valid_datetime_words(
                value->datetime_array_values[i].high32,
                value->datetime_array_values[i].low32)) {
          free(datetime_array_values);
          return CPKT_OPCUA_ERR_RANGE;
        }
        datetime_array_values[i] = cpkt_make_datetime(value->datetime_array_values[i]);
      }
      status = UA_Variant_setArrayCopy(
          variant,
          datetime_array_values,
          value->datetime_array_length,
          &UA_TYPES[UA_TYPES_DATETIME]);
      free(datetime_array_values);
      if (status != UA_STATUSCODE_GOOD) {
        return CPKT_OPCUA_ERR_ALLOC;
      }
      break;
    case CPKT_OPCUA_VALUE_INTEGER_ARRAY:
      if (value->integer_array_values == NULL && value->integer_array_length != 0) {
        return CPKT_OPCUA_ERR_ARG;
      }
      integer_array_values = NULL;
      if (value->integer_array_length != 0) {
        integer_array_values =
            (UA_Int32 *)calloc(value->integer_array_length, sizeof(*integer_array_values));
        if (integer_array_values == NULL) {
          return CPKT_OPCUA_ERR_ALLOC;
        }
      }
      for (i = 0; i < value->integer_array_length; ++i) {
        if (value->integer_array_values[i] < (long)INT_MIN ||
            value->integer_array_values[i] > (long)INT_MAX) {
          free(integer_array_values);
          return CPKT_OPCUA_ERR_RANGE;
        }
        integer_array_values[i] = (UA_Int32)value->integer_array_values[i];
      }
      status = UA_Variant_setArrayCopy(
          variant,
          integer_array_values,
          value->integer_array_length,
          &UA_TYPES[UA_TYPES_INT32]);
      free(integer_array_values);
      if (status != UA_STATUSCODE_GOOD) {
        return CPKT_OPCUA_ERR_ALLOC;
      }
      break;
    case CPKT_OPCUA_VALUE_DOUBLE:
      double_value = (UA_Double)value->double_value;
      UA_Variant_setScalarCopy(variant, &double_value, &UA_TYPES[UA_TYPES_DOUBLE]);
      break;
    case CPKT_OPCUA_VALUE_DOUBLE_ARRAY:
      if (value->double_array_values == NULL && value->double_array_length != 0) {
        return CPKT_OPCUA_ERR_ARG;
      }
      double_array_values = NULL;
      if (value->double_array_length != 0) {
        double_array_values =
            (UA_Double *)calloc(value->double_array_length, sizeof(*double_array_values));
        if (double_array_values == NULL) {
          return CPKT_OPCUA_ERR_ALLOC;
        }
      }
      for (i = 0; i < value->double_array_length; ++i) {
        double_array_values[i] = (UA_Double)value->double_array_values[i];
      }
      status = UA_Variant_setArrayCopy(
          variant,
          double_array_values,
          value->double_array_length,
          &UA_TYPES[UA_TYPES_DOUBLE]);
      free(double_array_values);
      if (status != UA_STATUSCODE_GOOD) {
        return CPKT_OPCUA_ERR_ALLOC;
      }
      break;
    case CPKT_OPCUA_VALUE_STRING:
      if (value->string_value == NULL && value->string_length != 0) {
        return CPKT_OPCUA_ERR_ARG;
      }
      string_value = UA_STRING_NULL;
      string_value.data = (UA_Byte *)value->string_value;
      string_value.length = value->string_length;
      UA_Variant_setScalarCopy(variant, &string_value, &UA_TYPES[UA_TYPES_STRING]);
      break;
    case CPKT_OPCUA_VALUE_STRING_ARRAY:
      if (value->string_array_values == NULL && value->string_array_length != 0) {
        return CPKT_OPCUA_ERR_ARG;
      }
      string_array_values = NULL;
      if (value->string_array_length != 0) {
        string_array_values =
            (UA_String *)calloc(value->string_array_length, sizeof(*string_array_values));
        if (string_array_values == NULL) {
          return CPKT_OPCUA_ERR_ALLOC;
        }
      }
      for (i = 0; i < value->string_array_length; ++i) {
        if (value->string_array_values[i].data == NULL && value->string_array_values[i].length != 0) {
          free(string_array_values);
          return CPKT_OPCUA_ERR_ARG;
        }
        string_array_values[i] = UA_STRING_NULL;
        string_array_values[i].data = (UA_Byte *)value->string_array_values[i].data;
        string_array_values[i].length = value->string_array_values[i].length;
      }
      status = UA_Variant_setArrayCopy(
          variant,
          string_array_values,
          value->string_array_length,
          &UA_TYPES[UA_TYPES_STRING]);
      free(string_array_values);
      if (status != UA_STATUSCODE_GOOD) {
        return CPKT_OPCUA_ERR_ALLOC;
      }
      break;
    case CPKT_OPCUA_VALUE_BYTE_STRING:
      if (value->bytes_value == NULL && value->bytes_length != 0) {
        return CPKT_OPCUA_ERR_ARG;
      }
      bytes_value = UA_BYTESTRING_NULL;
      bytes_value.data = (UA_Byte *)value->bytes_value;
      bytes_value.length = value->bytes_length;
      UA_Variant_setScalarCopy(variant, &bytes_value, &UA_TYPES[UA_TYPES_BYTESTRING]);
      break;
    case CPKT_OPCUA_VALUE_GUID:
      guid_value = cpkt_make_guid(value->guid_value);
      UA_Variant_setScalarCopy(variant, &guid_value, &UA_TYPES[UA_TYPES_GUID]);
      break;
    case CPKT_OPCUA_VALUE_GUID_ARRAY:
      if (value->guid_array_values == NULL && value->guid_array_length != 0) {
        return CPKT_OPCUA_ERR_ARG;
      }
      guid_array_values = NULL;
      if (value->guid_array_length != 0) {
        guid_array_values = (UA_Guid *)calloc(value->guid_array_length, sizeof(*guid_array_values));
        if (guid_array_values == NULL) {
          return CPKT_OPCUA_ERR_ALLOC;
        }
      }
      for (i = 0; i < value->guid_array_length; ++i) {
        guid_array_values[i] = cpkt_make_guid(value->guid_array_values[i].bytes);
      }
      status = UA_Variant_setArrayCopy(
          variant,
          guid_array_values,
          value->guid_array_length,
          &UA_TYPES[UA_TYPES_GUID]);
      free(guid_array_values);
      if (status != UA_STATUSCODE_GOOD) {
        return CPKT_OPCUA_ERR_ALLOC;
      }
      break;
    case CPKT_OPCUA_VALUE_STATUS:
      if (value->status_value > (unsigned long)UINT_MAX) {
        return CPKT_OPCUA_ERR_RANGE;
      }
      status_value = (UA_StatusCode)value->status_value;
      UA_Variant_setScalarCopy(variant, &status_value, &UA_TYPES[UA_TYPES_STATUSCODE]);
      break;
    case CPKT_OPCUA_VALUE_STATUS_ARRAY:
      if (value->status_array_values == NULL && value->status_array_length != 0) {
        return CPKT_OPCUA_ERR_ARG;
      }
      status_array_values = NULL;
      if (value->status_array_length != 0) {
        status_array_values =
            (UA_StatusCode *)calloc(value->status_array_length, sizeof(*status_array_values));
        if (status_array_values == NULL) {
          return CPKT_OPCUA_ERR_ALLOC;
        }
      }
      for (i = 0; i < value->status_array_length; ++i) {
        if (value->status_array_values[i] > (unsigned long)UINT_MAX) {
          free(status_array_values);
          return CPKT_OPCUA_ERR_RANGE;
        }
        status_array_values[i] = (UA_StatusCode)value->status_array_values[i];
      }
      status = UA_Variant_setArrayCopy(
          variant,
          status_array_values,
          value->status_array_length,
          &UA_TYPES[UA_TYPES_STATUSCODE]);
      free(status_array_values);
      if (status != UA_STATUSCODE_GOOD) {
        return CPKT_OPCUA_ERR_ALLOC;
      }
      break;
    case CPKT_OPCUA_VALUE_QUALIFIED_NAME:
      if (value->qualified_name == NULL && value->qualified_name_length != 0) {
        return CPKT_OPCUA_ERR_ARG;
      }
      qualified_name_value.namespaceIndex = (UA_UInt16)value->qualified_name_namespace_index;
      qualified_name_value.name = UA_STRING_NULL;
      qualified_name_value.name.data = (UA_Byte *)value->qualified_name;
      qualified_name_value.name.length = value->qualified_name_length;
      UA_Variant_setScalarCopy(variant, &qualified_name_value, &UA_TYPES[UA_TYPES_QUALIFIEDNAME]);
      break;
    case CPKT_OPCUA_VALUE_QUALIFIED_NAME_ARRAY:
      if (value->qualified_name_array_values == NULL && value->qualified_name_array_length != 0) {
        return CPKT_OPCUA_ERR_ARG;
      }
      qualified_name_array_values = NULL;
      if (value->qualified_name_array_length != 0) {
        qualified_name_array_values = (UA_QualifiedName *)calloc(
            value->qualified_name_array_length,
            sizeof(*qualified_name_array_values));
        if (qualified_name_array_values == NULL) {
          return CPKT_OPCUA_ERR_ALLOC;
        }
      }
      for (i = 0; i < value->qualified_name_array_length; ++i) {
        if (value->qualified_name_array_values[i].name == NULL &&
            value->qualified_name_array_values[i].name_length != 0) {
          free(qualified_name_array_values);
          return CPKT_OPCUA_ERR_ARG;
        }
        qualified_name_array_values[i].namespaceIndex =
            (UA_UInt16)value->qualified_name_array_values[i].namespace_index;
        qualified_name_array_values[i].name = UA_STRING_NULL;
        qualified_name_array_values[i].name.data = (UA_Byte *)value->qualified_name_array_values[i].name;
        qualified_name_array_values[i].name.length = value->qualified_name_array_values[i].name_length;
      }
      status = UA_Variant_setArrayCopy(
          variant,
          qualified_name_array_values,
          value->qualified_name_array_length,
          &UA_TYPES[UA_TYPES_QUALIFIEDNAME]);
      free(qualified_name_array_values);
      if (status != UA_STATUSCODE_GOOD) {
        return CPKT_OPCUA_ERR_ALLOC;
      }
      break;
    case CPKT_OPCUA_VALUE_LOCALIZED_TEXT:
      if ((value->localized_text_locale == NULL && value->localized_text_locale_length != 0) ||
          (value->localized_text == NULL && value->localized_text_length != 0)) {
        return CPKT_OPCUA_ERR_ARG;
      }
      localized_text_value.locale = UA_STRING_NULL;
      localized_text_value.locale.data = (UA_Byte *)value->localized_text_locale;
      localized_text_value.locale.length = value->localized_text_locale_length;
      localized_text_value.text = UA_STRING_NULL;
      localized_text_value.text.data = (UA_Byte *)value->localized_text;
      localized_text_value.text.length = value->localized_text_length;
      UA_Variant_setScalarCopy(variant, &localized_text_value, &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]);
      break;
    case CPKT_OPCUA_VALUE_LOCALIZED_TEXT_ARRAY:
      if (value->localized_text_array_values == NULL && value->localized_text_array_length != 0) {
        return CPKT_OPCUA_ERR_ARG;
      }
      localized_text_array_values = NULL;
      if (value->localized_text_array_length != 0) {
        localized_text_array_values = (UA_LocalizedText *)calloc(
            value->localized_text_array_length,
            sizeof(*localized_text_array_values));
        if (localized_text_array_values == NULL) {
          return CPKT_OPCUA_ERR_ALLOC;
        }
      }
      for (i = 0; i < value->localized_text_array_length; ++i) {
        if ((value->localized_text_array_values[i].locale == NULL &&
             value->localized_text_array_values[i].locale_length != 0) ||
            (value->localized_text_array_values[i].text == NULL &&
             value->localized_text_array_values[i].text_length != 0)) {
          free(localized_text_array_values);
          return CPKT_OPCUA_ERR_ARG;
        }
        localized_text_array_values[i].locale = UA_STRING_NULL;
        localized_text_array_values[i].locale.data =
            (UA_Byte *)value->localized_text_array_values[i].locale;
        localized_text_array_values[i].locale.length =
            value->localized_text_array_values[i].locale_length;
        localized_text_array_values[i].text = UA_STRING_NULL;
        localized_text_array_values[i].text.data =
            (UA_Byte *)value->localized_text_array_values[i].text;
        localized_text_array_values[i].text.length =
            value->localized_text_array_values[i].text_length;
      }
      status = UA_Variant_setArrayCopy(
          variant,
          localized_text_array_values,
          value->localized_text_array_length,
          &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]);
      free(localized_text_array_values);
      if (status != UA_STATUSCODE_GOOD) {
        return CPKT_OPCUA_ERR_ALLOC;
      }
      break;
    case CPKT_OPCUA_VALUE_BYTE_STRING_ARRAY:
      if (value->byte_string_array_values == NULL && value->byte_string_array_length != 0) {
        return CPKT_OPCUA_ERR_ARG;
      }
      byte_string_array_values = NULL;
      if (value->byte_string_array_length != 0) {
        byte_string_array_values =
            (UA_ByteString *)calloc(value->byte_string_array_length, sizeof(*byte_string_array_values));
        if (byte_string_array_values == NULL) {
          return CPKT_OPCUA_ERR_ALLOC;
        }
      }
      for (i = 0; i < value->byte_string_array_length; ++i) {
        if (value->byte_string_array_values[i].data == NULL &&
            value->byte_string_array_values[i].length != 0) {
          free(byte_string_array_values);
          return CPKT_OPCUA_ERR_ARG;
        }
        byte_string_array_values[i] = UA_BYTESTRING_NULL;
        byte_string_array_values[i].data = (UA_Byte *)value->byte_string_array_values[i].data;
        byte_string_array_values[i].length = value->byte_string_array_values[i].length;
      }
      status = UA_Variant_setArrayCopy(
          variant,
          byte_string_array_values,
          value->byte_string_array_length,
          &UA_TYPES[UA_TYPES_BYTESTRING]);
      free(byte_string_array_values);
      if (status != UA_STATUSCODE_GOOD) {
        return CPKT_OPCUA_ERR_ALLOC;
      }
      break;
    default:
      return CPKT_OPCUA_ERR_TYPE;
  }

  if (variant->data == NULL && value->type != CPKT_OPCUA_VALUE_EMPTY) {
    if ((value->type == CPKT_OPCUA_VALUE_BOOLEAN_ARRAY && value->boolean_array_length == 0) ||
        (value->type == CPKT_OPCUA_VALUE_INTEGER_ARRAY && value->integer_array_length == 0) ||
        (value->type == CPKT_OPCUA_VALUE_UINT64_ARRAY && value->uint64_array_length == 0) ||
        (value->type == CPKT_OPCUA_VALUE_DATETIME_ARRAY && value->datetime_array_length == 0) ||
        (value->type == CPKT_OPCUA_VALUE_STATUS_ARRAY && value->status_array_length == 0) ||
        (value->type == CPKT_OPCUA_VALUE_GUID_ARRAY && value->guid_array_length == 0) ||
        (value->type == CPKT_OPCUA_VALUE_QUALIFIED_NAME_ARRAY &&
         value->qualified_name_array_length == 0) ||
        (value->type == CPKT_OPCUA_VALUE_LOCALIZED_TEXT_ARRAY &&
         value->localized_text_array_length == 0) ||
        (value->type == CPKT_OPCUA_VALUE_DOUBLE_ARRAY && value->double_array_length == 0) ||
        (value->type == CPKT_OPCUA_VALUE_STRING_ARRAY && value->string_array_length == 0) ||
        (value->type == CPKT_OPCUA_VALUE_BYTE_STRING_ARRAY && value->byte_string_array_length == 0)) {
      return CPKT_OPCUA_OK;
    }
    return CPKT_OPCUA_ERR_ALLOC;
  }
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_set_variant_array(
    UA_Variant *variants,
    const cpkt_opcua_value *values,
    size_t value_count) {
  size_t i;
  cpkt_opcua_result result;

  if (value_count != 0 && (variants == NULL || values == NULL)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  for (i = 0; i < value_count; ++i) {
    UA_Variant_init(&variants[i]);
    result = cpkt_set_variant(&variants[i], &values[i]);
    if (result != CPKT_OPCUA_OK) {
      UA_Variant_clear(&variants[i]);
      while (i > 0) {
        --i;
        UA_Variant_clear(&variants[i]);
      }
      return result;
    }
  }
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_server_write_boolean_attribute(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    UA_UInt32 attribute_id,
    int value,
    cpkt_opcua_status *status_out) {
  UA_WriteValue write_value;
  UA_Boolean native_value;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_value = value ? true : false;
  UA_WriteValue_init(&write_value);
  write_value.nodeId = cpkt_make_node_id(node_id);
  write_value.attributeId = attribute_id;
  write_value.value.hasValue = true;
  UA_Variant_setScalar(&write_value.value.value, &native_value, &UA_TYPES[UA_TYPES_BOOLEAN]);
  status = UA_Server_write(server->server, &write_value);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

static cpkt_opcua_result cpkt_server_read_scalar_attribute(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    UA_UInt32 attribute_id,
    const UA_DataType *expected_type,
    UA_Variant *variant_out,
    cpkt_opcua_status *status_out) {
  UA_ReadValueId read_value;
  UA_DataValue data_value;
  UA_StatusCode copy_status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (variant_out != NULL) {
    UA_Variant_init(variant_out);
  }
  if (server == NULL || server->server == NULL || expected_type == NULL || variant_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  UA_ReadValueId_init(&read_value);
  read_value.nodeId = cpkt_make_node_id(node_id);
  read_value.attributeId = attribute_id;
  data_value = UA_Server_read(server->server, &read_value, UA_TIMESTAMPSTORETURN_NEITHER);
  if (status_out != NULL) {
    *status_out = cpkt_status(data_value.status);
  }
  if (data_value.status != UA_STATUSCODE_GOOD) {
    UA_DataValue_clear(&data_value);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (!data_value.hasValue || !UA_Variant_hasScalarType(&data_value.value, expected_type)) {
    UA_DataValue_clear(&data_value);
    return CPKT_OPCUA_ERR_TYPE;
  }
  copy_status = UA_Variant_copy(&data_value.value, variant_out);
  UA_DataValue_clear(&data_value);
  return copy_status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_ALLOC;
}

static cpkt_opcua_result cpkt_server_read_uint32_attribute(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    UA_UInt32 attribute_id,
    unsigned long *value_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  UA_UInt32 *native_value;
  cpkt_opcua_result result;

  if (value_out != NULL) {
    *value_out = 0;
  }
  if (value_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_server_read_scalar_attribute(
      server,
      node_id,
      attribute_id,
      &UA_TYPES[UA_TYPES_UINT32],
      &variant,
      status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  native_value = (UA_UInt32 *)variant.data;
  *value_out = (unsigned long)*native_value;
  UA_Variant_clear(&variant);
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_server_read_byte_attribute(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    UA_UInt32 attribute_id,
    unsigned long *value_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  UA_Byte *native_value;
  cpkt_opcua_result result;

  if (value_out != NULL) {
    *value_out = 0;
  }
  if (value_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_server_read_scalar_attribute(
      server,
      node_id,
      attribute_id,
      &UA_TYPES[UA_TYPES_BYTE],
      &variant,
      status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  native_value = (UA_Byte *)variant.data;
  *value_out = (unsigned long)*native_value;
  UA_Variant_clear(&variant);
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_server_read_boolean_attribute(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    UA_UInt32 attribute_id,
    int *value_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  UA_Boolean *native_value;
  cpkt_opcua_result result;

  if (value_out != NULL) {
    *value_out = 0;
  }
  if (value_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_server_read_scalar_attribute(
      server,
      node_id,
      attribute_id,
      &UA_TYPES[UA_TYPES_BOOLEAN],
      &variant,
      status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  native_value = (UA_Boolean *)variant.data;
  *value_out = *native_value ? 1 : 0;
  UA_Variant_clear(&variant);
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_get_variant(
    const UA_Variant *variant,
    cpkt_opcua_value *value_out,
    char *string_buffer,
    size_t string_buffer_size,
    size_t *required_string_size_out) {
  UA_String *string_value;
  UA_ByteString *bytes_value;
  UA_Guid *guid_value;
  UA_StatusCode *status_value;
  UA_UInt16 *uint16_value;
  UA_UInt32 *uint32_value;
  UA_Int16 *int16_value;
  UA_UInt64 *uint64_value;
  UA_DateTime *datetime_value;
  UA_QualifiedName *qualified_name_value;
  UA_LocalizedText *localized_text_value;
  size_t required;

  if (variant == NULL || value_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }

  cpkt_opcua_value_clear(value_out);
  if (required_string_size_out != NULL) {
    *required_string_size_out = 0;
  }

  if (UA_Variant_isEmpty(variant)) {
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_BOOLEAN])) {
    value_out->type = CPKT_OPCUA_VALUE_BOOLEAN;
    value_out->boolean_value = (*(UA_Boolean *)variant->data) ? 1 : 0;
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_INT32])) {
    value_out->type = CPKT_OPCUA_VALUE_INTEGER;
    value_out->integer_value = (long)(*(UA_Int32 *)variant->data);
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_INT16])) {
    int16_value = (UA_Int16 *)variant->data;
    value_out->type = CPKT_OPCUA_VALUE_INTEGER;
    value_out->integer_value = (long)*int16_value;
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_UINT16])) {
    uint16_value = (UA_UInt16 *)variant->data;
    value_out->type = CPKT_OPCUA_VALUE_INTEGER;
    value_out->integer_value = (long)*uint16_value;
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_UINT32])) {
    uint32_value = (UA_UInt32 *)variant->data;
#if LONG_MAX < 4294967295UL
    if ((unsigned long)*uint32_value > (unsigned long)LONG_MAX) {
      return CPKT_OPCUA_ERR_RANGE;
    }
#endif
    value_out->type = CPKT_OPCUA_VALUE_INTEGER;
    value_out->integer_value = (long)*uint32_value;
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_UINT64])) {
    uint64_value = (UA_UInt64 *)variant->data;
    value_out->type = CPKT_OPCUA_VALUE_UINT64;
    value_out->uint64_value = cpkt_uint64_from_native(*uint64_value);
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_DATETIME])) {
    datetime_value = (UA_DateTime *)variant->data;
    value_out->type = CPKT_OPCUA_VALUE_DATETIME;
    value_out->datetime_value = cpkt_datetime_from_native(*datetime_value);
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_DOUBLE])) {
    value_out->type = CPKT_OPCUA_VALUE_DOUBLE;
    value_out->double_value = (double)(*(UA_Double *)variant->data);
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_STRING])) {
    string_value = (UA_String *)variant->data;
    value_out->type = CPKT_OPCUA_VALUE_STRING;
    value_out->string_length = string_value->length;
    if (required_string_size_out != NULL) {
      *required_string_size_out = string_value->length + 1;
    }
    if (string_buffer == NULL || string_buffer_size == 0) {
      return string_value->length == 0 ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_RANGE;
    }
    if (string_buffer_size <= string_value->length) {
      string_buffer[0] = '\0';
      return CPKT_OPCUA_ERR_RANGE;
    }
    if (string_value->length != 0) {
      memcpy(string_buffer, string_value->data, string_value->length);
    }
    string_buffer[string_value->length] = '\0';
    value_out->string_value = string_buffer;
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_BYTESTRING])) {
    bytes_value = (UA_ByteString *)variant->data;
    value_out->type = CPKT_OPCUA_VALUE_BYTE_STRING;
    value_out->bytes_length = bytes_value->length;
    if (required_string_size_out != NULL) {
      *required_string_size_out = bytes_value->length;
    }
    if (bytes_value->length == 0) {
      value_out->bytes_value = (const unsigned char *)string_buffer;
      return CPKT_OPCUA_OK;
    }
    if (string_buffer == NULL || string_buffer_size < bytes_value->length) {
      if (string_buffer != NULL && string_buffer_size != 0) {
        string_buffer[0] = '\0';
      }
      return CPKT_OPCUA_ERR_RANGE;
    }
    memcpy(string_buffer, bytes_value->data, bytes_value->length);
    value_out->bytes_value = (const unsigned char *)string_buffer;
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_GUID])) {
    guid_value = (UA_Guid *)variant->data;
    value_out->type = CPKT_OPCUA_VALUE_GUID;
    cpkt_guid_from_native(*guid_value, value_out->guid_value);
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_STATUSCODE])) {
    status_value = (UA_StatusCode *)variant->data;
    value_out->type = CPKT_OPCUA_VALUE_STATUS;
    value_out->status_value = (cpkt_opcua_status)*status_value;
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_QUALIFIEDNAME])) {
    qualified_name_value = (UA_QualifiedName *)variant->data;
    value_out->type = CPKT_OPCUA_VALUE_QUALIFIED_NAME;
    value_out->qualified_name_namespace_index = (unsigned short)qualified_name_value->namespaceIndex;
    value_out->qualified_name_length = qualified_name_value->name.length;
    required = qualified_name_value->name.length + 1;
    if (required_string_size_out != NULL) {
      *required_string_size_out = required;
    }
    if (string_buffer == NULL || string_buffer_size < required) {
      if (string_buffer != NULL && string_buffer_size != 0) {
        string_buffer[0] = '\0';
      }
      return CPKT_OPCUA_ERR_RANGE;
    }
    if (qualified_name_value->name.length != 0) {
      memcpy(string_buffer, qualified_name_value->name.data, qualified_name_value->name.length);
    }
    string_buffer[qualified_name_value->name.length] = '\0';
    value_out->qualified_name = string_buffer;
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_LOCALIZEDTEXT])) {
    localized_text_value = (UA_LocalizedText *)variant->data;
    value_out->type = CPKT_OPCUA_VALUE_LOCALIZED_TEXT;
    value_out->localized_text_locale_length = localized_text_value->locale.length;
    value_out->localized_text_length = localized_text_value->text.length;
    required = localized_text_value->locale.length + 1 + localized_text_value->text.length + 1;
    if (required_string_size_out != NULL) {
      *required_string_size_out = required;
    }
    if (string_buffer == NULL || string_buffer_size < required) {
      if (string_buffer != NULL && string_buffer_size != 0) {
        string_buffer[0] = '\0';
      }
      return CPKT_OPCUA_ERR_RANGE;
    }
    if (localized_text_value->locale.length != 0) {
      memcpy(string_buffer, localized_text_value->locale.data, localized_text_value->locale.length);
    }
    string_buffer[localized_text_value->locale.length] = '\0';
    value_out->localized_text_locale = string_buffer;
    string_buffer += localized_text_value->locale.length + 1;
    if (localized_text_value->text.length != 0) {
      memcpy(string_buffer, localized_text_value->text.data, localized_text_value->text.length);
    }
    string_buffer[localized_text_value->text.length] = '\0';
    value_out->localized_text = string_buffer;
    return CPKT_OPCUA_OK;
  }

  return CPKT_OPCUA_ERR_TYPE;
}

static cpkt_opcua_result cpkt_get_variant_borrowed(
    const UA_Variant *variant,
    cpkt_opcua_value *value_out) {
  UA_String *string_value;
  UA_ByteString *bytes_value;
  UA_Guid *guid_value;
  UA_StatusCode *status_value;
  UA_UInt16 *uint16_value;
  UA_UInt32 *uint32_value;
  UA_Int16 *int16_value;
  UA_UInt64 *uint64_value;
  UA_DateTime *datetime_value;
  UA_QualifiedName *qualified_name_value;
  UA_LocalizedText *localized_text_value;

  if (variant == NULL || value_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  cpkt_opcua_value_clear(value_out);
  if (UA_Variant_isEmpty(variant)) {
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_BOOLEAN])) {
    value_out->type = CPKT_OPCUA_VALUE_BOOLEAN;
    value_out->boolean_value = (*(UA_Boolean *)variant->data) ? 1 : 0;
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_INT32])) {
    value_out->type = CPKT_OPCUA_VALUE_INTEGER;
    value_out->integer_value = (long)(*(UA_Int32 *)variant->data);
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_INT16])) {
    int16_value = (UA_Int16 *)variant->data;
    value_out->type = CPKT_OPCUA_VALUE_INTEGER;
    value_out->integer_value = (long)*int16_value;
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_UINT16])) {
    uint16_value = (UA_UInt16 *)variant->data;
    value_out->type = CPKT_OPCUA_VALUE_INTEGER;
    value_out->integer_value = (long)*uint16_value;
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_UINT32])) {
    uint32_value = (UA_UInt32 *)variant->data;
#if LONG_MAX < 4294967295UL
    if ((unsigned long)*uint32_value > (unsigned long)LONG_MAX) {
      return CPKT_OPCUA_ERR_RANGE;
    }
#endif
    value_out->type = CPKT_OPCUA_VALUE_INTEGER;
    value_out->integer_value = (long)*uint32_value;
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_UINT64])) {
    uint64_value = (UA_UInt64 *)variant->data;
    value_out->type = CPKT_OPCUA_VALUE_UINT64;
    value_out->uint64_value = cpkt_uint64_from_native(*uint64_value);
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_DATETIME])) {
    datetime_value = (UA_DateTime *)variant->data;
    value_out->type = CPKT_OPCUA_VALUE_DATETIME;
    value_out->datetime_value = cpkt_datetime_from_native(*datetime_value);
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_DOUBLE])) {
    value_out->type = CPKT_OPCUA_VALUE_DOUBLE;
    value_out->double_value = (double)(*(UA_Double *)variant->data);
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_STRING])) {
    string_value = (UA_String *)variant->data;
    value_out->type = CPKT_OPCUA_VALUE_STRING;
    value_out->string_value = (const char *)string_value->data;
    value_out->string_length = string_value->length;
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_BYTESTRING])) {
    bytes_value = (UA_ByteString *)variant->data;
    value_out->type = CPKT_OPCUA_VALUE_BYTE_STRING;
    value_out->bytes_value = (const unsigned char *)bytes_value->data;
    value_out->bytes_length = bytes_value->length;
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_GUID])) {
    guid_value = (UA_Guid *)variant->data;
    value_out->type = CPKT_OPCUA_VALUE_GUID;
    cpkt_guid_from_native(*guid_value, value_out->guid_value);
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_STATUSCODE])) {
    status_value = (UA_StatusCode *)variant->data;
    value_out->type = CPKT_OPCUA_VALUE_STATUS;
    value_out->status_value = cpkt_status(*status_value);
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_QUALIFIEDNAME])) {
    qualified_name_value = (UA_QualifiedName *)variant->data;
    value_out->type = CPKT_OPCUA_VALUE_QUALIFIED_NAME;
    value_out->qualified_name_namespace_index = qualified_name_value->namespaceIndex;
    value_out->qualified_name = (const char *)qualified_name_value->name.data;
    value_out->qualified_name_length = qualified_name_value->name.length;
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_LOCALIZEDTEXT])) {
    localized_text_value = (UA_LocalizedText *)variant->data;
    value_out->type = CPKT_OPCUA_VALUE_LOCALIZED_TEXT;
    value_out->localized_text_locale = (const char *)localized_text_value->locale.data;
    value_out->localized_text_locale_length = localized_text_value->locale.length;
    value_out->localized_text = (const char *)localized_text_value->text.data;
    value_out->localized_text_length = localized_text_value->text.length;
    return CPKT_OPCUA_OK;
  }
  return CPKT_OPCUA_ERR_TYPE;
}

static cpkt_opcua_result cpkt_get_data_value(
    const UA_DataValue *data_value,
    cpkt_opcua_data_value *data_value_out,
    char *string_buffer,
    size_t string_buffer_size,
    size_t *required_string_size_out) {
  cpkt_opcua_result result;

  if (required_string_size_out != NULL) {
    *required_string_size_out = 0;
  }
  if (data_value_out != NULL) {
    cpkt_opcua_data_value_clear(data_value_out);
  }
  if (data_value == NULL || data_value_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  data_value_out->status = cpkt_status(data_value->status);
  data_value_out->has_status = data_value->hasStatus ? 1 : 0;
  if (data_value->hasSourceTimestamp) {
    data_value_out->has_source_timestamp = 1;
    data_value_out->source_timestamp = cpkt_datetime_from_native(data_value->sourceTimestamp);
  }
  if (data_value->hasServerTimestamp) {
    data_value_out->has_server_timestamp = 1;
    data_value_out->server_timestamp = cpkt_datetime_from_native(data_value->serverTimestamp);
  }
  if (!data_value->hasValue) {
    return CPKT_OPCUA_OK;
  }
  result = cpkt_get_variant(
      &data_value->value,
      &data_value_out->value,
      string_buffer,
      string_buffer_size,
      required_string_size_out);
  if (result != CPKT_OPCUA_OK) {
    cpkt_opcua_data_value_clear(data_value_out);
    data_value_out->status = cpkt_status(data_value->status);
    data_value_out->has_status = data_value->hasStatus ? 1 : 0;
    return result;
  }
  data_value_out->has_value = 1;
  return CPKT_OPCUA_OK;
}

static UA_Boolean cpkt_history_read_raw_callback(
    UA_Client *client,
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
        &history_data->dataValues[i],
        &data_value,
        context->string_buffer,
        context->string_buffer_size,
        context->required_string_size_out);
    if (context->result != CPKT_OPCUA_OK) {
      return false;
    }
    if (context->fn(&data_value, more_data_available ? 1 : 0, context->user) != 0) {
      cpkt_opcua_data_value_clear(&data_value);
      context->result = CPKT_OPCUA_ERR_CALLBACK;
      return false;
    }
    cpkt_opcua_data_value_clear(&data_value);
  }
  return true;
}

static cpkt_opcua_result cpkt_copy_boolean_array_from_variant(
    const UA_Variant *variant,
    int *values,
    size_t value_count,
    size_t *required_value_count_out) {
  UA_Boolean *native_values;
  size_t i;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (variant == NULL || required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!UA_Variant_hasArrayType(variant, &UA_TYPES[UA_TYPES_BOOLEAN])) {
    return CPKT_OPCUA_ERR_TYPE;
  }
  *required_value_count_out = variant->arrayLength;
  if (variant->arrayLength == 0) {
    return CPKT_OPCUA_OK;
  }
  if (values == NULL || value_count < variant->arrayLength) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  native_values = (UA_Boolean *)variant->data;
  for (i = 0; i < variant->arrayLength; ++i) {
    values[i] = native_values[i] ? 1 : 0;
  }
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_copy_integer_array_from_variant(
    const UA_Variant *variant,
    long *values,
    size_t value_count,
    size_t *required_value_count_out) {
  UA_Int32 *native_values;
  size_t i;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (variant == NULL || required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!UA_Variant_hasArrayType(variant, &UA_TYPES[UA_TYPES_INT32])) {
    return CPKT_OPCUA_ERR_TYPE;
  }
  *required_value_count_out = variant->arrayLength;
  if (variant->arrayLength == 0) {
    return CPKT_OPCUA_OK;
  }
  if (values == NULL || value_count < variant->arrayLength) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  native_values = (UA_Int32 *)variant->data;
  for (i = 0; i < variant->arrayLength; ++i) {
    values[i] = (long)native_values[i];
  }
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_copy_uint64_array_from_variant(
    const UA_Variant *variant,
    cpkt_opcua_uint64 *values,
    size_t value_count,
    size_t *required_value_count_out) {
  UA_UInt64 *native_values;
  size_t i;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (variant == NULL || required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!UA_Variant_hasArrayType(variant, &UA_TYPES[UA_TYPES_UINT64])) {
    return CPKT_OPCUA_ERR_TYPE;
  }
  *required_value_count_out = variant->arrayLength;
  if (variant->arrayLength == 0) {
    return CPKT_OPCUA_OK;
  }
  if (values == NULL || value_count < variant->arrayLength) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  native_values = (UA_UInt64 *)variant->data;
  for (i = 0; i < variant->arrayLength; ++i) {
    values[i] = cpkt_uint64_from_native(native_values[i]);
  }
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_copy_datetime_array_from_variant(
    const UA_Variant *variant,
    cpkt_opcua_datetime *values,
    size_t value_count,
    size_t *required_value_count_out) {
  UA_DateTime *native_values;
  size_t i;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (variant == NULL || required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!UA_Variant_hasArrayType(variant, &UA_TYPES[UA_TYPES_DATETIME])) {
    return CPKT_OPCUA_ERR_TYPE;
  }
  *required_value_count_out = variant->arrayLength;
  if (variant->arrayLength == 0) {
    return CPKT_OPCUA_OK;
  }
  if (values == NULL || value_count < variant->arrayLength) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  native_values = (UA_DateTime *)variant->data;
  for (i = 0; i < variant->arrayLength; ++i) {
    values[i] = cpkt_datetime_from_native(native_values[i]);
  }
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_copy_status_array_from_variant(
    const UA_Variant *variant,
    cpkt_opcua_status *values,
    size_t value_count,
    size_t *required_value_count_out) {
  UA_StatusCode *native_values;
  size_t i;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (variant == NULL || required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!UA_Variant_hasArrayType(variant, &UA_TYPES[UA_TYPES_STATUSCODE])) {
    return CPKT_OPCUA_ERR_TYPE;
  }
  *required_value_count_out = variant->arrayLength;
  if (variant->arrayLength == 0) {
    return CPKT_OPCUA_OK;
  }
  if (values == NULL || value_count < variant->arrayLength) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  native_values = (UA_StatusCode *)variant->data;
  for (i = 0; i < variant->arrayLength; ++i) {
    values[i] = cpkt_status(native_values[i]);
  }
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_copy_guid_array_from_variant(
    const UA_Variant *variant,
    cpkt_opcua_guid *values,
    size_t value_count,
    size_t *required_value_count_out) {
  UA_Guid *native_values;
  size_t i;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (variant == NULL || required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!UA_Variant_hasArrayType(variant, &UA_TYPES[UA_TYPES_GUID])) {
    return CPKT_OPCUA_ERR_TYPE;
  }
  *required_value_count_out = variant->arrayLength;
  if (variant->arrayLength == 0) {
    return CPKT_OPCUA_OK;
  }
  if (values == NULL || value_count < variant->arrayLength) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  native_values = (UA_Guid *)variant->data;
  for (i = 0; i < variant->arrayLength; ++i) {
    cpkt_guid_from_native(native_values[i], values[i].bytes);
  }
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_copy_double_array_from_variant(
    const UA_Variant *variant,
    double *values,
    size_t value_count,
    size_t *required_value_count_out) {
  UA_Double *native_values;
  size_t i;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (variant == NULL || required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!UA_Variant_hasArrayType(variant, &UA_TYPES[UA_TYPES_DOUBLE])) {
    return CPKT_OPCUA_ERR_TYPE;
  }
  *required_value_count_out = variant->arrayLength;
  if (variant->arrayLength == 0) {
    return CPKT_OPCUA_OK;
  }
  if (values == NULL || value_count < variant->arrayLength) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  native_values = (UA_Double *)variant->data;
  for (i = 0; i < variant->arrayLength; ++i) {
    values[i] = (double)native_values[i];
  }
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_each_string_array_from_variant(
    const UA_Variant *variant,
    cpkt_opcua_string_array_fn fn,
    void *user,
    size_t *value_count_out) {
  UA_String *native_values;
  size_t i;

  if (value_count_out != NULL) {
    *value_count_out = 0;
  }
  if (variant == NULL || fn == NULL || value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!UA_Variant_hasArrayType(variant, &UA_TYPES[UA_TYPES_STRING])) {
    return CPKT_OPCUA_ERR_TYPE;
  }
  *value_count_out = variant->arrayLength;
  native_values = (UA_String *)variant->data;
  for (i = 0; i < variant->arrayLength; ++i) {
    if (fn(i, (const char *)native_values[i].data, native_values[i].length, user) != 0) {
      return CPKT_OPCUA_ERR_CALLBACK;
    }
  }
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_each_byte_string_array_from_variant(
    const UA_Variant *variant,
    cpkt_opcua_byte_string_array_fn fn,
    void *user,
    size_t *value_count_out) {
  UA_ByteString *native_values;
  size_t i;

  if (value_count_out != NULL) {
    *value_count_out = 0;
  }
  if (variant == NULL || fn == NULL || value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!UA_Variant_hasArrayType(variant, &UA_TYPES[UA_TYPES_BYTESTRING])) {
    return CPKT_OPCUA_ERR_TYPE;
  }
  *value_count_out = variant->arrayLength;
  native_values = (UA_ByteString *)variant->data;
  for (i = 0; i < variant->arrayLength; ++i) {
    if (fn(i, (const unsigned char *)native_values[i].data, native_values[i].length, user) != 0) {
      return CPKT_OPCUA_ERR_CALLBACK;
    }
  }
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_each_qualified_name_array_from_variant(
    const UA_Variant *variant,
    cpkt_opcua_qualified_name_array_fn fn,
    void *user,
    size_t *value_count_out) {
  UA_QualifiedName *native_values;
  size_t i;

  if (value_count_out != NULL) {
    *value_count_out = 0;
  }
  if (variant == NULL || fn == NULL || value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!UA_Variant_hasArrayType(variant, &UA_TYPES[UA_TYPES_QUALIFIEDNAME])) {
    return CPKT_OPCUA_ERR_TYPE;
  }
  *value_count_out = variant->arrayLength;
  native_values = (UA_QualifiedName *)variant->data;
  for (i = 0; i < variant->arrayLength; ++i) {
    if (fn(
            i,
            (unsigned short)native_values[i].namespaceIndex,
            (const char *)native_values[i].name.data,
            native_values[i].name.length,
            user) != 0) {
      return CPKT_OPCUA_ERR_CALLBACK;
    }
  }
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_each_qualified_name_range_from_variant(
    const UA_Variant *variant,
    cpkt_opcua_qualified_name_array_fn fn,
    void *user,
    size_t *value_count_out) {
  UA_QualifiedName *native_value;

  if (value_count_out != NULL) {
    *value_count_out = 0;
  }
  if (variant == NULL || fn == NULL || value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_QUALIFIEDNAME])) {
    native_value = (UA_QualifiedName *)variant->data;
    *value_count_out = 1;
    return fn(
               0,
               (unsigned short)native_value->namespaceIndex,
               (const char *)native_value->name.data,
               native_value->name.length,
               user) == 0
               ? CPKT_OPCUA_OK
               : CPKT_OPCUA_ERR_CALLBACK;
  }
  return cpkt_each_qualified_name_array_from_variant(variant, fn, user, value_count_out);
}

static cpkt_opcua_result cpkt_each_localized_text_array_from_variant(
    const UA_Variant *variant,
    cpkt_opcua_localized_text_array_fn fn,
    void *user,
    size_t *value_count_out) {
  UA_LocalizedText *native_values;
  size_t i;

  if (value_count_out != NULL) {
    *value_count_out = 0;
  }
  if (variant == NULL || fn == NULL || value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!UA_Variant_hasArrayType(variant, &UA_TYPES[UA_TYPES_LOCALIZEDTEXT])) {
    return CPKT_OPCUA_ERR_TYPE;
  }
  *value_count_out = variant->arrayLength;
  native_values = (UA_LocalizedText *)variant->data;
  for (i = 0; i < variant->arrayLength; ++i) {
    if (fn(
            i,
            (const char *)native_values[i].locale.data,
            native_values[i].locale.length,
            (const char *)native_values[i].text.data,
            native_values[i].text.length,
            user) != 0) {
      return CPKT_OPCUA_ERR_CALLBACK;
    }
  }
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_each_localized_text_range_from_variant(
    const UA_Variant *variant,
    cpkt_opcua_localized_text_array_fn fn,
    void *user,
    size_t *value_count_out) {
  UA_LocalizedText *native_value;

  if (value_count_out != NULL) {
    *value_count_out = 0;
  }
  if (variant == NULL || fn == NULL || value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_LOCALIZEDTEXT])) {
    native_value = (UA_LocalizedText *)variant->data;
    *value_count_out = 1;
    return fn(
               0,
               (const char *)native_value->locale.data,
               native_value->locale.length,
               (const char *)native_value->text.data,
               native_value->text.length,
               user) == 0
               ? CPKT_OPCUA_OK
               : CPKT_OPCUA_ERR_CALLBACK;
  }
  return cpkt_each_localized_text_array_from_variant(variant, fn, user, value_count_out);
}

static cpkt_opcua_result cpkt_each_string_range_from_variant(
    const UA_Variant *variant,
    cpkt_opcua_string_array_fn fn,
    void *user,
    size_t *value_count_out) {
  UA_String *native_value;

  if (value_count_out != NULL) {
    *value_count_out = 0;
  }
  if (variant == NULL || fn == NULL || value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_STRING])) {
    native_value = (UA_String *)variant->data;
    *value_count_out = 1;
    return fn(0, (const char *)native_value->data, native_value->length, user) == 0
        ? CPKT_OPCUA_OK
        : CPKT_OPCUA_ERR_CALLBACK;
  }
  return cpkt_each_string_array_from_variant(variant, fn, user, value_count_out);
}

static cpkt_opcua_result cpkt_each_byte_string_range_from_variant(
    const UA_Variant *variant,
    cpkt_opcua_byte_string_array_fn fn,
    void *user,
    size_t *value_count_out) {
  UA_ByteString *native_value;

  if (value_count_out != NULL) {
    *value_count_out = 0;
  }
  if (variant == NULL || fn == NULL || value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_BYTESTRING])) {
    native_value = (UA_ByteString *)variant->data;
    *value_count_out = 1;
    return fn(0, (const unsigned char *)native_value->data, native_value->length, user) == 0
        ? CPKT_OPCUA_OK
        : CPKT_OPCUA_ERR_CALLBACK;
  }
  return cpkt_each_byte_string_array_from_variant(variant, fn, user, value_count_out);
}

static int cpkt_parse_array_index_component(const char **cursor, size_t *value_out) {
  const char *p;
  size_t value;
  size_t digit;

  if (cursor == NULL || *cursor == NULL || value_out == NULL || **cursor < '0' || **cursor > '9') {
    return 0;
  }
  p = *cursor;
  value = 0;
  while (*p >= '0' && *p <= '9') {
    digit = (size_t)(*p - '0');
    if (value > (((size_t)-1) - digit) / 10U) {
      return 0;
    }
    value = (value * 10U) + digit;
    ++p;
  }
  *cursor = p;
  *value_out = value;
  return 1;
}

static int cpkt_parse_simple_array_index_range(const char *index_range, size_t *start_out, size_t *count_out) {
  const char *p;
  size_t start;
  size_t end;

  if (index_range == NULL || start_out == NULL || count_out == NULL) {
    return 0;
  }
  p = index_range;
  if (!cpkt_parse_array_index_component(&p, &start)) {
    return 0;
  }
  end = start;
  if (*p == ':') {
    ++p;
    if (!cpkt_parse_array_index_component(&p, &end) || end < start) {
      return 0;
    }
  }
  if (*p != '\0' || end == (size_t)-1) {
    return 0;
  }
  *start_out = start;
  *count_out = (end - start) + 1U;
  return 1;
}

static cpkt_opcua_result cpkt_each_string_array_slice_from_variant(
    const UA_Variant *variant,
    size_t start,
    size_t count,
    cpkt_opcua_string_array_fn fn,
    void *user,
    size_t *value_count_out) {
  UA_String *native_values;
  size_t i;

  if (value_count_out != NULL) {
    *value_count_out = 0;
  }
  if (variant == NULL || fn == NULL || value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!UA_Variant_hasArrayType(variant, &UA_TYPES[UA_TYPES_STRING])) {
    return CPKT_OPCUA_ERR_TYPE;
  }
  if (start > variant->arrayLength || count > variant->arrayLength - start) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  *value_count_out = count;
  native_values = (UA_String *)variant->data;
  for (i = 0; i < count; ++i) {
    if (fn(i, (const char *)native_values[start + i].data, native_values[start + i].length, user) != 0) {
      return CPKT_OPCUA_ERR_CALLBACK;
    }
  }
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_each_byte_string_array_slice_from_variant(
    const UA_Variant *variant,
    size_t start,
    size_t count,
    cpkt_opcua_byte_string_array_fn fn,
    void *user,
    size_t *value_count_out) {
  UA_ByteString *native_values;
  size_t i;

  if (value_count_out != NULL) {
    *value_count_out = 0;
  }
  if (variant == NULL || fn == NULL || value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!UA_Variant_hasArrayType(variant, &UA_TYPES[UA_TYPES_BYTESTRING])) {
    return CPKT_OPCUA_ERR_TYPE;
  }
  if (start > variant->arrayLength || count > variant->arrayLength - start) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  *value_count_out = count;
  native_values = (UA_ByteString *)variant->data;
  for (i = 0; i < count; ++i) {
    if (fn(i, (const unsigned char *)native_values[start + i].data, native_values[start + i].length, user) != 0) {
      return CPKT_OPCUA_ERR_CALLBACK;
    }
  }
  return CPKT_OPCUA_OK;
}

static UA_Argument *cpkt_make_method_arguments(
    const int *types,
    size_t type_count,
    const char *name_prefix) {
  UA_Argument *arguments;
  size_t i;
  char name_buffer[32];

  if (type_count == 0) {
    return NULL;
  }
  if (types == NULL || name_prefix == NULL) {
    return NULL;
  }
  arguments = (UA_Argument *)calloc(type_count, sizeof(*arguments));
  if (arguments == NULL) {
    return NULL;
  }
  for (i = 0; i < type_count; ++i) {
    UA_Argument_init(&arguments[i]);
    if (!cpkt_valid_method_value_type(types[i])) {
      while (i > 0) {
        --i;
        UA_Argument_clear(&arguments[i]);
      }
      free(arguments);
      return NULL;
    }
    snprintf(name_buffer, sizeof(name_buffer), "%s%lu", name_prefix, (unsigned long)(i + 1));
    arguments[i].description = UA_LOCALIZEDTEXT_ALLOC((char *)"en-US", name_buffer);
    arguments[i].name = UA_STRING_ALLOC(name_buffer);
    if (arguments[i].description.text.data == NULL || arguments[i].name.data == NULL) {
      UA_Argument_clear(&arguments[i]);
      while (i > 0) {
        --i;
        UA_Argument_clear(&arguments[i]);
      }
      free(arguments);
      return NULL;
    }
    arguments[i].dataType = cpkt_data_type_node_id_for_value_type(types[i]);
    arguments[i].valueRank = UA_VALUERANK_SCALAR;
  }
  return arguments;
}

static void cpkt_clear_method_arguments(UA_Argument *arguments, size_t argument_count) {
  size_t i;

  if (arguments == NULL) {
    return;
  }
  for (i = 0; i < argument_count; ++i) {
    UA_Argument_clear(&arguments[i]);
  }
  free(arguments);
}

static int cpkt_valid_method_argument_direction(int direction) {
  return direction == CPKT_OPCUA_METHOD_ARGUMENT_INPUT ||
         direction == CPKT_OPCUA_METHOD_ARGUMENT_OUTPUT;
}

static const char *cpkt_method_argument_property_name(int direction) {
  return direction == CPKT_OPCUA_METHOD_ARGUMENT_INPUT ? "InputArguments" : "OutputArguments";
}

static cpkt_opcua_result cpkt_decode_method_argument_count(
    const UA_Variant *variant,
    size_t *argument_count_out) {
  if (argument_count_out != NULL) {
    *argument_count_out = 0;
  }
  if (variant == NULL || argument_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!UA_Variant_hasArrayType(variant, &UA_TYPES[UA_TYPES_ARGUMENT])) {
    return CPKT_OPCUA_ERR_TYPE;
  }
  *argument_count_out = variant->arrayLength;
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_decode_method_argument(
    const UA_Variant *variant,
    size_t argument_index,
    cpkt_opcua_node_id *data_type_out,
    long *value_rank_out,
    char *name_buffer,
    size_t name_buffer_size,
    size_t *required_name_size_out) {
  const UA_Argument *arguments;
  const UA_Argument *argument;
  struct cpkt_owned_node_id_memory data_type_memory;
  cpkt_opcua_node_id data_type;
  cpkt_opcua_result result;

  if (required_name_size_out != NULL) {
    *required_name_size_out = 0;
  }
  if (data_type_out != NULL) {
    *data_type_out = cpkt_opcua_node_id_null();
  }
  if (value_rank_out != NULL) {
    *value_rank_out = 0;
  }
  if (variant == NULL || data_type_out == NULL || value_rank_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!UA_Variant_hasArrayType(variant, &UA_TYPES[UA_TYPES_ARGUMENT])) {
    return CPKT_OPCUA_ERR_TYPE;
  }
  if (argument_index >= variant->arrayLength) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  arguments = (const UA_Argument *)variant->data;
  argument = &arguments[argument_index];
  data_type_memory.string = NULL;
  data_type_memory.byte_string = NULL;
  if (!cpkt_native_node_id_to_facade(&argument->dataType, &data_type, &data_type_memory)) {
    cpkt_owned_node_id_memory_clear(&data_type_memory);
    return CPKT_OPCUA_ERR_TYPE;
  }
  if (data_type.identifier_type != CPKT_OPCUA_NODE_ID_NUMERIC &&
      data_type.identifier_type != CPKT_OPCUA_NODE_ID_NULL) {
    cpkt_owned_node_id_memory_clear(&data_type_memory);
    return CPKT_OPCUA_ERR_TYPE;
  }
  *data_type_out = data_type;
  *value_rank_out = (long)argument->valueRank;
  cpkt_owned_node_id_memory_clear(&data_type_memory);
  result = cpkt_copy_ua_string_to_buffer(
      &argument->name,
      name_buffer,
      name_buffer_size,
      required_name_size_out);
  return result;
}

static cpkt_opcua_result cpkt_browse_path_result_native_node_id(
    UA_BrowsePathResult *browse_path_result,
    UA_NodeId *target_node_id_out,
    cpkt_opcua_status *status_out) {
  if (target_node_id_out != NULL) {
    UA_NodeId_init(target_node_id_out);
  }
  if (browse_path_result == NULL || target_node_id_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (status_out != NULL) {
    *status_out = cpkt_status(browse_path_result->statusCode);
  }
  if (browse_path_result->statusCode != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (browse_path_result->targetsSize == 0) {
    if (status_out != NULL) {
      *status_out = cpkt_status(UA_STATUSCODE_BADNOMATCH);
    }
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (browse_path_result->targets[0].targetId.serverIndex != 0 ||
      browse_path_result->targets[0].targetId.namespaceUri.length != 0 ||
      browse_path_result->targets[0].remainingPathIndex != (UA_UInt32)UINT_MAX) {
    return CPKT_OPCUA_ERR_TYPE;
  }
  *target_node_id_out = browse_path_result->targets[0].targetId.nodeId;
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_server_read_method_argument_variant(
    UA_Server *server,
    cpkt_opcua_node_id method_node_id,
    int direction,
    UA_Variant *variant_out,
    cpkt_opcua_status *status_out) {
  cpkt_opcua_browse_path_element element;
  UA_BrowsePath browse_path;
  UA_BrowsePathResult browse_path_result;
  UA_NodeId argument_node_id;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (variant_out != NULL) {
    UA_Variant_init(variant_out);
  }
  if (server == NULL || variant_out == NULL || !cpkt_valid_node_id(method_node_id) ||
      !cpkt_valid_method_argument_direction(direction)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  element.namespace_index = 0;
  element.browse_name = cpkt_method_argument_property_name(direction);
  result = cpkt_fill_browse_path(&browse_path, method_node_id, &element, 1);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  browse_path_result = UA_Server_translateBrowsePathToNodeIds(server, &browse_path);
  result = cpkt_browse_path_result_native_node_id(&browse_path_result, &argument_node_id, status_out);
  if (result == CPKT_OPCUA_OK) {
    status = UA_Server_readValue(server, argument_node_id, variant_out);
    if (status_out != NULL) {
      *status_out = cpkt_status(status);
    }
    if (status != UA_STATUSCODE_GOOD) {
      result = CPKT_OPCUA_ERR_UPSTREAM;
    }
  }
  UA_BrowsePathResult_clear(&browse_path_result);
  UA_BrowsePath_clear(&browse_path);
  return result;
}

static cpkt_opcua_result cpkt_client_read_method_argument_variant(
    UA_Client *client,
    cpkt_opcua_node_id method_node_id,
    int direction,
    UA_Variant *variant_out,
    cpkt_opcua_status *status_out) {
  cpkt_opcua_browse_path_element element;
  UA_BrowsePath browse_path;
  UA_BrowsePathResult browse_path_result;
  UA_NodeId argument_node_id;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (variant_out != NULL) {
    UA_Variant_init(variant_out);
  }
  if (client == NULL || variant_out == NULL || !cpkt_valid_node_id(method_node_id) ||
      !cpkt_valid_method_argument_direction(direction)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  element.namespace_index = 0;
  element.browse_name = cpkt_method_argument_property_name(direction);
  result = cpkt_fill_browse_path(&browse_path, method_node_id, &element, 1);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  browse_path_result = UA_Client_translateBrowsePathToNodeIds(client, &browse_path);
  result = cpkt_browse_path_result_native_node_id(&browse_path_result, &argument_node_id, status_out);
  if (result == CPKT_OPCUA_OK) {
    status = UA_Client_readValueAttribute(client, argument_node_id, variant_out);
    if (status_out != NULL) {
      *status_out = cpkt_status(status);
    }
    if (status != UA_STATUSCODE_GOOD) {
      result = CPKT_OPCUA_ERR_UPSTREAM;
    }
  }
  UA_BrowsePathResult_clear(&browse_path_result);
  UA_BrowsePath_clear(&browse_path);
  return result;
}

static UA_StatusCode cpkt_method_callback(
    UA_Server *native_server,
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
  struct cpkt_opcua_method_context *context;
  cpkt_opcua_value *inputs;
  cpkt_opcua_value *outputs;
  char **string_buffers;
  cpkt_opcua_result result;
  size_t required;
  size_t i;
  size_t j;

  (void)native_server;
  (void)session_id;
  (void)session_context;
  (void)method_id;
  (void)object_id;
  (void)object_context;
  context = (struct cpkt_opcua_method_context *)method_context;
  if (context == NULL || (context->fn == NULL && context->single_fn == NULL) ||
      input_size != context->input_count || output_size != context->output_count || output == NULL) {
    return UA_STATUSCODE_BADINTERNALERROR;
  }
  inputs = NULL;
  outputs = NULL;
  string_buffers = NULL;
  if (output_size == 0) {
    return UA_STATUSCODE_BADINTERNALERROR;
  }
  if (input_size != 0) {
    inputs = (cpkt_opcua_value *)calloc(input_size, sizeof(*inputs));
    if (inputs == NULL) {
      return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    string_buffers = (char **)calloc(input_size, sizeof(*string_buffers));
    if (string_buffers == NULL) {
      free(inputs);
      return UA_STATUSCODE_BADOUTOFMEMORY;
    }
  }
  outputs = (cpkt_opcua_value *)calloc(output_size, sizeof(*outputs));
  if (outputs == NULL) {
    free(string_buffers);
    free(inputs);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  for (i = 0; i < input_size; ++i) {
    result = cpkt_get_variant(&input[i], &inputs[i], NULL, 0, &required);
    if (result == CPKT_OPCUA_ERR_RANGE && cpkt_value_type_needs_buffer(inputs[i].type) &&
        required != 0) {
      string_buffers[i] = (char *)malloc(required);
      if (string_buffers[i] == NULL) {
        for (j = 0; j < i; ++j) {
          free(string_buffers[j]);
        }
        free(string_buffers);
        free(inputs);
        free(outputs);
        return UA_STATUSCODE_BADOUTOFMEMORY;
      }
      result = cpkt_get_variant(&input[i], &inputs[i], string_buffers[i], required, &required);
    }
    if (result != CPKT_OPCUA_OK || inputs[i].type != context->input_types[i]) {
      for (j = 0; j <= i; ++j) {
        free(string_buffers[j]);
      }
      free(string_buffers);
      free(inputs);
      free(outputs);
      return UA_STATUSCODE_BADTYPEMISMATCH;
    }
  }
  for (i = 0; i < output_size; ++i) {
    cpkt_opcua_value_clear(&outputs[i]);
  }
  if (context->single_fn != NULL) {
    if (output_size != 1) {
      result = CPKT_OPCUA_ERR_ARG;
    } else {
      result = context->single_fn(inputs, input_size, &outputs[0], context->user);
    }
  } else {
    result = context->fn(inputs, input_size, outputs, output_size, context->user);
  }
  if (result == CPKT_OPCUA_OK) {
    for (i = 0; i < output_size; ++i) {
      if (outputs[i].type != context->output_types[i]) {
        result = CPKT_OPCUA_ERR_TYPE;
        break;
      }
    }
  }
  if (result == CPKT_OPCUA_OK) {
    for (i = 0; i < output_size; ++i) {
      result = cpkt_set_variant(&output[i], &outputs[i]);
      if (result != CPKT_OPCUA_OK) {
        break;
      }
    }
  }
  for (i = 0; i < input_size; ++i) {
    free(string_buffers[i]);
  }
  free(string_buffers);
  free(inputs);
  free(outputs);
  if (result != CPKT_OPCUA_OK) {
    if (result == CPKT_OPCUA_ERR_ALLOC) {
      return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    if (result == CPKT_OPCUA_ERR_TYPE || result == CPKT_OPCUA_ERR_RANGE) {
      return UA_STATUSCODE_BADTYPEMISMATCH;
    }
    return UA_STATUSCODE_BADINTERNALERROR;
  }
  return UA_STATUSCODE_GOOD;
}

/** Returns the linked open62541 version string. */
const char *cpkt_opcua_open62541_version(void) { return UA_OPEN62541_VERSION; }

/** Returns the public OPC UA facade ABI version string. */
const char *cpkt_opcua_facade_version(void) { return CPKT_OPCUA_FACADE_VERSION; }

/** Converts an upstream status code into its stable diagnostic name. */
const char *cpkt_opcua_status_name(cpkt_opcua_status status) {
  return UA_StatusCode_name((UA_StatusCode)status);
}

/** Converts an OPC UA facade result code into a stable diagnostic string. */
const char *cpkt_opcua_result_string(cpkt_opcua_result result) {
  switch (result) {
    case CPKT_OPCUA_OK:
      return "ok";
    case CPKT_OPCUA_ERR_ARG:
      return "invalid argument";
    case CPKT_OPCUA_ERR_ALLOC:
      return "allocation failed";
    case CPKT_OPCUA_ERR_UPSTREAM:
      return "open62541 operation failed";
    case CPKT_OPCUA_ERR_TYPE:
      return "unsupported value type";
    case CPKT_OPCUA_ERR_RANGE:
      return "value or buffer out of range";
    case CPKT_OPCUA_ERR_CALLBACK:
      return "callback failed";
    default:
      return "unknown result";
  }
}

static int cpkt_hex_digit_value(char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'a' && value <= 'f') {
    return value - 'a' + 10;
  }
  if (value >= 'A' && value <= 'F') {
    return value - 'A' + 10;
  }
  return -1;
}

static int cpkt_parse_hex_byte(const char *text, unsigned char *value_out) {
  int high;
  int low;

  high = cpkt_hex_digit_value(text[0]);
  low = cpkt_hex_digit_value(text[1]);
  if (high < 0 || low < 0) {
    return 0;
  }
  *value_out = (unsigned char)((high << 4) | low);
  return 1;
}

static int cpkt_parse_guid_text(const char *text, unsigned char guid_out[16]) {
  if (text == NULL || guid_out == NULL ||
      strlen(text) != 36 || text[8] != '-' || text[13] != '-' ||
      text[18] != '-' || text[23] != '-') {
    return 0;
  }
  return cpkt_parse_hex_byte(text, &guid_out[0]) &&
      cpkt_parse_hex_byte(text + 2, &guid_out[1]) &&
      cpkt_parse_hex_byte(text + 4, &guid_out[2]) &&
      cpkt_parse_hex_byte(text + 6, &guid_out[3]) &&
      cpkt_parse_hex_byte(text + 9, &guid_out[4]) &&
      cpkt_parse_hex_byte(text + 11, &guid_out[5]) &&
      cpkt_parse_hex_byte(text + 14, &guid_out[6]) &&
      cpkt_parse_hex_byte(text + 16, &guid_out[7]) &&
      cpkt_parse_hex_byte(text + 19, &guid_out[8]) &&
      cpkt_parse_hex_byte(text + 21, &guid_out[9]) &&
      cpkt_parse_hex_byte(text + 24, &guid_out[10]) &&
      cpkt_parse_hex_byte(text + 26, &guid_out[11]) &&
      cpkt_parse_hex_byte(text + 28, &guid_out[12]) &&
      cpkt_parse_hex_byte(text + 30, &guid_out[13]) &&
      cpkt_parse_hex_byte(text + 32, &guid_out[14]) &&
      cpkt_parse_hex_byte(text + 34, &guid_out[15]);
}

static size_t cpkt_base64_encoded_size(size_t length) {
  return ((length + 2) / 3) * 4;
}

static void cpkt_base64_encode(const unsigned char *data, size_t length, char *out) {
  static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t i;
  size_t o;
  unsigned long value;
  unsigned long remaining;

  o = 0;
  for (i = 0; i < length; i += 3) {
    remaining = (unsigned long)(length - i);
    value = (unsigned long)data[i] << 16;
    if (remaining > 1) {
      value |= (unsigned long)data[i + 1] << 8;
    }
    if (remaining > 2) {
      value |= (unsigned long)data[i + 2];
    }
    out[o++] = alphabet[(value >> 18) & 0x3fU];
    out[o++] = alphabet[(value >> 12) & 0x3fU];
    out[o++] = remaining > 1 ? alphabet[(value >> 6) & 0x3fU] : '=';
    out[o++] = remaining > 2 ? alphabet[value & 0x3fU] : '=';
  }
  out[o] = '\0';
}

static int cpkt_base64_value(char value) {
  if (value >= 'A' && value <= 'Z') {
    return value - 'A';
  }
  if (value >= 'a' && value <= 'z') {
    return value - 'a' + 26;
  }
  if (value >= '0' && value <= '9') {
    return value - '0' + 52;
  }
  if (value == '+') {
    return 62;
  }
  if (value == '/') {
    return 63;
  }
  return -1;
}

static int cpkt_base64_decode(
    const char *text,
    unsigned char *buffer,
    size_t buffer_size,
    size_t *required_size_out) {
  size_t length;
  size_t required;
  size_t i;
  size_t o;
  int values[4];
  unsigned long packed;

  length = strlen(text);
  if ((length % 4) != 0) {
    return 0;
  }
  required = (length / 4) * 3;
  if (length != 0 && text[length - 1] == '=') {
    --required;
    if (text[length - 2] == '=') {
      --required;
    }
  }
  if (required_size_out != NULL) {
    *required_size_out = required;
  }
  if (required != 0 && (buffer == NULL || buffer_size < required)) {
    return 2;
  }
  o = 0;
  for (i = 0; i < length; i += 4) {
    values[0] = cpkt_base64_value(text[i]);
    values[1] = cpkt_base64_value(text[i + 1]);
    values[2] = text[i + 2] == '=' ? 0 : cpkt_base64_value(text[i + 2]);
    values[3] = text[i + 3] == '=' ? 0 : cpkt_base64_value(text[i + 3]);
    if (values[0] < 0 || values[1] < 0 || values[2] < 0 || values[3] < 0 ||
        (text[i + 2] == '=' && text[i + 3] != '=') ||
        (i + 4 != length && (text[i + 2] == '=' || text[i + 3] == '='))) {
      return 0;
    }
    packed =
        ((unsigned long)values[0] << 18) |
        ((unsigned long)values[1] << 12) |
        ((unsigned long)values[2] << 6) |
        (unsigned long)values[3];
    if (o < required) {
      buffer[o++] = (unsigned char)((packed >> 16) & 0xffU);
    }
    if (o < required) {
      buffer[o++] = (unsigned char)((packed >> 8) & 0xffU);
    }
    if (o < required) {
      buffer[o++] = (unsigned char)(packed & 0xffU);
    }
  }
  return 1;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_node_id cpkt_opcua_node_id_null(void) {
  cpkt_opcua_node_id node_id;
  node_id.namespace_index = 0;
  node_id.identifier_type = CPKT_OPCUA_NODE_ID_NULL;
  node_id.numeric = 0;
  node_id.string = NULL;
  memset(node_id.guid, 0, sizeof(node_id.guid));
  node_id.byte_string = NULL;
  node_id.byte_string_length = 0;
  return node_id;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_node_id cpkt_opcua_node_id_numeric(
    unsigned short namespace_index,
    unsigned long identifier) {
  cpkt_opcua_node_id node_id;
  node_id = cpkt_opcua_node_id_null();
  node_id.namespace_index = namespace_index;
  node_id.identifier_type = CPKT_OPCUA_NODE_ID_NUMERIC;
  node_id.numeric = identifier;
  return node_id;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_node_id cpkt_opcua_node_id_string(
    unsigned short namespace_index,
    const char *identifier) {
  cpkt_opcua_node_id node_id;
  node_id = cpkt_opcua_node_id_null();
  node_id.namespace_index = namespace_index;
  node_id.identifier_type = CPKT_OPCUA_NODE_ID_STRING;
  node_id.string = identifier;
  return node_id;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_node_id cpkt_opcua_node_id_guid(
    unsigned short namespace_index,
    const unsigned char guid[16]) {
  cpkt_opcua_node_id node_id;
  node_id = cpkt_opcua_node_id_null();
  node_id.namespace_index = namespace_index;
  node_id.identifier_type = CPKT_OPCUA_NODE_ID_GUID;
  if (guid != NULL) {
    memcpy(node_id.guid, guid, sizeof(node_id.guid));
  }
  return node_id;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_node_id cpkt_opcua_node_id_byte_string(
    unsigned short namespace_index,
    const unsigned char *identifier,
    size_t identifier_length) {
  cpkt_opcua_node_id node_id;
  node_id = cpkt_opcua_node_id_null();
  node_id.namespace_index = namespace_index;
  node_id.identifier_type = CPKT_OPCUA_NODE_ID_BYTE_STRING;
  node_id.byte_string = identifier;
  node_id.byte_string_length = identifier_length;
  return node_id;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
int cpkt_opcua_node_id_equal(cpkt_opcua_node_id a, cpkt_opcua_node_id b) {
  if (a.namespace_index != b.namespace_index || a.identifier_type != b.identifier_type) {
    return 0;
  }
  if (a.identifier_type == CPKT_OPCUA_NODE_ID_NULL) {
    return 1;
  }
  if (a.identifier_type == CPKT_OPCUA_NODE_ID_NUMERIC) {
    return a.numeric == b.numeric ? 1 : 0;
  }
  if (a.identifier_type == CPKT_OPCUA_NODE_ID_STRING) {
    if (a.string == NULL || b.string == NULL) {
      return a.string == b.string ? 1 : 0;
    }
    return strcmp(a.string, b.string) == 0 ? 1 : 0;
  }
  if (a.identifier_type == CPKT_OPCUA_NODE_ID_GUID) {
    return memcmp(a.guid, b.guid, sizeof(a.guid)) == 0 ? 1 : 0;
  }
  if (a.identifier_type == CPKT_OPCUA_NODE_ID_BYTE_STRING) {
    if (a.byte_string_length != b.byte_string_length) {
      return 0;
    }
    if (a.byte_string_length == 0) {
      return 1;
    }
    if (a.byte_string == NULL || b.byte_string == NULL) {
      return 0;
    }
    return memcmp(a.byte_string, b.byte_string, a.byte_string_length) == 0 ? 1 : 0;
  }
  return 0;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_node_id_print(
    cpkt_opcua_node_id node_id,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out) {
  char prefix[64];
  size_t prefix_length;
  size_t value_length;
  size_t required;
  size_t encoded_length;

  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (!cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (node_id.identifier_type == CPKT_OPCUA_NODE_ID_NULL) {
    return cpkt_copy_c_string_to_buffer("i=0", buffer, buffer_size, required_size_out);
  }
  if (node_id.identifier_type == CPKT_OPCUA_NODE_ID_NUMERIC) {
    sprintf(prefix, "ns=%hu;i=%lu", node_id.namespace_index, node_id.numeric);
    return cpkt_copy_c_string_to_buffer(prefix, buffer, buffer_size, required_size_out);
  }
  if (node_id.identifier_type == CPKT_OPCUA_NODE_ID_STRING) {
    sprintf(prefix, "ns=%hu;s=", node_id.namespace_index);
    prefix_length = strlen(prefix);
    value_length = strlen(node_id.string);
    required = prefix_length + value_length + 1;
    if (required_size_out != NULL) {
      *required_size_out = required;
    }
    if (buffer == NULL || buffer_size < required) {
      if (buffer != NULL && buffer_size != 0) {
        buffer[0] = '\0';
      }
      return CPKT_OPCUA_ERR_RANGE;
    }
    memcpy(buffer, prefix, prefix_length);
    memcpy(buffer + prefix_length, node_id.string, value_length + 1);
    return CPKT_OPCUA_OK;
  }
  if (node_id.identifier_type == CPKT_OPCUA_NODE_ID_GUID) {
    sprintf(
        prefix,
        "ns=%hu;g=%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        node_id.namespace_index,
        node_id.guid[0],
        node_id.guid[1],
        node_id.guid[2],
        node_id.guid[3],
        node_id.guid[4],
        node_id.guid[5],
        node_id.guid[6],
        node_id.guid[7],
        node_id.guid[8],
        node_id.guid[9],
        node_id.guid[10],
        node_id.guid[11],
        node_id.guid[12],
        node_id.guid[13],
        node_id.guid[14],
        node_id.guid[15]);
    return cpkt_copy_c_string_to_buffer(prefix, buffer, buffer_size, required_size_out);
  }
  sprintf(prefix, "ns=%hu;b=", node_id.namespace_index);
  prefix_length = strlen(prefix);
  encoded_length = cpkt_base64_encoded_size(node_id.byte_string_length);
  required = prefix_length + encoded_length + 1;
  if (required_size_out != NULL) {
    *required_size_out = required;
  }
  if (buffer == NULL || buffer_size < required) {
    if (buffer != NULL && buffer_size != 0) {
      buffer[0] = '\0';
    }
    return CPKT_OPCUA_ERR_RANGE;
  }
  memcpy(buffer, prefix, prefix_length);
  cpkt_base64_encode(node_id.byte_string, node_id.byte_string_length, buffer + prefix_length);
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_node_id_parse(
    const char *text,
    cpkt_opcua_node_id *node_id_out,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out) {
  const char *cursor;
  const char *end;
  unsigned long parsed;
  unsigned short namespace_index;
  cpkt_opcua_result copy_result;
  unsigned char guid[16];
  int decode_result;
  size_t decoded_size;

  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (node_id_out != NULL) {
    *node_id_out = cpkt_opcua_node_id_null();
  }
  if (text == NULL || node_id_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }

  cursor = text;
  namespace_index = 0;
  if (strncmp(cursor, "ns=", 3) == 0) {
    cursor += 3;
    if (!cpkt_parse_unsigned_long(cursor, &end, &parsed) || parsed > (unsigned long)USHRT_MAX ||
        *end != ';') {
      return CPKT_OPCUA_ERR_ARG;
    }
    namespace_index = (unsigned short)parsed;
    cursor = end + 1;
  }

  if (strncmp(cursor, "i=", 2) == 0) {
    cursor += 2;
    if (!cpkt_parse_unsigned_long(cursor, &end, &parsed) || *end != '\0') {
      return CPKT_OPCUA_ERR_ARG;
    }
    if (namespace_index == 0 && parsed == 0) {
      *node_id_out = cpkt_opcua_node_id_null();
    } else {
      *node_id_out = cpkt_opcua_node_id_numeric(namespace_index, parsed);
    }
    return CPKT_OPCUA_OK;
  }
  if (strncmp(cursor, "s=", 2) == 0) {
    cursor += 2;
    copy_result = cpkt_copy_c_string_to_buffer(
        cursor,
        buffer,
        buffer_size,
        required_size_out);
    if (copy_result != CPKT_OPCUA_OK) {
      return copy_result;
    }
    *node_id_out = cpkt_opcua_node_id_string(namespace_index, buffer);
    return CPKT_OPCUA_OK;
  }
  if (strncmp(cursor, "g=", 2) == 0) {
    cursor += 2;
    if (!cpkt_parse_guid_text(cursor, guid)) {
      return CPKT_OPCUA_ERR_ARG;
    }
    *node_id_out = cpkt_opcua_node_id_guid(namespace_index, guid);
    return CPKT_OPCUA_OK;
  }
  if (strncmp(cursor, "b=", 2) == 0) {
    cursor += 2;
    decoded_size = 0;
    decode_result = cpkt_base64_decode(
        cursor,
        (unsigned char *)buffer,
        buffer_size,
        &decoded_size);
    if (required_size_out != NULL) {
      *required_size_out = decoded_size;
    }
    if (decode_result == 0) {
      return CPKT_OPCUA_ERR_ARG;
    }
    if (decode_result == 2) {
      return CPKT_OPCUA_ERR_RANGE;
    }
    *node_id_out = cpkt_opcua_node_id_byte_string(
        namespace_index,
        (const unsigned char *)buffer,
        decoded_size);
    return CPKT_OPCUA_OK;
  }
  return CPKT_OPCUA_ERR_ARG;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_expanded_node_id cpkt_opcua_expanded_node_id_local(cpkt_opcua_node_id node_id) {
  cpkt_opcua_expanded_node_id expanded;

  expanded.node_id = node_id;
  expanded.namespace_uri = NULL;
  expanded.namespace_uri_length = 0;
  expanded.server_index = 0;
  return expanded;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_expanded_node_id cpkt_opcua_expanded_node_id_uri(
    const char *namespace_uri,
    size_t namespace_uri_length,
    cpkt_opcua_node_id node_id) {
  cpkt_opcua_expanded_node_id expanded;

  expanded = cpkt_opcua_expanded_node_id_local(node_id);
  expanded.namespace_uri = namespace_uri;
  expanded.namespace_uri_length = namespace_uri_length;
  return expanded;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_expanded_node_id cpkt_opcua_expanded_node_id_server(
    unsigned long server_index,
    cpkt_opcua_node_id node_id) {
  cpkt_opcua_expanded_node_id expanded;

  expanded = cpkt_opcua_expanded_node_id_local(node_id);
  expanded.server_index = server_index;
  return expanded;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_expanded_node_id cpkt_opcua_expanded_node_id_server_uri(
    unsigned long server_index,
    const char *namespace_uri,
    size_t namespace_uri_length,
    cpkt_opcua_node_id node_id) {
  cpkt_opcua_expanded_node_id expanded;

  expanded = cpkt_opcua_expanded_node_id_uri(namespace_uri, namespace_uri_length, node_id);
  expanded.server_index = server_index;
  return expanded;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
int cpkt_opcua_expanded_node_id_equal(
    cpkt_opcua_expanded_node_id a,
    cpkt_opcua_expanded_node_id b) {
  if (a.server_index != b.server_index || a.namespace_uri_length != b.namespace_uri_length ||
      !cpkt_opcua_node_id_equal(a.node_id, b.node_id)) {
    return 0;
  }
  if (a.namespace_uri_length == 0) {
    return 1;
  }
  if (a.namespace_uri == NULL || b.namespace_uri == NULL) {
    return 0;
  }
  return memcmp(a.namespace_uri, b.namespace_uri, a.namespace_uri_length) == 0 ? 1 : 0;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_expanded_node_id_print(
    cpkt_opcua_expanded_node_id node_id,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out) {
  char *inner;
  char prefix[64];
  size_t inner_required;
  size_t prefix_length;
  size_t required;
  cpkt_opcua_result result;

  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (!cpkt_valid_expanded_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  inner = NULL;
  result = cpkt_opcua_node_id_print(node_id.node_id, NULL, 0, &inner_required);
  if (result != CPKT_OPCUA_ERR_RANGE) {
    return result == CPKT_OPCUA_OK ? CPKT_OPCUA_ERR_ALLOC : result;
  }
  inner = (char *)malloc(inner_required);
  if (inner == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  result = cpkt_opcua_node_id_print(node_id.node_id, inner, inner_required, &inner_required);
  if (result != CPKT_OPCUA_OK) {
    free(inner);
    return result;
  }
  prefix[0] = '\0';
  if (node_id.server_index != 0) {
    sprintf(prefix + strlen(prefix), "svr=%lu;", node_id.server_index);
  }
  if (node_id.namespace_uri_length != 0) {
    sprintf(prefix + strlen(prefix), "nsu=");
  }
  prefix_length = strlen(prefix);
  required = prefix_length + node_id.namespace_uri_length +
      (node_id.namespace_uri_length != 0 ? 1 : 0) + strlen(inner) + 1;
  if (required_size_out != NULL) {
    *required_size_out = required;
  }
  if (buffer == NULL || buffer_size < required) {
    if (buffer != NULL && buffer_size != 0) {
      buffer[0] = '\0';
    }
    free(inner);
    return CPKT_OPCUA_ERR_RANGE;
  }
  memcpy(buffer, prefix, prefix_length);
  if (node_id.namespace_uri_length != 0) {
    memcpy(buffer + prefix_length, node_id.namespace_uri, node_id.namespace_uri_length);
    buffer[prefix_length + node_id.namespace_uri_length] = ';';
    memcpy(
        buffer + prefix_length + node_id.namespace_uri_length + 1,
        inner,
        strlen(inner) + 1);
  } else {
    memcpy(buffer + prefix_length, inner, strlen(inner) + 1);
  }
  free(inner);
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_expanded_node_id_parse(
    const char *text,
    cpkt_opcua_expanded_node_id *node_id_out,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out) {
  const char *cursor;
  const char *end;
  unsigned long parsed;
  char *node_buffer;
  size_t namespace_uri_length;
  size_t node_buffer_size;
  size_t node_required;
  cpkt_opcua_node_id node_id;
  cpkt_opcua_result result;

  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (node_id_out != NULL) {
    *node_id_out = cpkt_opcua_expanded_node_id_local(cpkt_opcua_node_id_null());
  }
  if (text == NULL || node_id_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  cursor = text;
  parsed = 0;
  if (strncmp(cursor, "svr=", 4) == 0) {
    cursor += 4;
    if (!cpkt_parse_unsigned_long(cursor, &end, &parsed) || parsed > (unsigned long)UINT_MAX ||
        *end != ';') {
      return CPKT_OPCUA_ERR_ARG;
    }
    cursor = end + 1;
  }
  namespace_uri_length = 0;
  if (strncmp(cursor, "nsu=", 4) == 0) {
    cursor += 4;
    end = strchr(cursor, ';');
    if (end == NULL || end == cursor) {
      return CPKT_OPCUA_ERR_ARG;
    }
    namespace_uri_length = (size_t)(end - cursor);
    if (required_size_out != NULL) {
      *required_size_out = namespace_uri_length + 1;
    }
    if (buffer == NULL || buffer_size < namespace_uri_length + 1) {
      if (buffer != NULL && buffer_size != 0) {
        buffer[0] = '\0';
      }
      return CPKT_OPCUA_ERR_RANGE;
    }
    memcpy(buffer, cursor, namespace_uri_length);
    buffer[namespace_uri_length] = '\0';
    cursor = end + 1;
  }
  node_buffer = NULL;
  node_buffer_size = 0;
  if (namespace_uri_length != 0) {
    node_buffer = buffer + namespace_uri_length + 1;
    node_buffer_size = buffer_size - namespace_uri_length - 1;
  } else {
    node_buffer = buffer;
    node_buffer_size = buffer_size;
  }
  node_required = 0;
  result = cpkt_opcua_node_id_parse(cursor, &node_id, node_buffer, node_buffer_size, &node_required);
  if (result != CPKT_OPCUA_OK) {
    if (namespace_uri_length != 0 && result == CPKT_OPCUA_ERR_RANGE && required_size_out != NULL) {
      *required_size_out = namespace_uri_length + 1 + node_required;
    } else if (required_size_out != NULL) {
      *required_size_out = node_required;
    }
    return result;
  }
  if (required_size_out != NULL) {
    *required_size_out = node_required;
    if (namespace_uri_length != 0) {
      *required_size_out += namespace_uri_length + 1;
    }
  }
  *node_id_out = cpkt_opcua_expanded_node_id_local(node_id);
  node_id_out->server_index = parsed;
  if (namespace_uri_length != 0) {
    node_id_out->namespace_uri = buffer;
    node_id_out->namespace_uri_length = namespace_uri_length;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_guid_print(
    const unsigned char guid[16],
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out) {
  char text[37];

  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (guid == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  sprintf(
      text,
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      guid[0],
      guid[1],
      guid[2],
      guid[3],
      guid[4],
      guid[5],
      guid[6],
      guid[7],
      guid[8],
      guid[9],
      guid[10],
      guid[11],
      guid[12],
      guid[13],
      guid[14],
      guid[15]);
  return cpkt_copy_c_string_to_buffer(text, buffer, buffer_size, required_size_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_guid_parse(
    const char *text,
    unsigned char guid_out[16]) {
  if (!cpkt_parse_guid_text(text, guid_out)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_qualified_name_print(
    unsigned short namespace_index,
    const char *name,
    size_t name_length,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out) {
  char prefix[64];
  size_t prefix_length;
  size_t required;

  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (name == NULL && name_length != 0) {
    return CPKT_OPCUA_ERR_ARG;
  }
  sprintf(prefix, "ns=%hu;q=", namespace_index);
  prefix_length = strlen(prefix);
  required = prefix_length + name_length + 1;
  if (required_size_out != NULL) {
    *required_size_out = required;
  }
  if (buffer == NULL || buffer_size < required) {
    if (buffer != NULL && buffer_size != 0) {
      buffer[0] = '\0';
    }
    return CPKT_OPCUA_ERR_RANGE;
  }
  memcpy(buffer, prefix, prefix_length);
  if (name_length != 0) {
    memcpy(buffer + prefix_length, name, name_length);
  }
  buffer[prefix_length + name_length] = '\0';
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_qualified_name_parse(
    const char *text,
    unsigned short *namespace_index_out,
    char *name_buffer,
    size_t name_buffer_size,
    size_t *name_length_out,
    size_t *required_size_out) {
  const char *cursor;
  const char *end;
  unsigned long parsed;
  size_t name_length;
  cpkt_opcua_result result;

  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (name_length_out != NULL) {
    *name_length_out = 0;
  }
  if (namespace_index_out != NULL) {
    *namespace_index_out = 0;
  }
  if (text == NULL || namespace_index_out == NULL || name_length_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  cursor = text;
  if (strncmp(cursor, "ns=", 3) != 0) {
    return CPKT_OPCUA_ERR_ARG;
  }
  cursor += 3;
  if (!cpkt_parse_unsigned_long(cursor, &end, &parsed) || parsed > (unsigned long)USHRT_MAX ||
      strncmp(end, ";q=", 3) != 0) {
    return CPKT_OPCUA_ERR_ARG;
  }
  cursor = end + 3;
  name_length = strlen(cursor);
  result = cpkt_copy_counted_string_to_buffer(
      cursor,
      name_length,
      name_buffer,
      name_buffer_size,
      required_size_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  *namespace_index_out = (unsigned short)parsed;
  *name_length_out = name_length;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_localized_text_print(
    const char *locale,
    size_t locale_length,
    const char *text,
    size_t text_length,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out) {
  static const char prefix[] = "locale=";
  static const char separator[] = ";text=";
  size_t prefix_length;
  size_t separator_length;
  size_t required;

  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if ((locale == NULL && locale_length != 0) || (text == NULL && text_length != 0)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  prefix_length = strlen(prefix);
  separator_length = strlen(separator);
  required = prefix_length + locale_length + separator_length + text_length + 1;
  if (required_size_out != NULL) {
    *required_size_out = required;
  }
  if (buffer == NULL || buffer_size < required) {
    if (buffer != NULL && buffer_size != 0) {
      buffer[0] = '\0';
    }
    return CPKT_OPCUA_ERR_RANGE;
  }
  memcpy(buffer, prefix, prefix_length);
  if (locale_length != 0) {
    memcpy(buffer + prefix_length, locale, locale_length);
  }
  memcpy(buffer + prefix_length + locale_length, separator, separator_length);
  if (text_length != 0) {
    memcpy(buffer + prefix_length + locale_length + separator_length, text, text_length);
  }
  buffer[required - 1] = '\0';
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_localized_text_parse(
    const char *input,
    char *buffer,
    size_t buffer_size,
    const char **locale_out,
    size_t *locale_length_out,
    const char **text_out,
    size_t *text_length_out,
    size_t *required_size_out) {
  static const char prefix[] = "locale=";
  static const char separator[] = ";text=";
  const char *separator_at;
  const char *text;
  size_t prefix_length;
  size_t separator_length;
  size_t locale_length;
  size_t text_length;
  size_t required;

  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (locale_length_out != NULL) {
    *locale_length_out = 0;
  }
  if (text_length_out != NULL) {
    *text_length_out = 0;
  }
  if (locale_out != NULL) {
    *locale_out = NULL;
  }
  if (text_out != NULL) {
    *text_out = NULL;
  }
  prefix_length = strlen(prefix);
  separator_length = strlen(separator);
  if (input == NULL || locale_out == NULL || locale_length_out == NULL ||
      text_out == NULL || text_length_out == NULL ||
      strncmp(input, prefix, prefix_length) != 0) {
    return CPKT_OPCUA_ERR_ARG;
  }
  separator_at = strstr(input + prefix_length, separator);
  if (separator_at == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  locale_length = (size_t)(separator_at - (input + prefix_length));
  text = separator_at + separator_length;
  text_length = strlen(text);
  required = locale_length + 1 + text_length + 1;
  if (required_size_out != NULL) {
    *required_size_out = required;
  }
  if (buffer == NULL || buffer_size < required) {
    if (buffer != NULL && buffer_size != 0) {
      buffer[0] = '\0';
    }
    return CPKT_OPCUA_ERR_RANGE;
  }
  if (locale_length != 0) {
    memcpy(buffer, input + prefix_length, locale_length);
  }
  buffer[locale_length] = '\0';
  if (text_length != 0) {
    memcpy(buffer + locale_length + 1, text, text_length);
  }
  buffer[locale_length + 1 + text_length] = '\0';
  *locale_out = buffer;
  *locale_length_out = locale_length;
  *text_out = buffer + locale_length + 1;
  *text_length_out = text_length;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_clear(cpkt_opcua_value *value) {
  if (value == NULL) {
    return;
  }
  value->type = CPKT_OPCUA_VALUE_EMPTY;
  value->boolean_value = 0;
  value->integer_value = 0;
  value->double_value = 0.0;
  value->string_value = NULL;
  value->string_length = 0;
  value->bytes_value = NULL;
  value->bytes_length = 0;
  value->boolean_array_values = NULL;
  value->boolean_array_length = 0;
  value->integer_array_values = NULL;
  value->integer_array_length = 0;
  value->double_array_values = NULL;
  value->double_array_length = 0;
  value->string_array_values = NULL;
  value->string_array_length = 0;
  value->byte_string_array_values = NULL;
  value->byte_string_array_length = 0;
  value->uint64_array_values = NULL;
  value->uint64_array_length = 0;
  value->datetime_array_values = NULL;
  value->datetime_array_length = 0;
  value->status_array_values = NULL;
  value->status_array_length = 0;
  value->guid_array_values = NULL;
  value->guid_array_length = 0;
  value->qualified_name_array_values = NULL;
  value->qualified_name_array_length = 0;
  value->localized_text_array_values = NULL;
  value->localized_text_array_length = 0;
  memset(value->guid_value, 0, sizeof(value->guid_value));
  value->status_value = 0;
  value->qualified_name_namespace_index = 0;
  value->qualified_name = NULL;
  value->qualified_name_length = 0;
  value->localized_text_locale = NULL;
  value->localized_text_locale_length = 0;
  value->localized_text = NULL;
  value->localized_text_length = 0;
  value->uint64_value.high32 = 0;
  value->uint64_value.low32 = 0;
  value->datetime_value.high32 = 0;
  value->datetime_value.low32 = 0;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_boolean(cpkt_opcua_value *value, int boolean_value) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_BOOLEAN;
    value->boolean_value = boolean_value ? 1 : 0;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_integer(cpkt_opcua_value *value, long integer_value) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_INTEGER;
    value->integer_value = integer_value;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_double(cpkt_opcua_value *value, double double_value) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_DOUBLE;
    value->double_value = double_value;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_string(
    cpkt_opcua_value *value,
    const char *string_value,
    size_t string_length) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_STRING;
    value->string_value = string_value;
    value->string_length = string_length;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_byte_string(
    cpkt_opcua_value *value,
    const unsigned char *bytes_value,
    size_t bytes_length) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_BYTE_STRING;
    value->bytes_value = bytes_value;
    value->bytes_length = bytes_length;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_guid(
    cpkt_opcua_value *value,
    const unsigned char guid[16]) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_GUID;
    if (guid != NULL) {
      memcpy(value->guid_value, guid, sizeof(value->guid_value));
    }
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_status(
    cpkt_opcua_value *value,
    cpkt_opcua_status status_value) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_STATUS;
    value->status_value = status_value;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_qualified_name(
    cpkt_opcua_value *value,
    unsigned short namespace_index,
    const char *name,
    size_t name_length) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_QUALIFIED_NAME;
    value->qualified_name_namespace_index = namespace_index;
    value->qualified_name = name;
    value->qualified_name_length = name_length;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_localized_text(
    cpkt_opcua_value *value,
    const char *locale,
    size_t locale_length,
    const char *text,
    size_t text_length) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_LOCALIZED_TEXT;
    value->localized_text_locale = locale;
    value->localized_text_locale_length = locale_length;
    value->localized_text = text;
    value->localized_text_length = text_length;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_uint64(
    cpkt_opcua_value *value,
    unsigned long high32,
    unsigned long low32) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_UINT64;
    value->uint64_value.high32 = high32;
    value->uint64_value.low32 = low32;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_datetime(
    cpkt_opcua_value *value,
    long high32,
    unsigned long low32) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_DATETIME;
    value->datetime_value.high32 = high32;
    value->datetime_value.low32 = low32;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_uint64_array(
    cpkt_opcua_value *value,
    const cpkt_opcua_uint64 *values,
    size_t value_count) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_UINT64_ARRAY;
    value->uint64_array_values = values;
    value->uint64_array_length = value_count;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_datetime_array(
    cpkt_opcua_value *value,
    const cpkt_opcua_datetime *values,
    size_t value_count) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_DATETIME_ARRAY;
    value->datetime_array_values = values;
    value->datetime_array_length = value_count;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_status_array(
    cpkt_opcua_value *value,
    const cpkt_opcua_status *values,
    size_t value_count) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_STATUS_ARRAY;
    value->status_array_values = values;
    value->status_array_length = value_count;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_guid_array(
    cpkt_opcua_value *value,
    const cpkt_opcua_guid *values,
    size_t value_count) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_GUID_ARRAY;
    value->guid_array_values = values;
    value->guid_array_length = value_count;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_qualified_name_array(
    cpkt_opcua_value *value,
    const cpkt_opcua_qualified_name_view *values,
    size_t value_count) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_QUALIFIED_NAME_ARRAY;
    value->qualified_name_array_values = values;
    value->qualified_name_array_length = value_count;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_localized_text_array(
    cpkt_opcua_value *value,
    const cpkt_opcua_localized_text_view *values,
    size_t value_count) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_LOCALIZED_TEXT_ARRAY;
    value->localized_text_array_values = values;
    value->localized_text_array_length = value_count;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_boolean_array(
    cpkt_opcua_value *value,
    const int *values,
    size_t value_count) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_BOOLEAN_ARRAY;
    value->boolean_array_values = values;
    value->boolean_array_length = value_count;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_integer_array(
    cpkt_opcua_value *value,
    const long *values,
    size_t value_count) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_INTEGER_ARRAY;
    value->integer_array_values = values;
    value->integer_array_length = value_count;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_double_array(
    cpkt_opcua_value *value,
    const double *values,
    size_t value_count) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_DOUBLE_ARRAY;
    value->double_array_values = values;
    value->double_array_length = value_count;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_string_array(
    cpkt_opcua_value *value,
    const cpkt_opcua_string_view *values,
    size_t value_count) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_STRING_ARRAY;
    value->string_array_values = values;
    value->string_array_length = value_count;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_byte_string_array(
    cpkt_opcua_value *value,
    const cpkt_opcua_byte_string_view *values,
    size_t value_count) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_BYTE_STRING_ARRAY;
    value->byte_string_array_values = values;
    value->byte_string_array_length = value_count;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_data_value_clear(cpkt_opcua_data_value *data_value) {
  if (data_value == NULL) {
    return;
  }
  data_value->has_value = 0;
  cpkt_opcua_value_clear(&data_value->value);
  data_value->has_status = 0;
  data_value->status = 0;
  data_value->has_source_timestamp = 0;
  data_value->source_timestamp.high32 = 0;
  data_value->source_timestamp.low32 = 0;
  data_value->has_server_timestamp = 0;
  data_value->server_timestamp.high32 = 0;
  data_value->server_timestamp.low32 = 0;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_browse_options_default(cpkt_opcua_browse_options *options) {
  if (options == NULL) {
    return;
  }
  options->browse_direction = CPKT_OPCUA_BROWSE_FORWARD;
  options->include_subtypes = 1;
  options->has_reference_type = 1;
  options->reference_type_id = cpkt_opcua_node_id_numeric(0, UA_NS0ID_HIERARCHICALREFERENCES);
  options->node_class_mask = 0;
  options->result_mask = CPKT_OPCUA_BROWSE_RESULT_ALL;
  options->max_references = 0;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_monitor_options_default(cpkt_opcua_monitor_options *options) {
  if (options == NULL) {
    return;
  }
  options->sampling_interval_ms = 0.0;
  options->queue_size = 0;
  options->discard_oldest = 1;
  options->deadband_type = CPKT_OPCUA_DEADBAND_NONE;
  options->deadband_value = 0.0;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_mqtt_connection_options_default(cpkt_opcua_mqtt_connection_options *options) {
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

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_new(
    cpkt_opcua_server **out,
    unsigned short port) {
  cpkt_opcua_server *server;
  UA_StatusCode status;

  if (out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  *out = NULL;
  server = (cpkt_opcua_server *)calloc(1, sizeof(*server));
  if (server == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  server->server = UA_Server_new();
  if (server->server == NULL) {
    free(server);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  status = UA_ServerConfig_setMinimal(UA_Server_getConfig(server->server), (UA_UInt16)port, NULL);
  if (status != UA_STATUSCODE_GOOD) {
    UA_Server_delete(server->server);
    free(server);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  server->port = port;
  *out = server;
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_opcua_server_wrap_native(
    cpkt_opcua_server **out,
    UA_Server *native_server) {
  cpkt_opcua_server *server;

  if (out == NULL) {
    if (native_server != NULL) {
      UA_Server_delete(native_server);
    }
    return CPKT_OPCUA_ERR_ARG;
  }
  *out = NULL;
  if (native_server == NULL) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  server = (cpkt_opcua_server *)calloc(1, sizeof(*server));
  if (server == NULL) {
    UA_Server_delete(native_server);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  server->server = native_server;
  *out = server;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_new_from_json(
    cpkt_opcua_server **out,
    const unsigned char *json,
    size_t json_length,
    cpkt_opcua_status *status_out) {
  UA_ByteString json_config;
  UA_Server *native_server;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  *out = NULL;
  if (json == NULL || json_length == 0) {
    return CPKT_OPCUA_ERR_ARG;
  }
  json_config.length = json_length;
  json_config.data = (UA_Byte *)(const void *)json;
  native_server = UA_Server_newFromFile(json_config);
  if (native_server == NULL) {
    if (status_out != NULL) {
      *status_out = cpkt_status(UA_STATUSCODE_BADCONFIGURATIONERROR);
    }
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_opcua_server_wrap_native(out, native_server);
  if (result != CPKT_OPCUA_OK && status_out != NULL && result == CPKT_OPCUA_ERR_ALLOC) {
    *status_out = cpkt_status(UA_STATUSCODE_BADOUTOFMEMORY);
  }
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_new_from_json_file(
    cpkt_opcua_server **out,
    const char *path,
    cpkt_opcua_status *status_out) {
  UA_ByteString json_config;
  cpkt_opcua_result result;

  if (out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  *out = NULL;
  result = cpkt_read_file_bytes(path, &json_config, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_opcua_server_new_from_json(out, json_config.data, json_config.length, status_out);
  cpkt_byte_string_clear_malloc(&json_config);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_server_free(cpkt_opcua_server *server) {
  struct cpkt_opcua_method_context *method;
  struct cpkt_opcua_method_context *next;

  if (server == NULL) {
    return;
  }
  if (server->server != NULL) {
    if (server->started) {
      (void)UA_Server_run_shutdown(server->server);
    }
    UA_Server_delete(server->server);
  }
  method = server->methods;
  while (method != NULL) {
    next = method->next;
    free(method->input_types);
    free(method->output_types);
    free(method);
    method = next;
  }
  free(server->access_username);
  free(server->access_password);
  free(server->endpoint_hostname);
  free(server);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_startup(
    cpkt_opcua_server *server,
    cpkt_opcua_status *status_out) {
  UA_StatusCode status;
  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (server->started) {
    return CPKT_OPCUA_OK;
  }
  status = UA_Server_run_startup(server->server);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  server->started = 1;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_iterate(
    cpkt_opcua_server *server,
    int wait_internal,
    unsigned short *wait_ms_out) {
  UA_UInt16 wait_ms;
  if (server == NULL || server->server == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  wait_ms = UA_Server_run_iterate(server->server, wait_internal ? true : false);
  if (wait_ms_out != NULL) {
    *wait_ms_out = (unsigned short)wait_ms;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_shutdown(
    cpkt_opcua_server *server,
    cpkt_opcua_status *status_out) {
  UA_StatusCode status;
  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!server->started) {
    return CPKT_OPCUA_OK;
  }
  status = UA_Server_run_shutdown(server->server);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  server->started = 0;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_endpoint_url(
    const cpkt_opcua_server *server,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out) {
  char port_text[6];
  const char *hostname;
  int written;
  size_t required;

  if (server == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  hostname = server->endpoint_hostname != NULL ? server->endpoint_hostname : "127.0.0.1";
  written = snprintf(port_text, sizeof(port_text), "%u", (unsigned)server->port);
  if (written < 0 || (size_t)written >= sizeof(port_text)) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  required = strlen("opc.tcp://") + strlen(hostname) + 1 + strlen(port_text) + 1;
  if (required_size_out != NULL) {
    *required_size_out = required;
  }
  if (buffer == NULL || buffer_size < required) {
    if (buffer != NULL && buffer_size != 0) {
      buffer[0] = '\0';
    }
    return CPKT_OPCUA_ERR_RANGE;
  }
  written = snprintf(buffer, buffer_size, "opc.tcp://%s:%s", hostname, port_text);
  if (written < 0 || (size_t)written >= buffer_size) {
    buffer[0] = '\0';
    return CPKT_OPCUA_ERR_RANGE;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_set_endpoint(
    cpkt_opcua_server *server,
    const char *hostname,
    unsigned short port) {
  UA_ServerConfig *config;
  UA_String *server_urls;
  UA_String server_url;
  char *hostname_copy;
  char *endpoint;
  size_t hostname_length;
  size_t endpoint_length;
  int written;

  if (server == NULL || server->server == NULL || server->started ||
      hostname == NULL || hostname[0] == '\0') {
    return CPKT_OPCUA_ERR_ARG;
  }
  hostname_length = strlen(hostname);
  endpoint_length = strlen("opc.tcp://") + hostname_length + strlen(":65535") + 1;
  endpoint = (char *)calloc(endpoint_length, 1);
  if (endpoint == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  written = snprintf(endpoint, endpoint_length, "opc.tcp://%s:%u", hostname, (unsigned)port);
  if (written < 0 || (size_t)written >= endpoint_length) {
    free(endpoint);
    return CPKT_OPCUA_ERR_RANGE;
  }
  hostname_copy = cpkt_strdup_c89(hostname);
  if (hostname_copy == NULL) {
    free(endpoint);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  server_url = UA_STRING_ALLOC(endpoint);
  free(endpoint);
  if (server_url.data == NULL) {
    free(hostname_copy);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  server_urls = (UA_String *)UA_Array_new(1, &UA_TYPES[UA_TYPES_STRING]);
  if (server_urls == NULL) {
    UA_String_clear(&server_url);
    free(hostname_copy);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  server_urls[0] = server_url;
  config = UA_Server_getConfig(server->server);
  UA_Array_delete(config->serverUrls, config->serverUrlsSize, &UA_TYPES[UA_TYPES_STRING]);
  config->serverUrls = server_urls;
  config->serverUrlsSize = 1;
  free(server->endpoint_hostname);
  server->endpoint_hostname = hostname_copy;
  server->port = port;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_set_application_identity(
    cpkt_opcua_server *server,
    const char *application_uri,
    const char *product_uri,
    const char *application_name) {
  UA_ServerConfig *config;
  UA_String native_application_uri;
  UA_String native_product_uri;
  UA_LocalizedText native_application_name;

  if (server == NULL || server->server == NULL || server->started ||
      application_uri == NULL || product_uri == NULL || application_name == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_application_uri = UA_STRING_ALLOC((char *)application_uri);
  native_product_uri = UA_STRING_ALLOC((char *)product_uri);
  native_application_name = UA_LOCALIZEDTEXT_ALLOC((char *)"en-US", (char *)application_name);
  if ((application_uri[0] != '\0' && native_application_uri.data == NULL) ||
      (product_uri[0] != '\0' && native_product_uri.data == NULL) ||
      (application_name[0] != '\0' && native_application_name.text.data == NULL)) {
    UA_String_clear(&native_application_uri);
    UA_String_clear(&native_product_uri);
    UA_LocalizedText_clear(&native_application_name);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  config = UA_Server_getConfig(server->server);
  UA_String_clear(&config->applicationDescription.applicationUri);
  UA_String_clear(&config->applicationDescription.productUri);
  UA_LocalizedText_clear(&config->applicationDescription.applicationName);
  config->applicationDescription.applicationUri = native_application_uri;
  config->applicationDescription.productUri = native_product_uri;
  config->applicationDescription.applicationName = native_application_name;
  config->applicationDescription.applicationType = UA_APPLICATIONTYPE_SERVER;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_native_config(
    cpkt_opcua_server *server,
    cpkt_opcua_server_native_config_fn fn,
    void *user,
    cpkt_opcua_status *status_out) {
  cpkt_opcua_status callback_status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || server->started || fn == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  callback_status = fn(UA_Server_getConfig(server->server), user);
  if (status_out != NULL) {
    *status_out = callback_status;
  }
  return callback_status == 0 ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_file_config_native_config(
    cpkt_opcua_server *server,
    cpkt_opcua_server_native_config_fn fn,
    void *user,
    cpkt_opcua_status *status_out) {
  return cpkt_opcua_server_native_config(server, fn, user, status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_security_plugin_native_config(
    cpkt_opcua_server *server,
    cpkt_opcua_server_native_config_fn fn,
    void *user,
    cpkt_opcua_status *status_out) {
  return cpkt_opcua_server_native_config(server, fn, user, status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_set_default_security(
    cpkt_opcua_server *server,
    int secure_only,
    const unsigned char *certificate,
    size_t certificate_length,
    const unsigned char *private_key,
    size_t private_key_length,
    const cpkt_opcua_byte_string_view *trust_list,
    size_t trust_list_count,
    const cpkt_opcua_byte_string_view *issuer_list,
    size_t issuer_list_count,
    const cpkt_opcua_byte_string_view *revocation_list,
    size_t revocation_list_count,
    cpkt_opcua_status *status_out) {
  UA_StatusCode status;
#ifdef UA_ENABLE_ENCRYPTION
  UA_ByteString native_certificate;
  UA_ByteString native_private_key;
  UA_ByteString *native_trust_list;
  UA_ByteString *native_issuer_list;
  UA_ByteString *native_revocation_list;
  cpkt_opcua_result result;

  native_trust_list = NULL;
  native_issuer_list = NULL;
  native_revocation_list = NULL;
#endif

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || server->started ||
      certificate == NULL || certificate_length == 0 ||
      private_key == NULL || private_key_length == 0) {
    return CPKT_OPCUA_ERR_ARG;
  }
#ifndef UA_ENABLE_ENCRYPTION
  status = UA_STATUSCODE_BADNOTSUPPORTED;
#else
  native_certificate = cpkt_make_borrowed_byte_string(certificate, certificate_length);
  native_private_key = cpkt_make_borrowed_byte_string(private_key, private_key_length);
  result = cpkt_make_borrowed_byte_string_array(trust_list, trust_list_count, &native_trust_list);
  if (result == CPKT_OPCUA_OK) {
    result = cpkt_make_borrowed_byte_string_array(issuer_list, issuer_list_count, &native_issuer_list);
  }
  if (result == CPKT_OPCUA_OK) {
    result = cpkt_make_borrowed_byte_string_array(revocation_list, revocation_list_count, &native_revocation_list);
  }
  if (result != CPKT_OPCUA_OK) {
    free(native_trust_list);
    free(native_issuer_list);
    free(native_revocation_list);
    return result;
  }
  if (secure_only) {
    status = UA_ServerConfig_setDefaultWithSecureSecurityPolicies(
        UA_Server_getConfig(server->server),
        (UA_UInt16)server->port,
        &native_certificate,
        &native_private_key,
        native_trust_list,
        trust_list_count,
        native_issuer_list,
        issuer_list_count,
        native_revocation_list,
        revocation_list_count);
  } else {
    status = UA_ServerConfig_setDefaultWithSecurityPolicies(
        UA_Server_getConfig(server->server),
        (UA_UInt16)server->port,
        &native_certificate,
        &native_private_key,
        native_trust_list,
        trust_list_count,
        native_issuer_list,
        issuer_list_count,
        native_revocation_list,
        revocation_list_count);
  }
  free(native_trust_list);
  free(native_issuer_list);
  free(native_revocation_list);
#endif
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_set_access_control(
    cpkt_opcua_server *server,
    int allow_anonymous,
    const char *username,
    const char *password,
    cpkt_opcua_status *status_out) {
  UA_ServerConfig *config;
  UA_UsernamePasswordLogin login;
  char *username_copy;
  unsigned char *password_copy;
  size_t password_length;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || server->started ||
      ((username == NULL) != (password == NULL))) {
    return CPKT_OPCUA_ERR_ARG;
  }
  username_copy = NULL;
  password_copy = NULL;
  password_length = 0;
  if (username != NULL) {
    password_length = strlen(password);
    username_copy = cpkt_strdup_c89(username);
    password_copy = cpkt_memdup_c89((const unsigned char *)password, password_length);
    if (username_copy == NULL || (password_length != 0 && password_copy == NULL)) {
      free(username_copy);
      free(password_copy);
      return CPKT_OPCUA_ERR_ALLOC;
    }
  }

  config = UA_Server_getConfig(server->server);
  login.username = UA_STRING_NULL;
  login.password = UA_BYTESTRING_NULL;
  if (username_copy != NULL) {
    login.username = UA_STRING(username_copy);
    login.password.data = (UA_Byte *)password_copy;
    login.password.length = password_length;
  }
  config->allowNonePolicyPassword = username_copy != NULL ? true : false;
  status = UA_AccessControl_default(
      config,
      allow_anonymous ? true : false,
      NULL,
      username_copy != NULL ? 1 : 0,
      username_copy != NULL ? &login : NULL);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    free(username_copy);
    free(password_copy);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  free(server->access_username);
  free(server->access_password);
  server->access_username = username_copy;
  server->access_password = password_copy;
  server->access_login_fn = NULL;
  server->access_login_user = NULL;
  return CPKT_OPCUA_OK;
}

static UA_StatusCode cpkt_opcua_access_control_login_callback(
    const UA_String *userName,
    const UA_ByteString *password,
    size_t usernamePasswordLoginSize,
    const UA_UsernamePasswordLogin *usernamePasswordLogin,
    void **sessionContext,
    void *loginContext) {
  cpkt_opcua_server *server;
  const char *username_data;
  const unsigned char *password_data;
  size_t username_length;
  size_t password_length;

  (void)usernamePasswordLoginSize;
  (void)usernamePasswordLogin;
  if (sessionContext != NULL) {
    *sessionContext = NULL;
  }
  server = (cpkt_opcua_server *)loginContext;
  if (server == NULL || server->access_login_fn == NULL) {
    return UA_STATUSCODE_BADUSERACCESSDENIED;
  }
  username_data = NULL;
  username_length = 0;
  if (userName != NULL) {
    username_data = (const char *)userName->data;
    username_length = userName->length;
  }
  password_data = NULL;
  password_length = 0;
  if (password != NULL) {
    password_data = (const unsigned char *)password->data;
    password_length = password->length;
  }
  return (UA_StatusCode)server->access_login_fn(
      username_data,
      username_length,
      password_data,
      password_length,
      server->access_login_user);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_set_access_control_callback(
    cpkt_opcua_server *server,
    int allow_anonymous,
    cpkt_opcua_login_fn fn,
    void *user,
    cpkt_opcua_status *status_out) {
  UA_ServerConfig *config;
  UA_UsernamePasswordLogin login;
  UA_StatusCode status;
  char callback_username[] = "cpkt-callback";
  unsigned char callback_password[] = "cpkt-callback";

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || server->started || fn == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }

  login.username = UA_STRING(callback_username);
  login.password.data = (UA_Byte *)callback_password;
  login.password.length = strlen((const char *)callback_password);
  config = UA_Server_getConfig(server->server);
  config->allowNonePolicyPassword = true;
  status = UA_AccessControl_defaultWithLoginCallback(
      config,
      allow_anonymous ? true : false,
      NULL,
      1,
      &login,
      cpkt_opcua_access_control_login_callback,
      server);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  free(server->access_username);
  free(server->access_password);
  server->access_username = NULL;
  server->access_password = NULL;
  server->access_login_fn = fn;
  server->access_login_user = user;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_variable(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *browse_name,
    const char *display_name,
    const cpkt_opcua_value *value,
    cpkt_opcua_status *status_out) {
  return cpkt_opcua_server_add_variable_under(
      server,
      node_id,
      cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
      browse_name,
      display_name,
      value,
      status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_object(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    cpkt_opcua_status *status_out) {
  UA_ObjectAttributes attr;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || browse_name == NULL ||
      !cpkt_valid_node_id(node_id) || !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  attr = UA_ObjectAttributes_default;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)(display_name != NULL ? display_name : browse_name));
  attr.writeMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION | UA_WRITEMASK_EVENTNOTIFIER;
  attr.userWriteMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION | UA_WRITEMASK_EVENTNOTIFIER;
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  status = UA_Server_addObjectNode(
      server->server,
      requested_node_id,
      parent_native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
      attr,
      NULL,
      NULL);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_variable_under(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    const cpkt_opcua_value *value,
    cpkt_opcua_status *status_out) {
  UA_VariableAttributes attr;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || browse_name == NULL || value == NULL ||
      !cpkt_valid_node_id(node_id) || !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.writeMask = UA_WRITEMASK_ACCESSLEVEL | UA_WRITEMASK_HISTORIZING | UA_WRITEMASK_DISPLAYNAME |
                   UA_WRITEMASK_DESCRIPTION | UA_WRITEMASK_VALUERANK | UA_WRITEMASK_ARRAYDIMENSIONS;
  attr.userWriteMask = UA_WRITEMASK_ACCESSLEVEL | UA_WRITEMASK_HISTORIZING | UA_WRITEMASK_DISPLAYNAME |
                       UA_WRITEMASK_DESCRIPTION | UA_WRITEMASK_VALUERANK | UA_WRITEMASK_ARRAYDIMENSIONS;
  attr.dataType = cpkt_data_type_node_id_for_value_type(value->type);
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)(display_name != NULL ? display_name : browse_name));
  result = cpkt_apply_value_shape_to_variable_attributes(&attr, value);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_set_variant(&attr.value, value);
  if (result != CPKT_OPCUA_OK) {
    UA_Array_delete(attr.arrayDimensions, attr.arrayDimensionsSize, &UA_TYPES[UA_TYPES_UINT32]);
    return result;
  }
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  status = UA_Server_addVariableNode(
      server->server,
      requested_node_id,
      parent_native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  UA_Variant_clear(&attr.value);
  UA_Array_delete(attr.arrayDimensions, attr.arrayDimensionsSize, &UA_TYPES[UA_TYPES_UINT32]);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_object_type(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    int is_abstract,
    cpkt_opcua_status *status_out) {
  UA_ObjectTypeAttributes attr;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || browse_name == NULL ||
      !cpkt_valid_node_id(node_id) || !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  attr = UA_ObjectTypeAttributes_default;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)(display_name != NULL ? display_name : browse_name));
  attr.writeMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION | UA_WRITEMASK_ISABSTRACT;
  attr.userWriteMask = attr.writeMask;
  attr.isAbstract = is_abstract ? true : false;
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  status = UA_Server_addObjectTypeNode(
      server->server,
      requested_node_id,
      parent_native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      attr,
      NULL,
      NULL);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_variable_type(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    const cpkt_opcua_value *value,
    int is_abstract,
    cpkt_opcua_status *status_out) {
  UA_VariableTypeAttributes attr;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || browse_name == NULL || value == NULL ||
      !cpkt_valid_node_id(node_id) || !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  attr = UA_VariableTypeAttributes_default;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)(display_name != NULL ? display_name : browse_name));
  attr.writeMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION | UA_WRITEMASK_ISABSTRACT;
  attr.userWriteMask = attr.writeMask;
  attr.dataType = cpkt_data_type_node_id_for_value_type(value->type);
  attr.isAbstract = is_abstract ? true : false;
  result = cpkt_apply_value_shape_to_variable_type_attributes(&attr, value);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_set_variant(&attr.value, value);
  if (result != CPKT_OPCUA_OK) {
    UA_Array_delete(attr.arrayDimensions, attr.arrayDimensionsSize, &UA_TYPES[UA_TYPES_UINT32]);
    return result;
  }
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  status = UA_Server_addVariableTypeNode(
      server->server,
      requested_node_id,
      parent_native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL,
      NULL);
  UA_Variant_clear(&attr.value);
  UA_Array_delete(attr.arrayDimensions, attr.arrayDimensionsSize, &UA_TYPES[UA_TYPES_UINT32]);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_reference_type(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    const char *inverse_name,
    int is_abstract,
    int symmetric,
    cpkt_opcua_status *status_out) {
  UA_ReferenceTypeAttributes attr;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || browse_name == NULL || inverse_name == NULL ||
      !cpkt_valid_node_id(node_id) || !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  attr = UA_ReferenceTypeAttributes_default;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)(display_name != NULL ? display_name : browse_name));
  attr.inverseName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)inverse_name);
  attr.writeMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION | UA_WRITEMASK_INVERSENAME |
                   UA_WRITEMASK_ISABSTRACT;
  attr.userWriteMask = attr.writeMask;
  attr.isAbstract = is_abstract ? true : false;
  attr.symmetric = symmetric ? true : false;
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  status = UA_Server_addReferenceTypeNode(
      server->server,
      requested_node_id,
      parent_native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      attr,
      NULL,
      NULL);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_data_type(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    int is_abstract,
    cpkt_opcua_status *status_out) {
  UA_DataTypeAttributes attr;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || browse_name == NULL ||
      !cpkt_valid_node_id(node_id) || !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  attr = UA_DataTypeAttributes_default;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)(display_name != NULL ? display_name : browse_name));
  attr.writeMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION | UA_WRITEMASK_ISABSTRACT;
  attr.userWriteMask = attr.writeMask;
  attr.isAbstract = is_abstract ? true : false;
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  status = UA_Server_addDataTypeNode(
      server->server,
      requested_node_id,
      parent_native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      attr,
      NULL,
      NULL);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_view(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    int contains_no_loops,
    unsigned long event_notifier,
    cpkt_opcua_status *status_out) {
  UA_ViewAttributes attr;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || browse_name == NULL ||
      !cpkt_valid_node_id(node_id) || !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (event_notifier > (unsigned long)UCHAR_MAX) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  attr = UA_ViewAttributes_default;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)(display_name != NULL ? display_name : browse_name));
  attr.writeMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION | UA_WRITEMASK_EVENTNOTIFIER;
  attr.userWriteMask = attr.writeMask;
  attr.containsNoLoops = contains_no_loops ? true : false;
  attr.eventNotifier = (UA_Byte)event_notifier;
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  status = UA_Server_addViewNode(
      server->server,
      requested_node_id,
      parent_native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      attr,
      NULL,
      NULL);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_method(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    const int *input_types,
    size_t input_count,
    int output_type,
    cpkt_opcua_method_fn fn,
    void *user,
    cpkt_opcua_status *status_out) {
  struct cpkt_opcua_method_context *context;
  UA_MethodAttributes attr;
  UA_Argument *input_arguments;
  UA_Argument *output_arguments;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_StatusCode status;
  size_t i;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || browse_name == NULL || fn == NULL ||
      !cpkt_valid_node_id(node_id) || !cpkt_valid_node_id(parent_node_id) ||
      (input_count != 0 && input_types == NULL)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!cpkt_valid_method_value_type(output_type)) {
    return CPKT_OPCUA_ERR_TYPE;
  }
  for (i = 0; i < input_count; ++i) {
    if (!cpkt_valid_method_value_type(input_types[i])) {
      return CPKT_OPCUA_ERR_TYPE;
    }
  }
  context = (struct cpkt_opcua_method_context *)calloc(1, sizeof(*context));
  if (context == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->single_fn = fn;
  context->user = user;
  context->input_count = input_count;
  context->output_count = 1;
  if (input_count != 0) {
    context->input_types = (int *)calloc(input_count, sizeof(*context->input_types));
    if (context->input_types == NULL) {
      free(context);
      return CPKT_OPCUA_ERR_ALLOC;
    }
    for (i = 0; i < input_count; ++i) {
      context->input_types[i] = input_types[i];
    }
  }
  context->output_types = (int *)calloc(1, sizeof(*context->output_types));
  if (context->output_types == NULL) {
    free(context->input_types);
    free(context);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->output_types[0] = output_type;

  input_arguments = cpkt_make_method_arguments(input_types, input_count, "input");
  if (input_count != 0 && input_arguments == NULL) {
    free(context->output_types);
    free(context->input_types);
    free(context);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  output_arguments = cpkt_make_method_arguments(&output_type, 1, "output");
  if (output_arguments == NULL) {
    cpkt_clear_method_arguments(input_arguments, input_count);
    free(context->output_types);
    free(context->input_types);
    free(context);
    return CPKT_OPCUA_ERR_ALLOC;
  }

  attr = UA_MethodAttributes_default;
  attr.executable = true;
  attr.userExecutable = true;
  attr.writeMask = UA_WRITEMASK_EXECUTABLE | UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION;
  attr.userWriteMask = UA_WRITEMASK_EXECUTABLE | UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)(display_name != NULL ? display_name : browse_name));
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  status = UA_Server_addMethodNode(
      server->server,
      requested_node_id,
      parent_native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      attr,
      cpkt_method_callback,
      input_count,
      input_arguments,
      1,
      output_arguments,
      context,
      NULL);
  cpkt_clear_method_arguments(input_arguments, input_count);
  cpkt_clear_method_arguments(output_arguments, 1);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    free(context->output_types);
    free(context->input_types);
    free(context);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  context->next = server->methods;
  server->methods = context;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_method_many(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    const int *input_types,
    size_t input_count,
    const int *output_types,
    size_t output_count,
    cpkt_opcua_method_many_fn fn,
    void *user,
    cpkt_opcua_status *status_out) {
  struct cpkt_opcua_method_context *context;
  UA_MethodAttributes attr;
  UA_Argument *input_arguments;
  UA_Argument *output_arguments;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_StatusCode status;
  size_t i;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || browse_name == NULL || fn == NULL ||
      !cpkt_valid_node_id(node_id) || !cpkt_valid_node_id(parent_node_id) ||
      (input_count != 0 && input_types == NULL) || output_types == NULL || output_count == 0) {
    return CPKT_OPCUA_ERR_ARG;
  }
  for (i = 0; i < input_count; ++i) {
    if (!cpkt_valid_method_value_type(input_types[i])) {
      return CPKT_OPCUA_ERR_TYPE;
    }
  }
  for (i = 0; i < output_count; ++i) {
    if (!cpkt_valid_method_value_type(output_types[i])) {
      return CPKT_OPCUA_ERR_TYPE;
    }
  }
  context = (struct cpkt_opcua_method_context *)calloc(1, sizeof(*context));
  if (context == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->fn = fn;
  context->user = user;
  context->input_count = input_count;
  context->output_count = output_count;
  if (input_count != 0) {
    context->input_types = (int *)calloc(input_count, sizeof(*context->input_types));
    if (context->input_types == NULL) {
      free(context);
      return CPKT_OPCUA_ERR_ALLOC;
    }
    for (i = 0; i < input_count; ++i) {
      context->input_types[i] = input_types[i];
    }
  }
  context->output_types = (int *)calloc(output_count, sizeof(*context->output_types));
  if (context->output_types == NULL) {
    free(context->input_types);
    free(context);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  for (i = 0; i < output_count; ++i) {
    context->output_types[i] = output_types[i];
  }

  input_arguments = cpkt_make_method_arguments(input_types, input_count, "input");
  if (input_count != 0 && input_arguments == NULL) {
    free(context->output_types);
    free(context->input_types);
    free(context);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  output_arguments = cpkt_make_method_arguments(output_types, output_count, "output");
  if (output_arguments == NULL) {
    cpkt_clear_method_arguments(input_arguments, input_count);
    free(context->output_types);
    free(context->input_types);
    free(context);
    return CPKT_OPCUA_ERR_ALLOC;
  }

  attr = UA_MethodAttributes_default;
  attr.executable = true;
  attr.userExecutable = true;
  attr.writeMask = UA_WRITEMASK_EXECUTABLE | UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION;
  attr.userWriteMask = UA_WRITEMASK_EXECUTABLE | UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)(display_name != NULL ? display_name : browse_name));
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  status = UA_Server_addMethodNode(
      server->server,
      requested_node_id,
      parent_native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      attr,
      cpkt_method_callback,
      input_count,
      input_arguments,
      output_count,
      output_arguments,
      context,
      NULL);
  cpkt_clear_method_arguments(input_arguments, input_count);
  cpkt_clear_method_arguments(output_arguments, output_count);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    free(context->output_types);
    free(context->input_types);
    free(context);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  context->next = server->methods;
  server->methods = context;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_namespace(
    cpkt_opcua_server *server,
    const char *namespace_uri,
    unsigned short *namespace_index_out) {
  UA_UInt16 namespace_index;

  if (namespace_index_out != NULL) {
    *namespace_index_out = 0;
  }
  if (server == NULL || server->server == NULL || namespace_uri == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  namespace_index = UA_Server_addNamespace(server->server, namespace_uri);
  if (namespace_index_out != NULL) {
    *namespace_index_out = (unsigned short)namespace_index;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_delete_node(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int delete_target_refs,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Server_deleteNode(server->server, native_node_id, delete_target_refs ? true : false);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_reference(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id source_node_id,
    cpkt_opcua_node_id reference_type_id,
    int is_forward,
    cpkt_opcua_node_id target_node_id,
    unsigned long target_node_class,
    cpkt_opcua_status *status_out) {
  return cpkt_opcua_server_add_reference_ex(
      server,
      source_node_id,
      reference_type_id,
      is_forward,
      cpkt_opcua_expanded_node_id_local(target_node_id),
      target_node_class,
      status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_reference_ex(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id source_node_id,
    cpkt_opcua_node_id reference_type_id,
    int is_forward,
    cpkt_opcua_expanded_node_id target_node_id,
    unsigned long target_node_class,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_source_node_id;
  UA_NodeId native_reference_type_id;
  UA_ExpandedNodeId native_target_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL ||
      !cpkt_valid_node_id(source_node_id) || !cpkt_valid_node_id(reference_type_id) ||
      !cpkt_valid_expanded_node_id(target_node_id) || !cpkt_valid_node_class(target_node_class)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_source_node_id = cpkt_make_node_id(source_node_id);
  native_reference_type_id = cpkt_make_node_id(reference_type_id);
  native_target_node_id = cpkt_make_expanded_node_id(target_node_id);
  status = UA_Server_addReference(
      server->server,
      native_source_node_id,
      native_reference_type_id,
      native_target_node_id,
      is_forward ? true : false);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_delete_reference(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id source_node_id,
    cpkt_opcua_node_id reference_type_id,
    int is_forward,
    cpkt_opcua_node_id target_node_id,
    int delete_bidirectional,
    cpkt_opcua_status *status_out) {
  return cpkt_opcua_server_delete_reference_ex(
      server,
      source_node_id,
      reference_type_id,
      is_forward,
      cpkt_opcua_expanded_node_id_local(target_node_id),
      delete_bidirectional,
      status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_delete_reference_ex(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id source_node_id,
    cpkt_opcua_node_id reference_type_id,
    int is_forward,
    cpkt_opcua_expanded_node_id target_node_id,
    int delete_bidirectional,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_source_node_id;
  UA_NodeId native_reference_type_id;
  UA_ExpandedNodeId native_target_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL ||
      !cpkt_valid_node_id(source_node_id) || !cpkt_valid_node_id(reference_type_id) ||
      !cpkt_valid_expanded_node_id(target_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_source_node_id = cpkt_make_node_id(source_node_id);
  native_reference_type_id = cpkt_make_node_id(reference_type_id);
  native_target_node_id = cpkt_make_expanded_node_id(target_node_id);
  status = UA_Server_deleteReference(
      server->server,
      native_source_node_id,
      native_reference_type_id,
      is_forward ? true : false,
      native_target_node_id,
      delete_bidirectional ? true : false);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_browse_children(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id parent_node_id,
    cpkt_opcua_browse_fn fn,
    void *user,
    cpkt_opcua_status *status_out) {
  cpkt_opcua_browse_options options;

  cpkt_opcua_browse_options_default(&options);
  return cpkt_opcua_server_browse_children_ex(server, parent_node_id, &options, fn, user, status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_browse_children_ex(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id parent_node_id,
    const cpkt_opcua_browse_options *options,
    cpkt_opcua_browse_fn fn,
    void *user,
    cpkt_opcua_status *status_out) {
  UA_BrowseDescription browse_description;
  UA_BrowseResult browse_result;
  cpkt_opcua_result result;
  unsigned long max_references;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || fn == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_fill_browse_description(&browse_description, parent_node_id, options);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  max_references = options != NULL ? options->max_references : 0;
  browse_result = UA_Server_browse(server->server, (UA_UInt32)max_references, &browse_description);
  result = cpkt_browse_result_each(&browse_result, fn, user, status_out);
  UA_BrowseResult_clear(&browse_result);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_browse_children_page(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id parent_node_id,
    const cpkt_opcua_browse_options *options,
    cpkt_opcua_browse_fn fn,
    void *user,
    unsigned char *continuation_point_buffer,
    size_t continuation_point_buffer_size,
    size_t *required_continuation_point_size_out,
    cpkt_opcua_status *status_out) {
  UA_BrowseDescription browse_description;
  UA_BrowseResult browse_result;
  cpkt_opcua_result result;
  unsigned long max_references;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_continuation_point_size_out != NULL) {
    *required_continuation_point_size_out = 0;
  }
  if (server == NULL || server->server == NULL || fn == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_fill_browse_description(&browse_description, parent_node_id, options);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  max_references = options != NULL ? options->max_references : 0;
  browse_result = UA_Server_browse(server->server, (UA_UInt32)max_references, &browse_description);
  result = cpkt_browse_result_page(
      &browse_result,
      fn,
      user,
      continuation_point_buffer,
      continuation_point_buffer_size,
      required_continuation_point_size_out,
      status_out);
  if (result == CPKT_OPCUA_ERR_RANGE) {
    cpkt_server_release_browse_continuation_point(server->server, &browse_result.continuationPoint);
  }
  UA_BrowseResult_clear(&browse_result);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_browse_next(
    cpkt_opcua_server *server,
    const unsigned char *continuation_point,
    size_t continuation_point_size,
    int release_continuation_point,
    cpkt_opcua_browse_fn fn,
    void *user,
    unsigned char *next_continuation_point_buffer,
    size_t next_continuation_point_buffer_size,
    size_t *required_next_continuation_point_size_out,
    cpkt_opcua_status *status_out) {
  UA_ByteString native_continuation_point;
  UA_BrowseResult browse_result;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_next_continuation_point_size_out != NULL) {
    *required_next_continuation_point_size_out = 0;
  }
  if (server == NULL || server->server == NULL ||
      (continuation_point_size != 0 && continuation_point == NULL) ||
      (!release_continuation_point && fn == NULL)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_continuation_point.length = continuation_point_size;
  native_continuation_point.data = (UA_Byte *)continuation_point;
  browse_result = UA_Server_browseNext(
      server->server,
      release_continuation_point ? true : false,
      &native_continuation_point);
  result = cpkt_browse_result_page(
      &browse_result,
      release_continuation_point ? NULL : fn,
      user,
      next_continuation_point_buffer,
      next_continuation_point_buffer_size,
      required_next_continuation_point_size_out,
      status_out);
  if (result == CPKT_OPCUA_ERR_RANGE) {
    cpkt_server_release_browse_continuation_point(server->server, &browse_result.continuationPoint);
  }
  UA_BrowseResult_clear(&browse_result);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_translate_browse_path(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id start_node_id,
    const cpkt_opcua_browse_path_element *elements,
    size_t element_count,
    cpkt_opcua_node_id *target_node_id_out,
    char *target_buffer,
    size_t target_buffer_size,
    size_t *required_target_size_out,
    cpkt_opcua_status *status_out) {
  UA_BrowsePath browse_path;
  UA_BrowsePathResult browse_path_result;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || target_node_id_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_fill_browse_path(&browse_path, start_node_id, elements, element_count);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  browse_path_result = UA_Server_translateBrowsePathToNodeIds(server->server, &browse_path);
  result = cpkt_translate_browse_path_result_to_target(
      &browse_path_result,
      target_node_id_out,
      target_buffer,
      target_buffer_size,
      required_target_size_out,
      status_out);
  UA_BrowsePathResult_clear(&browse_path_result);
  UA_BrowsePath_clear(&browse_path);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_value *value_out,
    char *string_buffer,
    size_t string_buffer_size,
    size_t *required_string_size_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Variant variant;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || value_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_Variant_init(&variant);
  status = UA_Server_readValue(server->server, native_node_id, &variant);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_get_variant(&variant, value_out, string_buffer, string_buffer_size, required_string_size_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_native_variant(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_native_variant_fn fn,
    void *user,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Variant variant;
  UA_StatusCode status;
  int callback_status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || fn == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_Variant_init(&variant);
  status = UA_Server_readValue(server->server, native_node_id, &variant);
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
cpkt_opcua_result cpkt_opcua_server_read_native_data_value(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_native_data_value_fn fn,
    void *user,
    cpkt_opcua_status *status_out) {
  UA_ReadValueId read_value;
  UA_DataValue data_value;
  int callback_status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || fn == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  UA_ReadValueId_init(&read_value);
  read_value.nodeId = cpkt_make_node_id(node_id);
  read_value.attributeId = UA_ATTRIBUTEID_VALUE;
  data_value = UA_Server_read(server->server, &read_value, UA_TIMESTAMPSTORETURN_BOTH);
  if (status_out != NULL) {
    *status_out = cpkt_status(data_value.status);
  }
  callback_status = fn(&data_value, user);
  UA_DataValue_clear(&data_value);
  return callback_status == 0 ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_CALLBACK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_data_value(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_data_value *data_value_out,
    char *string_buffer,
    size_t string_buffer_size,
    size_t *required_string_size_out,
    cpkt_opcua_status *status_out) {
  UA_ReadValueId read_value;
  UA_DataValue data_value;
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
  if (server == NULL || server->server == NULL || data_value_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  UA_ReadValueId_init(&read_value);
  read_value.nodeId = cpkt_make_node_id(node_id);
  read_value.attributeId = UA_ATTRIBUTEID_VALUE;
  data_value = UA_Server_read(server->server, &read_value, UA_TIMESTAMPSTORETURN_BOTH);
  if (status_out != NULL) {
    *status_out = cpkt_status(data_value.status);
  }
  result = cpkt_get_data_value(
      &data_value,
      data_value_out,
      string_buffer,
      string_buffer_size,
      required_string_size_out);
  UA_DataValue_clear(&data_value);
  return result;
}

static cpkt_opcua_result cpkt_server_read_array_variant(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    UA_Variant *variant_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (variant_out != NULL) {
    UA_Variant_init(variant_out);
  }
  if (server == NULL || server->server == NULL || variant_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Server_readValue(server->server, native_node_id, variant_out);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_boolean_array(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result = cpkt_server_read_array_variant(server, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_boolean_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_integer_array(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    long *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result = cpkt_server_read_array_variant(server, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_integer_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_uint64_array(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_uint64 *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result = cpkt_server_read_array_variant(server, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_uint64_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_datetime_array(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_datetime *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result = cpkt_server_read_array_variant(server, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_datetime_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_status_array(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_status *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result = cpkt_server_read_array_variant(server, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_status_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_guid_array(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_guid *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result = cpkt_server_read_array_variant(server, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_guid_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_qualified_name_array(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_qualified_name_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result = cpkt_server_read_array_variant(server, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_each_qualified_name_range_from_variant(&variant, fn, user, value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_localized_text_array(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_localized_text_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result = cpkt_server_read_array_variant(server, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_each_localized_text_array_from_variant(&variant, fn, user, value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_double_array(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    double *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result = cpkt_server_read_array_variant(server, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_double_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_string_array(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_string_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result = cpkt_server_read_array_variant(server, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_each_string_array_from_variant(&variant, fn, user, value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_byte_string_array(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_byte_string_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result = cpkt_server_read_array_variant(server, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_each_byte_string_array_from_variant(&variant, fn, user, value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

static cpkt_opcua_result cpkt_server_read_index_range_variant(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    UA_Variant *variant_out,
    cpkt_opcua_status *status_out) {
  UA_ReadValueId read_value;
  UA_DataValue data_value;
  UA_StatusCode copy_status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (variant_out != NULL) {
    UA_Variant_init(variant_out);
  }
  if (server == NULL || server->server == NULL || index_range == NULL || variant_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  UA_ReadValueId_init(&read_value);
  read_value.nodeId = cpkt_make_node_id(node_id);
  read_value.attributeId = UA_ATTRIBUTEID_VALUE;
  read_value.indexRange = UA_STRING((char *)index_range);
  data_value = UA_Server_read(server->server, &read_value, UA_TIMESTAMPSTORETURN_NEITHER);
  if (status_out != NULL) {
    *status_out = cpkt_status(data_value.status);
  }
  if (data_value.status != UA_STATUSCODE_GOOD) {
    UA_DataValue_clear(&data_value);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  copy_status = UA_Variant_copy(&data_value.value, variant_out);
  UA_DataValue_clear(&data_value);
  return copy_status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_ALLOC;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_boolean_array_range(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    int *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_server_read_index_range_variant(server, node_id, index_range, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_boolean_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_integer_array_range(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    long *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_server_read_index_range_variant(server, node_id, index_range, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_integer_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_double_array_range(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    double *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_server_read_index_range_variant(server, node_id, index_range, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_double_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_uint64_array_range(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_uint64 *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_server_read_index_range_variant(server, node_id, index_range, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_uint64_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_datetime_array_range(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_datetime *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_server_read_index_range_variant(server, node_id, index_range, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_datetime_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_status_array_range(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_status *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_server_read_index_range_variant(server, node_id, index_range, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_status_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_guid_array_range(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_guid *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_server_read_index_range_variant(server, node_id, index_range, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_guid_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_qualified_name_array_range(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_qualified_name_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (value_count_out != NULL) {
    *value_count_out = 0;
  }
  if (value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_server_read_index_range_variant(server, node_id, index_range, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_each_qualified_name_range_from_variant(&variant, fn, user, value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_localized_text_array_range(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_localized_text_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (value_count_out != NULL) {
    *value_count_out = 0;
  }
  if (value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_server_read_index_range_variant(server, node_id, index_range, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_each_localized_text_range_from_variant(&variant, fn, user, value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_string_array_range(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_string_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out) {
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
  result = cpkt_server_read_index_range_variant(server, node_id, index_range, &variant, effective_status);
  if (result != CPKT_OPCUA_OK) {
    if (result != CPKT_OPCUA_ERR_UPSTREAM ||
        *effective_status != cpkt_status(UA_STATUSCODE_BADINDEXRANGEINVALID)) {
      return result;
    }
    if (!cpkt_parse_simple_array_index_range(index_range, &start, &count)) {
      return result;
    }
    result = cpkt_server_read_array_variant(server, node_id, &variant, effective_status);
    if (result != CPKT_OPCUA_OK) {
      return result;
    }
    result = cpkt_each_string_array_slice_from_variant(&variant, start, count, fn, user, value_count_out);
    if (result == CPKT_OPCUA_ERR_RANGE && status_out != NULL) {
      *status_out = cpkt_status(UA_STATUSCODE_BADINDEXRANGENODATA);
    }
    UA_Variant_clear(&variant);
    return result;
  }
  result = cpkt_each_string_range_from_variant(&variant, fn, user, value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_byte_string_array_range(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_byte_string_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out) {
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
  result = cpkt_server_read_index_range_variant(server, node_id, index_range, &variant, effective_status);
  if (result != CPKT_OPCUA_OK) {
    if (result != CPKT_OPCUA_ERR_UPSTREAM ||
        *effective_status != cpkt_status(UA_STATUSCODE_BADINDEXRANGEINVALID)) {
      return result;
    }
    if (!cpkt_parse_simple_array_index_range(index_range, &start, &count)) {
      return result;
    }
    result = cpkt_server_read_array_variant(server, node_id, &variant, effective_status);
    if (result != CPKT_OPCUA_OK) {
      return result;
    }
    result = cpkt_each_byte_string_array_slice_from_variant(&variant, start, count, fn, user, value_count_out);
    if (result == CPKT_OPCUA_ERR_RANGE && status_out != NULL) {
      *status_out = cpkt_status(UA_STATUSCODE_BADINDEXRANGENODATA);
    }
    UA_Variant_clear(&variant);
    return result;
  }
  result = cpkt_each_byte_string_range_from_variant(&variant, fn, user, value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_write_index_range(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    const cpkt_opcua_value *value,
    cpkt_opcua_status *status_out) {
  UA_WriteValue write_value;
  UA_Variant variant;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || index_range == NULL || value == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  UA_Variant_init(&variant);
  result = cpkt_set_variant(&variant, value);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  UA_WriteValue_init(&write_value);
  write_value.nodeId = cpkt_make_node_id(node_id);
  write_value.attributeId = UA_ATTRIBUTEID_VALUE;
  write_value.indexRange = UA_STRING((char *)index_range);
  write_value.value.hasValue = true;
  write_value.value.value = variant;
  status = UA_Server_write(server->server, &write_value);
  UA_Variant_clear(&variant);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_node_id(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id *node_id_out,
    char *identifier_buffer,
    size_t identifier_buffer_size,
    size_t *required_identifier_size_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_NodeId native_result;
  UA_StatusCode status;
  cpkt_opcua_node_id facade_result;
  struct cpkt_owned_node_id_memory owned;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_identifier_size_out != NULL) {
    *required_identifier_size_out = 0;
  }
  if (node_id_out != NULL) {
    *node_id_out = cpkt_opcua_node_id_null();
  }
  if (server == NULL || server->server == NULL || node_id_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_NodeId_init(&native_result);
  status = UA_Server_readNodeId(server->server, native_node_id, &native_result);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_NodeId_clear(&native_result);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  owned.string = NULL;
  owned.byte_string = NULL;
  if (!cpkt_native_node_id_to_facade(&native_result, &facade_result, &owned)) {
    cpkt_owned_node_id_memory_clear(&owned);
    UA_NodeId_clear(&native_result);
    return CPKT_OPCUA_ERR_TYPE;
  }
  result = cpkt_copy_node_id_to_caller(
      &facade_result,
      &owned,
      node_id_out,
      identifier_buffer,
      identifier_buffer_size,
      required_identifier_size_out);
  if (result != CPKT_OPCUA_OK) {
    *node_id_out = cpkt_opcua_node_id_null();
  }
  cpkt_owned_node_id_memory_clear(&owned);
  UA_NodeId_clear(&native_result);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_node_class(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned long *node_class_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_NodeClass node_class;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (node_class_out != NULL) {
    *node_class_out = 0;
  }
  if (server == NULL || server->server == NULL || node_class_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Server_readNodeClass(server->server, native_node_id, &node_class);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *node_class_out = (unsigned long)node_class;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_browse_name(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned short *namespace_index_out,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_QualifiedName browse_name;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (namespace_index_out != NULL) {
    *namespace_index_out = 0;
  }
  if (server == NULL || server->server == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_QualifiedName_init(&browse_name);
  status = UA_Server_readBrowseName(server->server, native_node_id, &browse_name);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_QualifiedName_clear(&browse_name);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (namespace_index_out != NULL) {
    *namespace_index_out = (unsigned short)browse_name.namespaceIndex;
  }
  result = cpkt_copy_ua_string_to_buffer(&browse_name.name, buffer, buffer_size, required_size_out);
  UA_QualifiedName_clear(&browse_name);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_display_name(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_LocalizedText display_name;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (server == NULL || server->server == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_LocalizedText_init(&display_name);
  status = UA_Server_readDisplayName(server->server, native_node_id, &display_name);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_LocalizedText_clear(&display_name);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_ua_string_to_buffer(&display_name.text, buffer, buffer_size, required_size_out);
  UA_LocalizedText_clear(&display_name);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_description(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_LocalizedText description;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (server == NULL || server->server == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_LocalizedText_init(&description);
  status = UA_Server_readDescription(server->server, native_node_id, &description);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_LocalizedText_clear(&description);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_ua_string_to_buffer(&description.text, buffer, buffer_size, required_size_out);
  UA_LocalizedText_clear(&description);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_write_display_name(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *display_name,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_LocalizedText native_display_name;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || display_name == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_display_name = UA_LOCALIZEDTEXT((char *)"en-US", (char *)display_name);
  status = UA_Server_writeDisplayName(server->server, native_node_id, native_display_name);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_write_description(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *description,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_LocalizedText native_description;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || description == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_description = UA_LOCALIZEDTEXT((char *)"en-US", (char *)description);
  status = UA_Server_writeDescription(server->server, native_node_id, native_description);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_write_mask(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned long *write_mask_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_UInt32 write_mask;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (write_mask_out != NULL) {
    *write_mask_out = 0;
  }
  if (server == NULL || server->server == NULL || write_mask_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Server_readWriteMask(server->server, native_node_id, &write_mask);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *write_mask_out = (unsigned long)write_mask;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_user_write_mask(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned long *write_mask_out,
    cpkt_opcua_status *status_out) {
  return cpkt_server_read_uint32_attribute(
      server,
      node_id,
      UA_ATTRIBUTEID_USERWRITEMASK,
      write_mask_out,
      status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_write_write_mask(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned long write_mask,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!cpkt_ulong_fits_uint32(write_mask)) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Server_writeWriteMask(server->server, native_node_id, (UA_UInt32)write_mask);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_is_abstract(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int *is_abstract_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean is_abstract;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (is_abstract_out != NULL) {
    *is_abstract_out = 0;
  }
  if (server == NULL || server->server == NULL || is_abstract_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Server_readIsAbstract(server->server, native_node_id, &is_abstract);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *is_abstract_out = is_abstract ? 1 : 0;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_write_is_abstract(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int is_abstract,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Server_writeIsAbstract(server->server, native_node_id, is_abstract ? true : false);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_symmetric(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int *symmetric_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean symmetric;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (symmetric_out != NULL) {
    *symmetric_out = 0;
  }
  if (server == NULL || server->server == NULL || symmetric_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Server_readSymmetric(server->server, native_node_id, &symmetric);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *symmetric_out = symmetric ? 1 : 0;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_write_symmetric(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int symmetric,
    cpkt_opcua_status *status_out) {
  return cpkt_server_write_boolean_attribute(
      server,
      node_id,
      UA_ATTRIBUTEID_SYMMETRIC,
      symmetric,
      status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_inverse_name(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_LocalizedText inverse_name;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (server == NULL || server->server == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_LocalizedText_init(&inverse_name);
  status = UA_Server_readInverseName(server->server, native_node_id, &inverse_name);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_LocalizedText_clear(&inverse_name);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_ua_string_to_buffer(&inverse_name.text, buffer, buffer_size, required_size_out);
  UA_LocalizedText_clear(&inverse_name);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_write_inverse_name(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const char *inverse_name,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_LocalizedText native_inverse_name;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || inverse_name == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_inverse_name = UA_LOCALIZEDTEXT((char *)"en-US", (char *)inverse_name);
  status = UA_Server_writeInverseName(server->server, native_node_id, native_inverse_name);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_contains_no_loops(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int *contains_no_loops_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean contains_no_loops;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (contains_no_loops_out != NULL) {
    *contains_no_loops_out = 0;
  }
  if (server == NULL || server->server == NULL || contains_no_loops_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Server_readContainsNoLoops(server->server, native_node_id, &contains_no_loops);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *contains_no_loops_out = contains_no_loops ? 1 : 0;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_write_contains_no_loops(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int contains_no_loops,
    cpkt_opcua_status *status_out) {
  return cpkt_server_write_boolean_attribute(
      server,
      node_id,
      UA_ATTRIBUTEID_CONTAINSNOLOOPS,
      contains_no_loops,
      status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_event_notifier(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned long *event_notifier_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Byte event_notifier;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (event_notifier_out != NULL) {
    *event_notifier_out = 0;
  }
  if (server == NULL || server->server == NULL || event_notifier_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Server_readEventNotifier(server->server, native_node_id, &event_notifier);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *event_notifier_out = (unsigned long)event_notifier;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_write_event_notifier(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned long event_notifier,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (event_notifier > (unsigned long)UCHAR_MAX) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Server_writeEventNotifier(server->server, native_node_id, (UA_Byte)event_notifier);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_data_type(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id *data_type_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_NodeId native_data_type;
  UA_StatusCode status;
  struct cpkt_owned_node_id_memory data_type_memory;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (data_type_out != NULL) {
    *data_type_out = cpkt_opcua_node_id_numeric(0, 0);
  }
  if (server == NULL || server->server == NULL || data_type_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_NodeId_init(&native_data_type);
  status = UA_Server_readDataType(server->server, native_node_id, &native_data_type);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_NodeId_clear(&native_data_type);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  data_type_memory.string = NULL;
  data_type_memory.byte_string = NULL;
  if (!cpkt_native_node_id_to_facade(&native_data_type, data_type_out, &data_type_memory)) {
    UA_NodeId_clear(&native_data_type);
    cpkt_owned_node_id_memory_clear(&data_type_memory);
    return CPKT_OPCUA_ERR_TYPE;
  }
  if (data_type_memory.string != NULL || data_type_memory.byte_string != NULL) {
    UA_NodeId_clear(&native_data_type);
    cpkt_owned_node_id_memory_clear(&data_type_memory);
    *data_type_out = cpkt_opcua_node_id_numeric(0, 0);
    return CPKT_OPCUA_ERR_TYPE;
  }
  cpkt_owned_node_id_memory_clear(&data_type_memory);
  UA_NodeId_clear(&native_data_type);
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_write_data_type(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id data_type,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_NodeId native_data_type;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || !cpkt_valid_node_id(node_id) ||
      !cpkt_valid_node_id(data_type)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_data_type = cpkt_make_node_id(data_type);
  status = UA_Server_writeDataType(server->server, native_node_id, native_data_type);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_value_rank(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    long *value_rank_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Int32 value_rank;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (value_rank_out != NULL) {
    *value_rank_out = 0;
  }
  if (server == NULL || server->server == NULL || value_rank_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Server_readValueRank(server->server, native_node_id, &value_rank);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *value_rank_out = (long)value_rank;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_write_value_rank(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    long value_rank,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!cpkt_long_fits_int32(value_rank)) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Server_writeValueRank(server->server, native_node_id, (UA_Int32)value_rank);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_array_dimensions(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned long *dimensions,
    size_t dimension_count,
    size_t *required_dimension_count_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Variant variant;
  UA_StatusCode status;
  cpkt_opcua_result result;
  size_t native_dimension_count;
  UA_UInt32 *native_dimensions;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_dimension_count_out != NULL) {
    *required_dimension_count_out = 0;
  }
  if (server == NULL || server->server == NULL || required_dimension_count_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_Variant_init(&variant);
  status = UA_Server_readArrayDimensions(server->server, native_node_id, &variant);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_Variant_clear(&variant);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (variant.data == NULL && variant.arrayLength == 0) {
    UA_Variant_clear(&variant);
    return CPKT_OPCUA_OK;
  }
  if (!UA_Variant_hasArrayType(&variant, &UA_TYPES[UA_TYPES_UINT32])) {
    UA_Variant_clear(&variant);
    return CPKT_OPCUA_ERR_TYPE;
  }
  native_dimension_count = variant.arrayLength;
  native_dimensions = (UA_UInt32 *)variant.data;
  result = cpkt_copy_array_dimensions_to_buffer(
      native_dimensions,
      native_dimension_count,
      dimensions,
      dimension_count,
      required_dimension_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_write_array_dimensions(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    const unsigned long *dimensions,
    size_t dimension_count,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_UInt32 *native_dimensions;
  UA_Variant variant;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_make_array_dimensions(dimensions, dimension_count, &native_dimensions);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_Variant_init(&variant);
  status = UA_Variant_setArrayCopy(
      &variant,
      native_dimensions,
      dimension_count,
      &UA_TYPES[UA_TYPES_UINT32]);
  free(native_dimensions);
  if (status != UA_STATUSCODE_GOOD) {
    UA_Variant_clear(&variant);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  status = UA_Server_writeArrayDimensions(server->server, native_node_id, variant);
  UA_Variant_clear(&variant);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_access_level(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned long *access_level_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Byte access_level;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (access_level_out != NULL) {
    *access_level_out = 0;
  }
  if (server == NULL || server->server == NULL || access_level_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Server_readAccessLevel(server->server, native_node_id, &access_level);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *access_level_out = (unsigned long)access_level;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_user_access_level(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned long *access_level_out,
    cpkt_opcua_status *status_out) {
  return cpkt_server_read_byte_attribute(
      server,
      node_id,
      UA_ATTRIBUTEID_USERACCESSLEVEL,
      access_level_out,
      status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_write_access_level(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned long access_level,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Byte native_access_level;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (access_level > (unsigned long)UCHAR_MAX) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_access_level = (UA_Byte)access_level;
  status = UA_Server_writeAccessLevel(server->server, native_node_id, native_access_level);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_access_level_ex(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned long *access_level_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_UInt32 access_level;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (access_level_out != NULL) {
    *access_level_out = 0;
  }
  if (server == NULL || server->server == NULL || access_level_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Server_readAccessLevelEx(server->server, native_node_id, &access_level);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *access_level_out = (unsigned long)access_level;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_write_access_level_ex(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    unsigned long access_level,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!cpkt_ulong_fits_uint32(access_level)) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Server_writeAccessLevelEx(server->server, native_node_id, (UA_UInt32)access_level);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_minimum_sampling_interval(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    double *minimum_sampling_interval_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Double minimum_sampling_interval;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (minimum_sampling_interval_out != NULL) {
    *minimum_sampling_interval_out = 0.0;
  }
  if (server == NULL || server->server == NULL || minimum_sampling_interval_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Server_readMinimumSamplingInterval(
      server->server,
      native_node_id,
      &minimum_sampling_interval);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *minimum_sampling_interval_out = (double)minimum_sampling_interval;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_write_minimum_sampling_interval(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    double minimum_sampling_interval,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Server_writeMinimumSamplingInterval(
      server->server,
      native_node_id,
      (UA_Double)minimum_sampling_interval);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_historizing(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int *historizing_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean historizing;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (historizing_out != NULL) {
    *historizing_out = 0;
  }
  if (server == NULL || server->server == NULL || historizing_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Server_readHistorizing(server->server, native_node_id, &historizing);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *historizing_out = historizing ? 1 : 0;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_write_historizing(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int historizing,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Server_writeHistorizing(server->server, native_node_id, historizing ? true : false);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_executable(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int *executable_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean executable;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (executable_out != NULL) {
    *executable_out = 0;
  }
  if (server == NULL || server->server == NULL || executable_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Server_readExecutable(server->server, native_node_id, &executable);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *executable_out = executable ? 1 : 0;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_user_executable(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int *executable_out,
    cpkt_opcua_status *status_out) {
  return cpkt_server_read_boolean_attribute(
      server,
      node_id,
      UA_ATTRIBUTEID_USEREXECUTABLE,
      executable_out,
      status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_write_executable(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id node_id,
    int executable,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Server_writeExecutable(server->server, native_node_id, executable ? true : false);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_method_argument_count(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id method_node_id,
    int direction,
    size_t *argument_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (argument_count_out != NULL) {
    *argument_count_out = 0;
  }
  if (server == NULL || server->server == NULL || argument_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_server_read_method_argument_variant(
      server->server,
      method_node_id,
      direction,
      &variant,
      status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_decode_method_argument_count(&variant, argument_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_method_argument(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id method_node_id,
    int direction,
    size_t argument_index,
    cpkt_opcua_node_id *data_type_out,
    long *value_rank_out,
    char *name_buffer,
    size_t name_buffer_size,
    size_t *required_name_size_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (required_name_size_out != NULL) {
    *required_name_size_out = 0;
  }
  if (data_type_out != NULL) {
    *data_type_out = cpkt_opcua_node_id_null();
  }
  if (value_rank_out != NULL) {
    *value_rank_out = 0;
  }
  if (server == NULL || server->server == NULL || data_type_out == NULL || value_rank_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_server_read_method_argument_variant(
      server->server,
      method_node_id,
      direction,
      &variant,
      status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_decode_method_argument(
      &variant,
      argument_index,
      data_type_out,
      value_rank_out,
      name_buffer,
      name_buffer_size,
      required_name_size_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_write(
    cpkt_opcua_server *server,
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
  if (server == NULL || server->server == NULL || value == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  UA_Variant_init(&variant);
  result = cpkt_set_variant(&variant, value);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Server_writeValue(server->server, native_node_id, variant);
  UA_Variant_clear(&variant);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

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
    cpkt_opcua_node_id source_node_id,
    cpkt_opcua_node_id event_type_id,
    unsigned long severity,
    const char *message,
    cpkt_opcua_server_event **event_out,
    cpkt_opcua_status *status_out) {
  cpkt_opcua_server_event *event;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (event_out != NULL) {
    *event_out = NULL;
  }
  if (event_out == NULL || message == NULL || severity > (unsigned long)USHRT_MAX ||
      !cpkt_valid_node_id(source_node_id) || !cpkt_valid_node_id(event_type_id)) {
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
  result = cpkt_copy_facade_node_id(source_node_id, &event->source_node_id, &event->source_memory);
  if (result != CPKT_OPCUA_OK) {
    cpkt_opcua_server_event_free(event);
    return result;
  }
  result = cpkt_copy_facade_node_id(event_type_id, &event->event_type_id, &event->event_type_memory);
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
    cpkt_opcua_server_event *event,
    unsigned short namespace_index,
    const char *field_name,
    const cpkt_opcua_value *value,
    cpkt_opcua_status *status_out) {
  UA_QualifiedName key;
  UA_Variant variant;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (event == NULL || field_name == NULL || field_name[0] == '\0' || value == NULL) {
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
    cpkt_opcua_server *server,
    cpkt_opcua_server_event *event,
    unsigned char *event_id_buffer,
    size_t event_id_buffer_size,
    size_t *required_event_id_size_out,
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
  if (server == NULL || server->server == NULL || event == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_source_node_id = cpkt_make_node_id(event->source_node_id);
  native_event_type_id = cpkt_make_node_id(event->event_type_id);
  native_message = UA_LOCALIZEDTEXT((char *)"en-US", event->message);
  UA_ByteString_init(&event_id);
  status = UA_Server_createEvent(
      server->server,
      native_source_node_id,
      native_event_type_id,
      (UA_UInt16)event->severity,
      native_message,
      &event->fields,
      NULL,
      &event_id);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_ByteString_clear(&event_id);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_ua_byte_string_to_buffer(
      &event_id,
      event_id_buffer,
      event_id_buffer_size,
      required_event_id_size_out);
  UA_ByteString_clear(&event_id);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_trigger_event(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id source_node_id,
    cpkt_opcua_node_id event_type_id,
    unsigned long severity,
    const char *message,
    unsigned char *event_id_buffer,
    size_t event_id_buffer_size,
    size_t *required_event_id_size_out,
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
      severity > (unsigned long)USHRT_MAX || !cpkt_valid_node_id(source_node_id) ||
      !cpkt_valid_node_id(event_type_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_source_node_id = cpkt_make_node_id(source_node_id);
  native_event_type_id = cpkt_make_node_id(event_type_id);
  native_message = UA_LOCALIZEDTEXT((char *)"en-US", (char *)message);
  UA_ByteString_init(&event_id);
  status = UA_Server_createEvent(
      server->server,
      native_source_node_id,
      native_event_type_id,
      (UA_UInt16)severity,
      native_message,
      NULL,
      NULL,
      &event_id);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_ByteString_clear(&event_id);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_ua_byte_string_to_buffer(
      &event_id,
      event_id_buffer,
      event_id_buffer_size,
      required_event_id_size_out);
  UA_ByteString_clear(&event_id);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_native(
    cpkt_opcua_server *server,
    cpkt_opcua_server_native_fn fn,
    void *user) {
  if (server == NULL || server->server == NULL || fn == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  return fn(server->server, user) == 0 ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_pubsub_native(
    cpkt_opcua_server *server,
    cpkt_opcua_server_native_fn fn,
    void *user) {
  return cpkt_opcua_server_native(server, fn, user);
}

static UA_String cpkt_borrowed_string_or_null(const char *value) {
  if (value == NULL) {
    return UA_STRING_NULL;
  }
  return UA_STRING((char *)value);
}

static cpkt_opcua_result cpkt_pubsub_map_set_string(
    UA_KeyValueMap *map,
    const char *key,
    const char *value) {
  UA_String string_value;
  UA_StatusCode status;

  if (value == NULL) {
    return CPKT_OPCUA_OK;
  }
  string_value = UA_STRING((char *)value);
  status = UA_KeyValueMap_setScalar(
      map,
      UA_QUALIFIEDNAME(0, (char *)key),
      &string_value,
      &UA_TYPES[UA_TYPES_STRING]);
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_ALLOC;
}

static cpkt_opcua_result cpkt_pubsub_map_set_uint16(
    UA_KeyValueMap *map,
    const char *key,
    unsigned short value) {
  UA_UInt16 native_value;
  UA_StatusCode status;

  native_value = (UA_UInt16)value;
  status = UA_KeyValueMap_setScalar(
      map,
      UA_QUALIFIEDNAME(0, (char *)key),
      &native_value,
      &UA_TYPES[UA_TYPES_UINT16]);
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_ALLOC;
}

static cpkt_opcua_result cpkt_pubsub_map_set_boolean(
    UA_KeyValueMap *map,
    const char *key,
    int value) {
  UA_Boolean native_value;
  UA_StatusCode status;

  native_value = value ? true : false;
  status = UA_KeyValueMap_setScalar(
      map,
      UA_QUALIFIEDNAME(0, (char *)key),
      &native_value,
      &UA_TYPES[UA_TYPES_BOOLEAN]);
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_ALLOC;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_mqtt_pubsub_connection(
    cpkt_opcua_server *server,
    const cpkt_opcua_mqtt_connection_options *options,
    cpkt_opcua_node_id *connection_id_out,
    char *node_id_buffer,
    size_t node_id_buffer_size,
    size_t *required_node_id_size_out,
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
  if (server == NULL || server->server == NULL || options == NULL || connection_id_out == NULL ||
      options->broker_host == NULL || options->topic == NULL ||
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
  config.transportProfileUri =
      UA_STRING((char *)"http://opcfoundation.org/UA-Profile/Transport/pubsub-mqtt-uadp");
  address.url = UA_STRING((char *)options->topic);
  UA_Variant_setScalar(&config.address, &address, &UA_TYPES[UA_TYPES_NETWORKADDRESSURLDATATYPE]);
  result = cpkt_pubsub_map_set_string(&config.connectionProperties, "address", options->broker_host);
  if (result == CPKT_OPCUA_OK) {
    result = cpkt_pubsub_map_set_uint16(&config.connectionProperties, "port", options->broker_port);
  }
  if (result == CPKT_OPCUA_OK) {
    result = cpkt_pubsub_map_set_string(&config.connectionProperties, "topic", options->topic);
  }
  if (result == CPKT_OPCUA_OK) {
    result = cpkt_pubsub_map_set_boolean(&config.connectionProperties, "subscribe", options->subscribe);
  }
  if (result == CPKT_OPCUA_OK) {
    result = cpkt_pubsub_map_set_string(&config.connectionProperties, "username", options->username);
  }
  if (result == CPKT_OPCUA_OK) {
    result = cpkt_pubsub_map_set_string(&config.connectionProperties, "password", options->password);
  }
  if (result == CPKT_OPCUA_OK) {
    result = cpkt_pubsub_map_set_uint16(
        &config.connectionProperties,
        "keep-alive",
        options->keep_alive_seconds);
  }
  if (result == CPKT_OPCUA_OK) {
    result = cpkt_pubsub_map_set_boolean(&config.connectionProperties, "validate", options->validate_only);
  }
  if (result != CPKT_OPCUA_OK) {
    UA_KeyValueMap_clear(&config.connectionProperties);
    return result;
  }
  status = UA_Server_addPubSubConnection(server->server, &config, &connection_id);
  UA_KeyValueMap_clear(&config.connectionProperties);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_NodeId_clear(&connection_id);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_native_node_id_to_output(
      &connection_id,
      connection_id_out,
      node_id_buffer,
      node_id_buffer_size,
      required_node_id_size_out);
  UA_NodeId_clear(&connection_id);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_published_dataset(
    cpkt_opcua_server *server,
    const char *name,
    cpkt_opcua_node_id *published_dataset_id_out,
    char *node_id_buffer,
    size_t node_id_buffer_size,
    size_t *required_node_id_size_out,
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
  if (server == NULL || server->server == NULL || name == NULL || published_dataset_id_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  memset(&config, 0, sizeof(config));
  UA_NodeId_init(&published_dataset_id);
  config.name = UA_STRING((char *)name);
  config.publishedDataSetType = UA_PUBSUB_DATASET_PUBLISHEDITEMS;
  add_result = UA_Server_addPublishedDataSet(server->server, &config, &published_dataset_id);
  if (status_out != NULL) {
    *status_out = cpkt_status(add_result.addResult);
  }
  if (add_result.addResult != UA_STATUSCODE_GOOD) {
    UA_NodeId_clear(&published_dataset_id);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_native_node_id_to_output(
      &published_dataset_id,
      published_dataset_id_out,
      node_id_buffer,
      node_id_buffer_size,
      required_node_id_size_out);
  UA_NodeId_clear(&published_dataset_id);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_published_variable(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id published_dataset_id,
    cpkt_opcua_node_id variable_node_id,
    const char *field_name,
    cpkt_opcua_node_id *field_id_out,
    char *node_id_buffer,
    size_t node_id_buffer_size,
    size_t *required_node_id_size_out,
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
      !cpkt_valid_node_id(published_dataset_id) || !cpkt_valid_node_id(variable_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_published_dataset_id = cpkt_make_node_id(published_dataset_id);
  native_variable_node_id = cpkt_make_node_id(variable_node_id);
  UA_NodeId_init(&field_id);
  memset(&config, 0, sizeof(config));
  config.dataSetFieldType = UA_PUBSUB_DATASETFIELD_VARIABLE;
  config.field.variable.fieldNameAlias = cpkt_borrowed_string_or_null(field_name);
  config.field.variable.publishParameters.publishedVariable = native_variable_node_id;
  config.field.variable.publishParameters.attributeId = UA_ATTRIBUTEID_VALUE;
  add_result = UA_Server_addDataSetField(
      server->server,
      native_published_dataset_id,
      &config,
      &field_id);
  if (status_out != NULL) {
    *status_out = cpkt_status(add_result.result);
  }
  if (add_result.result != UA_STATUSCODE_GOOD) {
    UA_NodeId_clear(&field_id);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_native_node_id_to_output(
      &field_id,
      field_id_out,
      node_id_buffer,
      node_id_buffer_size,
      required_node_id_size_out);
  UA_NodeId_clear(&field_id);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_pubsub_writer_group(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id connection_id,
    const cpkt_opcua_pubsub_writer_group_options *options,
    cpkt_opcua_node_id *writer_group_id_out,
    char *node_id_buffer,
    size_t node_id_buffer_size,
    size_t *required_node_id_size_out,
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
  if (server == NULL || server->server == NULL || options == NULL || writer_group_id_out == NULL ||
      !cpkt_valid_node_id(connection_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_connection_id = cpkt_make_node_id(connection_id);
  UA_NodeId_init(&writer_group_id);
  memset(&config, 0, sizeof(config));
  config.name = cpkt_borrowed_string_or_null(options->name);
  config.enabled = options->enabled ? true : false;
  config.writerGroupId = (UA_UInt16)options->writer_group_id;
  config.publishingInterval = options->publishing_interval_ms;
  config.encodingMimeType = options->json_encoding ? UA_PUBSUB_ENCODING_JSON : UA_PUBSUB_ENCODING_UADP;
  status = UA_Server_addWriterGroup(server->server, native_connection_id, &config, &writer_group_id);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_NodeId_clear(&writer_group_id);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_native_node_id_to_output(
      &writer_group_id,
      writer_group_id_out,
      node_id_buffer,
      node_id_buffer_size,
      required_node_id_size_out);
  UA_NodeId_clear(&writer_group_id);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_pubsub_data_set_writer(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id writer_group_id,
    cpkt_opcua_node_id published_dataset_id,
    const cpkt_opcua_pubsub_data_set_writer_options *options,
    cpkt_opcua_node_id *data_set_writer_id_out,
    char *node_id_buffer,
    size_t node_id_buffer_size,
    size_t *required_node_id_size_out,
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
  if (server == NULL || server->server == NULL || options == NULL || data_set_writer_id_out == NULL ||
      !cpkt_valid_node_id(writer_group_id) || !cpkt_valid_node_id(published_dataset_id) ||
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
  status = UA_Server_addDataSetWriter(
      server->server,
      native_writer_group_id,
      native_published_dataset_id,
      &config,
      &data_set_writer_id);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_NodeId_clear(&data_set_writer_id);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_native_node_id_to_output(
      &data_set_writer_id,
      data_set_writer_id_out,
      node_id_buffer,
      node_id_buffer_size,
      required_node_id_size_out);
  UA_NodeId_clear(&data_set_writer_id);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_pubsub_reader_group(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id connection_id,
    const cpkt_opcua_pubsub_reader_group_options *options,
    cpkt_opcua_node_id *reader_group_id_out,
    char *node_id_buffer,
    size_t node_id_buffer_size,
    size_t *required_node_id_size_out,
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
  if (server == NULL || server->server == NULL || options == NULL || reader_group_id_out == NULL ||
      !cpkt_valid_node_id(connection_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_connection_id = cpkt_make_node_id(connection_id);
  UA_NodeId_init(&reader_group_id);
  memset(&config, 0, sizeof(config));
  config.name = cpkt_borrowed_string_or_null(options->name);
  config.enabled = options->enabled ? true : false;
  config.encodingMimeType = options->json_encoding ? UA_PUBSUB_ENCODING_JSON : UA_PUBSUB_ENCODING_UADP;
  status = UA_Server_addReaderGroup(server->server, native_connection_id, &config, &reader_group_id);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_NodeId_clear(&reader_group_id);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_native_node_id_to_output(
      &reader_group_id,
      reader_group_id_out,
      node_id_buffer,
      node_id_buffer_size,
      required_node_id_size_out);
  UA_NodeId_clear(&reader_group_id);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_pubsub_data_set_reader(
    cpkt_opcua_server *server,
    cpkt_opcua_node_id reader_group_id,
    const cpkt_opcua_pubsub_data_set_reader_options *options,
    cpkt_opcua_node_id *data_set_reader_id_out,
    char *node_id_buffer,
    size_t node_id_buffer_size,
    size_t *required_node_id_size_out,
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
  if (server == NULL || server->server == NULL || options == NULL || data_set_reader_id_out == NULL ||
      !cpkt_valid_node_id(reader_group_id) ||
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
  status = UA_Server_addDataSetReader(
      server->server,
      native_reader_group_id,
      &config,
      &data_set_reader_id);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_NodeId_clear(&data_set_reader_id);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_native_node_id_to_output(
      &data_set_reader_id,
      data_set_reader_id_out,
      node_id_buffer,
      node_id_buffer_size,
      required_node_id_size_out);
  UA_NodeId_clear(&data_set_reader_id);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_write_pubsub_configuration(
    cpkt_opcua_server *server,
    unsigned char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out) {
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
  status = UA_Server_writePubSubConfigurationToByteString(server->server, &bytes);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_ByteString_clear(&bytes);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_ua_byte_string_to_buffer(&bytes, buffer, buffer_size, required_size_out);
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
    cpkt_opcua_server *server,
    const unsigned char *buffer,
    size_t buffer_size,
    cpkt_opcua_status *status_out) {
#ifdef UA_ENABLE_PUBSUB_FILE_CONFIG
  UA_ByteString bytes;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || (buffer == NULL && buffer_size != 0)) {
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

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_history_native(
    cpkt_opcua_server *server,
    cpkt_opcua_server_native_fn fn,
    void *user) {
  return cpkt_opcua_server_native(server, fn, user);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_new(cpkt_opcua_client **out) {
  cpkt_opcua_client *client;
  UA_StatusCode status;

  if (out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  *out = NULL;
  client = (cpkt_opcua_client *)calloc(1, sizeof(*client));
  if (client == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  client->client = UA_Client_new();
  if (client->client == NULL) {
    free(client);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  status = UA_ClientConfig_setDefault(UA_Client_getConfig(client->client));
  if (status != UA_STATUSCODE_GOOD) {
    UA_Client_delete(client->client);
    free(client);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *out = client;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_client_free(cpkt_opcua_client *client) {
  struct cpkt_opcua_monitor_context *monitor;
  struct cpkt_opcua_monitor_context *next;
  struct cpkt_opcua_async_context *async_context;
  struct cpkt_opcua_async_context *async_next;

  if (client == NULL) {
    return;
  }
  if (client->client != NULL) {
    UA_Client_delete(client->client);
  }
  monitor = client->monitors;
  while (monitor != NULL) {
    next = monitor->next;
    free(monitor);
    monitor = next;
  }
  async_context = client->asyncs;
  while (async_context != NULL) {
    async_next = async_context->next;
    free(async_context);
    async_context = async_next;
  }
  free(client);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_connect(
    cpkt_opcua_client *client,
    const char *endpoint_url,
    cpkt_opcua_status *status_out) {
  UA_StatusCode status;
  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || endpoint_url == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  status = UA_Client_connect(client->client, endpoint_url);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_connect_username(
    cpkt_opcua_client *client,
    const char *endpoint_url,
    const char *username,
    const char *password,
    cpkt_opcua_status *status_out) {
  UA_StatusCode status;
  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || endpoint_url == NULL ||
      username == NULL || password == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  UA_Client_getConfig(client->client)->allowNonePolicyPassword = true;
  status = UA_Client_connectUsername(client->client, endpoint_url, username, password);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_native_config(
    cpkt_opcua_client *client,
    cpkt_opcua_client_native_config_fn fn,
    void *user,
    cpkt_opcua_status *status_out) {
  cpkt_opcua_status callback_status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || fn == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  callback_status = fn(UA_Client_getConfig(client->client), user);
  if (status_out != NULL) {
    *status_out = callback_status;
  }
  return callback_status == 0 ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_security_plugin_native_config(
    cpkt_opcua_client *client,
    cpkt_opcua_client_native_config_fn fn,
    void *user,
    cpkt_opcua_status *status_out) {
  return cpkt_opcua_client_native_config(client, fn, user, status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_set_default_encryption(
    cpkt_opcua_client *client,
    const unsigned char *certificate,
    size_t certificate_length,
    const unsigned char *private_key,
    size_t private_key_length,
    const cpkt_opcua_byte_string_view *trust_list,
    size_t trust_list_count,
    const cpkt_opcua_byte_string_view *revocation_list,
    size_t revocation_list_count,
    cpkt_opcua_status *status_out) {
  UA_StatusCode status;
#ifdef UA_ENABLE_ENCRYPTION
  UA_ByteString native_certificate;
  UA_ByteString native_private_key;
  UA_ByteString *native_trust_list;
  UA_ByteString *native_revocation_list;
  cpkt_opcua_result result;

  native_trust_list = NULL;
  native_revocation_list = NULL;
#endif

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      certificate == NULL || certificate_length == 0 ||
      private_key == NULL || private_key_length == 0) {
    return CPKT_OPCUA_ERR_ARG;
  }
#ifndef UA_ENABLE_ENCRYPTION
  status = UA_STATUSCODE_BADNOTSUPPORTED;
#else
  native_certificate = cpkt_make_borrowed_byte_string(certificate, certificate_length);
  native_private_key = cpkt_make_borrowed_byte_string(private_key, private_key_length);
  result = cpkt_make_borrowed_byte_string_array(trust_list, trust_list_count, &native_trust_list);
  if (result == CPKT_OPCUA_OK) {
    result = cpkt_make_borrowed_byte_string_array(revocation_list, revocation_list_count, &native_revocation_list);
  }
  if (result != CPKT_OPCUA_OK) {
    free(native_trust_list);
    free(native_revocation_list);
    return result;
  }
  status = UA_ClientConfig_setDefaultEncryption(
      UA_Client_getConfig(client->client),
      native_certificate,
      native_private_key,
      native_trust_list,
      trust_list_count,
      native_revocation_list,
      revocation_list_count);
  free(native_trust_list);
  free(native_revocation_list);
#endif
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_disconnect(
    cpkt_opcua_client *client,
    cpkt_opcua_status *status_out) {
  UA_StatusCode status;
  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  status = UA_Client_disconnect(client->client);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_run_iterate(
    cpkt_opcua_client *client,
    unsigned long timeout_ms,
    cpkt_opcua_status *status_out) {
  UA_StatusCode status;
  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || timeout_ms > (unsigned long)UINT_MAX) {
    return CPKT_OPCUA_ERR_ARG;
  }
  status = UA_Client_run_iterate(client->client, (UA_UInt32)timeout_ms);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_get_namespace_index(
    cpkt_opcua_client *client,
    const char *namespace_uri,
    unsigned short *namespace_index_out,
    cpkt_opcua_status *status_out) {
  UA_String native_namespace_uri;
  UA_UInt16 namespace_index;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (namespace_index_out != NULL) {
    *namespace_index_out = 0;
  }
  if (client == NULL || client->client == NULL || namespace_uri == NULL ||
      namespace_index_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_namespace_uri = UA_STRING((char *)namespace_uri);
  status = UA_Client_getNamespaceIndex(client->client, native_namespace_uri, &namespace_index);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *namespace_index_out = (unsigned short)namespace_index;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_get_namespace_uri(
    cpkt_opcua_client *client,
    unsigned short namespace_index,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out) {
  UA_String namespace_uri;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (client == NULL || client->client == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  UA_String_init(&namespace_uri);
  status = UA_Client_getNamespaceUri(client->client, (UA_UInt16)namespace_index, &namespace_uri);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_String_clear(&namespace_uri);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_ua_string_to_buffer(&namespace_uri, buffer, buffer_size, required_size_out);
  UA_String_clear(&namespace_uri);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_get_endpoint_count(
    cpkt_opcua_client *client,
    const char *server_url,
    size_t *endpoint_count_out,
    cpkt_opcua_status *status_out) {
  UA_EndpointDescription *endpoints;
  size_t endpoint_count;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (endpoint_count_out != NULL) {
    *endpoint_count_out = 0;
  }
  if (client == NULL || client->client == NULL || server_url == NULL || endpoint_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  endpoints = NULL;
  endpoint_count = 0;
  status = UA_Client_getEndpoints(client->client, server_url, &endpoint_count, &endpoints);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_Array_delete(endpoints, endpoint_count, &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *endpoint_count_out = endpoint_count;
  UA_Array_delete(endpoints, endpoint_count, &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_get_endpoint_url(
    cpkt_opcua_client *client,
    const char *server_url,
    size_t endpoint_index,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out) {
  UA_EndpointDescription *endpoints;
  size_t endpoint_count;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (client == NULL || client->client == NULL || server_url == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  endpoints = NULL;
  endpoint_count = 0;
  status = UA_Client_getEndpoints(client->client, server_url, &endpoint_count, &endpoints);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_Array_delete(endpoints, endpoint_count, &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (endpoint_index >= endpoint_count) {
    UA_Array_delete(endpoints, endpoint_count, &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);
    return CPKT_OPCUA_ERR_RANGE;
  }
  result = cpkt_copy_ua_string_to_buffer(
      &endpoints[endpoint_index].endpointUrl,
      buffer,
      buffer_size,
      required_size_out);
  UA_Array_delete(endpoints, endpoint_count, &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);
  return result;
}

static cpkt_opcua_result cpkt_client_find_servers(
    cpkt_opcua_client *client,
    const char *server_url,
    size_t *server_count_out,
    UA_ApplicationDescription **servers_out,
    cpkt_opcua_status *status_out) {
  UA_ApplicationDescription *servers;
  size_t server_count;
  UA_StatusCode status;

  if (server_count_out != NULL) {
    *server_count_out = 0;
  }
  if (servers_out != NULL) {
    *servers_out = NULL;
  }
  if (client == NULL || client->client == NULL || server_url == NULL || server_count_out == NULL ||
      servers_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  servers = NULL;
  server_count = 0;
  status = UA_Client_findServers(
      client->client,
      server_url,
      0,
      NULL,
      0,
      NULL,
      &server_count,
      &servers);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_Array_delete(servers, server_count, &UA_TYPES[UA_TYPES_APPLICATIONDESCRIPTION]);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *server_count_out = server_count;
  *servers_out = servers;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_find_server_count(
    cpkt_opcua_client *client,
    const char *server_url,
    size_t *server_count_out,
    cpkt_opcua_status *status_out) {
  UA_ApplicationDescription *servers;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  result = cpkt_client_find_servers(client, server_url, server_count_out, &servers, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  UA_Array_delete(servers, *server_count_out, &UA_TYPES[UA_TYPES_APPLICATIONDESCRIPTION]);
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_find_server_application_uri(
    cpkt_opcua_client *client,
    const char *server_url,
    size_t server_index,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out) {
  UA_ApplicationDescription *servers;
  size_t server_count;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  result = cpkt_client_find_servers(client, server_url, &server_count, &servers, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  if (server_index >= server_count) {
    UA_Array_delete(servers, server_count, &UA_TYPES[UA_TYPES_APPLICATIONDESCRIPTION]);
    return CPKT_OPCUA_ERR_RANGE;
  }
  result = cpkt_copy_ua_string_to_buffer(
      &servers[server_index].applicationUri,
      buffer,
      buffer_size,
      required_size_out);
  UA_Array_delete(servers, server_count, &UA_TYPES[UA_TYPES_APPLICATIONDESCRIPTION]);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_find_server_application_name(
    cpkt_opcua_client *client,
    const char *server_url,
    size_t server_index,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out) {
  UA_ApplicationDescription *servers;
  size_t server_count;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  result = cpkt_client_find_servers(client, server_url, &server_count, &servers, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  if (server_index >= server_count) {
    UA_Array_delete(servers, server_count, &UA_TYPES[UA_TYPES_APPLICATIONDESCRIPTION]);
    return CPKT_OPCUA_ERR_RANGE;
  }
  result = cpkt_copy_ua_string_to_buffer(
      &servers[server_index].applicationName.text,
      buffer,
      buffer_size,
      required_size_out);
  UA_Array_delete(servers, server_count, &UA_TYPES[UA_TYPES_APPLICATIONDESCRIPTION]);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_value *value_out,
    char *string_buffer,
    size_t string_buffer_size,
    size_t *required_string_size_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Variant variant;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || value_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_Variant_init(&variant);
  status = UA_Client_readValueAttribute(client->client, native_node_id, &variant);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_get_variant(&variant, value_out, string_buffer, string_buffer_size, required_string_size_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_native_variant(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_native_variant_fn fn,
    void *user,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Variant variant;
  UA_StatusCode status;
  int callback_status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || fn == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_Variant_init(&variant);
  status = UA_Client_readValueAttribute(client->client, native_node_id, &variant);
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
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_native_data_value_fn fn,
    void *user,
    cpkt_opcua_status *status_out) {
  UA_ReadRequest request;
  UA_ReadResponse response;
  UA_ReadValueId read_value;
  UA_StatusCode service_status;
  int callback_status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || fn == NULL || !cpkt_valid_node_id(node_id)) {
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
  if (service_status != UA_STATUSCODE_GOOD || response.resultsSize != 1 || response.results == NULL) {
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
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_data_value *data_value_out,
    char *string_buffer,
    size_t string_buffer_size,
    size_t *required_string_size_out,
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
  if (service_status != UA_STATUSCODE_GOOD || response.resultsSize != 1 || response.results == NULL) {
    if (status_out != NULL) {
      *status_out = cpkt_status(service_status);
    }
    UA_ReadResponse_clear(&response);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (status_out != NULL) {
    *status_out = cpkt_status(response.results[0].status);
  }
  result = cpkt_get_data_value(
      &response.results[0],
      data_value_out,
      string_buffer,
      string_buffer_size,
      required_string_size_out);
  UA_ReadResponse_clear(&response);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_history_read_raw(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_datetime start_time,
    cpkt_opcua_datetime end_time,
    const char *index_range,
    int return_bounds,
    unsigned long values_per_response,
    cpkt_opcua_history_data_value_fn fn,
    void *user,
    char *string_buffer,
    size_t string_buffer_size,
    size_t *required_string_size_out,
    cpkt_opcua_status *status_out) {
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
  if (client == NULL || client->client == NULL || fn == NULL || !cpkt_valid_node_id(node_id)) {
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
      client->client,
      &native_node_id,
      cpkt_history_read_raw_callback,
      cpkt_make_datetime(start_time),
      cpkt_make_datetime(end_time),
      native_index_range,
      return_bounds ? true : false,
      (UA_UInt32)values_per_response,
      UA_TIMESTAMPSTORETURN_BOTH,
      &context);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (context.result != CPKT_OPCUA_OK) {
    return context.result;
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

static cpkt_opcua_result cpkt_client_read_array_variant(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    UA_Variant *variant_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (variant_out != NULL) {
    UA_Variant_init(variant_out);
  }
  if (client == NULL || client->client == NULL || variant_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readValueAttribute(client->client, native_node_id, variant_out);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_boolean_array(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result = cpkt_client_read_array_variant(client, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_boolean_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_integer_array(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    long *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result = cpkt_client_read_array_variant(client, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_integer_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_uint64_array(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_uint64 *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result = cpkt_client_read_array_variant(client, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_uint64_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_datetime_array(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_datetime *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result = cpkt_client_read_array_variant(client, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_datetime_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_status_array(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_status *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result = cpkt_client_read_array_variant(client, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_status_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_guid_array(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_guid *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result = cpkt_client_read_array_variant(client, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_guid_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_qualified_name_array(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_qualified_name_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result = cpkt_client_read_array_variant(client, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_each_qualified_name_array_from_variant(&variant, fn, user, value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_localized_text_array(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_localized_text_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result = cpkt_client_read_array_variant(client, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_each_localized_text_array_from_variant(&variant, fn, user, value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_double_array(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    double *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result = cpkt_client_read_array_variant(client, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_double_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_string_array(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_string_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result = cpkt_client_read_array_variant(client, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_each_string_array_from_variant(&variant, fn, user, value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_byte_string_array(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_byte_string_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  result = cpkt_client_read_array_variant(client, node_id, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_each_byte_string_array_from_variant(&variant, fn, user, value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

static cpkt_opcua_result cpkt_client_read_index_range_variant(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    UA_Variant *variant_out,
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
  if (client == NULL || client->client == NULL || index_range == NULL || variant_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
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
  return copy_status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_ALLOC;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_boolean_array_range(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    int *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_client_read_index_range_variant(client, node_id, index_range, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_boolean_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_integer_array_range(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    long *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_client_read_index_range_variant(client, node_id, index_range, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_integer_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_double_array_range(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    double *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_client_read_index_range_variant(client, node_id, index_range, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_double_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_uint64_array_range(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_uint64 *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_client_read_index_range_variant(client, node_id, index_range, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_uint64_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_datetime_array_range(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_datetime *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_client_read_index_range_variant(client, node_id, index_range, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_datetime_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_status_array_range(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_status *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_client_read_index_range_variant(client, node_id, index_range, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_status_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_guid_array_range(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_guid *values,
    size_t value_count,
    size_t *required_value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (required_value_count_out != NULL) {
    *required_value_count_out = 0;
  }
  if (required_value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_client_read_index_range_variant(client, node_id, index_range, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_copy_guid_array_from_variant(&variant, values, value_count, required_value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_qualified_name_array_range(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_qualified_name_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (value_count_out != NULL) {
    *value_count_out = 0;
  }
  if (value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_client_read_index_range_variant(client, node_id, index_range, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_each_qualified_name_range_from_variant(&variant, fn, user, value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_localized_text_array_range(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_localized_text_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (value_count_out != NULL) {
    *value_count_out = 0;
  }
  if (value_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_client_read_index_range_variant(client, node_id, index_range, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_each_localized_text_range_from_variant(&variant, fn, user, value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_string_array_range(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_string_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out) {
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
  result = cpkt_client_read_index_range_variant(client, node_id, index_range, &variant, effective_status);
  if (result != CPKT_OPCUA_OK) {
    if (result != CPKT_OPCUA_ERR_UPSTREAM ||
        *effective_status != cpkt_status(UA_STATUSCODE_BADINDEXRANGEINVALID)) {
      return result;
    }
    if (!cpkt_parse_simple_array_index_range(index_range, &start, &count)) {
      return result;
    }
    result = cpkt_client_read_array_variant(client, node_id, &variant, effective_status);
    if (result != CPKT_OPCUA_OK) {
      return result;
    }
    result = cpkt_each_string_array_slice_from_variant(&variant, start, count, fn, user, value_count_out);
    if (result == CPKT_OPCUA_ERR_RANGE && status_out != NULL) {
      *status_out = cpkt_status(UA_STATUSCODE_BADINDEXRANGENODATA);
    }
    UA_Variant_clear(&variant);
    return result;
  }
  result = cpkt_each_string_range_from_variant(&variant, fn, user, value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_byte_string_array_range(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    cpkt_opcua_byte_string_array_fn fn,
    void *user,
    size_t *value_count_out,
    cpkt_opcua_status *status_out) {
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
  result = cpkt_client_read_index_range_variant(client, node_id, index_range, &variant, effective_status);
  if (result != CPKT_OPCUA_OK) {
    if (result != CPKT_OPCUA_ERR_UPSTREAM ||
        *effective_status != cpkt_status(UA_STATUSCODE_BADINDEXRANGEINVALID)) {
      return result;
    }
    if (!cpkt_parse_simple_array_index_range(index_range, &start, &count)) {
      return result;
    }
    result = cpkt_client_read_array_variant(client, node_id, &variant, effective_status);
    if (result != CPKT_OPCUA_OK) {
      return result;
    }
    result = cpkt_each_byte_string_array_slice_from_variant(&variant, start, count, fn, user, value_count_out);
    if (result == CPKT_OPCUA_ERR_RANGE && status_out != NULL) {
      *status_out = cpkt_status(UA_STATUSCODE_BADINDEXRANGENODATA);
    }
    UA_Variant_clear(&variant);
    return result;
  }
  result = cpkt_each_byte_string_range_from_variant(&variant, fn, user, value_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_index_range(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *index_range,
    const cpkt_opcua_value *value,
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
  if (client == NULL || client->client == NULL || index_range == NULL || value == NULL || !cpkt_valid_node_id(node_id)) {
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
cpkt_opcua_result cpkt_opcua_client_read_node_id(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id *node_id_out,
    char *identifier_buffer,
    size_t identifier_buffer_size,
    size_t *required_identifier_size_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_NodeId native_result;
  UA_StatusCode status;
  cpkt_opcua_node_id facade_result;
  struct cpkt_owned_node_id_memory owned;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_identifier_size_out != NULL) {
    *required_identifier_size_out = 0;
  }
  if (node_id_out != NULL) {
    *node_id_out = cpkt_opcua_node_id_null();
  }
  if (client == NULL || client->client == NULL || node_id_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_NodeId_init(&native_result);
  status = UA_Client_readNodeIdAttribute(client->client, native_node_id, &native_result);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_NodeId_clear(&native_result);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  owned.string = NULL;
  owned.byte_string = NULL;
  if (!cpkt_native_node_id_to_facade(&native_result, &facade_result, &owned)) {
    cpkt_owned_node_id_memory_clear(&owned);
    UA_NodeId_clear(&native_result);
    return CPKT_OPCUA_ERR_TYPE;
  }
  result = cpkt_copy_node_id_to_caller(
      &facade_result,
      &owned,
      node_id_out,
      identifier_buffer,
      identifier_buffer_size,
      required_identifier_size_out);
  if (result != CPKT_OPCUA_OK) {
    *node_id_out = cpkt_opcua_node_id_null();
  }
  cpkt_owned_node_id_memory_clear(&owned);
  UA_NodeId_clear(&native_result);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_node_class(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned long *node_class_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_NodeClass node_class;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (node_class_out != NULL) {
    *node_class_out = 0;
  }
  if (client == NULL || client->client == NULL || node_class_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readNodeClassAttribute(client->client, native_node_id, &node_class);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *node_class_out = (unsigned long)node_class;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_browse_name(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned short *namespace_index_out,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_QualifiedName browse_name;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (namespace_index_out != NULL) {
    *namespace_index_out = 0;
  }
  if (client == NULL || client->client == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_QualifiedName_init(&browse_name);
  status = UA_Client_readBrowseNameAttribute(client->client, native_node_id, &browse_name);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_QualifiedName_clear(&browse_name);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (namespace_index_out != NULL) {
    *namespace_index_out = (unsigned short)browse_name.namespaceIndex;
  }
  result = cpkt_copy_ua_string_to_buffer(&browse_name.name, buffer, buffer_size, required_size_out);
  UA_QualifiedName_clear(&browse_name);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_display_name(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_LocalizedText display_name;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (client == NULL || client->client == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_LocalizedText_init(&display_name);
  status = UA_Client_readDisplayNameAttribute(client->client, native_node_id, &display_name);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_LocalizedText_clear(&display_name);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_ua_string_to_buffer(&display_name.text, buffer, buffer_size, required_size_out);
  UA_LocalizedText_clear(&display_name);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_description(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_LocalizedText description;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (client == NULL || client->client == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_LocalizedText_init(&description);
  status = UA_Client_readDescriptionAttribute(client->client, native_node_id, &description);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_LocalizedText_clear(&description);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_ua_string_to_buffer(&description.text, buffer, buffer_size, required_size_out);
  UA_LocalizedText_clear(&description);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_display_name(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *display_name,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_LocalizedText native_display_name;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || display_name == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_display_name = UA_LOCALIZEDTEXT((char *)"en-US", (char *)display_name);
  status = UA_Client_writeDisplayNameAttribute(client->client, native_node_id, &native_display_name);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_description(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *description,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_LocalizedText native_description;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || description == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_description = UA_LOCALIZEDTEXT((char *)"en-US", (char *)description);
  status = UA_Client_writeDescriptionAttribute(client->client, native_node_id, &native_description);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_write_mask(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned long *write_mask_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_UInt32 write_mask;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (write_mask_out != NULL) {
    *write_mask_out = 0;
  }
  if (client == NULL || client->client == NULL || write_mask_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readWriteMaskAttribute(client->client, native_node_id, &write_mask);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *write_mask_out = (unsigned long)write_mask;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_user_write_mask(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned long *write_mask_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_UInt32 write_mask;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (write_mask_out != NULL) {
    *write_mask_out = 0;
  }
  if (client == NULL || client->client == NULL || write_mask_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readUserWriteMaskAttribute(client->client, native_node_id, &write_mask);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *write_mask_out = (unsigned long)write_mask;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_write_mask(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned long write_mask,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_UInt32 native_write_mask;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!cpkt_ulong_fits_uint32(write_mask)) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_write_mask = (UA_UInt32)write_mask;
  status = UA_Client_writeWriteMaskAttribute(client->client, native_node_id, &native_write_mask);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_is_abstract(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int *is_abstract_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean is_abstract;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (is_abstract_out != NULL) {
    *is_abstract_out = 0;
  }
  if (client == NULL || client->client == NULL || is_abstract_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readIsAbstractAttribute(client->client, native_node_id, &is_abstract);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *is_abstract_out = is_abstract ? 1 : 0;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_is_abstract(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int is_abstract,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean native_is_abstract;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_is_abstract = is_abstract ? true : false;
  status = UA_Client_writeIsAbstractAttribute(client->client, native_node_id, &native_is_abstract);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_symmetric(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int *symmetric_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean symmetric;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (symmetric_out != NULL) {
    *symmetric_out = 0;
  }
  if (client == NULL || client->client == NULL || symmetric_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readSymmetricAttribute(client->client, native_node_id, &symmetric);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *symmetric_out = symmetric ? 1 : 0;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_symmetric(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int symmetric,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean native_symmetric;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_symmetric = symmetric ? true : false;
  status = UA_Client_writeSymmetricAttribute(client->client, native_node_id, &native_symmetric);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_inverse_name(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    char *buffer,
    size_t buffer_size,
    size_t *required_size_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_LocalizedText inverse_name;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (client == NULL || client->client == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_LocalizedText_init(&inverse_name);
  status = UA_Client_readInverseNameAttribute(client->client, native_node_id, &inverse_name);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_LocalizedText_clear(&inverse_name);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_ua_string_to_buffer(&inverse_name.text, buffer, buffer_size, required_size_out);
  UA_LocalizedText_clear(&inverse_name);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_inverse_name(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *inverse_name,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_LocalizedText native_inverse_name;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || inverse_name == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_inverse_name = UA_LOCALIZEDTEXT((char *)"en-US", (char *)inverse_name);
  status = UA_Client_writeInverseNameAttribute(client->client, native_node_id, &native_inverse_name);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_contains_no_loops(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int *contains_no_loops_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean contains_no_loops;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (contains_no_loops_out != NULL) {
    *contains_no_loops_out = 0;
  }
  if (client == NULL || client->client == NULL || contains_no_loops_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readContainsNoLoopsAttribute(client->client, native_node_id, &contains_no_loops);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *contains_no_loops_out = contains_no_loops ? 1 : 0;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_contains_no_loops(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int contains_no_loops,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean native_contains_no_loops;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_contains_no_loops = contains_no_loops ? true : false;
  status = UA_Client_writeContainsNoLoopsAttribute(client->client, native_node_id, &native_contains_no_loops);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_event_notifier(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned long *event_notifier_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Byte event_notifier;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (event_notifier_out != NULL) {
    *event_notifier_out = 0;
  }
  if (client == NULL || client->client == NULL || event_notifier_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readEventNotifierAttribute(client->client, native_node_id, &event_notifier);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *event_notifier_out = (unsigned long)event_notifier;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_event_notifier(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned long event_notifier,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Byte native_event_notifier;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (event_notifier > (unsigned long)UCHAR_MAX) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_event_notifier = (UA_Byte)event_notifier;
  status = UA_Client_writeEventNotifierAttribute(client->client, native_node_id, &native_event_notifier);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_data_type(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id *data_type_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_NodeId native_data_type;
  UA_StatusCode status;
  struct cpkt_owned_node_id_memory data_type_memory;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (data_type_out != NULL) {
    *data_type_out = cpkt_opcua_node_id_numeric(0, 0);
  }
  if (client == NULL || client->client == NULL || data_type_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_NodeId_init(&native_data_type);
  status = UA_Client_readDataTypeAttribute(client->client, native_node_id, &native_data_type);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_NodeId_clear(&native_data_type);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  data_type_memory.string = NULL;
  data_type_memory.byte_string = NULL;
  if (!cpkt_native_node_id_to_facade(&native_data_type, data_type_out, &data_type_memory)) {
    UA_NodeId_clear(&native_data_type);
    cpkt_owned_node_id_memory_clear(&data_type_memory);
    return CPKT_OPCUA_ERR_TYPE;
  }
  if (data_type_memory.string != NULL || data_type_memory.byte_string != NULL) {
    UA_NodeId_clear(&native_data_type);
    cpkt_owned_node_id_memory_clear(&data_type_memory);
    *data_type_out = cpkt_opcua_node_id_numeric(0, 0);
    return CPKT_OPCUA_ERR_TYPE;
  }
  cpkt_owned_node_id_memory_clear(&data_type_memory);
  UA_NodeId_clear(&native_data_type);
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_data_type(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id data_type,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_NodeId native_data_type;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || !cpkt_valid_node_id(node_id) ||
      !cpkt_valid_node_id(data_type)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_data_type = cpkt_make_node_id(data_type);
  status = UA_Client_writeDataTypeAttribute(client->client, native_node_id, &native_data_type);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_value_rank(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    long *value_rank_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Int32 value_rank;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (value_rank_out != NULL) {
    *value_rank_out = 0;
  }
  if (client == NULL || client->client == NULL || value_rank_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readValueRankAttribute(client->client, native_node_id, &value_rank);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *value_rank_out = (long)value_rank;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_value_rank(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    long value_rank,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Int32 native_value_rank;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!cpkt_long_fits_int32(value_rank)) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_value_rank = (UA_Int32)value_rank;
  status = UA_Client_writeValueRankAttribute(client->client, native_node_id, &native_value_rank);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_array_dimensions(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned long *dimensions,
    size_t dimension_count,
    size_t *required_dimension_count_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_UInt32 *native_dimensions;
  size_t native_dimension_count;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_dimension_count_out != NULL) {
    *required_dimension_count_out = 0;
  }
  if (client == NULL || client->client == NULL || required_dimension_count_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_dimensions = NULL;
  native_dimension_count = 0;
  status = UA_Client_readArrayDimensionsAttribute(
      client->client,
      native_node_id,
      &native_dimension_count,
      &native_dimensions);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_Array_delete(native_dimensions, native_dimension_count, &UA_TYPES[UA_TYPES_UINT32]);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_array_dimensions_to_buffer(
      native_dimensions,
      native_dimension_count,
      dimensions,
      dimension_count,
      required_dimension_count_out);
  UA_Array_delete(native_dimensions, native_dimension_count, &UA_TYPES[UA_TYPES_UINT32]);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_array_dimensions(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const unsigned long *dimensions,
    size_t dimension_count,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_UInt32 *native_dimensions;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_make_array_dimensions(dimensions, dimension_count, &native_dimensions);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_writeArrayDimensionsAttribute(
      client->client,
      native_node_id,
      dimension_count,
      native_dimensions);
  free(native_dimensions);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_access_level(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned long *access_level_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Byte access_level;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (access_level_out != NULL) {
    *access_level_out = 0;
  }
  if (client == NULL || client->client == NULL || access_level_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readAccessLevelAttribute(client->client, native_node_id, &access_level);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *access_level_out = (unsigned long)access_level;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_user_access_level(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned long *access_level_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Byte access_level;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (access_level_out != NULL) {
    *access_level_out = 0;
  }
  if (client == NULL || client->client == NULL || access_level_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readUserAccessLevelAttribute(client->client, native_node_id, &access_level);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *access_level_out = (unsigned long)access_level;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_access_level(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned long access_level,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Byte native_access_level;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (access_level > (unsigned long)UCHAR_MAX) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_access_level = (UA_Byte)access_level;
  status = UA_Client_writeAccessLevelAttribute(client->client, native_node_id, &native_access_level);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_access_level_ex(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned long *access_level_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_UInt32 access_level;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (access_level_out != NULL) {
    *access_level_out = 0;
  }
  if (client == NULL || client->client == NULL || access_level_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readAccessLevelExAttribute(client->client, native_node_id, &access_level);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *access_level_out = (unsigned long)access_level;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_access_level_ex(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    unsigned long access_level,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_UInt32 native_access_level;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!cpkt_ulong_fits_uint32(access_level)) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_access_level = (UA_UInt32)access_level;
  status = UA_Client_writeAccessLevelExAttribute(client->client, native_node_id, &native_access_level);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_minimum_sampling_interval(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    double *minimum_sampling_interval_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Double minimum_sampling_interval;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (minimum_sampling_interval_out != NULL) {
    *minimum_sampling_interval_out = 0.0;
  }
  if (client == NULL || client->client == NULL || minimum_sampling_interval_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readMinimumSamplingIntervalAttribute(
      client->client,
      native_node_id,
      &minimum_sampling_interval);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *minimum_sampling_interval_out = (double)minimum_sampling_interval;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_minimum_sampling_interval(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    double minimum_sampling_interval,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Double native_minimum_sampling_interval;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_minimum_sampling_interval = (UA_Double)minimum_sampling_interval;
  status = UA_Client_writeMinimumSamplingIntervalAttribute(
      client->client,
      native_node_id,
      &native_minimum_sampling_interval);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_historizing(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int *historizing_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean historizing;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (historizing_out != NULL) {
    *historizing_out = 0;
  }
  if (client == NULL || client->client == NULL || historizing_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readHistorizingAttribute(client->client, native_node_id, &historizing);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *historizing_out = historizing ? 1 : 0;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_historizing(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int historizing,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean native_historizing;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_historizing = historizing ? true : false;
  status = UA_Client_writeHistorizingAttribute(client->client, native_node_id, &native_historizing);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_executable(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int *executable_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean executable;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (executable_out != NULL) {
    *executable_out = 0;
  }
  if (client == NULL || client->client == NULL || executable_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readExecutableAttribute(client->client, native_node_id, &executable);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *executable_out = executable ? 1 : 0;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_user_executable(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int *executable_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean executable;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (executable_out != NULL) {
    *executable_out = 0;
  }
  if (client == NULL || client->client == NULL || executable_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readUserExecutableAttribute(client->client, native_node_id, &executable);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *executable_out = executable ? 1 : 0;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_executable(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int executable,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean native_executable;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_executable = executable ? true : false;
  status = UA_Client_writeExecutableAttribute(client->client, native_node_id, &native_executable);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_method_argument_count(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id method_node_id,
    int direction,
    size_t *argument_count_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (argument_count_out != NULL) {
    *argument_count_out = 0;
  }
  if (client == NULL || client->client == NULL || argument_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_client_read_method_argument_variant(
      client->client,
      method_node_id,
      direction,
      &variant,
      status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_decode_method_argument_count(&variant, argument_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_method_argument(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id method_node_id,
    int direction,
    size_t argument_index,
    cpkt_opcua_node_id *data_type_out,
    long *value_rank_out,
    char *name_buffer,
    size_t name_buffer_size,
    size_t *required_name_size_out,
    cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (required_name_size_out != NULL) {
    *required_name_size_out = 0;
  }
  if (data_type_out != NULL) {
    *data_type_out = cpkt_opcua_node_id_null();
  }
  if (value_rank_out != NULL) {
    *value_rank_out = 0;
  }
  if (client == NULL || client->client == NULL || data_type_out == NULL || value_rank_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_client_read_method_argument_variant(
      client->client,
      method_node_id,
      direction,
      &variant,
      status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_decode_method_argument(
      &variant,
      argument_index,
      data_type_out,
      value_rank_out,
      name_buffer,
      name_buffer_size,
      required_name_size_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write(
    cpkt_opcua_client *client,
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
  if (client == NULL || client->client == NULL || value == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  UA_Variant_init(&variant);
  result = cpkt_set_variant(&variant, value);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_writeValueAttribute(client->client, native_node_id, &variant);
  UA_Variant_clear(&variant);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_add_object(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    cpkt_opcua_status *status_out) {
  UA_ObjectAttributes attr;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || browse_name == NULL ||
      !cpkt_valid_node_id(node_id) || !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  attr = UA_ObjectAttributes_default;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)(display_name != NULL ? display_name : browse_name));
  attr.writeMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION | UA_WRITEMASK_EVENTNOTIFIER;
  attr.userWriteMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION | UA_WRITEMASK_EVENTNOTIFIER;
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  status = UA_Client_addObjectNode(
      client->client,
      requested_node_id,
      parent_native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
      attr,
      NULL);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_add_variable(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const char *browse_name,
    const char *display_name,
    const cpkt_opcua_value *value,
    cpkt_opcua_status *status_out) {
  return cpkt_opcua_client_add_variable_under(
      client,
      node_id,
      cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
      browse_name,
      display_name,
      value,
      status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_add_variable_under(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    const cpkt_opcua_value *value,
    cpkt_opcua_status *status_out) {
  UA_VariableAttributes attr;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || browse_name == NULL || value == NULL ||
      !cpkt_valid_node_id(node_id) || !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.writeMask = UA_WRITEMASK_ACCESSLEVEL | UA_WRITEMASK_HISTORIZING | UA_WRITEMASK_DISPLAYNAME |
                   UA_WRITEMASK_DESCRIPTION | UA_WRITEMASK_VALUERANK | UA_WRITEMASK_ARRAYDIMENSIONS;
  attr.userWriteMask = UA_WRITEMASK_ACCESSLEVEL | UA_WRITEMASK_HISTORIZING | UA_WRITEMASK_DISPLAYNAME |
                       UA_WRITEMASK_DESCRIPTION | UA_WRITEMASK_VALUERANK | UA_WRITEMASK_ARRAYDIMENSIONS;
  attr.dataType = cpkt_data_type_node_id_for_value_type(value->type);
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)(display_name != NULL ? display_name : browse_name));
  result = cpkt_apply_value_shape_to_variable_attributes(&attr, value);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_set_variant(&attr.value, value);
  if (result != CPKT_OPCUA_OK) {
    UA_Array_delete(attr.arrayDimensions, attr.arrayDimensionsSize, &UA_TYPES[UA_TYPES_UINT32]);
    return result;
  }
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  status = UA_Client_addVariableNode(
      client->client,
      requested_node_id,
      parent_native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
      attr,
      NULL);
  UA_Variant_clear(&attr.value);
  UA_Array_delete(attr.arrayDimensions, attr.arrayDimensionsSize, &UA_TYPES[UA_TYPES_UINT32]);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_add_object_type(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    int is_abstract,
    cpkt_opcua_status *status_out) {
  UA_ObjectTypeAttributes attr;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || browse_name == NULL ||
      !cpkt_valid_node_id(node_id) || !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  attr = UA_ObjectTypeAttributes_default;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)(display_name != NULL ? display_name : browse_name));
  attr.writeMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION | UA_WRITEMASK_ISABSTRACT;
  attr.userWriteMask = attr.writeMask;
  attr.isAbstract = is_abstract ? true : false;
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  status = UA_Client_addObjectTypeNode(
      client->client,
      requested_node_id,
      parent_native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      attr,
      NULL);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_add_variable_type(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    const cpkt_opcua_value *value,
    int is_abstract,
    cpkt_opcua_status *status_out) {
  UA_VariableTypeAttributes attr;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || browse_name == NULL || value == NULL ||
      !cpkt_valid_node_id(node_id) || !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  attr = UA_VariableTypeAttributes_default;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)(display_name != NULL ? display_name : browse_name));
  attr.writeMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION | UA_WRITEMASK_ISABSTRACT;
  attr.userWriteMask = attr.writeMask;
  attr.dataType = cpkt_data_type_node_id_for_value_type(value->type);
  attr.isAbstract = is_abstract ? true : false;
  result = cpkt_apply_value_shape_to_variable_type_attributes(&attr, value);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_set_variant(&attr.value, value);
  if (result != CPKT_OPCUA_OK) {
    UA_Array_delete(attr.arrayDimensions, attr.arrayDimensionsSize, &UA_TYPES[UA_TYPES_UINT32]);
    return result;
  }
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  status = UA_Client_addVariableTypeNode(
      client->client,
      requested_node_id,
      parent_native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      attr,
      NULL);
  UA_Variant_clear(&attr.value);
  UA_Array_delete(attr.arrayDimensions, attr.arrayDimensionsSize, &UA_TYPES[UA_TYPES_UINT32]);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_add_reference_type(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    const char *inverse_name,
    int is_abstract,
    int symmetric,
    cpkt_opcua_status *status_out) {
  UA_ReferenceTypeAttributes attr;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || browse_name == NULL || inverse_name == NULL ||
      !cpkt_valid_node_id(node_id) || !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  attr = UA_ReferenceTypeAttributes_default;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)(display_name != NULL ? display_name : browse_name));
  attr.inverseName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)inverse_name);
  attr.writeMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION | UA_WRITEMASK_INVERSENAME |
                   UA_WRITEMASK_ISABSTRACT;
  attr.userWriteMask = attr.writeMask;
  attr.isAbstract = is_abstract ? true : false;
  attr.symmetric = symmetric ? true : false;
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  status = UA_Client_addReferenceTypeNode(
      client->client,
      requested_node_id,
      parent_native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      attr,
      NULL);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_add_data_type(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    int is_abstract,
    cpkt_opcua_status *status_out) {
  UA_DataTypeAttributes attr;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || browse_name == NULL ||
      !cpkt_valid_node_id(node_id) || !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  attr = UA_DataTypeAttributes_default;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)(display_name != NULL ? display_name : browse_name));
  attr.writeMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION | UA_WRITEMASK_ISABSTRACT;
  attr.userWriteMask = attr.writeMask;
  attr.isAbstract = is_abstract ? true : false;
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  status = UA_Client_addDataTypeNode(
      client->client,
      requested_node_id,
      parent_native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      attr,
      NULL);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_add_view(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    int contains_no_loops,
    unsigned long event_notifier,
    cpkt_opcua_status *status_out) {
  UA_ViewAttributes attr;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || browse_name == NULL ||
      !cpkt_valid_node_id(node_id) || !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (event_notifier > (unsigned long)UCHAR_MAX) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  attr = UA_ViewAttributes_default;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)(display_name != NULL ? display_name : browse_name));
  attr.writeMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION | UA_WRITEMASK_EVENTNOTIFIER;
  attr.userWriteMask = attr.writeMask;
  attr.containsNoLoops = contains_no_loops ? true : false;
  attr.eventNotifier = (UA_Byte)event_notifier;
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  status = UA_Client_addViewNode(
      client->client,
      requested_node_id,
      parent_native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      attr,
      NULL);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_delete_node(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    int delete_target_refs,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_deleteNode(client->client, native_node_id, delete_target_refs ? true : false);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_add_reference(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id source_node_id,
    cpkt_opcua_node_id reference_type_id,
    int is_forward,
    cpkt_opcua_node_id target_node_id,
    unsigned long target_node_class,
    cpkt_opcua_status *status_out) {
  return cpkt_opcua_client_add_reference_ex(
      client,
      source_node_id,
      reference_type_id,
      is_forward,
      cpkt_opcua_expanded_node_id_local(target_node_id),
      target_node_class,
      status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_add_reference_ex(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id source_node_id,
    cpkt_opcua_node_id reference_type_id,
    int is_forward,
    cpkt_opcua_expanded_node_id target_node_id,
    unsigned long target_node_class,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_source_node_id;
  UA_NodeId native_reference_type_id;
  UA_ExpandedNodeId native_target_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      !cpkt_valid_node_id(source_node_id) || !cpkt_valid_node_id(reference_type_id) ||
      !cpkt_valid_expanded_node_id(target_node_id) || !cpkt_valid_node_class(target_node_class)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_source_node_id = cpkt_make_node_id(source_node_id);
  native_reference_type_id = cpkt_make_node_id(reference_type_id);
  native_target_node_id = cpkt_make_expanded_node_id(target_node_id);
  status = UA_Client_addReference(
      client->client,
      native_source_node_id,
      native_reference_type_id,
      is_forward ? true : false,
      UA_STRING_NULL,
      native_target_node_id,
      (UA_NodeClass)target_node_class);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_delete_reference(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id source_node_id,
    cpkt_opcua_node_id reference_type_id,
    int is_forward,
    cpkt_opcua_node_id target_node_id,
    int delete_bidirectional,
    cpkt_opcua_status *status_out) {
  return cpkt_opcua_client_delete_reference_ex(
      client,
      source_node_id,
      reference_type_id,
      is_forward,
      cpkt_opcua_expanded_node_id_local(target_node_id),
      delete_bidirectional,
      status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_delete_reference_ex(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id source_node_id,
    cpkt_opcua_node_id reference_type_id,
    int is_forward,
    cpkt_opcua_expanded_node_id target_node_id,
    int delete_bidirectional,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_source_node_id;
  UA_NodeId native_reference_type_id;
  UA_ExpandedNodeId native_target_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      !cpkt_valid_node_id(source_node_id) || !cpkt_valid_node_id(reference_type_id) ||
      !cpkt_valid_expanded_node_id(target_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_source_node_id = cpkt_make_node_id(source_node_id);
  native_reference_type_id = cpkt_make_node_id(reference_type_id);
  native_target_node_id = cpkt_make_expanded_node_id(target_node_id);
  status = UA_Client_deleteReference(
      client->client,
      native_source_node_id,
      native_reference_type_id,
      is_forward ? true : false,
      native_target_node_id,
      delete_bidirectional ? true : false);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_browse_children(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id parent_node_id,
    cpkt_opcua_browse_fn fn,
    void *user,
    cpkt_opcua_status *status_out) {
  cpkt_opcua_browse_options options;

  cpkt_opcua_browse_options_default(&options);
  return cpkt_opcua_client_browse_children_ex(client, parent_node_id, &options, fn, user, status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_browse_children_ex(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id parent_node_id,
    const cpkt_opcua_browse_options *options,
    cpkt_opcua_browse_fn fn,
    void *user,
    cpkt_opcua_status *status_out) {
  UA_BrowseDescription browse_description;
  UA_BrowseResult browse_result;
  cpkt_opcua_result result;
  unsigned long max_references;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || fn == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_fill_browse_description(&browse_description, parent_node_id, options);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  max_references = options != NULL ? options->max_references : 0;
  browse_result = UA_Client_browse(client->client, NULL, (UA_UInt32)max_references, &browse_description);
  result = cpkt_browse_result_each(&browse_result, fn, user, status_out);
  UA_BrowseResult_clear(&browse_result);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_browse_children_page(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id parent_node_id,
    const cpkt_opcua_browse_options *options,
    cpkt_opcua_browse_fn fn,
    void *user,
    unsigned char *continuation_point_buffer,
    size_t continuation_point_buffer_size,
    size_t *required_continuation_point_size_out,
    cpkt_opcua_status *status_out) {
  UA_BrowseDescription browse_description;
  UA_BrowseResult browse_result;
  cpkt_opcua_result result;
  unsigned long max_references;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_continuation_point_size_out != NULL) {
    *required_continuation_point_size_out = 0;
  }
  if (client == NULL || client->client == NULL || fn == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_fill_browse_description(&browse_description, parent_node_id, options);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  max_references = options != NULL ? options->max_references : 0;
  browse_result = UA_Client_browse(client->client, NULL, (UA_UInt32)max_references, &browse_description);
  result = cpkt_browse_result_page(
      &browse_result,
      fn,
      user,
      continuation_point_buffer,
      continuation_point_buffer_size,
      required_continuation_point_size_out,
      status_out);
  if (result == CPKT_OPCUA_ERR_RANGE) {
    cpkt_client_release_browse_continuation_point(client->client, browse_result.continuationPoint);
  }
  UA_BrowseResult_clear(&browse_result);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_browse_next(
    cpkt_opcua_client *client,
    const unsigned char *continuation_point,
    size_t continuation_point_size,
    int release_continuation_point,
    cpkt_opcua_browse_fn fn,
    void *user,
    unsigned char *next_continuation_point_buffer,
    size_t next_continuation_point_buffer_size,
    size_t *required_next_continuation_point_size_out,
    cpkt_opcua_status *status_out) {
  UA_ByteString native_continuation_point;
  UA_BrowseResult browse_result;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_next_continuation_point_size_out != NULL) {
    *required_next_continuation_point_size_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      (continuation_point_size != 0 && continuation_point == NULL) ||
      (!release_continuation_point && fn == NULL)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_continuation_point.length = continuation_point_size;
  native_continuation_point.data = (UA_Byte *)continuation_point;
  browse_result = UA_Client_browseNext(
      client->client,
      release_continuation_point ? true : false,
      native_continuation_point);
  result = cpkt_browse_result_page(
      &browse_result,
      release_continuation_point ? NULL : fn,
      user,
      next_continuation_point_buffer,
      next_continuation_point_buffer_size,
      required_next_continuation_point_size_out,
      status_out);
  if (result == CPKT_OPCUA_ERR_RANGE) {
    cpkt_client_release_browse_continuation_point(client->client, browse_result.continuationPoint);
  }
  UA_BrowseResult_clear(&browse_result);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_translate_browse_path(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id start_node_id,
    const cpkt_opcua_browse_path_element *elements,
    size_t element_count,
    cpkt_opcua_node_id *target_node_id_out,
    char *target_buffer,
    size_t target_buffer_size,
    size_t *required_target_size_out,
    cpkt_opcua_status *status_out) {
  UA_BrowsePath browse_path;
  UA_BrowsePathResult browse_path_result;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || target_node_id_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_fill_browse_path(&browse_path, start_node_id, elements, element_count);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  browse_path_result = UA_Client_translateBrowsePathToNodeIds(client->client, &browse_path);
  result = cpkt_translate_browse_path_result_to_target(
      &browse_path_result,
      target_node_id_out,
      target_buffer,
      target_buffer_size,
      required_target_size_out,
      status_out);
  UA_BrowsePathResult_clear(&browse_path_result);
  UA_BrowsePath_clear(&browse_path);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_call_method(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id object_node_id,
    cpkt_opcua_node_id method_node_id,
    const cpkt_opcua_value *inputs,
    size_t input_count,
    cpkt_opcua_value *output,
    char *string_buffer,
    size_t string_buffer_size,
    size_t *required_string_size_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_object_node_id;
  UA_NodeId native_method_node_id;
  UA_Variant *input_variants;
  UA_Variant *output_variants;
  size_t output_count;
  UA_StatusCode status;
  cpkt_opcua_result result;
  size_t i;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_string_size_out != NULL) {
    *required_string_size_out = 0;
  }
  if (client == NULL || client->client == NULL || output == NULL ||
      (input_count != 0 && inputs == NULL) ||
      !cpkt_valid_node_id(object_node_id) || !cpkt_valid_node_id(method_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  input_variants = NULL;
  if (input_count != 0) {
    input_variants = (UA_Variant *)calloc(input_count, sizeof(*input_variants));
    if (input_variants == NULL) {
      return CPKT_OPCUA_ERR_ALLOC;
    }
    result = cpkt_set_variant_array(input_variants, inputs, input_count);
    if (result != CPKT_OPCUA_OK) {
      free(input_variants);
      return result;
    }
  }
  native_object_node_id = cpkt_make_node_id(object_node_id);
  native_method_node_id = cpkt_make_node_id(method_node_id);
  output_count = 0;
  output_variants = NULL;
  status = UA_Client_call(
      client->client,
      native_object_node_id,
      native_method_node_id,
      input_count,
      input_variants,
      &output_count,
      &output_variants);
  for (i = 0; i < input_count; ++i) {
    UA_Variant_clear(&input_variants[i]);
  }
  free(input_variants);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    if (output_variants != NULL) {
      UA_Array_delete(output_variants, output_count, &UA_TYPES[UA_TYPES_VARIANT]);
    }
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (output_count != 1 || output_variants == NULL) {
    if (output_variants != NULL) {
      UA_Array_delete(output_variants, output_count, &UA_TYPES[UA_TYPES_VARIANT]);
    }
    return CPKT_OPCUA_ERR_TYPE;
  }
  result = cpkt_get_variant(
      &output_variants[0],
      output,
      string_buffer,
      string_buffer_size,
      required_string_size_out);
  UA_Array_delete(output_variants, output_count, &UA_TYPES[UA_TYPES_VARIANT]);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_call_method_many(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id object_node_id,
    cpkt_opcua_node_id method_node_id,
    const cpkt_opcua_value *inputs,
    size_t input_count,
    cpkt_opcua_value *outputs,
    size_t expected_output_count,
    char **string_buffers,
    const size_t *string_buffer_sizes,
    size_t *required_string_sizes_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_object_node_id;
  UA_NodeId native_method_node_id;
  UA_Variant *input_variants;
  UA_Variant *output_variants;
  size_t output_count;
  UA_StatusCode status;
  cpkt_opcua_result result;
  cpkt_opcua_result output_result;
  char *string_buffer;
  size_t string_buffer_size;
  size_t i;

  if (status_out != NULL) {
    *status_out = 0;
  }
  for (i = 0; required_string_sizes_out != NULL && i < expected_output_count; ++i) {
    required_string_sizes_out[i] = 0;
  }
  if (client == NULL || client->client == NULL || outputs == NULL || expected_output_count == 0 ||
      (input_count != 0 && inputs == NULL) ||
      !cpkt_valid_node_id(object_node_id) || !cpkt_valid_node_id(method_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  input_variants = NULL;
  if (input_count != 0) {
    input_variants = (UA_Variant *)calloc(input_count, sizeof(*input_variants));
    if (input_variants == NULL) {
      return CPKT_OPCUA_ERR_ALLOC;
    }
    result = cpkt_set_variant_array(input_variants, inputs, input_count);
    if (result != CPKT_OPCUA_OK) {
      free(input_variants);
      return result;
    }
  }
  native_object_node_id = cpkt_make_node_id(object_node_id);
  native_method_node_id = cpkt_make_node_id(method_node_id);
  output_count = 0;
  output_variants = NULL;
  status = UA_Client_call(
      client->client,
      native_object_node_id,
      native_method_node_id,
      input_count,
      input_variants,
      &output_count,
      &output_variants);
  for (i = 0; i < input_count; ++i) {
    UA_Variant_clear(&input_variants[i]);
  }
  free(input_variants);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    if (output_variants != NULL) {
      UA_Array_delete(output_variants, output_count, &UA_TYPES[UA_TYPES_VARIANT]);
    }
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (output_count != expected_output_count || output_variants == NULL) {
    if (output_variants != NULL) {
      UA_Array_delete(output_variants, output_count, &UA_TYPES[UA_TYPES_VARIANT]);
    }
    return CPKT_OPCUA_ERR_TYPE;
  }
  result = CPKT_OPCUA_OK;
  for (i = 0; i < output_count; ++i) {
    string_buffer = string_buffers != NULL ? string_buffers[i] : NULL;
    string_buffer_size = string_buffer_sizes != NULL ? string_buffer_sizes[i] : 0;
    output_result = cpkt_get_variant(
        &output_variants[i],
        &outputs[i],
        string_buffer,
        string_buffer_size,
        required_string_sizes_out != NULL ? &required_string_sizes_out[i] : NULL);
    if (output_result != CPKT_OPCUA_OK && result == CPKT_OPCUA_OK) {
      result = output_result;
    }
  }
  UA_Array_delete(output_variants, output_count, &UA_TYPES[UA_TYPES_VARIANT]);
  return result;
}

static void cpkt_async_read_value_callback(
    UA_Client *native_client,
    void *userdata,
    UA_UInt32 request_id,
    UA_StatusCode status,
    UA_DataValue *value) {
  struct cpkt_opcua_async_context *context;
  cpkt_opcua_value out;
  cpkt_opcua_result result;

  (void)native_client;
  context = (struct cpkt_opcua_async_context *)userdata;
  cpkt_opcua_value_clear(&out);
  result = status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
  if (result == CPKT_OPCUA_OK) {
    if (value == NULL || !value->hasValue) {
      result = CPKT_OPCUA_ERR_TYPE;
    } else {
      result = cpkt_get_variant(
          &value->value,
          &out,
          context->string_buffer,
          context->string_buffer_size,
          context->required_string_size_out);
    }
  }
  context->value_fn((unsigned long)request_id, result, result == CPKT_OPCUA_OK ? &out : NULL,
      cpkt_status(status), context->user);
  cpkt_async_finish(context);
}

static void cpkt_async_write_callback(
    UA_Client *native_client,
    void *userdata,
    UA_UInt32 request_id,
    UA_WriteResponse *response) {
  struct cpkt_opcua_async_context *context;
  UA_StatusCode status;
  cpkt_opcua_result result;

  (void)native_client;
  context = (struct cpkt_opcua_async_context *)userdata;
  status = response != NULL ? response->responseHeader.serviceResult : UA_STATUSCODE_BADUNEXPECTEDERROR;
  if (status == UA_STATUSCODE_GOOD && response != NULL && response->resultsSize != 0) {
    status = response->results[0];
  }
  result = status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
  context->status_fn((unsigned long)request_id, result, cpkt_status(status), context->user);
  cpkt_async_finish(context);
}

static void cpkt_async_browse_callback(
    UA_Client *native_client,
    void *userdata,
    UA_UInt32 request_id,
    UA_BrowseResponse *response) {
  struct cpkt_opcua_async_context *context;
  UA_StatusCode status;
  cpkt_opcua_status browse_status;
  cpkt_opcua_result result;

  (void)native_client;
  context = (struct cpkt_opcua_async_context *)userdata;
  status = response != NULL ? response->responseHeader.serviceResult : UA_STATUSCODE_BADUNEXPECTEDERROR;
  result = status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
  if (result == CPKT_OPCUA_OK) {
    if (response == NULL || response->resultsSize != 1) {
      result = CPKT_OPCUA_ERR_TYPE;
    } else {
      browse_status = 0;
      result = cpkt_browse_result_each(
          &response->results[0],
          context->browse_fn,
          context->user,
          &browse_status);
      status = (UA_StatusCode)browse_status;
    }
  }
  context->browse_done_fn((unsigned long)request_id, result, cpkt_status(status), context->user);
  cpkt_async_finish(context);
}

static void cpkt_async_call_callback(
    UA_Client *native_client,
    void *userdata,
    UA_UInt32 request_id,
    UA_CallResponse *response) {
  struct cpkt_opcua_async_context *context;
  UA_StatusCode status;
  cpkt_opcua_result result;
  cpkt_opcua_result output_result;
  UA_CallMethodResult *method_result;
  char *string_buffer;
  size_t string_buffer_size;
  size_t i;

  (void)native_client;
  context = (struct cpkt_opcua_async_context *)userdata;
  status = response != NULL ? response->responseHeader.serviceResult : UA_STATUSCODE_BADUNEXPECTEDERROR;
  result = status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
  if (result == CPKT_OPCUA_OK) {
    if (response == NULL || response->resultsSize != 1 || response->results == NULL) {
      result = CPKT_OPCUA_ERR_TYPE;
    } else {
      method_result = &response->results[0];
      status = method_result->statusCode;
      result = status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
      if (result == CPKT_OPCUA_OK) {
        if (method_result->outputArgumentsSize != context->expected_output_count ||
            (context->expected_output_count != 0 && method_result->outputArguments == NULL)) {
          result = CPKT_OPCUA_ERR_TYPE;
        } else {
          for (i = 0; i < context->expected_output_count; ++i) {
            string_buffer = context->string_buffers != NULL ? context->string_buffers[i] : NULL;
            string_buffer_size =
                context->string_buffer_sizes != NULL ? context->string_buffer_sizes[i] : 0;
            output_result = cpkt_get_variant(
                &method_result->outputArguments[i],
                &context->outputs[i],
                string_buffer,
                string_buffer_size,
                context->required_string_sizes_out != NULL ?
                    &context->required_string_sizes_out[i] : NULL);
            if (output_result != CPKT_OPCUA_OK && result == CPKT_OPCUA_OK) {
              result = output_result;
            }
          }
        }
      }
    }
  }
  context->call_fn(
      (unsigned long)request_id,
      result,
      result == CPKT_OPCUA_OK ? context->outputs : NULL,
      result == CPKT_OPCUA_OK ? context->expected_output_count : 0,
      cpkt_status(status),
      context->user);
  cpkt_async_finish(context);
}

static void cpkt_async_add_node_callback(
    UA_Client *native_client,
    void *userdata,
    UA_UInt32 request_id,
  UA_AddNodesResponse *response) {
  struct cpkt_opcua_async_context *context;
  struct cpkt_owned_node_id_memory owned;
  cpkt_opcua_node_id node_id;
  UA_StatusCode status;
  cpkt_opcua_result result;

  (void)native_client;
  context = (struct cpkt_opcua_async_context *)userdata;
  status = response != NULL ? response->responseHeader.serviceResult : UA_STATUSCODE_BADUNEXPECTEDERROR;
  result = status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
  if (result == CPKT_OPCUA_OK) {
    if (response == NULL || response->resultsSize != 1 || response->results == NULL) {
      result = CPKT_OPCUA_ERR_TYPE;
    } else {
      status = response->results[0].statusCode;
      result = status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
      if (result == CPKT_OPCUA_OK) {
        if (!cpkt_native_node_id_to_facade(&response->results[0].addedNodeId, &node_id, &owned)) {
          result = CPKT_OPCUA_ERR_ALLOC;
        } else {
          result = cpkt_copy_node_id_to_caller(
              &node_id,
              &owned,
              context->node_id_out,
              context->node_id_buffer,
              context->node_id_buffer_size,
              context->required_node_id_size_out);
          cpkt_owned_node_id_memory_clear(&owned);
        }
      }
    }
  }
  context->node_fn(
      (unsigned long)request_id,
      result,
      result == CPKT_OPCUA_OK ? context->node_id_out : NULL,
      cpkt_status(status),
      context->user);
  cpkt_async_finish(context);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_async(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_async_value_fn fn,
    void *user,
    cpkt_opcua_request_id *request_id_out,
    char *string_buffer,
    size_t string_buffer_size,
    size_t *required_string_size_out,
    cpkt_opcua_status *status_out) {
  struct cpkt_opcua_async_context *context;
  UA_NodeId native_node_id;
  UA_UInt32 request_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (request_id_out != NULL) {
    *request_id_out = 0;
  }
  if (required_string_size_out != NULL) {
    *required_string_size_out = 0;
  }
  if (client == NULL || client->client == NULL || fn == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  context = (struct cpkt_opcua_async_context *)calloc(1, sizeof(*context));
  if (context == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->value_fn = fn;
  context->user = user;
  context->string_buffer = string_buffer;
  context->string_buffer_size = string_buffer_size;
  context->required_string_size_out = required_string_size_out;
  cpkt_async_link(client, context);
  native_node_id = cpkt_make_node_id(node_id);
  request_id = 0;
  status = UA_Client_readValueAttribute_async(
      client->client,
      native_node_id,
      cpkt_async_read_value_callback,
      context,
      &request_id);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    cpkt_async_finish(context);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (request_id_out != NULL) {
    *request_id_out = (cpkt_opcua_request_id)request_id;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_async(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    const cpkt_opcua_value *value,
    cpkt_opcua_async_status_fn fn,
    void *user,
    cpkt_opcua_request_id *request_id_out,
    cpkt_opcua_status *status_out) {
  struct cpkt_opcua_async_context *context;
  UA_NodeId native_node_id;
  UA_Variant variant;
  UA_UInt32 request_id;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (request_id_out != NULL) {
    *request_id_out = 0;
  }
  if (client == NULL || client->client == NULL || value == NULL || fn == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  UA_Variant_init(&variant);
  result = cpkt_set_variant(&variant, value);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  context = (struct cpkt_opcua_async_context *)calloc(1, sizeof(*context));
  if (context == NULL) {
    UA_Variant_clear(&variant);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->status_fn = fn;
  context->user = user;
  cpkt_async_link(client, context);
  native_node_id = cpkt_make_node_id(node_id);
  request_id = 0;
  status = UA_Client_writeValueAttribute_async(
      client->client,
      native_node_id,
      &variant,
      cpkt_async_write_callback,
      context,
      &request_id);
  UA_Variant_clear(&variant);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    cpkt_async_finish(context);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (request_id_out != NULL) {
    *request_id_out = (cpkt_opcua_request_id)request_id;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_browse_children_async(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id parent_node_id,
    const cpkt_opcua_browse_options *options,
    cpkt_opcua_browse_fn browse_fn,
    cpkt_opcua_async_browse_fn done_fn,
    void *user,
    cpkt_opcua_request_id *request_id_out,
    cpkt_opcua_status *status_out) {
  struct cpkt_opcua_async_context *context;
  UA_BrowseRequest request;
  UA_UInt32 request_id;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (request_id_out != NULL) {
    *request_id_out = 0;
  }
  if (client == NULL || client->client == NULL || browse_fn == NULL || done_fn == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  UA_BrowseRequest_init(&request);
  request.nodesToBrowse = (UA_BrowseDescription *)UA_calloc(1, sizeof(*request.nodesToBrowse));
  if (request.nodesToBrowse == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  request.nodesToBrowseSize = 1;
  result = cpkt_fill_browse_description(&request.nodesToBrowse[0], parent_node_id, options);
  if (result != CPKT_OPCUA_OK) {
    UA_BrowseRequest_clear(&request);
    return result;
  }
  request.requestedMaxReferencesPerNode =
      options != NULL ? (UA_UInt32)options->max_references : (UA_UInt32)0;
  context = (struct cpkt_opcua_async_context *)calloc(1, sizeof(*context));
  if (context == NULL) {
    UA_BrowseRequest_clear(&request);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->browse_fn = browse_fn;
  context->browse_done_fn = done_fn;
  context->user = user;
  cpkt_async_link(client, context);
  request_id = 0;
  status = UA_Client_sendAsyncBrowseRequest(
      client->client,
      &request,
      cpkt_async_browse_callback,
      context,
      &request_id);
  UA_BrowseRequest_clear(&request);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    cpkt_async_finish(context);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (request_id_out != NULL) {
    *request_id_out = (cpkt_opcua_request_id)request_id;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_call_method_async(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id object_node_id,
    cpkt_opcua_node_id method_node_id,
    const cpkt_opcua_value *inputs,
    size_t input_count,
    size_t expected_output_count,
    cpkt_opcua_async_call_fn fn,
    void *user,
    cpkt_opcua_request_id *request_id_out,
    cpkt_opcua_value *outputs,
    char **string_buffers,
    const size_t *string_buffer_sizes,
    size_t *required_string_sizes_out,
    cpkt_opcua_status *status_out) {
  struct cpkt_opcua_async_context *context;
  UA_NodeId native_object_node_id;
  UA_NodeId native_method_node_id;
  UA_Variant *input_variants;
  UA_UInt32 request_id;
  UA_StatusCode status;
  cpkt_opcua_result result;
  size_t i;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (request_id_out != NULL) {
    *request_id_out = 0;
  }
  for (i = 0; required_string_sizes_out != NULL && i < expected_output_count; ++i) {
    required_string_sizes_out[i] = 0;
  }
  if (client == NULL || client->client == NULL || fn == NULL ||
      (expected_output_count != 0 && outputs == NULL) ||
      (input_count != 0 && inputs == NULL) ||
      !cpkt_valid_node_id(object_node_id) || !cpkt_valid_node_id(method_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  input_variants = NULL;
  if (input_count != 0) {
    input_variants = (UA_Variant *)calloc(input_count, sizeof(*input_variants));
    if (input_variants == NULL) {
      return CPKT_OPCUA_ERR_ALLOC;
    }
    result = cpkt_set_variant_array(input_variants, inputs, input_count);
    if (result != CPKT_OPCUA_OK) {
      free(input_variants);
      return result;
    }
  }
  context = (struct cpkt_opcua_async_context *)calloc(1, sizeof(*context));
  if (context == NULL) {
    for (i = 0; i < input_count; ++i) {
      UA_Variant_clear(&input_variants[i]);
    }
    free(input_variants);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->call_fn = fn;
  context->user = user;
  context->outputs = outputs;
  context->expected_output_count = expected_output_count;
  context->string_buffers = string_buffers;
  context->string_buffer_sizes = string_buffer_sizes;
  context->required_string_sizes_out = required_string_sizes_out;
  cpkt_async_link(client, context);
  native_object_node_id = cpkt_make_node_id(object_node_id);
  native_method_node_id = cpkt_make_node_id(method_node_id);
  request_id = 0;
  status = UA_Client_call_async(
      client->client,
      native_object_node_id,
      native_method_node_id,
      input_count,
      input_variants,
      cpkt_async_call_callback,
      context,
      &request_id);
  for (i = 0; i < input_count; ++i) {
    UA_Variant_clear(&input_variants[i]);
  }
  free(input_variants);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    cpkt_async_finish(context);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (request_id_out != NULL) {
    *request_id_out = (cpkt_opcua_request_id)request_id;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_add_object_async(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    cpkt_opcua_async_node_fn fn,
    void *user,
    cpkt_opcua_request_id *request_id_out,
    cpkt_opcua_node_id *node_id_out,
    char *node_id_buffer,
    size_t node_id_buffer_size,
    size_t *required_node_id_size_out,
    cpkt_opcua_status *status_out) {
  struct cpkt_opcua_async_context *context;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_NodeId reference_type_id;
  UA_NodeId type_definition;
  UA_ObjectAttributes attr;
  UA_UInt32 request_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (request_id_out != NULL) {
    *request_id_out = 0;
  }
  if (required_node_id_size_out != NULL) {
    *required_node_id_size_out = 0;
  }
  if (client == NULL || client->client == NULL || browse_name == NULL || fn == NULL ||
      node_id_out == NULL || !cpkt_valid_node_id(node_id) || !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  context = (struct cpkt_opcua_async_context *)calloc(1, sizeof(*context));
  if (context == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->node_fn = fn;
  context->user = user;
  context->node_id_out = node_id_out;
  context->node_id_buffer = node_id_buffer;
  context->node_id_buffer_size = node_id_buffer_size;
  context->required_node_id_size_out = required_node_id_size_out;
  cpkt_async_link(client, context);
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  reference_type_id = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
  type_definition = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE);
  attr = UA_ObjectAttributes_default;
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)(display_name != NULL ? display_name : browse_name));
  request_id = 0;
  status = UA_Client_addObjectNode_async(
      client->client,
      requested_node_id,
      parent_native_node_id,
      reference_type_id,
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      type_definition,
      attr,
      NULL,
      cpkt_async_add_node_callback,
      context,
      &request_id);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    cpkt_async_finish(context);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (request_id_out != NULL) {
    *request_id_out = (cpkt_opcua_request_id)request_id;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_add_variable_async(
    cpkt_opcua_client *client,
    cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id,
    const char *browse_name,
    const char *display_name,
    const cpkt_opcua_value *value,
    cpkt_opcua_async_node_fn fn,
    void *user,
    cpkt_opcua_request_id *request_id_out,
    cpkt_opcua_node_id *node_id_out,
    char *node_id_buffer,
    size_t node_id_buffer_size,
    size_t *required_node_id_size_out,
    cpkt_opcua_status *status_out) {
  struct cpkt_opcua_async_context *context;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_NodeId reference_type_id;
  UA_NodeId type_definition;
  UA_VariableAttributes attr;
  UA_UInt32 request_id;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (request_id_out != NULL) {
    *request_id_out = 0;
  }
  if (required_node_id_size_out != NULL) {
    *required_node_id_size_out = 0;
  }
  if (client == NULL || client->client == NULL || browse_name == NULL || value == NULL ||
      fn == NULL || node_id_out == NULL || !cpkt_valid_node_id(node_id) ||
      !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  attr = UA_VariableAttributes_default;
  result = cpkt_set_variant(&attr.value, value);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  context = (struct cpkt_opcua_async_context *)calloc(1, sizeof(*context));
  if (context == NULL) {
    UA_Variant_clear(&attr.value);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->node_fn = fn;
  context->user = user;
  context->node_id_out = node_id_out;
  context->node_id_buffer = node_id_buffer;
  context->node_id_buffer_size = node_id_buffer_size;
  context->required_node_id_size_out = required_node_id_size_out;
  cpkt_async_link(client, context);
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  reference_type_id = UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT);
  type_definition = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE);
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)(display_name != NULL ? display_name : browse_name));
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  request_id = 0;
  status = UA_Client_addVariableNode_async(
      client->client,
      requested_node_id,
      parent_native_node_id,
      reference_type_id,
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      type_definition,
      attr,
      NULL,
      cpkt_async_add_node_callback,
      context,
      &request_id);
  UA_Variant_clear(&attr.value);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    cpkt_async_finish(context);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (request_id_out != NULL) {
    *request_id_out = (cpkt_opcua_request_id)request_id;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_create_subscription(
    cpkt_opcua_client *client,
    double publishing_interval_ms,
    cpkt_opcua_subscription_id *subscription_id_out,
    cpkt_opcua_status *status_out) {
  UA_CreateSubscriptionRequest request;
  UA_CreateSubscriptionResponse response;
  UA_StatusCode service_result;
  UA_UInt32 subscription_id;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (subscription_id_out != NULL) {
    *subscription_id_out = 0;
  }
  if (client == NULL || client->client == NULL || publishing_interval_ms < 0.0) {
    return CPKT_OPCUA_ERR_ARG;
  }
  request = UA_CreateSubscriptionRequest_default();
  if (publishing_interval_ms > 0.0) {
    request.requestedPublishingInterval = publishing_interval_ms;
  }
  response = UA_Client_Subscriptions_create(client->client, request, NULL, NULL, NULL);
  service_result = response.responseHeader.serviceResult;
  subscription_id = response.subscriptionId;
  if (status_out != NULL) {
    *status_out = cpkt_status(service_result);
  }
  UA_CreateSubscriptionResponse_clear(&response);
  if (service_result != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (subscription_id_out != NULL) {
    *subscription_id_out = (cpkt_opcua_subscription_id)subscription_id;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_delete_subscription(
    cpkt_opcua_client *client,
    cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_status *status_out) {
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || subscription_id > (unsigned long)UINT_MAX) {
    return CPKT_OPCUA_ERR_ARG;
  }
  status = UA_Client_Subscriptions_deleteSingle(client->client, (UA_UInt32)subscription_id);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_modify_subscription(
    cpkt_opcua_client *client,
    cpkt_opcua_subscription_id subscription_id,
    double publishing_interval_ms,
    cpkt_opcua_status *status_out) {
  UA_ModifySubscriptionRequest request;
  UA_ModifySubscriptionResponse response;
  UA_StatusCode service_result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || subscription_id > (unsigned long)UINT_MAX ||
      publishing_interval_ms < 0.0) {
    return CPKT_OPCUA_ERR_ARG;
  }
  UA_ModifySubscriptionRequest_init(&request);
  request.subscriptionId = (UA_UInt32)subscription_id;
  request.requestedPublishingInterval = publishing_interval_ms;
  request.requestedLifetimeCount = 10000;
  request.requestedMaxKeepAliveCount = 10;
  request.maxNotificationsPerPublish = 0;
  request.priority = 0;
  response = UA_Client_Subscriptions_modify(client->client, request);
  service_result = response.responseHeader.serviceResult;
  if (status_out != NULL) {
    *status_out = cpkt_status(service_result);
  }
  UA_ModifySubscriptionResponse_clear(&response);
  return service_result == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

static void cpkt_data_change_callback(
    UA_Client *native_client,
    UA_UInt32 subscription_id,
    void *subscription_context,
    UA_UInt32 monitored_item_id,
    void *monitored_item_context,
    UA_DataValue *data_value) {
  struct cpkt_opcua_monitor_context *context;
  cpkt_opcua_value value;
  cpkt_opcua_result result;
  char *string_buffer;
  size_t required;

  (void)native_client;
  (void)subscription_context;
  context = (struct cpkt_opcua_monitor_context *)monitored_item_context;
  if (context == NULL || context->fn == NULL) {
    return;
  }
  string_buffer = NULL;
  cpkt_opcua_value_clear(&value);
  if (data_value == NULL || !data_value->hasValue) {
    context->fn(
        (cpkt_opcua_subscription_id)subscription_id,
        (cpkt_opcua_monitored_item_id)monitored_item_id,
        &value,
        UA_STATUSCODE_BADNODATAAVAILABLE,
        context->user);
    return;
  }
  result = cpkt_get_variant(&data_value->value, &value, NULL, 0, &required);
  if (result == CPKT_OPCUA_ERR_RANGE && cpkt_value_type_needs_buffer(value.type) && required != 0) {
    string_buffer = (char *)malloc(required);
    if (string_buffer != NULL) {
      result = cpkt_get_variant(&data_value->value, &value, string_buffer, required, &required);
    } else {
      result = CPKT_OPCUA_ERR_ALLOC;
    }
  }
  context->fn(
      (cpkt_opcua_subscription_id)subscription_id,
      (cpkt_opcua_monitored_item_id)monitored_item_id,
      &value,
      result == CPKT_OPCUA_OK ? UA_STATUSCODE_GOOD : UA_STATUSCODE_BADTYPEMISMATCH,
      context->user);
  free(string_buffer);
}

static void cpkt_delete_monitored_item_callback(
    UA_Client *native_client,
    UA_UInt32 subscription_id,
    void *subscription_context,
    UA_UInt32 monitored_item_id,
    void *monitored_item_context) {
  struct cpkt_opcua_monitor_context *context;

  (void)native_client;
  (void)subscription_id;
  (void)subscription_context;
  (void)monitored_item_id;
  context = (struct cpkt_opcua_monitor_context *)monitored_item_context;
  if (context == NULL) {
    return;
  }
  cpkt_remove_monitor_context(context->owner, context);
  cpkt_monitor_context_free(context);
}

static int cpkt_event_field_matches(const UA_QualifiedName *key, const char *name) {
  const char *key_text;
  size_t key_length;
  size_t name_length;
  size_t offset;

  if (key == NULL || key->name.data == NULL || name == NULL) {
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

static void cpkt_event_callback(
    UA_Client *native_client,
    UA_UInt32 subscription_id,
    void *subscription_context,
    UA_UInt32 monitored_item_id,
    void *monitored_item_context,
    const UA_KeyValueMap event_fields) {
  struct cpkt_opcua_monitor_context *context;
  cpkt_opcua_event event;
  UA_KeyValuePair *pair;
  UA_ByteString *event_id;
  UA_String *source_name;
  UA_LocalizedText *message;
  UA_UInt16 *severity;
  size_t i;

  (void)native_client;
  (void)subscription_context;
  context = (struct cpkt_opcua_monitor_context *)monitored_item_context;
  if (context == NULL || context->event_fn == NULL) {
    return;
  }
  memset(&event, 0, sizeof(event));
  for (i = 0; i < event_fields.mapSize; ++i) {
    pair = &event_fields.map[i];
    if (cpkt_event_field_matches(&pair->key, "EventId") &&
        UA_Variant_hasScalarType(&pair->value, &UA_TYPES[UA_TYPES_BYTESTRING])) {
      event_id = (UA_ByteString *)pair->value.data;
      event.event_id = (const unsigned char *)event_id->data;
      event.event_id_length = event_id->length;
    } else if (cpkt_event_field_matches(&pair->key, "SourceName") &&
               UA_Variant_hasScalarType(&pair->value, &UA_TYPES[UA_TYPES_STRING])) {
      source_name = (UA_String *)pair->value.data;
      event.source_name = (const char *)source_name->data;
      event.source_name_length = source_name->length;
    } else if (cpkt_event_field_matches(&pair->key, "Message") &&
               UA_Variant_hasScalarType(&pair->value, &UA_TYPES[UA_TYPES_LOCALIZEDTEXT])) {
      message = (UA_LocalizedText *)pair->value.data;
      event.message = (const char *)message->text.data;
      event.message_length = message->text.length;
    } else if (cpkt_event_field_matches(&pair->key, "Severity") &&
               UA_Variant_hasScalarType(&pair->value, &UA_TYPES[UA_TYPES_UINT16])) {
      severity = (UA_UInt16 *)pair->value.data;
      event.severity = (unsigned long)*severity;
    }
  }
  context->event_fn(
      (cpkt_opcua_subscription_id)subscription_id,
      (cpkt_opcua_monitored_item_id)monitored_item_id,
      &event,
      0,
      context->user);
}

static void cpkt_event_fields_callback(
    UA_Client *native_client,
    UA_UInt32 subscription_id,
    void *subscription_context,
    UA_UInt32 monitored_item_id,
    void *monitored_item_context,
    const UA_KeyValueMap event_fields) {
  struct cpkt_opcua_monitor_context *context;
  cpkt_opcua_event_field *fields;
  UA_KeyValuePair *pair;
  cpkt_opcua_status callback_status;
  cpkt_opcua_status field_status;
  size_t i;
  size_t j;

  (void)native_client;
  (void)subscription_context;
  context = (struct cpkt_opcua_monitor_context *)monitored_item_context;
  if (context == NULL || context->event_fields_fn == NULL) {
    return;
  }
  fields = NULL;
  callback_status = 0;
  if (context->event_field_count != 0) {
    fields = (cpkt_opcua_event_field *)calloc(context->event_field_count, sizeof(*fields));
    if (fields == NULL) {
      context->event_fields_fn(
          (cpkt_opcua_subscription_id)subscription_id,
          (cpkt_opcua_monitored_item_id)monitored_item_id,
          NULL,
          0,
          UA_STATUSCODE_BADOUTOFMEMORY,
          context->user);
      return;
    }
  }
  for (i = 0; i < context->event_field_count; ++i) {
    fields[i].name = context->event_field_names[i];
    fields[i].name_length = strlen(context->event_field_names[i]);
    field_status = cpkt_status(UA_STATUSCODE_BADNOTFOUND);
    for (j = 0; j < event_fields.mapSize; ++j) {
      pair = &event_fields.map[j];
      if (!cpkt_event_field_matches(&pair->key, context->event_field_names[i])) {
        continue;
      }
      field_status = cpkt_status(UA_STATUSCODE_GOOD);
      if (cpkt_get_variant_borrowed(&pair->value, &fields[i].value) != CPKT_OPCUA_OK) {
        field_status = cpkt_status(UA_STATUSCODE_BADTYPEMISMATCH);
      }
      break;
    }
    fields[i].status = field_status;
    if (field_status != cpkt_status(UA_STATUSCODE_GOOD)) {
      callback_status = field_status;
    }
  }
  context->event_fields_fn(
      (cpkt_opcua_subscription_id)subscription_id,
      (cpkt_opcua_monitored_item_id)monitored_item_id,
      fields,
      context->event_field_count,
      callback_status,
      context->user);
  free(fields);
}

static void cpkt_init_event_select_clause(
    UA_SimpleAttributeOperand *select_clause,
    UA_QualifiedName *browse_name,
    const char *field_name) {
  UA_SimpleAttributeOperand_init(select_clause);
  *browse_name = UA_QUALIFIEDNAME(0, (char *)field_name);
  select_clause->typeDefinitionId = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEEVENTTYPE);
  select_clause->browsePathSize = 1;
  select_clause->browsePath = browse_name;
  select_clause->attributeId = UA_ATTRIBUTEID_VALUE;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_monitor_value(
    cpkt_opcua_client *client,
    cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_node_id node_id,
    double sampling_interval_ms,
    cpkt_opcua_data_change_fn fn,
    void *user,
    cpkt_opcua_monitored_item_id *monitored_item_id_out,
    cpkt_opcua_status *status_out) {
  cpkt_opcua_monitor_options options;

  cpkt_opcua_monitor_options_default(&options);
  options.sampling_interval_ms = sampling_interval_ms;
  return cpkt_opcua_client_monitor_value_ex(
      client,
      subscription_id,
      node_id,
      &options,
      fn,
      user,
      monitored_item_id_out,
      status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_monitor_value_ex(
    cpkt_opcua_client *client,
    cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_node_id node_id,
    const cpkt_opcua_monitor_options *options,
    cpkt_opcua_data_change_fn fn,
    void *user,
    cpkt_opcua_monitored_item_id *monitored_item_id_out,
    cpkt_opcua_status *status_out) {
  struct cpkt_opcua_monitor_context *context;
  UA_MonitoredItemCreateRequest request;
  UA_MonitoredItemCreateResult result;
  UA_DataChangeFilter filter;
  UA_NodeId native_node_id;
  UA_StatusCode item_status;
  UA_UInt32 monitored_item_id;
  double sampling_interval_ms;
  unsigned long queue_size;
  int discard_oldest;
  int deadband_type;
  double deadband_value;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (monitored_item_id_out != NULL) {
    *monitored_item_id_out = 0;
  }
  sampling_interval_ms = options != NULL ? options->sampling_interval_ms : 0.0;
  queue_size = options != NULL ? options->queue_size : 0;
  discard_oldest = options != NULL ? options->discard_oldest : 1;
  deadband_type = options != NULL ? options->deadband_type : CPKT_OPCUA_DEADBAND_NONE;
  deadband_value = options != NULL ? options->deadband_value : 0.0;
  if (client == NULL || client->client == NULL || fn == NULL ||
      subscription_id > (unsigned long)UINT_MAX || sampling_interval_ms < 0.0 ||
      queue_size > (unsigned long)UINT_MAX || !cpkt_valid_node_id(node_id) ||
      (deadband_type != CPKT_OPCUA_DEADBAND_NONE &&
       deadband_type != CPKT_OPCUA_DEADBAND_ABSOLUTE &&
       deadband_type != CPKT_OPCUA_DEADBAND_PERCENT) ||
      (deadband_type == CPKT_OPCUA_DEADBAND_NONE && deadband_value != 0.0) ||
      (deadband_type != CPKT_OPCUA_DEADBAND_NONE && deadband_value < 0.0) ||
      (deadband_type == CPKT_OPCUA_DEADBAND_PERCENT && deadband_value > 100.0)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  context = (struct cpkt_opcua_monitor_context *)calloc(1, sizeof(*context));
  if (context == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->owner = client;
  context->subscription_id = subscription_id;
  context->fn = fn;
  context->user = user;

  native_node_id = cpkt_make_node_id(node_id);
  request = UA_MonitoredItemCreateRequest_default(native_node_id);
  if (sampling_interval_ms > 0.0) {
    request.requestedParameters.samplingInterval = sampling_interval_ms;
  }
  if (queue_size != 0) {
    request.requestedParameters.queueSize = (UA_UInt32)queue_size;
  }
  request.requestedParameters.discardOldest = discard_oldest ? true : false;
  if (deadband_type != CPKT_OPCUA_DEADBAND_NONE) {
    UA_DataChangeFilter_init(&filter);
    filter.trigger = UA_DATACHANGETRIGGER_STATUSVALUE;
    filter.deadbandType = (UA_UInt32)deadband_type;
    filter.deadbandValue = (UA_Double)deadband_value;
    request.requestedParameters.filter.encoding = UA_EXTENSIONOBJECT_DECODED;
    request.requestedParameters.filter.content.decoded.type = &UA_TYPES[UA_TYPES_DATACHANGEFILTER];
    request.requestedParameters.filter.content.decoded.data = &filter;
  }
  result = UA_Client_MonitoredItems_createDataChange(
      client->client,
      (UA_UInt32)subscription_id,
      UA_TIMESTAMPSTORETURN_BOTH,
      request,
      context,
      cpkt_data_change_callback,
      cpkt_delete_monitored_item_callback);
  item_status = result.statusCode;
  monitored_item_id = result.monitoredItemId;
  if (status_out != NULL) {
    *status_out = cpkt_status(item_status);
  }
  UA_MonitoredItemCreateResult_clear(&result);
  if (item_status != UA_STATUSCODE_GOOD) {
    cpkt_monitor_context_free(context);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  context->monitored_item_id = (cpkt_opcua_monitored_item_id)monitored_item_id;
  context->next = client->monitors;
  client->monitors = context;
  if (monitored_item_id_out != NULL) {
    *monitored_item_id_out = context->monitored_item_id;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_monitor_events(
    cpkt_opcua_client *client,
    cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_node_id node_id,
    double sampling_interval_ms,
    cpkt_opcua_event_fn fn,
    void *user,
    cpkt_opcua_monitored_item_id *monitored_item_id_out,
    cpkt_opcua_status *status_out) {
  struct cpkt_opcua_monitor_context *context;
  UA_MonitoredItemCreateRequest request;
  UA_MonitoredItemCreateResult result;
  UA_NodeId native_node_id;
  UA_EventFilter filter;
  UA_SimpleAttributeOperand select_clauses[4];
  UA_QualifiedName browse_names[4];
  UA_StatusCode item_status;
  UA_UInt32 monitored_item_id;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (monitored_item_id_out != NULL) {
    *monitored_item_id_out = 0;
  }
  if (client == NULL || client->client == NULL || fn == NULL ||
      subscription_id > (unsigned long)UINT_MAX || sampling_interval_ms < 0.0 ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  context = (struct cpkt_opcua_monitor_context *)calloc(1, sizeof(*context));
  if (context == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->owner = client;
  context->subscription_id = subscription_id;
  context->event_fn = fn;
  context->user = user;

  native_node_id = cpkt_make_node_id(node_id);
  request = UA_MonitoredItemCreateRequest_default(native_node_id);
  request.itemToMonitor.attributeId = UA_ATTRIBUTEID_EVENTNOTIFIER;
  if (sampling_interval_ms > 0.0) {
    request.requestedParameters.samplingInterval = sampling_interval_ms;
  }
  UA_EventFilter_init(&filter);
  cpkt_init_event_select_clause(&select_clauses[0], &browse_names[0], "EventId");
  cpkt_init_event_select_clause(&select_clauses[1], &browse_names[1], "SourceName");
  cpkt_init_event_select_clause(&select_clauses[2], &browse_names[2], "Message");
  cpkt_init_event_select_clause(&select_clauses[3], &browse_names[3], "Severity");
  filter.selectClauses = select_clauses;
  filter.selectClausesSize = 4;
  request.requestedParameters.filter.encoding = UA_EXTENSIONOBJECT_DECODED;
  request.requestedParameters.filter.content.decoded.type = &UA_TYPES[UA_TYPES_EVENTFILTER];
  request.requestedParameters.filter.content.decoded.data = &filter;

  result = UA_Client_MonitoredItems_createEvent(
      client->client,
      (UA_UInt32)subscription_id,
      UA_TIMESTAMPSTORETURN_BOTH,
      request,
      context,
      cpkt_event_callback,
      cpkt_delete_monitored_item_callback);
  item_status = result.statusCode;
  monitored_item_id = result.monitoredItemId;
  if (status_out != NULL) {
    *status_out = cpkt_status(item_status);
  }
  UA_MonitoredItemCreateResult_clear(&result);
  if (item_status != UA_STATUSCODE_GOOD) {
    cpkt_monitor_context_free(context);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  context->monitored_item_id = (cpkt_opcua_monitored_item_id)monitored_item_id;
  context->next = client->monitors;
  client->monitors = context;
  if (monitored_item_id_out != NULL) {
    *monitored_item_id_out = context->monitored_item_id;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_monitor_event_fields(
    cpkt_opcua_client *client,
    cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_node_id node_id,
    double sampling_interval_ms,
    const cpkt_opcua_string_view *field_names,
    size_t field_count,
    cpkt_opcua_event_fields_fn fn,
    void *user,
    cpkt_opcua_monitored_item_id *monitored_item_id_out,
    cpkt_opcua_status *status_out) {
  struct cpkt_opcua_monitor_context *context;
  UA_MonitoredItemCreateRequest request;
  UA_MonitoredItemCreateResult result;
  UA_NodeId native_node_id;
  UA_EventFilter filter;
  UA_SimpleAttributeOperand *select_clauses;
  UA_QualifiedName *browse_names;
  UA_StatusCode item_status;
  UA_UInt32 monitored_item_id;
  size_t i;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (monitored_item_id_out != NULL) {
    *monitored_item_id_out = 0;
  }
  if (client == NULL || client->client == NULL || fn == NULL || field_names == NULL ||
      field_count == 0 || subscription_id > (unsigned long)UINT_MAX ||
      sampling_interval_ms < 0.0 || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  context = (struct cpkt_opcua_monitor_context *)calloc(1, sizeof(*context));
  if (context == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->event_field_names = (char **)calloc(field_count, sizeof(*context->event_field_names));
  if (context->event_field_names == NULL) {
    cpkt_monitor_context_free(context);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->event_field_count = field_count;
  for (i = 0; i < field_count; ++i) {
    if (field_names[i].data == NULL || field_names[i].length == 0) {
      cpkt_monitor_context_free(context);
      return CPKT_OPCUA_ERR_ARG;
    }
    context->event_field_names[i] = (char *)calloc(field_names[i].length + 1, 1);
    if (context->event_field_names[i] == NULL) {
      cpkt_monitor_context_free(context);
      return CPKT_OPCUA_ERR_ALLOC;
    }
    memcpy(context->event_field_names[i], field_names[i].data, field_names[i].length);
  }
  select_clauses = (UA_SimpleAttributeOperand *)calloc(field_count, sizeof(*select_clauses));
  browse_names = (UA_QualifiedName *)calloc(field_count, sizeof(*browse_names));
  if (select_clauses == NULL || browse_names == NULL) {
    free(select_clauses);
    free(browse_names);
    cpkt_monitor_context_free(context);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->owner = client;
  context->subscription_id = subscription_id;
  context->event_fields_fn = fn;
  context->user = user;

  native_node_id = cpkt_make_node_id(node_id);
  request = UA_MonitoredItemCreateRequest_default(native_node_id);
  request.itemToMonitor.attributeId = UA_ATTRIBUTEID_EVENTNOTIFIER;
  if (sampling_interval_ms > 0.0) {
    request.requestedParameters.samplingInterval = sampling_interval_ms;
  }
  UA_EventFilter_init(&filter);
  for (i = 0; i < field_count; ++i) {
    cpkt_init_event_select_clause(
        &select_clauses[i],
        &browse_names[i],
        context->event_field_names[i]);
  }
  filter.selectClauses = select_clauses;
  filter.selectClausesSize = field_count;
  request.requestedParameters.filter.encoding = UA_EXTENSIONOBJECT_DECODED;
  request.requestedParameters.filter.content.decoded.type = &UA_TYPES[UA_TYPES_EVENTFILTER];
  request.requestedParameters.filter.content.decoded.data = &filter;

  result = UA_Client_MonitoredItems_createEvent(
      client->client,
      (UA_UInt32)subscription_id,
      UA_TIMESTAMPSTORETURN_BOTH,
      request,
      context,
      cpkt_event_fields_callback,
      cpkt_delete_monitored_item_callback);
  item_status = result.statusCode;
  monitored_item_id = result.monitoredItemId;
  if (status_out != NULL) {
    *status_out = cpkt_status(item_status);
  }
  UA_MonitoredItemCreateResult_clear(&result);
  free(select_clauses);
  free(browse_names);
  if (item_status != UA_STATUSCODE_GOOD) {
    cpkt_monitor_context_free(context);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  context->monitored_item_id = (cpkt_opcua_monitored_item_id)monitored_item_id;
  context->next = client->monitors;
  client->monitors = context;
  if (monitored_item_id_out != NULL) {
    *monitored_item_id_out = context->monitored_item_id;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_set_monitoring_mode(
    cpkt_opcua_client *client,
    cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_monitored_item_id monitored_item_id,
    int monitoring_mode,
    cpkt_opcua_status *status_out) {
  UA_SetMonitoringModeRequest request;
  UA_SetMonitoringModeResponse response;
  UA_UInt32 native_monitored_item_id;
  UA_StatusCode item_status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || subscription_id > (unsigned long)UINT_MAX ||
      monitored_item_id > (unsigned long)UINT_MAX ||
      (monitoring_mode != CPKT_OPCUA_MONITORING_DISABLED &&
       monitoring_mode != CPKT_OPCUA_MONITORING_SAMPLING &&
       monitoring_mode != CPKT_OPCUA_MONITORING_REPORTING)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_monitored_item_id = (UA_UInt32)monitored_item_id;
  UA_SetMonitoringModeRequest_init(&request);
  request.subscriptionId = (UA_UInt32)subscription_id;
  request.monitoringMode = (UA_MonitoringMode)monitoring_mode;
  request.monitoredItemIdsSize = 1;
  request.monitoredItemIds = &native_monitored_item_id;
  response = UA_Client_MonitoredItems_setMonitoringMode(client->client, request);
  if (status_out != NULL) {
    *status_out = cpkt_status(response.responseHeader.serviceResult);
  }
  if (response.responseHeader.serviceResult != UA_STATUSCODE_GOOD) {
    UA_SetMonitoringModeResponse_clear(&response);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (response.resultsSize != 1 || response.results == NULL) {
    UA_SetMonitoringModeResponse_clear(&response);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  item_status = response.results[0];
  if (status_out != NULL) {
    *status_out = cpkt_status(item_status);
  }
  UA_SetMonitoringModeResponse_clear(&response);
  return item_status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_delete_monitored_item(
    cpkt_opcua_client *client,
    cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_monitored_item_id monitored_item_id,
    cpkt_opcua_status *status_out) {
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      subscription_id > (unsigned long)UINT_MAX ||
      monitored_item_id > (unsigned long)UINT_MAX) {
    return CPKT_OPCUA_ERR_ARG;
  }
  status = UA_Client_MonitoredItems_deleteSingle(
      client->client,
      (UA_UInt32)subscription_id,
      (UA_UInt32)monitored_item_id);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_native(
    cpkt_opcua_client *client,
    cpkt_opcua_client_native_fn fn,
    void *user) {
  if (client == NULL || client->client == NULL || fn == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  return fn(client->client, user) == 0 ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_async_native(
    cpkt_opcua_client *client,
    cpkt_opcua_client_native_fn fn,
    void *user) {
  return cpkt_opcua_client_native(client, fn, user);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_history_native(
    cpkt_opcua_client *client,
    cpkt_opcua_client_native_fn fn,
    void *user) {
  return cpkt_opcua_client_native(client, fn, user);
}
