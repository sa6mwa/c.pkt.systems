/* Native-backed configuration plugin views. Their public metadata is copied;
 * every operation resolves and invokes the actual native record and context. */
#ifndef CPKT_OPCUA_CONFIG_PLUGINS_IMPL_H
#define CPKT_OPCUA_CONFIG_PLUGINS_IMPL_H
enum {
  CPKT_CFG_CLIENT_CG,
  CPKT_CFG_CHANNEL_CG,
  CPKT_CFG_SESSION_CG,
  CPKT_CFG_CLIENT_SP,
  CPKT_CFG_AUTH_SP,
  CPKT_CFG_SERVER_SP,
  CPKT_CFG_PSP,
  CPKT_CFG_AC,
  CPKT_CFG_HDB,
  CPKT_CFG_GLC,
  CPKT_CFG_PSCONFIG
};
struct cpkt_cfg_plugin_view {
  cpkt_cfg_plugin_view *next;
  int kind;
  size_t index;
  cpkt_opcua_ClientConfig *client;
  cpkt_opcua_ServerConfig *server;
  cpkt_opcua_log_config logger;
  union {
    cpkt_opcua_CertificateGroup cg;
    cpkt_opcua_SecurityPolicy sp;
    cpkt_opcua_PubSubSecurityPolicy psp;
    cpkt_opcua_AccessControl ac;
    cpkt_opcua_HistoryDatabase hdb;
    cpkt_opcua_GlobalNodeLifecycle glc;
    cpkt_opcua_PubSubConfiguration pubsub;
  } record;
};
static void *cpkt_cfg_view_native(cpkt_cfg_plugin_view *view) {
  UA_ClientConfig *client = view->client ? view->client->native : NULL;
  UA_ServerConfig *server = view->server ? view->server->native : NULL;
  switch (view->kind) {
  case CPKT_CFG_CLIENT_CG:
    return client ? &client->certificateVerification : NULL;
  case CPKT_CFG_CHANNEL_CG:
    return server ? &server->secureChannelPKI : NULL;
  case CPKT_CFG_SESSION_CG:
    return server ? &server->sessionPKI : NULL;
  case CPKT_CFG_CLIENT_SP:
    return client && view->index < client->securityPoliciesSize
               ? &client->securityPolicies[view->index]
               : NULL;
  case CPKT_CFG_AUTH_SP:
    return client && view->index < client->authSecurityPoliciesSize
               ? &client->authSecurityPolicies[view->index]
               : NULL;
  case CPKT_CFG_SERVER_SP:
    return server && view->index < server->securityPoliciesSize
               ? &server->securityPolicies[view->index]
               : NULL;
  case CPKT_CFG_PSCONFIG:
    return server ? &server->pubSubConfig : NULL;
  case CPKT_CFG_AC:
    return server ? &server->accessControl : NULL;
  case CPKT_CFG_HDB:
    return server ? &server->historyDatabase : NULL;
  case CPKT_CFG_GLC:
    return server ? server->nodeLifecycle : NULL;
  case CPKT_CFG_PSP:
    return server && view->index < server->pubSubConfig.securityPoliciesSize
               ? &server->pubSubConfig.securityPolicies[view->index]
               : NULL;
  }
  return NULL;
}
static void cpkt_cfg_view_enter(cpkt_cfg_plugin_view *view) {
  if (view->client)
    cpkt_cfg_client_enter(view->client);
  else
    cpkt_cfg_server_enter(view->server);
}
static void cpkt_cfg_view_leave(cpkt_cfg_plugin_view *view) {
  if (view->client)
    cpkt_cfg_client_leave(view->client);
  else
    cpkt_cfg_server_leave(view->server);
}
static int cpkt_cfg_view_mutable(cpkt_cfg_plugin_view *view) {
  return view->client ? cpkt_cfg_client_mutable(view->client)
                      : cpkt_cfg_server_mutable(view->server);
}
static void cpkt_cfg_view_error(cpkt_cfg_plugin_view *view,
                                UA_StatusCode status) {
  const UA_Logger *logger = view->client ? view->client->native->logging
                                         : view->server->native->logging;
  UA_LOG_ERROR(logger, UA_LOGCATEGORY_SECURITYPOLICY,
               "C89 configuration plugin operation failed: %08lx",
               (unsigned long)status);
}
static void cpkt_cfg_view_metadata_clear(cpkt_cfg_plugin_view *view) {
  if (view->kind <= CPKT_CFG_SESSION_CG) {
    cpkt_opcua_NodeId_clear(&view->record.cg.certificateGroupId);
  } else if (view->kind == CPKT_CFG_PSP) {
    cpkt_psp_metadata_clear(&view->record.psp);
  } else if (view->kind == CPKT_CFG_PSCONFIG) {
    size_t i;
    for (i = 0; i < view->record.pubsub.securityPoliciesSize; ++i)
      cpkt_psp_metadata_clear(&view->record.pubsub.securityPolicies[i]);
    UA_free(view->record.pubsub.securityPolicies);
  } else if (view->kind == CPKT_CFG_AC) {
    cpkt_opcua_array_delete(view->record.ac.userTokenPolicies,
                            view->record.ac.userTokenPoliciesSize,
                            &cpkt_types[CPKT_OPCUA_TYPES_USERTOKENPOLICY]);
  } else if (view->kind < CPKT_CFG_AC) {
    cpkt_sp_metadata_clear(&view->record.sp);
  }
  memset(&view->record, 0, sizeof(view->record));
}
/* Native configuration loops call policy clear even for an empty array slot.
 * Keep a valid destructor after explicit view clear; no backend state remains.
 */
