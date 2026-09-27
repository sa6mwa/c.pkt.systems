#include "opcua_callbacks_test.h"
#include "opcua_types_peer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void check(int ok, const char *expression, int line) {
  if (!ok) {
    fprintf(stderr, "async client line %d: %s\n", line, expression);
    abort();
  }
}
#define CHECK(e) check(!!(e), #e, __LINE__)
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
void cpkt_types_fail_after(size_t count);
int cpkt_types_fail_stop(void);
#endif
static cpkt_opcua_NodeId node(unsigned int id) {
  cpkt_opcua_NodeId result;
  cpkt_opcua_NodeId_init(&result);
  result.namespaceIndex = 1;
  result.identifier.numeric = id;
  return result;
}
static cpkt_opcua_NodeId zero(unsigned int id) {
  cpkt_opcua_NodeId result = node(id);
  result.namespaceIndex = 0;
  return result;
}
static cpkt_opcua_QualifiedName name(const char *text) {
  cpkt_opcua_QualifiedName result;
  cpkt_opcua_QualifiedName_init(&result);
  result.namespaceIndex = 1;
  result.name.length = strlen(text);
  result.name.data = (cpkt_opcua_Byte *)text;
  return result;
}
struct result {
  cpkt_opcua_client *client;
  const cpkt_opcua_Type *type;
  cpkt_opcua_UInt32 id;
  cpkt_opcua_StatusCode native, conversion;
  int calls, no_copy;
  struct result *reentrant;
  void *copy;
};
static void cb_Value(cpkt_opcua_client *, void *, cpkt_opcua_UInt32,
                     cpkt_opcua_StatusCode, cpkt_opcua_StatusCode,
                     cpkt_opcua_DataValue *);
static void setup(struct result *state, cpkt_opcua_client *client,
                  size_t type) {
  memset(state, 0, sizeof(*state));
  state->client = client;
  state->type = cpkt_opcua_type_at(type);
  CHECK(state->type);
}
static void capture(cpkt_opcua_client *client, void *user, cpkt_opcua_UInt32 id,
                    cpkt_opcua_StatusCode native,
                    cpkt_opcua_StatusCode conversion,
                    const cpkt_opcua_Type *type, const void *value) {
  struct result *state = user;
  CHECK(state && state->client == client && id &&
        (!state->id || state->id == id) && state->type == type &&
        !state->calls);
  state->id = id;
  state->calls = 1;
  state->native = native;
  state->conversion = conversion;
  CHECK(!conversion || !value);
  if (value && state->no_copy) {
    const cpkt_opcua_DataValue *data = value;
    const cpkt_opcua_Int64 *integers = data->value.data;
    CHECK(type == cpkt_opcua_type_at(CPKT_OPCUA_TYPES_DATAVALUE) &&
          data->hasValue &&
          data->value.type == cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64) &&
          data->value.arrayLength == 2 && integers &&
          integers[0].high32 == 0x80000000U && !integers[0].low32 &&
          integers[1].high32 == 0x7fffffffU &&
          integers[1].low32 == 0xffffffffU);
  }
  if (value && !state->no_copy) {
    state->copy = cpkt_opcua_type_new(type);
    CHECK(state->copy);
    CHECK(!cpkt_opcua_type_copy(value, state->copy, type));
  }
  if (state->reentrant) {
    struct result *next = state->reentrant;
    CHECK(!cpkt_opcua_client_readValueAttribute_async_typed(
        client, node(6201), cb_Value, next, &next->id));
  }
}
static void wait_result(struct result *state) {
  size_t i;
  for (i = 0; i < 200 && !state->calls; ++i)
    CHECK(cpkt_opcua_client_run_iterate(state->client, 10, NULL) ==
          CPKT_OPCUA_OK);
  CHECK(state->calls == 1 && !state->conversion);
}
static void clear_result(struct result *state) {
  cpkt_opcua_type_delete(state->copy, state->type);
  state->copy = NULL;
}
#define RECORD_CALLBACK(label, Type, TYPE)                                     \
  static void cb_##label(                                                      \
      cpkt_opcua_client *client, void *user, cpkt_opcua_UInt32 id,             \
      cpkt_opcua_StatusCode conversion, cpkt_opcua_##Type *value) {            \
    capture(client, user, id, value ? value->responseHeader.serviceResult : 0, \
            conversion, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_##TYPE), value);   \
  }
