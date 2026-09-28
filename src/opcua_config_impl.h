/* Configuration backing remains native. Opaque wrappers hold only facade
 * identity, callback/logger metadata and C89/native ownership boundaries. */
#ifndef CPKT_OPCUA_CONFIG_IMPL_H
#define CPKT_OPCUA_CONFIG_IMPL_H
#include <open62541/client_config_default.h>
#include <open62541/server_config_default.h>
#include <open62541/server_config_file_based.h>

typedef struct cpkt_cfg_plugin_view cpkt_cfg_plugin_view;
typedef struct cpkt_cfg_type_allocation cpkt_cfg_type_allocation;
typedef struct cpkt_cfg_type_snapshot cpkt_cfg_type_snapshot;
static void cpkt_cfg_type_metadata_clear(cpkt_cfg_type_allocation **,
                                         cpkt_cfg_type_snapshot **);
static void cpkt_cfg_views_clear(cpkt_cfg_plugin_view **);
struct cpkt_opcua_ClientConfig {
  struct cpkt_opcua_ClientConfig *next;
  struct cpkt_opcua_ClientConfig *discovery_next;
  cpkt_opcua_server *discovery_parent;
  UA_EventLoop *discovery_loop;
  UA_Logger discovery_logger;
  UA_Logger *discovery_original_logger;
  UA_ClientNotificationCallback discovery_global;
  UA_DelayedCallback discovery_delayed;
  int discovery_external_loop, discovery_finished, discovery_queued,
      discovery_logger_cleared;
  UA_ClientConfig *native;
  UA_ClientConfig storage;
  cpkt_opcua_client *owner;
  struct cpkt_opcua_logger logger;
  cpkt_opcua_ClientConfigCallbacks callbacks;
  cpkt_cfg_plugin_view *views;
  cpkt_cfg_type_allocation *custom_allocations;
  cpkt_cfg_type_snapshot *custom_snapshot;
  unsigned int depth;
};
struct cpkt_opcua_ServerConfig {
  struct cpkt_opcua_ServerConfig *next;
  UA_ServerConfig *native;
  UA_ServerConfig storage;
  cpkt_opcua_server *owner;
  cpkt_opcua_server *pending_owner;
  struct cpkt_opcua_logger logger;
  cpkt_opcua_ServerConfigCallbacks callbacks;
  cpkt_cfg_plugin_view *views;
  cpkt_cfg_type_allocation *custom_allocations;
  cpkt_cfg_type_snapshot *custom_snapshot;
  unsigned int depth;
};
static UA_ServerConfig *cpkt_cfg_server_native(cpkt_opcua_server *server) {
  if (server->typed_config)
    return server->typed_config->native;
  return server->server ? UA_Server_getConfig(server->server) : NULL;
}
static void cpkt_cfg_discovery_log(void *, UA_LogLevel, UA_LogCategory,
                                   const char *, va_list);
static void cpkt_cfg_discovery_notify(UA_Client *,
                                      UA_ApplicationNotificationType,
                                      UA_KeyValueMap);
static UA_ClientNotificationCallback
cpkt_cfg_client_global(const cpkt_opcua_ClientConfig *config) {
  return config->discovery_parent ? config->discovery_global
                                  : config->native->globalNotificationCallback;
}
static void cpkt_cfg_loop_moved(UA_EventLoop **, UA_Boolean);
static void cpkt_cfg_client_rebind(cpkt_opcua_ClientConfig *,
                                   UA_ClientConfig *);
static void cpkt_cfg_server_rebind(cpkt_opcua_ServerConfig *,
                                   UA_ServerConfig *);
static void cpkt_cfg_store_moved(cpkt_opcua_ServerConfig *);
static pthread_mutex_t cpkt_cfg_mutex = PTHREAD_MUTEX_INITIALIZER;
static cpkt_opcua_ClientConfig *cpkt_cfg_clients;
static cpkt_opcua_ServerConfig *cpkt_cfg_servers;
/* Constructors can issue native callbacks before returning the new handle.
 * A per-thread scoped creation record preserves facade identity then, without
 * changing native application context or relying on a process-global current
 * constructor. Nested construction restores the previous scope. */
