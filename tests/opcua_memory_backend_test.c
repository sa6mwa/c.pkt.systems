#include "opcua_types_peer.h"
#include <open62541/plugin/historydata/history_data_backend_memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void memory_check(int condition, const char *expression, int line) {
  if (!condition) {
    fprintf(stderr, "native memory backend line %d: %s\n", line, expression);
    abort();
  }
}
#define CHECK(e) memory_check(!!(e), #e, __LINE__)
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
void cpkt_types_fail_after(size_t count);
int cpkt_types_fail_stop(void);
#endif

static UA_HistoryDataBackend backend_new(int circular) {
  UA_HistoryDataBackend backend =
      circular ? UA_HistoryDataBackend_Memory_Circular(1, 1)
               : UA_HistoryDataBackend_Memory(1, 1);
  CHECK(backend.context && backend.deleteMembers && backend.getDataValue);
  return backend;
}
static UA_DataValue sample(UA_String *text, UA_DateTime timestamp) {
  UA_DataValue value;
  UA_DataValue_init(&value);
  UA_Variant_setScalar(&value.value, text, &UA_TYPES[UA_TYPES_STRING]);
  value.value.storageType = UA_VARIANT_DATA_NODELETE;
  value.hasValue = true;
  value.hasSourceTimestamp = true;
  value.sourceTimestamp = timestamp;
  return value;
}
static void stored(UA_HistoryDataBackend *backend, const UA_NodeId *node,
                   size_t index, const char *text, UA_DateTime timestamp) {
  const UA_DataValue *value =
      backend->getDataValue(NULL, backend->context, NULL, NULL, node, index);
  UA_String expected = UA_STRING((char *)text);
  CHECK(value && value->hasValue && value->hasSourceTimestamp);
  CHECK(value->sourceTimestamp == timestamp && value->hasServerTimestamp);
  CHECK(UA_Variant_hasScalarType(&value->value, &UA_TYPES[UA_TYPES_STRING]));
  CHECK(UA_String_equal(value->value.data, &expected));
}
static UA_StatusCode write_value(UA_HistoryDataBackend *backend,
                                 const UA_NodeId *node,
                                 const UA_DataValue *value, int action) {
  switch (action) {
  case 0:
    return backend->serverSetHistoryData(NULL, backend->context, NULL, NULL,
                                         node, true, value);
  case 1:
    return backend->insertDataValue(NULL, backend->context, NULL, NULL, node,
                                    value);
  case 2:
    return backend->replaceDataValue(NULL, backend->context, NULL, NULL, node,
                                     value);
  default:
    return backend->updateDataValue(NULL, backend->context, NULL, NULL, node,
                                    value);
  }
}
static void writes(void) {
  UA_NodeId node = UA_NODEID_STRING(65535, "existing node");
  UA_String old_text = UA_STRING("original owned value"),
            new_text = UA_STRING("replacement owned value");
  UA_DataValue old_value = sample(&old_text, 100);
  for (int circular = 0; circular < 2; ++circular)
    for (int action = 0; action < (circular ? 1 : 5); ++action)
      for (size_t position = 0; position < 10; ++position) {
        UA_HistoryDataBackend backend = backend_new(circular);
        CHECK(!write_value(&backend, &node, &old_value, 0));
        UA_DataValue next =
            sample(&new_text, action == 2 || action == 3 ? 100 : 200);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
        cpkt_types_fail_after(position);
#else
        (void)position;
#endif
        UA_StatusCode status = write_value(&backend, &node, &next, action);
        int injected = 0;
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
        injected = cpkt_types_fail_stop();
#endif
        size_t count = backend.getEnd(NULL, backend.context, NULL, NULL, &node);
        if (injected) {
          CHECK(status == UA_STATUSCODE_BADOUTOFMEMORY && count == 1);
          stored(&backend, &node, 0, "original owned value", 100);
          CHECK(!UA_StatusCode_isBad(
              write_value(&backend, &node, &next, action)));
        } else {
          CHECK(!UA_StatusCode_isBad(status));
          CHECK(count == (circular || action == 2 || action == 3 ? 1 : 2));
        }
        stored(&backend, &node, circular || action == 2 || action == 3 ? 0 : 1,
               "replacement owned value", next.sourceTimestamp);
        UA_HistoryDataBackend_Memory_clear(&backend);
        CHECK(!backend.context && !backend.deleteMembers);
      }
}
static void nodes(void) {
  UA_NodeId first = UA_NODEID_STRING(1, "first node"),
            second = UA_NODEID_STRING(65535, "new node requiring a copy");
  UA_String text = UA_STRING("value with allocated contents");
  UA_DataValue value = sample(&text, 100);
  for (size_t position = 0; position < 12; ++position) {
    UA_HistoryDataBackend backend = backend_new(0);
    CHECK(!write_value(&backend, &first, &value, 0));
    const UA_DataValue *borrowed =
        backend.getDataValue(NULL, backend.context, NULL, NULL, &first, 0);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
    cpkt_types_fail_after(position);
#else
    (void)position;
#endif
    UA_StatusCode status = write_value(&backend, &second, &value, 0);
    int injected = 0;
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
    injected = cpkt_types_fail_stop();
#endif
    CHECK(backend.getDataValue(NULL, backend.context, NULL, NULL, &first, 0) ==
          borrowed);
    stored(&backend, &first, 0, "value with allocated contents", 100);
    if (injected) {
      CHECK(status == UA_STATUSCODE_BADOUTOFMEMORY);
      CHECK(!write_value(&backend, &second, &value, 0));
    } else
      CHECK(!status);
    stored(&backend, &second, 0, "value with allocated contents", 100);
    backend.deleteMembers(&backend);
  }
}
static void copies(void) {
  UA_NodeId node = UA_NODEID_NUMERIC(1, 100);
  UA_String text = UA_STRING("copy this counted string");
  UA_NumericRange range = {0, NULL};
  for (int circular = 0; circular < 2; ++circular)
    for (size_t position = 0; position < 8; ++position) {
      UA_HistoryDataBackend backend =
          circular ? UA_HistoryDataBackend_Memory_Circular(1, 2)
                   : UA_HistoryDataBackend_Memory(1, 2);
      CHECK(backend.context);
      for (UA_DateTime timestamp = 100; timestamp <= 200; timestamp += 100) {
        UA_DataValue value = sample(&text, timestamp);
        CHECK(!write_value(&backend, &node, &value, 0));
      }
      UA_ByteString input = UA_BYTESTRING_NULL, output = UA_BYTESTRING_NULL;
      UA_DataValue values[2];
      UA_DataValue_init(&values[0]);
      UA_DataValue_init(&values[1]);
      size_t provided = 0;
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
      cpkt_types_fail_after(position);
#else
      (void)position;
#endif
      UA_StatusCode status = backend.copyDataValues(
          NULL, backend.context, NULL, NULL, &node, 0, 1, false, 1, range,
          false, &input, &output, &provided, values);
      int injected = 0;
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
      injected = cpkt_types_fail_stop();
#endif
      CHECK(status == (injected ? UA_STATUSCODE_BADOUTOFMEMORY : 0));
      CHECK(provided <= 1);
      if (!status) {
        CHECK(provided == 1 && output.length == sizeof(size_t));
        /* Counted continuation bytes need not be naturally aligned. */
        union {
          size_t alignment;
          unsigned char bytes[sizeof(size_t) + 1];
        } unaligned;
        memcpy(unaligned.bytes + 1, output.data, sizeof(size_t));
        input.length = sizeof(size_t);
        input.data = unaligned.bytes + 1;
        UA_ByteString_clear(&output);
        CHECK(!backend.copyDataValues(NULL, backend.context, NULL, NULL, &node,
                                      0, 1, false, 1, range, false, &input,
                                      &output, &provided, &values[1]));
        CHECK(provided == 1);
        CHECK(((UA_String *)values[1].value.data)->length == text.length);
      }
      UA_DataValue_clear(&values[0]);
      UA_DataValue_clear(&values[1]);
      UA_ByteString_clear(&output);
      stored(&backend, &node, 0, "copy this counted string", 100);
      stored(&backend, &node, 1, "copy this counted string", 200);
      backend.deleteMembers(&backend);
    }
}
static void constructors(void) {
  for (int circular = 0; circular < 2; ++circular)
    for (size_t position = 0; position < 4; ++position) {
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
      cpkt_types_fail_after(position);
#else
      (void)position;
#endif
      UA_HistoryDataBackend backend =
          circular ? UA_HistoryDataBackend_Memory_Circular(0, 0)
                   : UA_HistoryDataBackend_Memory(0, 0);
      int injected = 0;
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
      injected = cpkt_types_fail_stop();
#endif
      CHECK(injected ? !backend.context : !!backend.context);
      if (backend.context)
        backend.deleteMembers(&backend);
    }
  UA_HistoryDataBackend invalid = UA_HistoryDataBackend_Memory((size_t)-1, 1);
  CHECK(!invalid.context);
  invalid = UA_HistoryDataBackend_Memory(1, (size_t)-1);
  CHECK(!invalid.context);
  UA_HistoryDataBackend_Memory_clear(&invalid);
  UA_HistoryDataBackend_Memory_clear(NULL);
}
static void queries(void) {
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  UA_NodeId first = UA_NODEID_STRING(1, "retained query node"),
            second = UA_NODEID_STRING(1, "unknown query node");
  UA_String text = UA_STRING("retained during query failure");
  UA_DataValue value = sample(&text, 100);
  for (int action = 0; action < 7; ++action)
    for (size_t position = 0; position < 3; ++position) {
      UA_HistoryDataBackend backend = backend_new(0);
      CHECK(!write_value(&backend, &first, &value, 0));
      cpkt_types_fail_after(position);
      size_t number = 0;
      const UA_DataValue *result = NULL;
      UA_Boolean supported = false;
      switch (action) {
      case 0:
        number = backend.getEnd(NULL, backend.context, NULL, NULL, &second);
        break;
      case 1:
        number = backend.lastIndex(NULL, backend.context, NULL, NULL, &second);
        break;
      case 2:
        number = backend.resultSize(NULL, backend.context, NULL, NULL, &second,
                                    0, 0);
        break;
      case 3:
        number = backend.getDateTimeMatch(NULL, backend.context, NULL, NULL,
                                          &second, 100, MATCH_EQUAL);
        break;
      case 4:
        result =
            backend.getDataValue(NULL, backend.context, NULL, NULL, &second, 0);
        break;
      case 5:
        supported = backend.timestampsToReturnSupported(
            NULL, backend.context, NULL, NULL, &second,
            UA_TIMESTAMPSTORETURN_BOTH);
        break;
      case 6:
        number = backend.removeDataValue(NULL, backend.context, NULL, NULL,
                                         &second, 100, 100);
        break;
      }
      CHECK(cpkt_types_fail_stop());
      CHECK(action == 6 ? number == UA_STATUSCODE_BADOUTOFMEMORY : number == 0);
      CHECK(!result && !supported);
      stored(&backend, &first, 0, "retained during query failure", 100);
      CHECK(!write_value(&backend, &second, &value, 0));
      stored(&backend, &second, 0, "retained during query failure", 100);
      backend.deleteMembers(&backend);
    }
#endif
}
static void circular_pages(void) {
  UA_NodeId node = UA_NODEID_NUMERIC(1, 200);
  UA_String text = UA_STRING("circular page value");
  UA_NumericRange range = {0, NULL};
  for (size_t position = 0; position < 10; ++position) {
    UA_HistoryDataBackend backend = UA_HistoryDataBackend_Memory_Circular(1, 2);
    CHECK(backend.context);
    for (UA_DateTime timestamp = 100; timestamp <= 200; timestamp += 100) {
      UA_DataValue value = sample(&text, timestamp);
      CHECK(!write_value(&backend, &node, &value, 0));
    }
    UA_HistoryData data;
    UA_HistoryData_init(&data);
    UA_ByteString input = UA_BYTESTRING_NULL, output = UA_BYTESTRING_NULL;
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
    cpkt_types_fail_after(position);
#else
    (void)position;
#endif
    UA_StatusCode status = backend.getHistoryData(
        NULL, NULL, NULL, &backend, 100, 200, &node, 1, 1, false,
        UA_TIMESTAMPSTORETURN_BOTH, range, false, &input, &output, &data);
    int injected = 0;
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
    injected = cpkt_types_fail_stop();
#endif
    CHECK(status == (injected ? UA_STATUSCODE_BADOUTOFMEMORY : 0));
    if (!status)
      CHECK(data.dataValuesSize == 1 && output.length > sizeof(size_t));
    UA_HistoryData_clear(&data);
    UA_ByteString_clear(&output);
    size_t invalid_skip = (size_t)-1;
    input.length = sizeof(invalid_skip);
    input.data = (UA_Byte *)&invalid_skip;
    CHECK(backend.getHistoryData(NULL, NULL, NULL, &backend, 100, 200, &node, 1,
                                 1, false, UA_TIMESTAMPSTORETURN_BOTH, range,
                                 false, &input, &output, &data) ==
          UA_STATUSCODE_BADCONTINUATIONPOINTINVALID);
    CHECK(!data.dataValues && !data.dataValuesSize && !output.data);
    stored(&backend, &node, 0, "circular page value", 100);
    stored(&backend, &node, 1, "circular page value", 200);
    backend.deleteMembers(&backend);
  }
}
void cpkt_types_test_memory_backend(void) {
  constructors();
  writes();
  nodes();
  copies();
  queries();
  circular_pages();
}

