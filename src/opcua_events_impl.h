/* Public event creation and local notifications. Native owns event filtering,
 * sampling, queues, and dispatch timing. Facade entries only retain callbacks.
 */
#ifndef CPKT_OPCUA_EVENTS_IMPL_H
#define CPKT_OPCUA_EVENTS_IMPL_H

typedef struct cpkt_local_monitor cpkt_local_monitor;
struct cpkt_local_monitor {
  cpkt_local_monitor *next;
  cpkt_nodes_owner *owner;
  void *context;
  cpkt_opcua_Server_DataChangeNotificationCallback data_change;
  cpkt_opcua_Server_EventNotificationCallback event;
  UA_UInt32 id;
  unsigned int references;
};

static void cpkt_local_monitor_release(cpkt_local_monitor *entry) {
  if (!--entry->references)
    UA_free(entry);
}

static void cpkt_local_monitor_cleanup(cpkt_nodes_owner *owner) {
  cpkt_local_monitor *entry = owner->local_monitors, *next;
  owner->local_monitors = NULL;
  while (entry) {
    next = entry->next;
    cpkt_local_monitor_release(entry);
    entry = next;
  }
}

static void cpkt_local_monitor_remove(cpkt_opcua_server *server, UA_UInt32 id) {
  cpkt_nodes_owner *owner = server->typed_nodes;
  cpkt_local_monitor **slot, *entry;
  if (!owner)
    return;
  slot = (cpkt_local_monitor **)&owner->local_monitors;
  while (*slot) {
    entry = *slot;
    if (entry->id == id) {
      *slot = entry->next;
      cpkt_local_monitor_release(entry);
      return;
    }
    slot = &entry->next;
  }
}

static void cpkt_local_data_change(UA_Server *native, UA_UInt32 id,
                                   void *context, const UA_NodeId *nodeId,
                                   void *nodeContext, UA_UInt32 attributeId,
                                   const UA_DataValue *value) {
  cpkt_local_monitor *entry = (cpkt_local_monitor *)context;
  cpkt_opcua_NodeId node;
  cpkt_opcua_DataValue converted;
  UA_StatusCode status;
  (void)native;
  if (!entry->data_change)
    return;
  ++entry->references;
  ++entry->owner->owner->typed_config_depth;
  cpkt_hb_borrow_node(nodeId, &node);
  memset(&converted, 0, sizeof(converted));
  status = cpkt_convert(value, &converted,
                        &cpkt_types[CPKT_OPCUA_TYPES_DATAVALUE], 0, 0);
  entry->data_change(entry->owner->owner, id, entry->context, &node,
                     nodeContext, attributeId, status,
                     status ? NULL : &converted);
  cpkt_opcua_DataValue_clear(&converted);
  --entry->owner->owner->typed_config_depth;
  cpkt_local_monitor_release(entry);
}

static void cpkt_local_event(UA_Server *native, UA_UInt32 id, void *context,
                             const UA_KeyValueMap fields) {
  cpkt_local_monitor *entry = (cpkt_local_monitor *)context;
  cpkt_opcua_KeyValueMap converted;
  UA_StatusCode status;
  (void)native;
  if (!entry->event)
    return;
  ++entry->references;
  ++entry->owner->owner->typed_config_depth;
  memset(&converted, 0, sizeof(converted));
  status = cpkt_array(fields.map, fields.mapSize, (void **)&converted.map,
                      &cpkt_types[CPKT_OPCUA_TYPES_KEYVALUEPAIR], 0, 0);
  converted.mapSize = converted.map ? fields.mapSize : 0;
  entry->event(entry->owner->owner, id, entry->context, status,
               status ? NULL : &converted);
  cpkt_opcua_KeyValueMap_clear(&converted);
  --entry->owner->owner->typed_config_depth;
  cpkt_local_monitor_release(entry);
}

