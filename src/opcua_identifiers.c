/* Handwritten OPC UA facade: identifiers. */
#include "opcua_facade_internal.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cpkt_opcua_status cpkt_status(UA_StatusCode status) {
  return (cpkt_opcua_status)status;
}

int cpkt_valid_uint64_words(unsigned long high32, unsigned long low32) {
  return high32 <= CPKT_OPCUA_UINT32_MAX_VALUE &&
         low32 <= CPKT_OPCUA_UINT32_MAX_VALUE;
}

int cpkt_valid_datetime_words(long high32, unsigned long low32) {
  return high32 >= CPKT_OPCUA_INT32_MIN_VALUE &&
         high32 <= CPKT_OPCUA_INT32_MAX_VALUE &&
         low32 <= CPKT_OPCUA_UINT32_MAX_VALUE;
}

UA_UInt64 cpkt_make_uint64(cpkt_opcua_uint64 value) {
  return ((UA_UInt64)value.high32 << 32) | (UA_UInt64)value.low32;
}

UA_DateTime cpkt_make_datetime(cpkt_opcua_datetime value) {
  return (UA_DateTime)((UA_Int64)value.high32 *
                           ((UA_Int64)CPKT_OPCUA_UINT32_MAX_VALUE + 1) +
                       (UA_Int64)value.low32);
}

cpkt_opcua_uint64 cpkt_uint64_from_native(UA_UInt64 value) {
  cpkt_opcua_uint64 out;

  out.high32 = (unsigned long)(value >> 32);
  out.low32 = (unsigned long)(value & (UA_UInt64)CPKT_OPCUA_UINT32_MAX_VALUE);
  return out;
}

