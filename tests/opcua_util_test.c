#include "opcua_callbacks_test.h"
#include "opcua_types_peer.h"
#include <cpkt/opcua_util.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void check(int condition, const char *expression, int line) {
  if (!condition) {
    fprintf(stderr, "utility line %d: %s\n", line, expression);
    abort();
  }
}
#define CHECK(e) check(!!(e), #e, __LINE__)
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
void cpkt_types_fail_after(size_t count);
int cpkt_types_fail_stop(void);
#endif
static cpkt_opcua_String text(const char *value) {
  cpkt_opcua_String string;
  string.length = strlen(value);
  string.data = (cpkt_opcua_Byte *)value;
  return string;
}
static void predicates(void) {
  size_t type, query, length;
  int state, encoding, populated;
  unsigned int actual, expected;
  cpkt_opcua_Variant variant;
  cpkt_opcua_ExtensionObject extension;
  cpkt_opcua_UInt64 number;
  const cpkt_opcua_Type *types[3];
  types[0] = NULL;
  types[1] = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
  types[2] = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_STRING);
  number.high32 = number.low32 = 0;
  for (type = 0; type < 3; ++type)
    for (query = 0; query < 3; ++query)
      for (length = 0; length < 2; ++length)
        for (state = 0; state < 3; ++state) {
          cpkt_opcua_Variant_init(&variant);
          variant.type = types[type];
          variant.arrayLength = length;
          variant.data = state == 2   ? (void *)&number
                         : state == 1 ? CPKT_OPCUA_EMPTY_ARRAY_SENTINEL
                                      : NULL;
          actual =
              cpkt_opcua_Variant_isEmpty(&variant) |
              (cpkt_opcua_Variant_isScalar(&variant) << 1) |
              (cpkt_opcua_Variant_isArray(&variant) << 2) |
              (cpkt_opcua_Variant_hasScalarType(&variant, types[query]) << 3) |
              (cpkt_opcua_Variant_hasArrayType(&variant, types[query]) << 4);
          expected =
              cpkt_types_peer_variant_predicates(type, query, length, state);
          CHECK(actual == expected);
        }
  for (populated = 0; populated < 2; ++populated)
    for (encoding = 0; encoding < 5; ++encoding)
      for (type = 0; type < 3; ++type)
        for (query = 0; query < 3; ++query) {
          cpkt_opcua_ExtensionObject_init(&extension);
          extension.encoding = (cpkt_opcua_ExtensionObjectEncoding)encoding;
          if (encoding >= 3)
            extension.content.decoded.type = types[type];
          if (encoding >= 3 && populated)
            extension.content.decoded.data = &number;
          CHECK(cpkt_opcua_ExtensionObject_hasDecodedType(&extension,
                                                          types[query]) ==
                cpkt_types_peer_extension_predicate(encoding, type, query,
                                                    populated));
        }
}
static void values(void) {
  cpkt_opcua_Int64 number, other, *owned;
  cpkt_opcua_Variant variant;
  cpkt_opcua_ExtensionObject extension;
  cpkt_opcua_Argument argument;
  cpkt_opcua_Order order;
  const cpkt_opcua_Type *type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
  number.high32 = 0x80000000U;
  number.low32 = 0;
  other.high32 = 0x7fffffffU;
  other.low32 = 0xffffffffU;
  CHECK(cpkt_opcua_type_order(&number, &other, type, &order) == 0 &&
        order == CPKT_OPCUA_ORDER_LESS);
  CHECK(cpkt_opcua_type_order(&other, &number, type, &order) == 0 &&
        order == CPKT_OPCUA_ORDER_MORE);
  CHECK(cpkt_opcua_type_order(&number, &number, type, &order) == 0 &&
        order == CPKT_OPCUA_ORDER_EQ);
  CHECK(cpkt_opcua_type_order(NULL, &number, type, &order) ==
            CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT &&
        order == CPKT_OPCUA_ORDER_EQ);
  cpkt_opcua_Variant_init(&variant);
  owned = cpkt_opcua_Int64_new();
  CHECK(owned != NULL);
  *owned = number;
  cpkt_opcua_Variant_setScalar(&variant, owned, type);
  CHECK(variant.data == owned &&
        cpkt_opcua_Variant_hasScalarType(&variant, type));
  cpkt_opcua_Variant_clear(&variant);
  CHECK(cpkt_opcua_Variant_setScalarCopy(&variant, &other, type) == 0);
  CHECK(variant.data != &other &&
        ((cpkt_opcua_Int64 *)variant.data)->low32 == 0xffffffffU);
  cpkt_opcua_Variant_clear(&variant);
  CHECK(cpkt_opcua_Variant_setArrayCopy(&variant, NULL, 0, type) == 0 &&
        variant.data == NULL);
  cpkt_opcua_Variant_clear(&variant);
  CHECK(cpkt_opcua_Variant_setArrayCopy(
            &variant, CPKT_OPCUA_EMPTY_ARRAY_SENTINEL, 0, type) == 0 &&
        variant.data == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL);
  cpkt_opcua_Variant_clear(&variant);
  CHECK(cpkt_opcua_Variant_setArrayCopy(&variant, &number, 1, type) == 0 &&
        cpkt_opcua_Variant_isArray(&variant));
  CHECK(((cpkt_opcua_Int64 *)variant.data)->high32 == 0x80000000U);
  cpkt_opcua_Variant_clear(&variant);
  owned = cpkt_opcua_array_new(2, type);
  CHECK(owned != NULL);
  owned[0] = number;
  owned[1] = other;
  cpkt_opcua_Variant_setArray(&variant, owned, 2, type);
  CHECK(variant.data == owned && variant.arrayLength == 2);
  cpkt_opcua_Variant_clear(&variant);
  cpkt_opcua_Argument_init(&argument);
  argument.name = text("borrowed argument");
  type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_ARGUMENT);
  cpkt_opcua_ExtensionObject_setValueNoDelete(&extension, &argument, type);
  CHECK(extension.content.decoded.data == &argument &&
        cpkt_opcua_ExtensionObject_hasDecodedType(&extension, type));
  cpkt_opcua_ExtensionObject_clear(&extension);
  CHECK(argument.name.length == 17);
  CHECK(cpkt_opcua_ExtensionObject_setValueCopy(&extension, &argument, type) ==
        0);
  CHECK(extension.content.decoded.data != &argument &&
        ((cpkt_opcua_Argument *)extension.content.decoded.data)->name.data !=
            argument.name.data);
  cpkt_opcua_ExtensionObject_clear(&extension);
}
static void arrays(void) {
  void *array = NULL;
  size_t length = 0;
  cpkt_opcua_Argument source, moved, *entries;
  const cpkt_opcua_Type *type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_ARGUMENT);
  cpkt_opcua_Argument_init(&source);
  source.name = text("nested");
  CHECK(cpkt_opcua_array_appendCopy(&array, &length, &source, type) == 0);
  entries = array;
  CHECK(length == 1 && entries[0].name.data != source.name.data);
  CHECK(cpkt_opcua_array_appendCopy(&array, &length, array, type) == 0);
  entries = array;
  CHECK(length == 2 && entries[0].name.data != entries[1].name.data);
  CHECK(cpkt_opcua_Argument_copy(&source, &moved) == 0);
  CHECK(cpkt_opcua_array_append(&array, &length, &moved, type) == 0);
  CHECK(length == 3 && moved.name.data == NULL && moved.name.length == 0);
  CHECK(cpkt_opcua_array_resize(&array, &length, 5, type) == 0);
  entries = array;
  CHECK(entries[3].name.data == NULL && entries[4].arrayDimensions == NULL);
  CHECK(cpkt_opcua_array_resize(&array, &length, 1, type) == 0 && length == 1);
  CHECK(((cpkt_opcua_Argument *)array)->name.length == 6);
  CHECK(cpkt_opcua_array_resize(&array, &length, 0, type) == 0 &&
        array == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL && length == 0);
  CHECK(cpkt_opcua_array_resize(&array, &length, 2, type) == 0 && length == 2);
  cpkt_opcua_array_delete(array, length, type);
  array = NULL;
  length = 0;
  CHECK(cpkt_opcua_array_resize(&array, &length, 0, type) == 0 &&
        array == NULL);
  CHECK(cpkt_opcua_array_resize(&array, &length, (size_t)-1, type) ==
            CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY &&
        array == NULL && length == 0);
  CHECK(cpkt_opcua_array_append(NULL, &length, &source, type) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
}
static void strings(void) {
  cpkt_opcua_String string, encoded, suffix;
  cpkt_opcua_ByteString bytes, decoded;
  const unsigned char raw[] = {0, 1, 2, 0xff, 0x80};
  unsigned int i;
  cpkt_opcua_String_init(&encoded);
  cpkt_opcua_ByteString_init(&decoded);
  string = cpkt_opcua_String_fromChars(NULL);
  CHECK(!string.data && !string.length);
  string = cpkt_opcua_String_fromChars("");
  CHECK(string.data == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL &&
        cpkt_opcua_String_isEmpty(&string));
  cpkt_opcua_String_clear(&string);
  string = cpkt_opcua_String_fromChars("abc");
  suffix = text("def");
  CHECK(string.length == 3 && !cpkt_opcua_String_isEmpty(&string));
  CHECK(cpkt_opcua_String_append(&string, suffix) == 0 && string.length == 6 &&
        !memcmp(string.data, "abcdef", 6));
  cpkt_opcua_String_clear(&string);
  CHECK(cpkt_opcua_ByteString_allocBuffer(&bytes, 0) == 0 &&
        bytes.data == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL);
  cpkt_opcua_ByteString_clear(&bytes);
  CHECK(cpkt_opcua_ByteString_allocBuffer(&bytes, sizeof(raw)) == 0);
  for (i = 0; i < sizeof(raw); ++i)
    CHECK(bytes.data[i] == 0);
  memcpy(bytes.data, raw, sizeof(raw));
  CHECK(cpkt_opcua_ByteString_toBase64(&bytes, &encoded) == 0 &&
        encoded.length == 8 && !memcmp(encoded.data, "AAEC/4A=", 8));
  CHECK(cpkt_opcua_ByteString_fromBase64(&decoded, &encoded) == 0 &&
        decoded.length == sizeof(raw) &&
        !memcmp(decoded.data, raw, sizeof(raw)));
  CHECK(cpkt_opcua_ByteString_hash(17, raw, sizeof(raw)) ==
        cpkt_types_peer_bytes_hash(17, raw, sizeof(raw)));
  CHECK(cpkt_opcua_ByteString_hash(17, NULL, 0) ==
        cpkt_types_peer_bytes_hash(17, NULL, 0));
  cpkt_opcua_ByteString_memZero(&bytes);
  for (i = 0; i < sizeof(raw); ++i)
    CHECK(bytes.data[i] == 0);
  cpkt_opcua_ByteString_clear(&bytes);
  cpkt_opcua_ByteString_clear(&decoded);
  cpkt_opcua_String_clear(&encoded);
  bytes.length = 1;
  bytes.data = CPKT_OPCUA_EMPTY_ARRAY_SENTINEL;
  CHECK(cpkt_opcua_ByteString_toBase64(&bytes, &encoded) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_ByteString_fromBase64(&decoded, &bytes) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  cpkt_opcua_ByteString_memZero(&bytes);
  CHECK(cpkt_opcua_ByteString_hash(0, bytes.data, bytes.length) == 0);
  string.length = (size_t)-1;
  string.data = (cpkt_opcua_Byte *)"unused";
  CHECK(cpkt_opcua_String_append(&string, suffix) ==
        CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY);
  CHECK(cpkt_opcua_ByteString_toBase64(NULL, &encoded) == 0 &&
        encoded.data == NULL && encoded.length == 0);
  bytes.length = 7;
  bytes.data = NULL;
  CHECK(cpkt_opcua_ByteString_toBase64(&bytes, &encoded) == 0 &&
        encoded.data == NULL && encoded.length == 0);
  bytes.length = 0;
  bytes.data = CPKT_OPCUA_EMPTY_ARRAY_SENTINEL;
  CHECK(cpkt_opcua_ByteString_toBase64(&bytes, &encoded) == 0 &&
        encoded.data == NULL && encoded.length == 0);
  string = text("not*base64");
  CHECK(cpkt_opcua_ByteString_fromBase64(&decoded, &string) != 0 &&
        decoded.data == NULL);
}
static void statuses(void) {
  const cpkt_opcua_StatusCode codes[] = {
      0,           0x00000400U, 0x40000000U,
      0x80000000U, 0xc0000000U, CPKT_OPCUA_STATUSCODE_BADTIMEOUT};
  size_t i;
  for (i = 0; i < sizeof(codes) / sizeof(codes[0]); ++i) {
    unsigned int bits = cpkt_opcua_StatusCode_isGood(codes[i]) |
                        (cpkt_opcua_StatusCode_isUncertain(codes[i]) << 1) |
                        (cpkt_opcua_StatusCode_isBad(codes[i]) << 2);
    CHECK(bits == cpkt_types_peer_status_predicates(codes[i]));
  }
  CHECK(cpkt_opcua_StatusCode_equalTop(CPKT_OPCUA_STATUSCODE_BADTIMEOUT,
                                       CPKT_OPCUA_STATUSCODE_BADTIMEOUT |
                                           0x400U));
  CHECK(!cpkt_opcua_StatusCode_equalTop(0, CPKT_OPCUA_STATUSCODE_BADTIMEOUT));
  CHECK(!strcmp(cpkt_opcua_StatusCode_name(CPKT_OPCUA_STATUSCODE_BADTIMEOUT),
                "BadTimeout"));
}
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
static void failures(void) {
  cpkt_opcua_Argument source, moved, *entries;
  const cpkt_opcua_Type *type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_ARGUMENT);
  cpkt_opcua_Variant variant;
  cpkt_opcua_ExtensionObject extension;
  cpkt_opcua_String string, encoded, suffix;
  cpkt_opcua_ByteString bytes;
  cpkt_opcua_Order order;
  void *array, *original, *move_original;
  size_t length, i;
  int mode, injected;
  cpkt_opcua_StatusCode status;
  cpkt_opcua_Argument_init(&source);
  source.name = text("nested");
  for (mode = 0; mode < 9; ++mode) {
    for (i = 0; i < 128; ++i) {
      cpkt_opcua_Variant_init(&variant);
      cpkt_opcua_ExtensionObject_init(&extension);
      array = NULL;
      length = 0;
      if (mode == 4 || mode == 5 || mode == 7 || mode == 8) {
        CHECK(cpkt_opcua_array_appendCopy(&array, &length, &source, type) == 0);
        CHECK(cpkt_opcua_array_appendCopy(&array, &length, &source, type) == 0);
      }
      cpkt_opcua_Argument_init(&moved);
      if (mode == 8)
        CHECK(cpkt_opcua_Argument_copy(&source, &moved) == 0);
      move_original = moved.name.data;
      if (mode == 1)
        variant.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
      original = array;
      cpkt_types_fail_after(i);
      switch (mode) {
      case 0:
        status = cpkt_opcua_type_order(&source, &source, type, &order);
        break;
      case 1:
        status = cpkt_opcua_Variant_setScalarCopy(&variant, &source, type);
        break;
      case 2:
        status = cpkt_opcua_Variant_setArrayCopy(&variant, &source, 1, type);
        break;
      case 3:
        status =
            cpkt_opcua_ExtensionObject_setValueCopy(&extension, &source, type);
        break;
      case 4:
        status = cpkt_opcua_array_resize(&array, &length, 1, type);
        break;
      case 5:
        status = cpkt_opcua_array_appendCopy(&array, &length, array, type);
        break;
      case 7:
        status = cpkt_opcua_array_resize(&array, &length, 4, type);
        break;
      case 8:
        status = cpkt_opcua_array_append(&array, &length, &moved, type);
        break;
      default:
        status = cpkt_opcua_array_appendCopy(&array, &length, &source, type);
        break;
      }
      injected = cpkt_types_fail_stop();
      if (injected) {
        CHECK(status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY);
        if (mode == 0)
          CHECK(order == CPKT_OPCUA_ORDER_EQ);
        if (mode == 1)
          CHECK(variant.storageType == CPKT_OPCUA_VARIANT_DATA_NODELETE);
        if (mode == 8)
          CHECK(moved.name.data == move_original && moved.name.length == 6);
        CHECK(variant.data == NULL && extension.content.decoded.data == NULL);
        if (mode == 4 || mode == 5 || mode == 7 || mode == 8) {
          CHECK(array == original && length == 2);
          entries = array;
          CHECK(entries[0].name.length == 6 && entries[1].name.length == 6);
        } else
          CHECK(array == NULL && length == 0);
      } else
        CHECK(status == 0);
      if (!injected && mode == 8)
        CHECK(moved.name.data == NULL && moved.name.length == 0);
      cpkt_opcua_Argument_clear(&moved);
      cpkt_opcua_Variant_clear(&variant);
      cpkt_opcua_ExtensionObject_clear(&extension);
      cpkt_opcua_array_delete(array, length, type);
      if (!injected)
        break;
    }
    CHECK(i > 0 && i < 128);
  }
  string = cpkt_opcua_String_fromChars("original");
  suffix = text("suffix");
  original = string.data;
  cpkt_types_fail_after(0);
  status = cpkt_opcua_String_append(&string, suffix);
  CHECK(cpkt_types_fail_stop() &&
        status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY &&
        string.data == original && string.length == 8);
  cpkt_opcua_String_clear(&string);
  cpkt_types_fail_after(0);
  string = cpkt_opcua_String_fromChars("fail");
  CHECK(cpkt_types_fail_stop() && !string.data && !string.length);
  cpkt_types_fail_after(0);
  status = cpkt_opcua_ByteString_allocBuffer(&bytes, 7);
  CHECK(cpkt_types_fail_stop() &&
        status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY && !bytes.data &&
        !bytes.length);
  bytes = text("raw");
  cpkt_opcua_String_init(&encoded);
  cpkt_types_fail_after(0);
  status = cpkt_opcua_ByteString_toBase64(&bytes, &encoded);
  CHECK(cpkt_types_fail_stop() &&
        status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY && !encoded.data);
  encoded = text("cmF3");
  cpkt_opcua_ByteString_init(&bytes);
  cpkt_types_fail_after(0);
  status = cpkt_opcua_ByteString_fromBase64(&bytes, &encoded);
  /* Upstream intentionally reports decoder allocation failure as an internal
   * error. Preserve that status rather than inventing a different decoder. */
  CHECK(cpkt_types_fail_stop() &&
        status == CPKT_OPCUA_STATUSCODE_BADINTERNALERROR && !bytes.data);
}
#endif

