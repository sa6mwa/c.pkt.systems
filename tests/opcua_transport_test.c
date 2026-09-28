/* C89 ownership, codecs and native event-loop regression coverage. */
#include "opcua_callbacks_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check(int value, const char *expression, int line) {
  if (!value) {
    fprintf(stderr, "transport line %d: %s\n", line, expression);
    abort();
  }
}
#define CHECK(e) check(!!(e), #e, __LINE__)
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
void cpkt_types_fail_after(size_t);
int cpkt_types_fail_stop(void);
#endif

static void ranges(void) {
  cpkt_opcua_Variant value, selected;
  cpkt_opcua_String *elements, incoming[2], copy, *after;
  cpkt_opcua_NumericRangeDimension dimensions[2];
  cpkt_opcua_NumericRange range;
  const cpkt_opcua_Type *type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_STRING);
  unsigned char *moved[2], *untouched;
  size_t i;
  memset(&value, 0, sizeof(value));
  memset(&selected, 0, sizeof(selected));
  memset(incoming, 0, sizeof(incoming));
  memset(&copy, 0, sizeof(copy));
  elements = (cpkt_opcua_String *)cpkt_opcua_array_new(6, type);
  CHECK(elements != NULL);
  for (i = 0; i < 6; ++i) {
    cpkt_opcua_String source = cpkt_opcua_STRING("original");
    CHECK(cpkt_opcua_String_copy(&source, &elements[i]) == 0);
  }
  value.type = type;
  value.arrayLength = 6;
  value.data = elements;
  value.arrayDimensionsSize = 2;
  value.arrayDimensions = (cpkt_opcua_UInt32 *)cpkt_opcua_array_new(
      2, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_UINT32));
  CHECK(value.arrayDimensions != NULL);
  value.arrayDimensions[0] = 2;
  value.arrayDimensions[1] = 3;
  dimensions[0].min = 0;
  dimensions[0].max = 1;
  dimensions[1].min = dimensions[1].max = 1;
  range.dimensionsSize = 2;
  range.dimensions = dimensions;
  copy = cpkt_opcua_STRING("moved-first");
  CHECK(cpkt_opcua_String_copy(&copy, &incoming[0]) == 0);
  copy = cpkt_opcua_STRING("moved-second");
  CHECK(cpkt_opcua_String_copy(&copy, &incoming[1]) == 0);
  moved[0] = incoming[0].data;
  moved[1] = incoming[1].data;
  untouched = elements[0].data;
  CHECK(cpkt_opcua_Variant_setRange(&value, incoming, 2, range) == 0);
  CHECK(value.data == elements && elements[0].data == untouched);
  CHECK(elements[1].data == moved[0] && elements[4].data == moved[1]);
  CHECK(incoming[0].data == NULL && incoming[1].data == NULL);
  CHECK(cpkt_opcua_Variant_copyRange(&value, &selected, range) == 0);
  after = (cpkt_opcua_String *)selected.data;
  CHECK(selected.arrayLength == 2 &&
        cpkt_opcua_String_equal(&after[0], &elements[1]) &&
        after[0].data != moved[0]);
  cpkt_opcua_Variant_clear(&selected);
  incoming[0] = cpkt_opcua_STRING("copied-first");
  incoming[1] = cpkt_opcua_STRING("copied-second");
  CHECK(cpkt_opcua_Variant_setRangeCopy(&value, incoming, 2, range) == 0);
  CHECK(value.data == elements && elements[0].data == untouched &&
        elements[1].data != incoming[0].data);
  CHECK(cpkt_opcua_String_equal(&elements[1], &incoming[0]) &&
        cpkt_opcua_String_equal(&elements[4], &incoming[1]));
  CHECK(cpkt_opcua_Variant_setRange(&value, incoming, 1, range) ==
        CPKT_OPCUA_STATUSCODE_BADINDEXRANGEINVALID);
  CHECK(incoming[0].data != NULL);
  dimensions[0].min = dimensions[0].max = 2;
  CHECK(cpkt_opcua_Variant_setRangeCopy(&value, incoming, 2, range) ==
        CPKT_OPCUA_STATUSCODE_BADINDEXRANGENODATA);
  cpkt_opcua_Variant_clear(&value);
}

