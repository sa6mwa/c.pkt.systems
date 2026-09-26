#include "opcua_types_peer.h"
#include <arpa/inet.h>
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
struct peer {
  UA_Server *server;
  pthread_t thread;
  pthread_mutex_t lock;
  int running;
};
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
