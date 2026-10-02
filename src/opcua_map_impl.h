/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *    Copyright 2014, 2017 (c) Fraunhofer IOSB (Author: Julius Pfrommer)
 *    Copyright 2014 (c) Florian Palm
 *    Copyright 2017 (c) Stefan Profanter, fortiss GmbH
 */

/* Native key/value map semantics on generated C89 storage. Native records
 * cannot be reinterpreted: Variants and 64-bit payloads have different layouts.
 * Borrowed lookup and shallow insertion must keep the caller's actual address.
 */
const cpkt_opcua_KeyValueMap cpkt_opcua_KEYVALUEMAP_NULL = {0, NULL};
cpkt_opcua_KeyValueMap *cpkt_opcua_KeyValueMap_new(void) {
  return (cpkt_opcua_KeyValueMap *)UA_calloc(1, sizeof(cpkt_opcua_KeyValueMap));
}
static int cpkt_map_key_equal(const cpkt_opcua_QualifiedName *a,
                              const cpkt_opcua_QualifiedName *b) {
  UA_String first, second;
  first.length = a->name.length;
  first.data = a->name.data;
  second.length = b->name.length;
  second.data = b->name.data;
  return a->namespaceIndex == b->namespaceIndex &&
         UA_String_equal(&first, &second);
}
const cpkt_opcua_Variant *
cpkt_opcua_KeyValueMap_get(const cpkt_opcua_KeyValueMap *map,
                           cpkt_opcua_QualifiedName key) {
  size_t i;
  if (!map)
    return NULL;
  for (i = 0; i < map->mapSize; ++i)
    if (cpkt_map_key_equal(&map->map[i].key, &key))
      return &map->map[i].value;
  return NULL;
}
cpkt_opcua_Boolean
cpkt_opcua_KeyValueMap_isEmpty(const cpkt_opcua_KeyValueMap *map) {
  return !map || !map->mapSize;
}
cpkt_opcua_Boolean
cpkt_opcua_KeyValueMap_contains(const cpkt_opcua_KeyValueMap *map,
                                cpkt_opcua_QualifiedName key) {
  return cpkt_opcua_KeyValueMap_get(map, key) != NULL;
}
const void *cpkt_opcua_KeyValueMap_getScalar(const cpkt_opcua_KeyValueMap *map,
                                             cpkt_opcua_QualifiedName key,
                                             const cpkt_opcua_Type *type) {
  const cpkt_opcua_Variant *value = cpkt_opcua_KeyValueMap_get(map, key);
  return value && cpkt_opcua_Variant_hasScalarType(value, type) ? value->data
                                                                : NULL;
}
void cpkt_opcua_KeyValueMap_clear(cpkt_opcua_KeyValueMap *map) {
  if (!map)
    return;
  if (map->mapSize)
    cpkt_opcua_array_delete(map->map, map->mapSize,
                            &cpkt_types[CPKT_OPCUA_TYPES_KEYVALUEPAIR]);
  map->map = NULL;
  map->mapSize = 0;
}
void cpkt_opcua_KeyValueMap_delete(cpkt_opcua_KeyValueMap *map) {
  cpkt_opcua_KeyValueMap_clear(map);
  UA_free(map);
}
cpkt_opcua_StatusCode
cpkt_opcua_KeyValueMap_set(cpkt_opcua_KeyValueMap *map,
                           cpkt_opcua_QualifiedName key,
                           const cpkt_opcua_Variant *value) {
  cpkt_opcua_Variant *target, copy;
  cpkt_opcua_KeyValuePair pair;
  UA_StatusCode status;
  if (!map || !value)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  target = (cpkt_opcua_Variant *)cpkt_opcua_KeyValueMap_get(map, key);
  if (target) {
    status = cpkt_opcua_Variant_copy(value, &copy);
    if (status)
      return status;
    cpkt_opcua_Variant_clear(target);
    *target = copy;
    return 0;
  }
  pair.key = key;
  pair.value = *value;
  return cpkt_opcua_array_appendCopy(
      (void **)&map->map, &map->mapSize, &pair,
      &cpkt_types[CPKT_OPCUA_TYPES_KEYVALUEPAIR]);
}
cpkt_opcua_StatusCode
cpkt_opcua_KeyValueMap_setShallow(cpkt_opcua_KeyValueMap *map,
                                  cpkt_opcua_QualifiedName key,
                                  cpkt_opcua_Variant *value) {
  cpkt_opcua_Variant *target;
  cpkt_opcua_KeyValuePair pair;
  UA_StatusCode status;
  if (!map || !value)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  target = (cpkt_opcua_Variant *)cpkt_opcua_KeyValueMap_get(map, key);
  if (target)
    cpkt_opcua_Variant_clear(target);
  else {
    pair.key = key;
    cpkt_opcua_Variant_init(&pair.value);
    status =
        cpkt_opcua_array_appendCopy((void **)&map->map, &map->mapSize, &pair,
                                    &cpkt_types[CPKT_OPCUA_TYPES_KEYVALUEPAIR]);
    if (status)
      return status;
    target = &map->map[map->mapSize - 1].value;
  }
  *target = *value;
  target->storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
  return 0;
}
cpkt_opcua_StatusCode cpkt_opcua_KeyValueMap_setScalar(
    cpkt_opcua_KeyValueMap *map, cpkt_opcua_QualifiedName key,
    const void *value, const cpkt_opcua_Type *type) {
  cpkt_opcua_Variant variant;
  if (!value || !cpkt_valid_type(type))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  cpkt_opcua_Variant_init(&variant);
  variant.type = type;
  variant.data = (void *)value;
  return cpkt_opcua_KeyValueMap_set(map, key, &variant);
}
cpkt_opcua_StatusCode cpkt_opcua_KeyValueMap_setScalarShallow(
    cpkt_opcua_KeyValueMap *map, cpkt_opcua_QualifiedName key, void *value,
    const cpkt_opcua_Type *type) {
  cpkt_opcua_Variant variant;
  if (!value || !cpkt_valid_type(type))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  cpkt_opcua_Variant_init(&variant);
  variant.type = type;
  variant.data = value;
  return cpkt_opcua_KeyValueMap_setShallow(map, key, &variant);
}
cpkt_opcua_StatusCode
cpkt_opcua_KeyValueMap_remove(cpkt_opcua_KeyValueMap *map,
                              cpkt_opcua_QualifiedName key) {
  size_t i;
  UA_StatusCode status;
  if (!map)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  for (i = 0; i < map->mapSize; ++i)
    if (cpkt_map_key_equal(&map->map[i].key, &key))
      break;
  if (i == map->mapSize)
    return UA_STATUSCODE_BADNOTFOUND;
  cpkt_opcua_KeyValuePair_clear(&map->map[i]);
  if (i < map->mapSize - 1) {
    map->map[i] = map->map[map->mapSize - 1];
    cpkt_opcua_KeyValuePair_init(&map->map[map->mapSize - 1]);
  }
  status = cpkt_opcua_array_resize((void **)&map->map, &map->mapSize,
                                   map->mapSize - 1,
                                   &cpkt_types[CPKT_OPCUA_TYPES_KEYVALUEPAIR]);
  if (status)
    --map->mapSize;
  return 0;
}
cpkt_opcua_StatusCode
cpkt_opcua_KeyValueMap_copy(const cpkt_opcua_KeyValueMap *src,
                            cpkt_opcua_KeyValueMap *dst) {
  UA_StatusCode status;
  if (!dst)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!src) {
    dst->map = NULL;
    dst->mapSize = 0;
    return 0;
  }
  status = cpkt_opcua_array_copy(src->map, src->mapSize, (void **)&dst->map,
                                 &cpkt_types[CPKT_OPCUA_TYPES_KEYVALUEPAIR]);
  if (!status)
    dst->mapSize = src->mapSize;
  return status;
}
cpkt_opcua_StatusCode
cpkt_opcua_KeyValueMap_merge(cpkt_opcua_KeyValueMap *lhs,
                             const cpkt_opcua_KeyValueMap *rhs) {
  cpkt_opcua_KeyValueMap merged = {0, NULL};
  size_t i;
  UA_StatusCode status;
  if (!lhs)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!rhs)
    return 0;
  status = cpkt_opcua_KeyValueMap_copy(lhs, &merged);
  if (status)
    return status;
  for (i = 0; i < rhs->mapSize; ++i) {
    status = cpkt_opcua_KeyValueMap_set(&merged, rhs->map[i].key,
                                        &rhs->map[i].value);
    if (status) {
      cpkt_opcua_KeyValueMap_clear(&merged);
      return status;
    }
  }
  cpkt_opcua_KeyValueMap_clear(lhs);
  *lhs = merged;
  return 0;
}