static cpkt_opcua_QualifiedName map_key(unsigned int ns, const char *key) {
  cpkt_opcua_QualifiedName result;
  result.namespaceIndex = (cpkt_opcua_UInt16)ns;
  result.name = text(key);
  return result;
}
static void maps(void) {
  cpkt_opcua_KeyValueMap map = {0, NULL}, other = {0, NULL}, copy = {0, NULL},
                         *allocated;
  cpkt_opcua_QualifiedName key = map_key(1, "option"),
                           second = map_key(2, "option");
  cpkt_opcua_QualifiedName binary = map_key(1, "a\0b");
  cpkt_opcua_Int64 value, changed;
  const cpkt_opcua_Type *type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
  const cpkt_opcua_Variant *borrowed;
  const cpkt_opcua_Int64 *number;
  cpkt_opcua_Variant array;
  cpkt_opcua_UInt32 dimension = 1;
  unsigned int results[24], expected[24];
  size_t i;
  value.high32 = 0x80000000U;
  value.low32 = 1;
  changed.high32 = 0x7fffffffU;
  changed.low32 = 0xffffffffU;
  results[0] = cpkt_opcua_KeyValueMap_isEmpty(NULL);
  results[1] = cpkt_opcua_KeyValueMap_contains(NULL, key);
  results[2] = cpkt_opcua_KeyValueMap_remove(NULL, key);
  results[3] = cpkt_opcua_KeyValueMap_remove(&map, key);
  results[4] = cpkt_opcua_KeyValueMap_setScalarShallow(&map, key, &value, type);
  borrowed = cpkt_opcua_KeyValueMap_get(&map, key);
  results[5] = borrowed && borrowed->data == &value;
  results[6] = borrowed ? (unsigned int)borrowed->storageType : 99;
  results[7] = cpkt_opcua_KeyValueMap_contains(&map, second);
  results[8] = cpkt_opcua_KeyValueMap_copy(&map, &copy);
  results[9] = cpkt_opcua_KeyValueMap_getScalar(&copy, key, type) != &value;
  CHECK(results[4] == 0 && results[8] == 0 && borrowed == &map.map[0].value);
  number = (const cpkt_opcua_Int64 *)cpkt_opcua_KeyValueMap_getScalar(
      &copy, key, type);
  CHECK(number && number->high32 == value.high32 && number->low32 == 1);
  value.low32 = 3;
  CHECK(((const cpkt_opcua_Int64 *)borrowed->data)->low32 == 3 &&
        number->low32 == 1);
  results[10] = cpkt_opcua_KeyValueMap_setScalar(&other, second, &value, type);
  results[11] = cpkt_opcua_KeyValueMap_merge(&map, &other);
  results[12] = (unsigned int)map.mapSize;
  results[13] = cpkt_opcua_KeyValueMap_remove(&map, key);
  results[14] = cpkt_opcua_QualifiedName_equal(&map.map[0].key, &second);
  results[15] = cpkt_opcua_KeyValueMap_remove(&map, second);
  results[16] = map.map == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL;
  cpkt_opcua_Variant_init(&array);
  cpkt_opcua_Variant_setArray(&array, &value, 1, type);
  array.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
  results[17] = cpkt_opcua_KeyValueMap_setShallow(&map, key, &array);
  results[18] = cpkt_opcua_KeyValueMap_getScalar(&map, key, type) == NULL;
  results[19] = cpkt_opcua_KeyValueMap_set(NULL, key, &array);
  results[20] = cpkt_opcua_KeyValueMap_setScalar(&map, key, NULL, type);
  results[21] = cpkt_opcua_KeyValueMap_merge(NULL, &map);
  results[22] = cpkt_opcua_KeyValueMap_merge(&map, NULL);
  cpkt_opcua_KeyValueMap_clear(&copy);
  results[23] = cpkt_opcua_KeyValueMap_copy(NULL, &copy);
  array.arrayDimensions = &dimension;
  array.arrayDimensionsSize = 1;
  CHECK(cpkt_opcua_KeyValueMap_setShallow(&map, key, &array) == 0);
  CHECK(cpkt_opcua_KeyValueMap_get(&map, key)->data == &value &&
        cpkt_opcua_KeyValueMap_get(&map, key)->arrayDimensions == &dimension);
  cpkt_types_peer_maps(expected);
  for (i = 0; i < 24; ++i)
    CHECK(results[i] == expected[i]);
  /* Deep overwrites accept the current map value as the source. */
  borrowed = cpkt_opcua_KeyValueMap_get(&map, key);
  CHECK(cpkt_opcua_KeyValueMap_set(&map, key, borrowed) == 0);
  CHECK(cpkt_opcua_KeyValueMap_get(&map, key)->data != &value);
  CHECK(cpkt_opcua_KeyValueMap_setScalar(&map, key, &changed, type) == 0);
  number = (const cpkt_opcua_Int64 *)cpkt_opcua_KeyValueMap_getScalar(&map, key,
                                                                      type);
  CHECK(number && number->high32 == changed.high32 &&
        number->low32 == changed.low32);
  CHECK(!cpkt_opcua_KeyValueMap_getScalar(
      &map, key, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_UINT64)));
  CHECK(cpkt_opcua_KeyValueMap_merge(&map, &map) == 0);
  CHECK(cpkt_opcua_KeyValueMap_setScalarShallow(&map, key, &value, type) == 0);
  CHECK(cpkt_opcua_KeyValueMap_getScalar(&map, key, type) == &value);
  /* Namespace and embedded-NUL bytes are part of key identity. */
  binary.name.length = 3;
  CHECK(cpkt_opcua_KeyValueMap_setScalar(&map, binary, &changed, type) == 0);
  CHECK(!cpkt_opcua_KeyValueMap_contains(&map, map_key(1, "a")));
  CHECK(cpkt_opcua_KeyValueMap_contains(&map, binary));
  cpkt_opcua_KeyValueMap_clear(&map);
  cpkt_opcua_KeyValueMap_clear(&other);
  cpkt_opcua_KeyValueMap_clear(&copy);
  CHECK(value.low32 == 3 && array.data == &value);
  allocated = cpkt_opcua_KeyValueMap_new();
  CHECK(allocated);
  CHECK(cpkt_opcua_KeyValueMap_isEmpty(allocated) &&
        !cpkt_opcua_KEYVALUEMAP_NULL.mapSize);
  cpkt_opcua_KeyValueMap_delete(allocated);
  cpkt_opcua_KeyValueMap_delete(NULL);
  cpkt_opcua_KeyValueMap_clear(NULL);
  CHECK(cpkt_opcua_KeyValueMap_copy(NULL, NULL) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_KeyValueMap_setScalar(&map, key, &value, NULL) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_KeyValueMap_setShallow(&map, key, NULL) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_KeyValueMap_set(&map, key, NULL) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
}
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
static void map_failures(void) {
  cpkt_opcua_KeyValueMap map, other, copy;
  cpkt_opcua_KeyValuePair *original;
  cpkt_opcua_QualifiedName key = map_key(1, "existing"),
                           added = map_key(1, "new");
  const cpkt_opcua_Type *type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
  cpkt_opcua_Int64 value, changed;
  cpkt_opcua_StatusCode status;
  const cpkt_opcua_Int64 *result;
  cpkt_opcua_Variant variant;
  size_t countdown;
  int mode, failed;
  value.high32 = 0x80000000U;
  value.low32 = 1;
  changed.high32 = 0x7fffffffU;
  changed.low32 = 0xffffffffU;
  for (mode = 0; mode < 6; ++mode) {
    for (countdown = 0; countdown < 200; ++countdown) {
      memset(&map, 0, sizeof(map));
      memset(&other, 0, sizeof(other));
      memset(&copy, 0, sizeof(copy));
      CHECK(cpkt_opcua_KeyValueMap_setScalar(&map, key, &value, type) == 0);
      CHECK(cpkt_opcua_KeyValueMap_setScalar(&other, key, &changed, type) == 0);
      CHECK(cpkt_opcua_KeyValueMap_setScalar(&other, added, &changed, type) ==
            0);
      original = map.map;
      cpkt_opcua_Variant_init(&variant);
      variant.type = type;
      variant.data = &changed;
      variant.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
      cpkt_types_fail_after(countdown);
      switch (mode) {
      case 0:
        status = cpkt_opcua_KeyValueMap_setScalar(&map, added, &changed, type);
        break;
      case 1:
        status = cpkt_opcua_KeyValueMap_setScalar(&map, key, &changed, type);
        break;
      case 2:
        status = cpkt_opcua_KeyValueMap_setShallow(&map, added, &variant);
        break;
      case 3:
        status = cpkt_opcua_KeyValueMap_copy(&map, &copy);
        break;
      case 4:
        status = cpkt_opcua_KeyValueMap_merge(&map, &other);
        break;
      default:
        status = cpkt_opcua_KeyValueMap_merge(&map, &map);
        break;
      }
      failed = cpkt_types_fail_stop();
      if (failed) {
        CHECK(status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY);
        CHECK(map.map == original && map.mapSize == 1);
        result = (const cpkt_opcua_Int64 *)cpkt_opcua_KeyValueMap_getScalar(
            &map, key, type);
        CHECK(result && result->high32 == value.high32 && result->low32 == 1);
        CHECK(other.mapSize == 2 && copy.map == NULL && !copy.mapSize);
      } else
        CHECK(status == 0);
      cpkt_opcua_KeyValueMap_clear(&map);
      cpkt_opcua_KeyValueMap_clear(&other);
      cpkt_opcua_KeyValueMap_clear(&copy);
      if (!failed)
        break;
    }
    CHECK(countdown > 0 && countdown < 200);
  }
  memset(&map, 0, sizeof(map));
  CHECK(cpkt_opcua_KeyValueMap_setScalarShallow(&map, key, &value, type) == 0);
  CHECK(cpkt_opcua_KeyValueMap_setScalarShallow(&map, added, &changed, type) ==
        0);
  cpkt_types_fail_after(0);
  status = cpkt_opcua_KeyValueMap_remove(&map, key);
  CHECK(cpkt_types_fail_stop() && status == 0 && map.mapSize == 1);
  CHECK(cpkt_opcua_KeyValueMap_getScalar(&map, added, type) == &changed);
  cpkt_opcua_KeyValueMap_clear(&map);
  cpkt_types_fail_after(0);
  CHECK(cpkt_opcua_KeyValueMap_new() == NULL);
  CHECK(cpkt_types_fail_stop());
}
#endif
void cpkt_types_test_utilities(void) {
  predicates();
  values();
  arrays();
  strings();
  statuses();
  maps();
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  failures();
  map_failures();
#endif
}
