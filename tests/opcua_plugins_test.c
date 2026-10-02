#include "opcua_callbacks_test.h"
#include "opcua_types_peer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void check(int ok, const char *expression, int line) {
  if (!ok) {
    fprintf(stderr, "plugin line %d: %s\n", line, expression);
    abort();
  }
}
#define CHECK(expression) check(!!(expression), #expression, __LINE__)
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
void cpkt_types_fail_after(size_t count);
int cpkt_types_fail_stop(void);
#endif
typedef struct {
  cpkt_opcua_server *server;
  unsigned int calls, closed, cleared;
  int allow_missing_session;
} plugin_state;
static plugin_state *state_of(cpkt_opcua_server *server,
                              cpkt_opcua_AccessControl *ac) {
  plugin_state *state = (plugin_state *)ac->context;
  CHECK(server == state->server && ac->userTokenPoliciesSize == 1);
  CHECK(ac->userTokenPolicies[0].policyId.length == 5 &&
        memcmp(ac->userTokenPolicies[0].policyId.data, "typed", 5) == 0);
  ++state->calls;
  return state;
}
static cpkt_opcua_StatusCode
activate(cpkt_opcua_server *server, cpkt_opcua_AccessControl *ac,
         const cpkt_opcua_EndpointDescription *endpoint,
         const cpkt_opcua_ByteString *certificate,
         const cpkt_opcua_NodeId *session,
         const cpkt_opcua_ExtensionObject *identity, void **context) {
  plugin_state *state = state_of(server, ac);
  CHECK(endpoint && endpoint->server.applicationUri.length == 11);
  CHECK(certificate && certificate->length == 4);
  CHECK(session && session->namespaceIndex == 2 &&
        session->identifier.numeric == 17);
  CHECK(identity &&
        identity->content.decoded.type ==
            cpkt_opcua_type_at(CPKT_OPCUA_TYPES_ANONYMOUSIDENTITYTOKEN));
  CHECK(((const cpkt_opcua_AnonymousIdentityToken *)
             identity->content.decoded.data)
            ->policyId.length == 5);
  *context = state;
  return 0;
}
static void close_session(cpkt_opcua_server *server,
                          cpkt_opcua_AccessControl *ac,
                          const cpkt_opcua_NodeId *session, void *context) {
  plugin_state *state = state_of(server, ac);
  CHECK(context == state);
  CHECK((session && session->identifier.numeric == 17) ||
        (!session && state->allow_missing_session));
  ++state->closed;
}
static cpkt_opcua_UInt32 rights(cpkt_opcua_server *server,
                                cpkt_opcua_AccessControl *ac,
                                const cpkt_opcua_NodeId *session, void *context,
                                const cpkt_opcua_NodeId *node,
                                void *node_context) {
  plugin_state *state = state_of(server, ac);
  CHECK(session && context == state && !node_context &&
        node->namespaceIndex == 3);
  CHECK(node->identifier.string.length == 4 &&
        memcmp(node->identifier.string.data, "wide", 4) == 0);
  return 0xffffffffU;
}
static cpkt_opcua_Byte
access_level(cpkt_opcua_server *server, cpkt_opcua_AccessControl *ac,
             const cpkt_opcua_NodeId *session, void *context,
             const cpkt_opcua_NodeId *node, void *node_context) {
  (void)rights(server, ac, session, context, node, node_context);
  return 3;
}
static cpkt_opcua_Boolean
executable(cpkt_opcua_server *server, cpkt_opcua_AccessControl *ac,
           const cpkt_opcua_NodeId *session, void *context,
           const cpkt_opcua_NodeId *node, void *node_context) {
  (void)rights(server, ac, session, context, node, node_context);
  return 254;
}
static cpkt_opcua_Boolean
executable_object(cpkt_opcua_server *server, cpkt_opcua_AccessControl *ac,
                  const cpkt_opcua_NodeId *session, void *context,
                  const cpkt_opcua_NodeId *node, void *node_context,
                  const cpkt_opcua_NodeId *object, void *object_context) {
  CHECK(object && object->namespaceIndex == 3 && !object_context);
  return executable(server, ac, session, context, node, node_context);
}
static cpkt_opcua_Boolean browse(cpkt_opcua_server *server,
                                 cpkt_opcua_AccessControl *ac,
                                 const cpkt_opcua_NodeId *session,
                                 void *context, const cpkt_opcua_NodeId *node,
                                 void *node_context) {
  plugin_state *state = state_of(server, ac);
  CHECK(!session && context == state && node && !node_context);
  return 1;
}
static cpkt_opcua_Boolean
transfer(cpkt_opcua_server *server, cpkt_opcua_AccessControl *ac,
         const cpkt_opcua_NodeId *old_session, void *old_context,
         const cpkt_opcua_NodeId *new_session, void *new_context) {
  plugin_state *state = state_of(server, ac);
  CHECK(old_session && !new_session && old_context == state &&
        new_context == state);
  return 1;
}
#define ITEM_CALLBACK(name, type, expression)                                  \
  static cpkt_opcua_Boolean name(                                              \
      cpkt_opcua_server *server, cpkt_opcua_AccessControl *ac,                 \
      const cpkt_opcua_NodeId *session, void *context,                         \
      const cpkt_opcua_##type *item) {                                         \
    plugin_state *state = state_of(server, ac);                                \
    CHECK(session && context == state && item && (expression));                \
    return 1;                                                                  \
  }
