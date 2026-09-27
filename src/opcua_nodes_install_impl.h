/* Installation stages the complete record before changing native callback
 * slots. Contexts belong to the application; this registry owns dispatch
 * metadata only. */
static UA_StatusCode cpkt_nodes_stage(cpkt_opcua_server *server,
                                      const cpkt_opcua_NodeId *node,
                                      cpkt_nodes_entry **out) {
  cpkt_nodes_owner *owner;
  cpkt_nodes_entry *entry, *previous;
  UA_StatusCode status;
  *out = NULL;
  owner = cpkt_nodes_owner_new(server);
  if (!owner)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  entry = (cpkt_nodes_entry *)UA_calloc(1, sizeof(*entry));
  if (!entry)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  entry->owner = owner;
  entry->references = 1;
  status = cpkt_convert(node, &entry->node,
                        &cpkt_types[CPKT_OPCUA_TYPES_NODEID], 1, 0);
  if (status) {
    cpkt_nodes_release(entry);
    return status;
  }
  previous = cpkt_nodes_acquire(server->server, &entry->node);
  if (previous) {
    entry->notifications = previous->notifications;
    entry->lifecycle = previous->lifecycle;
    entry->source = previous->source;
    entry->method = previous->method;
    cpkt_nodes_release(previous);
  }
  *out = entry;
  return 0;
}
static void cpkt_nodes_commit(cpkt_nodes_entry *entry) {
  cpkt_nodes_entry **slot, *previous;
  cpkt_creation *creation;
  for (creation = entry->owner->creations; creation;
       creation = creation->previous)
    if (UA_NodeId_equal(creation->id, &entry->node))
      creation->superseded = 1;
  for (slot = &entry->owner->entries; *slot; slot = &(*slot)->next)
    if (UA_NodeId_equal(&(*slot)->node, &entry->node))
      break;
  previous = *slot;
  entry->next = previous ? previous->next : NULL;
  *slot = entry;
  if (previous)
    cpkt_nodes_release(previous);
}
cpkt_opcua_StatusCode
cpkt_opcua_server_setVariableNode_internalValueSource_typed(
    cpkt_opcua_server *server, cpkt_opcua_NodeId nodeId,
    const cpkt_opcua_DataValue *value,
    const cpkt_opcua_ValueSourceNotifications *notifications) {
  UA_DataValue native_value;
  UA_ValueSourceNotifications native_notifications;
  cpkt_nodes_entry *entry = NULL;
  UA_StatusCode status;
  if (!server || !server->server || server->destroying)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  UA_DataValue_init(&native_value);
  status = value ? cpkt_convert(value, &native_value,
                                &cpkt_types[CPKT_OPCUA_TYPES_DATAVALUE], 1, 0)
                 : 0;
  if (!status)
    status = cpkt_nodes_stage(server, &nodeId, &entry);
  if (!status) {
    if (notifications)
      entry->notifications = *notifications;
    else
      memset(&entry->notifications, 0, sizeof(entry->notifications));
    cpkt_nodes_assign_ValueSourceNotifications(&native_notifications,
                                               &entry->notifications);
    status = UA_Server_setVariableNode_internalValueSource(
        server->server, entry->node, value ? &native_value : NULL,
        notifications ? &native_notifications : NULL);
  }
  UA_DataValue_clear(&native_value);
  if (!status)
    cpkt_nodes_commit(entry);
  else if (entry)
    cpkt_nodes_release(entry);
  return status;
}
cpkt_opcua_StatusCode cpkt_opcua_server_setNodeTypeLifecycle_typed(
    cpkt_opcua_server *server, cpkt_opcua_NodeId nodeId,
    cpkt_opcua_NodeTypeLifecycle lifecycle) {
  cpkt_nodes_entry *entry = NULL;
  UA_NodeTypeLifecycle native;
  UA_StatusCode status;
  if (!server || !server->server || server->destroying)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  status = cpkt_nodes_stage(server, &nodeId, &entry);
  if (!status) {
    entry->lifecycle = lifecycle;
    cpkt_nodes_assign_NodeTypeLifecycle(&native, &lifecycle);
    status =
        UA_Server_setNodeTypeLifecycle(server->server, entry->node, native);
  }
  if (!status)
    cpkt_nodes_commit(entry);
  else if (entry)
    cpkt_nodes_release(entry);
  return status;
}
cpkt_opcua_StatusCode cpkt_opcua_server_set_global_node_lifecycle(
    cpkt_opcua_server *server,
    const cpkt_opcua_GlobalNodeLifecycle *lifecycle) {
  cpkt_nodes_owner *owner;
  if (!server || !server->server || server->destroying || server->started)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  owner = cpkt_nodes_owner_new(server);
  if (!owner)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  if (lifecycle)
    owner->global = *lifecycle;
  else
    memset(&owner->global, 0, sizeof(owner->global));
  cpkt_nodes_assign_GlobalNodeLifecycle(&owner->native_global, &owner->global);
  UA_Server_getConfig(server->server)->nodeLifecycle =
      lifecycle ? &owner->native_global : NULL;
  return 0;
}
