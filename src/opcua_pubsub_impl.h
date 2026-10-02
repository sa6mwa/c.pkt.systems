/* Public PubSub dispatch records. Native contexts and policies remain borrowed.
 * The existing server registry supplies facade identity without replacing the
 * application/server context. Callbacks run without its registry mutex. */
static void cpkt_pubsub_entry_free(cpkt_pubsub_entry *entry) {
  if (entry->policy) {
    cpkt_psp_custom_clear(entry->policy);
    UA_free(entry->policy);
  }
  UA_NodeId_clear(&entry->id);
  UA_free(entry);
}
static void cpkt_pubsub_cleanup(cpkt_nodes_owner *nodes) {
  cpkt_pubsub_owner *owner = (cpkt_pubsub_owner *)nodes->pubsub;
  cpkt_pubsub_entry *entry, *next;
  if (!owner)
    return;
  for (entry = owner->entries; entry; entry = next) {
    next = entry->next;
    cpkt_pubsub_entry_free(entry);
  }
  UA_free(owner);
  nodes->pubsub = NULL;
}
static cpkt_pubsub_owner *cpkt_pubsub_owner_get(UA_Server *native) {
  cpkt_nodes_owner *nodes = cpkt_nodes_find_owner(native);
  return nodes ? (cpkt_pubsub_owner *)nodes->pubsub : NULL;
}
static cpkt_pubsub_entry *cpkt_pubsub_find(cpkt_opcua_server *server,
                                           const UA_NodeId *id) {
  cpkt_pubsub_owner *owner = cpkt_pubsub_owner_get(server->server);
  cpkt_pubsub_entry *entry;
  if (!owner || !id)
    return NULL;
  for (entry = owner->entries; entry; entry = entry->next)
    if (!UA_NodeId_isNull(&entry->id) && UA_NodeId_equal(id, &entry->id))
      return entry;
  return NULL;
}
static void cpkt_pubsub_enter(void *context) {
  cpkt_pubsub_owner *owner = (cpkt_pubsub_owner *)context;
  ++owner->depth;
}
static void cpkt_pubsub_leave(void *context) {
  cpkt_pubsub_owner *owner = (cpkt_pubsub_owner *)context;
  --owner->depth;
}
static UA_StatusCode cpkt_pubsub_sync(cpkt_pubsub_owner *owner) {
  cpkt_pubsub_entry *entry;
  UA_PubSubConfiguration *config =
      &cpkt_cfg_server_native(owner->server)->pubSubConfig;
  cpkt_psp_custom *holder;
  cpkt_opcua_PubSubSecurityPolicy *source;
  UA_StatusCode status;
  unsigned int owned;
  size_t i;
  for (entry = owner->entries; entry; entry = entry->next) {
    if (!entry->policy)
      continue;
    holder = (cpkt_psp_custom *)entry->policy->policyContext;
    owned = (unsigned int)-1;
    status = cpkt_psp_metadata_write(entry->policy, entry->borrowed_policy,
                                     &holder->logger, &owned);
    if (status)
      return status;
    cpkt_psp_native_assign(entry->policy, entry->borrowed_policy);
  }
  for (i = 0; i < config->securityPoliciesSize; ++i) {
    UA_PubSubSecurityPolicy *policy = &config->securityPolicies[i];
    if (policy->clear != cpkt_psp_custom_clear)
      continue;
    holder = (cpkt_psp_custom *)policy->policyContext;
    source = holder->borrowed ? holder->borrowed : &holder->plugin;
    owned = (unsigned int)-1;
    status = cpkt_psp_metadata_write(policy, source, &holder->logger, &owned);
    if (status)
      return status;
    cpkt_psp_native_assign(policy, source);
  }
  return 0;
}
static void cpkt_pubsub_prune(cpkt_pubsub_owner *owner) {
  cpkt_pubsub_entry **slot = &owner->entries, *entry, *newer;
  UA_PubSubComponentType type;
  UA_StatusCode status;
  int obsolete;
  if (owner->depth)
    return;
  while (*slot) {
    entry = *slot;
    obsolete = 0;
    if (entry->scoped || !entry->committed) {
      slot = &entry->next;
      continue;
    }
    status = UA_Server_getPubSubComponentType(owner->server->server, entry->id,
                                              &type);
    if (status == UA_STATUSCODE_BADNOTFOUND ||
        status == UA_STATUSCODE_BADNODEIDUNKNOWN)
      obsolete = 1;
    else if (!status) {
      for (newer = owner->entries; newer != entry; newer = newer->next)
        if (newer->committed && !newer->scoped &&
            UA_NodeId_equal(&newer->id, &entry->id)) {
          obsolete = 1;
          break;
        }
    }
    if (obsolete) {
      *slot = entry->next;
      cpkt_pubsub_entry_free(entry);
    } else
      slot = &entry->next;
  }
}
static UA_StatusCode cpkt_pubsub_prepare_server(cpkt_opcua_server *server) {
  cpkt_pubsub_owner *owner = cpkt_pubsub_owner_get(server->server);
  UA_StatusCode status;
  if (!owner)
    return 0;
  cpkt_pubsub_prune(owner);
  status = cpkt_pubsub_sync(owner);
  if (!status)
    ++owner->depth;
  return status;
}
/* Teardown must retain existing callback metadata even when synchronization
 * would allocate and fail. It does not submit a new policy configuration. */