static void cpkt_cfg_empty_sp_clear(UA_SecurityPolicy *policy) { (void)policy; }
static void cpkt_cfg_empty_psp_clear(UA_PubSubSecurityPolicy *policy) {
  (void)policy;
}
static void cpkt_cfg_view_clear_backend(cpkt_cfg_plugin_view *view) {
  void *native = cpkt_cfg_view_native(view);
  if (!native)
    return;
  if (!cpkt_cfg_view_mutable(view)) {
    cpkt_cfg_view_error(view, UA_STATUSCODE_BADINVALIDSTATE);
    return;
  }
  cpkt_cfg_view_enter(view);
  if (view->kind <= CPKT_CFG_SESSION_CG) {
    UA_CertificateGroup *group = (UA_CertificateGroup *)native;
    if (group->clear)
      group->clear(group);
    memset(group, 0, sizeof(*group));
  } else if (view->kind == CPKT_CFG_PSP) {
    UA_PubSubSecurityPolicy *policy = (UA_PubSubSecurityPolicy *)native;
    if (policy->clear)
      policy->clear(policy);
    memset(policy, 0, sizeof(*policy));
    policy->clear = cpkt_cfg_empty_psp_clear;
  } else if (view->kind == CPKT_CFG_AC) {
    UA_AccessControl *plugin = (UA_AccessControl *)native;
    if (plugin->clear)
      plugin->clear(plugin);
    memset(plugin, 0, sizeof(*plugin));
  } else if (view->kind == CPKT_CFG_HDB) {
    UA_HistoryDatabase *plugin = (UA_HistoryDatabase *)native;
    if (plugin->clear)
      plugin->clear(plugin);
    memset(plugin, 0, sizeof(*plugin));
  } else if (view->kind == CPKT_CFG_GLC) {
    view->server->native->nodeLifecycle = NULL;
  } else {
    UA_SecurityPolicy *policy = (UA_SecurityPolicy *)native;
    if (policy->clear)
      policy->clear(policy);
    memset(policy, 0, sizeof(*policy));
    policy->clear = cpkt_cfg_empty_sp_clear;
  }
  cpkt_cfg_view_metadata_clear(view);
  cpkt_cfg_view_leave(view);
}
static UA_StatusCode cpkt_cfg_view_refresh(cpkt_cfg_plugin_view *view);
static UA_StatusCode cpkt_cfg_view_get(cpkt_opcua_ClientConfig *,
                                       cpkt_opcua_ServerConfig *, int, size_t,
                                       cpkt_cfg_plugin_view **);
static UA_StatusCode cpkt_cfg_sp_replace(UA_SecurityPolicy **, size_t *,
                                         cpkt_opcua_SecurityPolicy *, size_t);
