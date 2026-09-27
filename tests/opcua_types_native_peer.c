#include "opcua_types_peer.h"
#include <arpa/inet.h>
#include <open62541/client_highlevel.h>
#include <open62541/client_highlevel_async.h>
#include <open62541/client_subscriptions.h>
#include <open62541/plugin/accesscontrol.h>
#include <open62541/plugin/historydatabase.h>
#include <open62541/plugin/nodestore.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>
#include <open62541/util.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

unsigned int cpkt_types_native_empty(size_t index, unsigned char **bytes,
                                     size_t *length) {
  void *value;
  UA_ByteString encoded = UA_BYTESTRING_NULL;
  UA_StatusCode status;
  if (index >= UA_TYPES_COUNT)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  value = UA_new(&UA_TYPES[index]);
  if (!value)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  status = UA_encodeBinary(value, &UA_TYPES[index], &encoded, NULL);
  UA_delete(value, &UA_TYPES[index]);
  if (!status) {
    *bytes = encoded.data;
    *length = encoded.length;
  } else
    UA_ByteString_clear(&encoded);
  return status;
}
void cpkt_types_native_free(unsigned char *bytes) { UA_free(bytes); }
unsigned int cpkt_types_peer_defaults(int monitored, unsigned char **bytes,
                                      size_t *length) {
  UA_ByteString output = UA_BYTESTRING_NULL;
  UA_StatusCode status;
  if (monitored) {
    UA_MonitoredItemCreateRequest request =
        UA_MonitoredItemCreateRequest_default(
            UA_NODEID_STRING(1, "borrowed-node"));
    status = UA_encodeBinary(&request,
                             &UA_TYPES[UA_TYPES_MONITOREDITEMCREATEREQUEST],
                             &output, NULL);
  } else {
    UA_CreateSubscriptionRequest request =
        UA_CreateSubscriptionRequest_default();
    status = UA_encodeBinary(
        &request, &UA_TYPES[UA_TYPES_CREATESUBSCRIPTIONREQUEST], &output, NULL);
  }
  *bytes = output.data;
  *length = output.length;
  return status;
}
static void single_error_deleted(UA_Client *client, UA_UInt32 sub,
                                 void *sub_context, UA_UInt32 id,
                                 void *context) {
  unsigned int *deleted = context;
  (void)client;
  (void)sub;
  (void)sub_context;
  if (id)
    abort();
  ++*deleted;
}
unsigned int cpkt_types_peer_single_error(void *client,
                                          unsigned int subscription_id,
                                          int events, int missing,
                                          unsigned int *deleted) {
  UA_MonitoredItemCreateRequest request = UA_MonitoredItemCreateRequest_default(
      UA_NODEID_NUMERIC(1, missing ? 0xffffffffU : 6001));
  UA_MonitoredItemCreateResult result;
  request.requestedParameters.samplingInterval = 10;
  *deleted = 0;
  if (events)
    result = UA_Client_MonitoredItems_createEvent(
        client, subscription_id, UA_TIMESTAMPSTORETURN_BOTH, request, deleted,
        NULL, single_error_deleted);
  else
    result = UA_Client_MonitoredItems_createDataChange(
        client, subscription_id, UA_TIMESTAMPSTORETURN_BOTH, request, deleted,
        NULL, single_error_deleted);
  UA_StatusCode status = result.statusCode;
  UA_MonitoredItemCreateResult_clear(&result);
  return status;
}
unsigned int cpkt_types_peer_access_control(void *arg) {
  UA_Server *server = (UA_Server *)arg;
  UA_AccessControl *ac = &UA_Server_getConfig(server)->accessControl;
  UA_NodeId session = UA_NODEID_NUMERIC(2, 17),
            node = UA_NODEID_STRING(3, "wide");
  UA_EndpointDescription endpoint;
  UA_AnonymousIdentityToken token;
  UA_ExtensionObject identity;
  UA_AddNodesItem add;
  UA_AddReferencesItem add_reference;
  UA_DeleteNodesItem deletion;
  UA_DeleteReferencesItem delete_reference;
  UA_DataValue value;
  UA_Int64 number = INT64_MIN;
  UA_ByteString certificate = UA_BYTESTRING("cert");
  void *context = NULL;
  UA_EndpointDescription_init(&endpoint);
  endpoint.server.applicationUri = UA_STRING("application");
  UA_AnonymousIdentityToken_init(&token);
  token.policyId = UA_STRING("typed");
  UA_ExtensionObject_init(&identity);
  identity.encoding = UA_EXTENSIONOBJECT_DECODED_NODELETE;
  identity.content.decoded.type = &UA_TYPES[UA_TYPES_ANONYMOUSIDENTITYTOKEN];
  identity.content.decoded.data = &token;
  if (ac->activateSession(server, ac, &endpoint, &certificate, &session,
                          &identity, &context) != 0 ||
      !context)
    return UA_STATUSCODE_BADUNEXPECTEDERROR;
  if (ac->getUserRightsMask(server, ac, &session, context, &node, NULL) !=
          0xffffffffU ||
      ac->getUserAccessLevel(server, ac, &session, context, &node, NULL) != 3 ||
      !ac->getUserExecutable(server, ac, &session, context, &node, NULL) ||
      !ac->getUserExecutableOnObject(server, ac, &session, context, &node, NULL,
                                     &node, NULL) ||
      !ac->allowBrowseNode(server, ac, NULL, context, &node, NULL) ||
      !ac->allowTransferSubscription(server, ac, &session, context, NULL,
                                     context))
    return UA_STATUSCODE_BADUNEXPECTEDERROR;
  UA_AddNodesItem_init(&add);
  add.browseName = UA_QUALIFIEDNAME(1, "added");
  UA_AddReferencesItem_init(&add_reference);
  add_reference.sourceNodeId = node;
  UA_DeleteNodesItem_init(&deletion);
  deletion.nodeId = node;
  UA_DeleteReferencesItem_init(&delete_reference);
  delete_reference.sourceNodeId = node;
  if (!ac->allowAddNode(server, ac, &session, context, &add) ||
      !ac->allowAddReference(server, ac, &session, context, &add_reference) ||
      !ac->allowDeleteNode(server, ac, &session, context, &deletion) ||
      !ac->allowDeleteReference(server, ac, &session, context,
                                &delete_reference))
    return UA_STATUSCODE_BADUNEXPECTEDERROR;
  UA_DataValue_init(&value);
  value.hasValue = true;
  UA_Variant_setScalar(&value.value, &number, &UA_TYPES[UA_TYPES_INT64]);
  value.value.storageType = UA_VARIANT_DATA_NODELETE;
  if (!ac->allowHistoryUpdateUpdateData(server, ac, &session, context, &node,
                                        UA_PERFORMUPDATETYPE_INSERT, &value) ||
      !ac->allowHistoryUpdateDeleteRawModified(
          server, ac, &session, context, &node, INT64_MIN, INT64_MAX, true))
    return UA_STATUSCODE_BADUNEXPECTEDERROR;
  ac->closeSession(server, ac, &session, context);
  return 0;
}
unsigned int cpkt_types_peer_history(void *arg) {
  UA_Server *server = (UA_Server *)arg;
  UA_HistoryDatabase *hdb = &UA_Server_getConfig(server)->historyDatabase;
  UA_NodeId node = UA_NODEID_NUMERIC(1, 6001);
  UA_DataValue value;
  UA_Int64 number = INT64_MIN;
  UA_RequestHeader header;
  UA_HistoryReadValueId read;
  UA_ReadRawModifiedDetails raw;
  UA_ReadEventDetails event;
  UA_ReadProcessedDetails processed;
  UA_ReadAtTimeDetails at_time;
  UA_HistoryReadResponse response;
  UA_HistoryUpdateResult result;
  UA_UpdateDataDetails update;
  UA_DeleteRawModifiedDetails deletion;
  UA_DeleteEventDetails delete_event;
  size_t i;
  UA_DataValue_init(&value);
  value.hasValue = true;
  UA_Variant_setScalar(&value.value, &number, &UA_TYPES[UA_TYPES_INT64]);
  value.value.storageType = UA_VARIANT_DATA_NODELETE;
  hdb->setValue(server, hdb->context, NULL, NULL, &node, true, &value);
  UA_EventFieldList fields;
  UA_EventFieldList_init(&fields);
  hdb->setEvent(server, hdb->context, &node, &node, NULL, &fields);
  UA_EventFieldList_clear(&fields);
  UA_RequestHeader_init(&header);
  UA_HistoryReadValueId_init(&read);
  read.nodeId = node;
  UA_ReadRawModifiedDetails_init(&raw);
  raw.startTime = INT64_MIN;
  raw.endTime = INT64_MAX;
  UA_ReadEventDetails_init(&event);
  UA_ReadProcessedDetails_init(&processed);
  UA_ReadAtTimeDetails_init(&at_time);
  for (i = 0; i < 5; ++i) {
    const UA_DataType *type = &UA_TYPES[i == 1   ? UA_TYPES_HISTORYMODIFIEDDATA
                                        : i == 2 ? UA_TYPES_HISTORYEVENT
                                                 : UA_TYPES_HISTORYDATA];
    void *payload = UA_new(type);
    UA_HistoryReadResponse_init(&response);
    response.results = UA_Array_new(1, &UA_TYPES[UA_TYPES_HISTORYREADRESULT]);
    response.resultsSize = 1;
    if (!payload || !response.results)
      abort();
    UA_ExtensionObject_setValue(&response.results[0].historyData, payload,
                                type);
    if (i == 1) {
      UA_HistoryModifiedData *items[1] = {payload};
      hdb->readModified(server, hdb->context, NULL, NULL, &header, &raw,
                        UA_TIMESTAMPSTORETURN_BOTH, false, 1, &read, &response,
                        items);
    } else if (i == 2) {
      UA_HistoryEvent *items[1] = {payload};
      hdb->readEvent(server, hdb->context, NULL, NULL, &header, &event,
                     UA_TIMESTAMPSTORETURN_BOTH, false, 1, &read, &response,
                     items);
    } else {
      UA_HistoryData *items[1] = {payload};
      if (i == 0)
        hdb->readRaw(server, hdb->context, NULL, NULL, &header, &raw,
                     UA_TIMESTAMPSTORETURN_BOTH, false, 1, &read, &response,
                     items);
      else if (i == 3)
        hdb->readProcessed(server, hdb->context, NULL, NULL, &header,
                           &processed, UA_TIMESTAMPSTORETURN_BOTH, false, 1,
                           &read, &response, items);
      else
        hdb->readAtTime(server, hdb->context, NULL, NULL, &header, &at_time,
                        UA_TIMESTAMPSTORETURN_BOTH, false, 1, &read, &response,
                        items);
    }
    if (response.responseHeader.serviceResult || response.resultsSize != 1 ||
        response.results[0].historyData.content.decoded.type != type)
      abort();
    if (i == 2) {
      UA_HistoryEvent *history =
          response.results[0].historyData.content.decoded.data;
      if (history->eventsSize != 1 || history->events[0].eventFieldsSize != 1 ||
          *(const UA_Int64 *)history->events[0].eventFields[0].data !=
              INT64_MIN)
        abort();
    } else if (i == 1) {
      UA_HistoryModifiedData *history =
          response.results[0].historyData.content.decoded.data;
      if (history->dataValuesSize != 1 ||
          *(const UA_Int64 *)history->dataValues[0].value.data != INT64_MIN)
        abort();
    } else {
      UA_HistoryData *history =
          response.results[0].historyData.content.decoded.data;
      if (history->dataValuesSize != 1 ||
          *(const UA_Int64 *)history->dataValues[0].value.data != INT64_MIN)
        abort();
    }
    UA_HistoryReadResponse_clear(&response);
  }
  UA_UpdateDataDetails_init(&update);
  update.nodeId = node;
  update.updateValues = &value;
  update.updateValuesSize = 1;
  UA_DeleteRawModifiedDetails_init(&deletion);
  deletion.nodeId = node;
  deletion.startTime = INT64_MIN;
  deletion.endTime = INT64_MAX;
  UA_DeleteEventDetails_init(&delete_event);
  delete_event.nodeId = node;
  for (i = 0; i < 3; ++i) {
    UA_HistoryUpdateResult_init(&result);
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
        result.operationResults[0] != UA_STATUSCODE_BADNOTFOUND)
      abort();
    UA_HistoryUpdateResult_clear(&result);
  }
  return 0;
}
unsigned int cpkt_types_peer_close_session_failure(void *native_server,
                                                   void *context) {
  UA_Server *server = (UA_Server *)native_server;
  UA_AccessControl *ac = &UA_Server_getConfig(server)->accessControl;
  UA_NodeId session = UA_NODEID_STRING(2, "session");
  ac->closeSession(server, ac, &session, context);
  return 0;
}
unsigned int cpkt_types_peer_history_failure(void *native_server) {
  UA_Server *server = (UA_Server *)native_server;
  UA_HistoryDatabase *hdb = &UA_Server_getConfig(server)->historyDatabase;
  UA_RequestHeader header;
  UA_DeleteEventDetails details;
  UA_HistoryUpdateResult result;
  UA_StatusCode status;
  UA_RequestHeader_init(&header);
  UA_DeleteEventDetails_init(&details);
  UA_HistoryUpdateResult_init(&result);
  details.nodeId = UA_NODEID_NUMERIC(2, 6001);
  hdb->deleteEvent(server, hdb->context, NULL, NULL, &header, &details,
                   &result);
  status = result.statusCode;
  if (result.operationResultsSize != 0 || result.operationResults != NULL)
    abort();
  UA_HistoryUpdateResult_clear(&result);
  return status;
}
struct peer {
  UA_Server *server;
  pthread_t thread;
  pthread_mutex_t lock;
  int running;
  unsigned int history_pages[3], history_releases[3];
};
unsigned int cpkt_types_peer_event(void *arg) {
  struct peer *peer = (struct peer *)arg;
  UA_StatusCode status;
  UA_LocalizedText message = UA_LOCALIZEDTEXT("en", "typed event");
  UA_UInt16 severity = 321;
  pthread_mutex_lock(&peer->lock);
  status =
      UA_Server_createEvent(peer->server, UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEEVENTTYPE),
                            severity, message, NULL, NULL, NULL);
  pthread_mutex_unlock(&peer->lock);
  return status;
}
static void *server_loop(void *arg) {
  struct peer *peer = (struct peer *)arg;
  for (;;) {
    pthread_mutex_lock(&peer->lock);
    int running = peer->running;
    if (running)
      UA_Server_run_iterate(peer->server, false);
    pthread_mutex_unlock(&peer->lock);
    if (!running)
      break;
    usleep(1000);
  }
  return NULL;
}
void *cpkt_types_peer_start(unsigned short *port) {
  struct sockaddr_in address;
  socklen_t size = sizeof(address);
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  struct peer *peer = calloc(1, sizeof(*peer));
  if (fd < 0 || !peer) {
    if (fd >= 0)
      close(fd);
    free(peer);
    return NULL;
  }
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(fd, (struct sockaddr *)&address, size) ||
      getsockname(fd, (struct sockaddr *)&address, &size)) {
    close(fd);
    free(peer);
    return NULL;
  }
  *port = ntohs(address.sin_port);
  close(fd);
  peer->server = UA_Server_new();
  if (!peer->server) {
    free(peer);
    return NULL;
  }
  UA_StatusCode status = UA_ServerConfig_setMinimal(
      UA_Server_getConfig(peer->server), *port, NULL);
  UA_VariableAttributes attr = UA_VariableAttributes_default;
  UA_Int64 values[4] = {0, 0, 0, 0};
  UA_UInt32 dimension = 4;
  attr.arrayDimensionsSize = 1;
  attr.arrayDimensions = &dimension;
  attr.dataType = UA_TYPES[UA_TYPES_INT64].typeId;
  attr.valueRank = 1;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  UA_Variant_setArray(&attr.value, values, 4, &UA_TYPES[UA_TYPES_INT64]);
  attr.value.storageType = UA_VARIANT_DATA_NODELETE;
  if (!status)
    status = UA_Server_addVariableNode(
        peer->server, UA_NODEID_NUMERIC(1, 6001),
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), UA_QUALIFIEDNAME(1, "wide"),
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, NULL);
  if (!status)
    status = UA_Server_run_startup(peer->server);
  if (status) {
    UA_Server_delete(peer->server);
    free(peer);
    return NULL;
  }
  pthread_mutex_init(&peer->lock, NULL);
  peer->running = 1;
  if (pthread_create(&peer->thread, NULL, server_loop, peer)) {
    UA_Server_run_shutdown(peer->server);
    UA_Server_delete(peer->server);
    pthread_mutex_destroy(&peer->lock);
    free(peer);
    return NULL;
  }
  return peer;
}
int cpkt_types_peer_check(void *arg) {
  struct peer *peer = (struct peer *)arg;
  UA_Variant value;
  UA_Variant_init(&value);
  pthread_mutex_lock(&peer->lock);
  UA_StatusCode status =
      UA_Server_readValue(peer->server, UA_NODEID_NUMERIC(1, 6001), &value);
  int ok = !status && value.type == &UA_TYPES[UA_TYPES_INT64] &&
           value.arrayLength == 4;
  if (ok) {
    const UA_Int64 *values = value.data;
    ok = values[0] == INT64_MIN && values[1] == INT64_MAX && values[2] == -1 &&
         values[3] == 0;
  }
  UA_Variant_clear(&value);
  pthread_mutex_unlock(&peer->lock);
  return ok;
}
void cpkt_types_peer_stop(void *arg) {
  struct peer *peer = (struct peer *)arg;
  if (!peer)
    return;
  pthread_mutex_lock(&peer->lock);
  peer->running = 0;
  pthread_mutex_unlock(&peer->lock);
  pthread_join(peer->thread, NULL);
  UA_Server_run_shutdown(peer->server);
  UA_Server_delete(peer->server);
  pthread_mutex_destroy(&peer->lock);
  free(peer);
}

