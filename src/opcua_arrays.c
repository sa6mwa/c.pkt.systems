/* Handwritten OPC UA facade: arrays. */
#include "opcua_facade_internal.h"

cpkt_opcua_result
cpkt_copy_boolean_array_from_variant(const UA_Variant *variant, int *values,
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

cpkt_opcua_result
cpkt_copy_integer_array_from_variant(const UA_Variant *variant, long *values,
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

cpkt_opcua_result cpkt_copy_uint64_array_from_variant(
    const UA_Variant *variant, cpkt_opcua_uint64 *values, size_t value_count,
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

cpkt_opcua_result cpkt_copy_datetime_array_from_variant(
    const UA_Variant *variant, cpkt_opcua_datetime *values, size_t value_count,
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

cpkt_opcua_result cpkt_copy_status_array_from_variant(
    const UA_Variant *variant, cpkt_opcua_status *values, size_t value_count,
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

cpkt_opcua_result
cpkt_copy_guid_array_from_variant(const UA_Variant *variant,
                                  cpkt_opcua_guid *values, size_t value_count,
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

cpkt_opcua_result
cpkt_copy_double_array_from_variant(const UA_Variant *variant, double *values,
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

cpkt_opcua_result
cpkt_each_string_array_from_variant(const UA_Variant *variant,
                                    cpkt_opcua_string_array_fn fn, void *user,
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
    if (fn(i, (const char *)native_values[i].data, native_values[i].length,
           user) != 0) {
      return CPKT_OPCUA_ERR_CALLBACK;
    }
  }
  return CPKT_OPCUA_OK;
}

cpkt_opcua_result
cpkt_each_byte_string_array_from_variant(const UA_Variant *variant,
                                         cpkt_opcua_byte_string_array_fn fn,
                                         void *user, size_t *value_count_out) {
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
    if (fn(i, (const unsigned char *)native_values[i].data,
           native_values[i].length, user) != 0) {
      return CPKT_OPCUA_ERR_CALLBACK;
    }
  }
  return CPKT_OPCUA_OK;
}

cpkt_opcua_result cpkt_each_qualified_name_array_from_variant(
    const UA_Variant *variant, cpkt_opcua_qualified_name_array_fn fn,
    void *user, size_t *value_count_out) {
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
    if (fn(i, (unsigned short)native_values[i].namespaceIndex,
           (const char *)native_values[i].name.data,
           native_values[i].name.length, user) != 0) {
      return CPKT_OPCUA_ERR_CALLBACK;
    }
  }
  return CPKT_OPCUA_OK;
}

cpkt_opcua_result cpkt_each_qualified_name_range_from_variant(
    const UA_Variant *variant, cpkt_opcua_qualified_name_array_fn fn,
    void *user, size_t *value_count_out) {
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
    return fn(0, (unsigned short)native_value->namespaceIndex,
              (const char *)native_value->name.data, native_value->name.length,
              user) == 0
               ? CPKT_OPCUA_OK
               : CPKT_OPCUA_ERR_CALLBACK;
  }
  return cpkt_each_qualified_name_array_from_variant(variant, fn, user,
                                                     value_count_out);
}

cpkt_opcua_result cpkt_each_localized_text_array_from_variant(
    const UA_Variant *variant, cpkt_opcua_localized_text_array_fn fn,
    void *user, size_t *value_count_out) {
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
    if (fn(i, (const char *)native_values[i].locale.data,
           native_values[i].locale.length,
           (const char *)native_values[i].text.data,
           native_values[i].text.length, user) != 0) {
      return CPKT_OPCUA_ERR_CALLBACK;
    }
  }
  return CPKT_OPCUA_OK;
}

cpkt_opcua_result cpkt_each_localized_text_range_from_variant(
    const UA_Variant *variant, cpkt_opcua_localized_text_array_fn fn,
    void *user, size_t *value_count_out) {
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
    return fn(0, (const char *)native_value->locale.data,
              native_value->locale.length,
              (const char *)native_value->text.data, native_value->text.length,
              user) == 0
               ? CPKT_OPCUA_OK
               : CPKT_OPCUA_ERR_CALLBACK;
  }
  return cpkt_each_localized_text_array_from_variant(variant, fn, user,
                                                     value_count_out);
}

cpkt_opcua_result
cpkt_each_string_range_from_variant(const UA_Variant *variant,
                                    cpkt_opcua_string_array_fn fn, void *user,
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
    return fn(0, (const char *)native_value->data, native_value->length,
              user) == 0
               ? CPKT_OPCUA_OK
               : CPKT_OPCUA_ERR_CALLBACK;
  }
  return cpkt_each_string_array_from_variant(variant, fn, user,
                                             value_count_out);
}

cpkt_opcua_result
cpkt_each_byte_string_range_from_variant(const UA_Variant *variant,
                                         cpkt_opcua_byte_string_array_fn fn,
                                         void *user, size_t *value_count_out) {
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
    return fn(0, (const unsigned char *)native_value->data,
              native_value->length, user) == 0
               ? CPKT_OPCUA_OK
               : CPKT_OPCUA_ERR_CALLBACK;
  }
  return cpkt_each_byte_string_array_from_variant(variant, fn, user,
                                                  value_count_out);
}

static int cpkt_parse_array_index_component(const char **cursor,
                                            size_t *value_out) {
  const char *p;
  size_t value;
  size_t digit;

  if (cursor == NULL || *cursor == NULL || value_out == NULL ||
      **cursor < '0' || **cursor > '9') {
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

int cpkt_parse_simple_array_index_range(const char *index_range,
                                        size_t *start_out, size_t *count_out) {
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

cpkt_opcua_result cpkt_each_string_array_slice_from_variant(
    const UA_Variant *variant, size_t start, size_t count,
    cpkt_opcua_string_array_fn fn, void *user, size_t *value_count_out) {
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
    if (fn(i, (const char *)native_values[start + i].data,
           native_values[start + i].length, user) != 0) {
      return CPKT_OPCUA_ERR_CALLBACK;
    }
  }
  return CPKT_OPCUA_OK;
}

cpkt_opcua_result cpkt_each_byte_string_array_slice_from_variant(
    const UA_Variant *variant, size_t start, size_t count,
    cpkt_opcua_byte_string_array_fn fn, void *user, size_t *value_count_out) {
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
    if (fn(i, (const unsigned char *)native_values[start + i].data,
           native_values[start + i].length, user) != 0) {
      return CPKT_OPCUA_ERR_CALLBACK;
    }
  }
  return CPKT_OPCUA_OK;
}