RECORD_CALLBACK(read, ReadResponse, READRESPONSE)
RECORD_CALLBACK(write, WriteResponse, WRITERESPONSE)
RECORD_CALLBACK(browse, BrowseResponse, BROWSERESPONSE)
RECORD_CALLBACK(next, BrowseNextResponse, BROWSENEXTRESPONSE)
RECORD_CALLBACK(call, CallResponse, CALLRESPONSE)
RECORD_CALLBACK(add, AddNodesResponse, ADDNODESRESPONSE)
#define ATTRIBUTE_CALLBACK(label, Type, TYPE)                                  \
  static void cb_##label(cpkt_opcua_client *client, void *user,                \
                         cpkt_opcua_UInt32 id, cpkt_opcua_StatusCode native,   \
                         cpkt_opcua_StatusCode conversion,                     \
                         cpkt_opcua_##Type *value) {                           \
    capture(client, user, id, native, conversion,                              \
            cpkt_opcua_type_at(CPKT_OPCUA_TYPES_##TYPE), value);               \
  }
ATTRIBUTE_CALLBACK(Attribute, DataValue, DATAVALUE)
ATTRIBUTE_CALLBACK(Value, DataValue, DATAVALUE)
ATTRIBUTE_CALLBACK(DataType, NodeId, NODEID)
ATTRIBUTE_CALLBACK(ArrayDimensions, Variant, VARIANT)
ATTRIBUTE_CALLBACK(NodeClass, NodeClass, NODECLASS)
ATTRIBUTE_CALLBACK(BrowseName, QualifiedName, QUALIFIEDNAME)
ATTRIBUTE_CALLBACK(DisplayName, LocalizedText, LOCALIZEDTEXT)
ATTRIBUTE_CALLBACK(Description, LocalizedText, LOCALIZEDTEXT)
ATTRIBUTE_CALLBACK(WriteMask, UInt32, UINT32)
ATTRIBUTE_CALLBACK(UserWriteMask, UInt32, UINT32)
ATTRIBUTE_CALLBACK(IsAbstract, Boolean, BOOLEAN)
ATTRIBUTE_CALLBACK(Symmetric, Boolean, BOOLEAN)
ATTRIBUTE_CALLBACK(InverseName, LocalizedText, LOCALIZEDTEXT)
ATTRIBUTE_CALLBACK(ContainsNoLoops, Boolean, BOOLEAN)
ATTRIBUTE_CALLBACK(EventNotifier, Byte, BYTE)
ATTRIBUTE_CALLBACK(ValueRank, Int32, INT32)
ATTRIBUTE_CALLBACK(AccessLevel, Byte, BYTE)
ATTRIBUTE_CALLBACK(AccessLevelEx, UInt32, UINT32)
ATTRIBUTE_CALLBACK(UserAccessLevel, Byte, BYTE)
ATTRIBUTE_CALLBACK(MinimumSamplingInterval, Double, DOUBLE)
ATTRIBUTE_CALLBACK(Historizing, Boolean, BOOLEAN)
ATTRIBUTE_CALLBACK(Executable, Boolean, BOOLEAN)
ATTRIBUTE_CALLBACK(UserExecutable, Boolean, BOOLEAN)
static void cb_generic(cpkt_opcua_client *client, void *user,
                       cpkt_opcua_UInt32 id, cpkt_opcua_StatusCode conversion,
                       void *value) {
  struct result *state = user;
  cpkt_opcua_StatusCode native = 0;
  if (value &&
      state->type == cpkt_opcua_type_at(CPKT_OPCUA_TYPES_QUERYFIRSTRESPONSE))
    native =
        ((cpkt_opcua_QueryFirstResponse *)value)->responseHeader.serviceResult;
  capture(client, user, id, native, conversion, state->type, value);
}
#define READ(method, node_id, Type, TYPE, expected)                            \
  do {                                                                         \
    setup(&state, client, CPKT_OPCUA_TYPES_##TYPE);                            \
    CHECK(!cpkt_opcua_client_read##method##Attribute_async_typed(              \
        client, node_id, cb_##method, &state, &state.id));                     \
    wait_result(&state);                                                       \
    CHECK(!state.native && state.copy &&                                       \
          *(cpkt_opcua_##Type *)state.copy == (expected));                     \
    clear_result(&state);                                                      \
  } while (0)
#define WRITE(method, node_id, Type, expected, status)                         \
  do {                                                                         \
    cpkt_opcua_##Type attr = (expected);                                       \
    setup(&state, client, CPKT_OPCUA_TYPES_WRITERESPONSE);                     \
    CHECK(!cpkt_opcua_client_write##method##Attribute_async_typed(             \
        client, node_id, &attr, cb_write, &state, &state.id));                 \
    wait_result(&state);                                                       \
    CHECK(!state.native && state.copy &&                                       \
          ((cpkt_opcua_WriteResponse *)state.copy)->resultsSize == 1 &&        \
          ((cpkt_opcua_WriteResponse *)state.copy)->results[0] == (status));   \
    clear_result(&state);                                                      \
  } while (0)
#define WRITE_READ(method, node_id, Type, TYPE, expected)                      \
  do {                                                                         \
    WRITE(method, node_id, Type, expected, 0);                                 \
    READ(method, node_id, Type, TYPE, expected);                               \
  } while (0)
#define LABEL(method, node_id, literal)                                        \
  do {                                                                         \
    cpkt_opcua_LocalizedText attr;                                             \
    cpkt_opcua_LocalizedText_init(&attr);                                      \
    attr.text = name(literal).name;                                            \
    setup(&state, client, CPKT_OPCUA_TYPES_WRITERESPONSE);                     \
    CHECK(!cpkt_opcua_client_write##method##Attribute_async_typed(             \
        client, node_id, &attr, cb_write, &state, &state.id));                 \
    wait_result(&state);                                                       \
    CHECK(!state.native &&                                                     \
          ((cpkt_opcua_WriteResponse *)state.copy)->results[0] == 0);          \
    clear_result(&state);                                                      \
    setup(&state, client, CPKT_OPCUA_TYPES_LOCALIZEDTEXT);                     \
    CHECK(!cpkt_opcua_client_read##method##Attribute_async_typed(              \
        client, node_id, cb_##method, &state, &state.id));                     \
    wait_result(&state);                                                       \
    CHECK(!state.native &&                                                     \
          cpkt_opcua_String_equal(                                             \
              &((cpkt_opcua_LocalizedText *)state.copy)->text, &attr.text));   \
    clear_result(&state);                                                      \
  } while (0)
static cpkt_opcua_status native_output(void *client, void *user) {
  (void)user;
  return cpkt_types_peer_async_add_output(client);
}
struct timeout_control {
  unsigned int value, previous;
};
static cpkt_opcua_status native_timeout(void *client, void *user) {
  struct timeout_control *control = user;
  control->previous = cpkt_types_peer_client_timeout(client, control->value);
  return 0;
}
static int null_calls;
static cpkt_opcua_client *null_client;
static void cb_null(cpkt_opcua_client *client, void *user, cpkt_opcua_UInt32 id,
                    cpkt_opcua_StatusCode native,
                    cpkt_opcua_StatusCode conversion,
                    cpkt_opcua_DataValue *value) {
  CHECK(client == null_client && !user && id && !native && !conversion &&
        value && value->hasValue);
  ++null_calls;
}
/* The synchronous client fixture owns nodes 6200..6207 and calls this before
 * deleting them. Native peer methods 6100/6101 are independently implemented.
 */
void cpkt_types_test_async_client(cpkt_opcua_client *client, void *peer) {
  struct result state, shutdown[2];
  cpkt_opcua_Variant variant;
  cpkt_opcua_Int64 integers[2], *received;
  cpkt_opcua_ReadRequest read;
  cpkt_opcua_ReadValueId rvi;
  cpkt_opcua_WriteRequest write;
  cpkt_opcua_WriteValue wvi;
  cpkt_opcua_BrowseRequest browse;
  cpkt_opcua_BrowseDescription description;
  cpkt_opcua_BrowseNextRequest next;
  cpkt_opcua_ByteString continuation;
  cpkt_opcua_QueryFirstRequest query;
  cpkt_opcua_NodeId ignored = node(999), typeid;
  cpkt_opcua_QualifiedName qualified;
  cpkt_opcua_client *disconnected = NULL, *closing = NULL;
  cpkt_opcua_UInt32 cancelled, ignored_id;
  cpkt_opcua_StatusCode status;
  struct timeout_control timeout;
  unsigned short port;
  char endpoint[64];
  void *closing_peer;
  size_t i;
  integers[0].high32 = 0x80000000U;
  integers[0].low32 = 0;
  integers[1].high32 = 0x7fffffffU;
  integers[1].low32 = 0xffffffffU;
  cpkt_opcua_Variant_init(&variant);
  variant.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
  variant.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
  variant.arrayLength = 2;
  variant.data = integers;
  setup(&state, client, CPKT_OPCUA_TYPES_WRITERESPONSE);
  CHECK(!cpkt_opcua_client_writeValueAttribute_async_typed(
      client, node(6201), &variant, cb_write, &state, &state.id));
  memset(integers, 0, sizeof(integers)); /* encoded during submission */
  wait_result(&state);
  CHECK(!state.native &&
        ((cpkt_opcua_WriteResponse *)state.copy)->results[0] == 0);
  clear_result(&state);
  integers[0].high32 = 0x80000000U;
  integers[1].high32 = 0x7fffffffU;
  integers[1].low32 = 0xffffffffU;
  setup(&state, client, CPKT_OPCUA_TYPES_DATAVALUE);
  CHECK(!cpkt_opcua_client_readValueAttribute_async_typed(
      client, node(6201), cb_Value, &state, &state.id));
  wait_result(&state);
  CHECK(!state.native && ((cpkt_opcua_DataValue *)state.copy)->hasValue);
  received = ((cpkt_opcua_DataValue *)state.copy)->value.data;
  CHECK(((cpkt_opcua_DataValue *)state.copy)->value.arrayLength == 2 &&
        !memcmp(received, integers, sizeof(integers)));
  clear_result(&state);
  WRITE_READ(WriteMask, node(6201), UInt32, UINT32, 0xffffffffU);
  READ(UserWriteMask, node(6201), UInt32, UINT32, 0xffffffffU);
  WRITE_READ(IsAbstract, node(6203), Boolean, BOOLEAN, 1);
  WRITE_READ(Symmetric, node(6204), Boolean, BOOLEAN, 0);
  WRITE_READ(ContainsNoLoops, node(6206), Boolean, BOOLEAN, 1);
  WRITE_READ(EventNotifier, node(6200), Byte, BYTE, 1);
  WRITE_READ(ValueRank, node(6201), Int32, INT32, 1);
  WRITE_READ(AccessLevel, node(6201), Byte, BYTE, 0xff);
  WRITE_READ(AccessLevelEx, node(6201), UInt32, UINT32, 0xff);
  READ(UserAccessLevel, node(6201), Byte, BYTE, 0xff);
  WRITE_READ(MinimumSamplingInterval, node(6201), Double, DOUBLE, 25.5);
  WRITE_READ(Historizing, node(6201), Boolean, BOOLEAN, 1);
  WRITE_READ(Executable, node(6207), Boolean, BOOLEAN, 0);
  READ(UserExecutable, node(6207), Boolean, BOOLEAN, 0);
  READ(NodeClass, node(6201), NodeClass, NODECLASS,
       cpkt_opcua_NODECLASS_VARIABLE);
  LABEL(DisplayName, node(6201), "async-display");
  LABEL(Description, node(6201), "async-description");
  LABEL(InverseName, node(6204), "async-inverse");
  WRITE(NodeClass, node(6201), NodeClass, cpkt_opcua_NODECLASS_VARIABLE,
        CPKT_OPCUA_STATUSCODE_BADWRITENOTSUPPORTED);
  qualified = name("unchanged");
  setup(&state, client, CPKT_OPCUA_TYPES_WRITERESPONSE);
  CHECK(!cpkt_opcua_client_writeBrowseNameAttribute_async_typed(
      client, node(6201), &qualified, cb_write, &state, &state.id));
  wait_result(&state);
  CHECK(((cpkt_opcua_WriteResponse *)state.copy)->results[0] ==
        CPKT_OPCUA_STATUSCODE_BADWRITENOTSUPPORTED);
  clear_result(&state);
  setup(&state, client, CPKT_OPCUA_TYPES_QUALIFIEDNAME);
  CHECK(!cpkt_opcua_client_readBrowseNameAttribute_async_typed(
      client, node(6201), cb_BrowseName, &state, &state.id));
  wait_result(&state);
  qualified = name("client-variable");
  CHECK(cpkt_opcua_String_equal(&((cpkt_opcua_QualifiedName *)state.copy)->name,
                                &qualified.name));
  clear_result(&state);
  typeid = zero(CPKT_OPCUA_NS0ID_INT64);
  setup(&state, client, CPKT_OPCUA_TYPES_WRITERESPONSE);
  CHECK(!cpkt_opcua_client_writeDataTypeAttribute_async_typed(
      client, node(6201), &typeid, cb_write, &state, &state.id));
  wait_result(&state);
  CHECK(!((cpkt_opcua_WriteResponse *)state.copy)->results[0]);
  clear_result(&state);
  setup(&state, client, CPKT_OPCUA_TYPES_NODEID);
  CHECK(!cpkt_opcua_client_readDataTypeAttribute_async_typed(
      client, node(6201), cb_DataType, &state, &state.id));
  wait_result(&state);
  CHECK(cpkt_opcua_NodeId_equal(state.copy, &typeid));
  clear_result(&state);
  WRITE(NodeId, node(6201), NodeId, ignored,
        CPKT_OPCUA_STATUSCODE_BADWRITENOTSUPPORTED);
  setup(&state, client, CPKT_OPCUA_TYPES_VARIANT);
  CHECK(!cpkt_opcua_client_readArrayDimensionsAttribute_async_typed(
      client, node(6201), cb_ArrayDimensions, &state, &state.id));
  wait_result(&state);
  CHECK(((cpkt_opcua_Variant *)state.copy)->arrayLength == 1 &&
        *(cpkt_opcua_UInt32 *)((cpkt_opcua_Variant *)state.copy)->data == 2);
  clear_result(&state);
  cpkt_opcua_ReadValueId_init(&rvi);
  rvi.nodeId = node(6201);
  rvi.attributeId = 13;
  setup(&state, client, CPKT_OPCUA_TYPES_DATAVALUE);
  CHECK(!cpkt_opcua_client_readAttribute_async_typed(
      client, &rvi, cpkt_opcua_TIMESTAMPSTORETURN_BOTH, cb_Attribute, &state,
      &state.id));
  wait_result(&state);
  CHECK(((cpkt_opcua_DataValue *)state.copy)->hasValue &&
        ((cpkt_opcua_DataValue *)state.copy)->hasServerTimestamp);
  clear_result(&state);
  cpkt_opcua_ReadRequest_init(&read);
  read.nodesToReadSize = 1;
  read.nodesToRead = &rvi;
  setup(&state, client, CPKT_OPCUA_TYPES_READRESPONSE);
  CHECK(!cpkt_opcua_client_sendAsyncReadRequest_typed(client, &read, cb_read,
                                                      &state, &state.id));
  CHECK(read.requestHeader.requestHandle > 100000 &&
        read.requestHeader.timeoutHint &&
        (read.requestHeader.timestamp.high32 ||
         read.requestHeader.timestamp.low32) &&
        read.nodesToRead == &rvi);
  wait_result(&state);
  CHECK(((cpkt_opcua_ReadResponse *)state.copy)->resultsSize == 1 &&
        ((cpkt_opcua_ReadResponse *)state.copy)->results[0].hasValue);
  clear_result(&state);
  cpkt_opcua_WriteValue_init(&wvi);
  wvi.nodeId = rvi.nodeId;
  wvi.attributeId = 13;
  wvi.value.hasValue = 1;
  wvi.value.value = variant;
  cpkt_opcua_WriteRequest_init(&write);
  write.nodesToWriteSize = 1;
  write.nodesToWrite = &wvi;
  setup(&state, client, CPKT_OPCUA_TYPES_WRITERESPONSE);
  CHECK(!cpkt_opcua_client_sendAsyncWriteRequest_typed(client, &write, cb_write,
                                                       &state, &state.id));
  wait_result(&state);
  CHECK(!((cpkt_opcua_WriteResponse *)state.copy)->results[0]);
  clear_result(&state);
  cpkt_opcua_BrowseDescription_init(&description);
  description.nodeId = node(6200);
  description.resultMask = 63;
  cpkt_opcua_BrowseRequest_init(&browse);
  browse.nodesToBrowseSize = 1;
  browse.nodesToBrowse = &description;
  browse.requestedMaxReferencesPerNode = 1;
  setup(&state, client, CPKT_OPCUA_TYPES_BROWSERESPONSE);
  CHECK(!cpkt_opcua_client_sendAsyncBrowseRequest_typed(
      client, &browse, cb_browse, &state, &state.id));
  wait_result(&state);
  CHECK(((cpkt_opcua_BrowseResponse *)state.copy)->resultsSize == 1 &&
        ((cpkt_opcua_BrowseResponse *)state.copy)->results[0].referencesSize ==
            1);
  cpkt_opcua_ByteString_init(&continuation);
  CHECK(!cpkt_opcua_ByteString_copy(
      &((cpkt_opcua_BrowseResponse *)state.copy)->results[0].continuationPoint,
      &continuation));
  clear_result(&state);
  CHECK(continuation.length);
  cpkt_opcua_BrowseNextRequest_init(&next);
  next.continuationPointsSize = 1;
  next.continuationPoints = &continuation;
  next.releaseContinuationPoints = 1;
  setup(&state, client, CPKT_OPCUA_TYPES_BROWSENEXTRESPONSE);
  CHECK(!cpkt_opcua_client_sendAsyncBrowseNextRequest_typed(
      client, &next, cb_next, &state, &state.id));
  cpkt_opcua_ByteString_clear(&continuation);
  wait_result(&state);
  CHECK(!state.native &&
        ((cpkt_opcua_BrowseNextResponse *)state.copy)->resultsSize == 1);
  clear_result(&state);
  setup(&state, client, CPKT_OPCUA_TYPES_CALLRESPONSE);
  CHECK(!cpkt_opcua_client_call_async_typed(
      client, zero(CPKT_OPCUA_NS0ID_OBJECTSFOLDER), node(6100), 1, &variant,
      cb_call, &state, &state.id));
  wait_result(&state);
  CHECK(
      ((cpkt_opcua_CallResponse *)state.copy)->resultsSize == 1 &&
      ((cpkt_opcua_CallResponse *)state.copy)->results[0].statusCode ==
          CPKT_OPCUA_STATUSCODE_GOODCLAMPED &&
      ((cpkt_opcua_CallResponse *)state.copy)->results[0].outputArgumentsSize ==
          1);
  CHECK(!memcmp(((cpkt_opcua_CallResponse *)state.copy)
                    ->results[0]
                    .outputArguments[0]
                    .data,
                integers, sizeof(integers)));
  clear_result(&state);
  setup(&state, client, CPKT_OPCUA_TYPES_READRESPONSE);
  CHECK(!cpkt_opcua_client_AsyncService_typed(
      client, &read, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_READREQUEST),
      cb_generic, state.type, &state, &state.id));
  wait_result(&state);
  CHECK(((cpkt_opcua_ReadResponse *)state.copy)->results[0].hasValue);
  clear_result(&state);
  cpkt_opcua_QueryFirstRequest_init(&query);
  setup(&state, client, CPKT_OPCUA_TYPES_QUERYFIRSTRESPONSE);
  CHECK(!cpkt_opcua_client_AsyncService_typed(
      client, &query, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_QUERYFIRSTREQUEST),
      cb_generic, state.type, &state, &state.id));
  wait_result(&state);
  CHECK(state.native == CPKT_OPCUA_STATUSCODE_BADSERVICEUNSUPPORTED);
  clear_result(&state);
  CHECK(!cpkt_opcua_client_writeValueAttribute_async_typed(
            client, node(6201), &variant, NULL, &state, &ignored_id) &&
        ignored_id);
  CHECK(!cpkt_opcua_client_sendAsyncReadRequest_typed(client, &read, NULL, NULL,
                                                      NULL));
  CHECK(!cpkt_opcua_client_call_async_typed(
      client, zero(CPKT_OPCUA_NS0ID_OBJECTSFOLDER), node(6100), 1, &variant,
      NULL, NULL, NULL));
  {
    cpkt_opcua_ObjectAttributes attr;
    cpkt_opcua_NodeId assigned;
    CHECK(!cpkt_opcua_ObjectAttributes_default(&attr));
    CHECK(!cpkt_opcua_client_addObjectNode_async_typed(
        client, node(6560), node(6200), zero(CPKT_OPCUA_NS0ID_HASCOMPONENT),
        name("no-callback"), zero(CPKT_OPCUA_NS0ID_BASEOBJECTTYPE), attr,
        &ignored, NULL, NULL, NULL));
    cpkt_opcua_ObjectAttributes_clear(&attr);
    CHECK(!cpkt_opcua_client_readNodeIdAttribute_typed(client, node(6560),
                                                       &assigned));
    CHECK(cpkt_opcua_NodeId_equal(&assigned, &(ignored)) == 0 &&
          ignored.identifier.numeric == 999);
    cpkt_opcua_NodeId_clear(&assigned);
    CHECK(!cpkt_opcua_client_deleteNode_typed(client, node(6560), 1));
  }
  CHECK(!cpkt_opcua_client_AsyncService_typed(
      client, &write, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_WRITEREQUEST), NULL,
      cpkt_opcua_type_at(CPKT_OPCUA_TYPES_WRITERESPONSE), NULL, NULL));
  cpkt_opcua_Variant_init(&variant);
  CHECK(!cpkt_opcua_client_readValueAttribute_typed(client, node(6201),
                                                    &variant));
  CHECK(variant.arrayLength == 2 &&
        !memcmp(variant.data, integers, sizeof(integers)));
  cpkt_opcua_Variant_clear(&variant);
  variant.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
  variant.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
  variant.arrayLength = 2;
  variant.data = integers;
  cancelled = 777;
  CHECK(cpkt_opcua_client_cancelByRequestId_typed(client, 0xffffffffU,
                                                  &cancelled) ==
            CPKT_OPCUA_STATUSCODE_BADNOTFOUND &&
        cancelled == 777);
  CHECK(cpkt_opcua_client_cancelByRequestId_typed(client, 0xffffffffU, NULL) ==
        CPKT_OPCUA_STATUSCODE_BADNOTFOUND);
  CHECK(!cpkt_opcua_client_cancelByRequestHandle_typed(client, 0xffffffffU,
                                                       &cancelled) &&
        !cancelled);
  CHECK(!cpkt_opcua_client_cancelByRequestHandle_typed(client, 0xffffffffU,
                                                       NULL));
  CHECK(cpkt_opcua_client_renewSecureChannel_typed(client) ==
        CPKT_OPCUA_STATUSCODE_GOODCALLAGAIN);
  CHECK(cpkt_opcua_client_native(client, native_output, NULL) == CPKT_OPCUA_OK);
  {
    cpkt_opcua_ObjectAttributes oa;
    cpkt_opcua_VariableAttributes va;
    cpkt_opcua_VariableTypeAttributes vta;
    cpkt_opcua_ObjectTypeAttributes ota;
    cpkt_opcua_ReferenceTypeAttributes rta;
    cpkt_opcua_DataTypeAttributes dta;
    cpkt_opcua_ViewAttributes view;
    cpkt_opcua_MethodAttributes ma;
    CHECK(!cpkt_opcua_ObjectAttributes_default(&oa));
    CHECK(!cpkt_opcua_VariableAttributes_default(&va));
    CHECK(!cpkt_opcua_VariableTypeAttributes_default(&vta));
    CHECK(!cpkt_opcua_ObjectTypeAttributes_default(&ota));
    CHECK(!cpkt_opcua_ReferenceTypeAttributes_default(&rta));
    CHECK(!cpkt_opcua_DataTypeAttributes_default(&dta));
    CHECK(!cpkt_opcua_ViewAttributes_default(&view));
    CHECK(!cpkt_opcua_MethodAttributes_default(&ma));
#define ADD_DONE(expected)                                                     \
  do {                                                                         \
    wait_result(&state);                                                       \
    CHECK(                                                                     \
        ignored.identifier.numeric == 999 &&                                   \
        ((cpkt_opcua_AddNodesResponse *)state.copy)->resultsSize == 1 &&       \
        !((cpkt_opcua_AddNodesResponse *)state.copy)->results[0].statusCode && \
        ((cpkt_opcua_AddNodesResponse *)state.copy)                            \
                ->results[0]                                                   \
                .addedNodeId.identifier.numeric == (expected));                \
    clear_result(&state);                                                      \
  } while (0)
#define ADD_SETUP() setup(&state, client, CPKT_OPCUA_TYPES_ADDNODESRESPONSE)
    ADD_SETUP();
    CHECK(!cpkt_opcua_client_addObjectNode_async_typed(
        client, node(6500), node(6200), zero(CPKT_OPCUA_NS0ID_HASCOMPONENT),
        name("async-object"), zero(CPKT_OPCUA_NS0ID_BASEOBJECTTYPE), oa,
        &ignored, cb_add, &state, &state.id));
    cpkt_opcua_ObjectAttributes_clear(&oa);
    ADD_DONE(6500);
    va.dataType = zero(CPKT_OPCUA_NS0ID_INT64);
    va.valueRank = -1;
    va.value = variant;
    va.value.arrayLength = 0;
    ADD_SETUP();
    CHECK(!cpkt_opcua_client_addVariableNode_async_typed(
        client, node(6501), node(6500), zero(CPKT_OPCUA_NS0ID_HASCOMPONENT),
        name("async-variable"), zero(CPKT_OPCUA_NS0ID_BASEDATAVARIABLETYPE), va,
        &ignored, cb_add, &state, &state.id));
    memset(&va.value, 0, sizeof(va.value));
    cpkt_opcua_VariableAttributes_clear(&va);
    ADD_DONE(6501);
    ADD_SETUP();
    CHECK(!cpkt_opcua_client_addVariableTypeNode_async_typed(
        client, node(6502), zero(CPKT_OPCUA_NS0ID_BASEVARIABLETYPE),
        zero(CPKT_OPCUA_NS0ID_HASSUBTYPE), name("async-variable-type"), vta,
        &ignored, cb_add, &state, &state.id));
    cpkt_opcua_VariableTypeAttributes_clear(&vta);
    ADD_DONE(6502);
    ADD_SETUP();
    CHECK(!cpkt_opcua_client_addObjectTypeNode_async_typed(
        client, node(6503), zero(CPKT_OPCUA_NS0ID_BASEOBJECTTYPE),
        zero(CPKT_OPCUA_NS0ID_HASSUBTYPE), name("async-object-type"), ota,
        &ignored, cb_add, &state, &state.id));
    cpkt_opcua_ObjectTypeAttributes_clear(&ota);
    ADD_DONE(6503);
    ADD_SETUP();
    CHECK(!cpkt_opcua_client_addReferenceTypeNode_async_typed(
        client, node(6504), zero(CPKT_OPCUA_NS0ID_NONHIERARCHICALREFERENCES),
        zero(CPKT_OPCUA_NS0ID_HASSUBTYPE), name("async-reference-type"), rta,
        &ignored, cb_add, &state, &state.id));
    cpkt_opcua_ReferenceTypeAttributes_clear(&rta);
    ADD_DONE(6504);
    ADD_SETUP();
    CHECK(!cpkt_opcua_client_addDataTypeNode_async_typed(
        client, node(6505), zero(CPKT_OPCUA_NS0ID_BASEDATATYPE),
        zero(CPKT_OPCUA_NS0ID_HASSUBTYPE), name("async-data-type"), dta,
        &ignored, cb_add, &state, &state.id));
    cpkt_opcua_DataTypeAttributes_clear(&dta);
    ADD_DONE(6505);
    ADD_SETUP();
    CHECK(!cpkt_opcua_client_addViewNode_async_typed(
        client, node(6506), zero(CPKT_OPCUA_NS0ID_VIEWSFOLDER),
        zero(CPKT_OPCUA_NS0ID_ORGANIZES), name("async-view"), view, NULL,
        cb_add, &state, &state.id));
    cpkt_opcua_ViewAttributes_clear(&view);
    ADD_DONE(6506);
    ADD_SETUP();
    CHECK(!cpkt_opcua_client_addMethodNode_async_typed(
        client, node(6507), node(6500), zero(CPKT_OPCUA_NS0ID_HASCOMPONENT),
        name("async-method"), ma, &ignored, cb_add, &state, NULL));
    cpkt_opcua_MethodAttributes_clear(&ma);
    ADD_DONE(6507);
    for (i = 8; i > 0; --i)
      CHECK(!cpkt_opcua_client_deleteNode_typed(
          client, node(6499 + (unsigned int)i), 1));
  }
  setup(&state, client, CPKT_OPCUA_TYPES_DATAVALUE);
  CHECK(!cpkt_opcua_client_readValueAttribute_async_typed(
      client, node(0xffffffU), cb_Value, &state, &state.id));
  wait_result(&state);
  CHECK(!state.native && state.copy &&
        ((cpkt_opcua_DataValue *)state.copy)->status ==
            CPKT_OPCUA_STATUSCODE_BADNODEIDUNKNOWN);
  clear_result(&state);
  setup(&state, client, CPKT_OPCUA_TYPES_NODECLASS);
  CHECK(!cpkt_opcua_client_readNodeClassAttribute_async_typed(
      client, node(0xffffffU), cb_NodeClass, &state, &state.id));
  wait_result(&state); /* Native typed reads without a value report
                        * BadInternalError. The full DataValue callback above
                        * retains the per-operation BadNodeIdUnknown. */
  CHECK(state.native == CPKT_OPCUA_STATUSCODE_BADINTERNALERROR && !state.copy);
  setup(&state, client, CPKT_OPCUA_TYPES_DATAVALUE);
  state.id = 999;
  CHECK(cpkt_opcua_client_readValueAttribute_async_typed(
            NULL, node(6201), cb_Value, &state, &state.id) ==
            CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT &&
        !state.id && !state.calls);
  CHECK(cpkt_opcua_client_readValueAttribute_async_typed(
            client, node(6201), NULL, &state, &state.id) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_client_AsyncService_typed(client, &read, NULL, cb_generic,
                                             state.type, &state, &state.id) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_client_AsyncService_typed(
            client, integers, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64),
            cb_generic, state.type, &state,
            &state.id) == CPKT_OPCUA_STATUSCODE_BADTYPEMISMATCH &&
        !state.id && !state.calls);
  CHECK(cpkt_opcua_client_call_async_typed(
            client, node(6200), node(6100), (size_t)-1, &variant, cb_call,
            &state, &state.id) == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY &&
        !state.calls);
  CHECK(cpkt_opcua_client_new(&disconnected) == CPKT_OPCUA_OK);
  CHECK(cpkt_opcua_client_readValueAttribute_async_typed(
            disconnected, node(6201), cb_Value, &state, &state.id) != 0 &&
        !state.id && !state.calls);
  cpkt_opcua_client_free(disconnected);
  null_client = client;
  null_calls = 0;
  CHECK(!cpkt_opcua_client_readValueAttribute_async_typed(client, node(6201),
                                                          cb_null, NULL, NULL));
  for (i = 0; i < 200 && !null_calls; ++i)
    CHECK(cpkt_opcua_client_run_iterate(client, 10, NULL) == CPKT_OPCUA_OK);
  CHECK(null_calls == 1);
  setup(&state, client, CPKT_OPCUA_TYPES_READRESPONSE);
  CHECK(!cpkt_opcua_client_sendAsyncReadRequest_typed(client, &read, cb_read,
                                                      &state, &state.id));
  CHECK(!cpkt_opcua_client_cancelByRequestId_typed(client, state.id,
                                                   &cancelled) &&
        !cancelled);
  wait_result(&state);
  CHECK(!state.native);
  clear_result(&state);
  read.requestHeader.requestHandle = 777;
  setup(&state, client, CPKT_OPCUA_TYPES_READRESPONSE);
  CHECK(!cpkt_opcua_client_sendAsyncReadRequest_typed(client, &read, cb_read,
                                                      &state, &state.id));
  CHECK(
      !cpkt_opcua_client_cancelByRequestHandle_typed(client, 777, &cancelled) &&
      !cancelled);
  wait_result(&state);
  CHECK(!state.native);
  clear_result(&state);
  read.requestHeader.requestHandle = 0;
  setup(&shutdown[0], client, CPKT_OPCUA_TYPES_DATAVALUE);
  setup(&shutdown[1], client, CPKT_OPCUA_TYPES_DATAVALUE);
  shutdown[0].reentrant = &shutdown[1];
  CHECK(!cpkt_opcua_client_readValueAttribute_async_typed(
      client, node(6201), cb_Value, &shutdown[0], &shutdown[0].id));
  wait_result(&shutdown[0]);
  wait_result(&shutdown[1]);
  CHECK(shutdown[0].id != shutdown[1].id && !shutdown[0].native &&
        !shutdown[1].native);
  clear_result(&shutdown[0]);
  clear_result(&shutdown[1]);
  timeout.value = 1;
  CHECK(cpkt_opcua_client_native(client, native_timeout, &timeout) ==
        CPKT_OPCUA_OK);
  setup(&state, client, CPKT_OPCUA_TYPES_DATAVALUE);
  cpkt_types_peer_pause(peer, 1);
  CHECK(!cpkt_opcua_client_readValueAttribute_async_typed(
      client, node(6201), cb_Value, &state, &state.id));
  wait_result(&state);
  CHECK(state.native == CPKT_OPCUA_STATUSCODE_BADTIMEOUT && !state.copy);
  cpkt_types_peer_pause(peer, 0);
  timeout.value = timeout.previous;
  CHECK(cpkt_opcua_client_native(client, native_timeout, &timeout) ==
        CPKT_OPCUA_OK);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  setup(&state, client, CPKT_OPCUA_TYPES_DATAVALUE);
  cpkt_types_fail_after(0);
  status = cpkt_opcua_client_readValueAttribute_async_typed(
      client, node(6201), cb_Value, &state, &state.id);
  CHECK(cpkt_types_fail_stop() &&
        status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY && !state.calls &&
        !state.id);
  for (i = 0; i < 32; ++i) {
    size_t j;
    setup(&state, client, CPKT_OPCUA_TYPES_DATAVALUE);
    state.no_copy = 1;
    cpkt_types_peer_client_fail_conversion(5, i);
    CHECK(!cpkt_opcua_client_readValueAttribute_async_typed(
        client, node(6201), cb_Value, &state, &state.id));
    for (j = 0; j < 200 && !state.calls; ++j)
      CHECK(cpkt_opcua_client_run_iterate(client, 10, NULL) == CPKT_OPCUA_OK);
    CHECK(state.calls == 1);
    if (!cpkt_types_fail_stop()) {
      CHECK(!state.conversion && !state.native);
      break;
    }
    CHECK(state.conversion == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY &&
          !state.native && !state.copy);
  }
  CHECK(i > 0 && i < 32);
#else
  (void)status;
#endif
  closing_peer = cpkt_types_peer_start(&port);
  CHECK(closing_peer);
  sprintf(endpoint, "opc.tcp://127.0.0.1:%u", (unsigned int)port);
  CHECK(cpkt_opcua_client_new(&closing) == CPKT_OPCUA_OK &&
        cpkt_opcua_client_connect(closing, endpoint, NULL) == CPKT_OPCUA_OK);
  timeout.value = 10;
  CHECK(cpkt_opcua_client_native(closing, native_timeout, &timeout) ==
        CPKT_OPCUA_OK);
  cpkt_types_peer_pause(closing_peer, 1);
  setup(&shutdown[0], closing, CPKT_OPCUA_TYPES_DATAVALUE);
  CHECK(!cpkt_opcua_client_readValueAttribute_async_typed(
      closing, node(6001), cb_Value, &shutdown[0], &shutdown[0].id));
  setup(&shutdown[1], closing, CPKT_OPCUA_TYPES_READRESPONSE);
  rvi.nodeId = node(6001);
  read.requestHeader.timeoutHint = 5000;
  CHECK(!cpkt_opcua_client_sendAsyncReadRequest_typed(
      closing, &read, cb_read, &shutdown[1], &shutdown[1].id));
  cpkt_opcua_client_free(closing);
  cpkt_types_peer_pause(closing_peer, 0);
  cpkt_types_peer_stop(closing_peer);
  /* Native deletion disconnects and closes the session before native clear.
   * The short operation may time out first; the long request is removed by
   * session cleanup with BadSessionClosed, not a facade-invented shutdown code.
   */
  CHECK(shutdown[0].calls == 1 && !shutdown[0].conversion &&
        !shutdown[0].copy &&
        (shutdown[0].native == CPKT_OPCUA_STATUSCODE_BADTIMEOUT ||
         shutdown[0].native == CPKT_OPCUA_STATUSCODE_BADSESSIONCLOSED));
  CHECK(shutdown[1].calls == 1 && !shutdown[1].conversion && shutdown[1].copy &&
        shutdown[1].native == CPKT_OPCUA_STATUSCODE_BADSESSIONCLOSED);
  clear_result(&shutdown[1]);
}
