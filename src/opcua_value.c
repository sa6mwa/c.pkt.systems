/* Handwritten OPC UA facade: value. */
#include "opcua_facade_internal.h"
#include <open62541/nodeids.h>
#include <stdlib.h>
#include <string.h>

cpkt_opcua_result cpkt_copy_array_dimensions_to_buffer(
    const UA_UInt32 *native_dimensions, size_t native_dimension_count,
    unsigned long *dimensions, size_t dimension_count,
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

cpkt_opcua_result
cpkt_make_array_dimensions(const unsigned long *dimensions,
                           size_t dimension_count,
                           UA_UInt32 **native_dimensions_out) {
  UA_UInt32 *native_dimensions;
  size_t i;

  if (native_dimensions_out != NULL) {
    *native_dimensions_out = NULL;
  }
  if (native_dimensions_out == NULL ||
      (dimension_count != 0 && dimensions == NULL)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (dimension_count == 0) {
    return CPKT_OPCUA_OK;
  }
  native_dimensions =
      (UA_UInt32 *)calloc(dimension_count, sizeof(*native_dimensions));
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

cpkt_opcua_result
cpkt_apply_value_shape_to_variable_attributes(UA_VariableAttributes *attr,
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
  attr->arrayDimensions =
      (UA_UInt32 *)UA_Array_new(1, &UA_TYPES[UA_TYPES_UINT32]);
  if (attr->arrayDimensions == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  attr->arrayDimensionsSize = 1;
  attr->arrayDimensions[0] = (UA_UInt32)array_length;
  return CPKT_OPCUA_OK;
}

cpkt_opcua_result cpkt_apply_value_shape_to_variable_type_attributes(
    UA_VariableTypeAttributes *attr, const cpkt_opcua_value *value) {
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

cpkt_opcua_result
cpkt_copy_node_id_to_caller(const cpkt_opcua_node_id *source,
                            const struct cpkt_owned_node_id_memory *owned,
                            cpkt_opcua_node_id *target_out, char *buffer,
                            size_t buffer_size, size_t *required_size_out) {
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
        source->namespace_index, (const unsigned char *)buffer, required);
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

UA_NodeId cpkt_data_type_node_id_for_value_type(int type) {
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
  return type == CPKT_OPCUA_VALUE_EMPTY ||
         cpkt_data_type_for_value_type(type) != NULL;
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

int cpkt_valid_method_value_type(int type) {
  return cpkt_valid_value_type(type) && !cpkt_value_type_is_array(type);
}

int cpkt_value_type_needs_buffer(int type) {
  return type == CPKT_OPCUA_VALUE_STRING ||
         type == CPKT_OPCUA_VALUE_BYTE_STRING ||
         type == CPKT_OPCUA_VALUE_QUALIFIED_NAME ||
         type == CPKT_OPCUA_VALUE_LOCALIZED_TEXT;
}

cpkt_opcua_result cpkt_set_variant(UA_Variant *variant,
                                   const cpkt_opcua_value *value) {
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
    UA_Variant_setScalarCopy(variant, &boolean_value,
                             &UA_TYPES[UA_TYPES_BOOLEAN]);
    break;
  case CPKT_OPCUA_VALUE_BOOLEAN_ARRAY:
    if (value->boolean_array_values == NULL &&
        value->boolean_array_length != 0) {
      return CPKT_OPCUA_ERR_ARG;
    }
    boolean_array_values = NULL;
    if (value->boolean_array_length != 0) {
      boolean_array_values = (UA_Boolean *)calloc(
          value->boolean_array_length, sizeof(*boolean_array_values));
      if (boolean_array_values == NULL) {
        return CPKT_OPCUA_ERR_ALLOC;
      }
    }
    for (i = 0; i < value->boolean_array_length; ++i) {
      boolean_array_values[i] = value->boolean_array_values[i] ? true : false;
    }
    status = UA_Variant_setArrayCopy(variant, boolean_array_values,
                                     value->boolean_array_length,
                                     &UA_TYPES[UA_TYPES_BOOLEAN]);
    free(boolean_array_values);
    if (status != UA_STATUSCODE_GOOD) {
      return CPKT_OPCUA_ERR_ALLOC;
    }
    break;
  case CPKT_OPCUA_VALUE_INTEGER:
    if (value->integer_value < (long)INT_MIN ||
        value->integer_value > (long)INT_MAX) {
      return CPKT_OPCUA_ERR_RANGE;
    }
    integer_value = (UA_Int32)value->integer_value;
    UA_Variant_setScalarCopy(variant, &integer_value,
                             &UA_TYPES[UA_TYPES_INT32]);
    break;
  case CPKT_OPCUA_VALUE_UINT64:
    if (!cpkt_valid_uint64_words(value->uint64_value.high32,
                                 value->uint64_value.low32)) {
      return CPKT_OPCUA_ERR_RANGE;
    }
    uint64_value = cpkt_make_uint64(value->uint64_value);
    UA_Variant_setScalarCopy(variant, &uint64_value,
                             &UA_TYPES[UA_TYPES_UINT64]);
    break;
  case CPKT_OPCUA_VALUE_UINT64_ARRAY:
    if (value->uint64_array_values == NULL && value->uint64_array_length != 0) {
      return CPKT_OPCUA_ERR_ARG;
    }
    uint64_array_values = NULL;
    if (value->uint64_array_length != 0) {
      uint64_array_values = (UA_UInt64 *)calloc(value->uint64_array_length,
                                                sizeof(*uint64_array_values));
      if (uint64_array_values == NULL) {
        return CPKT_OPCUA_ERR_ALLOC;
      }
    }
    for (i = 0; i < value->uint64_array_length; ++i) {
      if (!cpkt_valid_uint64_words(value->uint64_array_values[i].high32,
                                   value->uint64_array_values[i].low32)) {
        free(uint64_array_values);
        return CPKT_OPCUA_ERR_RANGE;
      }
      uint64_array_values[i] = cpkt_make_uint64(value->uint64_array_values[i]);
    }
    status = UA_Variant_setArrayCopy(variant, uint64_array_values,
                                     value->uint64_array_length,
                                     &UA_TYPES[UA_TYPES_UINT64]);
    free(uint64_array_values);
    if (status != UA_STATUSCODE_GOOD) {
      return CPKT_OPCUA_ERR_ALLOC;
    }
    break;
  case CPKT_OPCUA_VALUE_DATETIME:
    if (!cpkt_valid_datetime_words(value->datetime_value.high32,
                                   value->datetime_value.low32)) {
      return CPKT_OPCUA_ERR_RANGE;
    }
    datetime_value = cpkt_make_datetime(value->datetime_value);
    UA_Variant_setScalarCopy(variant, &datetime_value,
                             &UA_TYPES[UA_TYPES_DATETIME]);
    break;
  case CPKT_OPCUA_VALUE_DATETIME_ARRAY:
    if (value->datetime_array_values == NULL &&
        value->datetime_array_length != 0) {
      return CPKT_OPCUA_ERR_ARG;
    }
    datetime_array_values = NULL;
    if (value->datetime_array_length != 0) {
      datetime_array_values = (UA_DateTime *)calloc(
          value->datetime_array_length, sizeof(*datetime_array_values));
      if (datetime_array_values == NULL) {
        return CPKT_OPCUA_ERR_ALLOC;
      }
    }
    for (i = 0; i < value->datetime_array_length; ++i) {
      if (!cpkt_valid_datetime_words(value->datetime_array_values[i].high32,
                                     value->datetime_array_values[i].low32)) {
        free(datetime_array_values);
        return CPKT_OPCUA_ERR_RANGE;
      }
      datetime_array_values[i] =
          cpkt_make_datetime(value->datetime_array_values[i]);
    }
    status = UA_Variant_setArrayCopy(variant, datetime_array_values,
                                     value->datetime_array_length,
                                     &UA_TYPES[UA_TYPES_DATETIME]);
    free(datetime_array_values);
    if (status != UA_STATUSCODE_GOOD) {
      return CPKT_OPCUA_ERR_ALLOC;
    }
    break;
  case CPKT_OPCUA_VALUE_INTEGER_ARRAY:
    if (value->integer_array_values == NULL &&
        value->integer_array_length != 0) {
      return CPKT_OPCUA_ERR_ARG;
    }
    integer_array_values = NULL;
    if (value->integer_array_length != 0) {
      integer_array_values = (UA_Int32 *)calloc(value->integer_array_length,
                                                sizeof(*integer_array_values));
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
    status = UA_Variant_setArrayCopy(variant, integer_array_values,
                                     value->integer_array_length,
                                     &UA_TYPES[UA_TYPES_INT32]);
    free(integer_array_values);
    if (status != UA_STATUSCODE_GOOD) {
      return CPKT_OPCUA_ERR_ALLOC;
    }
    break;
  case CPKT_OPCUA_VALUE_DOUBLE:
    double_value = (UA_Double)value->double_value;
    UA_Variant_setScalarCopy(variant, &double_value,
                             &UA_TYPES[UA_TYPES_DOUBLE]);
    break;
  case CPKT_OPCUA_VALUE_DOUBLE_ARRAY:
    if (value->double_array_values == NULL && value->double_array_length != 0) {
      return CPKT_OPCUA_ERR_ARG;
    }
    double_array_values = NULL;
    if (value->double_array_length != 0) {
      double_array_values = (UA_Double *)calloc(value->double_array_length,
                                                sizeof(*double_array_values));
      if (double_array_values == NULL) {
        return CPKT_OPCUA_ERR_ALLOC;
      }
    }
    for (i = 0; i < value->double_array_length; ++i) {
      double_array_values[i] = (UA_Double)value->double_array_values[i];
    }
    status = UA_Variant_setArrayCopy(variant, double_array_values,
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
    UA_Variant_setScalarCopy(variant, &string_value,
                             &UA_TYPES[UA_TYPES_STRING]);
    break;
  case CPKT_OPCUA_VALUE_STRING_ARRAY:
    if (value->string_array_values == NULL && value->string_array_length != 0) {
      return CPKT_OPCUA_ERR_ARG;
    }
    string_array_values = NULL;
    if (value->string_array_length != 0) {
      string_array_values = (UA_String *)calloc(value->string_array_length,
                                                sizeof(*string_array_values));
      if (string_array_values == NULL) {
        return CPKT_OPCUA_ERR_ALLOC;
      }
    }
    for (i = 0; i < value->string_array_length; ++i) {
      if (value->string_array_values[i].data == NULL &&
          value->string_array_values[i].length != 0) {
        free(string_array_values);
        return CPKT_OPCUA_ERR_ARG;
      }
      string_array_values[i] = UA_STRING_NULL;
      string_array_values[i].data =
          (UA_Byte *)value->string_array_values[i].data;
      string_array_values[i].length = value->string_array_values[i].length;
    }
    status = UA_Variant_setArrayCopy(variant, string_array_values,
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
    UA_Variant_setScalarCopy(variant, &bytes_value,
                             &UA_TYPES[UA_TYPES_BYTESTRING]);
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
      guid_array_values = (UA_Guid *)calloc(value->guid_array_length,
                                            sizeof(*guid_array_values));
      if (guid_array_values == NULL) {
        return CPKT_OPCUA_ERR_ALLOC;
      }
    }
    for (i = 0; i < value->guid_array_length; ++i) {
      guid_array_values[i] = cpkt_make_guid(value->guid_array_values[i].bytes);
    }
    status = UA_Variant_setArrayCopy(variant, guid_array_values,
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
    UA_Variant_setScalarCopy(variant, &status_value,
                             &UA_TYPES[UA_TYPES_STATUSCODE]);
    break;
  case CPKT_OPCUA_VALUE_STATUS_ARRAY:
    if (value->status_array_values == NULL && value->status_array_length != 0) {
      return CPKT_OPCUA_ERR_ARG;
    }
    status_array_values = NULL;
    if (value->status_array_length != 0) {
      status_array_values = (UA_StatusCode *)calloc(
          value->status_array_length, sizeof(*status_array_values));
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
    status = UA_Variant_setArrayCopy(variant, status_array_values,
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
    qualified_name_value.namespaceIndex =
        (UA_UInt16)value->qualified_name_namespace_index;
    qualified_name_value.name = UA_STRING_NULL;
    qualified_name_value.name.data = (UA_Byte *)value->qualified_name;
    qualified_name_value.name.length = value->qualified_name_length;
    UA_Variant_setScalarCopy(variant, &qualified_name_value,
                             &UA_TYPES[UA_TYPES_QUALIFIEDNAME]);
    break;
  case CPKT_OPCUA_VALUE_QUALIFIED_NAME_ARRAY:
    if (value->qualified_name_array_values == NULL &&
        value->qualified_name_array_length != 0) {
      return CPKT_OPCUA_ERR_ARG;
    }
    qualified_name_array_values = NULL;
    if (value->qualified_name_array_length != 0) {
      qualified_name_array_values =
          (UA_QualifiedName *)calloc(value->qualified_name_array_length,
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
      qualified_name_array_values[i].name.data =
          (UA_Byte *)value->qualified_name_array_values[i].name;
      qualified_name_array_values[i].name.length =
          value->qualified_name_array_values[i].name_length;
    }
    status = UA_Variant_setArrayCopy(variant, qualified_name_array_values,
                                     value->qualified_name_array_length,
                                     &UA_TYPES[UA_TYPES_QUALIFIEDNAME]);
    free(qualified_name_array_values);
    if (status != UA_STATUSCODE_GOOD) {
      return CPKT_OPCUA_ERR_ALLOC;
    }
    break;
  case CPKT_OPCUA_VALUE_LOCALIZED_TEXT:
    if ((value->localized_text_locale == NULL &&
         value->localized_text_locale_length != 0) ||
        (value->localized_text == NULL && value->localized_text_length != 0)) {
      return CPKT_OPCUA_ERR_ARG;
    }
    localized_text_value.locale = UA_STRING_NULL;
    localized_text_value.locale.data = (UA_Byte *)value->localized_text_locale;
    localized_text_value.locale.length = value->localized_text_locale_length;
    localized_text_value.text = UA_STRING_NULL;
    localized_text_value.text.data = (UA_Byte *)value->localized_text;
    localized_text_value.text.length = value->localized_text_length;
    UA_Variant_setScalarCopy(variant, &localized_text_value,
                             &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]);
    break;
  case CPKT_OPCUA_VALUE_LOCALIZED_TEXT_ARRAY:
    if (value->localized_text_array_values == NULL &&
        value->localized_text_array_length != 0) {
      return CPKT_OPCUA_ERR_ARG;
    }
    localized_text_array_values = NULL;
    if (value->localized_text_array_length != 0) {
      localized_text_array_values =
          (UA_LocalizedText *)calloc(value->localized_text_array_length,
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
    status = UA_Variant_setArrayCopy(variant, localized_text_array_values,
                                     value->localized_text_array_length,
                                     &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]);
    free(localized_text_array_values);
    if (status != UA_STATUSCODE_GOOD) {
      return CPKT_OPCUA_ERR_ALLOC;
    }
    break;
  case CPKT_OPCUA_VALUE_BYTE_STRING_ARRAY:
    if (value->byte_string_array_values == NULL &&
        value->byte_string_array_length != 0) {
      return CPKT_OPCUA_ERR_ARG;
    }
    byte_string_array_values = NULL;
    if (value->byte_string_array_length != 0) {
      byte_string_array_values = (UA_ByteString *)calloc(
          value->byte_string_array_length, sizeof(*byte_string_array_values));
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
      byte_string_array_values[i].data =
          (UA_Byte *)value->byte_string_array_values[i].data;
      byte_string_array_values[i].length =
          value->byte_string_array_values[i].length;
    }
    status = UA_Variant_setArrayCopy(variant, byte_string_array_values,
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
    if ((value->type == CPKT_OPCUA_VALUE_BOOLEAN_ARRAY &&
         value->boolean_array_length == 0) ||
        (value->type == CPKT_OPCUA_VALUE_INTEGER_ARRAY &&
         value->integer_array_length == 0) ||
        (value->type == CPKT_OPCUA_VALUE_UINT64_ARRAY &&
         value->uint64_array_length == 0) ||
        (value->type == CPKT_OPCUA_VALUE_DATETIME_ARRAY &&
         value->datetime_array_length == 0) ||
        (value->type == CPKT_OPCUA_VALUE_STATUS_ARRAY &&
         value->status_array_length == 0) ||
        (value->type == CPKT_OPCUA_VALUE_GUID_ARRAY &&
         value->guid_array_length == 0) ||
        (value->type == CPKT_OPCUA_VALUE_QUALIFIED_NAME_ARRAY &&
         value->qualified_name_array_length == 0) ||
        (value->type == CPKT_OPCUA_VALUE_LOCALIZED_TEXT_ARRAY &&
         value->localized_text_array_length == 0) ||
        (value->type == CPKT_OPCUA_VALUE_DOUBLE_ARRAY &&
         value->double_array_length == 0) ||
        (value->type == CPKT_OPCUA_VALUE_STRING_ARRAY &&
         value->string_array_length == 0) ||
        (value->type == CPKT_OPCUA_VALUE_BYTE_STRING_ARRAY &&
         value->byte_string_array_length == 0)) {
      return CPKT_OPCUA_OK;
    }
    return CPKT_OPCUA_ERR_ALLOC;
  }
  return CPKT_OPCUA_OK;
}

cpkt_opcua_result cpkt_set_variant_array(UA_Variant *variants,
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

cpkt_opcua_result cpkt_get_variant(const UA_Variant *variant,
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
    value_out->qualified_name_namespace_index =
        (unsigned short)qualified_name_value->namespaceIndex;
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
      memcpy(string_buffer, qualified_name_value->name.data,
             qualified_name_value->name.length);
    }
    string_buffer[qualified_name_value->name.length] = '\0';
    value_out->qualified_name = string_buffer;
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_LOCALIZEDTEXT])) {
    localized_text_value = (UA_LocalizedText *)variant->data;
    value_out->type = CPKT_OPCUA_VALUE_LOCALIZED_TEXT;
    value_out->localized_text_locale_length =
        localized_text_value->locale.length;
    value_out->localized_text_length = localized_text_value->text.length;
    required = localized_text_value->locale.length + 1 +
               localized_text_value->text.length + 1;
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
      memcpy(string_buffer, localized_text_value->locale.data,
             localized_text_value->locale.length);
    }
    string_buffer[localized_text_value->locale.length] = '\0';
    value_out->localized_text_locale = string_buffer;
    string_buffer += localized_text_value->locale.length + 1;
    if (localized_text_value->text.length != 0) {
      memcpy(string_buffer, localized_text_value->text.data,
             localized_text_value->text.length);
    }
    string_buffer[localized_text_value->text.length] = '\0';
    value_out->localized_text = string_buffer;
    return CPKT_OPCUA_OK;
  }

  return CPKT_OPCUA_ERR_TYPE;
}

cpkt_opcua_result cpkt_get_variant_borrowed(const UA_Variant *variant,
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
    value_out->qualified_name_namespace_index =
        qualified_name_value->namespaceIndex;
    value_out->qualified_name = (const char *)qualified_name_value->name.data;
    value_out->qualified_name_length = qualified_name_value->name.length;
    return CPKT_OPCUA_OK;
  }
  if (UA_Variant_hasScalarType(variant, &UA_TYPES[UA_TYPES_LOCALIZEDTEXT])) {
    localized_text_value = (UA_LocalizedText *)variant->data;
    value_out->type = CPKT_OPCUA_VALUE_LOCALIZED_TEXT;
    value_out->localized_text_locale =
        (const char *)localized_text_value->locale.data;
    value_out->localized_text_locale_length =
        localized_text_value->locale.length;
    value_out->localized_text = (const char *)localized_text_value->text.data;
    value_out->localized_text_length = localized_text_value->text.length;
    return CPKT_OPCUA_OK;
  }
  return CPKT_OPCUA_ERR_TYPE;
}

cpkt_opcua_result cpkt_get_data_value(const UA_DataValue *data_value,
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
    data_value_out->source_timestamp =
        cpkt_datetime_from_native(data_value->sourceTimestamp);
  }
  if (data_value->hasServerTimestamp) {
    data_value_out->has_server_timestamp = 1;
    data_value_out->server_timestamp =
        cpkt_datetime_from_native(data_value->serverTimestamp);
  }
  if (!data_value->hasValue) {
    return CPKT_OPCUA_OK;
  }
  result = cpkt_get_variant(&data_value->value, &data_value_out->value,
                            string_buffer, string_buffer_size,
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
void cpkt_opcua_value_string(cpkt_opcua_value *value, const char *string_value,
                             size_t string_length) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_STRING;
    value->string_value = string_value;
    value->string_length = string_length;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_byte_string(cpkt_opcua_value *value,
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
void cpkt_opcua_value_guid(cpkt_opcua_value *value,
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
void cpkt_opcua_value_status(cpkt_opcua_value *value,
                             cpkt_opcua_status status_value) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_STATUS;
    value->status_value = status_value;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_qualified_name(cpkt_opcua_value *value,
                                     unsigned short namespace_index,
                                     const char *name, size_t name_length) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_QUALIFIED_NAME;
    value->qualified_name_namespace_index = namespace_index;
    value->qualified_name = name;
    value->qualified_name_length = name_length;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_localized_text(cpkt_opcua_value *value,
                                     const char *locale, size_t locale_length,
                                     const char *text, size_t text_length) {
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
void cpkt_opcua_value_uint64(cpkt_opcua_value *value, unsigned long high32,
                             unsigned long low32) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_UINT64;
    value->uint64_value.high32 = high32;
    value->uint64_value.low32 = low32;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_datetime(cpkt_opcua_value *value, long high32,
                               unsigned long low32) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_DATETIME;
    value->datetime_value.high32 = high32;
    value->datetime_value.low32 = low32;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_uint64_array(cpkt_opcua_value *value,
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
void cpkt_opcua_value_datetime_array(cpkt_opcua_value *value,
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
void cpkt_opcua_value_status_array(cpkt_opcua_value *value,
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
void cpkt_opcua_value_guid_array(cpkt_opcua_value *value,
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
    cpkt_opcua_value *value, const cpkt_opcua_qualified_name_view *values,
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
    cpkt_opcua_value *value, const cpkt_opcua_localized_text_view *values,
    size_t value_count) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_LOCALIZED_TEXT_ARRAY;
    value->localized_text_array_values = values;
    value->localized_text_array_length = value_count;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_boolean_array(cpkt_opcua_value *value, const int *values,
                                    size_t value_count) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_BOOLEAN_ARRAY;
    value->boolean_array_values = values;
    value->boolean_array_length = value_count;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_integer_array(cpkt_opcua_value *value, const long *values,
                                    size_t value_count) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_INTEGER_ARRAY;
    value->integer_array_values = values;
    value->integer_array_length = value_count;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_double_array(cpkt_opcua_value *value,
                                   const double *values, size_t value_count) {
  cpkt_opcua_value_clear(value);
  if (value != NULL) {
    value->type = CPKT_OPCUA_VALUE_DOUBLE_ARRAY;
    value->double_array_values = values;
    value->double_array_length = value_count;
  }
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_value_string_array(cpkt_opcua_value *value,
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
    cpkt_opcua_value *value, const cpkt_opcua_byte_string_view *values,
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