static void codecs(void) {
  cpkt_opcua_Int64 input, output;
  cpkt_opcua_ByteString bytes;
  cpkt_opcua_EncodeBinaryOptions binary_encode;
  cpkt_opcua_DecodeBinaryOptions binary_decode;
  cpkt_opcua_EncodeJsonOptions json_encode;
  cpkt_opcua_DecodeJsonOptions json_decode;
  cpkt_opcua_EncodeXmlOptions xml_encode;
  cpkt_opcua_DecodeXmlOptions xml_decode;
  const cpkt_opcua_Type *type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
  size_t decoded = 0;
  input.high32 = 0x80000000U;
  input.low32 = 7;
  memset(&binary_encode, 0, sizeof(binary_encode));
  memset(&binary_decode, 0, sizeof(binary_decode));
  memset(&json_encode, 0, sizeof(json_encode));
  memset(&json_decode, 0, sizeof(json_decode));
  memset(&xml_encode, 0, sizeof(xml_encode));
  memset(&xml_decode, 0, sizeof(xml_decode));
  memset(&bytes, 0, sizeof(bytes));
  memset(&output, 0, sizeof(output));
  CHECK(cpkt_opcua_calcSizeBinary(&input, type, &binary_encode) == 8);
  CHECK(cpkt_opcua_encodeBinary(&input, type, &bytes, &binary_encode) == 0 &&
        bytes.length == 8);
  CHECK(bytes.data[0] == 7 && bytes.data[7] == 0x80);
  CHECK(cpkt_opcua_decodeBinary(&bytes, &output, type, &binary_decode) == 0 &&
        output.high32 == input.high32 && output.low32 == input.low32);
  --bytes.length;
  memset(&output, 0, sizeof(output));
  CHECK(cpkt_opcua_decodeBinary(&bytes, &output, type, &binary_decode) != 0 &&
        output.high32 == 0 && output.low32 == 0);
  cpkt_opcua_ByteString_clear(&bytes);
  CHECK(cpkt_opcua_encodeJson(&input, type, &bytes, &json_encode) == 0);
  CHECK(bytes.length == cpkt_opcua_calcSizeJson(&input, type, &json_encode));
  json_decode.decodedLength = &decoded;
  CHECK(cpkt_opcua_decodeJson(&bytes, &output, type, &json_decode) == 0 &&
        decoded == bytes.length);
  CHECK(output.high32 == input.high32 && output.low32 == input.low32);
  /* Consume a quoted value without consuming the next JSON value. */
  {
    cpkt_opcua_ByteString joined;
    size_t first_length = bytes.length;
    memset(&joined, 0, sizeof(joined));
    CHECK(cpkt_opcua_ByteString_allocBuffer(&joined, bytes.length * 2) == 0);
    memcpy(joined.data, bytes.data, bytes.length);
    memcpy(joined.data + bytes.length, bytes.data, bytes.length);
    decoded = 0;
    memset(&output, 0, sizeof(output));
    CHECK(cpkt_opcua_decodeJson(&joined, &output, type, &json_decode) == 0 &&
          decoded == first_length);
    CHECK(output.high32 == input.high32 && output.low32 == input.low32);
    cpkt_opcua_ByteString_clear(&joined);
  }
  cpkt_opcua_ByteString_clear(&bytes);
  CHECK(cpkt_opcua_encodeXml(&input, type, &bytes, &xml_encode) == 0);
  CHECK(bytes.length == cpkt_opcua_calcSizeXml(&input, type, &xml_encode));
  CHECK(cpkt_opcua_decodeXml(&bytes, &output, type, &xml_decode) == 0);
  CHECK(output.high32 == input.high32 && output.low32 == input.low32);
  cpkt_opcua_ByteString_clear(&bytes);
}

static void messages(void) {
  cpkt_opcua_NetworkMessage message, readback, headers, copied;
  cpkt_opcua_DataSetMessage data;
  cpkt_opcua_DataValue value;
  cpkt_opcua_UInt64 number;
  cpkt_opcua_ByteString bytes;
  size_t offset = 0;
  memset(&message, 0, sizeof(message));
  memset(&readback, 0, sizeof(readback));
  memset(&headers, 0, sizeof(headers));
  memset(&copied, 0, sizeof(copied));
  memset(&data, 0, sizeof(data));
  memset(&value, 0, sizeof(value));
  memset(&bytes, 0, sizeof(bytes));
  number.high32 = 0xffffffffU;
  number.low32 = 7;
  value.hasValue = 1;
  value.value.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_UINT64);
  value.value.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
  value.value.data = &number;
  data.header.dataSetMessageValid = 1;
  data.header.fieldEncoding = CPKT_OPCUA_FIELDENCODING_VARIANT;
  data.header.dataSetMessageType = CPKT_OPCUA_DATASETMESSAGETYPE_DATAKEYFRAME;
  data.fieldCount = 1;
  data.data.keyFrameFields = &value;
  message.version = 1;
  message.publisherIdEnabled = 1;
  message.publisherId.idType = CPKT_OPCUA_PUBLISHERIDTYPE_UINT64;
  message.publisherId.id.uint64 = number;
  message.networkMessageType = CPKT_OPCUA_NETWORKMESSAGE_DATASET;
  message.payloadHeaderEnabled = 1;
  message.messageCount = 1;
  message.dataSetWriterIds[0] = 123;
  message.payload.dataSetMessages = &data;
  CHECK(cpkt_opcua_NetworkMessage_copy(&message, &copied) == 0);
  CHECK(copied.payload.dataSetMessages != &data &&
        copied.publisherId.id.uint64.high32 == number.high32);
  cpkt_opcua_NetworkMessage_clear(&copied);
  CHECK(cpkt_opcua_NetworkMessage_encodeBinary(&message, &bytes, NULL) == 0);
  CHECK(bytes.length ==
        cpkt_opcua_NetworkMessage_calcSizeBinary(&message, NULL));
  CHECK(cpkt_opcua_NetworkMessage_decodeBinaryHeaders(&bytes, &headers, NULL,
                                                      NULL, &offset) == 0);
  CHECK(offset > 0 && offset < bytes.length && headers.messageCount == 1 &&
        headers.payload.dataSetMessages == NULL);
  cpkt_opcua_NetworkMessage_clear(&headers);
  CHECK(cpkt_opcua_NetworkMessage_decodeBinary(&bytes, &readback, NULL, NULL) ==
        0);
  CHECK(readback.messageCount == 1 && readback.dataSetWriterIds[0] == 123 &&
        readback.publisherId.id.uint64.high32 == number.high32);
  CHECK(readback.payload.dataSetMessages[0].fieldCount == 1);
  CHECK(((cpkt_opcua_UInt64 *)readback.payload.dataSetMessages[0]
             .data.keyFrameFields[0]
             .value.data)
            ->high32 == number.high32);
  cpkt_opcua_NetworkMessage_clear(&readback);
  {
    cpkt_opcua_ByteString truncated = bytes;
    size_t i;
    for (i = 0; i < bytes.length; ++i) {
      truncated.length = i;
      memset(&readback, 0, sizeof(readback));
      CHECK(cpkt_opcua_NetworkMessage_decodeBinary(&truncated, &readback, NULL,
                                                   NULL) != 0);
      cpkt_opcua_NetworkMessage_clear(&readback);
    }
  }
  cpkt_opcua_ByteString_clear(&bytes);
  /* Owned publisher/promoted values depend on flags that must survive clear
   * until all selected arms and nested arrays have been released. */
  {
    cpkt_opcua_String label = cpkt_opcua_STRING("owned-message");
    cpkt_opcua_Variant promoted;
    cpkt_opcua_DataSetMessage_DeltaFrameField delta;
    cpkt_opcua_DataSetMessage delta_copy;
    memset(&promoted, 0, sizeof(promoted));
    memset(&delta, 0, sizeof(delta));
    memset(&delta_copy, 0, sizeof(delta_copy));
    promoted.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_STRING);
    promoted.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
    promoted.data = &label;
    message.publisherId.idType = CPKT_OPCUA_PUBLISHERIDTYPE_STRING;
    message.publisherId.id.string = label;
    message.promotedFieldsEnabled = 1;
    message.promotedFieldsSize = 1;
    message.promotedFields = &promoted;
    CHECK(cpkt_opcua_NetworkMessage_copy(&message, &copied) == 0);
    CHECK(copied.publisherId.id.string.data != label.data &&
          cpkt_opcua_String_equal(&copied.publisherId.id.string, &label));
    CHECK(copied.promotedFields != &promoted &&
          copied.promotedFields[0].data != &label);
    cpkt_opcua_NetworkMessage_clear(&copied);
    CHECK(copied.messageCount == 0 && copied.promotedFields == NULL);
    delta.index = 4;
    delta.value.hasValue = 1;
    delta.value.value = promoted;
    data.header.dataSetMessageType =
        CPKT_OPCUA_DATASETMESSAGETYPE_DATADELTAFRAME;
    data.data.deltaFrameFields = &delta;
    CHECK(cpkt_opcua_DataSetMessage_copy(&data, &delta_copy) == 0);
    CHECK(delta_copy.data.deltaFrameFields != &delta &&
          delta_copy.data.deltaFrameFields[0].index == 4);
    cpkt_opcua_DataSetMessage_clear(&delta_copy);
    CHECK(delta_copy.fieldCount == 0 &&
          delta_copy.data.deltaFrameFields == NULL);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
    {
      size_t position;
      int injected;
      for (position = 0; position < 256; ++position) {
        cpkt_opcua_StatusCode status;
        memset(&copied, 0, sizeof(copied));
        cpkt_types_fail_after(position);
        status = cpkt_opcua_NetworkMessage_copy(&message, &copied);
        injected = cpkt_types_fail_stop();
        if (injected) {
          CHECK(status != 0 && copied.payload.dataSetMessages == NULL &&
                copied.promotedFields == NULL);
        } else
          CHECK(status == 0);
        cpkt_opcua_NetworkMessage_clear(&copied);
        if (!injected)
          break;
      }
      CHECK(position < 256);
    }
