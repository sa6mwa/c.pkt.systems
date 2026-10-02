/* Native installation runs only after all dispatch metadata is staged. */
cpkt_opcua_StatusCode
cpkt_opcua_server_setVariableNode_callbackValueSource_typed(
    cpkt_opcua_server *server, cpkt_opcua_NodeId node,
    cpkt_opcua_CallbackValueSource source) {
  cpkt_nodes_entry *entry = NULL;
  UA_CallbackValueSource native;
  UA_StatusCode status;
  if (!server || !server->server || server->destroying)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  status = cpkt_nodes_stage(server, &node, &entry);
  if (!status) {
    entry->source = source;
    native.read = source.read ? cpkt_producer_native_read : NULL;
    native.write = source.write ? cpkt_producer_native_write : NULL;
    status = UA_Server_setVariableNode_callbackValueSource(server->server,
                                                           entry->node, native);
  }
  if (!status)
    cpkt_nodes_commit(entry);
  else if (entry)
    cpkt_nodes_release(entry);
  return status;
}
cpkt_opcua_StatusCode cpkt_opcua_server_setMethodNodeCallback_typed(
    cpkt_opcua_server *server, cpkt_opcua_NodeId node,
    cpkt_opcua_MethodCallback method) {
  cpkt_nodes_entry *entry = NULL;
  UA_StatusCode status;
  if (!server || !server->server || server->destroying)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  status = cpkt_nodes_stage(server, &node, &entry);
  if (!status) {
    entry->method = method;
    status = UA_Server_setMethodNodeCallback(
        server->server, entry->node,
        method ? cpkt_producer_native_method : NULL);
  }
  if (!status)
    cpkt_nodes_commit(entry);
  else if (entry)
    cpkt_nodes_release(entry);
  return status;
}
cpkt_opcua_StatusCode cpkt_opcua_server_getMethodNodeCallback_typed(
    cpkt_opcua_server *server, cpkt_opcua_NodeId node,
    cpkt_opcua_MethodCallback *method) {
  cpkt_nodes_entry *entry;
  UA_NodeId native;
  UA_MethodCallback callback = NULL;
  UA_StatusCode status;
  if (!server || !server->server || server->destroying || !method)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  *method = NULL;
  UA_NodeId_init(&native);
  status =
      cpkt_convert(&node, &native, &cpkt_types[CPKT_OPCUA_TYPES_NODEID], 1, 0);
  if (!status)
    status = UA_Server_getMethodNodeCallback(server->server, native, &callback);
  if (!status && callback) {
    entry = cpkt_nodes_acquire(server->server, &native);
    if (callback != cpkt_producer_native_method || !entry || !entry->method)
      status = UA_STATUSCODE_BADNOTSUPPORTED;
    else
      *method = entry->method;
    if (entry)
      cpkt_nodes_release(entry);
  }
  UA_NodeId_clear(&native);
  return status;
}