ITEM_CALLBACK(add_node, AddNodesItem, item->browseName.name.length == 5)
ITEM_CALLBACK(add_reference, AddReferencesItem,
              item->sourceNodeId.namespaceIndex == 3)
ITEM_CALLBACK(delete_node, DeleteNodesItem, item->nodeId.namespaceIndex == 3)
ITEM_CALLBACK(delete_reference, DeleteReferencesItem,
              item->sourceNodeId.namespaceIndex == 3)
static cpkt_opcua_Boolean update_history(cpkt_opcua_server *server,
                                         cpkt_opcua_AccessControl *ac,
                                         const cpkt_opcua_NodeId *session,
                                         void *context,
                                         const cpkt_opcua_NodeId *node,
                                         cpkt_opcua_PerformUpdateType operation,
                                         const cpkt_opcua_DataValue *value) {
  cpkt_opcua_Int64 *number;
  plugin_state *state = state_of(server, ac);
  CHECK(session && context == state && node &&
        operation == cpkt_opcua_PERFORMUPDATETYPE_INSERT);
  CHECK(value && value->hasValue &&
        value->value.type == cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64));
  number = (cpkt_opcua_Int64 *)value->value.data;
  CHECK(number && number->high32 == 0x80000000U && number->low32 == 0);
  return 1;
}
static cpkt_opcua_Boolean
delete_history(cpkt_opcua_server *server, cpkt_opcua_AccessControl *ac,
               const cpkt_opcua_NodeId *session, void *context,
               const cpkt_opcua_NodeId *node, cpkt_opcua_DateTime start,
               cpkt_opcua_DateTime end, cpkt_opcua_Boolean modified) {
  plugin_state *state = state_of(server, ac);
  CHECK(session && context == state && node && modified);
  CHECK(start.high32 == 0x80000000U && start.low32 == 0 &&
        end.high32 == 0x7fffffffU && end.low32 == 0xffffffffU);
  return 1;
}
static void cleared(cpkt_opcua_AccessControl *ac) {
  plugin_state *state = (plugin_state *)ac->context;
  ++state->cleared;
}
static cpkt_opcua_status run_native(void *native, void *user) {
  (void)user;
  return cpkt_types_peer_access_control(native);
}
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
static cpkt_opcua_status failed_close(void *native, void *user) {
  cpkt_types_fail_after(0);
  CHECK(cpkt_types_peer_close_session_failure(native, user) == 0);
  CHECK(cpkt_types_fail_stop());
  return 0;
}
#endif
static void arrays(void) {
  cpkt_opcua_String *strings;
  const cpkt_opcua_Type *type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_STRING);
  void *copy;
  size_t i;
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  int injected;
  cpkt_opcua_StatusCode status;