/* Populate the entire upstream graph independently of the facade metadata.
 * Every structure field and array is visited; recursion is bounded so recursive
 * DiagnosticInfo and schema arrays remain finite test values. */
static UA_StatusCode populate(void *value, const UA_DataType *type,
                              unsigned int depth) {
  UA_StatusCode status = 0;
  size_t i;
  char *cursor;
  const UA_DataTypeMember *member;
  if (depth > 6)
    return 0;
  switch (type->typeKind) {
  case UA_DATATYPEKIND_BOOLEAN:
    *(UA_Boolean *)value = true;
    break;
  case UA_DATATYPEKIND_SBYTE:
    *(UA_SByte *)value = -77;
    break;
  case UA_DATATYPEKIND_BYTE:
    *(UA_Byte *)value = 0xab;
    break;
  case UA_DATATYPEKIND_INT16:
    *(UA_Int16 *)value = -12345;
    break;
  case UA_DATATYPEKIND_UINT16:
    *(UA_UInt16 *)value = 54321;
    break;
  case UA_DATATYPEKIND_INT32:
    *(UA_Int32 *)value = -123456789;
    break;
  case UA_DATATYPEKIND_UINT32:
  case UA_DATATYPEKIND_STATUSCODE:
    *(UA_UInt32 *)value = 0xfedcba98U;
    break;
  case UA_DATATYPEKIND_INT64:
  case UA_DATATYPEKIND_DATETIME:
    *(UA_Int64 *)value = INT64_MIN + 12345;
    break;
  case UA_DATATYPEKIND_UINT64:
    *(UA_UInt64 *)value = UINT64_MAX - 23456;
    break;
  case UA_DATATYPEKIND_FLOAT:
    *(UA_Float *)value = -1.25f;
    break;
  case UA_DATATYPEKIND_DOUBLE:
    *(UA_Double *)value = 123.75;
    break;
  case UA_DATATYPEKIND_ENUM:
    break; /* Zero enum representations match upstream handling. */
  case UA_DATATYPEKIND_STRING:
  case UA_DATATYPEKIND_BYTESTRING:
  case UA_DATATYPEKIND_XMLELEMENT:
    status = UA_ByteString_allocBuffer(value, 5);
    if (!status)
      memcpy(((UA_String *)value)->data, "a\0bcd", 5);
    break;
  case UA_DATATYPEKIND_GUID:
    memset(value, 0x55, sizeof(UA_Guid));
    break;
  case UA_DATATYPEKIND_NODEID: {
    UA_NodeId *id = value;
    id->namespaceIndex = 1;
    if (depth % 3 == 0) {
      id->identifierType = UA_NODEIDTYPE_GUID;
      memset(&id->identifier.guid, 0x39, sizeof(UA_Guid));
    } else {
      id->identifierType =
          depth % 3 == 1 ? UA_NODEIDTYPE_STRING : UA_NODEIDTYPE_BYTESTRING;
      status = populate(&id->identifier.string, &UA_TYPES[UA_TYPES_STRING],
                        depth + 1);
    }
    break;
  }
  case UA_DATATYPEKIND_EXPANDEDNODEID: {
    UA_ExpandedNodeId *id = value;
    status = populate(&id->nodeId, &UA_TYPES[UA_TYPES_NODEID], depth + 1);
    if (!status)
      status =
          populate(&id->namespaceUri, &UA_TYPES[UA_TYPES_STRING], depth + 1);
    id->nodeId.namespaceIndex = 0;
    id->serverIndex = 123;
    break;
  }
  case UA_DATATYPEKIND_QUALIFIEDNAME: {
    UA_QualifiedName *name = value;
    name->namespaceIndex = 1;
    status = populate(&name->name, &UA_TYPES[UA_TYPES_STRING], depth + 1);
    break;
  }
  case UA_DATATYPEKIND_LOCALIZEDTEXT: {
    UA_LocalizedText *text = value;
    status = populate(&text->locale, &UA_TYPES[UA_TYPES_STRING], depth + 1);
    if (!status)
      status = populate(&text->text, &UA_TYPES[UA_TYPES_STRING], depth + 1);
    break;
  }
  case UA_DATATYPEKIND_VARIANT: {
    UA_Variant *v = value;
    v->type = &UA_TYPES[UA_TYPES_UINT64];
    v->arrayLength = 2;
    v->data = UA_Array_new(2, v->type);
    if (!v->data)
      return UA_STATUSCODE_BADOUTOFMEMORY;
    status = populate(v->data, v->type, depth + 1);
    if (!status)
      status = populate((char *)v->data + v->type->memSize, v->type, depth + 1);
    v->arrayDimensionsSize = 1;
    v->arrayDimensions = UA_Array_new(1, &UA_TYPES[UA_TYPES_UINT32]);
    if (!v->arrayDimensions) {
      v->arrayDimensionsSize = 0;
      return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    v->arrayDimensions[0] = 2;
    break;
  }
  case UA_DATATYPEKIND_EXTENSIONOBJECT: {
    UA_ExtensionObject *v = value;
    v->encoding = UA_EXTENSIONOBJECT_DECODED;
    v->content.decoded.type = &UA_TYPES[UA_TYPES_ARGUMENT];
    v->content.decoded.data = UA_new(v->content.decoded.type);
    if (!v->content.decoded.data)
      return UA_STATUSCODE_BADOUTOFMEMORY;
    status =
        populate(v->content.decoded.data, v->content.decoded.type, depth + 1);
    break;
  }
  case UA_DATATYPEKIND_DATAVALUE: {
    UA_DataValue *v = value;
    v->hasValue = true;
    v->hasStatus = true;
    v->status = UA_STATUSCODE_BADTIMEOUT;
    v->hasSourceTimestamp = true;
    v->sourceTimestamp = INT64_MAX;
    v->hasServerTimestamp = true;
    v->serverTimestamp = INT64_MIN;
    v->hasSourcePicoseconds = true;
    v->sourcePicoseconds = 9876;
    v->hasServerPicoseconds = true;
    v->serverPicoseconds = 5432;
    status = populate(&v->value, &UA_TYPES[UA_TYPES_VARIANT], depth + 1);
    break;
  }
  case UA_DATATYPEKIND_DIAGNOSTICINFO: {
    UA_DiagnosticInfo *v = value;
    v->hasSymbolicId = true;
    v->symbolicId = 42;
    v->hasNamespaceUri = true;
    v->namespaceUri = 2;
    v->hasLocalizedText = true;
    v->localizedText = 3;
    v->hasLocale = true;
    v->locale = 4;
    v->hasInnerStatusCode = true;
    v->innerStatusCode = UA_STATUSCODE_BADINTERNALERROR;
    v->hasAdditionalInfo = true;
    status =
        populate(&v->additionalInfo, &UA_TYPES[UA_TYPES_STRING], depth + 1);
    if (depth < 3 && !status) {
      v->innerDiagnosticInfo = UA_new(type);
      if (!v->innerDiagnosticInfo)
        return UA_STATUSCODE_BADOUTOFMEMORY;
      v->hasInnerDiagnosticInfo = true;
      status = populate(v->innerDiagnosticInfo, type, depth + 1);
    }
    break;
  }
  case UA_DATATYPEKIND_STRUCTURE:
  case UA_DATATYPEKIND_OPTSTRUCT:
    cursor = value;
    for (i = 0; i < type->membersSize && !status; ++i) {
      member = &type->members[i];
      cursor += member->padding;
      if (member->isArray) {
        size_t count = 2;
        void *array = UA_Array_new(count, member->memberType);
        if (!array)
          return UA_STATUSCODE_BADOUTOFMEMORY;
        memcpy(cursor, &count, sizeof(count));
        cursor += sizeof(count);
        memcpy(cursor, &array, sizeof(array));
        cursor += sizeof(array);
        status = populate(array, member->memberType, depth + 1);
        if (!status)
          status = populate((char *)array + member->memberType->memSize,
                            member->memberType, depth + 1);
      } else if (member->isOptional) {
        void *optional = UA_new(member->memberType);
        if (!optional)
          return UA_STATUSCODE_BADOUTOFMEMORY;
        memcpy(cursor, &optional, sizeof(optional));
        cursor += sizeof(optional);
        status = populate(optional, member->memberType, depth + 1);
      } else {
        status = populate(cursor, member->memberType, depth + 1);
        cursor += member->memberType->memSize;
      }
    }
    break;
  default:
    return UA_STATUSCODE_BADNOTSUPPORTED;
  }
  return status;
}
unsigned int cpkt_types_native_populated(size_t index, unsigned char **bytes,
                                         size_t *length) {
  void *value = UA_new(&UA_TYPES[index]);
  UA_ByteString encoded = UA_BYTESTRING_NULL;
  UA_StatusCode status = value ? populate(value, &UA_TYPES[index], 0)
                               : UA_STATUSCODE_BADOUTOFMEMORY;
  if (!status)
    status = UA_encodeBinary(value, &UA_TYPES[index], &encoded, NULL);
  if (value)
    UA_delete(value, &UA_TYPES[index]);
  if (!status) {
    *bytes = encoded.data;
    *length = encoded.length;
  } else
    UA_ByteString_clear(&encoded);
  return status;
}

/* Exercise the actual default history engine with application C89 backends. */
unsigned int cpkt_types_peer_history_backend(void *arg, int mode) {
  UA_Server *server = (UA_Server *)arg;
  UA_HistoryDatabase *database = &UA_Server_getConfig(server)->historyDatabase;
  UA_NodeId node = UA_NODEID_NUMERIC(1, 6100);
  UA_RequestHeader header;
  UA_HistoryReadValueId read;
  UA_ReadRawModifiedDetails details;
  UA_HistoryReadResponse response;
  UA_HistoryData data, *data_pointer = &data;
  UA_HistoryUpdateResult result;
  UA_UpdateDataDetails update;
  UA_DeleteRawModifiedDetails deletion;
  UA_DataValue value;
  UA_Int64 number = INT64_MIN;
  size_t i;
  UA_StatusCode status = 0;
  for (i = 0; !mode && i < 2; ++i) {
    UA_VariableAttributes attributes = UA_VariableAttributes_default;
    attributes.historizing = true;
    attributes.accessLevel =
        UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE |
        UA_ACCESSLEVELMASK_HISTORYREAD | UA_ACCESSLEVELMASK_HISTORYWRITE;
    attributes.dataType = UA_TYPES[UA_TYPES_INT64].typeId;
    UA_Variant_setScalar(&attributes.value, &number, &UA_TYPES[UA_TYPES_INT64]);
    status = UA_Server_addVariableNode(
        server, UA_NODEID_NUMERIC(1, 6100 + (UA_UInt32)i),
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
        UA_QUALIFIEDNAME(1, "history"),
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attributes, NULL,
        NULL);
    if (status)
      return status;
  }
  UA_RequestHeader_init(&header);
  UA_HistoryReadValueId_init(&read);
  read.nodeId = node;
  read.indexRange = UA_STRING("0:1");
  UA_ReadRawModifiedDetails_init(&details);
  details.startTime = INT64_MIN;
  details.endTime = 200;
  details.returnBounds = true;
  UA_HistoryReadResponse_init(&response);
  response.results = (UA_HistoryReadResult *)UA_Array_new(
      1, &UA_TYPES[UA_TYPES_HISTORYREADRESULT]);
  if (!response.results)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  response.resultsSize = 1;
  UA_HistoryData_init(&data);
  database->readRaw(server, database->context, NULL, NULL, &header, &details,
                    UA_TIMESTAMPSTORETURN_BOTH, false, 1, &read, &response,
                    &data_pointer);
  if (mode == 1) {
    status =
        response.results[0].statusCode == UA_STATUSCODE_BADINVALIDARGUMENT &&
                !data.dataValues
            ? 0
            : UA_STATUSCODE_BADUNEXPECTEDERROR;
    UA_HistoryData_clear(&data);
    UA_HistoryReadResponse_clear(&response);
    return status;
  }
  if (mode == 2 && response.results[0].statusCode) {
    status = response.results[0].statusCode;
    UA_HistoryData_clear(&data);
    UA_HistoryReadResponse_clear(&response);
    return status;
  }
  if (response.results[0].statusCode || data.dataValuesSize != 3 ||
      !UA_Variant_hasScalarType(&data.dataValues[0].value,
                                &UA_TYPES[UA_TYPES_INT64]) ||
      *(UA_Int64 *)data.dataValues[0].value.data != INT64_MAX ||
      *(UA_Int64 *)data.dataValues[1].value.data != INT64_MIN ||
      data.dataValues[2].sourceTimestamp != 100 - UA_DATETIME_SEC)
    status = UA_STATUSCODE_BADUNEXPECTEDERROR;
  UA_HistoryData_clear(&data);
  UA_HistoryReadResponse_clear(&response);
  if (status || mode == 2)
    return status;
  read.nodeId = UA_NODEID_NUMERIC(1, 6101);
  read.continuationPoint = UA_BYTESTRING("token");
  UA_HistoryReadResponse_init(&response);
  response.results = (UA_HistoryReadResult *)UA_Array_new(
      1, &UA_TYPES[UA_TYPES_HISTORYREADRESULT]);
  if (!response.results)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  response.resultsSize = 1;
  UA_HistoryData_init(&data);
  database->readRaw(server, database->context, NULL, NULL, &header, &details,
                    UA_TIMESTAMPSTORETURN_BOTH, false, 1, &read, &response,
                    &data_pointer);
  if (response.results[0].statusCode || data.dataValuesSize != 2 ||
      *(UA_Int64 *)data.dataValues[0].value.data != INT64_MIN ||
      *(UA_Int64 *)data.dataValues[1].value.data != INT64_MAX ||
      response.results[0].continuationPoint.length != 4 ||
      memcmp(response.results[0].continuationPoint.data, "next", 4))
    status = UA_STATUSCODE_BADUNEXPECTEDERROR;
  UA_HistoryData_clear(&data);
  UA_HistoryReadResponse_clear(&response);
  if (status)
    return status;
  UA_DataValue_init(&value);
  value.hasValue = true;
  UA_Variant_setScalar(&value.value, &number, &UA_TYPES[UA_TYPES_INT64]);
  value.value.storageType = UA_VARIANT_DATA_NODELETE;
  database->setValue(server, database->context, NULL, NULL, &node, true,
                     &value);
  UA_UpdateDataDetails_init(&update);
  update.nodeId = node;
  update.updateValuesSize = 1;
  update.updateValues = &value;
  for (i = 1; i <= 3; ++i) {
    UA_HistoryUpdateResult_init(&result);
    update.performInsertReplace = (UA_PerformUpdateType)i;
    database->updateData(server, database->context, NULL, NULL, &header,
                         &update, &result);
    if (result.statusCode || result.operationResultsSize != 1 ||
        result.operationResults[0])
      status = UA_STATUSCODE_BADUNEXPECTEDERROR;
    UA_HistoryUpdateResult_clear(&result);
    if (status)
      return status;
  }
  UA_DeleteRawModifiedDetails_init(&deletion);
  deletion.nodeId = node;
  deletion.startTime = INT64_MIN;
  deletion.endTime = INT64_MAX;
  UA_HistoryUpdateResult_init(&result);
  database->deleteRawModified(server, database->context, NULL, NULL, &header,
                              &deletion, &result);
  status = result.statusCode;
  UA_HistoryUpdateResult_clear(&result);
  return status;
}

unsigned int cpkt_types_peer_history_poll_node(void *arg) {
  UA_Server *server = (UA_Server *)arg;
  UA_VariableAttributes attributes = UA_VariableAttributes_default;
  UA_Int64 value = INT64_MIN;
  attributes.historizing = true;
  attributes.dataType = UA_TYPES[UA_TYPES_INT64].typeId;
  UA_Variant_setScalar(&attributes.value, &value, &UA_TYPES[UA_TYPES_INT64]);
  return UA_Server_addVariableNode(
      server, UA_NODEID_NUMERIC(1, 6102),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), UA_QUALIFIEDNAME(1, "poll"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attributes, NULL,
      NULL);
}

/* Exercise the actual native pending-operation identities, independently of
 * the facade representation. No test callback includes a facade header. */
static UA_StatusCode
peer_async_read(UA_Server *server, const UA_NodeId *session,
                void *session_context, const UA_NodeId *node, void *context,
                UA_Boolean timestamp, const UA_NumericRange *range,
                UA_DataValue *value) {
  struct cpkt_async_peer *state = context;
  UA_Int64 number = INT64_MIN;
  (void)server;
  (void)session;
  (void)session_context;
  (void)node;
  (void)timestamp;
  (void)range;
  if (state->deferred) {
    state->read_result = value;
    return UA_STATUSCODE_GOODCOMPLETESASYNCHRONOUSLY;
  }
  value->hasValue = true;
  value->hasSourceTimestamp = true;
  value->sourceTimestamp = INT64_MAX;
  return UA_Variant_setScalarCopy(&value->value, &number,
                                  &UA_TYPES[UA_TYPES_INT64]);
}
static UA_StatusCode
peer_async_write(UA_Server *server, const UA_NodeId *session,
                 void *session_context, const UA_NodeId *node, void *context,
                 const UA_NumericRange *range, const UA_DataValue *value) {
  struct cpkt_async_peer *state = context;
  (void)server;
  (void)session;
  (void)session_context;
  (void)node;
  (void)range;
  if (!value->hasValue ||
      !UA_Variant_hasScalarType(&value->value, &UA_TYPES[UA_TYPES_INT64]) ||
      *(UA_Int64 *)value->value.data != INT64_MIN)
    return UA_STATUSCODE_BADTYPEMISMATCH;
  if (!state->deferred)
    return UA_STATUSCODE_GOOD;
  state->write_result = value;
  return UA_STATUSCODE_GOODCOMPLETESASYNCHRONOUSLY;
}
static void peer_async_cancel(UA_Server *server, const void *operation) {
  struct cpkt_async_peer *state = NULL;
  UA_Server_getNodeContext(server, UA_NODEID_NUMERIC(1, 6200), (void **)&state);
  if (!state)
    abort();
  ++state->cancelled;
  if (state->read_result == operation)
    state->read_result = NULL;
  if (state->write_result == operation)
    state->write_result = NULL;
  if (state->call_result == operation)
    state->call_result = NULL;
}
static UA_StatusCode peer_async_method(
    UA_Server *server, const UA_NodeId *session, void *session_context,
    const UA_NodeId *method, void *method_context, const UA_NodeId *object,
    void *object_context, size_t input_size, const UA_Variant *input,
    size_t output_size, UA_Variant *output) {
  struct cpkt_async_peer *state = method_context;
  UA_Int64 number = INT64_MIN;
  UA_StatusCode status;
  (void)server;
  (void)session;
  (void)session_context;
  (void)method;
  (void)object;
  (void)object_context;
  (void)input;
  if (input_size || output_size != 2)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (state->deferred) {
    state->call_result = output;
    return UA_STATUSCODE_GOODCOMPLETESASYNCHRONOUSLY;
  }
  status =
      UA_Variant_setScalarCopy(&output[0], &number, &UA_TYPES[UA_TYPES_INT64]);
  number = INT64_MAX;
  if (!status)
    status = UA_Variant_setScalarCopy(&output[1], &number,
                                      &UA_TYPES[UA_TYPES_INT64]);
  return status;
}
unsigned int cpkt_types_peer_async_install(void *native, void *context) {
  UA_Server *server = native;
  UA_VariableAttributes attributes = UA_VariableAttributes_default;
  UA_CallbackValueSource source = {peer_async_read, peer_async_write};
  UA_StatusCode status;
  UA_Argument outputs[2];
  UA_MethodAttributes method_attributes = UA_MethodAttributes_default;
  attributes.dataType = UA_TYPES[UA_TYPES_INT64].typeId;
  attributes.valueRank = UA_VALUERANK_SCALAR;
  attributes.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  status = UA_Server_addVariableNode(
      server, UA_NODEID_NUMERIC(1, 6200),
      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), UA_QUALIFIEDNAME(1, "async"),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attributes, context,
      NULL);
  if (!status)
    status = UA_Server_setVariableNode_callbackValueSource(
        server, UA_NODEID_NUMERIC(1, 6200), source);
  memset(outputs, 0, sizeof(outputs));
  outputs[0].dataType = outputs[1].dataType = UA_TYPES[UA_TYPES_INT64].typeId;
  outputs[0].valueRank = outputs[1].valueRank = UA_VALUERANK_SCALAR;
  method_attributes.executable = method_attributes.userExecutable = true;
  if (!status)
    status = UA_Server_addMethodNode(
        server, UA_NODEID_NUMERIC(1, 6201),
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
        UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
        UA_QUALIFIEDNAME(1, "asyncMethod"), method_attributes,
        peer_async_method, 0, NULL, 2, outputs, context, NULL);
  UA_Server_getConfig(server)->asyncOperationCancelCallback = peer_async_cancel;
  return status;
}
unsigned int cpkt_types_peer_async_complete(void *native, void *context) {
  struct cpkt_async_peer *state = context;
  UA_Server *server = native;
  UA_StatusCode status = UA_STATUSCODE_GOOD;
  if (state->read_result) {
    UA_DataValue *value = state->read_result;
    UA_Int64 number = INT64_MIN;
    value->hasValue = true;
    value->hasSourceTimestamp = true;
    value->sourceTimestamp = INT64_MAX;
    status = UA_Variant_setScalarCopy(&value->value, &number,
                                      &UA_TYPES[UA_TYPES_INT64]);
    if (!status)
      status = UA_Server_setAsyncReadResult(server, value);
    state->read_result = NULL;
  }
  if (!status && state->write_result) {
    status = UA_Server_setAsyncWriteResult(server, state->write_result,
                                           UA_STATUSCODE_GOOD);
    state->write_result = NULL;
  }
  if (!status && state->call_result) {
    UA_Variant *outputs = state->call_result;
    UA_Int64 number = INT64_MIN;
    status = UA_Variant_setScalarCopy(&outputs[0], &number,
                                      &UA_TYPES[UA_TYPES_INT64]);
    number = INT64_MAX;
    if (!status)
      status = UA_Variant_setScalarCopy(&outputs[1], &number,
                                        &UA_TYPES[UA_TYPES_INT64]);
    if (!status)
      status = UA_Server_setAsyncCallMethodResult(server, outputs,
                                                  UA_STATUSCODE_GOOD);
    state->call_result = NULL;
  }
  return status;
}