#endif
  }
}

struct loop_test {
  cpkt_opcua_EventLoop *loop;
  cpkt_opcua_UInt64 timer;
  unsigned int timers, delayed;
};
static void timer(void *application, void *context) {
  struct loop_test *state = (struct loop_test *)context;
  CHECK(application == state);
  ++state->timers;
  cpkt_opcua_EventLoop_removeTimer(state->loop, state->timer);
  CHECK(cpkt_opcua_EventLoop_free(state->loop) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDSTATE);
}
static void delayed(void *application, void *context) {
  struct loop_test *state = (struct loop_test *)context;
  CHECK(application == state);
  ++state->delayed;
}
static void event_loop(void) {
  struct loop_test test;
  cpkt_opcua_EventLoopState state;
  cpkt_opcua_EventSourceInfo info;
  cpkt_opcua_ConnectionManager *tcp, *udp, *mqtt;
  cpkt_opcua_EventSource *source;
  cpkt_opcua_DelayedCallback *record;
  cpkt_opcua_String protocol;
  unsigned int i;
  memset(&test, 0, sizeof(test));
  test.loop = cpkt_opcua_EventLoop_new_POSIX(NULL);
  CHECK(test.loop != NULL);
  tcp =
      cpkt_opcua_ConnectionManager_new_POSIX_TCP(cpkt_opcua_STRING("tcp-test"));
  udp =
      cpkt_opcua_ConnectionManager_new_POSIX_UDP(cpkt_opcua_STRING("udp-test"));
  mqtt = cpkt_opcua_ConnectionManager_new_MQTT(cpkt_opcua_STRING("mqtt-test"));
  CHECK(tcp != NULL && udp != NULL && mqtt != NULL);
  CHECK(cpkt_opcua_ConnectionManager_protocol(tcp, &protocol) == 0 &&
        protocol.length == 3 && memcmp(protocol.data, "tcp", 3) == 0);
  source = cpkt_opcua_ConnectionManager_eventSource(tcp);
  CHECK(cpkt_opcua_EventSource_connectionManager(source) == tcp);
  CHECK(cpkt_opcua_EventLoop_registerEventSource(test.loop, source) == 0);
  CHECK(cpkt_opcua_EventSource_getInfo(source, &info) == 0 &&
        info.eventLoop == test.loop);
  CHECK(cpkt_opcua_EventSource_free(source) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDSTATE);
  CHECK(cpkt_opcua_EventLoop_registerEventSource(
            test.loop, cpkt_opcua_ConnectionManager_eventSource(udp)) == 0);
  CHECK(cpkt_opcua_EventLoop_registerEventSource(
            test.loop, cpkt_opcua_ConnectionManager_eventSource(mqtt)) == 0);
  record = cpkt_opcua_DelayedCallback_new(delayed, &test, &test);
  CHECK(record != NULL);
  CHECK(cpkt_opcua_EventLoop_start(test.loop) == 0);
  CHECK(cpkt_opcua_EventLoop_addTimer(test.loop, timer, &test, &test, 0.01,
                                      NULL, CPKT_OPCUA_TIMERPOLICY_CURRENTTIME,
                                      &test.timer) == 0);
  CHECK(cpkt_opcua_EventLoop_modifyTimer(test.loop, test.timer, 0.01, NULL,
                                         CPKT_OPCUA_TIMERPOLICY_CURRENTTIME) ==
        0);
  cpkt_opcua_EventLoop_addDelayedCallback(test.loop, record);
  cpkt_opcua_EventLoop_removeDelayedCallback(test.loop, record);
  CHECK(cpkt_opcua_EventLoop_run(test.loop, 0) == 0 && test.delayed == 0);
  cpkt_opcua_EventLoop_addDelayedCallback(test.loop, record);
  for (i = 0; i < 10 && (!test.delayed || !test.timers); ++i)
    CHECK(cpkt_opcua_EventLoop_run(test.loop, 1) == 0);
  CHECK(test.delayed == 1 && test.timers == 1);
  cpkt_opcua_DelayedCallback_delete(record);
  cpkt_opcua_EventLoop_stop(test.loop);
  for (i = 0; i < 100; ++i) {
    CHECK(cpkt_opcua_EventLoop_getState(test.loop, &state) == 0);
    if (state == CPKT_OPCUA_EVENTLOOPSTATE_STOPPED)
      break;
    CHECK(cpkt_opcua_EventLoop_run(test.loop, 0) == 0);
  }
  CHECK(state == CPKT_OPCUA_EVENTLOOPSTATE_STOPPED);
  CHECK(cpkt_opcua_EventLoop_deregisterEventSource(test.loop, source) == 0);
  CHECK(cpkt_opcua_EventSource_getInfo(source, &info) == 0 &&
        info.eventLoop == NULL);
  CHECK(cpkt_opcua_EventSource_free(source) == 0);
  CHECK(cpkt_opcua_EventLoop_free(test.loop) ==
        0); /* Owns remaining sources. */
}

