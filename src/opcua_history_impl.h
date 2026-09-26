/* Private persistent-value and native default-history bindings. */
/** Deep-copies a C89 value into a backend-owned native storage object. */
cpkt_opcua_StatusCode
cpkt_opcua_history_value_new(const cpkt_opcua_DataValue *value,
                             cpkt_opcua_history_value **out) {
  cpkt_opcua_history_value *stored;
  UA_StatusCode status;
  if (!out)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  *out = NULL;
  if (!value)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  stored = (cpkt_opcua_history_value *)UA_calloc(1, sizeof(*stored));
  if (!stored)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  status = cpkt_opcua_history_value_set(stored, value);
  if (status) {
    UA_free(stored);
    return status;
  }
  *out = stored;
  return 0;
}
/** Stages conversion before replacing the persistent native value. */
cpkt_opcua_StatusCode
cpkt_opcua_history_value_set(cpkt_opcua_history_value *stored,
                             const cpkt_opcua_DataValue *value) {
  UA_DataValue staged;
  UA_StatusCode status;
  if (!stored || !value)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  UA_DataValue_init(&staged);
  status = cpkt_convert(value, &staged, &cpkt_types[CPKT_OPCUA_TYPES_DATAVALUE],
                        1, 0);
  if (!status) {
    UA_DataValue_clear(&stored->native);
    stored->native = staged;
  } else
    UA_DataValue_clear(&staged);
  return status;
}
/** Returns an independent, owned C89 copy of the stored native value. */
cpkt_opcua_StatusCode
cpkt_opcua_history_value_get(const cpkt_opcua_history_value *stored,
                             cpkt_opcua_DataValue *out) {
  UA_StatusCode status;
  if (!out)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memset(out, 0, sizeof(*out));
  if (!stored)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  status = cpkt_convert(&stored->native, out,
                        &cpkt_types[CPKT_OPCUA_TYPES_DATAVALUE], 0, 0);
  if (status)
    cpkt_opcua_DataValue_clear(out);
  return status;
}
/** Releases persistent storage once native borrowers have finished. */
void cpkt_opcua_history_value_free(cpkt_opcua_history_value *stored) {
  if (stored) {
    UA_DataValue_clear(&stored->native);
    UA_free(stored);
  }
}
/* Native Server_delete destroys the admin subscription before clearing config.
 * Stop polling while monitored items and server components are still available.
 */