static const UA_DataType *peer_utility_type(size_t type) {
  return type == 1   ? &UA_TYPES[UA_TYPES_INT64]
         : type == 2 ? &UA_TYPES[UA_TYPES_STRING]
                     : NULL;
}
unsigned int cpkt_types_peer_variant_predicates(size_t type, size_t query,
                                                size_t length, int state) {
  UA_Variant value;
  UA_Int64 number = 0;
  UA_Variant_init(&value);
  value.type = peer_utility_type(type);
  value.arrayLength = length;
  value.data = state == 2   ? (void *)&number
               : state == 1 ? UA_EMPTY_ARRAY_SENTINEL
                            : NULL;
  return UA_Variant_isEmpty(&value) | (UA_Variant_isScalar(&value) << 1) |
         (UA_Variant_isArray(&value) << 2) |
         (UA_Variant_hasScalarType(&value, peer_utility_type(query)) << 3) |
         (UA_Variant_hasArrayType(&value, peer_utility_type(query)) << 4);
}
unsigned int cpkt_types_peer_extension_predicate(int encoding, size_t type,
                                                 size_t query, int populated) {
  UA_ExtensionObject value;
  UA_Int64 number = 0;
  UA_ExtensionObject_init(&value);
  value.encoding = (UA_ExtensionObjectEncoding)encoding;
  if (encoding >= 3)
    value.content.decoded.type = peer_utility_type(type);
  if (encoding >= 3 && populated)
    value.content.decoded.data = &number;
  return UA_ExtensionObject_hasDecodedType(&value, peer_utility_type(query));
}
unsigned int cpkt_types_peer_bytes_hash(unsigned int initial,
                                        const unsigned char *bytes,
                                        size_t length) {
  return UA_ByteString_hash(initial, bytes, length);
}
unsigned int cpkt_types_peer_status_predicates(unsigned int status) {
  return UA_StatusCode_isGood(status) |
         (UA_StatusCode_isUncertain(status) << 1) |
         (UA_StatusCode_isBad(status) << 2);
}