struct transport_backend {
  cpkt_opcua_ConnectionManager *manager;
  cpkt_opcua_ConnectionManager_connectionCallback callback;
  void *application, *connection_context;
  void **delivered_slot;
  unsigned int starts, stops, frees, sends, deliveries;
  int fail_free;
  unsigned char bytes[4];
};
static struct transport_backend *backend(cpkt_opcua_EventSource *source) {
  struct transport_backend *state =
      (struct transport_backend *)cpkt_opcua_EventSource_context(source);
  CHECK(state != NULL);
  return state;
}
static cpkt_opcua_StatusCode source_start(cpkt_opcua_EventSource *source) {
  cpkt_opcua_EventSourceInfo info;
  ++backend(source)->starts;
  CHECK(cpkt_opcua_EventSource_getInfo(source, &info) == 0);
  info.state = CPKT_OPCUA_EVENTSOURCESTATE_STARTED;
  return cpkt_opcua_EventSource_setInfo(source, &info);
}
static void source_stop(cpkt_opcua_EventSource *source) {
  cpkt_opcua_EventSourceInfo info;
  ++backend(source)->stops;
  CHECK(cpkt_opcua_EventSource_getInfo(source, &info) == 0);
  info.state = CPKT_OPCUA_EVENTSOURCESTATE_STOPPED;
  CHECK(cpkt_opcua_EventSource_setInfo(source, &info) == 0);
}
static cpkt_opcua_StatusCode source_free(cpkt_opcua_EventSource *source) {
  struct transport_backend *state = backend(source);
  ++state->frees;
  CHECK(cpkt_opcua_EventSource_free(source) != 0);
  return state->fail_free ? CPKT_OPCUA_STATUSCODE_BADINTERNALERROR : 0;
}
static cpkt_opcua_StatusCode
source_open(cpkt_opcua_ConnectionManager *manager,
            const cpkt_opcua_KeyValueMap *params, void *application,
            void *context,
            cpkt_opcua_ConnectionManager_connectionCallback callback) {
  struct transport_backend *state =
      backend(cpkt_opcua_ConnectionManager_eventSource(manager));
  cpkt_opcua_ByteString bytes;
  CHECK(params != NULL && callback != NULL);
  state->callback = callback;
  state->application = application;
  state->connection_context = context;
  bytes.data = state->bytes;
  bytes.length = sizeof(state->bytes);
  callback(manager, 123, application, &state->connection_context,
           CPKT_OPCUA_CONNECTIONSTATE_ESTABLISHED, 0, params, bytes);
  return 0;
}
static cpkt_opcua_StatusCode source_send(cpkt_opcua_ConnectionManager *manager,
                                         size_t connection,
                                         const cpkt_opcua_KeyValueMap *params,
                                         cpkt_opcua_ByteString *bytes) {
  struct transport_backend *state =
      backend(cpkt_opcua_ConnectionManager_eventSource(manager));
  (void)params;
  CHECK(connection == 123 && bytes != NULL && bytes->length == 4);
  ++state->sends;
  cpkt_opcua_ByteString_clear(bytes);
  return CPKT_OPCUA_STATUSCODE_BADCOMMUNICATIONERROR;
}
static cpkt_opcua_StatusCode source_close(cpkt_opcua_ConnectionManager *manager,
                                          size_t connection) {
  struct transport_backend *state =
      backend(cpkt_opcua_ConnectionManager_eventSource(manager));
  cpkt_opcua_KeyValueMap params;
  cpkt_opcua_ByteString empty;
  CHECK(connection == 123);
  memset(&params, 0, sizeof(params));
  memset(&empty, 0, sizeof(empty));
  state->callback(manager, connection, state->application,
                  &state->connection_context,
                  CPKT_OPCUA_CONNECTIONSTATE_CLOSING, 0, &params, empty);
  return 0;
}
static cpkt_opcua_StatusCode source_alloc(cpkt_opcua_ConnectionManager *manager,
                                          size_t connection,
                                          cpkt_opcua_ByteString *bytes,
                                          size_t size) {
  (void)manager;
  CHECK(connection == 123);
  return cpkt_opcua_ByteString_allocBuffer(bytes, size);
}
static void source_release(cpkt_opcua_ConnectionManager *manager,
                           size_t connection, cpkt_opcua_ByteString *bytes) {
  (void)manager;
  CHECK(connection == 123);
  cpkt_opcua_ByteString_clear(bytes);
}
static void received(cpkt_opcua_ConnectionManager *manager, size_t connection,
                     void *application, void **context,
                     cpkt_opcua_ConnectionState connection_state,
                     cpkt_opcua_StatusCode conversion,
                     const cpkt_opcua_KeyValueMap *params,
                     cpkt_opcua_ByteString message) {
  struct transport_backend *state = (struct transport_backend *)application;
  CHECK(manager == state->manager && connection == 123 && conversion == 0 &&
        params != NULL);
  CHECK(context != NULL);
  if (connection_state == CPKT_OPCUA_CONNECTIONSTATE_ESTABLISHED) {
    CHECK(*context == &state->starts && message.data == state->bytes &&
          message.length == 4);
    state->delivered_slot = context;
    *context = &state->stops;
  } else {
    CHECK(connection_state == CPKT_OPCUA_CONNECTIONSTATE_CLOSING &&
          *context == &state->stops && context == state->delivered_slot);
  }
  ++state->deliveries;
}
static void custom_transport(void) {
  struct transport_backend state;
  cpkt_opcua_ConnectionManagerPlugin plugin;
  cpkt_opcua_EventLoop *loop = cpkt_opcua_EventLoop_new_POSIX(NULL);
  cpkt_opcua_EventSource *source;
  cpkt_opcua_EventSourceInfo info;
  cpkt_opcua_KeyValueMap params;
  cpkt_opcua_ByteString bytes;
  cpkt_opcua_EventLoopState loop_state;
  unsigned int i;
  CHECK(loop != NULL);
  memset(&state, 0, sizeof(state));
  memset(&plugin, 0, sizeof(plugin));
  memset(&params, 0, sizeof(params));
  memset(&bytes, 0, sizeof(bytes));
  plugin.eventSource.start = source_start;
  plugin.eventSource.stop = source_stop;
  plugin.eventSource.free = source_free;
  plugin.openConnection = source_open;
  plugin.closeConnection = source_close;
  plugin.sendWithConnection = source_send;
  plugin.allocNetworkBuffer = source_alloc;
  plugin.freeNetworkBuffer = source_release;
  state.manager = cpkt_opcua_ConnectionManager_fromPlugin(
      &plugin, &state, cpkt_opcua_STRING("custom"), cpkt_opcua_STRING("test"),
      &params);
  CHECK(state.manager != NULL);
  source = cpkt_opcua_ConnectionManager_eventSource(state.manager);
  CHECK(cpkt_opcua_EventLoop_registerEventSource(loop, source) == 0);
  CHECK(cpkt_opcua_EventLoop_start(loop) == 0 && state.starts == 1);
  CHECK(cpkt_opcua_ConnectionManager_openConnection(
            state.manager, &params, &state, &state.starts, received) == 0);
  CHECK(state.deliveries == 1 && state.connection_context != NULL);
  CHECK(cpkt_opcua_ConnectionManager_allocNetworkBuffer(state.manager, 123,
                                                        &bytes, 4) == 0);
  CHECK(cpkt_opcua_ConnectionManager_sendWithConnection(state.manager, 123,
                                                        NULL, &bytes) ==
        CPKT_OPCUA_STATUSCODE_BADCOMMUNICATIONERROR);
  CHECK(bytes.data == NULL && bytes.length == 0 && state.sends == 1);
  CHECK(cpkt_opcua_ConnectionManager_allocNetworkBuffer(state.manager, 123,
                                                        &bytes, 4) == 0);
  cpkt_opcua_ConnectionManager_freeNetworkBuffer(state.manager, 123, &bytes);
  CHECK(bytes.data == NULL && bytes.length == 0);
  CHECK(cpkt_opcua_ConnectionManager_closeConnection(state.manager, 123) == 0 &&
        state.deliveries == 2);
  cpkt_opcua_EventLoop_stop(loop);
  for (i = 0; i < 10; ++i) {
    CHECK(cpkt_opcua_EventLoop_getState(loop, &loop_state) == 0);
    if (loop_state == CPKT_OPCUA_EVENTLOOPSTATE_STOPPED)
      break;
    CHECK(cpkt_opcua_EventLoop_run(loop, 0) == 0);
  }
  CHECK(loop_state == CPKT_OPCUA_EVENTLOOPSTATE_STOPPED && state.stops == 1);
  state.fail_free = 1;
  CHECK(cpkt_opcua_EventLoop_free(loop) ==
            CPKT_OPCUA_STATUSCODE_BADINTERNALERROR &&
        state.frees == 1);
  CHECK(cpkt_opcua_EventSource_getInfo(source, &info) == 0 &&
        info.eventLoop == loop &&
        info.state == CPKT_OPCUA_EVENTSOURCESTATE_STOPPED);
  state.fail_free = 0;
  CHECK(cpkt_opcua_EventLoop_free(loop) == 0 && state.frees == 2);
}