typedef struct {
  void *previous;
  cpkt_opcua_ClientConfig *client;
  cpkt_opcua_ServerConfig *server;
} cpkt_cfg_creation;
static pthread_key_t cpkt_cfg_creation_key;
static pthread_once_t cpkt_cfg_creation_once = PTHREAD_ONCE_INIT;
static int cpkt_cfg_creation_key_status;
static void cpkt_cfg_creation_key_init(void) {
  cpkt_cfg_creation_key_status =
      pthread_key_create(&cpkt_cfg_creation_key, NULL);
}
static UA_StatusCode cpkt_cfg_creation_begin(cpkt_cfg_creation *scope) {
  if (pthread_once(&cpkt_cfg_creation_once, cpkt_cfg_creation_key_init) ||
      cpkt_cfg_creation_key_status)
    return UA_STATUSCODE_BADINTERNALERROR;
  scope->previous = pthread_getspecific(cpkt_cfg_creation_key);
  if (pthread_setspecific(cpkt_cfg_creation_key, scope))
    return UA_STATUSCODE_BADOUTOFMEMORY;
  return 0;
}
static void cpkt_cfg_creation_end(cpkt_cfg_creation *scope) {
  (void)pthread_setspecific(cpkt_cfg_creation_key, scope->previous);
}
static void cpkt_cfg_client_rebind(cpkt_opcua_ClientConfig *config,
                                   UA_ClientConfig *native) {
  (void)pthread_mutex_lock(&cpkt_cfg_mutex);
  config->native = native;
  (void)pthread_mutex_unlock(&cpkt_cfg_mutex);
}
static void cpkt_cfg_server_rebind(cpkt_opcua_ServerConfig *config,
                                   UA_ServerConfig *native) {
  (void)pthread_mutex_lock(&cpkt_cfg_mutex);
  config->native = native;
  (void)pthread_mutex_unlock(&cpkt_cfg_mutex);
  cpkt_cfg_store_moved(config);
}
static cpkt_opcua_ClientConfig *cpkt_cfg_client_find(UA_ClientConfig *native) {
  cpkt_opcua_ClientConfig *config;
  cpkt_cfg_creation *scope = NULL;
  (void)pthread_mutex_lock(&cpkt_cfg_mutex);
  for (config = cpkt_cfg_clients; config; config = config->next)
    if (config->native == native)
      break;
  (void)pthread_mutex_unlock(&cpkt_cfg_mutex);
  if (!config && native->logging &&
      native->logging->log == cpkt_cfg_discovery_log) {
    config = (cpkt_opcua_ClientConfig *)native->logging->context;
    cpkt_cfg_client_rebind(config, native);
  }
  if (!config &&
      !pthread_once(&cpkt_cfg_creation_once, cpkt_cfg_creation_key_init) &&
      !cpkt_cfg_creation_key_status)
    scope = (cpkt_cfg_creation *)pthread_getspecific(cpkt_cfg_creation_key);
  if (scope && scope->client && scope->client->owner &&
      !scope->client->owner->client) {
    /* Binding happens in the constructor's scope helper when its actual native
     * handle is available. Client construction currently issues no callbacks
     * before returning; never invent an enclosing-struct offset here. */
    config = scope->client;
    cpkt_cfg_client_rebind(config, native);
  }
  return config;
}
static cpkt_opcua_ServerConfig *cpkt_cfg_server_find(UA_ServerConfig *native) {
  cpkt_opcua_ServerConfig *config;
  cpkt_cfg_creation *scope = NULL;
  (void)pthread_mutex_lock(&cpkt_cfg_mutex);
  for (config = cpkt_cfg_servers; config; config = config->next)
    if (config->native == native)
      break;
  (void)pthread_mutex_unlock(&cpkt_cfg_mutex);
  if (!config &&
      !pthread_once(&cpkt_cfg_creation_once, cpkt_cfg_creation_key_init) &&
      !cpkt_cfg_creation_key_status)
    scope = (cpkt_cfg_creation *)pthread_getspecific(cpkt_cfg_creation_key);
  if (scope && scope->server && scope->server->owner &&
      !scope->server->owner->server) {
    config = scope->server;
    cpkt_cfg_server_rebind(config, native);
  }
  return config;
}
/* The pending object contains facade identity only. No temporary native server
 * is constructed, and native plugin contexts are never duplicated. */