/* Independent native parser/printer and binary encoder, with matching borrowed
 * URI fixtures. No C89 facade headers or conversion code participate. */
void cpkt_types_peer_identifier(int kind, int extended, const char *text,
                                size_t capacity,
                                struct cpkt_identifier_peer *result) {
  UA_NodeId node = UA_NODEID_NULL, null_node = UA_NODEID_NULL;
  UA_ExpandedNodeId expanded = UA_EXPANDEDNODEID_NULL,
                    null_expanded = UA_EXPANDEDNODEID_NULL;
  UA_QualifiedName name;
  UA_Guid guid = UA_GUID_NULL;
  UA_String uris[2] = {UA_STRING("urn:local"), UA_STRING("urn:widgets")};
  UA_String servers[2] = {UA_STRING("urn:server:local"),
                          UA_STRING("urn:server:remote")};
  UA_NamespaceMapping mapping;
  UA_String input = UA_STRING((char *)text), output;
  UA_ByteString encoded = UA_BYTESTRING_NULL;
  const UA_NamespaceMapping *map = extended ? &mapping : NULL;
  const UA_DataType *type;
  void *value;
  UA_QualifiedName_init(&name);
  memset(&mapping, 0, sizeof(mapping));
  mapping.namespaceUris = uris;
  mapping.namespaceUrisSize = 2;
  memset(result, 0, sizeof(*result));
  memset(result->output, 0xa5, sizeof(result->output));
  output.length = capacity;
  output.data = capacity ? result->output : NULL;
  switch (kind) {
  case 0:
    result->parse_status = extended ? UA_NodeId_parseEx(&node, input, map)
                                    : UA_NodeId_parse(&node, input);
    result->print_status = extended ? UA_NodeId_printEx(&node, &output, map)
                                    : UA_NodeId_print(&node, &output);
    result->hash = UA_NodeId_hash(&node);
    result->predicate = UA_NodeId_isNull(&node);
    result->order = UA_NodeId_order(&node, &null_node);
    value = &node;
    type = &UA_TYPES[UA_TYPES_NODEID];
    break;
  case 1:
    result->parse_status =
        extended ? UA_ExpandedNodeId_parseEx(&expanded, input, map, 2, servers)
                 : UA_ExpandedNodeId_parse(&expanded, input);
    result->print_status =
        extended
            ? UA_ExpandedNodeId_printEx(&expanded, &output, map, 2, servers)
            : UA_ExpandedNodeId_print(&expanded, &output);
    result->hash = UA_ExpandedNodeId_hash(&expanded);
    result->predicate = UA_ExpandedNodeId_isLocal(&expanded);
    result->order = UA_ExpandedNodeId_order(&expanded, &null_expanded);
    value = &expanded;
    type = &UA_TYPES[UA_TYPES_EXPANDEDNODEID];
    break;
  case 2:
    result->parse_status = extended
                               ? UA_QualifiedName_parseEx(&name, input, map)
                               : UA_QualifiedName_parse(&name, input);
    result->print_status = extended
                               ? UA_QualifiedName_printEx(&name, &output, map)
                               : UA_QualifiedName_print(&name, &output);
    result->hash = UA_QualifiedName_hash(&name);
    result->predicate = UA_QualifiedName_isNull(&name);
    value = &name;
    type = &UA_TYPES[UA_TYPES_QUALIFIEDNAME];
    break;
  default:
    result->parse_status = UA_Guid_parse(&guid, input);
    result->print_status = UA_Guid_print(&guid, &output);
    value = &guid;
    type = &UA_TYPES[UA_TYPES_GUID];
    break;
  }
  result->output_length = output.length;
  if (!capacity && output.length)
    memcpy(result->output, output.data, output.length);
  if (!capacity)
    UA_String_clear(&output);
  if (!result->parse_status && !UA_encodeBinary(value, type, &encoded, NULL)) {
    result->encoded = encoded.data;
    result->encoded_length = encoded.length;
  }
  UA_clear(value, type);
}
void cpkt_types_peer_time(unsigned int high, unsigned int low,
                          unsigned int *unix_high, unsigned int *unix_low,
                          unsigned short *fields, short *year) {
  UA_UInt64 bits = ((UA_UInt64)high << 32) | low;
  UA_DateTime native;
  UA_DateTimeStruct calendar;
  UA_Int64 seconds;
  memcpy(&native, &bits, sizeof(native));
  seconds = UA_DateTime_toUnixTime(native);
  memcpy(&bits, &seconds, sizeof(bits));
  *unix_high = (unsigned int)(bits >> 32);
  *unix_low = (unsigned int)bits;
  calendar = UA_DateTime_toStruct(native);
  fields[0] = calendar.nanoSec;
  fields[1] = calendar.microSec;
  fields[2] = calendar.milliSec;
  fields[3] = calendar.sec;
  fields[4] = calendar.min;
  fields[5] = calendar.hour;
  fields[6] = calendar.day;
  fields[7] = calendar.month;
  *year = calendar.year;
}

