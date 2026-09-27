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
void cpkt_types_test_identifiers(void) {
  identifiers();
  randoms();
  namespaces();
  times();
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  failures();
#endif
}
