/* Native discovery retains a client asynchronously. Keep the consumed facade
 * metadata until its native STOPPED/clear notification and deferred teardown.
 * This does not implement registration or change native client state/context.
 */
#ifndef CPKT_OPCUA_CONFIG_DISCOVERY_IMPL_H
#define CPKT_OPCUA_CONFIG_DISCOVERY_IMPL_H
static void cpkt_cfg_discovery_log(void *context, UA_LogLevel level,
                                   UA_LogCategory category, const char *format,
                                   va_list args) {
  cpkt_opcua_ClientConfig *config = (cpkt_opcua_ClientConfig *)context;
  UA_Logger *original = config->discovery_original_logger;
  cpkt_cfg_client_enter(config);
  if (original && original->log)
    original->log(original->context, level, category, format, args);
  cpkt_cfg_client_leave(config);
}
static void cpkt_cfg_discovery_logger_clear(UA_Logger *logger) {
  cpkt_opcua_ClientConfig *config = (cpkt_opcua_ClientConfig *)logger->context;
  UA_Logger *original = config->discovery_original_logger;
  config->discovery_original_logger = NULL;
  config->discovery_logger_cleared = 1;
  cpkt_cfg_client_enter(config);
  if (original && original->clear)
    original->clear(original);
  cpkt_cfg_client_leave(config);
}
static void cpkt_cfg_discovery_unlink(cpkt_opcua_ClientConfig *config) {
  cpkt_nodes_owner *nodes = config->discovery_parent->typed_nodes;
  cpkt_opcua_ClientConfig **slot;
  for (slot = &nodes->discovery; *slot; slot = &(*slot)->discovery_next)
    if (*slot == config) {
      *slot = config->discovery_next;
      break;
    }
}
static void cpkt_cfg_discovery_release(cpkt_opcua_ClientConfig *config,
                                       UA_EventLoop *loop) {
  cpkt_opcua_client *client = config->owner;
  cpkt_cfg_discovery_unlink(config);
  config->discovery_parent = NULL;
  config->native = NULL; /* Native config storage has finished deletion. */
  client->client = NULL;
  if (client->typed_callbacks_clear)
    client->typed_callbacks_clear(client, loop);
  client->borrowed = 0;
  client->destroying = 0;
  cpkt_opcua_client_free(client);
}
static void cpkt_cfg_discovery_deferred(void *application, void *context) {
  cpkt_opcua_ClientConfig *config = (cpkt_opcua_ClientConfig *)context;
  (void)application;
  config->discovery_queued = 0;
  cpkt_cfg_discovery_release(config, config->discovery_loop);
}
static void cpkt_cfg_discovery_notify(UA_Client *client,
                                      UA_ApplicationNotificationType type,
                                      UA_KeyValueMap payload) {
  cpkt_opcua_ClientConfig *config =
      cpkt_cfg_client_find(UA_Client_getConfig(client));
  if (!config || !config->discovery_parent)
    return;
  config->owner->client = client;
  cpkt_cfg_client_enter(config);
  if (config->discovery_global)
    config->discovery_global(client, type, payload);
  cpkt_cfg_client_leave(config);
  /* This native notification is emitted by UA_Client_clear, after pending
   * services and subscriptions finish, before its configuration is destroyed.
   * Deferred cleanup runs after that native call, never from inside it. */
  if (type == UA_APPLICATIONNOTIFICATIONTYPE_LIFECYCLE_STOPPED &&
      !config->discovery_finished) {
    config->discovery_finished = 1;
    config->owner->destroying = 1;
    config->discovery_delayed.callback = cpkt_cfg_discovery_deferred;
    config->discovery_delayed.context = config;
    config->discovery_queued = 1;
    config->discovery_loop->addDelayedCallback(config->discovery_loop,
                                               &config->discovery_delayed);
  }
}
static void cpkt_cfg_discovery_cleanup(cpkt_nodes_owner *nodes) {
  cpkt_opcua_ClientConfig *config;
  while ((config = (cpkt_opcua_ClientConfig *)nodes->discovery) != NULL) {
    /* Native server destruction has already deleted every discovery client.
     * Native POSIX loop free drains deferred callbacks. An external loop stays
     * alive: detach any remaining queued observer before freeing its metadata.
     * A custom internal loop has finished destruction at this point. */
    if (config->discovery_queued && config->discovery_external_loop)
      config->discovery_loop->removeDelayedCallback(config->discovery_loop,
                                                    &config->discovery_delayed);
    config->discovery_queued = 0;
    cpkt_cfg_discovery_release(config, config->discovery_external_loop
                                           ? config->discovery_loop
                                           : NULL);
  }
}
static UA_StatusCode cpkt_cfg_discovery(cpkt_opcua_server *server,
                                        cpkt_opcua_ClientConfig **source,
                                        const cpkt_opcua_String *url,
                                        const cpkt_opcua_String *semaphore,
                                        int deregister) {
  cpkt_opcua_ServerConfig *parent;
  cpkt_opcua_ClientConfig *config = source ? *source : NULL;
  cpkt_opcua_client *client;
  cpkt_nodes_owner *nodes;
  UA_ClientConfig empty, *input;
  UA_StatusCode status;
  int consumed;
  if (!server || !server->server || server->destroying || !config ||
      config->owner || config->depth || config->native != &config->storage ||
      !url || !cpkt_string_valid(url) || !semaphore ||
      !cpkt_string_valid(semaphore))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  status = cpkt_opcua_server_get_config_typed(server, &parent);
  if (status)
    return status;
  if (UA_Server_getLifecycleState(server->server) ==
          UA_LIFECYCLESTATE_STARTED &&
      (!parent->native->eventLoop ||
       !parent->native->eventLoop->addDelayedCallback ||
       !parent->native->eventLoop->removeDelayedCallback))
    return UA_STATUSCODE_BADNOTSUPPORTED;
  nodes = server->typed_nodes;
  client = (cpkt_opcua_client *)UA_calloc(1, sizeof(*client));
  if (!client)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  client->borrowed = 1;
  client->typed_config = config;
  client->typed_config_clear = cpkt_cfg_client_cleanup;
  config->owner = client;
  config->discovery_parent = server;
  config->discovery_loop = parent->native->eventLoop;
  config->discovery_external_loop = parent->native->externalEventLoop;
  config->discovery_global = config->native->globalNotificationCallback;
  config->discovery_original_logger = config->native->logging;
  config->discovery_logger_cleared = 0;
  memset(&config->discovery_logger, 0, sizeof(config->discovery_logger));
  config->discovery_logger.context = config;
  config->discovery_logger.log = cpkt_cfg_discovery_log;
  config->discovery_logger.clear = cpkt_cfg_discovery_logger_clear;
  config->native->logging = &config->discovery_logger;
  config->native->globalNotificationCallback = cpkt_cfg_discovery_notify;
  config->discovery_next = (cpkt_opcua_ClientConfig *)nodes->discovery;
  nodes->discovery = config;
  nodes->discovery_cleanup = cpkt_cfg_discovery_cleanup;
  input = config->native;
  cpkt_cfg_client_enter(config);
  status = deregister
               ? UA_Server_deregisterDiscovery(server->server, input,
                                               cpkt_string_view(url))
               : UA_Server_registerDiscovery(server->server, input,
                                             cpkt_string_view(url),
                                             cpkt_string_view(semaphore));
  cpkt_cfg_client_leave(config);
  memset(&empty, 0, sizeof(empty));
  consumed = !memcmp(input, &empty, sizeof(empty));
  if (!consumed) {
    int cleared = config->discovery_logger_cleared;
    /* Early allocation/lifecycle errors clear the native input. A failed
     * old-loop free precedes handoff and preserves the complete config. */
    cpkt_cfg_discovery_unlink(config);
    if (!cleared) {
      input->logging = config->discovery_original_logger;
      input->globalNotificationCallback = config->discovery_global;
    }
    config->owner = NULL;
    config->discovery_parent = NULL;
    config->discovery_next = NULL;
    config->discovery_global = NULL;
    config->discovery_loop = NULL;
    config->discovery_original_logger = NULL;
    cpkt_cfg_client_rebind(config, &config->storage);
    if (cleared) {
      memset(&config->storage, 0, sizeof(config->storage));
      memset(&config->callbacks, 0, sizeof(config->callbacks));
      memset(&config->logger, 0, sizeof(config->logger));
      cpkt_cfg_views_clear(&config->views);
      cpkt_cfg_type_metadata_clear(&config->custom_allocations,
                                   &config->custom_snapshot);
    }
    UA_free(client);
  }
  if (consumed)
    *source = NULL;
  /* Zeroed input means ownership moved, even if asynchronous connect setup
   * returned an error. The native discovery manager still owns that client. */
  return status;
}
cpkt_opcua_StatusCode cpkt_opcua_server_registerDiscovery_typed(
    cpkt_opcua_server *server, cpkt_opcua_ClientConfig **config,
    const cpkt_opcua_String discoveryServerUrl,
    const cpkt_opcua_String semaphoreFilePath) {
  return cpkt_cfg_discovery(server, config, &discoveryServerUrl,
                            &semaphoreFilePath, 0);
}
cpkt_opcua_StatusCode cpkt_opcua_server_deregisterDiscovery_typed(
    cpkt_opcua_server *server, cpkt_opcua_ClientConfig **config,
    const cpkt_opcua_String discoveryServerUrl) {
  cpkt_opcua_String empty;
  memset(&empty, 0, sizeof(empty));
  return cpkt_cfg_discovery(server, config, &discoveryServerUrl, &empty, 1);
}
#endif