#endif
  strings = (cpkt_opcua_String *)cpkt_opcua_array_new(2, type);
  CHECK(strings && strings[0].data == NULL && strings[1].data == NULL);
  CHECK(cpkt_opcua_array_copy(strings, 2, &copy, type) == 0 && copy != strings);
  cpkt_opcua_array_delete(copy, 2, type);
  cpkt_opcua_array_delete(strings, 2, type);
  CHECK(cpkt_opcua_array_new(0, type) == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL);
  CHECK(cpkt_opcua_array_copy(CPKT_OPCUA_EMPTY_ARRAY_SENTINEL, 0, &copy,
                              type) == 0 &&
        copy == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL);
  cpkt_opcua_array_delete(copy, 0, type);
  CHECK(cpkt_opcua_array_copy(NULL, 0, &copy, type) == 0 && copy == NULL);
  CHECK(cpkt_opcua_array_copy(NULL, 1, &copy, type) != 0 && copy == NULL);
  CHECK(cpkt_opcua_array_new((size_t)-1, type) == NULL);
  CHECK(cpkt_opcua_array_new(1, NULL) == NULL);
  CHECK(cpkt_opcua_array_copy(CPKT_OPCUA_EMPTY_ARRAY_SENTINEL, 1, &copy,
                              type) != 0 &&
        copy == NULL);
  strings = (cpkt_opcua_String *)cpkt_opcua_array_new(2, type);
  CHECK(strings);
  for (i = 0; i < 2; ++i) {
    cpkt_opcua_String borrowed;
    borrowed.data = (cpkt_opcua_Byte *)"nested";
    borrowed.length = 6;
    CHECK(cpkt_opcua_String_copy(&borrowed, &strings[i]) == 0);
  }
  CHECK(cpkt_opcua_array_copy(strings, 2, &copy, type) == 0);
  CHECK(((cpkt_opcua_String *)copy)[0].data != strings[0].data);
  strings[0].data[0] = '!';
  CHECK(memcmp(((cpkt_opcua_String *)copy)[0].data, "nested", 6) == 0);
  cpkt_opcua_array_delete(copy, 2, type);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  for (i = 0; i < 100; ++i) {
    cpkt_types_fail_after(i);
    status = cpkt_opcua_array_copy(strings, 2, &copy, type);
    injected = cpkt_types_fail_stop();
    if (injected)
      CHECK(status != 0 && copy == NULL);
    else {
      CHECK(status == 0);
      cpkt_opcua_array_delete(copy, 2, type);
      break;
    }
  }
  CHECK(i > 0 && i < 100);
