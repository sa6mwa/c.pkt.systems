/* Native range semantics on C89 payloads. Native selects/validates positions;
 * C89 moves transfer actual caller pointers rather than replacing their values
 * with reallocated copies. Existing array/dimension storage never moves. */
#ifndef CPKT_OPCUA_RANGE_IMPL_H
#define CPKT_OPCUA_RANGE_IMPL_H
static UA_StatusCode cpkt_variant_insert_range(cpkt_opcua_Variant *value,
                                               const void *array, size_t count,
                                               cpkt_opcua_NumericRange range,
                                               int copy) {
  const cpkt_opcua_Type *type;
  UA_NumericRange native_range;
  UA_Variant destination, indexes, positions;
  UA_UInt64 *index_values = NULL, *selected;
  void *native_input = NULL, *public_output = NULL;
  UA_StatusCode status, native_status = 0;
  size_t i, position;
  char *slot;
  if (!value || !(type = cpkt_valid_type(value->type)) ||
      (count && (!array || array == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL)) ||
      (value->arrayLength &&
       (!value->data || value->data == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL)))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memset(&native_range, 0, sizeof(native_range));
  UA_Variant_init(&destination);
  UA_Variant_init(&indexes);
  UA_Variant_init(&positions);
  destination.type = type->native;
  destination.arrayLength = value->arrayLength;
  status = cpkt_utility_range(range, &native_range);
  if (!status) {
    status = cpkt_array(value->arrayDimensions, value->arrayDimensionsSize,
                        (void **)&destination.arrayDimensions,
                        &cpkt_types[CPKT_OPCUA_TYPES_UINT32], 1, 0);
    destination.arrayDimensionsSize =
        destination.arrayDimensions ? value->arrayDimensionsSize : 0;
  }
  if (status)
    goto done;
  /* A nonempty valid range cannot match a zero input count. This native call
   * validates metadata without accessing a payload or copying any members. */
  status = UA_Variant_setRange(&destination, NULL, 0, native_range);
  if (status != UA_STATUSCODE_BADINDEXRANGEINVALID)
    goto done;
  index_values = (UA_UInt64 *)UA_Array_new(
      value->arrayLength, cpkt_types[CPKT_OPCUA_TYPES_UINT64].native);
  if (!index_values) {
    status = UA_STATUSCODE_BADOUTOFMEMORY;
    goto done;
  }
  for (i = 0; i < value->arrayLength; ++i)
    index_values[i] = (UA_UInt64)i;
  indexes.type = cpkt_types[CPKT_OPCUA_TYPES_UINT64].native;
  indexes.storageType = UA_VARIANT_DATA_NODELETE;
  indexes.data = index_values;
  indexes.arrayLength = value->arrayLength;
  indexes.arrayDimensions = destination.arrayDimensions;
  indexes.arrayDimensionsSize = destination.arrayDimensionsSize;
  status = UA_Variant_copyRange(&indexes, &positions, native_range);
  if (status)
    goto done;
  if (positions.arrayLength != count) {
    /* The native dry validation and read selection establish this mismatch
     * without passing a NULL payload to a possibly writing operation. */
    status = UA_STATUSCODE_BADINDEXRANGEINVALID;
    goto done;
  }
  destination.data = UA_Array_new(value->arrayLength, type->native);
  if (!destination.data) {
    status = UA_STATUSCODE_BADOUTOFMEMORY;
    goto done;
  }
  status = cpkt_array(array, count, &native_input, type, 1, 0);
  if (status)
    goto done;
  native_status = copy ? UA_Variant_setRangeCopy(&destination, native_input,
                                                 count, native_range)
                       : UA_Variant_setRange(&destination, native_input, count,
                                             native_range);
  selected = (UA_UInt64 *)positions.data;
  if (copy) {
    /* All selected output slots are staged before committing C89 ownership.
     * Failed native copies can leave empty/partial values; preserve them. */
    public_output = cpkt_opcua_array_new(count, type);
    if (!public_output) {
      status = UA_STATUSCODE_BADOUTOFMEMORY;
      goto done;
    }
    for (i = 0; i < count; ++i) {
      position = (size_t)selected[i];
      status = cpkt_convert((char *)destination.data +
                                position * type->native->memSize,
                            (char *)public_output + i * type->size, type, 0, 0);
      if (status)
        goto done;
    }
  } else if (native_status) {
    /* Native move is all-or-nothing once validation has passed. */
    goto done;
  }
  for (i = 0; i < count; ++i) {
    position = (size_t)selected[i];
    slot = (char *)value->data + position * type->size;
    if (!type->native->pointerFree)
      cpkt_opcua_type_clear(slot, type);
    memcpy(slot, (const char *)(copy ? public_output : array) + i * type->size,
           type->size);
    if (copy)
      memset((char *)public_output + i * type->size, 0, type->size);
  }
  if (!copy && !type->native->pointerFree)
    memset((void *)array, 0, count * type->size);
  status = 0;
done:
  UA_Variant_clear(&destination);
  UA_Variant_clear(&positions);
  UA_Array_delete(index_values, value ? value->arrayLength : 0,
                  cpkt_types[CPKT_OPCUA_TYPES_UINT64].native);
  if (native_input)
    UA_Array_delete(native_input, count, type ? type->native : NULL);
  cpkt_opcua_array_delete(public_output, count, type);
  UA_free(native_range.dimensions);
  return status ? status : native_status;
}
cpkt_opcua_StatusCode
cpkt_opcua_Variant_setRange(cpkt_opcua_Variant *value, void *array,
                            size_t count, const cpkt_opcua_NumericRange range) {
  return cpkt_variant_insert_range(value, array, count, range, 0);
}
cpkt_opcua_StatusCode
cpkt_opcua_Variant_setRangeCopy(cpkt_opcua_Variant *value, const void *array,
                                size_t count,
                                const cpkt_opcua_NumericRange range) {
  return cpkt_variant_insert_range(value, array, count, range, 1);
}
#endif
