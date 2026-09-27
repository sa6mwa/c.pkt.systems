#include "opcua_callbacks_test.h"
#include "opcua_types_peer.h"
#include <cpkt/opcua_util.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void check(int condition, const char *expression, int line) {
  if (!condition) {
    fprintf(stderr, "identifier line %d: %s\n", line, expression);
    abort();
  }
}
#define CHECK(e) check(!!(e), #e, __LINE__)
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
void cpkt_types_fail_after(size_t count);
int cpkt_types_fail_stop(void);
#endif
static cpkt_opcua_String text(const char *value) {
  cpkt_opcua_String s;
  s.length = strlen(value);
  s.data = (cpkt_opcua_Byte *)value;
  return s;
}
static void fixture(cpkt_opcua_NamespaceMapping *map, cpkt_opcua_String *uris,
                    cpkt_opcua_String *servers) {
  cpkt_opcua_NamespaceMapping_init(map);
  uris[0] = text("urn:local");
  uris[1] = text("urn:widgets");
  servers[0] = text("urn:server:local");
  servers[1] = text("urn:server:remote");
  map->namespaceUris = uris;
  map->namespaceUrisSize = 2;
}
static void identifier_case(int kind, int extended, const char *input,
                            size_t capacity) {
  struct cpkt_identifier_peer peer;
  cpkt_opcua_NodeId node, null_node;
  cpkt_opcua_ExpandedNodeId expanded, null_expanded;
  cpkt_opcua_QualifiedName name;
  cpkt_opcua_Guid guid;
  cpkt_opcua_NamespaceMapping mapping;
  cpkt_opcua_String uris[2], servers[2], output;
  cpkt_opcua_ByteString encoded;
  cpkt_opcua_StatusCode parse, print;
  unsigned char buffer[256];
  const cpkt_opcua_Type *type;
  void *value;
  fixture(&mapping, uris, servers);
  cpkt_opcua_NodeId_init(&node);
  cpkt_opcua_NodeId_init(&null_node);
  cpkt_opcua_ExpandedNodeId_init(&expanded);
  cpkt_opcua_ExpandedNodeId_init(&null_expanded);
  cpkt_opcua_QualifiedName_init(&name);
  cpkt_opcua_Guid_init(&guid);
  cpkt_opcua_ByteString_init(&encoded);
  memset(buffer, 0xa5, sizeof(buffer));
  output.length = capacity;
  output.data = capacity ? buffer : NULL;
  cpkt_types_peer_identifier(kind, extended, input, capacity, &peer);
  switch (kind) {
  case 0:
    parse = extended ? cpkt_opcua_NodeId_parseEx(&node, text(input), &mapping)
                     : cpkt_opcua_NodeId_parse(&node, text(input));
    print = extended ? cpkt_opcua_NodeId_printEx(&node, &output, &mapping)
                     : cpkt_opcua_NodeId_print(&node, &output);
    CHECK(cpkt_opcua_NodeId_hash(&node) == peer.hash);
    CHECK(cpkt_opcua_NodeId_isNull(&node) == peer.predicate);
    CHECK(cpkt_opcua_NodeId_order(&node, &null_node) == peer.order);
    value = &node;
    type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_NODEID);
    break;
  case 1:
    parse = extended ? cpkt_opcua_ExpandedNodeId_parseEx(&expanded, text(input),
                                                         &mapping, 2, servers)
                     : cpkt_opcua_ExpandedNodeId_parse(&expanded, text(input));
    print = extended ? cpkt_opcua_ExpandedNodeId_printEx(&expanded, &output,
                                                         &mapping, 2, servers)
                     : cpkt_opcua_ExpandedNodeId_print(&expanded, &output);
    CHECK(cpkt_opcua_ExpandedNodeId_hash(&expanded) == peer.hash);
    CHECK(cpkt_opcua_ExpandedNodeId_isLocal(&expanded) == peer.predicate);
    CHECK(cpkt_opcua_ExpandedNodeId_order(&expanded, &null_expanded) ==
          peer.order);
    value = &expanded;
    type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_EXPANDEDNODEID);
    break;
  case 2:
    parse = extended
                ? cpkt_opcua_QualifiedName_parseEx(&name, text(input), &mapping)
                : cpkt_opcua_QualifiedName_parse(&name, text(input));
    print = extended
                ? cpkt_opcua_QualifiedName_printEx(&name, &output, &mapping)
                : cpkt_opcua_QualifiedName_print(&name, &output);
    CHECK(cpkt_opcua_QualifiedName_hash(&name) == peer.hash);
    CHECK(cpkt_opcua_QualifiedName_isNull(&name) == peer.predicate);
    value = &name;
    type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_QUALIFIEDNAME);
    break;
  default:
    parse = cpkt_opcua_Guid_parse(&guid, text(input));
    print = cpkt_opcua_Guid_print(&guid, &output);
    value = &guid;
    type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_GUID);
    break;
  }
  CHECK(parse == peer.parse_status && print == peer.print_status);
  CHECK(output.length == peer.output_length);
  if (capacity)
    CHECK(output.data == buffer &&
          !memcmp(buffer, peer.output, sizeof(buffer)));
  else {
    CHECK(!output.length || !memcmp(output.data, peer.output, output.length));
    cpkt_opcua_String_clear(&output);
  }
  if (!parse) {
    CHECK(cpkt_opcua_type_encode_binary(value, type, &encoded) == 0);
    CHECK(encoded.length == peer.encoded_length &&
          !memcmp(encoded.data, peer.encoded, encoded.length));
  }
  cpkt_opcua_ByteString_clear(&encoded);
  cpkt_types_native_free(peer.encoded);
  cpkt_opcua_type_clear(value, type);
}
static void identifiers(void) {
  static const char *cases[][10] = {
      {"i=0", "ns=1;i=4294967295", "ns=2;s=spaces ; &%", "ns=1;b=AAEC/w==",
       "ns=3;g=12345678-1234-5678-9abc-def012345678", "nsu=urn:widgets;i=42",
       "nsu=urn:unknown;i=5", "ns=65535;i=12", "invalid", NULL},
      {"i=0", "svr=1;ns=1;i=42", "nsu=urn:widgets;s=hello",
       "svu=urn:server:remote;nsu=urn:widgets;i=7",
       "svr=2;ns=3;b=AP8=", "nsu=urn:unknown;i=5", "invalid", NULL},
      {"0:hello", "1:with:colon", "nsu=urn:widgets;hello",
       "nsu=urn:unknown;hello", "65535:x", "", "invalid", NULL},
      {"12345678-1234-5678-9abc-def012345678",
       "00000000-0000-0000-0000-000000000000", "invalid", NULL}};
  static const size_t capacities[] = {0, 1, 12, 36, 128};
  int kind, extended;
  size_t i, j;
  for (kind = 0; kind < 4; ++kind)
    for (extended = 0; extended < 2; ++extended)
      for (i = 0; cases[kind][i]; ++i)
        for (j = 0; j < sizeof(capacities) / sizeof(capacities[0]); ++j)
          identifier_case(kind, extended, cases[kind][i], capacities[j]);
}
static void namespaces(void) {
  cpkt_opcua_NamespaceMapping mapping, *owned;
  cpkt_opcua_String uris[2], servers[2], result;
  cpkt_opcua_UInt16 local[2], remote[3], index;
  const cpkt_opcua_Type *string_type =
      cpkt_opcua_type_at(CPKT_OPCUA_TYPES_STRING);
  const cpkt_opcua_Type *index_type =
      cpkt_opcua_type_at(CPKT_OPCUA_TYPES_UINT16);
  fixture(&mapping, uris, servers);
  local[0] = 2;
  local[1] = 1;
  remote[0] = 7;
  remote[1] = 1;
  remote[2] = 0;
  mapping.local2remote = local;
  mapping.local2remoteSize = 2;
  mapping.remote2local = remote;
  mapping.remote2localSize = 3;
  CHECK(cpkt_opcua_NamespaceMapping_local2Remote(&mapping, 0) == 2);
  CHECK(cpkt_opcua_NamespaceMapping_remote2Local(&mapping, 2) == 0);
  CHECK(cpkt_opcua_NamespaceMapping_local2Remote(&mapping, 9) == 65526);
  CHECK(cpkt_opcua_NamespaceMapping_remote2Local(NULL, 9) == 65526);
  index = 123;
  CHECK(!cpkt_opcua_NamespaceMapping_uri2Index(&mapping, uris[1], &index) &&
        index == 1);
  CHECK(
      cpkt_opcua_NamespaceMapping_uri2Index(&mapping, text("absent"), &index) ==
          CPKT_OPCUA_STATUSCODE_BADNOTFOUND &&
      index == 1);
  result = text("untouched");
  CHECK(cpkt_opcua_NamespaceMapping_index2Uri(&mapping, 2, &result) ==
            CPKT_OPCUA_STATUSCODE_BADNOTFOUND &&
        result.length == 9);
  CHECK(!cpkt_opcua_NamespaceMapping_index2Uri(&mapping, 1, &result) &&
        result.data == uris[1].data);
  owned = cpkt_opcua_NamespaceMapping_new();
  CHECK(owned != NULL);
  CHECK(!cpkt_opcua_array_copy(uris, 2, (void **)&owned->namespaceUris,
                               string_type));
  owned->namespaceUrisSize = 2;
  CHECK(!cpkt_opcua_array_copy(local, 2, (void **)&owned->local2remote,
                               index_type));
  owned->local2remoteSize = 2;
  CHECK(!cpkt_opcua_array_copy(remote, 3, (void **)&owned->remote2local,
                               index_type));
  owned->remote2localSize = 3;
  CHECK(owned->namespaceUris[1].data != uris[1].data);
  cpkt_opcua_NamespaceMapping_clear(owned);
  CHECK(!owned->namespaceUris && !owned->namespaceUrisSize &&
        !owned->local2remote && !owned->local2remoteSize &&
        !owned->remote2local && !owned->remote2localSize);
  cpkt_opcua_NamespaceMapping_delete(owned);
  cpkt_opcua_NamespaceMapping_delete(NULL);
  mapping.namespaceUris = CPKT_OPCUA_EMPTY_ARRAY_SENTINEL;
  CHECK(cpkt_opcua_NamespaceMapping_uri2Index(&mapping, text("bad"), &index) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
}
static int time_equal(cpkt_opcua_Int64 a, cpkt_opcua_Int64 b) {
  return a.high32 == b.high32 && a.low32 == b.low32;
}
static void times(void) {
  static const char *dates[] = {
      "1970-01-01T00:00:00Z",         "1969-12-31T23:59:59Z",
      "2026-09-27T14:35:16.1234567Z", "2107-01-01T00:00:00Z",
      "1601-01-01T00:00:00Z",         "1970-01-01T00:00:01Z"};
  cpkt_opcua_DateTime time, next, parsed;
  cpkt_opcua_Int64 unix_time, zero;
  cpkt_opcua_DateTimeStruct calendar;
  unsigned int high, low;
  unsigned short fields[8];
  short year;
  size_t i;
  zero.high32 = zero.low32 = 0;
  for (i = 0; i < sizeof(dates) / sizeof(dates[0]); ++i) {
    CHECK(!cpkt_opcua_DateTime_parse(&time, text(dates[i])));
    CHECK(time_equal(time, cpkt_opcua_DATETIME(dates[i])));
    cpkt_types_peer_time(time.high32, time.low32, &high, &low, fields, &year);
    unix_time = cpkt_opcua_DateTime_toUnixTime(time);
    CHECK(unix_time.high32 == high && unix_time.low32 == low);
    calendar = cpkt_opcua_DateTime_toStruct(time);
    CHECK(calendar.nanoSec == fields[0] && calendar.microSec == fields[1] &&
          calendar.milliSec == fields[2] && calendar.sec == fields[3] &&
          calendar.min == fields[4] && calendar.hour == fields[5] &&
          calendar.day == fields[6] && calendar.month == fields[7] &&
          calendar.year == year);
    CHECK(time_equal(cpkt_opcua_DateTime_fromStruct(calendar), time));
    if (i == 0)
      CHECK(time_equal(unix_time, zero) &&
            time_equal(cpkt_opcua_DateTime_fromUnixTime(zero), time));
    if (i == 1)
      CHECK(unix_time.high32 == 0xffffffffU && unix_time.low32 == 0xffffffffU &&
            time_equal(cpkt_opcua_DateTime_fromUnixTime(unix_time), time));
    if (i == 5)
      CHECK(unix_time.high32 == 0 && unix_time.low32 == 1 &&
            time_equal(cpkt_opcua_DateTime_fromUnixTime(unix_time), time));
    if (i == 3)
      CHECK(unix_time.high32 == 1 &&
            time_equal(cpkt_opcua_DateTime_fromUnixTime(unix_time), time));
  }
  CHECK(cpkt_opcua_DateTime_parse(&parsed, text("invalid")) != 0);
  CHECK(time_equal(cpkt_opcua_DATETIME("invalid"), zero) &&
        time_equal(cpkt_opcua_DATETIME(NULL), zero));
  time = cpkt_opcua_DateTime_now();
  CHECK(time.high32 != 0);
  time = cpkt_opcua_DateTime_nowMonotonic();
  next = cpkt_opcua_DateTime_nowMonotonic();
  CHECK(next.high32 > time.high32 ||
        (next.high32 == time.high32 && next.low32 >= time.low32));
  unix_time = cpkt_opcua_DateTime_localTimeUtcOffset();
  cpkt_types_peer_offset(&high, &low);
  CHECK(unix_time.high32 == high && unix_time.low32 == low);
}
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
static void failures(void) {
  cpkt_opcua_NamespaceMapping mapping;
  cpkt_opcua_String uris[2], servers[2], out;
  cpkt_opcua_NodeId node;
  cpkt_opcua_ExpandedNodeId expanded;
  cpkt_opcua_QualifiedName name;
  cpkt_opcua_StatusCode status;
  size_t mode, i;
  int injected;
  unsigned char buffer[128];
  fixture(&mapping, uris, servers);
  for (mode = 0; mode < 8; ++mode) {
    for (i = 0; i < 32; ++i) {
      cpkt_opcua_NodeId_init(&node);
      cpkt_opcua_ExpandedNodeId_init(&expanded);
      cpkt_opcua_QualifiedName_init(&name);
      out.length = sizeof(buffer);
      out.data = buffer;
      memset(buffer, 0xa5, sizeof(buffer));
      if (mode >= 3 && mode <= 5) {
        CHECK(!cpkt_opcua_NodeId_parse(&node, text("ns=1;s=payload")));
        CHECK(!cpkt_opcua_ExpandedNodeId_parse(&expanded,
                                               text("svr=1;ns=1;s=payload")));
        CHECK(!cpkt_opcua_QualifiedName_parse(&name, text("1:payload")));
      }
      cpkt_types_fail_after(i);
      switch (mode) {
      case 0:
        status = cpkt_opcua_NodeId_parseEx(
            &node, text("nsu=urn:widgets;s=payload"), &mapping);
        break;
      case 1:
        status = cpkt_opcua_ExpandedNodeId_parseEx(
            &expanded, text("svu=urn:server:remote;nsu=urn:widgets;s=payload"),
            &mapping, 2, servers);
        break;
      case 2:
        status = cpkt_opcua_QualifiedName_parseEx(
            &name, text("nsu=urn:widgets;payload"), &mapping);
        break;
      case 3:
        status = cpkt_opcua_NodeId_printEx(&node, &out, &mapping);
        break;
      case 4:
        status = cpkt_opcua_ExpandedNodeId_printEx(&expanded, &out, &mapping, 2,
                                                   servers);
        break;
      case 5:
        status = cpkt_opcua_QualifiedName_printEx(&name, &out, &mapping);
        break;
      case 6: {
        cpkt_opcua_UInt16 index = 99;
        status =
            cpkt_opcua_NamespaceMapping_uri2Index(&mapping, uris[1], &index);
        if (status)
          CHECK(index == 99);
        break;
      }
      default:
        status = cpkt_opcua_NamespaceMapping_index2Uri(&mapping, 1, &out);
        break;
      }
      injected = cpkt_types_fail_stop();
      if (injected)
        CHECK(status != 0);
      else
        CHECK(status == 0);
      if (injected && mode >= 3 && mode <= 5)
        CHECK(out.data == buffer && out.length == sizeof(buffer) &&
              buffer[0] == 0xa5);
      CHECK(mapping.namespaceUris == uris && uris[1].length == 11 &&
            !memcmp(uris[1].data, "urn:widgets", 11));
      cpkt_opcua_NodeId_clear(&node);
      cpkt_opcua_ExpandedNodeId_clear(&expanded);
      cpkt_opcua_QualifiedName_clear(&name);
      if (!injected)
        break;
    }
    CHECK(i > 0 && i < 32);
  }
  cpkt_types_fail_after(0);
  CHECK(cpkt_opcua_NamespaceMapping_new() == NULL && cpkt_types_fail_stop());
}
#endif
static void randoms(void) {
  cpkt_opcua_UInt64 seed;
  cpkt_opcua_Guid guid;
  unsigned char expected[16];
  unsigned int number;
  size_t i;
  seed.high32 = 0xfedcba98U;
  seed.low32 = 0x76543210U;
  for (i = 0; i < 3; ++i) {
    cpkt_types_peer_random(seed.high32, seed.low32, expected, &number);
    cpkt_opcua_random_seed_deterministic(seed);
    guid = cpkt_opcua_Guid_random();
    CHECK(!memcmp(&guid, expected, sizeof(guid)));
    CHECK(cpkt_opcua_UInt32_random() == number);
    ++seed.low32;
  }
  cpkt_opcua_random_seed(seed);
  (void)cpkt_opcua_UInt32_random();
}

static void factory(int kind, char *first, char *second, int failure_position) {
  union {
    cpkt_opcua_String string;
    cpkt_opcua_Guid guid;
    cpkt_opcua_NodeId node;
    cpkt_opcua_ExpandedNodeId expanded;
    cpkt_opcua_QualifiedName name;
    cpkt_opcua_LocalizedText localized;
  } value;
  cpkt_opcua_Guid guid = {
      0xdeadbeefU, 0xabcd, 0xef12, {0, 127, 128, 255, 1, 2, 3, 4}};
  struct cpkt_value_factory_observed native;
  cpkt_opcua_ByteString encoded;
  const void *borrowed = NULL, *borrowed_second = NULL;
  const cpkt_opcua_Type *type;
  unsigned int aliases;
  int owned = 0, injected = 0;
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  if (failure_position >= 0)
    cpkt_types_fail_after((size_t)failure_position);
#endif
  cpkt_types_peer_value_factory(kind, first, second, failure_position >= 0,
                                &native);
  CHECK(!native.status);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  if (failure_position >= 0)
    cpkt_types_fail_after((size_t)failure_position);
#endif
  memset(&value, 0, sizeof(value));
  switch (kind) {
  case 0:
    value.string = cpkt_opcua_STRING(first);
    borrowed = value.string.data;
    break;
  case 1:
    value.guid = cpkt_opcua_GUID(first);
    break;
  case 2:
    value.node = cpkt_opcua_NODEID(first);
    owned = 1;
    break;
  case 3:
    value.node = cpkt_opcua_NODEID_NUMERIC(65535, 0xfedcba98U);
    break;
  case 4:
    value.node = cpkt_opcua_NODEID_STRING(65535, first);
    borrowed = value.node.identifier.string.data;
    break;
  case 5:
    value.node = cpkt_opcua_NODEID_STRING_ALLOC(65535, first);
    owned = 1;
    break;
  case 6:
    value.node = cpkt_opcua_NODEID_GUID(65535, guid);
    break;
  case 7:
    value.node = cpkt_opcua_NODEID_BYTESTRING(65535, first);
    borrowed = value.node.identifier.byteString.data;
    break;
  case 8:
    value.node = cpkt_opcua_NODEID_BYTESTRING_ALLOC(65535, first);
    owned = 1;
    break;
  case 9:
    value.expanded = cpkt_opcua_EXPANDEDNODEID(first);
    owned = 1;
    break;
  case 10:
    value.expanded = cpkt_opcua_EXPANDEDNODEID_NUMERIC(65535, 0xfedcba98U);
    break;
  case 11:
    value.expanded = cpkt_opcua_EXPANDEDNODEID_STRING(65535, first);
    borrowed = value.expanded.nodeId.identifier.string.data;
    break;
  case 12:
    value.expanded = cpkt_opcua_EXPANDEDNODEID_STRING_ALLOC(65535, first);
    owned = 1;
    break;
  case 13:
    value.expanded = cpkt_opcua_EXPANDEDNODEID_STRING_GUID(65535, guid);
    break;
  case 14:
    value.expanded = cpkt_opcua_EXPANDEDNODEID_BYTESTRING(65535, first);
    borrowed = value.expanded.nodeId.identifier.byteString.data;
    break;
  case 15:
    value.expanded = cpkt_opcua_EXPANDEDNODEID_BYTESTRING_ALLOC(65535, first);
    owned = 1;
    break;
  case 16:
    value.expanded = cpkt_opcua_EXPANDEDNODEID_NODEID(
        cpkt_opcua_NODEID_STRING(65535, first));
    borrowed = value.expanded.nodeId.identifier.string.data;
    break;
  case 17:
    value.name = cpkt_opcua_QUALIFIEDNAME(65535, first);
    borrowed = value.name.name.data;
    break;
  case 18:
    value.name = cpkt_opcua_QUALIFIEDNAME_ALLOC(65535, first);
    owned = 1;
    break;
  case 19:
    value.localized = cpkt_opcua_LOCALIZEDTEXT(first, second);
    borrowed = value.localized.locale.data;
    borrowed_second = value.localized.text.data;
    break;
  case 20:
    value.localized = cpkt_opcua_LOCALIZEDTEXT_ALLOC(first, second);
    owned = 1;
    break;
  case 21:
    value.string = cpkt_opcua_STRING_NULL;
    break;
  case 22:
    value.string = cpkt_opcua_BYTESTRING_NULL;
    break;
  case 23:
    value.guid = cpkt_opcua_GUID_NULL;
    break;
  case 24:
    value.node = cpkt_opcua_NODEID_NULL;
    break;
  case 25:
    value.expanded = cpkt_opcua_EXPANDEDNODEID_NULL;
    break;
  default:
    abort();
  }
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  if (failure_position >= 0)
    injected = cpkt_types_fail_stop();
#endif
  CHECK(injected == native.injected);
  aliases = (borrowed && borrowed == first ? 1U : 0U) |
            (borrowed_second && borrowed_second == second ? 2U : 0U);
  CHECK(aliases == native.aliases);
  type = cpkt_opcua_type_at(native.type);
  cpkt_opcua_ByteString_init(&encoded);
  CHECK(!cpkt_opcua_type_encode_binary(&value, type, &encoded));
  CHECK(encoded.length == native.length &&
        !memcmp(encoded.data, native.bytes, encoded.length));
  cpkt_opcua_ByteString_clear(&encoded);
  cpkt_types_native_free(native.bytes);
  if (owned)
    cpkt_opcua_type_clear(&value, type);
}
static void factories(void) {
  int kind;
  cpkt_opcua_String literal = cpkt_opcua_STRING_STATIC("literal");
  cpkt_opcua_ByteString bytes;
  cpkt_opcua_NodeId node;
  cpkt_opcua_ExpandedNodeId expanded;
  char ordinary[] = "native constructor";
  char second[] = "second constructor";
  char guid[] = "deadbeef-abcd-ef12-007f-80ff01020304";
  char parsed[] = "ns=65535;s=native constructor";
  char parsed_expanded[] = "svr=4294967295;nsu=urn:constructor;s=native";
  for (kind = 0; kind < 26; ++kind) {
    char *first = kind == 1   ? guid
                  : kind == 2 ? parsed
                  : kind == 9 ? parsed_expanded
                              : ordinary;
    factory(kind, first, second, -1);
    if (kind == 1 || kind == 2 || kind == 9)
      factory(kind, ordinary, second, -1);
    if (kind != 1 && kind != 2 && kind != 9)
      factory(kind, NULL, NULL, -1);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
    if (kind == 2 || kind == 5 || kind == 8 || kind == 9 || kind == 12 ||
        kind == 15 || kind == 18 || kind == 20) {
      int position;
      for (position = 0; position < 12; ++position)
        factory(kind, first, second, position);
    }
#endif
  }
  CHECK(literal.length == 7 && !memcmp(literal.data, "literal", 7));
  bytes = cpkt_opcua_BYTESTRING(ordinary);
  CHECK(bytes.data == (unsigned char *)ordinary);
  bytes = cpkt_opcua_STRING_ALLOC(ordinary);
  CHECK(bytes.length == strlen(ordinary) &&
        bytes.data != (unsigned char *)ordinary);
  cpkt_opcua_ByteString_clear(&bytes);
  bytes = cpkt_opcua_BYTESTRING_ALLOC(ordinary);
  CHECK(bytes.length == strlen(ordinary));
  cpkt_opcua_ByteString_clear(&bytes);
  node = cpkt_opcua_NS0ID(BASEOBJECTTYPE);
  CHECK(!node.namespaceIndex &&
        node.identifier.numeric == CPKT_OPCUA_NS0ID_BASEOBJECTTYPE);
  expanded = cpkt_opcua_NS0EXID(BASEOBJECTTYPE);
  CHECK(!expanded.nodeId.namespaceIndex &&
        expanded.nodeId.identifier.numeric == node.identifier.numeric);
  node = cpkt_opcua_NODEID_STRING(1, ordinary);
  expanded = cpkt_opcua_NODEID2EXPANDEDNODEID(node);
  CHECK(expanded.nodeId.identifier.string.data == node.identifier.string.data);
}
static void range_case(const char *input, int shorthand, int failure_position) {
  struct cpkt_range_observed native;
  cpkt_opcua_NumericRange range;
  cpkt_opcua_StatusCode status = 0;
  cpkt_opcua_String source;
  size_t i;
  int injected = 0;
  source.length = input ? strlen(input) : 0;
  source.data = (cpkt_opcua_Byte *)input;
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  if (failure_position >= 0)
    cpkt_types_fail_after((size_t)failure_position);
#endif
  cpkt_types_peer_range(input, source.length, shorthand, failure_position >= 0,
                        &native);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  if (failure_position >= 0)
    cpkt_types_fail_after((size_t)failure_position);
#endif
  memset(&range, 0, sizeof(range));
  if (shorthand)
    range = cpkt_opcua_NUMERICRANGE(input);
  else
    status = cpkt_opcua_NumericRange_parse(&range, source);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  if (failure_position >= 0)
    injected = cpkt_types_fail_stop();
#endif
  CHECK(status == native.status && injected == native.injected &&
        range.dimensionsSize == native.count);
  for (i = 0; i < native.count; ++i)
    CHECK(range.dimensions[i].min == native.bounds[2 * i] &&
          range.dimensions[i].max == native.bounds[2 * i + 1]);
  cpkt_opcua_NumericRange_clear(&range);
  CHECK(!range.dimensions && !range.dimensionsSize);
}
static void ranges(void) {
  static const char *inputs[] = {"0",
                                 "0:4294967295",
                                 "0:2,4,4294967295",
                                 "1:2,3:4,5:6,7:8,9:10,11:12",
                                 "4294967296",
                                 "",
                                 "1:1",
                                 "2:1",
                                 "1,",
                                 "x",
                                 "-1"};
  size_t i;
  int shorthand;
  cpkt_opcua_NumericRange range;
  cpkt_opcua_NumericRangeDimension seed = {7, 11};
  for (i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i)
    for (shorthand = 0; shorthand < 2; ++shorthand)
      range_case(inputs[i], shorthand, -1);
  range_case(NULL, 0, -1);
  range_case(NULL, 1, -1);
  range.dimensions = &seed;
  range.dimensionsSize = 1;
  CHECK(cpkt_opcua_NumericRange_parse(&range, text("1:1")) ==
            CPKT_OPCUA_STATUSCODE_BADINDEXRANGEINVALID &&
        range.dimensions == &seed && range.dimensionsSize == 1);
  CHECK(cpkt_opcua_NumericRange_parse(NULL, text("1")) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  cpkt_opcua_NumericRange_clear(NULL);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  for (i = 0; i < 8; ++i)
    for (shorthand = 0; shorthand < 2; ++shorthand)
      range_case(inputs[3], shorthand, (int)i);
#endif
}
static void endpoint_case(const char *input, size_t length, int ethernet,
                          int with_path) {
  struct cpkt_endpoint_observed native;
  cpkt_opcua_String url, host, path;
  cpkt_opcua_UInt16 port = 4242;
  cpkt_opcua_Byte pcp = 6;
  cpkt_opcua_StatusCode status;
  char seed[] = "seed";
  url.length = length;
  url.data = (cpkt_opcua_Byte *)input;
  host = text(seed);
  path = text(seed);
  cpkt_types_peer_endpoint(input, length, seed, ethernet, with_path, &native);
  status = ethernet
               ? cpkt_opcua_parseEndpointUrlEthernet(&url, &host, &port, &pcp)
               : cpkt_opcua_parseEndpointUrl(&url, &host, &port,
                                             with_path ? &path : NULL);
  CHECK(status == native.status && host.data == native.host &&
        host.length == native.host_length && port == native.port &&
        pcp == native.pcp);
  if (with_path && !ethernet)
    CHECK(path.data == native.path && path.length == native.path_length);
}
static void endpoints(void) {
  static const char *inputs[] = {"opc.tcp://host",
                                 "opc.tcp://host:65535/a/b/",
                                 "opc.tcp://[::1]:4840/path",
                                 "opc.tcp://[::1]",
                                 "opc.tcp://host/",
                                 "opc.tcp://host//",
                                 "opc.tcp://host:65536/path",
                                 "opc.tcp://host:0",
                                 "opc.tcp://host:",
                                 "opc.tcp://host:12x/path",
                                 "opc.tcp://[::1",
                                 "https://host:443/path",
                                 "opc.eth://01-23-45-67-89-ab:4096.7",
                                 "opc.eth://01-23-45-67-89-ab:4097.7",
                                 "opc.eth://01-23-45-67-89-ab:12.8",
                                 "opc.eth://01-23-45-67-89-ab:12.0",
                                 "opc.eth://01-23-45-67-89-ab",
                                 "bad",
                                 ""};
  static const char bounded[] = {'o', 'p', 'c', '.', 't', 'c', 'p', ':',
                                 '/', '/', 'h', ':', '1', '/', 'x'};
  size_t i;
  int mode, path;
  cpkt_opcua_String url = text("opc.tcp://host"), host;
  unsigned short port = 0;
  for (i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i)
    for (mode = 0; mode < 2; ++mode)
      for (path = 0; path < 2; ++path)
        endpoint_case(inputs[i], strlen(inputs[i]), mode, path);
  endpoint_case(bounded, sizeof(bounded), 0, 1);
  host = text("seed");
  CHECK(cpkt_opcua_parseEndpointUrl(NULL, &host, &port, NULL) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_parseEndpointUrl(&url, NULL, &port, NULL) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_parseEndpointUrl(&url, &host, NULL, NULL) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
}
static void readers(void) {
  static const unsigned char bytes[] = "0123456789abcdefXYZ!";
  static const unsigned char overflow[] = "429496729612345678901234567890";
  unsigned int a, b;
  size_t length, first, second, i;
  unsigned int base;
  unsigned char different[sizeof(bytes)];
  for (base = 0; base < 256; ++base)
    for (length = 0; length < sizeof(bytes); ++length) {
      a = b = 0xdeadbeefU;
      first = cpkt_types_peer_number(bytes, length, &a, (unsigned char)base, 0);
      second =
          cpkt_opcua_readNumberWithBase(bytes, length, &b, (unsigned char)base);
      CHECK(first == second && a == b);
    }
  for (length = 0; length < sizeof(overflow); ++length) {
    a = b = 0xdeadbeefU;
    first = cpkt_types_peer_number(overflow, length, &a, 10, 1);
    second = cpkt_opcua_readNumber(overflow, length, &b);
    CHECK(first == second && a == b);
  }
  a = 7;
  CHECK(!cpkt_opcua_readNumber(NULL, 0, &a) && a == 7);
  CHECK(!cpkt_opcua_readNumber(bytes, sizeof(bytes), NULL));
  CHECK(cpkt_opcua_constantTimeEqual(NULL, NULL, 0));
  CHECK(!cpkt_opcua_constantTimeEqual(NULL, bytes, 1));
  for (length = 0; length <= sizeof(bytes); ++length) {
    CHECK(cpkt_opcua_constantTimeEqual(bytes, bytes, length) ==
          cpkt_types_peer_constant_equal(bytes, bytes, length));
    for (i = 0; i < sizeof(bytes); ++i) {
      memcpy(different, bytes, sizeof(bytes));
      different[i] ^= 0xff;
      CHECK(cpkt_opcua_constantTimeEqual(bytes, different, length) ==
            cpkt_types_peer_constant_equal(bytes, different, length));
    }
  }
}

void cpkt_types_test_identifiers(void) {
  factories();
  ranges();
  endpoints();
  readers();
  identifiers();
  randoms();
  namespaces();
  times();
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  failures();
#endif
}