#endif
  cpkt_opcua_array_delete(strings, 2, type);
}
typedef struct {
  cpkt_opcua_server *server;
  unsigned int calls, cleared;
  int fail_result;
} history_state;
static void history_called(cpkt_opcua_server *server, void *context) {
  history_state *state = (history_state *)context;
  CHECK(server == state->server);
  ++state->calls;
}
static void fill_value(cpkt_opcua_DataValue *value) {
  cpkt_opcua_Int64 *number = cpkt_opcua_Int64_new();
  CHECK(number);
  number->high32 = 0x80000000U;
  number->low32 = 0;
  value->hasValue = 1;
  value->value.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
  value->value.data = number;
}
static void history_set(cpkt_opcua_server *server, void *context,
                        const cpkt_opcua_NodeId *session, void *session_context,
                        const cpkt_opcua_NodeId *node,
                        cpkt_opcua_Boolean historizing,
                        const cpkt_opcua_DataValue *value) {
  history_called(server, context);
  CHECK(!session && !session_context && node->identifier.numeric == 6001 &&
        historizing);
  CHECK(value->hasValue &&
        value->value.type == cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64));
  CHECK(((const cpkt_opcua_Int64 *)value->value.data)->high32 == 0x80000000U);
}
static void history_event(cpkt_opcua_server *server, void *context,
                          const cpkt_opcua_NodeId *origin,
                          const cpkt_opcua_NodeId *emitter,
                          const cpkt_opcua_EventFilter *filter,
                          cpkt_opcua_EventFieldList *fields) {
  history_called(server, context);
  CHECK(origin->identifier.numeric == 6001 &&
        emitter->identifier.numeric == 6001 && !filter);
  CHECK(fields->eventFieldsSize == 0);
  fields->eventFields = (cpkt_opcua_Variant *)cpkt_opcua_array_new(
      1, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_VARIANT));
  CHECK(fields->eventFields);
  fields->eventFieldsSize = 1;
  fields->eventFields[0].data = cpkt_opcua_Int64_new();
  CHECK(fields->eventFields[0].data);
  fields->eventFields[0].type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
  ((cpkt_opcua_Int64 *)fields->eventFields[0].data)->high32 = 0x80000000U;
}
static void history_read_check(
    cpkt_opcua_server *server, void *context, const cpkt_opcua_NodeId *session,
    void *session_context, const cpkt_opcua_RequestHeader *header,
    const void *details, cpkt_opcua_TimestampsToReturn timestamps,
    cpkt_opcua_Boolean release, size_t count,
    const cpkt_opcua_HistoryReadValueId *nodes,
    cpkt_opcua_HistoryReadResponse *response, const void *payload) {
  history_called(server, context);
  CHECK(!session && !session_context && header && details && !release);
  CHECK(timestamps == cpkt_opcua_TIMESTAMPSTORETURN_BOTH && count == 1 &&
        nodes[0].nodeId.identifier.numeric == 6001);
  CHECK(response->resultsSize == 1 &&
        response->results[0].historyData.content.decoded.data == payload);
}
#define HISTORY_READER(name, details_type, payload_type, value_type, field,    \
                       field_size, initialize)                                 \
  static void name(cpkt_opcua_server *server, void *context,                   \
                   const cpkt_opcua_NodeId *session, void *session_context,    \
                   const cpkt_opcua_RequestHeader *header,                     \
                   const cpkt_opcua_##details_type *details,                   \
                   cpkt_opcua_TimestampsToReturn timestamps,                   \
                   cpkt_opcua_Boolean release, size_t count,                   \
                   const cpkt_opcua_HistoryReadValueId *nodes,                 \
                   cpkt_opcua_HistoryReadResponse *response,                   \
                   cpkt_opcua_##payload_type *const *const data) {             \
    history_read_check(server, context, session, session_context, header,      \
                       details, timestamps, release, count, nodes, response,   \
                       data[0]);                                               \
    data[0]->field = (cpkt_opcua_##value_type *)cpkt_opcua_array_new(          \
        1, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_##initialize));                 \
    CHECK(data[0]->field);                                                     \
    data[0]->field_size = 1;                                                   \
  }
HISTORY_READER(read_raw_allocate, ReadRawModifiedDetails, HistoryData,
               DataValue, dataValues, dataValuesSize, DATAVALUE)
HISTORY_READER(read_modified_allocate, ReadRawModifiedDetails,
               HistoryModifiedData, DataValue, dataValues, dataValuesSize,
               DATAVALUE)
HISTORY_READER(read_event_allocate, ReadEventDetails, HistoryEvent,
               HistoryEventFieldList, events, eventsSize, HISTORYEVENTFIELDLIST)
HISTORY_READER(read_processed_allocate, ReadProcessedDetails, HistoryData,
               DataValue, dataValues, dataValuesSize, DATAVALUE)
HISTORY_READER(read_time_allocate, ReadAtTimeDetails, HistoryData, DataValue,
               dataValues, dataValuesSize, DATAVALUE)
#define READ_PARAMS(details_type, payload_type)                                \
  cpkt_opcua_server *server, void *context, const cpkt_opcua_NodeId *session,  \
      void *session_context, const cpkt_opcua_RequestHeader *header,           \
      const cpkt_opcua_##details_type *details,                                \
      cpkt_opcua_TimestampsToReturn timestamps, cpkt_opcua_Boolean release,    \
      size_t count, const cpkt_opcua_HistoryReadValueId *nodes,                \
      cpkt_opcua_HistoryReadResponse *response,                                \
      cpkt_opcua_##payload_type *const *const data
#define READ_ARGS                                                              \
  server, context, session, session_context, header, details, timestamps,      \
      release, count, nodes, response, data
static void read_raw(READ_PARAMS(ReadRawModifiedDetails, HistoryData)) {
  CHECK(details->startTime.high32 == 0x80000000U &&
        details->endTime.high32 == 0x7fffffffU);
  read_raw_allocate(READ_ARGS);
  fill_value(&data[0]->dataValues[0]);
}
static void read_modified(READ_PARAMS(ReadRawModifiedDetails,
                                      HistoryModifiedData)) {
  read_modified_allocate(READ_ARGS);
  fill_value(&data[0]->dataValues[0]);
}
static void read_processed(READ_PARAMS(ReadProcessedDetails, HistoryData)) {
  read_processed_allocate(READ_ARGS);
  fill_value(&data[0]->dataValues[0]);
}
static void read_time(READ_PARAMS(ReadAtTimeDetails, HistoryData)) {
  read_time_allocate(READ_ARGS);
  fill_value(&data[0]->dataValues[0]);
}
static void read_event(READ_PARAMS(ReadEventDetails, HistoryEvent)) {
  cpkt_opcua_HistoryEventFieldList *fields;
  read_event_allocate(READ_ARGS);
  fields = &data[0]->events[0];
  fields->eventFields = (cpkt_opcua_Variant *)cpkt_opcua_array_new(
      1, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_VARIANT));
  CHECK(fields->eventFields);
  fields->eventFieldsSize = 1;
  fields->eventFields[0].data = cpkt_opcua_Int64_new();
  CHECK(fields->eventFields[0].data);
  fields->eventFields[0].type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
  ((cpkt_opcua_Int64 *)fields->eventFields[0].data)->high32 = 0x80000000U;
}
static void update_result(cpkt_opcua_HistoryUpdateResult *result) {
  result->operationResults = (cpkt_opcua_StatusCode *)cpkt_opcua_array_new(
      1, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_STATUSCODE));
  CHECK(result->operationResults);
  result->operationResultsSize = 1;
  result->operationResults[0] = CPKT_OPCUA_STATUSCODE_BADNOTFOUND;
}
static void history_fail_result(void *context) {
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  if (((history_state *)context)->fail_result)
    cpkt_types_fail_after(0);
#else
  (void)context;
#endif
}
#define HISTORY_UPDATER(name, type, assertion)                                 \
  static void name(cpkt_opcua_server *server, void *context,                   \
                   const cpkt_opcua_NodeId *session, void *session_context,    \
                   const cpkt_opcua_RequestHeader *header,                     \
                   const cpkt_opcua_##type *details,                           \
                   cpkt_opcua_HistoryUpdateResult *result) {                   \
    history_called(server, context);                                           \
    CHECK(!session && !session_context && header && (assertion));              \
    update_result(result);                                                     \
    history_fail_result(context);                                              \
  }