static cpkt_local_monitor *cpkt_local_monitor_new(cpkt_opcua_server *server,
                                                  void *context) {
  cpkt_nodes_owner *owner = cpkt_nodes_owner_new(server);
  cpkt_local_monitor *entry;
  if (!owner)
    return NULL;
  entry = (cpkt_local_monitor *)UA_calloc(1, sizeof(*entry));
  if (!entry)
    return NULL;
  entry->owner = owner;
  entry->context = context;
  entry->references = 1;
  owner->local_monitors_cleanup = cpkt_local_monitor_cleanup;
  return entry;
}

static UA_StatusCode
cpkt_local_monitor_finish(cpkt_local_monitor *entry,
                          UA_MonitoredItemCreateResult *native,
                          cpkt_opcua_MonitoredItemCreateResult *response) {
  UA_StatusCode status;
  entry->id = native->monitoredItemId;
  status = cpkt_convert(native, response,
                        &cpkt_types[CPKT_OPCUA_TYPES_MONITOREDITEMCREATERESULT],
                        0, 0);
  if (!native->statusCode && status)
    (void)UA_Server_deleteMonitoredItem(entry->owner->owner->server, entry->id);
  if (native->statusCode || status)
    cpkt_local_monitor_release(entry);
  else {
    entry->next = entry->owner->local_monitors;
    entry->owner->local_monitors = entry;
  }
  UA_MonitoredItemCreateResult_clear(native);
  if (status)
    cpkt_opcua_MonitoredItemCreateResult_clear(response);
  return status;
}

cpkt_opcua_StatusCode cpkt_opcua_server_createDataChangeMonitoredItem_typed(
    cpkt_opcua_server *server, cpkt_opcua_TimestampsToReturn timestamps,
    const cpkt_opcua_MonitoredItemCreateRequest item, void *context,
    cpkt_opcua_Server_DataChangeNotificationCallback callback,
    cpkt_opcua_MonitoredItemCreateResult *response) {
  UA_MonitoredItemCreateRequest nativeItem;
  UA_MonitoredItemCreateResult nativeResult;
  cpkt_local_monitor *entry;
  UA_StatusCode status;
  if (!server || !server->server || !response)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memset(response, 0, sizeof(*response));
  memset(&nativeItem, 0, sizeof(nativeItem));
  status = cpkt_convert(
      &item, &nativeItem,
      &cpkt_types[CPKT_OPCUA_TYPES_MONITOREDITEMCREATEREQUEST], 1, 0);
  entry = status ? NULL : cpkt_local_monitor_new(server, context);
  if (!status && !entry)
    status = UA_STATUSCODE_BADOUTOFMEMORY;
  if (!status) {
    entry->data_change = callback;
    nativeResult = UA_Server_createDataChangeMonitoredItem(
        server->server, (UA_TimestampsToReturn)timestamps, nativeItem, entry,
        cpkt_local_data_change);
    status = cpkt_local_monitor_finish(entry, &nativeResult, response);
  }
  UA_MonitoredItemCreateRequest_clear(&nativeItem);
  return status;
}

cpkt_opcua_StatusCode cpkt_opcua_server_createEventMonitoredItemEx_typed(
    cpkt_opcua_server *server, const cpkt_opcua_MonitoredItemCreateRequest item,
    void *context, cpkt_opcua_Server_EventNotificationCallback callback,
    cpkt_opcua_MonitoredItemCreateResult *response) {
  UA_MonitoredItemCreateRequest nativeItem;
  UA_MonitoredItemCreateResult nativeResult;
  cpkt_local_monitor *entry;
  UA_StatusCode status;
  if (!server || !server->server || !response)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memset(response, 0, sizeof(*response));
  memset(&nativeItem, 0, sizeof(nativeItem));
  status = cpkt_convert(
      &item, &nativeItem,
      &cpkt_types[CPKT_OPCUA_TYPES_MONITOREDITEMCREATEREQUEST], 1, 0);
  entry = status ? NULL : cpkt_local_monitor_new(server, context);
  if (!status && !entry)
    status = UA_STATUSCODE_BADOUTOFMEMORY;
  if (!status) {
    entry->event = callback;
    nativeResult = UA_Server_createEventMonitoredItemEx(
        server->server, nativeItem, entry, cpkt_local_event);
    status = cpkt_local_monitor_finish(entry, &nativeResult, response);
  }
  UA_MonitoredItemCreateRequest_clear(&nativeItem);
  return status;
}