static UA_StatusCode cpkt_cfg_psp_replace(UA_PubSubSecurityPolicy **, size_t *,
                                          cpkt_opcua_PubSubSecurityPolicy *,
                                          size_t);

static cpkt_cfg_plugin_view *cpkt_cfg_global_view(cpkt_opcua_server *server) {
  cpkt_cfg_plugin_view *view;
  if (!server || !server->typed_config)
    return NULL;
  for (view = server->typed_config->views; view; view = view->next)
    if (view->kind == CPKT_CFG_GLC)
      return view;
  return NULL;
}
static UA_StatusCode
cpkt_cfg_pubsub_lifecycle(cpkt_opcua_server *server, const cpkt_opcua_NodeId id,
                          const cpkt_opcua_PubSubComponentType type,
                          cpkt_opcua_Boolean remove) {
  UA_NodeId native_id;
  UA_ServerConfig *native;
  UA_StatusCode status;
  if (!server || !server->server || !server->typed_config ||
      !cpkt_stock_node_valid(&id))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  native = cpkt_cfg_server_native(server);
  if (!native->pubSubConfig.componentLifecycleCallback)
    return UA_STATUSCODE_BADNOTSUPPORTED;
  native_id = cpkt_nodeid_view(&id);
  cpkt_cfg_server_enter(server->typed_config);
  status = native->pubSubConfig.componentLifecycleCallback(
      server->server, native_id, (UA_PubSubComponentType)type, remove);
  cpkt_cfg_server_leave(server->typed_config);
  return status;
}
static void cpkt_cfg_pubsub_before(cpkt_opcua_server *server,
                                   const cpkt_opcua_NodeId id,
                                   cpkt_opcua_PubSubState *target) {
  UA_PubSubState native_target;
  UA_ServerConfig *native;
  if (!server || !server->server || !server->typed_config || !target ||
      !cpkt_stock_node_valid(&id))
    return;
  native = cpkt_cfg_server_native(server);
  if (!native->pubSubConfig.beforeStateChangeCallback)
    return;
  native_target = (UA_PubSubState)*target;
  cpkt_cfg_server_enter(server->typed_config);
  native->pubSubConfig.beforeStateChangeCallback(
      server->server, cpkt_nodeid_view(&id), &native_target);
  *target = (cpkt_opcua_PubSubState)native_target;
  cpkt_cfg_server_leave(server->typed_config);
}
static void cpkt_cfg_pubsub_after(cpkt_opcua_server *server,
                                  const cpkt_opcua_NodeId id,
                                  cpkt_opcua_PubSubState state,
                                  cpkt_opcua_StatusCode status) {
  UA_ServerConfig *native;
  if (!server || !server->server || !server->typed_config ||
      !cpkt_stock_node_valid(&id))
    return;
  native = cpkt_cfg_server_native(server);
  if (!native->pubSubConfig.stateChangeCallback)
    return;
  cpkt_cfg_server_enter(server->typed_config);
  native->pubSubConfig.stateChangeCallback(
      server->server, cpkt_nodeid_view(&id), (UA_PubSubState)state, status);
  cpkt_cfg_server_leave(server->typed_config);
}
#include "opcua_config_plugins_metadata.inc"

