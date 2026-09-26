#include "opcua_types_peer.h"
#include <arpa/inet.h>
#include <open62541/plugin/accesscontrol.h>
#include <open62541/plugin/historydatabase.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>
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
