/* Dispatch metadata preserves native node/session contexts. A registry is
 * needed because these native callbacks have no userdata slot. The registry
 * mutex protects server lookup only; user callbacks never run under it. Native
 * operations and entry lifetime are serialized per server. */
#include <pthread.h>
typedef struct cpkt_nodes_owner cpkt_nodes_owner;
typedef struct cpkt_nodes_entry cpkt_nodes_entry;
struct cpkt_producer;
struct cpkt_nodes_owner {
  cpkt_nodes_owner *next;
  cpkt_opcua_server *owner;
  cpkt_nodes_entry *entries;
  cpkt_opcua_GlobalNodeLifecycle global;
  UA_GlobalNodeLifecycle native_global;
  struct cpkt_producer *producers;
  void (*producer_cleanup)(cpkt_nodes_owner *);
  void (*cancel)(cpkt_opcua_server *, const void *);
  void (*native_cancel)(UA_Server *, const void *);
};
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
    if (owner->owner->server == server)
      break;
  (void)pthread_mutex_unlock(&cpkt_nodes_mutex);
  return owner;
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
  if (!owner || !node)
    return NULL;
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
  if (owner->producer_cleanup)
    owner->producer_cleanup(owner);
  UA_free(owner);
  server->typed_nodes = NULL;
  server->typed_producers_refresh = NULL;
}
static cpkt_nodes_owner *cpkt_nodes_owner_new(cpkt_opcua_server *server) {
  cpkt_nodes_owner *owner;
  if (server->typed_nodes)
    return server->typed_nodes;
  owner = (cpkt_nodes_owner *)UA_calloc(1, sizeof(*owner));
  if (!owner)
    return NULL;
  owner->owner = server;
  (void)pthread_mutex_lock(&cpkt_nodes_mutex);
  owner->next = cpkt_nodes_owners;
  cpkt_nodes_owners = owner;
  (void)pthread_mutex_unlock(&cpkt_nodes_mutex);
  server->typed_nodes = owner;
  server->typed_nodes_clear = cpkt_nodes_clear;
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