static UA_StatusCode cpkt_cfg_view_refresh(cpkt_cfg_plugin_view *view) {
  const UA_Logger *logger;
  void *native = cpkt_cfg_view_native(view);
  UA_StatusCode status;
  if (!native)
    return UA_STATUSCODE_BADNOTFOUND;
  if (view->kind <= CPKT_CFG_SESSION_CG) {
    UA_CertificateGroup *group = (UA_CertificateGroup *)native;
    cpkt_opcua_NodeId id;
    memset(&id, 0, sizeof(id));
    status = cpkt_convert(&group->certificateGroupId, &id,
                          &cpkt_types[CPKT_OPCUA_TYPES_NODEID], 0, 0);
    if (status) {
      cpkt_opcua_NodeId_clear(&id);
      return status;
    }
    cpkt_opcua_NodeId_clear(&view->record.cg.certificateGroupId);
    view->record.cg.certificateGroupId = id;
    cpkt_cfg_view_cg_assign(view, group);
    logger = group->logging;
  } else if (view->kind == CPKT_CFG_PSP) {
    UA_PubSubSecurityPolicy *policy = (UA_PubSubSecurityPolicy *)native;
    status = cpkt_psp_metadata_read(policy, &view->record.psp);
    if (status)
      return status;
    cpkt_cfg_view_psp_assign(view, policy);
    logger = policy->logger;
  } else if (view->kind == CPKT_CFG_PSCONFIG) {
    UA_PubSubConfiguration *config = (UA_PubSubConfiguration *)native;
    cpkt_opcua_PubSubSecurityPolicy *policies = NULL;
    cpkt_cfg_plugin_view *policy_view;
    size_t i;
    if (config->securityPoliciesSize > (size_t)-1 / sizeof(*policies))
      return UA_STATUSCODE_BADOUTOFMEMORY;
    if (config->securityPoliciesSize) {
      policies = (cpkt_opcua_PubSubSecurityPolicy *)UA_calloc(
          config->securityPoliciesSize, sizeof(*policies));
      if (!policies)
        return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    for (i = 0; i < config->securityPoliciesSize; ++i) {
      status =
          cpkt_cfg_view_get(NULL, view->server, CPKT_CFG_PSP, i, &policy_view);
      if (!status) {
        policies[i] = policy_view->record.psp;
        memset(&policies[i].policyUri, 0, sizeof(policies[i].policyUri));
        status =
            cpkt_psp_metadata_read(&config->securityPolicies[i], &policies[i]);
      }
      if (status) {
        size_t done;
        for (done = 0; done <= i; ++done)
          cpkt_psp_metadata_clear(&policies[done]);
        UA_free(policies);
        return status;
      }
    }
    cpkt_cfg_view_metadata_clear(view);
    view->record.pubsub.securityPolicies = policies;
    view->record.pubsub.securityPoliciesSize = config->securityPoliciesSize;
    view->record.pubsub.enableDeltaFrames = config->enableDeltaFrames;
    view->record.pubsub.enableInformationModelMethods =
        config->enableInformationModelMethods;
    view->record.pubsub.componentLifecycleCallback =
        config->componentLifecycleCallback ? cpkt_cfg_pubsub_lifecycle : NULL;
    view->record.pubsub.beforeStateChangeCallback =
        config->beforeStateChangeCallback ? cpkt_cfg_pubsub_before : NULL;
    view->record.pubsub.stateChangeCallback =
        config->stateChangeCallback ? cpkt_cfg_pubsub_after : NULL;
    return 0;
  } else if (view->kind == CPKT_CFG_AC) {
    UA_AccessControl *plugin = (UA_AccessControl *)native;
    void *policies = NULL;
    status = cpkt_array(plugin->userTokenPolicies,
                        plugin->userTokenPoliciesSize, &policies,
                        &cpkt_types[CPKT_OPCUA_TYPES_USERTOKENPOLICY], 0, 0);
    if (status)
      return status;
    cpkt_cfg_view_metadata_clear(view);
    view->record.ac.userTokenPolicies = (cpkt_opcua_UserTokenPolicy *)policies;
    view->record.ac.userTokenPoliciesSize = plugin->userTokenPoliciesSize;
    cpkt_cfg_view_ac_assign(view, plugin);
    return 0;
  } else if (view->kind == CPKT_CFG_HDB) {
    cpkt_cfg_view_hdb_assign(view, (UA_HistoryDatabase *)native);
    return 0;
  } else if (view->kind == CPKT_CFG_GLC) {
    cpkt_cfg_view_glc_assign(view, (UA_GlobalNodeLifecycle *)native);
    return 0;
  } else {
    UA_SecurityPolicy *policy = (UA_SecurityPolicy *)native;
    status = cpkt_sp_metadata_read(policy, &view->record.sp);
    if (status)
      return status;
    cpkt_cfg_view_sp_assign(view, policy);
    logger = policy->logger;
  }
  memset(&view->logger, 0, sizeof(view->logger));
  if (logger && logger->log == cpkt_logger_log && logger->context)
    view->logger = ((struct cpkt_opcua_logger *)logger->context)->config;
  if (view->kind <= CPKT_CFG_SESSION_CG)
    view->record.cg.logging =
        logger && logger->log == cpkt_logger_log ? &view->logger : NULL;
  else if (view->kind == CPKT_CFG_PSP)
    view->record.psp.logger =
        logger && logger->log == cpkt_logger_log ? &view->logger : NULL;
  else
    view->record.sp.logger =
        logger && logger->log == cpkt_logger_log ? &view->logger : NULL;
  return 0;
}
static void cpkt_cfg_views_clear(cpkt_cfg_plugin_view **head) {
  cpkt_cfg_plugin_view *view;
  while (*head) {
    view = *head;
    *head = view->next;
    cpkt_cfg_view_metadata_clear(view);
    UA_free(view);
  }
}
static UA_StatusCode cpkt_cfg_view_get(cpkt_opcua_ClientConfig *client,
                                       cpkt_opcua_ServerConfig *server,
                                       int kind, size_t index,
                                       cpkt_cfg_plugin_view **output) {
  cpkt_cfg_plugin_view **head = client ? &client->views : &server->views;
  cpkt_cfg_plugin_view *view;
  UA_StatusCode status;
  *output = NULL;
  for (view = *head; view; view = view->next)
    if (view->kind == kind && view->index == index)
      break;
  if (!view) {
    view = (cpkt_cfg_plugin_view *)UA_calloc(1, sizeof(*view));
    if (!view)
      return UA_STATUSCODE_BADOUTOFMEMORY;
    view->client = client;
    view->server = server;
    view->kind = kind;
    view->index = index;
    status = cpkt_cfg_view_refresh(view);
    if (status) {
      cpkt_cfg_view_metadata_clear(view);
      UA_free(view);
      return status;
    }
    view->next = *head;
    *head = view;
  } else {
    status = cpkt_cfg_view_refresh(view);
    if (status)
      return status;
  }
  *output = view;
  return 0;
}
cpkt_opcua_StatusCode cpkt_opcua_ClientConfig_getCertificateGroup(
    cpkt_opcua_ClientConfig *config, cpkt_opcua_CertificateGroup **group) {
  cpkt_cfg_plugin_view *view;
  UA_StatusCode status;
  if (!group)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  *group = NULL;
  if (!config || !config->native)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  status = cpkt_cfg_view_get(config, NULL, CPKT_CFG_CLIENT_CG, 0, &view);
  if (!status)
    *group = &view->record.cg;
  return status;
}
cpkt_opcua_StatusCode cpkt_opcua_ServerConfig_getCertificateGroup(
    cpkt_opcua_ServerConfig *config, cpkt_opcua_Boolean session,
    cpkt_opcua_CertificateGroup **group) {
  cpkt_cfg_plugin_view *view;
  UA_StatusCode status;
  if (!group)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  *group = NULL;
  if (!config || !config->native)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  status = cpkt_cfg_view_get(
      NULL, config, session ? CPKT_CFG_SESSION_CG : CPKT_CFG_CHANNEL_CG, 0,
      &view);
  if (!status)
    *group = &view->record.cg;
  return status;
}
cpkt_opcua_StatusCode cpkt_opcua_ClientConfig_setCertificateGroup(
    cpkt_opcua_ClientConfig *config, cpkt_opcua_CertificateGroup *group) {
  UA_StatusCode status;
  if (!config || !config->native)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!cpkt_cfg_client_mutable(config))
    return UA_STATUSCODE_BADINVALIDSTATE;
  cpkt_cfg_client_enter(config);
  status = cpkt_cg_install(&config->native->certificateVerification, group);
  cpkt_cfg_client_leave(config);
  return status;
}
cpkt_opcua_StatusCode cpkt_opcua_ServerConfig_setCertificateGroup(
    cpkt_opcua_ServerConfig *config, cpkt_opcua_Boolean session,
    cpkt_opcua_CertificateGroup *group) {
  UA_StatusCode status;
  if (!config || !config->native)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!cpkt_cfg_server_mutable(config))
    return UA_STATUSCODE_BADINVALIDSTATE;
  cpkt_cfg_server_enter(config);
  status = cpkt_cg_install(session ? &config->native->sessionPKI
                                   : &config->native->secureChannelPKI,
                           group);
  cpkt_cfg_server_leave(config);
  return status;
}
static UA_StatusCode cpkt_cfg_sp_replace(UA_SecurityPolicy **array,
                                         size_t *count,
                                         cpkt_opcua_SecurityPolicy *source,
                                         size_t size) {
  UA_SecurityPolicy *staged = NULL;
  cpkt_opcua_SecurityPolicy value;
  size_t staged_size = 0, i;
  UA_StatusCode status = 0;
  if (size && (!source || source == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  for (i = 0; i < size; ++i) {
    value = source[i];
    status = cpkt_sp_append(&staged, &staged_size, &value);
    if (status)
      break;
  }
  if (status) {
    for (i = 0; i < staged_size; ++i) {
      cpkt_sp_custom *holder = (cpkt_sp_custom *)staged[i].policyContext;
      memset(&holder->plugin, 0, sizeof(holder->plugin));
      cpkt_sp_custom_clear(&staged[i]);
    }
    UA_free(staged);
    return status;
  }
  for (i = 0; i < *count; ++i)
    if ((*array)[i].clear)
      (*array)[i].clear(&(*array)[i]);
  UA_free(*array);
  *array = staged;
  *count = staged_size;
  for (i = 0; i < size; ++i)
    memset(&source[i], 0, sizeof(source[i]));
  return 0;
}
static UA_StatusCode
cpkt_cfg_psp_replace(UA_PubSubSecurityPolicy **array, size_t *count,
                     cpkt_opcua_PubSubSecurityPolicy *source, size_t size) {
  UA_PubSubSecurityPolicy *staged = NULL;
  cpkt_opcua_PubSubSecurityPolicy value;
  size_t staged_size = 0, i;
  UA_StatusCode status = 0;
  if (size && (!source || source == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  for (i = 0; i < size; ++i) {
    value = source[i];
    status = cpkt_psp_append(&staged, &staged_size, &value);
    if (status)
      break;
  }
  if (status) {
    for (i = 0; i < staged_size; ++i) {
      cpkt_psp_custom *holder = (cpkt_psp_custom *)staged[i].policyContext;
      memset(&holder->plugin, 0, sizeof(holder->plugin));
      cpkt_psp_custom_clear(&staged[i]);
    }
    UA_free(staged);
    return status;
  }
  for (i = 0; i < *count; ++i)
    if ((*array)[i].clear)
      (*array)[i].clear(&(*array)[i]);
  UA_free(*array);
  *array = staged;
  *count = staged_size;
  for (i = 0; i < size; ++i)
    memset(&source[i], 0, sizeof(source[i]));
  return 0;
}
cpkt_opcua_StatusCode cpkt_opcua_ServerConfig_setAccessControl(
    cpkt_opcua_ServerConfig *config, const cpkt_opcua_AccessControl *plugin) {
  cpkt_opcua_server *owner;
  UA_StatusCode status;
  if (!config || !config->native || !plugin)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!cpkt_cfg_server_mutable(config))
    return UA_STATUSCODE_BADINVALIDSTATE;
  owner = cpkt_cfg_server_owner(config);
  if (!owner)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  cpkt_cfg_server_enter(config);
  status = cpkt_ac_install(&config->native->accessControl, owner, plugin);
  cpkt_cfg_server_leave(config);
  return status;
}
cpkt_opcua_StatusCode cpkt_opcua_ServerConfig_setHistoryDatabase(
    cpkt_opcua_ServerConfig *config, const cpkt_opcua_HistoryDatabase *plugin) {
  cpkt_opcua_server *owner;
  UA_StatusCode status;
  if (!config || !config->native || !plugin)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!cpkt_cfg_server_mutable(config))
    return UA_STATUSCODE_BADINVALIDSTATE;
  owner = cpkt_cfg_server_owner(config);
  if (!owner)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  cpkt_cfg_server_enter(config);
  status = cpkt_hdb_install(&config->native->historyDatabase, owner, plugin);
  cpkt_cfg_server_leave(config);
  return status;
}
cpkt_opcua_StatusCode cpkt_opcua_ServerConfig_setGlobalNodeLifecycle(
    cpkt_opcua_ServerConfig *config,
    const cpkt_opcua_GlobalNodeLifecycle *lifecycle) {
  cpkt_opcua_server *owner;
  if (!config || !config->native || cpkt_cfg_global_is_view(lifecycle))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!cpkt_cfg_server_mutable(config))
    return UA_STATUSCODE_BADINVALIDSTATE;
  owner = cpkt_cfg_server_owner(config);
  if (!owner)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  if (lifecycle)
    owner->typed_nodes->global = *lifecycle;
  else
    memset(&owner->typed_nodes->global, 0, sizeof(owner->typed_nodes->global));
  cpkt_nodes_assign_GlobalNodeLifecycle(&owner->typed_nodes->native_global,
                                        &owner->typed_nodes->global);
  config->native->nodeLifecycle =
      lifecycle ? &owner->typed_nodes->native_global : NULL;
  return 0;
}
cpkt_opcua_StatusCode
cpkt_opcua_ClientConfig_copy(const cpkt_opcua_ClientConfig *source,
                             cpkt_opcua_ClientConfig *destination) {
  UA_ClientConfig empty, staged;
  UA_StatusCode status;
  if (!source || !destination || source == destination || !source->native ||
      !destination->native)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (source->depth || destination->owner || destination->depth ||
      (source->owner && source->owner->destroying))
    return UA_STATUSCODE_BADINVALIDSTATE;
  memset(&empty, 0, sizeof(empty));
  if (memcmp(destination->native, &empty, sizeof(empty)))
    return UA_STATUSCODE_BADINVALIDSTATE;
  memset(&staged, 0, sizeof(staged));
  status = UA_ClientConfig_copy(source->native, &staged);
  if (status)
    return status;
  *destination->native = staged;
  destination->callbacks = source->callbacks;
  return 0;
}
cpkt_opcua_StatusCode
cpkt_opcua_ServerConfig_getNodestore(cpkt_opcua_ServerConfig *config,
                                     cpkt_opcua_Nodestore **store) {
  if (!store)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  *store = NULL;
  if (!config || !config->native)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!config->native->nodestore)
    return 0;
  *store = cpkt_store_adopt(config->native->nodestore);
  if (!*store)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  cpkt_cfg_store_moved(config);
  return 0;
}
cpkt_opcua_StatusCode
cpkt_opcua_ServerConfig_setNodestore(cpkt_opcua_ServerConfig *config,
                                     cpkt_opcua_Nodestore *store) {
  UA_Nodestore *old, *replacement;
  cpkt_opcua_Nodestore *old_view;
  if (!config || !config->native)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!cpkt_cfg_server_mutable(config))
    return UA_STATUSCODE_BADINVALIDSTATE;
  replacement = store ? store->native : NULL;
  if (store && (store->depth ||
                (store->configuration_slot &&
                 store->configuration_slot != &config->native->nodestore)))
    return UA_STATUSCODE_BADINVALIDSTATE;
  old = config->native->nodestore;
  old_view = cpkt_store_find(old);
  if (old_view && old_view->depth)
    return UA_STATUSCODE_BADINVALIDSTATE;
  cpkt_cfg_server_enter(config);
  if (store)
    ++store->depth;
  if (old && old != replacement)
    old->free(old);
  config->native->nodestore = replacement;
  cpkt_cfg_store_moved(config);
  if (store)
    --store->depth;
  cpkt_cfg_server_leave(config);
  return 0;
}
static int
cpkt_cfg_pubsub_is_view(const cpkt_opcua_PubSubConfiguration *pubsub) {
  return pubsub &&
         (pubsub->componentLifecycleCallback == cpkt_cfg_pubsub_lifecycle ||
          pubsub->beforeStateChangeCallback == cpkt_cfg_pubsub_before ||
          pubsub->stateChangeCallback == cpkt_cfg_pubsub_after);
}
cpkt_opcua_StatusCode cpkt_opcua_ServerConfig_getPubSubConfiguration(
    cpkt_opcua_ServerConfig *config, cpkt_opcua_PubSubConfiguration **pubsub) {
  cpkt_cfg_plugin_view *view;
  UA_StatusCode status;
  if (!pubsub)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  *pubsub = NULL;
  if (!config || !config->native)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  status = cpkt_cfg_view_get(NULL, config, CPKT_CFG_PSCONFIG, 0, &view);
  if (!status)
    *pubsub = &view->record.pubsub;
  return status;
}
cpkt_opcua_StatusCode cpkt_opcua_ServerConfig_setPubSubConfiguration(
    cpkt_opcua_ServerConfig *config, cpkt_opcua_PubSubConfiguration *pubsub) {
  cpkt_opcua_server *server;
  cpkt_pubsub_owner *owner;
  UA_StatusCode status;
  if (!config || !config->native || !pubsub || cpkt_cfg_pubsub_is_view(pubsub))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!cpkt_cfg_server_mutable(config))
    return UA_STATUSCODE_BADINVALIDSTATE;
  server = cpkt_cfg_server_owner(config);
  if (!server)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  owner = cpkt_pubsub_owner_at(server, &config->native->pubSubConfig);
  if (!owner)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  if (owner->depth || owner->entries)
    return UA_STATUSCODE_BADINVALIDSTATE;
  cpkt_cfg_server_enter(config);
  status = cpkt_cfg_psp_replace(
      &config->native->pubSubConfig.securityPolicies,
      &config->native->pubSubConfig.securityPoliciesSize,
      pubsub->securityPolicies, pubsub->securityPoliciesSize);
  if (!status)
    cpkt_pubsub_callbacks_set(owner, pubsub);
  cpkt_cfg_server_leave(config);
  return status;
}