static cpkt_opcua_server *
cpkt_cfg_server_owner(cpkt_opcua_ServerConfig *config) {
  cpkt_opcua_server *server;
  if (config->owner)
    return config->owner;
  if (config->pending_owner)
    return config->pending_owner;
  server = (cpkt_opcua_server *)UA_calloc(1, sizeof(*server));
  if (!server)
    return NULL;
  if (!cpkt_nodes_owner_new(server)) {
    UA_free(server);
    return NULL;
  }
  server->typed_config = config;
  config->pending_owner = server;
  return server;
}
static void cpkt_cfg_bind_server_owner(cpkt_opcua_server *owner,
                                       UA_Server *native) {
  if (!owner || !native || owner->server == native)
    return;
  owner->server = native;
  if (owner->typed_config)
    cpkt_cfg_server_rebind(owner->typed_config, UA_Server_getConfig(native));
  if (owner->typed_nodes) {
    (void)pthread_mutex_lock(&cpkt_nodes_mutex);
    owner->typed_nodes->native = native;
    owner->typed_nodes->event_loop = UA_Server_getConfig(native)->eventLoop;
    (void)pthread_mutex_unlock(&cpkt_nodes_mutex);
  }
}
static cpkt_nodes_owner *cpkt_cfg_find_constructing_server(UA_Server *native) {
  cpkt_cfg_creation *scope;
  if (pthread_once(&cpkt_cfg_creation_once, cpkt_cfg_creation_key_init) ||
      cpkt_cfg_creation_key_status)
    return NULL;
  scope = (cpkt_cfg_creation *)pthread_getspecific(cpkt_cfg_creation_key);
  if (!scope || !scope->server || !scope->server->owner ||
      (scope->server->owner->server && scope->server->owner->server != native))
    return NULL;
  cpkt_cfg_bind_server_owner(scope->server->owner, native);
  return scope->server->owner->typed_nodes;
}
static void cpkt_cfg_pending_clear(cpkt_opcua_ServerConfig *config) {
  cpkt_opcua_server *server = config->pending_owner;
  if (!server)
    return;
  if (server->typed_nodes_clear)
    server->typed_nodes_clear(server);
  UA_free(server);
  config->pending_owner = NULL;
}
static void cpkt_cfg_client_enter(cpkt_opcua_ClientConfig *config) {
  if (config->discovery_parent)
    ++config->discovery_parent->typed_config_depth;
  ++config->depth;
  if (config->owner)
    ++config->owner->typed_config_depth;
}
static void cpkt_cfg_client_leave(cpkt_opcua_ClientConfig *config) {
  if (config->discovery_parent)
    --config->discovery_parent->typed_config_depth;
  if (config->owner)
    --config->owner->typed_config_depth;
  --config->depth;
}
static void cpkt_cfg_server_enter(cpkt_opcua_ServerConfig *config) {
  ++config->depth;
  if (config->owner)
    ++config->owner->typed_config_depth;
}
static void cpkt_cfg_server_leave(cpkt_opcua_ServerConfig *config) {
  if (config->owner)
    --config->owner->typed_config_depth;
  --config->depth;
}
static void cpkt_cfg_store_enter(void *context) {
  cpkt_cfg_server_enter((cpkt_opcua_ServerConfig *)context);
}
static void cpkt_cfg_store_leave(void *context) {
  cpkt_cfg_server_leave((cpkt_opcua_ServerConfig *)context);
}
static void cpkt_cfg_store_moved(cpkt_opcua_ServerConfig *config) {
  cpkt_opcua_Nodestore *store = cpkt_store_find(config->native->nodestore);
  if (store)
    cpkt_store_bind(store, &config->native->nodestore, config,
                    cpkt_cfg_store_enter, cpkt_cfg_store_leave);
}
static int cpkt_cfg_client_mutable(const cpkt_opcua_ClientConfig *config) {
  UA_SecureChannelState channel;
  UA_SessionState session;
  if (config->depth || (config->owner && (config->owner->destroying ||
                                          config->owner->typed_config_depth)))
    return 0;
  if (!config->owner)
    return 1;
  if (!config->owner->client)
    return 0;
  UA_Client_getState(config->owner->client, &channel, &session, NULL);
  return channel == UA_SECURECHANNELSTATE_CLOSED &&
         session == UA_SESSIONSTATE_CLOSED;
}
static int cpkt_cfg_server_mutable(const cpkt_opcua_ServerConfig *config) {
  return !config->depth &&
         (!config->owner ||
          (!config->owner->destroying && !config->owner->typed_loop_depth &&
           !config->owner->typed_config_depth &&
           UA_Server_getLifecycleState(config->owner->server) ==
               UA_LIFECYCLESTATE_STOPPED));
}
static void cpkt_cfg_client_cleanup(cpkt_opcua_client *client) {
  cpkt_opcua_ClientConfig **slot;
  (void)pthread_mutex_lock(&cpkt_cfg_mutex);
  for (slot = &cpkt_cfg_clients; *slot; slot = &(*slot)->next)
    if (*slot == client->typed_config) {
      *slot = (*slot)->next;
      break;
    }
  (void)pthread_mutex_unlock(&cpkt_cfg_mutex);
  if (client->typed_config) {
    cpkt_cfg_views_clear(&client->typed_config->views);
    cpkt_cfg_type_metadata_clear(&client->typed_config->custom_allocations,
                                 &client->typed_config->custom_snapshot);
  }
  UA_free(client->typed_config);
  client->typed_config = NULL;
}
static void cpkt_cfg_server_cleanup(cpkt_opcua_server *server) {
  cpkt_opcua_ServerConfig **slot;
  (void)pthread_mutex_lock(&cpkt_cfg_mutex);
  for (slot = &cpkt_cfg_servers; *slot; slot = &(*slot)->next)
    if (*slot == server->typed_config) {
      *slot = (*slot)->next;
      break;
    }
  (void)pthread_mutex_unlock(&cpkt_cfg_mutex);
  if (server->typed_config) {
    cpkt_cfg_views_clear(&server->typed_config->views);
    cpkt_cfg_type_metadata_clear(&server->typed_config->custom_allocations,
                                 &server->typed_config->custom_snapshot);
  }
  UA_free(server->typed_config);
  server->typed_config = NULL;
}
cpkt_opcua_ClientConfig *cpkt_opcua_ClientConfig_new(void) {
  cpkt_opcua_ClientConfig *config =
      (cpkt_opcua_ClientConfig *)UA_calloc(1, sizeof(*config));
  if (config) {
    config->native = &config->storage;
    (void)pthread_mutex_lock(&cpkt_cfg_mutex);
    config->next = cpkt_cfg_clients;
    cpkt_cfg_clients = config;
    (void)pthread_mutex_unlock(&cpkt_cfg_mutex);
  }
  return config;
}
cpkt_opcua_ServerConfig *cpkt_opcua_ServerConfig_new(void) {
  cpkt_opcua_ServerConfig *config =
      (cpkt_opcua_ServerConfig *)UA_calloc(1, sizeof(*config));
  if (config) {
    config->native = &config->storage;
    (void)pthread_mutex_lock(&cpkt_cfg_mutex);
    config->next = cpkt_cfg_servers;
    cpkt_cfg_servers = config;
    (void)pthread_mutex_unlock(&cpkt_cfg_mutex);
  }
  return config;
}
cpkt_opcua_StatusCode
cpkt_opcua_ClientConfig_clear(cpkt_opcua_ClientConfig *config) {
  if (!config)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (config->owner || config->depth)
    return UA_STATUSCODE_BADINVALIDSTATE;
  ++config->depth;
  cpkt_loop_forget_config(&config->native->eventLoop);
  UA_ClientConfig_clear(config->native);
  cpkt_cfg_views_clear(&config->views);
  cpkt_cfg_type_metadata_clear(&config->custom_allocations,
                               &config->custom_snapshot);
  --config->depth;
  memset(config->native, 0, sizeof(*config->native));
  memset(&config->callbacks, 0, sizeof(config->callbacks));
  return 0;
}
cpkt_opcua_StatusCode
cpkt_opcua_ServerConfig_clear(cpkt_opcua_ServerConfig *config) {
  if (!config)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (config->owner || config->depth)
    return UA_STATUSCODE_BADINVALIDSTATE;
  ++config->depth;
  cpkt_loop_forget_config(&config->native->eventLoop);
  UA_ServerConfig_clear(config->native);
  cpkt_cfg_views_clear(&config->views);
  cpkt_cfg_type_metadata_clear(&config->custom_allocations,
                               &config->custom_snapshot);
  cpkt_cfg_pending_clear(config);
  --config->depth;
  memset(config->native, 0, sizeof(*config->native));
  memset(&config->callbacks, 0, sizeof(config->callbacks));
  return 0;
}
cpkt_opcua_StatusCode
cpkt_opcua_ClientConfig_delete(cpkt_opcua_ClientConfig *config) {
  UA_StatusCode status;
  if (!config)
    return 0;
  status = cpkt_opcua_ClientConfig_clear(config);
  if (!status) {
    cpkt_opcua_client temporary;
    memset(&temporary, 0, sizeof(temporary));
    temporary.typed_config = config;
    cpkt_cfg_client_cleanup(&temporary);
  }
  return status;
}
cpkt_opcua_StatusCode
cpkt_opcua_ServerConfig_delete(cpkt_opcua_ServerConfig *config) {
  UA_StatusCode status;
  if (!config)
    return 0;
  status = cpkt_opcua_ServerConfig_clear(config);
  if (!status) {
    cpkt_opcua_server temporary;
    memset(&temporary, 0, sizeof(temporary));
    temporary.typed_config = config;
    cpkt_cfg_server_cleanup(&temporary);
  }
  return status;
}
cpkt_opcua_StatusCode
cpkt_opcua_client_get_config_typed(cpkt_opcua_client *client,
                                   cpkt_opcua_ClientConfig **output) {
  cpkt_opcua_ClientConfig *config;
  if (!output)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  *output = NULL;
  if (!client || !client->client || client->destroying)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  config = client->typed_config;
  if (!config) {
    config = cpkt_opcua_ClientConfig_new();
    if (!config)
      return UA_STATUSCODE_BADOUTOFMEMORY;
    cpkt_cfg_client_rebind(config, UA_Client_getConfig(client->client));
    config->owner = client;
    client->typed_config = config;
    client->typed_config_clear = cpkt_cfg_client_cleanup;
    client->typed_eventloop_forget_config = cpkt_loop_forget_config;
  }
  *output = config;
  return 0;
}
cpkt_opcua_StatusCode
cpkt_opcua_server_get_config_typed(cpkt_opcua_server *server,
                                   cpkt_opcua_ServerConfig **output) {
  cpkt_opcua_ServerConfig *config;
  if (!output)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  *output = NULL;
  if (!server || !server->server || server->destroying)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!cpkt_nodes_owner_new(server))
    return UA_STATUSCODE_BADOUTOFMEMORY;
  cpkt_nodes_refresh_eventloop(server);
  config = server->typed_config;
  if (!config) {
    config = cpkt_opcua_ServerConfig_new();
    if (!config)
      return UA_STATUSCODE_BADOUTOFMEMORY;
    cpkt_cfg_server_rebind(config, UA_Server_getConfig(server->server));
    config->owner = server;
    server->typed_config = config;
    server->typed_config_clear = cpkt_cfg_server_cleanup;
    server->typed_eventloop_forget_config = cpkt_loop_forget_config;
  }
  *output = config;
  return 0;
}
cpkt_opcua_StatusCode
cpkt_opcua_client_newWithConfig_typed(cpkt_opcua_ClientConfig *config,
                                      cpkt_opcua_client **output) {
  cpkt_opcua_client *client;
  cpkt_cfg_creation scope;
  UA_StatusCode status;
  if (!output)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  *output = NULL;
  if (!config || config->owner || config->depth)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  client = (cpkt_opcua_client *)UA_calloc(1, sizeof(*client));
  if (!client)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  memset(&scope, 0, sizeof(scope));
  scope.client = config;
  status = cpkt_cfg_creation_begin(&scope);
  if (status) {
    UA_free(client);
    return status;
  }
  config->owner = client;
  client->typed_config = config;
  cpkt_cfg_client_enter(config);
  client->client = UA_Client_newWithConfig(config->native);
  cpkt_cfg_client_leave(config);
  cpkt_cfg_creation_end(&scope);
  if (!client->client) {
    config->owner = NULL;
    cpkt_cfg_client_rebind(config, &config->storage);
    UA_free(client);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  memset(&config->storage, 0, sizeof(config->storage));
  cpkt_cfg_client_rebind(config, UA_Client_getConfig(client->client));
  cpkt_cfg_loop_moved(&config->native->eventLoop,
                      config->native->externalEventLoop);
  config->owner = client;
  client->typed_config = config;
  client->typed_config_clear = cpkt_cfg_client_cleanup;
  client->typed_eventloop_forget_config = cpkt_loop_forget_config;
  *output = client;
  return 0;
}
cpkt_opcua_StatusCode
cpkt_opcua_server_newWithConfig_typed(cpkt_opcua_ServerConfig *config,
                                      cpkt_opcua_server **output) {
  cpkt_opcua_server *server;
  cpkt_cfg_creation scope;
  UA_StatusCode status;
  if (!output)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  *output = NULL;
  if (!config || config->owner || config->depth)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  server = cpkt_cfg_server_owner(config);
  if (!server)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  memset(&scope, 0, sizeof(scope));
  scope.server = config;
  status = cpkt_cfg_creation_begin(&scope);
  if (status)
    return status;
  config->owner = server;
  config->pending_owner = NULL;
  server->typed_config = config;
  cpkt_cfg_server_enter(config);
  server->server = UA_Server_newWithConfig(config->native);
  cpkt_cfg_server_leave(config);
  cpkt_cfg_creation_end(&scope);
  if (!server->server) {
    config->owner = NULL;
    cpkt_cfg_server_rebind(config, &config->storage);
    memset(&config->storage, 0, sizeof(config->storage));
    memset(&config->callbacks, 0, sizeof(config->callbacks));
    cpkt_cfg_views_clear(&config->views);
    cpkt_cfg_type_metadata_clear(&config->custom_allocations,
                                 &config->custom_snapshot);
    if (server->typed_nodes_clear)
      server->typed_nodes_clear(server);
    UA_free(server);
    return UA_STATUSCODE_BADINTERNALERROR;
  }
  cpkt_cfg_server_rebind(config, UA_Server_getConfig(server->server));
  cpkt_cfg_loop_moved(&config->native->eventLoop,
                      config->native->externalEventLoop);
  config->owner = server;
  server->typed_config = config;
  server->typed_config_clear = cpkt_cfg_server_cleanup;
  server->typed_eventloop_forget_config = cpkt_loop_forget_config;
  (void)pthread_mutex_lock(&cpkt_nodes_mutex);
  server->typed_nodes->native = server->server;
  server->typed_nodes->event_loop = config->native->eventLoop;
  (void)pthread_mutex_unlock(&cpkt_nodes_mutex);
  *output = server;
  return 0;
}

static UA_StatusCode cpkt_cfg_loop_assign(UA_EventLoop **slot,
                                          UA_Boolean *external,
                                          cpkt_opcua_EventLoop *loop,
                                          cpkt_opcua_Boolean new_external) {
  UA_EventLoop *old = *slot;
  UA_EventLoop *replacement = loop ? loop->native : NULL;
  cpkt_opcua_EventLoop *old_view;
  UA_StatusCode status;
  if (loop && (!loop->native || loop->freeing || loop->depth))
    return UA_STATUSCODE_BADINVALIDSTATE;
  if (loop && !new_external && loop->configuration_slot &&
      loop->configuration_slot != slot &&
      *loop->configuration_slot == replacement)
    return UA_STATUSCODE_BADINVALIDSTATE;
  if (old != replacement && old && !*external) {
    if (old->state != UA_EVENTLOOPSTATE_FRESH &&
        old->state != UA_EVENTLOOPSTATE_STOPPED)
      return UA_STATUSCODE_BADINVALIDSTATE;
    status = old->free(old);
    if (status)
      return status;
  } else if (old == replacement && new_external) {
    old_view = cpkt_loop_native_find(old);
    if (old_view && old_view->configuration_slot == slot)
      old_view->configuration_slot = NULL;
  }
  *slot = replacement;
  *external = new_external != 0;
  if (loop && !new_external)
    loop->configuration_slot = slot;
  return 0;
}
static void cpkt_cfg_loop_moved(UA_EventLoop **slot, UA_Boolean external) {
  cpkt_opcua_EventLoop *loop = cpkt_loop_native_find(*slot);
  if (loop && !external)
    loop->configuration_slot = slot;
}

#include "opcua_config_metadata.inc"

/* Separate groups retain the dependency order through include formatting. */
#include "opcua_config_types_impl.h"

#include "opcua_config_plugins_impl.h"

#include "opcua_config_discovery_impl.h"

#include "opcua_config_accesscontrol_impl.h"
#endif