/* The facade handle must be the native public object itself, not a snapshot. */
int cpkt_types_peer_stock_borrow(const void *stored, const unsigned char *text,
                                 size_t length, unsigned int timestamp) {
  const UA_DataValue *value = (const UA_DataValue *)stored;
  const UA_String *actual;
  if (!value || !value->hasValue || !value->hasSourceTimestamp ||
      value->sourceTimestamp != ((UA_DateTime)1 << 32) + timestamp ||
      !UA_Variant_hasScalarType(&value->value, &UA_TYPES[UA_TYPES_STRING]))
    return 0;
  actual = (const UA_String *)value->value.data;
  return actual->length == length && !memcmp(actual->data, text, length);
}

/* Compare the upstream endpoint behavior, rather than infer inclusive removal.
 */
size_t cpkt_types_peer_stock_remove_end(void) {
  UA_HistoryDataBackend backend = UA_HistoryDataBackend_Memory(1, 1);
  UA_NodeId node = UA_NODEID_NUMERIC(1, 1);
  UA_String text = UA_STRING("parity");
  for (unsigned int stamp = 100; stamp <= 500; stamp += 100) {
    UA_DataValue value = sample(&text, ((UA_DateTime)1 << 32) + stamp);
    CHECK(!backend.insertDataValue(NULL, backend.context, NULL, NULL, &node,
                                   &value));
  }
  CHECK(!backend.removeDataValue(NULL, backend.context, NULL, NULL, &node,
                                 ((UA_DateTime)1 << 32) + 100,
                                 ((UA_DateTime)1 << 32) + 200));
  size_t count = backend.getEnd(NULL, backend.context, NULL, NULL, &node);
  UA_HistoryDataBackend_Memory_clear(&backend);
  return count;
}