cpkt_opcua_StatusCode
cpkt_opcua_server_newFromFile_typed(const cpkt_opcua_ByteString json_config,
                                    cpkt_opcua_server **output) {
  cpkt_opcua_ServerConfig *config;
  cpkt_opcua_server *server;
  if (!output)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  *output = NULL;
  if (!cpkt_string_valid(&json_config))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  config = cpkt_opcua_ServerConfig_new();
  if (!config)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  server = cpkt_cfg_server_owner(config);
  if (!server) {
    (void)cpkt_opcua_ServerConfig_delete(config);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  server->server = UA_Server_newFromFile(cpkt_string_view(&json_config));
  if (!server->server) {
    (void)cpkt_opcua_ServerConfig_delete(config);
    return UA_STATUSCODE_BADINTERNALERROR;
  }
  config->pending_owner = NULL;
  config->owner = server;
  cpkt_cfg_server_rebind(config, UA_Server_getConfig(server->server));
  cpkt_cfg_loop_moved(&config->native->eventLoop,
                      config->native->externalEventLoop);
  server->typed_config_clear = cpkt_cfg_server_cleanup;
  server->typed_eventloop_forget_config = cpkt_loop_forget_config;
  (void)pthread_mutex_lock(&cpkt_nodes_mutex);
  server->typed_nodes->native = server->server;
  server->typed_nodes->event_loop = config->native->eventLoop;
  (void)pthread_mutex_unlock(&cpkt_nodes_mutex);
  *output = server;
  return 0;
}
#if defined(__linux__) || defined(_WIN32)
cpkt_opcua_StatusCode cpkt_opcua_ServerConfig_addSecurityPolicy_Filestore(
    cpkt_opcua_ServerConfig *config, cpkt_opcua_SecurityPolicy *innerPolicy,
    const cpkt_opcua_String storePath) {
  UA_SecurityPolicy *inner = NULL;
  cpkt_sp_custom *holder;
  cpkt_opcua_SecurityPolicy staged;
  UA_StatusCode status;
  size_t count = 0;
  if (!config || !config->native || !innerPolicy ||
      !cpkt_string_valid(&storePath))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!cpkt_cfg_server_mutable(config))
    return UA_STATUSCODE_BADINVALIDSTATE;
  staged = *innerPolicy;
  status = cpkt_sp_append(&inner, &count, &staged);
  if (status)
    return status;
  holder = (cpkt_sp_custom *)inner->policyContext;
  cpkt_cfg_server_enter(config);
  status = UA_ServerConfig_addSecurityPolicy_Filestore(
      config->native, inner, cpkt_string_view(&storePath));
  if (status) {
    memset(&holder->plugin, 0, sizeof(holder->plugin));
    cpkt_sp_custom_clear(inner);
    UA_free(inner);
  } else {
    memset(innerPolicy, 0, sizeof(*innerPolicy));
  }
  cpkt_cfg_server_leave(config);
  return status;
}
#endif

#endif
