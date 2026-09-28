/* Public operations needing actual native storage or live configuration roots.
 */
#ifndef CPKT_OPCUA_OPERATIONS_IMPL_H
#define CPKT_OPCUA_OPERATIONS_IMPL_H
struct cpkt_opcua_RunFlag {
  volatile UA_Boolean running;
};
struct cpkt_opcua_VariantView {
  UA_Variant native;
};
cpkt_opcua_RunFlag *cpkt_opcua_RunFlag_new(cpkt_opcua_Boolean running) {
  cpkt_opcua_RunFlag *flag = (cpkt_opcua_RunFlag *)UA_malloc(sizeof(*flag));
  if (flag)
    flag->running = running != 0;
  return flag;
}
void cpkt_opcua_RunFlag_set(cpkt_opcua_RunFlag *flag,
                            cpkt_opcua_Boolean running) {
  if (flag)
    flag->running = running != 0;
}
cpkt_opcua_Boolean cpkt_opcua_RunFlag_get(const cpkt_opcua_RunFlag *flag) {
  return flag ? flag->running : 0;
}
void cpkt_opcua_RunFlag_delete(cpkt_opcua_RunFlag *flag) { UA_free(flag); }
static UA_StatusCode cpkt_server_run(cpkt_opcua_server *server,
                                     const cpkt_opcua_RunFlag *flag,
                                     int until_interrupt) {
  UA_StatusCode status;
  int prepared = 0;
  if (!server || !server->server || (!until_interrupt && !flag))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (server->destroying || server->typed_loop_depth ||
      server->typed_config_depth)
    return UA_STATUSCODE_BADINVALIDSTATE;
  status =
      server->typed_pubsub_prepare ? server->typed_pubsub_prepare(server) : 0;
  prepared = !status && server->typed_pubsub_prepare != NULL;
  if (!status) {
    ++server->typed_loop_depth;
    server->started = 1;
    status = until_interrupt ? UA_Server_runUntilInterrupt(server->server)
                             : UA_Server_run(server->server, &flag->running);
    --server->typed_loop_depth;
    server->started = UA_Server_getLifecycleState(server->server) !=
                      UA_LIFECYCLESTATE_STOPPED;
  }
  if (prepared)
    server->typed_pubsub_finish(server);
  return status;
}
cpkt_opcua_StatusCode
cpkt_opcua_server_run_typed(cpkt_opcua_server *server,
                            const cpkt_opcua_RunFlag *flag) {
  return cpkt_server_run(server, flag, 0);
}
cpkt_opcua_StatusCode
cpkt_opcua_server_runUntilInterrupt_typed(cpkt_opcua_server *server) {
  return cpkt_server_run(server, NULL, 1);
}
cpkt_opcua_VariantView *cpkt_opcua_VariantView_new(void) {
  return (cpkt_opcua_VariantView *)UA_calloc(1, sizeof(cpkt_opcua_VariantView));
}
void cpkt_opcua_VariantView_delete(cpkt_opcua_VariantView *view) {
  /* Attribute roots retain no ownership of borrowed backend bytes. Clearing a
   * NODELETE Variant does not read through a potentially expired payload. */
  if (view) {
    UA_Variant_clear(&view->native);
    UA_free(view);
  }
}
cpkt_opcua_StatusCode
cpkt_opcua_VariantView_snapshot(const cpkt_opcua_VariantView *view,
                                cpkt_opcua_Variant *out) {
  cpkt_opcua_Variant staged;
  UA_StatusCode status;
  if (!view || !out)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memset(&staged, 0, sizeof(staged));
  status = cpkt_convert(&view->native, &staged,
                        &cpkt_types[CPKT_OPCUA_TYPES_VARIANT], 0, 0);
  if (!status)
    *out = staged;
  else
    cpkt_opcua_Variant_clear(&staged);
  return status;
}
cpkt_opcua_StatusCode
cpkt_opcua_client_getConnectionAttribute_typed(cpkt_opcua_client *client,
                                               cpkt_opcua_QualifiedName key,
                                               cpkt_opcua_VariantView *out) {
  UA_QualifiedName native;
  if (!client || !client->client || !out || !cpkt_string_valid(&key.name))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  native.namespaceIndex = key.namespaceIndex;
  native.name = cpkt_string_view(&key.name);
  return UA_Client_getConnectionAttribute(client->client, native, &out->native);
}
cpkt_opcua_StatusCode cpkt_opcua_server_getSessionAttribute_typed(
    cpkt_opcua_server *server, const cpkt_opcua_NodeId *session,
    cpkt_opcua_QualifiedName key, cpkt_opcua_VariantView *out) {
  UA_NodeId native_session;
  UA_QualifiedName native_key;
  if (!server || !server->server || !session || !out ||
      !cpkt_stock_node_valid(session) || !cpkt_string_valid(&key.name))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  native_session = cpkt_nodeid_view(session);
  native_key.namespaceIndex = key.namespaceIndex;
  native_key.name = cpkt_string_view(&key.name);
  return UA_Server_getSessionAttribute(server->server, &native_session,
                                       native_key, &out->native);
}
cpkt_opcua_StatusCode
cpkt_opcua_client_getConnectionAttributeCopy_typed(cpkt_opcua_client *client,
                                                   cpkt_opcua_QualifiedName key,
                                                   cpkt_opcua_Variant *out) {
  UA_Variant native;
  UA_QualifiedName native_key;
  UA_StatusCode status;
  cpkt_opcua_Variant staged;
  if (!client || !client->client || !out || !cpkt_string_valid(&key.name))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  native_key.namespaceIndex = key.namespaceIndex;
  native_key.name = cpkt_string_view(&key.name);
  UA_Variant_init(&native);
  memset(&staged, 0, sizeof(staged));
  status =
      UA_Client_getConnectionAttributeCopy(client->client, native_key, &native);
  if (!status)
    status = cpkt_convert(&native, &staged,
                          &cpkt_types[CPKT_OPCUA_TYPES_VARIANT], 0, 0);
  if (!status)
    *out = staged;
  else
    cpkt_opcua_Variant_clear(&staged);
  UA_Variant_clear(&native);
  return status;
}
static UA_StatusCode cpkt_attribute_scalar(cpkt_opcua_client *client,
                                           cpkt_opcua_server *server,
                                           const cpkt_opcua_NodeId *session,
                                           cpkt_opcua_QualifiedName key,
                                           const cpkt_opcua_Type *type,
                                           void *out) {
  UA_QualifiedName native_key;
  UA_NodeId native_session;
  UA_StatusCode status;
  void *native, *staged;
  type = cpkt_valid_type(type);
  if (!type || !out || !cpkt_string_valid(&key.name) ||
      (!client &&
       (!server || !server->server || !cpkt_stock_node_valid(session))) ||
      (client && !client->client))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  native = UA_malloc(type->native->memSize);
  if (!native)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  staged = UA_calloc(1, type->size);
  if (!staged) {
    UA_free(native);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  native_key.namespaceIndex = key.namespaceIndex;
  native_key.name = cpkt_string_view(&key.name);
  if (client)
    status = UA_Client_getConnectionAttribute_scalar(client->client, native_key,
                                                     type->native, native);
  else {
    native_session = cpkt_nodeid_view(session);
    status = UA_Server_getSessionAttribute_scalar(
        server->server, &native_session, native_key, type->native, native);
  }
  if (!status)
    status = cpkt_convert(native, staged, type, 0, 0);
  if (!status)
    memcpy(out, staged, type->size);
  else
    cpkt_opcua_type_clear(staged, type);
  /* Native scalar getters copy borrowed bytes only: do not UA_clear them. */
  UA_free(native);
  UA_free(staged);
  return status;
}
cpkt_opcua_StatusCode cpkt_opcua_client_getConnectionAttribute_scalar_typed(
    cpkt_opcua_client *client, cpkt_opcua_QualifiedName key,
    const cpkt_opcua_Type *type, void *out) {
  if (!client)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  return cpkt_attribute_scalar(client, NULL, NULL, key, type, out);
}
cpkt_opcua_StatusCode cpkt_opcua_server_getSessionAttribute_scalar_typed(
    cpkt_opcua_server *server, const cpkt_opcua_NodeId *session,
    cpkt_opcua_QualifiedName key, const cpkt_opcua_Type *type, void *out) {
  return cpkt_attribute_scalar(NULL, server, session, key, type, out);
}
cpkt_opcua_StatusCode cpkt_opcua_server_writeObjectProperty_scalar_typed(
    cpkt_opcua_server *server, cpkt_opcua_NodeId object,
    cpkt_opcua_QualifiedName property, const void *value,
    const cpkt_opcua_Type *type) {
  void *native;
  UA_NodeId native_object;
  UA_QualifiedName native_property;
  UA_StatusCode status;
  int prepared = 0;
  type = cpkt_valid_type(type);
  if (!server || !server->server || !value || !type ||
      !cpkt_stock_node_valid(&object) || !cpkt_string_valid(&property.name))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  native = UA_calloc(1, type->native->memSize);
  if (!native)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  native_object = cpkt_nodeid_view(&object);
  native_property.namespaceIndex = property.namespaceIndex;
  native_property.name = cpkt_string_view(&property.name);
  status = cpkt_convert(value, native, type, 1, 0);
  if (!status && server->typed_pubsub_prepare) {
    status = server->typed_pubsub_prepare(server);
    prepared = !status;
  }
  if (!status)
    status = UA_Server_writeObjectProperty_scalar(
        server->server, native_object, native_property, native, type->native);
  UA_delete(native, type->native);
  if (prepared)
    server->typed_pubsub_finish(server);
  return status;
}
const cpkt_opcua_Type *
cpkt_opcua_client_findDataType_typed(cpkt_opcua_client *client,
                                     const cpkt_opcua_NodeId *typeId) {
  cpkt_opcua_ClientConfig *config;
  const cpkt_opcua_DataTypeArray *custom;
  UA_NodeId native;
  if (!client || !client->client || !cpkt_stock_node_valid(typeId))
    return NULL;
  if (cpkt_opcua_client_get_config_typed(client, &config))
    return NULL;
  if (cpkt_opcua_ClientConfig_getCustomDataTypes(config, &custom))
    return NULL;
  native = cpkt_nodeid_view(typeId);
  return cpkt_from_native(UA_Client_findDataType(client->client, &native));
}
cpkt_opcua_StatusCode
cpkt_opcua_ConnectionConfig_default(cpkt_opcua_ConnectionConfig *out) {
  if (!out)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  out->protocolVersion = UA_ConnectionConfig_default.protocolVersion;
  out->recvBufferSize = UA_ConnectionConfig_default.recvBufferSize;
  out->sendBufferSize = UA_ConnectionConfig_default.sendBufferSize;
  out->localMaxMessageSize = UA_ConnectionConfig_default.localMaxMessageSize;
  out->remoteMaxMessageSize = UA_ConnectionConfig_default.remoteMaxMessageSize;
  out->localMaxChunkCount = UA_ConnectionConfig_default.localMaxChunkCount;
  out->remoteMaxChunkCount = UA_ConnectionConfig_default.remoteMaxChunkCount;
  return 0;
}
/* A static C89 byte view, not an allocated schema value. */
static const char cpkt_security_none[] =
    "http://opcfoundation.org/UA/SecurityPolicy#None";
const cpkt_opcua_String cpkt_opcua_SECURITY_POLICY_NONE_URI = {
    sizeof(cpkt_security_none) - 1, (cpkt_opcua_Byte *)cpkt_security_none};
#endif