cpkt_opcua_StatusCode cpkt_opcua_server_createEventMonitoredItem_typed(
    cpkt_opcua_server *server, const cpkt_opcua_NodeId nodeId,
    const cpkt_opcua_EventFilter filter, void *context,
    cpkt_opcua_Server_EventNotificationCallback callback,
    cpkt_opcua_MonitoredItemCreateResult *response) {
  UA_NodeId nativeNode;
  UA_EventFilter nativeFilter;
  UA_MonitoredItemCreateResult nativeResult;
  cpkt_local_monitor *entry;
  UA_StatusCode status;
  if (!server || !server->server || !response)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memset(response, 0, sizeof(*response));
  memset(&nativeNode, 0, sizeof(nativeNode));
  memset(&nativeFilter, 0, sizeof(nativeFilter));
  status = cpkt_convert(&nodeId, &nativeNode,
                        &cpkt_types[CPKT_OPCUA_TYPES_NODEID], 1, 0);
  if (!status)
    status = cpkt_convert(&filter, &nativeFilter,
                          &cpkt_types[CPKT_OPCUA_TYPES_EVENTFILTER], 1, 0);
  entry = status ? NULL : cpkt_local_monitor_new(server, context);
  if (!status && !entry)
    status = UA_STATUSCODE_BADOUTOFMEMORY;
  if (!status) {
    entry->event = callback;
    nativeResult = UA_Server_createEventMonitoredItem(
        server->server, nativeNode, nativeFilter, entry, cpkt_local_event);
    status = cpkt_local_monitor_finish(entry, &nativeResult, response);
  }
  UA_NodeId_clear(&nativeNode);
  UA_EventFilter_clear(&nativeFilter);
  return status;
}

typedef struct {
  UA_EventDescription description;
  UA_NodeId eventInstance, session;
  UA_KeyValueMap fields;
} cpkt_event_view;

static void cpkt_event_view_clear(cpkt_event_view *view) {
  UA_NodeId_clear(&view->description.sourceNode);
  UA_NodeId_clear(&view->description.eventType);
  UA_LocalizedText_clear(&view->description.message);
  UA_NodeId_clear(&view->eventInstance);
  UA_NodeId_clear(&view->session);
  UA_KeyValueMap_clear(&view->fields);
}

static UA_StatusCode
cpkt_event_description_view(const cpkt_opcua_EventDescription *source,
                            cpkt_event_view *view) {
  UA_StatusCode status;
  UA_EventDescription *dst = &view->description;
  status = cpkt_convert(&source->sourceNode, &dst->sourceNode,
                        &cpkt_types[CPKT_OPCUA_TYPES_NODEID], 1, 0);
  if (!status)
    status = cpkt_convert(&source->eventType, &dst->eventType,
                          &cpkt_types[CPKT_OPCUA_TYPES_NODEID], 1, 0);
  if (!status)
    status = cpkt_convert(&source->message, &dst->message,
                          &cpkt_types[CPKT_OPCUA_TYPES_LOCALIZEDTEXT], 1, 0);
  dst->severity = source->severity;
  dst->subscriptionId = source->subscriptionId;
  dst->monitoredItemId = source->monitoredItemId;
  if (source->eventFields) {
    if (!status)
      status =
          cpkt_array(source->eventFields->map, source->eventFields->mapSize,
                     (void **)&view->fields.map,
                     &cpkt_types[CPKT_OPCUA_TYPES_KEYVALUEPAIR], 1, 0);
    view->fields.mapSize = view->fields.map ? source->eventFields->mapSize : 0;
    dst->eventFields = &view->fields;
  }
  if (source->eventInstance) {
    if (!status)
      status = cpkt_convert(source->eventInstance, &view->eventInstance,
                            &cpkt_types[CPKT_OPCUA_TYPES_NODEID], 1, 0);
    dst->eventInstance = &view->eventInstance;
  }
  if (source->sessionId) {
    if (!status)
      status = cpkt_convert(source->sessionId, &view->session,
                            &cpkt_types[CPKT_OPCUA_TYPES_NODEID], 1, 0);
    dst->sessionId = &view->session;
  }
  return status;
}