typedef struct {
  cpkt_opcua_server *server;
  size_t calls;
  int remove;
  cpkt_opcua_UInt32 id;
  cpkt_opcua_Int64 expected;
} local_monitor;
static void local_changed(cpkt_opcua_server *server, cpkt_opcua_UInt32 item,
                          void *context, const cpkt_opcua_NodeId *node,
                          void *node_context, cpkt_opcua_UInt32 attribute,
                          cpkt_opcua_StatusCode conversion,
                          const cpkt_opcua_DataValue *value) {
  local_monitor *state = (local_monitor *)context;
  cpkt_opcua_Int64 *number;
  CHECK(server == state->server && node != NULL &&
        node->identifier.numeric == 64200 && node_context == state &&
        attribute == 13 && conversion == 0 && value && value->hasValue);
  CHECK(value->value.type == cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64));
  number = (cpkt_opcua_Int64 *)value->value.data;
  CHECK(number && number->high32 == state->expected.high32 &&
        number->low32 == state->expected.low32);
  CHECK(cpkt_opcua_server_delete_typed(server) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDSTATE);
  state->id = item;
  ++state->calls;
  if (state->remove) {
    state->remove = 0;
    CHECK(cpkt_opcua_server_deleteMonitoredItem_typed(server, item) == 0);
    /* Removal must not invalidate the current borrowed converted value. */
    CHECK(number->high32 == state->expected.high32);
  }
}
static void local_notifications(void) {
  local_monitor state;
  cpkt_opcua_NodeId id, parent, reference, type;
  cpkt_opcua_QualifiedName name;
  cpkt_opcua_VariableAttributes attributes;
  cpkt_opcua_MonitoredItemCreateRequest request;
  cpkt_opcua_MonitoredItemCreateResult response;
  cpkt_opcua_Variant value;
  unsigned int i;
  size_t count;
  memset(&state, 0, sizeof(state));
  memset(&id, 0, sizeof(id));
  memset(&parent, 0, sizeof(parent));
  memset(&reference, 0, sizeof(reference));
  memset(&type, 0, sizeof(type));
  memset(&name, 0, sizeof(name));
  memset(&request, 0, sizeof(request));
  memset(&response, 0, sizeof(response));
  memset(&value, 0, sizeof(value));
  state.expected.high32 = 0x80000000U;
  state.expected.low32 = 7;
  CHECK(cpkt_opcua_server_new(&state.server, 0) == CPKT_OPCUA_OK);
  id.namespaceIndex = 1;
  id.identifier.numeric = 64200;
  parent.identifier.numeric = 85;
  reference.identifier.numeric = 35;
  type.identifier.numeric = 63;
  name.namespaceIndex = 1;
  name.name = cpkt_opcua_STRING("local-64");
  CHECK(cpkt_opcua_VariableAttributes_default(&attributes) == 0);
  attributes.dataType.identifier.numeric = 8;
  attributes.value.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
  attributes.value.data = &state.expected;
  attributes.value.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
  attributes.accessLevel = 3;
  CHECK(cpkt_opcua_server_addVariableNode_typed(state.server, id, parent,
                                                reference, name, type,
                                                attributes, &state, NULL) == 0);
  cpkt_opcua_VariableAttributes_clear(&attributes);
  CHECK(cpkt_opcua_server_startup(state.server, NULL) == CPKT_OPCUA_OK);
  request.itemToMonitor.nodeId = id;
  request.itemToMonitor.attributeId = 13;
  request.monitoringMode = cpkt_opcua_MONITORINGMODE_REPORTING;
  request.requestedParameters.samplingInterval = 0;
  request.requestedParameters.queueSize = 1;
  request.requestedParameters.discardOldest = 1;
  CHECK(cpkt_opcua_server_createDataChangeMonitoredItem_typed(
            state.server, cpkt_opcua_TIMESTAMPSTORETURN_BOTH, request, &state,
            local_changed, &response) == 0 &&
        response.statusCode == 0);
  state.id = response.monitoredItemId;
  cpkt_opcua_MonitoredItemCreateResult_clear(&response);
  for (i = 0; i < 100 && state.calls == 0; ++i)
    CHECK(cpkt_opcua_server_iterate(state.server, 1, NULL) == CPKT_OPCUA_OK);
  CHECK(state.calls != 0);
  count = state.calls;
  state.remove = 1;
  ++state.expected.low32;
  value.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
  value.data = &state.expected;
  value.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
  CHECK(cpkt_opcua_server_writeValue_typed(state.server, id, value) == 0);
  for (i = 0; i < 100 && state.calls == count; ++i)
    CHECK(cpkt_opcua_server_iterate(state.server, 1, NULL) == CPKT_OPCUA_OK);
  CHECK(state.calls > count && !state.remove);
  CHECK(cpkt_opcua_server_deleteMonitoredItem_typed(state.server, state.id) ==
        CPKT_OPCUA_STATUSCODE_BADMONITOREDITEMIDINVALID);
  CHECK(cpkt_opcua_server_shutdown(state.server, NULL) == CPKT_OPCUA_OK);
  CHECK(cpkt_opcua_server_delete_typed(state.server) == 0);
}