static void cpkt_pubsub_retain_server(cpkt_opcua_server *server) {
  cpkt_pubsub_owner *owner = cpkt_pubsub_owner_get(server->server);
  if (owner)
    ++owner->depth;
}
static void cpkt_pubsub_finish_server(cpkt_opcua_server *server) {
  cpkt_pubsub_owner *owner = cpkt_pubsub_owner_get(server->server);
  if (!owner)
    return;
  --owner->depth;
  cpkt_pubsub_prune(owner);
}
static UA_StatusCode cpkt_pubsub_lifecycle(UA_Server *native,
                                           const UA_NodeId id,
                                           const UA_PubSubComponentType type,
                                           UA_Boolean remove) {
  cpkt_pubsub_owner *owner = cpkt_pubsub_owner_get(native);
  cpkt_pubsub_entry *entry;
  UA_StatusCode status = 0;
  cpkt_opcua_NodeId public_id;
  if (!owner)
    return UA_STATUSCODE_BADINTERNALERROR;
  if (!remove) {
    for (entry = owner->scope; entry; entry = entry->scoped_previous) {
      if (entry->type == type && UA_NodeId_isNull(&entry->id)) {
        status = UA_NodeId_copy(&id, &entry->id);
        if (status)
          return status;
        break;
      }
    }
  }
  ++owner->depth;
  ++owner->server->typed_config_depth;
  if (owner->original.componentLifecycleCallback)
    status =
        owner->original.componentLifecycleCallback(native, id, type, remove);
  if (!status && owner->callbacks.componentLifecycleCallback) {
    cpkt_hb_borrow_node(&id, &public_id);
    status = owner->callbacks.componentLifecycleCallback(
        owner->server, public_id, (cpkt_opcua_PubSubComponentType)type, remove);
  }
  --owner->server->typed_config_depth;
  --owner->depth;
  return status;
}
static void cpkt_pubsub_before_state(UA_Server *native, const UA_NodeId id,
                                     UA_PubSubState *target) {
  cpkt_pubsub_owner *owner = cpkt_pubsub_owner_get(native);
  cpkt_opcua_NodeId public_id;
  cpkt_opcua_PubSubState public_target;
  if (!owner)
    return;
  ++owner->depth;
  ++owner->server->typed_config_depth;
  if (owner->original.beforeStateChangeCallback)
    owner->original.beforeStateChangeCallback(native, id, target);
  if (owner->callbacks.beforeStateChangeCallback) {
    cpkt_hb_borrow_node(&id, &public_id);
    public_target = (cpkt_opcua_PubSubState)*target;
    owner->callbacks.beforeStateChangeCallback(owner->server, public_id,
                                               &public_target);
    *target = (UA_PubSubState)public_target;
  }
  if (cpkt_pubsub_sync(owner)) {
    *target = UA_PUBSUBSTATE_ERROR;
    UA_LOG_ERROR(
        UA_Server_getConfig(native)->logging, UA_LOGCATEGORY_PUBSUB,
        "C89 PubSub policy metadata conversion failed before state change");
  }
  --owner->server->typed_config_depth;
  --owner->depth;
}
static void cpkt_pubsub_after_state(UA_Server *native, const UA_NodeId id,
                                    UA_PubSubState state,
                                    UA_StatusCode status) {
  cpkt_pubsub_owner *owner = cpkt_pubsub_owner_get(native);
  cpkt_opcua_NodeId public_id;
  if (!owner)
    return;
  ++owner->depth;
  ++owner->server->typed_config_depth;
  if (owner->original.stateChangeCallback)
    owner->original.stateChangeCallback(native, id, state, status);
  if (owner->callbacks.stateChangeCallback) {
    cpkt_hb_borrow_node(&id, &public_id);
    owner->callbacks.stateChangeCallback(owner->server, public_id,
                                         (cpkt_opcua_PubSubState)state, status);
  }
  --owner->server->typed_config_depth;
  --owner->depth;
}
static UA_StatusCode cpkt_pubsub_state(UA_Server *native, const UA_NodeId id,
                                       void *context, UA_PubSubState *state,
                                       UA_PubSubState target) {
  cpkt_pubsub_owner *owner = cpkt_pubsub_owner_get(native);
  cpkt_pubsub_entry *entry;
  cpkt_pubsub_state_fn fn;
  cpkt_opcua_NodeId public_id;
  cpkt_opcua_PubSubState public_state;
  UA_StatusCode status;
  if (!owner)
    return UA_STATUSCODE_BADINTERNALERROR;
  entry = cpkt_pubsub_find(owner->server, &id);
  if (!entry || !entry->state)
    return UA_STATUSCODE_BADINTERNALERROR;
  fn = entry->state;
  cpkt_hb_borrow_node(&id, &public_id);
  public_state = (cpkt_opcua_PubSubState)*state;
  ++owner->depth;
  ++owner->server->typed_config_depth;
  status = fn(owner->server, public_id, context, &public_state,
              (cpkt_opcua_PubSubState)target);
  if (!status)
    status = cpkt_pubsub_sync(owner);
  --owner->server->typed_config_depth;
  --owner->depth;
  *state = status ? UA_PUBSUBSTATE_ERROR : (UA_PubSubState)public_state;
  return status;
}
static cpkt_pubsub_owner *cpkt_pubsub_owner_at(cpkt_opcua_server *server,
                                               UA_PubSubConfiguration *native) {
  cpkt_nodes_owner *nodes;
  cpkt_pubsub_owner *owner;
  nodes = cpkt_nodes_owner_new(server);
  if (!nodes)
    return NULL;
  owner = (cpkt_pubsub_owner *)nodes->pubsub;
  if (!owner) {
    owner = (cpkt_pubsub_owner *)UA_calloc(1, sizeof(*owner));
    if (!owner)
      return NULL;
    owner->server = server;
    owner->original = *native;
    nodes->pubsub = owner;
    nodes->pubsub_cleanup = cpkt_pubsub_cleanup;
    server->typed_pubsub_prepare = cpkt_pubsub_prepare_server;
    server->typed_pubsub_finish = cpkt_pubsub_finish_server;
    server->typed_pubsub_retain = cpkt_pubsub_retain_server;
    if (server->typed_loop_depth && !server->typed_loop_pubsub_held) {
      ++owner->depth;
      server->typed_loop_pubsub_held = 1;
    }
    native->componentLifecycleCallback = cpkt_pubsub_lifecycle;
    native->beforeStateChangeCallback = cpkt_pubsub_before_state;
    native->stateChangeCallback = cpkt_pubsub_after_state;
  }
  return owner;
}
static cpkt_pubsub_owner *cpkt_pubsub_owner_new(cpkt_opcua_server *server) {
  return cpkt_pubsub_owner_at(
      server, &UA_Server_getConfig(server->server)->pubSubConfig);
}
static UA_StatusCode cpkt_pubsub_stage(cpkt_opcua_server *server,
                                       UA_PubSubComponentType type,
                                       const UA_NodeId *id,
                                       cpkt_pubsub_state_fn state,
                                       cpkt_pubsub_entry **out) {
  cpkt_pubsub_owner *owner;
  cpkt_pubsub_entry *entry;
  UA_StatusCode status;
  *out = NULL;
  owner = cpkt_pubsub_owner_new(server);
  if (!owner)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  entry = (cpkt_pubsub_entry *)UA_calloc(1, sizeof(*entry));
  if (!entry)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  if (id) {
    status = UA_NodeId_copy(id, &entry->id);
    if (status) {
      cpkt_pubsub_entry_free(entry);
      return status;
    }
  }
  entry->owner = owner;
  entry->type = type;
  entry->state = state;
  entry->updating = id != NULL;
  entry->next = owner->entries;
  owner->entries = entry;
  entry->scoped_previous = owner->scope;
  owner->scope = entry;
  entry->scoped = 1;
  *out = entry;
  return 0;
}
static void cpkt_pubsub_finish(cpkt_pubsub_entry *entry, int success,
                               const UA_NodeId *id) {
  cpkt_pubsub_owner *owner;
  cpkt_pubsub_entry **slot;
  UA_PubSubComponentType actual;
  if (!entry)
    return;
  owner = entry->owner;
  owner->scope = entry->scoped_previous;
  entry->scoped = 0;
  /* Lifecycle runs before output assignment. If native reports a failure but
   * retained the component, retain its dispatch and borrowed-policy reference.
   */
  if (UA_NodeId_isNull(&entry->id) && id && !UA_NodeId_isNull(id))
    (void)UA_NodeId_copy(id, &entry->id);
  if (!success && !entry->updating && !UA_NodeId_isNull(&entry->id) &&
      UA_Server_getPubSubComponentType(owner->server->server, entry->id,
                                       &actual) == UA_STATUSCODE_GOOD)
    success = 1;
  if (success) {
    entry->committed = 1;
    return;
  }
  for (slot = &owner->entries; *slot; slot = &(*slot)->next)
    if (*slot == entry) {
      *slot = entry->next;
      break;
    }
  cpkt_pubsub_entry_free(entry);
}
static UA_StatusCode cpkt_pubsub_policy(cpkt_pubsub_entry *entry,
                                        cpkt_opcua_PubSubSecurityPolicy *source,
                                        UA_PubSubSecurityPolicy **out) {
  cpkt_psp_custom *store;
  UA_PubSubSecurityPolicy *native;
  unsigned int owned = (unsigned int)-1;
  UA_StatusCode status;
  if (!source || !cpkt_logger_valid(source->logger))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  native = (UA_PubSubSecurityPolicy *)UA_calloc(1, sizeof(*native));
  if (!native)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  store = (cpkt_psp_custom *)UA_calloc(1, sizeof(*store));
  if (!store) {
    UA_free(native);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  native->policyContext = store;
  store->borrowed = source;
  store->dispatch = entry->owner;
  store->enter = cpkt_pubsub_enter;
  store->leave = cpkt_pubsub_leave;
  status = cpkt_psp_metadata_write(native, source, &store->logger, &owned);
  if (status) {
    cpkt_psp_custom_clear(native);
    UA_free(native);
    return status;
  }
  cpkt_psp_native_assign(native, source);
  entry->policy = native;
  entry->borrowed_policy = source;
  *out = native;
  return 0;
}
static UA_StatusCode cpkt_ps_publisher_native(const cpkt_opcua_PublisherId *src,
                                              UA_PublisherId *dst) {
  dst->idType = (UA_PublisherIdType)src->idType;
  switch (src->idType) {
  case CPKT_OPCUA_PUBLISHERIDTYPE_BYTE:
    dst->id.byte = src->id.byte;
    break;
  case CPKT_OPCUA_PUBLISHERIDTYPE_UINT16:
    dst->id.uint16 = src->id.uint16;
    break;
  case CPKT_OPCUA_PUBLISHERIDTYPE_UINT32:
    dst->id.uint32 = src->id.uint32;
    break;
  case CPKT_OPCUA_PUBLISHERIDTYPE_UINT64:
    dst->id.uint64 =
        ((UA_UInt64)src->id.uint64.high32 << 32) | src->id.uint64.low32;
    break;
  case CPKT_OPCUA_PUBLISHERIDTYPE_STRING: {
    UA_String view = cpkt_string_view(&src->id.string);
    if (view.length && !view.data)
      return UA_STATUSCODE_BADINVALIDARGUMENT;
    return UA_String_copy(&view, &dst->id.string);
  }
  default:
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  }
  return 0;
}
static UA_StatusCode cpkt_ps_publisher_public(const UA_PublisherId *src,
                                              cpkt_opcua_PublisherId *dst) {
  dst->idType = (cpkt_opcua_PublisherIdType)src->idType;
  switch (src->idType) {
  case UA_PUBLISHERIDTYPE_BYTE:
    dst->id.byte = src->id.byte;
    break;
  case UA_PUBLISHERIDTYPE_UINT16:
    dst->id.uint16 = src->id.uint16;
    break;
  case UA_PUBLISHERIDTYPE_UINT32:
    dst->id.uint32 = src->id.uint32;
    break;
  case UA_PUBLISHERIDTYPE_UINT64:
    dst->id.uint64.high32 = (cpkt_opcua_UInt32)(src->id.uint64 >> 32);
    dst->id.uint64.low32 = (cpkt_opcua_UInt32)src->id.uint64;
    break;
  case UA_PUBLISHERIDTYPE_STRING: {
    cpkt_opcua_String view;
    cpkt_string_take(&view, src->id.string);
    return cpkt_opcua_String_copy(&view, &dst->id.string);
  }
  default:
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  }
  return 0;
}

static int cpkt_cfg_pubsub_is_view(const cpkt_opcua_PubSubConfiguration *);

static void
cpkt_pubsub_callbacks_set(cpkt_pubsub_owner *owner,
                          const cpkt_opcua_PubSubConfiguration *source) {
  UA_PubSubConfiguration *native =
      &cpkt_cfg_server_native(owner->server)->pubSubConfig;
  owner->callbacks = *source;
  owner->callbacks.securityPolicies = NULL;
  owner->callbacks.securityPoliciesSize = 0;
  owner->original.componentLifecycleCallback = NULL;
  owner->original.beforeStateChangeCallback = NULL;
  owner->original.stateChangeCallback = NULL;
  native->enableDeltaFrames = source->enableDeltaFrames;
  native->enableInformationModelMethods = source->enableInformationModelMethods;
}
cpkt_opcua_StatusCode cpkt_opcua_server_set_pubsub_callbacks(
    cpkt_opcua_server *server, const cpkt_opcua_PubSubConfiguration *config) {
  cpkt_pubsub_owner *owner;
  if (!server || !server->server || server->destroying || !config ||
      cpkt_cfg_pubsub_is_view(config) || config->securityPoliciesSize)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  owner = cpkt_pubsub_owner_new(server);
  if (!owner)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  cpkt_pubsub_callbacks_set(owner, config);
  return 0;
}
cpkt_opcua_StatusCode cpkt_opcua_server_set_pubsub_configuration(
    cpkt_opcua_server *server, cpkt_opcua_PubSubConfiguration *config) {
  cpkt_pubsub_owner *owner;
  UA_PubSubConfiguration *native;
  UA_PubSubSecurityPolicy *policies = NULL;
  cpkt_opcua_PubSubSecurityPolicy staged;
  size_t size = 0, i;
  UA_StatusCode status = 0;
  if (!server || !server->server || server->destroying || !config ||
      cpkt_cfg_pubsub_is_view(config) ||
      (config->securityPoliciesSize && !config->securityPolicies))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (server->started)
    return UA_STATUSCODE_BADINVALIDSTATE;
  owner = cpkt_pubsub_owner_new(server);
  if (!owner)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  if (owner->entries)
    return UA_STATUSCODE_BADINVALIDSTATE;
  for (i = 0; i < config->securityPoliciesSize; ++i) {
    staged = config->securityPolicies[i];
    status = cpkt_psp_append(&policies, &size, &staged);
    if (status)
      break;
  }
  if (status) {
    for (i = 0; i < size; ++i) {
      cpkt_psp_custom *holder = (cpkt_psp_custom *)policies[i].policyContext;
      memset(&holder->plugin, 0, sizeof(holder->plugin));
      cpkt_psp_custom_clear(&policies[i]);
    }
    UA_free(policies);
    return status;
  }
  native = &UA_Server_getConfig(server->server)->pubSubConfig;
  for (i = 0; i < native->securityPoliciesSize; ++i)
    if (native->securityPolicies[i].clear)
      native->securityPolicies[i].clear(&native->securityPolicies[i]);
  UA_free(native->securityPolicies);
  native->securityPolicies = policies;
  native->securityPoliciesSize = size;
  for (i = 0; i < size; ++i)
    memset(&config->securityPolicies[i], 0,
           sizeof(config->securityPolicies[i]));
  cpkt_pubsub_callbacks_set(owner, config);
  return 0;
}

cpkt_opcua_StatusCode cpkt_opcua_server_get_pubsub_security_policy(
    cpkt_opcua_server *server, size_t index,
    cpkt_opcua_PubSubSecurityPolicy **out) {
  UA_PubSubConfiguration *config;
  UA_PubSubSecurityPolicy *policy;
  cpkt_psp_custom *holder;
  cpkt_opcua_ServerConfig *configuration;
  UA_StatusCode status;
  if (!server || !server->server || !out)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  *out = NULL;
  config = &UA_Server_getConfig(server->server)->pubSubConfig;
  if (index >= config->securityPoliciesSize)
    return UA_STATUSCODE_BADNOTFOUND;
  policy = &config->securityPolicies[index];
  if (policy->clear != cpkt_psp_custom_clear) {
    status = cpkt_opcua_server_get_config_typed(server, &configuration);
    if (status)
      return status;
    return cpkt_opcua_ServerConfig_getPubSubSecurityPolicy(configuration, index,
                                                           out);
  }
  holder = (cpkt_psp_custom *)policy->policyContext;
  *out = holder->borrowed ? holder->borrowed : &holder->plugin;
  return 0;
}
