/* C89 representation utilities. Borrowed native views avoid allocations for
 * predicates; ownership setters operate on C89 storage without copying it. */
static UA_Variant cpkt_variant_view(const cpkt_opcua_Variant *value) {
  UA_Variant native;
  UA_Variant_init(&native);
  if (value) {
    native.type = value->type ? value->type->native : NULL;
    native.data = value->data;
    native.arrayLength = value->arrayLength;
  }
  return native;
}
cpkt_opcua_StatusCode cpkt_opcua_type_order(const void *a, const void *b,
                                            const cpkt_opcua_Type *type,
                                            cpkt_opcua_Order *order) {
  void *na, *nb;
  UA_StatusCode status;
  if (order)
    *order = CPKT_OPCUA_ORDER_EQ;
  if (!a || !b || !order || !cpkt_valid_type(type))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  na = UA_new(type->native);
  nb = UA_new(type->native);
  if (!na || !nb)
    status = UA_STATUSCODE_BADOUTOFMEMORY;
  else {
    status = cpkt_convert(a, na, type, 1, 0);
    if (!status)
      status = cpkt_convert(b, nb, type, 1, 0);
    if (!status)
      *order = (cpkt_opcua_Order)UA_order(na, nb, type->native);
  }
  if (na)
    UA_delete(na, type->native);
  if (nb)
    UA_delete(nb, type->native);
  return status;
}
cpkt_opcua_StatusCode cpkt_opcua_array_resize(void **array, size_t *length,
                                              size_t new_length,
                                              const cpkt_opcua_Type *type) {
  void *replacement, *removed = NULL, *old;
  size_t i, removed_count;
  if (!array || !length || !cpkt_valid_type(type))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (*length == new_length)
    return 0;
  if (*length > (size_t)-1 / type->size || new_length > (size_t)-1 / type->size)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  if (*length && (!*array || *array == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!new_length) {
    cpkt_opcua_array_delete(*array, *length, type);
    *array = CPKT_OPCUA_EMPTY_ARRAY_SENTINEL;
    *length = 0;
    return 0;
  }
  removed_count = *length > new_length ? *length - new_length : 0;
  if (removed_count && !type->native->pointerFree) {
    removed = UA_malloc(removed_count * type->size);
    if (!removed)
      return UA_STATUSCODE_BADOUTOFMEMORY;
    memcpy(removed, (char *)*array + new_length * type->size,
           removed_count * type->size);
  }
  old = *array == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL ? NULL : *array;
  replacement = UA_realloc(old, new_length * type->size);
  if (!replacement) {
    UA_free(removed);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  if (new_length > *length)
    memset((char *)replacement + *length * type->size, 0,
           (new_length - *length) * type->size);
  if (removed) {
    for (i = 0; i < removed_count; ++i)
      cpkt_opcua_type_clear((char *)removed + i * type->size, type);
    UA_free(removed);
  }
  *array = replacement;
  *length = new_length;
  return 0;
}
cpkt_opcua_StatusCode cpkt_opcua_array_append(void **array, size_t *length,
                                              void *element,
                                              const cpkt_opcua_Type *type) {
  size_t old_length;
  UA_StatusCode status;
  if (!array || !length || !element || !cpkt_valid_type(type))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  old_length = *length;
  if (old_length == (size_t)-1)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  status = cpkt_opcua_array_resize(array, length, old_length + 1, type);
  if (!status) {
    memcpy((char *)*array + old_length * type->size, element, type->size);
    memset(element, 0, type->size);
  }
  return status;
}
cpkt_opcua_StatusCode cpkt_opcua_array_appendCopy(void **array, size_t *length,
                                                  const void *element,
                                                  const cpkt_opcua_Type *type) {
  void *copy;
  UA_StatusCode status;
  if (!array || !length || !element || !cpkt_valid_type(type))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (type->native->memSize > 512)
    return UA_STATUSCODE_BADINTERNALERROR;
  copy = cpkt_opcua_type_new(type);
  if (!copy)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  status = cpkt_opcua_type_copy(element, copy, type);
  if (!status)
    status = cpkt_opcua_array_append(array, length, copy, type);
  cpkt_opcua_type_delete(copy, type);
  return status;
}
cpkt_opcua_Boolean cpkt_opcua_Variant_isEmpty(const cpkt_opcua_Variant *value) {
  UA_Variant native = cpkt_variant_view(value);
  return UA_Variant_isEmpty(&native);
}
cpkt_opcua_Boolean
cpkt_opcua_Variant_isScalar(const cpkt_opcua_Variant *value) {
  UA_Variant native = cpkt_variant_view(value);
  return UA_Variant_isScalar(&native);
}
cpkt_opcua_Boolean cpkt_opcua_Variant_isArray(const cpkt_opcua_Variant *value) {
  UA_Variant native = cpkt_variant_view(value);
  return UA_Variant_isArray(&native);
}
cpkt_opcua_Boolean
cpkt_opcua_Variant_hasScalarType(const cpkt_opcua_Variant *value,
                                 const cpkt_opcua_Type *type) {
  UA_Variant native = cpkt_variant_view(value);
  return UA_Variant_hasScalarType(&native, type ? type->native : NULL);
}
cpkt_opcua_Boolean
cpkt_opcua_Variant_hasArrayType(const cpkt_opcua_Variant *value,
                                const cpkt_opcua_Type *type) {
  UA_Variant native = cpkt_variant_view(value);
  return UA_Variant_hasArrayType(&native, type ? type->native : NULL);
}
void cpkt_opcua_Variant_setScalar(cpkt_opcua_Variant *value, void *scalar,
                                  const cpkt_opcua_Type *type) {
  if (!value)
    return;
  cpkt_opcua_Variant_init(value);
  value->data = scalar;
  value->type = type;
}
void cpkt_opcua_Variant_setArray(cpkt_opcua_Variant *value, void *array,
                                 size_t length, const cpkt_opcua_Type *type) {
  cpkt_opcua_Variant_setScalar(value, array, type);
  if (value)
    value->arrayLength = length;
}
cpkt_opcua_StatusCode
cpkt_opcua_Variant_setScalarCopy(cpkt_opcua_Variant *value, const void *scalar,
                                 const cpkt_opcua_Type *type) {
  void *copy;
  UA_StatusCode status;
  if (!value || !scalar || !cpkt_valid_type(type))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  copy = cpkt_opcua_type_new(type);
  if (!copy)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  status = cpkt_opcua_type_copy(scalar, copy, type);
  if (status)
    cpkt_opcua_type_delete(copy, type);
  else
    cpkt_opcua_Variant_setScalar(value, copy, type);
  return status;
}
cpkt_opcua_StatusCode
cpkt_opcua_Variant_setArrayCopy(cpkt_opcua_Variant *value, const void *array,
                                size_t length, const cpkt_opcua_Type *type) {
  void *copy;
  UA_StatusCode status;
  if (!value || !cpkt_valid_type(type))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  cpkt_opcua_Variant_init(value);
  status = cpkt_opcua_array_copy(array, length, &copy, type);
  if (!status)
    cpkt_opcua_Variant_setArray(value, copy, length, type);
  return status;
}
void cpkt_opcua_ExtensionObject_setValue(cpkt_opcua_ExtensionObject *value,
                                         void *payload,
                                         const cpkt_opcua_Type *type) {
  if (!value)
    return;
  cpkt_opcua_ExtensionObject_init(value);
  value->encoding = CPKT_OPCUA_EXTENSIONOBJECT_DECODED;
  value->content.decoded.data = payload;
  value->content.decoded.type = type;
}
void cpkt_opcua_ExtensionObject_setValueNoDelete(
    cpkt_opcua_ExtensionObject *value, void *payload,
    const cpkt_opcua_Type *type) {
  cpkt_opcua_ExtensionObject_setValue(value, payload, type);
  if (value)
    value->encoding = CPKT_OPCUA_EXTENSIONOBJECT_DECODED_NODELETE;
}
cpkt_opcua_StatusCode
cpkt_opcua_ExtensionObject_setValueCopy(cpkt_opcua_ExtensionObject *value,
                                        const void *payload,
                                        const cpkt_opcua_Type *type) {
  void *copy;
  UA_StatusCode status;
  if (!value || !payload || !cpkt_valid_type(type))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  cpkt_opcua_ExtensionObject_init(value);
  copy = cpkt_opcua_type_new(type);
  if (!copy)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  status = cpkt_opcua_type_copy(payload, copy, type);
  if (status)
    cpkt_opcua_type_delete(copy, type);
  else
    cpkt_opcua_ExtensionObject_setValue(value, copy, type);
  return status;
}
cpkt_opcua_Boolean cpkt_opcua_ExtensionObject_hasDecodedType(
    const cpkt_opcua_ExtensionObject *value, const cpkt_opcua_Type *type) {
  UA_ExtensionObject native;
  if (!value)
    return 0;
  UA_ExtensionObject_init(&native);
  native.encoding = (UA_ExtensionObjectEncoding)value->encoding;
  if (value->encoding == CPKT_OPCUA_EXTENSIONOBJECT_DECODED ||
      value->encoding == CPKT_OPCUA_EXTENSIONOBJECT_DECODED_NODELETE)
    native.content.decoded.data = value->content.decoded.data;
  if (value->encoding == CPKT_OPCUA_EXTENSIONOBJECT_DECODED ||
      value->encoding == CPKT_OPCUA_EXTENSIONOBJECT_DECODED_NODELETE)
    native.content.decoded.type = value->content.decoded.type
                                      ? value->content.decoded.type->native
                                      : NULL;
  return UA_ExtensionObject_hasDecodedType(&native, type ? type->native : NULL);
}
cpkt_opcua_Boolean cpkt_opcua_StatusCode_isGood(cpkt_opcua_StatusCode code) {
  return UA_StatusCode_isGood(code);
}
cpkt_opcua_Boolean
cpkt_opcua_StatusCode_isUncertain(cpkt_opcua_StatusCode code) {
  return UA_StatusCode_isUncertain(code);
}
cpkt_opcua_Boolean cpkt_opcua_StatusCode_isBad(cpkt_opcua_StatusCode code) {
  return UA_StatusCode_isBad(code);
}
cpkt_opcua_Boolean cpkt_opcua_StatusCode_equalTop(cpkt_opcua_StatusCode a,
                                                  cpkt_opcua_StatusCode b) {
  return UA_StatusCode_equalTop(a, b);
}
const char *cpkt_opcua_StatusCode_name(cpkt_opcua_StatusCode code) {
  return UA_StatusCode_name(code);
}
static int cpkt_string_valid(const cpkt_opcua_String *value) {
  return value &&
         (!value->length ||
          (value->data && value->data != CPKT_OPCUA_EMPTY_ARRAY_SENTINEL));
}
static UA_String cpkt_string_view(const cpkt_opcua_String *value) {
  UA_String native;
  native.length = value->length;
  native.data = value->data;
  return native;
}
static void cpkt_string_take(cpkt_opcua_String *value, UA_String native) {
  value->length = native.length;
  value->data = native.data;
}
cpkt_opcua_String cpkt_opcua_String_fromChars(const char *text) {
  cpkt_opcua_String value;
  cpkt_string_take(&value, UA_String_fromChars(text));
  return value;
}
cpkt_opcua_Boolean cpkt_opcua_String_isEmpty(const cpkt_opcua_String *value) {
  UA_String native;
  if (!value)
    return 1;
  native = cpkt_string_view(value);
  return UA_String_isEmpty(&native);
}
cpkt_opcua_StatusCode cpkt_opcua_String_append(cpkt_opcua_String *value,
                                               cpkt_opcua_String suffix) {
  UA_String native, addition;
  UA_StatusCode status;
  if (!cpkt_string_valid(value) || !cpkt_string_valid(&suffix))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (suffix.length > (size_t)-1 - value->length)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  native = cpkt_string_view(value);
  addition = cpkt_string_view(&suffix);
  status = UA_String_append(&native, addition);
  cpkt_string_take(value, native);
  return status;
}
cpkt_opcua_StatusCode
cpkt_opcua_ByteString_allocBuffer(cpkt_opcua_ByteString *value, size_t length) {
  UA_ByteString native;
  UA_StatusCode status;
  if (!value)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  status = UA_ByteString_allocBuffer(&native, length);
  cpkt_string_take(value, native);
  return status;
}
cpkt_opcua_StatusCode
cpkt_opcua_ByteString_toBase64(const cpkt_opcua_ByteString *value,
                               cpkt_opcua_String *output) {
  UA_ByteString native;
  UA_String result = UA_STRING_NULL;
  UA_StatusCode status;
  if (!output || (value && value->length &&
                  value->data == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  native = value ? cpkt_string_view(value) : UA_BYTESTRING_NULL;
  status = UA_ByteString_toBase64(&native, &result);
  cpkt_string_take(output, result);
  return status;
}
cpkt_opcua_StatusCode
cpkt_opcua_ByteString_fromBase64(cpkt_opcua_ByteString *output,
                                 const cpkt_opcua_String *value) {
  UA_String native;
  UA_ByteString result = UA_BYTESTRING_NULL;
  UA_StatusCode status;
  if (!output || !cpkt_string_valid(value))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  native = cpkt_string_view(value);
  status = UA_ByteString_fromBase64(&result, &native);
  cpkt_string_take(output, result);
  return status;
}
void cpkt_opcua_ByteString_memZero(cpkt_opcua_ByteString *value) {
  UA_ByteString native;
  if (!cpkt_string_valid(value))
    return;
  native = cpkt_string_view(value);
  UA_ByteString_memZero(&native);
}
cpkt_opcua_UInt32 cpkt_opcua_ByteString_hash(cpkt_opcua_UInt32 initial,
                                             const cpkt_opcua_Byte *bytes,
                                             size_t length) {
  return (length &&
          (!bytes || (const void *)bytes == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL))
             ? 0
             : UA_ByteString_hash(initial, bytes, length);
}