static size_t pubsub_added, pubsub_removed;
static unsigned int pubsub_added_kinds, pubsub_removed_kinds;
static cpkt_opcua_server *pubsub_server;
static cpkt_opcua_StatusCode
pubsub_lifecycle(cpkt_opcua_server *server, const cpkt_opcua_NodeId id,
                 const cpkt_opcua_PubSubComponentType kind,
                 cpkt_opcua_Boolean remove) {
  CHECK(server == pubsub_server && !cpkt_opcua_NodeId_isNull(&id));
  CHECK(kind >= CPKT_OPCUA_PUBSUBCOMPONENT_CONNECTION &&
        kind <= CPKT_OPCUA_PUBSUBCOMPONENT_SUBSCRIBEDDDATASET);
  CHECK(cpkt_opcua_server_delete_typed(server) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDSTATE);
  if (remove) {
    ++pubsub_removed;
    pubsub_removed_kinds |= 1U << kind;
  } else {
    ++pubsub_added;
    pubsub_added_kinds |= 1U << kind;
  }
  return 0;
}
static void pubsub_components(void) {
  cpkt_opcua_ServerConfig *config;
  cpkt_opcua_PubSubConfiguration settings;
  cpkt_opcua_PubSubConnectionConfig connection, connection_copy;
  cpkt_opcua_NetworkAddressUrlDataType address;
  cpkt_opcua_PublishedDataSetConfig dataset, dataset_copy;
  cpkt_opcua_AddPublishedDataSetResult added;
  cpkt_opcua_DataSetFieldConfig field, field_copy;
  cpkt_opcua_DataSetFieldResult field_result;
  cpkt_opcua_WriterGroupConfig writer, writer_copy;
  cpkt_opcua_DataSetWriterConfig data_writer, data_writer_copy;
  cpkt_opcua_ReaderGroupConfig reader, reader_copy;
  cpkt_opcua_DataSetReaderConfig data_reader, data_reader_copy;
  cpkt_opcua_DataSetMetaDataType metadata;
  cpkt_opcua_NodeId cid, did, fid, wid, dwid, rid, drid, parent;
  cpkt_opcua_NodeId *children = NULL;
  cpkt_opcua_PubSubComponentType kind;
  size_t children_size = 0;
  int context;
  memset(&settings, 0, sizeof(settings));
  memset(&connection, 0, sizeof(connection));
  memset(&connection_copy, 0, sizeof(connection_copy));
  memset(&address, 0, sizeof(address));
  memset(&dataset, 0, sizeof(dataset));
  memset(&dataset_copy, 0, sizeof(dataset_copy));
  memset(&added, 0, sizeof(added));
  memset(&field, 0, sizeof(field));
  memset(&field_copy, 0, sizeof(field_copy));
  memset(&field_result, 0, sizeof(field_result));
  memset(&writer, 0, sizeof(writer));
  memset(&writer_copy, 0, sizeof(writer_copy));
  memset(&data_writer, 0, sizeof(data_writer));
  memset(&data_writer_copy, 0, sizeof(data_writer_copy));
  memset(&reader, 0, sizeof(reader));
  memset(&reader_copy, 0, sizeof(reader_copy));
  memset(&data_reader, 0, sizeof(data_reader));
  memset(&data_reader_copy, 0, sizeof(data_reader_copy));
  memset(&metadata, 0, sizeof(metadata));
  memset(&cid, 0, sizeof(cid));
  memset(&did, 0, sizeof(did));
  memset(&fid, 0, sizeof(fid));
  memset(&wid, 0, sizeof(wid));
  memset(&dwid, 0, sizeof(dwid));
  memset(&rid, 0, sizeof(rid));
  memset(&drid, 0, sizeof(drid));
  memset(&parent, 0, sizeof(parent));
  pubsub_added = pubsub_removed = 0;
  pubsub_added_kinds = pubsub_removed_kinds = 0;
  CHECK(cpkt_opcua_server_new(&pubsub_server, 0) == CPKT_OPCUA_OK);
  CHECK(cpkt_opcua_server_get_config_typed(pubsub_server, &config) == 0);
  settings.componentLifecycleCallback = pubsub_lifecycle;
  CHECK(cpkt_opcua_ServerConfig_setPubSubConfiguration(config, &settings) == 0);
  address.url = cpkt_opcua_STRING("opc.udp://224.0.0.22:4840/");
  connection.name = cpkt_opcua_STRING("typed-connection");
  connection.context = &context;
  connection.publisherId.idType = CPKT_OPCUA_PUBLISHERIDTYPE_UINT64;
  connection.publisherId.id.uint64.high32 = 0x80000000U;
  connection.publisherId.id.uint64.low32 = 7;
  connection.transportProfileUri = cpkt_opcua_STRING(
      "http://opcfoundation.org/UA-Profile/Transport/pubsub-udp-uadp");
  connection.address.type =
      cpkt_opcua_type_at(CPKT_OPCUA_TYPES_NETWORKADDRESSURLDATATYPE);
  connection.address.data = &address;
  connection.address.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
  CHECK(cpkt_opcua_server_addPubSubConnection_typed(pubsub_server, &connection,
                                                    &cid) == 0);
  CHECK(cpkt_opcua_server_getPubSubConnectionConfig_typed(
            pubsub_server, cid, &connection_copy) == 0);
  CHECK(connection_copy.context == &context &&
        connection_copy.publisherId.id.uint64.high32 == 0x80000000U &&
        connection_copy.publisherId.id.uint64.low32 == 7 &&
        connection_copy.name.data != connection.name.data);
  connection_copy.name.data[0] = 'T';
  CHECK(cpkt_opcua_server_updatePubSubConnectionConfig_typed(
            pubsub_server, cid, &connection_copy) == 0);
  cpkt_opcua_PubSubConnectionConfig_clear(&connection_copy);
  dataset.name = cpkt_opcua_STRING("typed-dataset");
  dataset.publishedDataSetType = CPKT_OPCUA_PUBSUB_DATASET_PUBLISHEDITEMS;
  CHECK(cpkt_opcua_server_addPublishedDataSet_typed(pubsub_server, &dataset,
                                                    &did, &added) == 0 &&
        added.addResult == 0);
  cpkt_opcua_AddPublishedDataSetResult_clear(&added);
  CHECK(cpkt_opcua_server_getPublishedDataSetConfig_typed(pubsub_server, did,
                                                          &dataset_copy) == 0 &&
        cpkt_opcua_String_equal(&dataset.name, &dataset_copy.name));
  cpkt_opcua_PublishedDataSetConfig_clear(&dataset_copy);
  field.dataSetFieldType = CPKT_OPCUA_PUBSUB_DATASETFIELD_VARIABLE;
  field.field.variable.fieldNameAlias = cpkt_opcua_STRING("time64");
  field.field.variable.publishParameters.publishedVariable.identifier.numeric =
      2258;
  field.field.variable.publishParameters.attributeId = 13;
  CHECK(cpkt_opcua_server_addDataSetField_typed(pubsub_server, did, &field,
                                                &fid, &field_result) == 0 &&
        field_result.result == 0);
  CHECK(cpkt_opcua_server_getDataSetFieldConfig_typed(pubsub_server, fid,
                                                      &field_copy) == 0 &&
        cpkt_opcua_String_equal(&field.field.variable.fieldNameAlias,
                                &field_copy.field.variable.fieldNameAlias));
  cpkt_opcua_DataSetFieldConfig_clear(&field_copy);
  CHECK(cpkt_opcua_server_getPublishedDataSetMetaData_typed(pubsub_server, did,
                                                            &metadata) == 0 &&
        metadata.fieldsSize == 1);
  writer.name = cpkt_opcua_STRING("typed-writer");
  writer.writerGroupId = 123;
  writer.publishingInterval = 100;
  writer.encodingMimeType = CPKT_OPCUA_PUBSUB_ENCODING_UADP;
  CHECK(cpkt_opcua_server_addWriterGroup_typed(pubsub_server, cid, &writer,
                                               &wid) == 0);
  CHECK(cpkt_opcua_server_getWriterGroupConfig_typed(pubsub_server, wid,
                                                     &writer_copy) == 0 &&
        writer_copy.writerGroupId == 123);
  writer_copy.publishingInterval = 200;
  CHECK(cpkt_opcua_server_updateWriterGroupConfig_typed(pubsub_server, wid,
                                                        &writer_copy) == 0);
  cpkt_opcua_WriterGroupConfig_clear(&writer_copy);
  data_writer.name = cpkt_opcua_STRING("typed-data-writer");
  data_writer.dataSetWriterId = 456;
  data_writer.keyFrameCount = 1;
  CHECK(cpkt_opcua_server_addDataSetWriter_typed(pubsub_server, wid, did,
                                                 &data_writer, &dwid) == 0);
  CHECK(cpkt_opcua_server_getDataSetWriterConfig_typed(
            pubsub_server, dwid, &data_writer_copy) == 0 &&
        data_writer_copy.dataSetWriterId == 456);
  data_writer_copy.keyFrameCount = 2;
  CHECK(cpkt_opcua_server_updateDataSetWriterConfig_typed(
            pubsub_server, dwid, &data_writer_copy) == 0);
  cpkt_opcua_DataSetWriterConfig_clear(&data_writer_copy);
  reader.name = cpkt_opcua_STRING("typed-reader");
  reader.encodingMimeType = CPKT_OPCUA_PUBSUB_ENCODING_UADP;
  CHECK(cpkt_opcua_server_addReaderGroup_typed(pubsub_server, cid, &reader,
                                               &rid) == 0);
  CHECK(cpkt_opcua_server_getReaderGroupConfig_typed(pubsub_server, rid,
                                                     &reader_copy) == 0);
  CHECK(cpkt_opcua_server_updateReaderGroupConfig_typed(pubsub_server, rid,
                                                        &reader_copy) == 0);
  cpkt_opcua_ReaderGroupConfig_clear(&reader_copy);
  data_reader.name = cpkt_opcua_STRING("typed-data-reader");
  data_reader.publisherId = connection.publisherId;
  data_reader.writerGroupId = 123;
  data_reader.dataSetWriterId = 456;
  data_reader.dataSetMetaData = metadata;
  data_reader.subscribedDataSetType = CPKT_OPCUA_PUBSUB_SDS_TARGET;
  CHECK(cpkt_opcua_server_addDataSetReader_typed(pubsub_server, rid,
                                                 &data_reader, &drid) == 0);
  CHECK(cpkt_opcua_server_getDataSetReaderConfig_typed(
            pubsub_server, drid, &data_reader_copy) == 0 &&
        data_reader_copy.publisherId.id.uint64.high32 == 0x80000000U &&
        data_reader_copy.dataSetMetaData.fieldsSize == 1);
  CHECK(cpkt_opcua_server_updateDataSetReaderConfig_typed(
            pubsub_server, drid, &data_reader_copy) == 0);
  cpkt_opcua_DataSetReaderConfig_clear(&data_reader_copy);
  cpkt_opcua_DataSetMetaDataType_clear(&metadata);
  CHECK(cpkt_opcua_server_getPubSubComponentType_typed(pubsub_server, wid,
                                                       &kind) == 0);
  CHECK(cpkt_opcua_server_getPubSubComponentParent_typed(pubsub_server, wid,
                                                         &parent) == 0 &&
        cpkt_opcua_NodeId_equal(&parent, &cid));
  cpkt_opcua_NodeId_clear(&parent);
  CHECK(cpkt_opcua_server_getPubSubComponentChildren_typed(
            pubsub_server, cid, &children_size, &children) == 0 &&
        children_size == 2);
  cpkt_opcua_array_delete(children, children_size,
                          cpkt_opcua_type_at(CPKT_OPCUA_TYPES_NODEID));
  CHECK(cpkt_opcua_server_removeDataSetReader_typed(pubsub_server, drid) == 0);
  CHECK(cpkt_opcua_server_removeReaderGroup_typed(pubsub_server, rid) == 0);
  CHECK(cpkt_opcua_server_removeDataSetWriter_typed(pubsub_server, dwid) == 0);
  CHECK(cpkt_opcua_server_removeWriterGroup_typed(pubsub_server, wid) == 0);
  CHECK(cpkt_opcua_server_removeDataSetField_typed(pubsub_server, fid,
                                                   &field_result) == 0 &&
        field_result.result == 0);
  CHECK(cpkt_opcua_server_removePublishedDataSet_typed(pubsub_server, did) ==
        0);
  CHECK(cpkt_opcua_server_removePubSubConnection_typed(pubsub_server, cid) ==
        0);
  /* Upstream emits lifecycle hooks for connection/groups/writer/reader,
   * but not PublishedDataSet or DataSetField construction. */
  CHECK(pubsub_added == 5 && pubsub_removed == pubsub_added &&
        pubsub_added_kinds == 0x1fU && pubsub_removed_kinds == 0x1fU);
  CHECK(cpkt_opcua_server_delete_typed(pubsub_server) == 0);
  pubsub_server = NULL;
  cpkt_opcua_NodeId_clear(&cid);
  cpkt_opcua_NodeId_clear(&did);
  cpkt_opcua_NodeId_clear(&fid);
  cpkt_opcua_NodeId_clear(&wid);
  cpkt_opcua_NodeId_clear(&dwid);
  cpkt_opcua_NodeId_clear(&rid);
  cpkt_opcua_NodeId_clear(&drid);
}
void cpkt_types_test_transport(void) {
  ranges();
  codecs();
  messages();
  event_loop();
  custom_transport();
  local_notifications();
  pubsub_components();
}