cpkt_opcua_StatusCode cpkt_opcua_server_createEventEx_typed(
    cpkt_opcua_server *server, const cpkt_opcua_EventDescription *description,
    cpkt_opcua_ByteString *outEventId) {
  cpkt_event_view view;
  UA_ByteString id;
  UA_StatusCode status;
  int prepared = 0;
  if (!server || !server->server || !description)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (outEventId)
    memset(outEventId, 0, sizeof(*outEventId));
  memset(&view, 0, sizeof(view));
  memset(&id, 0, sizeof(id));
  status = cpkt_event_description_view(description, &view);
  if (!status && server->typed_pubsub_prepare) {
    status = server->typed_pubsub_prepare(server);
    prepared = !status;
  }
  if (!status)
    status = UA_Server_createEventEx(server->server, &view.description,
                                     outEventId ? &id : NULL);
  if (outEventId)
    cpkt_string_take(outEventId, id);
  if (prepared)
    server->typed_pubsub_finish(server);
  cpkt_event_view_clear(&view);
  return status;
}

cpkt_opcua_StatusCode cpkt_opcua_server_createEvent_typed(
    cpkt_opcua_server *server, const cpkt_opcua_NodeId sourceNode,
    const cpkt_opcua_NodeId eventType, cpkt_opcua_UInt16 severity,
    const cpkt_opcua_LocalizedText message,
    const cpkt_opcua_KeyValueMap *eventFields,
    const cpkt_opcua_NodeId *eventInstance, cpkt_opcua_ByteString *outEventId) {
  cpkt_opcua_EventDescription description;
  memset(&description, 0, sizeof(description));
  description.sourceNode = sourceNode;
  description.eventType = eventType;
  description.severity = severity;
  description.message = message;
  description.eventFields = eventFields;
  description.eventInstance = eventInstance;
  return cpkt_opcua_server_createEventEx_typed(server, &description,
                                               outEventId);
}
/* Reverse-connect callbacks can arrive during registration and during a
 * deferred native close. Keep the holder through the final CLOSED dispatch. */
typedef struct cpkt_reverse_connection cpkt_reverse_connection;
struct cpkt_reverse_connection {
  cpkt_reverse_connection *next;
  cpkt_nodes_owner *owner;
  cpkt_opcua_Server_ReverseConnectStateCallback callback;
  void *context;
  unsigned int references;
  UA_UInt64 handle;
  UA_SecureChannelState state;
  int detached;
};
static void cpkt_reverse_release(cpkt_reverse_connection *entry) {
  if (!--entry->references)
    UA_free(entry);
}
static void cpkt_reverse_detach(cpkt_reverse_connection *entry) {
  cpkt_reverse_connection **slot;
  if (entry->detached)
    return;
  slot = (cpkt_reverse_connection **)&entry->owner->reverse_connections;
  while (*slot && *slot != entry)
    slot = &(*slot)->next;
  if (*slot)
    *slot = entry->next;
  entry->detached = 1;
  cpkt_reverse_release(entry);
}
/* An initially failed registration can remain CLOSED; native removal then
 * emits no state change. Release that holder after successful removal. Active
 * connections retain their holder until the deferred final CLOSED callback. */