void cpkt_types_peer_offset(unsigned int *high, unsigned int *low) {
  UA_Int64 offset = UA_DateTime_localTimeUtcOffset();
  UA_UInt64 bits;
  memcpy(&bits, &offset, sizeof(bits));
  *high = (unsigned int)(bits >> 32);
  *low = (unsigned int)bits;
}
void cpkt_types_peer_random(unsigned int high, unsigned int low,
                            unsigned char *guid, unsigned int *number) {
  UA_Guid native;
  UA_random_seed_deterministic(((UA_UInt64)high << 32) | low);
  native = UA_Guid_random();
  memcpy(guid, &native, sizeof(native));
  *number = UA_UInt32_random();
}

/* Drive callback slots using the installed native SDK, including native
 * node operations. No facade callback structures are included here. */
unsigned int cpkt_types_peer_nodes(void *arg, int action, void *context) {
  UA_Server *server = arg;
  UA_ServerConfig *config = UA_Server_getConfig(server);
  UA_Nodestore *store = config->nodestore;
  UA_NodeId variable = UA_NODEID_NUMERIC(1, 7000),
            type = UA_NODEID_NUMERIC(1, 7001);
  UA_NodeId session = UA_NODEID_STRING(2, "session"),
            instance = UA_NODEID_STRING(1, "instance");
  UA_NodeId parent = UA_NODEID_STRING(1, "parent"),
            reference = UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT);
  UA_ReferenceTypeSet references;
  const UA_Node *node;
  UA_GlobalNodeLifecycle global;
  UA_NodeTypeLifecycle lifecycle;
  UA_ValueSourceNotifications notifications;
  UA_DataValue value;
  UA_Int64 number = INT64_MIN;
  UA_NumericRangeDimension dimension = {1, 1};
  UA_NumericRange range = {1, &dimension};
  UA_StatusCode status = 0;
  void *mutable = context;
  memset(&references, 0, sizeof(references));
  UA_DataValue_init(&value);
  if (action == 0) {
    UA_VariableAttributes va = UA_VariableAttributes_default;
    UA_ObjectTypeAttributes oa = UA_ObjectTypeAttributes_default;
    va.dataType = UA_NODEID_NUMERIC(0, UA_NS0ID_INT64);
    va.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
    UA_Variant_setScalar(&va.value, &number, &UA_TYPES[UA_TYPES_INT64]);
    status = UA_Server_addVariableNode(
        server, variable, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
        UA_QUALIFIEDNAME(1, "notified"),
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), va, context, NULL);
    if (!status)
      status = UA_Server_addObjectTypeNode(
          server, type, UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
          UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
          UA_QUALIFIEDNAME(1, "lifecycle"), oa, context, NULL);
    return status;
  }
  if (action == 1 || action == 2) {
    node = store->getNode(store, &variable, 0xffffffffU, references,
                          UA_BROWSEDIRECTION_BOTH);
    if (!node)
      return UA_STATUSCODE_BADNODEIDUNKNOWN;
    notifications = node->variableNode.valueSource.internal.notifications;
    store->releaseNode(store, node);
    value.hasValue = 1;
    UA_Variant_setScalar(&value.value, &number, &UA_TYPES[UA_TYPES_INT64]);
    value.value.storageType = UA_VARIANT_DATA_NODELETE;
    value.hasSourceTimestamp = 1;
    value.sourceTimestamp = INT64_MAX;
    value.hasServerTimestamp = 1;
    value.serverTimestamp = INT64_MIN;
    value.hasStatus = 1;
    value.status = UA_STATUSCODE_GOODCLAMPED;
    value.hasSourcePicoseconds = 1;
    value.sourcePicoseconds = 321;
    value.hasServerPicoseconds = 1;
    value.serverPicoseconds = 654;
    if (action == 1) {
      if (notifications.onRead)
        notifications.onRead(server, &session, context, &variable, context,
                             &range, &value);
    } else if (notifications.onWrite)
      notifications.onWrite(server, &session, context, &variable, context, NULL,
                            &value);
    return 0;
  }
  if (action >= 3 && action <= 6) {
    if (!config->nodeLifecycle)
      return UA_STATUSCODE_BADNOTFOUND;
    global = *config->nodeLifecycle;
    if (action == 3) {
      if (global.constructor)
        status =
            global.constructor(server, &session, context, &instance, &mutable);
    }
    if (action == 4) {
      if (global.destructor)
        global.destructor(server, &session, context, &instance, context);
    }
    if (action == 5) {
      if (!global.createOptionalChild ||
          !global.createOptionalChild(server, &session, context, &instance,
                                      &parent, &reference))
        status = UA_STATUSCODE_BADNOTFOUND;
    }
    if (action == 6) {
      UA_NodeId output = UA_NODEID_NUMERIC(1, 0);
      if (global.generateChildNodeId)
        status = global.generateChildNodeId(
            server, &session, context, &instance, &parent, &reference, &output);
      if (!status &&
          (output.identifierType != UA_NODEIDTYPE_STRING ||
           output.namespaceIndex != 1 || output.identifier.string.length != 5))
        status = UA_STATUSCODE_BADINTERNALERROR;
      UA_NodeId_clear(&output);
    }
    if (action == 3 && !status && mutable == context)
      status = UA_STATUSCODE_BADINTERNALERROR;
    return status;
  }
  if (action == 7 || action == 8) {
    node = store->getNode(store, &type, 0xffffffffU, references,
                          UA_BROWSEDIRECTION_BOTH);
    if (!node)
      return UA_STATUSCODE_BADNODEIDUNKNOWN;
    lifecycle = node->objectTypeNode.lifecycle;
    store->releaseNode(store, node);
    if (action == 7) {
      if (lifecycle.constructor)
        status = lifecycle.constructor(server, &session, context, &type,
                                       context, &instance, &mutable);
    } else if (lifecycle.destructor)
      lifecycle.destructor(server, &session, context, &type, context, &instance,
                           &mutable);
    if (!status && mutable == context)
      status = UA_STATUSCODE_BADINTERNALERROR;
    return status;
  }
  if (action == 9) {
    UA_Variant result;
    UA_Variant_init(&result);
    status = UA_Server_readValue(server, variable, &result);
    UA_Variant_clear(&result);
    return status;
  }
  if (action == 10) {
    UA_Variant result;
    UA_Variant_init(&result);
    UA_Variant_setScalar(&result, &number, &UA_TYPES[UA_TYPES_INT64]);
    return UA_Server_writeValue(server, variable, result);
  }
  if (action == 11) {
    UA_ObjectAttributes oa = UA_ObjectAttributes_default;
    return UA_Server_addObjectNode(server, UA_NODEID_NUMERIC(1, 7004),
                                   UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                   UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                   UA_QUALIFIEDNAME(1, "constructed"), type, oa,
                                   context, NULL);
  }
  if (action == 12)
    return UA_Server_deleteNode(server, UA_NODEID_NUMERIC(1, 7004), UA_TRUE);
  if (action == 13) {
    node = store->getNode(store, &type, 0xffffffffU, references,
                          UA_BROWSEDIRECTION_BOTH);
    if (!node)
      return UA_STATUSCODE_BADNODEIDUNKNOWN;
    lifecycle = node->objectTypeNode.lifecycle;
    store->releaseNode(store, node);
    return !lifecycle.constructor && !lifecycle.destructor &&
                   !config->nodeLifecycle
               ? 0
               : UA_STATUSCODE_BADINTERNALERROR;
  }
  return UA_STATUSCODE_BADINVALIDARGUMENT;
}
struct cpkt_nodes_thread {
  void *server;
  unsigned int status;
};
static void *cpkt_nodes_thread_work(void *arg) {
  struct cpkt_nodes_thread *state = arg;
  size_t i;
  for (i = 0; i < 8 && !state->status; ++i) {
    state->status = cpkt_types_peer_nodes(state->server, 9, NULL);
    if (!state->status)
      state->status = cpkt_types_peer_nodes(state->server, 10, NULL);
  }
  return NULL;
}
unsigned int cpkt_types_peer_nodes_parallel(void *first, void *second) {
  struct cpkt_nodes_thread states[2] = {{first, 0}, {second, 0}};
  pthread_t threads[2];
  if (pthread_create(&threads[0], NULL, cpkt_nodes_thread_work, &states[0]))
    return UA_STATUSCODE_BADINTERNALERROR;
  if (pthread_create(&threads[1], NULL, cpkt_nodes_thread_work, &states[1])) {
    (void)pthread_join(threads[0], NULL);
    return UA_STATUSCODE_BADINTERNALERROR;
  }
  (void)pthread_join(threads[0], NULL);
  (void)pthread_join(threads[1], NULL);
  return states[0].status ? states[0].status : states[1].status;
}