/* Read factory-backed values through the installed default HistoryDatabase. */
unsigned int cpkt_types_peer_stock_history(void *native_server) {
  UA_Server *server = (UA_Server *)native_server;
  UA_HistoryDatabase *database = &UA_Server_getConfig(server)->historyDatabase;
  UA_NodeId node = UA_NODEID_NUMERIC(1, 7775);
  UA_VariableAttributes attributes = UA_VariableAttributes_default;
  UA_String text = UA_STRING("stock history");
  attributes.historizing = true;
  attributes.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE |
                           UA_ACCESSLEVELMASK_HISTORYREAD |
                           UA_ACCESSLEVELMASK_HISTORYWRITE;
  attributes.dataType = UA_TYPES[UA_TYPES_STRING].typeId;
  UA_Variant_setScalar(&attributes.value, &text, &UA_TYPES[UA_TYPES_STRING]);
  UA_StatusCode status = UA_Server_addVariableNode(
      server, node, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(1, "stock history"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attributes, NULL,
      NULL);
  if (status)
    return status;
  UA_RequestHeader header;
  UA_RequestHeader_init(&header);
  UA_HistoryReadValueId read;
  UA_HistoryReadValueId_init(&read);
  read.nodeId = node;
  UA_ReadRawModifiedDetails details;
  UA_ReadRawModifiedDetails_init(&details);
  details.startTime = ((UA_DateTime)1 << 32) + 100;
  /* Native growable reads use an exclusive end when returnBounds is false. */
  details.endTime = ((UA_DateTime)1 << 32) + 301;
  details.numValuesPerNode = 3;
  UA_HistoryReadResponse response;
  UA_HistoryReadResponse_init(&response);
  response.results = (UA_HistoryReadResult *)UA_Array_new(
      1, &UA_TYPES[UA_TYPES_HISTORYREADRESULT]);
  CHECK(response.results);
  response.resultsSize = 1;
  UA_HistoryData data, *pointer = &data;
  UA_HistoryData_init(&data);
  response.results[0].historyData.encoding =
      UA_EXTENSIONOBJECT_DECODED_NODELETE;
  response.results[0].historyData.content.decoded.type =
      &UA_TYPES[UA_TYPES_HISTORYDATA];
  response.results[0].historyData.content.decoded.data = &data;
  database->readRaw(server, database->context, NULL, NULL, &header, &details,
                    UA_TIMESTAMPSTORETURN_BOTH, false, 1, &read, &response,
                    &pointer);
  status = response.results[0].statusCode;
  CHECK(!status && data.dataValuesSize == 3);
  CHECK(response.results[0].historyData.content.decoded.data == pointer);
  CHECK(response.results[0].historyData.encoding ==
        UA_EXTENSIONOBJECT_DECODED_NODELETE);
  unsigned char payload[] = {'a', 0, 'b', 'c'};
  for (size_t i = 0; i < data.dataValuesSize; ++i)
    CHECK(cpkt_types_peer_stock_borrow(&data.dataValues[i], payload,
                                       sizeof(payload),
                                       (unsigned int)(100 + i * 100)));
  UA_HistoryData_clear(&data);
  UA_HistoryReadResponse_clear(&response);
  return status;
}

#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
#include <open62541/plugin/historydata/history_database_default.h>
static UA_HistoryDataGathering captured_gathering;
UA_HistoryDatabase __real_UA_HistoryDatabase_default(UA_HistoryDataGathering);
UA_HistoryDatabase
__wrap_UA_HistoryDatabase_default(UA_HistoryDataGathering gathering) {
  UA_HistoryDatabase database = __real_UA_HistoryDatabase_default(gathering);
  memset(&captured_gathering, 0, sizeof(captured_gathering));
  if (database.context)
    captured_gathering = gathering;
  return database;
}
#endif
/* Capture the public constructor argument, never the private database context.
 */
unsigned int cpkt_types_peer_custom_gathering(void *native_server,
                                              const void *owned_setting) {
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  UA_Server *server = (UA_Server *)native_server;
  const UA_HistorizingNodeIdSettings *setting = owned_setting;
  UA_NodeId first = UA_NODEID_NUMERIC(1, 7775),
            second = UA_NODEID_NUMERIC(1, 7776);
  UA_String text = UA_STRING("native callback");
  UA_DataValue value = sample(&text, ((UA_DateTime)1 << 32) + 200);
  CHECK(captured_gathering.context);
  cpkt_types_fail_after(0);
  const UA_HistorizingNodeIdSettings *actual =
      captured_gathering.getHistorizingSetting(
          server, captured_gathering.context, &first);
  CHECK(!cpkt_types_fail_stop() && actual == setting);
  CHECK(!captured_gathering.registerNodeId(server, captured_gathering.context,
                                           &second, *setting));
  CHECK(!captured_gathering.startPoll(server, captured_gathering.context,
                                      &second));
  CHECK(!captured_gathering.stopPoll(server, captured_gathering.context,
                                     &second));
  CHECK(captured_gathering.updateNodeIdSetting(
      server, captured_gathering.context, &second, *setting));
  cpkt_types_fail_after(0);
  const UA_HistorizingNodeIdSettings *other =
      captured_gathering.getHistorizingSetting(
          server, captured_gathering.context, &second);
  CHECK(!cpkt_types_fail_stop() && other && other != actual);
  CHECK(actual->maxHistoryDataResponseSize == 3 &&
        other->maxHistoryDataResponseSize == 3);
  captured_gathering.setValue(server, captured_gathering.context, NULL, NULL,
                              &first, true, &value);
  CHECK(!actual->historizingBackend.serverSetHistoryData(
      NULL, actual->historizingBackend.context, NULL, NULL, &second, true,
      &value));
#else
  (void)native_server;
  (void)owned_setting;
#endif
  return 0;
}
