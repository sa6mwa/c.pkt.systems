/* Dispatch metadata preserves native node/session contexts. A registry is
 * needed because these native callbacks have no userdata slot. The registry
 * mutex protects server lookup only; user callbacks never run under it. Native
 * operations and entry lifetime are serialized per server. */
#include <pthread.h>
typedef struct cpkt_nodes_owner cpkt_nodes_owner;
typedef struct cpkt_nodes_entry cpkt_nodes_entry;
struct cpkt_producer;
struct cpkt_creation;
static int cpkt_loop_server_active(cpkt_opcua_server *);
static cpkt_nodes_owner *cpkt_cfg_find_constructing_server(UA_Server *);
struct cpkt_nodes_owner {
  cpkt_nodes_owner *next;
  cpkt_opcua_server *owner;
  UA_Server *native;
  UA_EventLoop *event_loop;
  int deleting;
  cpkt_nodes_entry *entries;
  struct cpkt_creation *creations;
  cpkt_opcua_GlobalNodeLifecycle global;
  UA_GlobalNodeLifecycle native_global;
  struct cpkt_producer *producers;
  void (*producer_cleanup)(cpkt_nodes_owner *);
  void (*cancel)(cpkt_opcua_server *, const void *);
  void (*native_cancel)(UA_Server *, const void *);
  cpkt_opcua_ClientConfig *discovery;
  void (*discovery_cleanup)(cpkt_nodes_owner *);
  void *dynamic_types;
  void (*dynamic_types_cleanup)(cpkt_nodes_owner *);
  void *reverse_connections;
  void (*reverse_connections_cleanup)(cpkt_nodes_owner *);
  cpkt_opcua_Server_registerServerCallback registered_server;
  void *registered_server_context;
  void *local_monitors;
  void (*local_monitors_cleanup)(cpkt_nodes_owner *);
  void *pubsub;
  void (*pubsub_cleanup)(cpkt_nodes_owner *);
};
/* The native out-ID is populated before construction callbacks. Borrow that
 * address during the actual native call, including automatically assigned IDs.
 * Reentrant replacement supersedes this scoped record without losing the
 * already-running callback's reference. */