void cpkt_types_peer_maps(unsigned int *results) {
  UA_KeyValueMap map = UA_KEYVALUEMAP_NULL, other = UA_KEYVALUEMAP_NULL;
  UA_KeyValueMap copy = UA_KEYVALUEMAP_NULL;
  UA_QualifiedName key = UA_QUALIFIEDNAME(1, "option");
  UA_QualifiedName second = UA_QUALIFIEDNAME(2, "option");
  UA_Int64 value = (UA_Int64)0x8000000000000001ULL;
  const UA_Variant *borrowed;
  UA_Variant array;
  results[0] = UA_KeyValueMap_isEmpty(NULL);
  results[1] = UA_KeyValueMap_contains(NULL, key);
  results[2] = UA_KeyValueMap_remove(NULL, key);
  results[3] = UA_KeyValueMap_remove(&map, key);
  results[4] = UA_KeyValueMap_setScalarShallow(&map, key, &value,
                                               &UA_TYPES[UA_TYPES_INT64]);
  borrowed = UA_KeyValueMap_get(&map, key);
  results[5] = borrowed && borrowed->data == &value;
  results[6] = borrowed ? (unsigned int)borrowed->storageType : 99;
  results[7] = UA_KeyValueMap_contains(&map, second);
  results[8] = UA_KeyValueMap_copy(&map, &copy);
  results[9] =
      UA_KeyValueMap_getScalar(&copy, key, &UA_TYPES[UA_TYPES_INT64]) != &value;
  results[10] = UA_KeyValueMap_setScalar(&other, second, &value,
                                         &UA_TYPES[UA_TYPES_INT64]);
  results[11] = UA_KeyValueMap_merge(&map, &other);
  results[12] = (unsigned int)map.mapSize;
  results[13] = UA_KeyValueMap_remove(&map, key);
  results[14] = UA_QualifiedName_equal(&map.map[0].key, &second);
  results[15] = UA_KeyValueMap_remove(&map, second);
  results[16] = map.map == UA_EMPTY_ARRAY_SENTINEL;
  UA_Variant_init(&array);
  UA_Variant_setArray(&array, &value, 1, &UA_TYPES[UA_TYPES_INT64]);
  array.storageType = UA_VARIANT_DATA_NODELETE;
  results[17] = UA_KeyValueMap_setShallow(&map, key, &array);
  results[18] =
      UA_KeyValueMap_getScalar(&map, key, &UA_TYPES[UA_TYPES_INT64]) == NULL;
  results[19] = UA_KeyValueMap_set(NULL, key, &array);
  results[20] =
      UA_KeyValueMap_setScalar(&map, key, NULL, &UA_TYPES[UA_TYPES_INT64]);
  results[21] = UA_KeyValueMap_merge(NULL, &map);
  results[22] = UA_KeyValueMap_merge(&map, NULL);
  UA_KeyValueMap_clear(&copy);
  results[23] = UA_KeyValueMap_copy(NULL, &copy);
  UA_KeyValueMap_clear(&map);
  UA_KeyValueMap_clear(&other);
  UA_KeyValueMap_clear(&copy);
}