HISTORY_UPDATER(history_update, UpdateDataDetails,
                details->updateValuesSize == 1)
HISTORY_UPDATER(history_delete, DeleteRawModifiedDetails,
                details->startTime.high32 == 0x80000000U &&
                    details->endTime.high32 == 0x7fffffffU)
HISTORY_UPDATER(history_delete_event, DeleteEventDetails,
                details->nodeId.identifier.numeric == 6001)
static void history_cleared(cpkt_opcua_HistoryDatabase *plugin) {
  history_state *state = (history_state *)plugin->context;
  ++state->cleared;
}
static cpkt_opcua_status history_native(void *native, void *user) {
  (void)user;
  return cpkt_types_peer_history(native);
}
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
static cpkt_opcua_status failed_history(void *native, void *user) {
  (void)user;
  CHECK(cpkt_types_peer_history_failure(native) ==
        CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY);
  CHECK(cpkt_types_fail_stop());
  return 0;
}
#endif
static void history_view(cpkt_opcua_server *server) {
  cpkt_opcua_ServerConfig *config;
  cpkt_opcua_HistoryDatabase *hdb;
  cpkt_opcua_NodeId node = cpkt_opcua_NODEID_NUMERIC(1, 6001);
  cpkt_opcua_DataValue value;
  cpkt_opcua_Int64 number;
  cpkt_opcua_RequestHeader header;
  cpkt_opcua_HistoryReadValueId read;
  cpkt_opcua_ReadRawModifiedDetails raw;
  cpkt_opcua_ReadEventDetails event;
  cpkt_opcua_ReadProcessedDetails processed;
  cpkt_opcua_ReadAtTimeDetails at_time;
  cpkt_opcua_HistoryReadResponse response;
  cpkt_opcua_HistoryUpdateResult result;
  cpkt_opcua_UpdateDataDetails update;
  cpkt_opcua_DeleteRawModifiedDetails deletion;
  cpkt_opcua_DeleteEventDetails delete_event;
  size_t i;
  cpkt_opcua_EventFieldList fields;
  CHECK(cpkt_opcua_server_get_config_typed(server, &config) == 0);
  CHECK(cpkt_opcua_ServerConfig_getHistoryDatabase(config, &hdb) == 0 &&
        hdb != NULL);
  CHECK(cpkt_opcua_ServerConfig_setHistoryDatabase(config, hdb) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  number.high32 = 0x80000000U;
  number.low32 = 0;
  cpkt_opcua_DataValue_init(&value);
  value.hasValue = 1;
  cpkt_opcua_Variant_setScalar(&value.value, &number,
                               cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64));
  value.value.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
  hdb->setValue(server, hdb->context, NULL, NULL, &node, 1, &value);
  cpkt_opcua_EventFieldList_init(&fields);
  hdb->setEvent(server, hdb->context, &node, &node, NULL, &fields);
  cpkt_opcua_EventFieldList_clear(&fields);
  cpkt_opcua_RequestHeader_init(&header);
  cpkt_opcua_HistoryReadValueId_init(&read);
  read.nodeId = node;
  cpkt_opcua_ReadRawModifiedDetails_init(&raw);
  raw.startTime.high32 = 0x80000000U;
  raw.startTime.low32 = 0;
  raw.endTime.high32 = 0x7fffffffU;
  raw.endTime.low32 = 0xffffffffU;
  cpkt_opcua_ReadEventDetails_init(&event);
  cpkt_opcua_ReadProcessedDetails_init(&processed);
  cpkt_opcua_ReadAtTimeDetails_init(&at_time);
  for (i = 0; i < 5; ++i) {
    const cpkt_opcua_Type *type =
        cpkt_opcua_type_at(i == 1   ? CPKT_OPCUA_TYPES_HISTORYMODIFIEDDATA
                           : i == 2 ? CPKT_OPCUA_TYPES_HISTORYEVENT
                                    : CPKT_OPCUA_TYPES_HISTORYDATA);
    void *payload = cpkt_opcua_type_new(type);
    cpkt_opcua_HistoryReadResponse_init(&response);
    response.results = cpkt_opcua_array_new(
        1, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_HISTORYREADRESULT));
    response.resultsSize = 1;
    if (!payload || !response.results)
      abort();
    cpkt_opcua_ExtensionObject_setValue(&response.results[0].historyData,
                                        payload, type);
    if (i == 1) {
      cpkt_opcua_HistoryModifiedData *items[1];
      items[0] = (cpkt_opcua_HistoryModifiedData *)payload;
      hdb->readModified(server, hdb->context, NULL, NULL, &header, &raw,
                        cpkt_opcua_TIMESTAMPSTORETURN_BOTH, 0, 1, &read,
                        &response, items);
    } else if (i == 2) {
      cpkt_opcua_HistoryEvent *items[1];
      items[0] = (cpkt_opcua_HistoryEvent *)payload;
      hdb->readEvent(server, hdb->context, NULL, NULL, &header, &event,
                     cpkt_opcua_TIMESTAMPSTORETURN_BOTH, 0, 1, &read, &response,
                     items);
    } else {
      cpkt_opcua_HistoryData *items[1];
      items[0] = (cpkt_opcua_HistoryData *)payload;
      if (i == 0)
        hdb->readRaw(server, hdb->context, NULL, NULL, &header, &raw,
                     cpkt_opcua_TIMESTAMPSTORETURN_BOTH, 0, 1, &read, &response,
                     items);
      else if (i == 3)
        hdb->readProcessed(server, hdb->context, NULL, NULL, &header,
                           &processed, cpkt_opcua_TIMESTAMPSTORETURN_BOTH, 0, 1,
                           &read, &response, items);
      else
        hdb->readAtTime(server, hdb->context, NULL, NULL, &header, &at_time,
                        cpkt_opcua_TIMESTAMPSTORETURN_BOTH, 0, 1, &read,
                        &response, items);
    }
    if (response.responseHeader.serviceResult || response.resultsSize != 1 ||
        response.results[0].historyData.content.decoded.type != type)
      abort();
    if (i == 2) {
      cpkt_opcua_HistoryEvent *history =
          response.results[0].historyData.content.decoded.data;
      if (history->eventsSize != 1 || history->events[0].eventFieldsSize != 1 ||
          ((const cpkt_opcua_Int64 *)history->events[0].eventFields[0].data)
                  ->high32 != 0x80000000U)
        abort();
    } else if (i == 1) {
      cpkt_opcua_HistoryModifiedData *history =
          response.results[0].historyData.content.decoded.data;
      if (history->dataValuesSize != 1 ||
          ((const cpkt_opcua_Int64 *)history->dataValues[0].value.data)
                  ->high32 != 0x80000000U)
        abort();
    } else {
      cpkt_opcua_HistoryData *history =
          response.results[0].historyData.content.decoded.data;
      if (history->dataValuesSize != 1 ||
          ((const cpkt_opcua_Int64 *)history->dataValues[0].value.data)
                  ->high32 != 0x80000000U)
        abort();
    }
    cpkt_opcua_HistoryReadResponse_clear(&response);
  }
  cpkt_opcua_UpdateDataDetails_init(&update);
  update.nodeId = node;
  update.updateValues = &value;
  update.updateValuesSize = 1;
  cpkt_opcua_DeleteRawModifiedDetails_init(&deletion);
  deletion.nodeId = node;
  deletion.startTime = raw.startTime;
  deletion.endTime = raw.endTime;
  cpkt_opcua_DeleteEventDetails_init(&delete_event);
  delete_event.nodeId = node;
  for (i = 0; i < 3; ++i) {
    cpkt_opcua_HistoryUpdateResult_init(&result);
    if (i == 0)
      hdb->updateData(server, hdb->context, NULL, NULL, &header, &update,
                      &result);
    else if (i == 1)
      hdb->deleteRawModified(server, hdb->context, NULL, NULL, &header,
                             &deletion, &result);
    else
      hdb->deleteEvent(server, hdb->context, NULL, NULL, &header, &delete_event,
                       &result);
    if (result.statusCode || result.operationResultsSize != 1 ||
        result.operationResults[0] != CPKT_OPCUA_STATUSCODE_BADNOTFOUND)
      abort();
    cpkt_opcua_HistoryUpdateResult_clear(&result);
  }
}
static void history_plugins(void) {
  history_state state;
  cpkt_opcua_HistoryDatabase plugin;
  memset(&state, 0, sizeof(state));
  memset(&plugin, 0, sizeof(plugin));
  CHECK(cpkt_opcua_server_new(&state.server, 0) == CPKT_OPCUA_OK);
  plugin.context = &state;
  plugin.clear = history_cleared;
  plugin.setValue = history_set;
  plugin.setEvent = history_event;
  plugin.readRaw = read_raw;
  plugin.readModified = read_modified;
  plugin.readEvent = read_event;
  plugin.readProcessed = read_processed;
  plugin.readAtTime = read_time;
  plugin.updateData = history_update;
  plugin.deleteRawModified = history_delete;
  plugin.deleteEvent = history_delete_event;
  CHECK(cpkt_opcua_server_set_history_database_plugin(state.server, &plugin) ==
        0);
  CHECK(cpkt_opcua_server_native(state.server, history_native, NULL) ==
        CPKT_OPCUA_OK);
  CHECK(state.calls == 10 && state.cleared == 0);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  cpkt_types_fail_after(0);
  CHECK(cpkt_opcua_server_set_history_database_plugin(state.server, &plugin) !=
        0);
  CHECK(cpkt_types_fail_stop() && state.cleared == 0);
  CHECK(cpkt_opcua_server_native(state.server, history_native, NULL) ==
        CPKT_OPCUA_OK);
  CHECK(state.calls == 20 && state.cleared == 0);
  state.fail_result = 1;
  CHECK(cpkt_opcua_server_native(state.server, failed_history, NULL) ==
        CPKT_OPCUA_OK);
  CHECK(state.calls == 21 && state.cleared == 0);
  state.fail_result = 0;
#endif
  CHECK(cpkt_opcua_server_set_history_database_plugin(state.server, &plugin) ==
        0);
  CHECK(state.cleared == 1);
  {
    unsigned int before = state.calls;
    history_view(state.server);
    CHECK(state.calls == before + 10 && state.cleared == 1);
  }
  cpkt_opcua_server_free(state.server);
  CHECK(state.cleared == 2);
}
void cpkt_types_test_plugins(void) {
  cpkt_opcua_AccessControl plugin;
  cpkt_opcua_UserTokenPolicy policy;
  plugin_state state;
  char policy_id[] = "typed";
  memset(&state, 0, sizeof(state));
  memset(&plugin, 0, sizeof(plugin));
  cpkt_opcua_UserTokenPolicy_init(&policy);
  CHECK(cpkt_opcua_server_new(&state.server, 0) == CPKT_OPCUA_OK);
  plugin.context = &state;
  plugin.clear = cleared;
  plugin.activateSession = activate;
  plugin.closeSession = close_session;
  plugin.getUserRightsMask = rights;
  plugin.getUserAccessLevel = access_level;
  plugin.getUserExecutable = executable;
  plugin.getUserExecutableOnObject = executable_object;
  plugin.allowAddNode = add_node;
  plugin.allowAddReference = add_reference;
  plugin.allowDeleteNode = delete_node;
  plugin.allowDeleteReference = delete_reference;
  plugin.allowBrowseNode = browse;
  plugin.allowTransferSubscription = transfer;
  plugin.allowHistoryUpdateUpdateData = update_history;
  plugin.allowHistoryUpdateDeleteRawModified = delete_history;
  policy.policyId.data = (cpkt_opcua_Byte *)policy_id;
  policy.policyId.length = 5;
  plugin.userTokenPoliciesSize = 1;
  plugin.userTokenPolicies = &policy;
  CHECK(cpkt_opcua_server_set_access_control_plugin(NULL, &plugin) != 0);
  CHECK(cpkt_opcua_server_set_access_control_plugin(state.server, NULL) != 0);
  CHECK(cpkt_opcua_server_set_access_control_plugin(state.server, &plugin) ==
        0);
  policy_id[0] = '!';
  CHECK(cpkt_opcua_server_native(state.server, run_native, NULL) ==
        CPKT_OPCUA_OK);
  CHECK(state.calls == 14 && state.closed == 1 && state.cleared == 0);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  state.allow_missing_session = 1;
  CHECK(cpkt_opcua_server_native(state.server, failed_close, &state) ==
        CPKT_OPCUA_OK);
  CHECK(state.closed == 2);
  state.allow_missing_session = 0;
  policy_id[0] = 't';
  {
    size_t i;
    int injected;
    cpkt_opcua_StatusCode status;
    for (i = 0; i < 100; ++i) {
      cpkt_types_fail_after(i);
      status =
          cpkt_opcua_server_set_access_control_plugin(state.server, &plugin);
      injected = cpkt_types_fail_stop();
      if (injected)
        CHECK(status != 0 && state.cleared == 0);
      else {
        CHECK(status == 0 && state.cleared == 1);
        break;
      }
      CHECK(cpkt_opcua_server_native(state.server, run_native, NULL) ==
            CPKT_OPCUA_OK);
    }
    CHECK(i > 0 && i < 100);
  }
#endif
  cpkt_opcua_server_free(state.server);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  CHECK(state.cleared == 2);
#else
  CHECK(state.cleared == 1);
#endif
  arrays();
  history_plugins();
}