static void cpkt_reverse_removed(cpkt_opcua_server *server, UA_UInt64 handle) {
  cpkt_nodes_owner *owner = server->typed_nodes;
  cpkt_reverse_connection *entry;
  if (!owner)
    return;
  for (entry = owner->reverse_connections; entry; entry = entry->next) {
    if (entry->handle == handle) {
      if (entry->state == UA_SECURECHANNELSTATE_CLOSED)
        cpkt_reverse_detach(entry);
      return;
    }
  }
}
static void cpkt_reverse_cleanup(cpkt_nodes_owner *owner) {
  cpkt_reverse_connection *entry;
  while ((entry = owner->reverse_connections) != NULL)
    cpkt_reverse_detach(entry);
}
static void cpkt_reverse_state(UA_Server *server, UA_UInt64 handle,
                               UA_SecureChannelState state, void *context) {
  cpkt_reverse_connection *entry = (cpkt_reverse_connection *)context;
  cpkt_opcua_UInt64 publicHandle;
  (void)server;
  ++entry->references;
  ++entry->owner->owner->typed_config_depth;
  entry->handle = handle;
  entry->state = state;
  (void)cpkt_convert(&handle, &publicHandle,
                     &cpkt_types[CPKT_OPCUA_TYPES_UINT64], 0, 0);
  if (entry->callback)
    entry->callback(entry->owner->owner, publicHandle,
                    (cpkt_opcua_SecureChannelState)state, entry->context);
  if (state == UA_SECURECHANNELSTATE_CLOSED)
    cpkt_reverse_detach(entry);
  --entry->owner->owner->typed_config_depth;
  cpkt_reverse_release(entry);
}
cpkt_opcua_StatusCode cpkt_opcua_server_addReverseConnect_typed(
    cpkt_opcua_server *server, cpkt_opcua_String url,
    cpkt_opcua_Server_ReverseConnectStateCallback callback, void *context,
    cpkt_opcua_UInt64 *handle) {
  cpkt_nodes_owner *owner;
  cpkt_reverse_connection *entry;
  UA_UInt64 nativeHandle = 0;
  UA_StatusCode status;
  if (!server || !server->server || !cpkt_string_valid(&url))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  owner = cpkt_nodes_owner_new(server);
  if (!owner)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  entry = (cpkt_reverse_connection *)UA_calloc(1, sizeof(*entry));
  if (!entry)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  entry->owner = owner;
  entry->callback = callback;
  entry->context = context;
  entry->references = 2; /* List plus registration frame. */
  entry->next = owner->reverse_connections;
  owner->reverse_connections = entry;
  owner->reverse_connections_cleanup = cpkt_reverse_cleanup;
  status =
      UA_Server_addReverseConnect(server->server, cpkt_string_view(&url),
                                  cpkt_reverse_state, entry, &nativeHandle);
  entry->handle = nativeHandle;
  if (nativeHandle && handle)
    (void)cpkt_convert(&nativeHandle, handle,
                       &cpkt_types[CPKT_OPCUA_TYPES_UINT64], 0, 0);
  if (!nativeHandle)
    cpkt_reverse_detach(entry);
  cpkt_reverse_release(entry);
  return status;
}

static void cpkt_registered_server(const UA_RegisteredServer *native,
                                   void *context) {
  cpkt_nodes_owner *owner = (cpkt_nodes_owner *)context;
  cpkt_opcua_Server_registerServerCallback callback = owner->registered_server;
  void *data = owner->registered_server_context;
  cpkt_opcua_RegisteredServer value;
  UA_StatusCode status;
  if (!callback)
    return;
  ++owner->owner->typed_config_depth;
  memset(&value, 0, sizeof(value));
  status = cpkt_convert(native, &value,
                        &cpkt_types[CPKT_OPCUA_TYPES_REGISTEREDSERVER], 0, 0);
  callback(status, status ? NULL : &value, data);
  cpkt_opcua_RegisteredServer_clear(&value);
  --owner->owner->typed_config_depth;
}
cpkt_opcua_StatusCode cpkt_opcua_server_setRegisterServerCallback_typed(
    cpkt_opcua_server *server,
    cpkt_opcua_Server_registerServerCallback callback, void *data) {
  cpkt_nodes_owner *owner;
  if (!server || !server->server)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  owner = cpkt_nodes_owner_new(server);
  if (!owner)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  owner->registered_server = callback;
  owner->registered_server_context = data;
  UA_Server_setRegisterServerCallback(
      server->server, callback ? cpkt_registered_server : NULL, owner);
  return 0;
}
#endif