unsigned int
cpkt_types_peer_producer_direct(void *native, int kind, void *context,
                                struct cpkt_producer_observed *out) {
  UA_Server *server = native;
  UA_NodeId session = UA_NODEID_STRING(2, "session");
  UA_NodeId variable = UA_NODEID_NUMERIC(1, 6200),
            method = UA_NODEID_NUMERIC(1, 6201);
  UA_NodeId object = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
  UA_NumericRangeDimension dimensions[2] = {{1, 3}, {2, 4}};
  UA_NumericRange range = {2, dimensions};
  UA_DataValue value;
  UA_MethodCallback callback = NULL;
  UA_StatusCode status;
  UA_CallbackValueSource source;
  const UA_Node *node;
  UA_Variant input[2], output[2];
  UA_Int64 minimum = INT64_MIN;
  UA_String string = {3, (UA_Byte *)"a\0b"};
  size_t i;
  memset(out, 0, sizeof(*out));
  UA_DataValue_init(&value);
  if (kind < 2) {
    UA_ServerConfig *config = UA_Server_getConfig(server);
    node = config->nodestore->getNode(config->nodestore, &variable, 0xffffffffU,
                                      UA_REFERENCETYPESET_ALL,
                                      UA_BROWSEDIRECTION_BOTH);
    if (!node || node->head.nodeClass != UA_NODECLASS_VARIABLE)
      return UA_STATUSCODE_BADNODEIDUNKNOWN;
    source = node->variableNode.valueSource.callback;
    config->nodestore->releaseNode(config->nodestore, node);
    if (kind == 1) {
      UA_Variant_setScalar(&value.value, &minimum, &UA_TYPES[UA_TYPES_INT64]);
      value.value.storageType = UA_VARIANT_DATA_NODELETE;
      value.hasValue = true;
      value.hasStatus = true;
      value.status = UA_STATUSCODE_UNCERTAINSUBNORMAL;
      value.hasSourceTimestamp = true;
      value.sourceTimestamp = INT64_MAX;
      value.hasSourcePicoseconds = true;
      value.sourcePicoseconds = 123;
      status = source.write(server, &session, context, &variable, context,
                            &range, &value);
    } else
      status = source.read(server, &session, context, &variable, context, true,
                           &range, &value);
    if (!status && kind == 0 && value.hasValue &&
        UA_Variant_hasScalarType(&value.value, &UA_TYPES[UA_TYPES_INT64])) {
      UA_UInt64 bits = (UA_UInt64) * (UA_Int64 *)value.value.data;
      out->high[0] = (unsigned int)(bits >> 32);
      out->low[0] = (unsigned int)bits;
      out->address = value.value.data;
      out->borrowed = value.value.storageType == UA_VARIANT_DATA_NODELETE;
      if (!value.hasSourceTimestamp || value.sourceTimestamp != INT64_MAX ||
          !value.hasStatus ||
          value.status != UA_STATUSCODE_UNCERTAINSUBNORMAL ||
          !value.hasSourcePicoseconds || value.sourcePicoseconds != 123)
        status = UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    UA_DataValue_clear(&value);
    return status;
  }
  status = UA_Server_getMethodNodeCallback(server, method, &callback);
  if (status || !callback)
    return status ? status : UA_STATUSCODE_BADNOTSUPPORTED;
  memset(input, 0, sizeof(input));
  memset(output, 0, sizeof(output));
  UA_Variant_setScalar(&input[0], &minimum, &UA_TYPES[UA_TYPES_INT64]);
  input[0].storageType = UA_VARIANT_DATA_NODELETE;
  UA_Variant_setScalar(&input[1], &string, &UA_TYPES[UA_TYPES_STRING]);
  input[1].storageType = UA_VARIANT_DATA_NODELETE;
  status = callback(server, &session, context, &method, context, &object,
                    context, 2, input, 2, output);
  out->address = output[0].data;
  out->borrowed = output[0].storageType == UA_VARIANT_DATA_NODELETE;
  for (i = 0; i < 2; ++i) {
    if (!status &&
        UA_Variant_hasScalarType(&output[i], &UA_TYPES[UA_TYPES_INT64])) {
      UA_UInt64 bits = (UA_UInt64) * (UA_Int64 *)output[i].data;
      out->high[i] = (unsigned int)(bits >> 32);
      out->low[i] = (unsigned int)bits;
    }
    UA_Variant_clear(&output[i]);
  }
  return status;
}

unsigned int cpkt_types_peer_producer_zero_methods(void *native,
                                                   void *context) {
  UA_Server *server = native;
  UA_MethodAttributes attr = UA_MethodAttributes_default;
  UA_StatusCode status = 0;
  unsigned int i;
  attr.executable = attr.userExecutable = true;
  for (i = 0; i < 2 && !status; ++i)
    status = UA_Server_addMethodNode(
        server, UA_NODEID_NUMERIC(1, 6202 + i),
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
        UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
        UA_QUALIFIEDNAME(1, i ? "zeroSecond" : "zeroFirst"), attr,
        peer_async_method, 0, NULL, 0, NULL, context, NULL);
  return status;
}

static UA_StatusCode
creation_failure_read(UA_Server *server, const UA_NodeId *session,
                      void *session_context, const UA_NodeId *id, void *context,
                      UA_Boolean timestamp, const UA_NumericRange *range,
                      UA_DataValue *value) {
  UA_Int64 number = INT64_MIN;
  (void)server;
  (void)session;
  (void)session_context;
  (void)id;
  (void)context;
  (void)timestamp;
  (void)range;
  value->hasValue = true;
  return UA_Variant_setScalarCopy(&value->value, &number,
                                  &UA_TYPES[UA_TYPES_INT64]);
}
void cpkt_types_peer_creation_failure(int kind,
                                      struct cpkt_creation_observed *out) {
  UA_Server *server = UA_Server_new();
  UA_NodeId id = UA_NODEID_NULL;
  void *context = NULL;
  memset(out, 0, sizeof(*out));
  if (!server)
    abort();
  if (UA_ServerConfig_setMinimal(UA_Server_getConfig(server), 0, NULL))
    abort();
  if (!kind) {
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    UA_CallbackValueSource source = {creation_failure_read, NULL};
    UA_Int64 number = INT64_MIN;
    attr.dataType = UA_NS0ID(INT64);
    attr.valueRank = -1;
    attr.accessLevel = 1;
    UA_Variant_setScalar(&attr.value, &number, &UA_TYPES[UA_TYPES_INT64]);
    out->status = UA_Server_addCallbackValueSourceVariableNode(
        server, UA_NODEID_STRING(1, "badVariable"),
        UA_NODEID_NUMERIC(1, 9999999), UA_NS0ID(ORGANIZES),
        UA_QUALIFIEDNAME(1, "creation"), UA_NS0ID(BASEDATAVARIABLETYPE), attr,
        source, NULL, &id);
  } else {
    UA_MethodAttributes attr = UA_MethodAttributes_default;
    UA_Argument argument;
    UA_Argument_init(&argument);
    argument.dataType = UA_NS0ID(INT64);
    argument.valueRank = -1;
    attr.executable = true;
    attr.userExecutable = true;
    out->status = UA_Server_addMethodNode(
        server, UA_NODEID_STRING(1, "badMethod"), UA_NODEID_NUMERIC(1, 9999999),
        UA_NS0ID(HASCOMPONENT), UA_QUALIFIEDNAME(1, "creation"), attr, NULL, 1,
        &argument, 1, &argument, NULL, &id);
  }
  out->assigned = !UA_NodeId_isNull(&id);
  out->exists = !UA_Server_getNodeContext(server, id, &context);
  UA_NodeId_clear(&id);
  UA_Server_delete(server);
}

/* Inspect native borrowed storage through the public nodestore plugin API.
 * No facade layouts or pointer slot representation are known to this peer. */
unsigned int cpkt_types_peer_external(void *handle, unsigned int id,
                                      struct cpkt_external_observed *out) {
  UA_Server *server = handle;
  UA_Nodestore *store = UA_Server_getConfig(server)->nodestore;
  UA_NodeId nodeId = UA_NODEID_NUMERIC(1, id);
  const UA_Node *node =
      store->getNode(store, &nodeId, UA_NODEATTRIBUTESMASK_VALUE,
                     UA_REFERENCETYPESET_NONE, UA_BROWSEDIRECTION_INVALID);
  if (!node)
    return UA_STATUSCODE_BADNODEIDUNKNOWN;
  const UA_VariableNode *variable = (const UA_VariableNode *)node;
  UA_StatusCode status = UA_STATUSCODE_BADTYPEMISMATCH;
  if (node->head.nodeClass == UA_NODECLASS_VARIABLE &&
      variable->valueSourceType == UA_VALUESOURCETYPE_EXTERNAL) {
    UA_DataValue *value = *variable->valueSource.external.value;
    if (value && value->hasValue &&
        value->value.type == &UA_TYPES[UA_TYPES_INT64] &&
        value->value.arrayLength == 2) {
      memset(out, 0, sizeof(*out));
      out->slot = variable->valueSource.external.value;
      out->storage = value;
      out->data = value->value.data;
      for (size_t i = 0; i < 2; ++i) {
        UA_UInt64 bits = (UA_UInt64)((UA_Int64 *)value->value.data)[i];
        out->high[i] = (unsigned int)(bits >> 32);
        out->low[i] = (unsigned int)bits;
      }
      status = UA_STATUSCODE_GOOD;
    }
  }
  store->releaseNode(store, node);
  if (status == UA_STATUSCODE_GOOD) {
    UA_Node *copy = NULL;
    status = store->getNodeCopy(store, &nodeId, &copy);
    if (!status) {
      const UA_VariableNode *copied = (const UA_VariableNode *)copy;
      if (copied->valueSourceType != UA_VALUESOURCETYPE_EXTERNAL ||
          copied->valueSource.external.value != out->slot)
        status = UA_STATUSCODE_BADINTERNALERROR;
      store->deleteNode(store, copy);
    }
  }
  return status;
}

static UA_StatusCode
highlevel_echo(UA_Server *server, const UA_NodeId *session,
               void *sessionContext, const UA_NodeId *method,
               void *methodContext, const UA_NodeId *object,
               void *objectContext, size_t inputSize, const UA_Variant *input,
               size_t outputSize, UA_Variant *output) {
  (void)server;
  (void)session;
  (void)sessionContext;
  (void)methodContext;
  (void)object;
  (void)objectContext;
  if (method->identifier.numeric == 6101)
    return inputSize || outputSize ? UA_STATUSCODE_BADINTERNALERROR : 0;
  if (inputSize != 1 || outputSize != 1)
    return UA_STATUSCODE_BADINTERNALERROR;
  UA_StatusCode status = UA_Variant_copy(input, output);
  return status ? status : UA_STATUSCODE_GOODCLAMPED;
}
/* Independent native page producer. Values/date limits are deliberately exact
 * 64-bit extrema; the client must preserve each page rather than collect them.
 */
static void highlevel_history_page(UA_HistoryReadResponse *response,
                                   const UA_HistoryReadValueId *read,
                                   UA_Boolean release, size_t count,
                                   UA_DataValue *value) {
  if (count != 1 || read->nodeId.namespaceIndex != 1 ||
      read->nodeId.identifier.numeric != 6001)
    abort();
  UA_HistoryReadResult *result = &response->results[0];
  if (release)
    return;
  UA_Int64 integer = read->continuationPoint.length ? INT64_MAX : INT64_MIN;
  value->hasValue = true;
  value->hasSourceTimestamp = true;
  value->sourceTimestamp = integer;
  if (UA_Variant_setScalarCopy(&value->value, &integer,
                               &UA_TYPES[UA_TYPES_INT64]))
    abort();
  if (!read->continuationPoint.length)
    result->continuationPoint = UA_BYTESTRING_ALLOC("next");
}
#define HISTORY_ARGS(detailsType, payloadType)                                 \
  UA_Server *server, void *context, const UA_NodeId *session,                  \
      void *sessionContext, const UA_RequestHeader *header,                    \
      const detailsType *details, UA_TimestampsToReturn timestamps,            \
      UA_Boolean release, size_t count, const UA_HistoryReadValueId *read,     \
      UA_HistoryReadResponse *response, payloadType *const *const payload
#define IGNORE_HISTORY_ARGS()                                                  \
  do {                                                                         \
    (void)server;                                                              \
    (void)session;                                                             \
    (void)sessionContext;                                                      \
    (void)header;                                                              \
    (void)timestamps;                                                          \
    (void)context;                                                             \
  } while (0)
static void highlevel_raw(HISTORY_ARGS(UA_ReadRawModifiedDetails,
                                       UA_HistoryData)) {
  IGNORE_HISTORY_ARGS();
  struct peer *peer = context;
  if (release)
    ++peer->history_releases[0];
  else
    ++peer->history_pages[0];
  if (details->startTime != INT64_MIN || details->endTime != INT64_MAX ||
      details->isReadModified || details->returnBounds ||
      details->numValuesPerNode != 1)
    abort();
  UA_DataValue value;
  UA_DataValue_init(&value);
  highlevel_history_page(response, read, release, count, &value);
  if (!release) {
    payload[0]->dataValues = UA_DataValue_new();
    if (!payload[0]->dataValues)
      abort();
    *payload[0]->dataValues = value;
    payload[0]->dataValuesSize = 1;
  }
}
static void highlevel_modified(HISTORY_ARGS(UA_ReadRawModifiedDetails,
                                            UA_HistoryModifiedData)) {
  IGNORE_HISTORY_ARGS();
  struct peer *peer = context;
  if (release)
    ++peer->history_releases[1];
  else
    ++peer->history_pages[1];
  if (details->startTime != INT64_MIN || details->endTime != INT64_MAX ||
      !details->isReadModified || details->returnBounds ||
      details->numValuesPerNode != 1)
    abort();
  UA_DataValue value;
  UA_DataValue_init(&value);
  highlevel_history_page(response, read, release, count, &value);
  if (!release) {
    payload[0]->dataValues = UA_DataValue_new();
    if (!payload[0]->dataValues)
      abort();
    *payload[0]->dataValues = value;
    payload[0]->dataValuesSize = 1;
    payload[0]->modificationInfos = UA_ModificationInfo_new();
    if (!payload[0]->modificationInfos)
      abort();
    payload[0]->modificationInfosSize = 1;
    payload[0]->modificationInfos[0].modificationTime = INT64_MAX;
    payload[0]->modificationInfos[0].updateType = UA_HISTORYUPDATETYPE_REPLACE;
  }
}
static void highlevel_events(HISTORY_ARGS(UA_ReadEventDetails,
                                          UA_HistoryEvent)) {
  IGNORE_HISTORY_ARGS();
  struct peer *peer = context;
  if (release)
    ++peer->history_releases[2];
  else
    ++peer->history_pages[2];
  if (details->startTime != INT64_MIN || details->endTime != INT64_MAX ||
      details->numValuesPerNode != 1 ||
      details->filter.selectClausesSize != 1 ||
      details->filter.selectClauses[0].attributeId != UA_ATTRIBUTEID_VALUE)
    abort();
  UA_DataValue value;
  UA_DataValue_init(&value);
  highlevel_history_page(response, read, release, count, &value);
  if (!release) {
    payload[0]->events = UA_HistoryEventFieldList_new();
    if (!payload[0]->events)
      abort();
    payload[0]->eventsSize = 1;
    payload[0]->events[0].eventFields = UA_Variant_new();
    if (!payload[0]->events[0].eventFields)
      abort();
    payload[0]->events[0].eventFieldsSize = 1;
    *payload[0]->events[0].eventFields = value.value;
  }
}
static void highlevel_update(UA_Server *server, void *context,
                             const UA_NodeId *session, void *sessionContext,
                             const UA_RequestHeader *header,
                             const UA_UpdateDataDetails *details,
                             UA_HistoryUpdateResult *result) {
  (void)server;
  (void)context;
  (void)session;
  (void)sessionContext;
  (void)header;
  if (details->nodeId.identifier.numeric != 6001 ||
      details->updateValuesSize != 1 || !details->updateValues[0].hasValue ||
      details->updateValues[0].value.type != &UA_TYPES[UA_TYPES_INT64] ||
      *(UA_Int64 *)details->updateValues[0].value.data != INT64_MIN ||
      details->performInsertReplace < UA_PERFORMUPDATETYPE_INSERT ||
      details->performInsertReplace > UA_PERFORMUPDATETYPE_UPDATE)
    abort();
  result->operationResults = UA_StatusCode_new();
  if (!result->operationResults)
    abort();
  result->operationResultsSize = 1;
  *result->operationResults = UA_STATUSCODE_GOODCLAMPED;
}
static void highlevel_delete_history(UA_Server *server, void *context,
                                     const UA_NodeId *session,
                                     void *sessionContext,
                                     const UA_RequestHeader *header,
                                     const UA_DeleteRawModifiedDetails *details,
                                     UA_HistoryUpdateResult *result) {
  (void)server;
  (void)context;
  (void)session;
  (void)sessionContext;
  (void)header;
  if (details->nodeId.identifier.numeric != 6001 ||
      details->startTime != INT64_MIN || details->endTime != INT64_MAX ||
      details->isDeleteModified)
    abort();
  result->statusCode = UA_STATUSCODE_GOODCLAMPED;
}
unsigned int cpkt_types_peer_highlevel_setup(void *arg) {
  struct peer *peer = arg;
  pthread_mutex_lock(&peer->lock);
  UA_Server *server = peer->server;
  UA_Argument argument;
  UA_Argument_init(&argument);
  argument.dataType = UA_TYPES[UA_TYPES_INT64].typeId;
  argument.valueRank = -2;
  UA_MethodAttributes attr = UA_MethodAttributes_default;
  attr.writeMask = UINT32_MAX;
  UA_StatusCode status = UA_Server_addMethodNode(
      server, UA_NODEID_NUMERIC(1, 6100), UA_NS0ID(OBJECTSFOLDER),
      UA_NS0ID(HASCOMPONENT), UA_QUALIFIEDNAME(1, "echo"), attr, highlevel_echo,
      1, &argument, 1, &argument, NULL, NULL);
  if (!status)
    status = UA_Server_addMethodNode(
        server, UA_NODEID_NUMERIC(1, 6101), UA_NS0ID(OBJECTSFOLDER),
        UA_NS0ID(HASCOMPONENT), UA_QUALIFIEDNAME(1, "nooutputs"), attr,
        highlevel_echo, 0, NULL, 0, NULL, NULL, NULL);
  if (!status) {
    UA_ServerConfig *config = UA_Server_getConfig(server);
    UA_HistoryDatabase *history = &config->historyDatabase;
    if (history->clear)
      history->clear(history);
    memset(history, 0, sizeof(*history));
    history->context = peer;
    history->readRaw = highlevel_raw;
    history->readModified = highlevel_modified;
    history->readEvent = highlevel_events;
    history->updateData = highlevel_update;
    history->deleteRawModified = highlevel_delete_history;
  }
  pthread_mutex_unlock(&peer->lock);
  return status;
}
#undef HISTORY_ARGS
#undef IGNORE_HISTORY_ARGS

void cpkt_types_peer_highlevel_history_counts(void *arg, unsigned int *pages,
                                              unsigned int *releases) {
  struct peer *peer = arg;
  pthread_mutex_lock(&peer->lock);
  memcpy(pages, peer->history_pages, sizeof(peer->history_pages));
  memcpy(releases, peer->history_releases, sizeof(peer->history_releases));
  pthread_mutex_unlock(&peer->lock);
}

#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
void cpkt_types_fail_after(size_t count);
static int client_failure_kind;
static size_t client_failure_count;
void cpkt_types_peer_client_fail_conversion(int kind, size_t count) {
  client_failure_kind = kind;
  client_failure_count = count;
}
static void client_conversion_arm(int kind, UA_StatusCode status) {
  if (client_failure_kind == kind && !UA_StatusCode_isBad(status)) {
    client_failure_kind = 0;
    cpkt_types_fail_after(client_failure_count);
  }
}
UA_MonitoredItemCreateResult __real_UA_Client_MonitoredItems_createEvent(
    UA_Client *, UA_UInt32, UA_TimestampsToReturn,
    UA_MonitoredItemCreateRequest, void *, UA_Client_EventNotificationCallback,
    UA_Client_DeleteMonitoredItemCallback);
UA_MonitoredItemCreateResult __wrap_UA_Client_MonitoredItems_createEvent(
    UA_Client *client, UA_UInt32 sub, UA_TimestampsToReturn timestamps,
    UA_MonitoredItemCreateRequest item, void *context,
    UA_Client_EventNotificationCallback callback,
    UA_Client_DeleteMonitoredItemCallback deleted) {
  UA_MonitoredItemCreateResult result =
      __real_UA_Client_MonitoredItems_createEvent(client, sub, timestamps, item,
                                                  context, callback, deleted);
  /* Error results may also carry owned filter diagnostics. */
  client_conversion_arm(6, UA_STATUSCODE_GOOD);
  return result;
}
UA_StatusCode __real_UA_Client_readValueAttribute(UA_Client *, UA_NodeId,
                                                  UA_Variant *);
UA_StatusCode __wrap_UA_Client_readValueAttribute(UA_Client *client,
                                                  UA_NodeId id,
                                                  UA_Variant *out) {
  UA_StatusCode status = __real_UA_Client_readValueAttribute(client, id, out);
  client_conversion_arm(1, status);
  return status;
}
UA_StatusCode __real_UA_Client_call(UA_Client *, UA_NodeId, UA_NodeId, size_t,
                                    const UA_Variant *, size_t *,
                                    UA_Variant **);
UA_StatusCode __wrap_UA_Client_call(UA_Client *client, UA_NodeId object,
                                    UA_NodeId method, size_t inputSize,
                                    const UA_Variant *input, size_t *outputSize,
                                    UA_Variant **output) {
  UA_StatusCode status = __real_UA_Client_call(
      client, object, method, inputSize, input, outputSize, output);
  client_conversion_arm(2, status);
  return status;
}
UA_StatusCode __real_UA_Client_addVariableNode(UA_Client *, UA_NodeId,
                                               UA_NodeId, UA_NodeId,
                                               UA_QualifiedName, UA_NodeId,
                                               UA_VariableAttributes,
                                               UA_NodeId *);
UA_StatusCode __wrap_UA_Client_addVariableNode(
    UA_Client *client, UA_NodeId requested, UA_NodeId parent,
    UA_NodeId reference, UA_QualifiedName name, UA_NodeId type,
    UA_VariableAttributes attributes, UA_NodeId *out) {
  UA_StatusCode status = __real_UA_Client_addVariableNode(
      client, requested, parent, reference, name, type, attributes, out);
  client_conversion_arm(3, status);
  return status;
}
struct client_history_failure {
  UA_HistoricalIteratorCallback callback;
  void *context;
};
static UA_Boolean client_history_conversion(UA_Client *client,
                                            const UA_NodeId *id,
                                            UA_Boolean more,
                                            const UA_ExtensionObject *data,
                                            void *context) {
  struct client_history_failure *state = context;
  client_conversion_arm(4, UA_STATUSCODE_GOOD);
  return state->callback(client, id, more, data, state->context);
}
UA_StatusCode __real_UA_Client_HistoryRead_raw(UA_Client *, const UA_NodeId *,
                                               UA_HistoricalIteratorCallback,
                                               UA_DateTime, UA_DateTime,
                                               UA_String, UA_Boolean, UA_UInt32,
                                               UA_TimestampsToReturn, void *);
UA_StatusCode __wrap_UA_Client_HistoryRead_raw(
    UA_Client *client, const UA_NodeId *id,
    UA_HistoricalIteratorCallback callback, UA_DateTime start, UA_DateTime end,
    UA_String range, UA_Boolean bounds, UA_UInt32 count,
    UA_TimestampsToReturn timestamps, void *context) {
  struct client_history_failure state = {callback, context};
  return __real_UA_Client_HistoryRead_raw(client, id, client_history_conversion,
                                          start, end, range, bounds, count,
                                          timestamps, &state);
}
#endif

static void async_output_probe(UA_Client *client, void *user, UA_UInt32 id,
                               UA_AddNodesResponse *response) {
  int *calls = user;
  (void)client;
  if (!id || *calls || response->responseHeader.serviceResult ||
      response->resultsSize != 1 || response->results[0].statusCode ||
      response->results[0].addedNodeId.identifier.numeric != 6599)
    abort();
  ++*calls;
}
static void async_class_probe(UA_Client *client, void *user, UA_UInt32 id,
                              UA_StatusCode status, UA_NodeClass *value) {
  int *calls = user;
  (void)client;
  if (!id || *calls || status != UA_STATUSCODE_BADINTERNALERROR || value)
    abort();
  ++*calls;
}
unsigned int cpkt_types_peer_async_add_output(void *arg) {
  UA_Client *client = arg;
  UA_NodeId ignored = UA_NODEID_NUMERIC(1, 999);
  int calls = 0;
  UA_StatusCode status = UA_Client_addObjectNode_async(
      client, UA_NODEID_NUMERIC(1, 6599), UA_NS0ID(OBJECTSFOLDER),
      UA_NS0ID(ORGANIZES), UA_QUALIFIEDNAME(1, "native-output-probe"),
      UA_NS0ID(BASEOBJECTTYPE), UA_ObjectAttributes_default, &ignored,
      async_output_probe, &calls, NULL);
  for (size_t i = 0; !status && !calls && i < 200; ++i)
    status = UA_Client_run_iterate(client, 10);
  if (!status && (calls != 1 || ignored.namespaceIndex != 1 ||
                  ignored.identifier.numeric != 999))
    status = UA_STATUSCODE_BADINTERNALERROR;
  if (!status)
    status = UA_Client_deleteNode(client, UA_NODEID_NUMERIC(1, 6599), true);
  calls = 0;
  if (!status)
    status = UA_Client_readNodeClassAttribute_async(
        client, UA_NODEID_NUMERIC(1, 0xffffffU), async_class_probe, &calls,
        NULL);
  for (size_t i = 0; !status && !calls && i < 200; ++i)
    status = UA_Client_run_iterate(client, 10);
  if (!status && calls != 1)
    status = UA_STATUSCODE_BADTIMEOUT;
  return status;
}
unsigned int cpkt_types_peer_client_timeout(void *arg, unsigned int timeout) {
  UA_ClientConfig *config = UA_Client_getConfig(arg);
  unsigned int previous = config->timeout;
  config->timeout = timeout;
  return previous;
}
void cpkt_types_peer_pause(void *arg, int pause) {
  struct peer *peer = arg;
  if (pause)
    pthread_mutex_lock(&peer->lock);
  else
    pthread_mutex_unlock(&peer->lock);
}
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
int cpkt_types_fail_stop(void);
static UA_ClientAsyncReadValueAttributeCallback async_conversion_callback;
static void *async_conversion_context;
static void async_conversion_complete(UA_Client *client, void *context,
                                      UA_UInt32 id, UA_StatusCode status,
                                      UA_DataValue *value) {
  UA_ClientAsyncReadValueAttributeCallback callback = async_conversion_callback;
  void *user = async_conversion_context;
  (void)context;
  async_conversion_callback = NULL;
  async_conversion_context = NULL;
  client_conversion_arm(5, UA_STATUSCODE_GOOD);
  callback(client, user, id, status, value);
  (void)cpkt_types_fail_stop();
}
UA_StatusCode __real_UA_Client_readValueAttribute_async(
    UA_Client *, UA_NodeId, UA_ClientAsyncReadValueAttributeCallback, void *,
    UA_UInt32 *);
UA_StatusCode __wrap_UA_Client_readValueAttribute_async(
    UA_Client *client, UA_NodeId nodeId,
    UA_ClientAsyncReadValueAttributeCallback callback, void *context,
    UA_UInt32 *id) {
  if (client_failure_kind != 5)
    return __real_UA_Client_readValueAttribute_async(client, nodeId, callback,
                                                     context, id);
  if (async_conversion_callback)
    abort();
  async_conversion_callback = callback;
  async_conversion_context = context;
  UA_StatusCode status = __real_UA_Client_readValueAttribute_async(
      client, nodeId, async_conversion_complete, NULL, id);
  if (status) {
    client_failure_kind = 0;
    async_conversion_callback = NULL;
    async_conversion_context = NULL;
  }
  return status;
}
#endif