typedef struct cpkt_creation {
  struct cpkt_creation *previous;
  cpkt_nodes_owner *owner;
  cpkt_nodes_entry *entry;
  const UA_NodeId *id;
  int superseded;
} cpkt_creation;
struct cpkt_nodes_entry {
  cpkt_nodes_entry *next;
  cpkt_nodes_owner *owner;
  UA_NodeId node;
  cpkt_opcua_ValueSourceNotifications notifications;
  cpkt_opcua_NodeTypeLifecycle lifecycle;
  cpkt_opcua_CallbackValueSource source;
  cpkt_opcua_MethodCallback method;
  unsigned int references;
};
static pthread_mutex_t cpkt_nodes_mutex = PTHREAD_MUTEX_INITIALIZER;
static cpkt_nodes_owner *cpkt_nodes_owners;
static cpkt_nodes_owner *cpkt_nodes_find_owner(UA_Server *server) {
  cpkt_nodes_owner *owner;
  (void)pthread_mutex_lock(&cpkt_nodes_mutex);
  for (owner = cpkt_nodes_owners; owner; owner = owner->next)
    if (owner->native == server)
      break;
  (void)pthread_mutex_unlock(&cpkt_nodes_mutex);
  return owner ? owner : cpkt_cfg_find_constructing_server(server);
}
static void cpkt_nodes_release(cpkt_nodes_entry *entry) {
  if (!--entry->references) {
    UA_NodeId_clear(&entry->node);
    UA_free(entry);
  }
}
static cpkt_nodes_entry *cpkt_nodes_acquire(UA_Server *server,
                                            const UA_NodeId *node) {
  cpkt_nodes_owner *owner = cpkt_nodes_find_owner(server);
  cpkt_nodes_entry *entry;
  cpkt_creation *creation;
  if (!owner || !node)
    return NULL;
  for (creation = owner->creations; creation; creation = creation->previous)
    if (!creation->superseded && !UA_NodeId_isNull(creation->id) &&
        UA_NodeId_equal(creation->id, node)) {
      ++creation->entry->references;
      return creation->entry;
    }
  for (entry = owner->entries; entry; entry = entry->next)
    if (UA_NodeId_equal(&entry->node, node)) {
      ++entry->references;
      return entry;
    }
  return NULL;
}
static void cpkt_nodes_clear(cpkt_opcua_server *server) {
  cpkt_nodes_owner *owner = server->typed_nodes, **slot;
  cpkt_nodes_entry *entry, *next;
  if (!owner)
    return;
  (void)pthread_mutex_lock(&cpkt_nodes_mutex);
  for (slot = &cpkt_nodes_owners; *slot; slot = &(*slot)->next)
    if (*slot == owner) {
      *slot = owner->next;
      break;
    }
  (void)pthread_mutex_unlock(&cpkt_nodes_mutex);
  for (entry = owner->entries; entry; entry = next) {
    next = entry->next;
    cpkt_nodes_release(entry);
  }
  if (owner->discovery_cleanup)
    owner->discovery_cleanup(owner);
  if (owner->producer_cleanup)
    owner->producer_cleanup(owner);
  if (owner->reverse_connections_cleanup)
    owner->reverse_connections_cleanup(owner);
  if (owner->local_monitors_cleanup)
    owner->local_monitors_cleanup(owner);
  if (owner->pubsub_cleanup)
    owner->pubsub_cleanup(owner);
  if (owner->dynamic_types_cleanup)
    owner->dynamic_types_cleanup(owner);
  UA_free(owner);
  server->typed_nodes = NULL;
  server->typed_nodes_set_deleting = NULL;
  server->typed_nodes_refresh_eventloop = NULL;
  server->typed_eventloop_active = NULL;
  server->typed_producers_refresh = NULL;
}
static void cpkt_nodes_set_deleting(cpkt_opcua_server *server, int deleting) {
  (void)pthread_mutex_lock(&cpkt_nodes_mutex);
  server->typed_nodes->deleting = deleting;
  server->destroying = deleting;
  (void)pthread_mutex_unlock(&cpkt_nodes_mutex);
}
static void cpkt_nodes_refresh_eventloop(cpkt_opcua_server *server) {
  UA_EventLoop *loop =
      server->server ? UA_Server_getConfig(server->server)->eventLoop : NULL;
  (void)pthread_mutex_lock(&cpkt_nodes_mutex);
  server->typed_nodes->event_loop = loop;
  (void)pthread_mutex_unlock(&cpkt_nodes_mutex);
}
static void cpkt_nodes_owner_publish(cpkt_opcua_server *server,
                                     cpkt_nodes_owner *owner) {
  owner->owner = server;
  owner->native = server->server;
  owner->event_loop =
      server->server ? UA_Server_getConfig(server->server)->eventLoop : NULL;
  (void)pthread_mutex_lock(&cpkt_nodes_mutex);
  owner->next = cpkt_nodes_owners;
  cpkt_nodes_owners = owner;
  (void)pthread_mutex_unlock(&cpkt_nodes_mutex);
  server->typed_nodes = owner;
  server->typed_nodes_clear = cpkt_nodes_clear;
  server->typed_nodes_set_deleting = cpkt_nodes_set_deleting;
  server->typed_nodes_refresh_eventloop = cpkt_nodes_refresh_eventloop;
  server->typed_eventloop_active = cpkt_loop_server_active;
}
static cpkt_nodes_owner *cpkt_nodes_owner_new(cpkt_opcua_server *server) {
  cpkt_nodes_owner *owner;
  if (server->typed_nodes)
    return server->typed_nodes;
  owner = (cpkt_nodes_owner *)UA_calloc(1, sizeof(*owner));
  if (!owner)
    return NULL;
  cpkt_nodes_owner_publish(server, owner);
  return owner;
}
static UA_StatusCode cpkt_nodes_range(const UA_NumericRange *native,
                                      cpkt_opcua_NumericRange *range) {
  size_t i;
  if (!native->dimensionsSize)
    return 0;
  if (!native->dimensions || native->dimensions == UA_EMPTY_ARRAY_SENTINEL ||
      native->dimensionsSize > (size_t)-1 / sizeof(*range->dimensions))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  range->dimensions = (cpkt_opcua_NumericRangeDimension *)UA_calloc(
      native->dimensionsSize, sizeof(*range->dimensions));
  if (!range->dimensions)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  range->dimensionsSize = native->dimensionsSize;
  for (i = 0; i < range->dimensionsSize; ++i) {
    range->dimensions[i].min = native->dimensions[i].min;
    range->dimensions[i].max = native->dimensions[i].max;
  }
  return 0;
}