static void cpkt_gather_stop(cpkt_gather_bridge *bridge) {
  cpkt_hb_bridge *backend;
  for (backend = bridge->backends; backend; backend = backend->next)
    (void)bridge->native.stopPoll(bridge->owner->server, bridge->native.context,
                                  &backend->node);
  bridge->prepared_delete = 1;
}
static void cpkt_history_prepare_delete(cpkt_opcua_server *server) {
  cpkt_gather_stop(server->typed_history);
}
static void cpkt_gather_delete(UA_HistoryDataGathering *native) {
  cpkt_gather_bridge *bridge = (cpkt_gather_bridge *)native->context;
  cpkt_hb_bridge *backend, *next;
  if (!bridge->prepared_delete)
    cpkt_gather_stop(bridge);
  bridge->native.deleteMembers(&bridge->native);
  for (backend = bridge->backends; backend; backend = next) {
    next = backend->next;
    if (backend->plugin.deleteMembers)
      backend->plugin.deleteMembers(&backend->plugin);
    UA_NodeId_clear(&backend->node);
    UA_free(backend);
  }
  if (bridge->owner->typed_history == bridge) {
    bridge->owner->typed_history = NULL;
    bridge->owner->history_prepare_delete = NULL;
  }
  UA_free(bridge);
  memset(native, 0, sizeof(*native));
}
static const UA_HistorizingNodeIdSettings *
cpkt_gather_setting(UA_Server *server, void *context, const UA_NodeId *node) {
  cpkt_gather_bridge *bridge = (cpkt_gather_bridge *)context;
  return bridge->native.getHistorizingSetting(server, bridge->native.context,
                                              node);
}
static void cpkt_gather_set(UA_Server *server, void *context,
                            const UA_NodeId *session, void *session_context,
                            const UA_NodeId *node, UA_Boolean historizing,
                            const UA_DataValue *value) {
  cpkt_gather_bridge *bridge = (cpkt_gather_bridge *)context;
  bridge->native.setValue(server, bridge->native.context, session,
                          session_context, node, historizing, value);
}
/** Installs upstream's default database with explicit backend cleanup. */
cpkt_opcua_StatusCode
cpkt_opcua_server_set_default_history_database(cpkt_opcua_server *server,
                                               size_t initial_capacity) {
  cpkt_gather_bridge *bridge;
  UA_HistoryDataGathering gathering;
  UA_HistoryDatabase database;
  UA_ServerConfig *config;
  if (!server || server->started || !initial_capacity)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  bridge = (cpkt_gather_bridge *)UA_calloc(1, sizeof(*bridge));
  if (!bridge)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  bridge->owner = server;
  bridge->native = UA_HistoryDataGathering_Default(initial_capacity);
  if (!bridge->native.context) {
    UA_free(bridge);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  memset(&gathering, 0, sizeof(gathering));
  gathering.context = bridge;
  gathering.deleteMembers = cpkt_gather_delete;
  gathering.getHistorizingSetting = cpkt_gather_setting;
  gathering.setValue = cpkt_gather_set;
  database = UA_HistoryDatabase_default(gathering);
  if (!database.context) {
    cpkt_gather_delete(&gathering);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  config = UA_Server_getConfig(server->server);
  if (config->historyDatabase.clear)
    config->historyDatabase.clear(&config->historyDatabase);
  config->historyDatabase = database;
  server->typed_history = bridge;
  server->history_prepare_delete = cpkt_history_prepare_delete;
  return 0;
}
/** Registers an application backend using native per-node history settings. */
cpkt_opcua_StatusCode cpkt_opcua_server_register_history_backend(
    cpkt_opcua_server *server, const cpkt_opcua_NodeId *node,
    const cpkt_opcua_HistorizingNodeIdSettings *settings) {
  cpkt_gather_bridge *gather;
  cpkt_hb_bridge *bridge;
  UA_HistorizingNodeIdSettings native;
  UA_StatusCode status;
  const cpkt_opcua_HistoryDataBackend *plugin;
  if (!server || server->started || !server->typed_history ||
      server->typed_history->polling_started || !node || !settings ||
      !settings->maxHistoryDataResponseSize ||
      settings->historizingUpdateStrategy <
          CPKT_OPCUA_HISTORIZINGUPDATESTRATEGY_USER ||
      settings->historizingUpdateStrategy >
          CPKT_OPCUA_HISTORIZINGUPDATESTRATEGY_POLL)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  plugin = &settings->historizingBackend;
  if (!plugin->boundSupported || !plugin->timestampsToReturnSupported ||
      (settings->historizingUpdateStrategy !=
           CPKT_OPCUA_HISTORIZINGUPDATESTRATEGY_USER &&
       !plugin->serverSetHistoryData) ||
      (!plugin->getHistoryData &&
       (!plugin->getDateTimeMatch || !plugin->getEnd || !plugin->lastIndex ||
        !plugin->firstIndex || !plugin->resultSize || !plugin->copyDataValues ||
        !plugin->getDataValue)))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  bridge = (cpkt_hb_bridge *)UA_calloc(1, sizeof(*bridge));
  if (!bridge)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  bridge->owner = server;
  bridge->plugin = *plugin;
  status = cpkt_convert(node, &bridge->node,
                        &cpkt_types[CPKT_OPCUA_TYPES_NODEID], 1, 0);
  memset(&native, 0, sizeof(native));
  cpkt_hb_assign(&native.historizingBackend, plugin);
  native.historizingBackend.context = bridge;
  native.maxHistoryDataResponseSize = settings->maxHistoryDataResponseSize;
  native.historizingUpdateStrategy =
      (UA_HistorizingUpdateStrategy)settings->historizingUpdateStrategy;
  native.pollingInterval = settings->pollingInterval;
  native.userContext = settings->userContext;
  gather = server->typed_history;
  if (!status)
    status = gather->native.registerNodeId(
        server->server, gather->native.context, &bridge->node, native);
  if (status) {
    UA_NodeId_clear(&bridge->node);
    UA_free(bridge);
    return status;
  }
  bridge->next = gather->backends;
  gather->backends = bridge;
  return 0;
}
static UA_StatusCode cpkt_history_poll(cpkt_opcua_server *server,
                                       const cpkt_opcua_NodeId *node,
                                       int start) {
  UA_NodeId native;
  UA_StatusCode status;
  cpkt_gather_bridge *gather;
  if (!server || !server->typed_history || !node)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  UA_NodeId_init(&native);
  status =
      cpkt_convert(node, &native, &cpkt_types[CPKT_OPCUA_TYPES_NODEID], 1, 0);
  gather = server->typed_history;
  if (!status)
    status = start ? gather->native.startPoll(server->server,
                                              gather->native.context, &native)
                   : gather->native.stopPoll(server->server,
                                             gather->native.context, &native);
  if (!status && start)
    gather->polling_started = 1;
  UA_NodeId_clear(&native);
  return status;
}
/** Starts native gathering's local monitored-item polling. */
cpkt_opcua_StatusCode
cpkt_opcua_server_history_start_poll(cpkt_opcua_server *server,
                                     const cpkt_opcua_NodeId *node) {
  return cpkt_history_poll(server, node, 1);
}
/** Stops native gathering's local monitored-item polling. */
cpkt_opcua_StatusCode
cpkt_opcua_server_history_stop_poll(cpkt_opcua_server *server,
                                    const cpkt_opcua_NodeId *node) {
  return cpkt_history_poll(server, node, 0);
}