cpkt_opcua_datetime cpkt_datetime_from_native(UA_DateTime value) {
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

UA_Guid cpkt_make_guid(const unsigned char guid[16]) {
  UA_Guid native_guid;

  native_guid.data1 = ((UA_UInt32)guid[0] << 24) | ((UA_UInt32)guid[1] << 16) |
                      ((UA_UInt32)guid[2] << 8) | (UA_UInt32)guid[3];
  native_guid.data2 =
      (UA_UInt16)(((UA_UInt16)guid[4] << 8) | (UA_UInt16)guid[5]);
  native_guid.data3 =
      (UA_UInt16)(((UA_UInt16)guid[6] << 8) | (UA_UInt16)guid[7]);
  memcpy(native_guid.data4, guid + 8, 8);
  return native_guid;
}

void cpkt_guid_from_native(UA_Guid native_guid, unsigned char guid[16]) {
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

UA_NodeId cpkt_make_node_id(cpkt_opcua_node_id node_id) {
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

UA_ExpandedNodeId
cpkt_make_expanded_node_id(cpkt_opcua_expanded_node_id node_id) {
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

UA_ByteString cpkt_make_borrowed_byte_string(const unsigned char *data,
                                             size_t length) {
  UA_ByteString bytes;

  bytes.data = (UA_Byte *)data;
  bytes.length = length;
  return bytes;
}

cpkt_opcua_result
cpkt_make_borrowed_byte_string_array(const cpkt_opcua_byte_string_view *views,
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

int cpkt_valid_node_id(cpkt_opcua_node_id node_id) {
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

int cpkt_valid_expanded_node_id(cpkt_opcua_expanded_node_id node_id) {
  if (!cpkt_valid_node_id(node_id.node_id) ||
      node_id.server_index > (unsigned long)UINT_MAX) {
    return 0;
  }
  if (node_id.namespace_uri == NULL && node_id.namespace_uri_length != 0) {
    return 0;
  }
  return 1;
}

static int cpkt_parse_unsigned_long(const char *text, const char **end_out,
                                    unsigned long *value_out) {
  char *end;
  unsigned long value;

  if (text == NULL || end_out == NULL || value_out == NULL || *text < '0' ||
      *text > '9') {
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

static cpkt_opcua_result
cpkt_copy_c_string_to_buffer(const char *value, char *buffer,
                             size_t buffer_size, size_t *required_size_out) {
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

static cpkt_opcua_result
cpkt_copy_counted_string_to_buffer(const char *value, size_t value_length,
                                   char *buffer, size_t buffer_size,
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

cpkt_opcua_result
cpkt_copy_ua_byte_string_to_buffer(const UA_ByteString *value,
                                   unsigned char *buffer, size_t buffer_size,
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

int cpkt_native_node_id_to_facade(const UA_NodeId *native,
                                  cpkt_opcua_node_id *out,
                                  struct cpkt_owned_node_id_memory *owned_out) {
  char *owned;
  unsigned char *owned_bytes;

  if (native == NULL || out == NULL || owned_out == NULL) {
    return 0;
  }
  owned_out->string = NULL;
  owned_out->byte_string = NULL;
  if (native->identifierType == UA_NODEIDTYPE_NUMERIC &&
      native->namespaceIndex == 0 && native->identifier.numeric == 0) {
    *out = cpkt_opcua_node_id_null();
    return 1;
  }
  if (native->identifierType == UA_NODEIDTYPE_NUMERIC) {
    *out =
        cpkt_opcua_node_id_numeric((unsigned short)native->namespaceIndex,
                                   (unsigned long)native->identifier.numeric);
    return 1;
  }
  if (native->identifierType == UA_NODEIDTYPE_STRING) {
    owned = (char *)malloc(native->identifier.string.length + 1);
    if (owned == NULL) {
      return 0;
    }
    if (native->identifier.string.length != 0) {
      memcpy(owned, native->identifier.string.data,
             native->identifier.string.length);
    }
    owned[native->identifier.string.length] = '\0';
    *out = cpkt_opcua_node_id_string((unsigned short)native->namespaceIndex,
                                     owned);
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
      owned_bytes =
          (unsigned char *)malloc(native->identifier.byteString.length);
      if (owned_bytes == NULL) {
        return 0;
      }
      memcpy(owned_bytes, native->identifier.byteString.data,
             native->identifier.byteString.length);
    } else {
      owned_bytes = NULL;
    }
    *out = cpkt_opcua_node_id_byte_string(
        (unsigned short)native->namespaceIndex, owned_bytes,
        native->identifier.byteString.length);
    owned_out->byte_string = owned_bytes;
    return 1;
  }
  return 0;
}

void cpkt_owned_node_id_memory_clear(struct cpkt_owned_node_id_memory *owned) {
  if (owned != NULL) {
    free(owned->string);
    free(owned->byte_string);
    owned->string = NULL;
    owned->byte_string = NULL;
  }
}

char *cpkt_copy_ua_string(const UA_String *value) {
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

char *cpkt_strdup_c89(const char *value) {
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

unsigned char *cpkt_memdup_c89(const unsigned char *value, size_t length) {
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

cpkt_opcua_result
cpkt_copy_facade_node_id(cpkt_opcua_node_id in, cpkt_opcua_node_id *out,
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
  } else if (in.identifier_type == CPKT_OPCUA_NODE_ID_BYTE_STRING &&
             in.byte_string_length != 0) {
    bytes_copy = cpkt_memdup_c89(in.byte_string, in.byte_string_length);
    if (bytes_copy == NULL) {
      return CPKT_OPCUA_ERR_ALLOC;
    }
    out->byte_string = bytes_copy;
    owned_out->byte_string = bytes_copy;
  }
  return CPKT_OPCUA_OK;
}

cpkt_opcua_result cpkt_copy_native_node_id_to_output(
    const UA_NodeId *native, cpkt_opcua_node_id *node_id_out, char *buffer,
    size_t buffer_size, size_t *required_size_out) {
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

cpkt_opcua_result cpkt_copy_ua_string_to_buffer(const UA_String *value,
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

int cpkt_ulong_fits_uint32(unsigned long value) {
  return value <= (unsigned long)UINT_MAX;
}

int cpkt_long_fits_int32(long value) {
  return value >= (long)INT_MIN && value <= (long)INT_MAX;
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
  if (text == NULL || guid_out == NULL || strlen(text) != 36 ||
      text[8] != '-' || text[13] != '-' || text[18] != '-' || text[23] != '-') {
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

static void cpkt_base64_encode(const unsigned char *data, size_t length,
                               char *out) {
  static const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
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

static int cpkt_base64_decode(const char *text, unsigned char *buffer,
                              size_t buffer_size, size_t *required_size_out) {
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
    packed = ((unsigned long)values[0] << 18) |
             ((unsigned long)values[1] << 12) |
             ((unsigned long)values[2] << 6) | (unsigned long)values[3];
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
cpkt_opcua_node_id cpkt_opcua_node_id_numeric(unsigned short namespace_index,
                                              unsigned long identifier) {
  cpkt_opcua_node_id node_id;
  node_id = cpkt_opcua_node_id_null();
  node_id.namespace_index = namespace_index;
  node_id.identifier_type = CPKT_OPCUA_NODE_ID_NUMERIC;
  node_id.numeric = identifier;
  return node_id;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_node_id cpkt_opcua_node_id_string(unsigned short namespace_index,
                                             const char *identifier) {
  cpkt_opcua_node_id node_id;
  node_id = cpkt_opcua_node_id_null();
  node_id.namespace_index = namespace_index;
  node_id.identifier_type = CPKT_OPCUA_NODE_ID_STRING;
  node_id.string = identifier;
  return node_id;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_node_id cpkt_opcua_node_id_guid(unsigned short namespace_index,
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
cpkt_opcua_node_id
cpkt_opcua_node_id_byte_string(unsigned short namespace_index,
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
  if (a.namespace_index != b.namespace_index ||
      a.identifier_type != b.identifier_type) {
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
    return memcmp(a.byte_string, b.byte_string, a.byte_string_length) == 0 ? 1
                                                                           : 0;
  }
  return 0;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_node_id_print(cpkt_opcua_node_id node_id,
                                           char *buffer, size_t buffer_size,
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
    return cpkt_copy_c_string_to_buffer("i=0", buffer, buffer_size,
                                        required_size_out);
  }
  if (node_id.identifier_type == CPKT_OPCUA_NODE_ID_NUMERIC) {
    sprintf(prefix, "ns=%hu;i=%lu", node_id.namespace_index, node_id.numeric);
    return cpkt_copy_c_string_to_buffer(prefix, buffer, buffer_size,
                                        required_size_out);
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
    sprintf(prefix,
            "ns=%hu;g=%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%"
            "02x%02x%02x",
            node_id.namespace_index, node_id.guid[0], node_id.guid[1],
            node_id.guid[2], node_id.guid[3], node_id.guid[4], node_id.guid[5],
            node_id.guid[6], node_id.guid[7], node_id.guid[8], node_id.guid[9],
            node_id.guid[10], node_id.guid[11], node_id.guid[12],
            node_id.guid[13], node_id.guid[14], node_id.guid[15]);
    return cpkt_copy_c_string_to_buffer(prefix, buffer, buffer_size,
                                        required_size_out);
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
  cpkt_base64_encode(node_id.byte_string, node_id.byte_string_length,
                     buffer + prefix_length);
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_node_id_parse(const char *text,
                                           cpkt_opcua_node_id *node_id_out,
                                           char *buffer, size_t buffer_size,
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
    if (!cpkt_parse_unsigned_long(cursor, &end, &parsed) ||
        parsed > (unsigned long)USHRT_MAX || *end != ';') {
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
    copy_result = cpkt_copy_c_string_to_buffer(cursor, buffer, buffer_size,
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
    decode_result = cpkt_base64_decode(cursor, (unsigned char *)buffer,
                                       buffer_size, &decoded_size);
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
        namespace_index, (const unsigned char *)buffer, decoded_size);
    return CPKT_OPCUA_OK;
  }
  return CPKT_OPCUA_ERR_ARG;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_expanded_node_id
cpkt_opcua_expanded_node_id_local(cpkt_opcua_node_id node_id) {
  cpkt_opcua_expanded_node_id expanded;

  expanded.node_id = node_id;
  expanded.namespace_uri = NULL;
  expanded.namespace_uri_length = 0;
  expanded.server_index = 0;
  return expanded;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_expanded_node_id
cpkt_opcua_expanded_node_id_uri(const char *namespace_uri,
                                size_t namespace_uri_length,
                                cpkt_opcua_node_id node_id) {
  cpkt_opcua_expanded_node_id expanded;

  expanded = cpkt_opcua_expanded_node_id_local(node_id);
  expanded.namespace_uri = namespace_uri;
  expanded.namespace_uri_length = namespace_uri_length;
  return expanded;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_expanded_node_id
cpkt_opcua_expanded_node_id_server(unsigned long server_index,
                                   cpkt_opcua_node_id node_id) {
  cpkt_opcua_expanded_node_id expanded;

  expanded = cpkt_opcua_expanded_node_id_local(node_id);
  expanded.server_index = server_index;
  return expanded;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_expanded_node_id cpkt_opcua_expanded_node_id_server_uri(
    unsigned long server_index, const char *namespace_uri,
    size_t namespace_uri_length, cpkt_opcua_node_id node_id) {
  cpkt_opcua_expanded_node_id expanded;

  expanded = cpkt_opcua_expanded_node_id_uri(namespace_uri,
                                             namespace_uri_length, node_id);
  expanded.server_index = server_index;
  return expanded;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
int cpkt_opcua_expanded_node_id_equal(cpkt_opcua_expanded_node_id a,
                                      cpkt_opcua_expanded_node_id b) {
  if (a.server_index != b.server_index ||
      a.namespace_uri_length != b.namespace_uri_length ||
      !cpkt_opcua_node_id_equal(a.node_id, b.node_id)) {
    return 0;
  }
  if (a.namespace_uri_length == 0) {
    return 1;
  }
  if (a.namespace_uri == NULL || b.namespace_uri == NULL) {
    return 0;
  }
  return memcmp(a.namespace_uri, b.namespace_uri, a.namespace_uri_length) == 0
             ? 1
             : 0;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result
cpkt_opcua_expanded_node_id_print(cpkt_opcua_expanded_node_id node_id,
                                  char *buffer, size_t buffer_size,
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
  result = cpkt_opcua_node_id_print(node_id.node_id, inner, inner_required,
                                    &inner_required);
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
    memcpy(buffer + prefix_length, node_id.namespace_uri,
           node_id.namespace_uri_length);
    buffer[prefix_length + node_id.namespace_uri_length] = ';';
    memcpy(buffer + prefix_length + node_id.namespace_uri_length + 1, inner,
           strlen(inner) + 1);
  } else {
    memcpy(buffer + prefix_length, inner, strlen(inner) + 1);
  }
  free(inner);
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_expanded_node_id_parse(
    const char *text, cpkt_opcua_expanded_node_id *node_id_out, char *buffer,
    size_t buffer_size, size_t *required_size_out) {
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
    if (!cpkt_parse_unsigned_long(cursor, &end, &parsed) ||
        parsed > (unsigned long)UINT_MAX || *end != ';') {
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
  result = cpkt_opcua_node_id_parse(cursor, &node_id, node_buffer,
                                    node_buffer_size, &node_required);
  if (result != CPKT_OPCUA_OK) {
    if (namespace_uri_length != 0 && result == CPKT_OPCUA_ERR_RANGE &&
        required_size_out != NULL) {
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
cpkt_opcua_result cpkt_opcua_guid_print(const unsigned char guid[16],
                                        char *buffer, size_t buffer_size,
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
      guid[0], guid[1], guid[2], guid[3], guid[4], guid[5], guid[6], guid[7],
      guid[8], guid[9], guid[10], guid[11], guid[12], guid[13], guid[14],
      guid[15]);
  return cpkt_copy_c_string_to_buffer(text, buffer, buffer_size,
                                      required_size_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_guid_parse(const char *text,
                                        unsigned char guid_out[16]) {
  if (!cpkt_parse_guid_text(text, guid_out)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_qualified_name_print(
    unsigned short namespace_index, const char *name, size_t name_length,
    char *buffer, size_t buffer_size, size_t *required_size_out) {
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
    const char *text, unsigned short *namespace_index_out, char *name_buffer,
    size_t name_buffer_size, size_t *name_length_out,
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
  if (!cpkt_parse_unsigned_long(cursor, &end, &parsed) ||
      parsed > (unsigned long)USHRT_MAX || strncmp(end, ";q=", 3) != 0) {
    return CPKT_OPCUA_ERR_ARG;
  }
  cursor = end + 3;
  name_length = strlen(cursor);
  result = cpkt_copy_counted_string_to_buffer(
      cursor, name_length, name_buffer, name_buffer_size, required_size_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  *namespace_index_out = (unsigned short)parsed;
  *name_length_out = name_length;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result
cpkt_opcua_localized_text_print(const char *locale, size_t locale_length,
                                const char *text, size_t text_length,
                                char *buffer, size_t buffer_size,
                                size_t *required_size_out) {
  static const char prefix[] = "locale=";
  static const char separator[] = ";text=";
  size_t prefix_length;
  size_t separator_length;
  size_t required;

  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if ((locale == NULL && locale_length != 0) ||
      (text == NULL && text_length != 0)) {
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
    memcpy(buffer + prefix_length + locale_length + separator_length, text,
           text_length);
  }
  buffer[required - 1] = '\0';
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_localized_text_parse(
    const char *input, char *buffer, size_t buffer_size,
    const char **locale_out, size_t *locale_length_out, const char **text_out,
    size_t *text_length_out, size_t *required_size_out) {
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
