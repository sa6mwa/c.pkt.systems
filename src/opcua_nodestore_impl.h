/* Opaque handles denote actual native node/identifier/reference storage.
 * Store wrappers retain identity and callback lifetimes, never node layouts. */
#ifndef CPKT_OPCUA_NODESTORE_IMPL_H
#define CPKT_OPCUA_NODESTORE_IMPL_H
#include "opcua_native_nodestore.h"
#include <open62541/plugin/nodestore_default.h>

struct cpkt_opcua_Nodestore {
  cpkt_opcua_Nodestore *next;
  UA_Nodestore *native;
  void (*native_free)(UA_Nodestore *);
  cpkt_opcua_NodestorePlugin plugin;
  void *context;
  void *scope;
  void (*enter)(void *), (*leave)(void *);
  UA_Nodestore **configuration_slot;
  unsigned int depth;
};
static pthread_mutex_t cpkt_store_mutex = PTHREAD_MUTEX_INITIALIZER;
static cpkt_opcua_Nodestore *cpkt_stores;
static cpkt_opcua_Nodestore *cpkt_store_find(UA_Nodestore *native) {
  cpkt_opcua_Nodestore *store;
  (void)pthread_mutex_lock(&cpkt_store_mutex);
  for (store = cpkt_stores; store; store = store->next)
    if (store->native == native)
      break;
  (void)pthread_mutex_unlock(&cpkt_store_mutex);
  return store;
}
static void cpkt_store_enter(cpkt_opcua_Nodestore *store) {
  ++store->depth;
  if (store->enter)
    store->enter(store->scope);
}
static void cpkt_store_leave(cpkt_opcua_Nodestore *store) {
  if (store->leave)
    store->leave(store->scope);
  --store->depth;
}
static void cpkt_store_custom_free(UA_Nodestore *native) {
  cpkt_opcua_Nodestore *store = cpkt_store_find(native);
  store->plugin.free(store);
  UA_free(native);
}
static void cpkt_store_native_free(UA_Nodestore *native) {
  cpkt_opcua_Nodestore *store = cpkt_store_find(native), **slot;
  if (!store)
    return;
  /* All facade entries protect the active method frame. Native callers must
   * obey the same quiescent destruction requirement as the upstream backend. */
  cpkt_store_enter(store);
  store->native_free(native);
  cpkt_store_leave(store);
  if (store->configuration_slot && *store->configuration_slot == native)
    *store->configuration_slot = NULL;
  (void)pthread_mutex_lock(&cpkt_store_mutex);
  for (slot = &cpkt_stores; *slot; slot = &(*slot)->next)
    if (*slot == store) {
      *slot = store->next;
      break;
    }
  (void)pthread_mutex_unlock(&cpkt_store_mutex);
  UA_free(store);
}
static cpkt_opcua_Nodestore *cpkt_store_adopt(UA_Nodestore *native) {
  cpkt_opcua_Nodestore *store;
  if (!native || !native->free)
    return NULL;
  store = cpkt_store_find(native);
  if (store)
    return store;
  store = (cpkt_opcua_Nodestore *)UA_calloc(1, sizeof(*store));
  if (!store)
    return NULL;
  store->native = native;
  store->native_free = native->free;
  native->free = cpkt_store_native_free;
  (void)pthread_mutex_lock(&cpkt_store_mutex);
  store->next = cpkt_stores;
  cpkt_stores = store;
  (void)pthread_mutex_unlock(&cpkt_store_mutex);
  return store;
}
static void cpkt_store_bind(cpkt_opcua_Nodestore *store, UA_Nodestore **slot,
                            void *scope, void (*enter)(void *),
                            void (*leave)(void *)) {
  if (!store)
    return;
  store->configuration_slot = slot;
  store->scope = scope;
  store->enter = enter;
  store->leave = leave;
}

const cpkt_opcua_ReferenceTypeSet cpkt_opcua_REFERENCETYPESET_NONE = {
    {0, 0, 0, 0}};
const cpkt_opcua_ReferenceTypeSet cpkt_opcua_REFERENCETYPESET_ALL = {
    {0xffffffffU, 0xffffffffU, 0xffffffffU, 0xffffffffU}};
typedef char cpkt_store_reference_width[(CPKT_OPCUA_REFERENCETYPESET_MAX ==
                                             UA_REFERENCETYPESET_MAX &&
                                         CPKT_OPCUA_REFERENCETYPESET_MAX == 128)
                                            ? 1
                                            : -1];
static UA_ReferenceTypeSet
cpkt_store_references_write(cpkt_opcua_ReferenceTypeSet value) {
  UA_ReferenceTypeSet native;
  size_t i;
  for (i = 0; i < UA_REFERENCETYPESET_MAX / 32; ++i)
    native.bits[i] = value.bits[i];
  return native;
}
static cpkt_opcua_ReferenceTypeSet
cpkt_store_references_read(UA_ReferenceTypeSet native) {
  cpkt_opcua_ReferenceTypeSet value;
  size_t i;
  for (i = 0; i < UA_REFERENCETYPESET_MAX / 32; ++i)
    value.bits[i] = native.bits[i];
  return value;
}
void cpkt_opcua_ReferenceTypeSet_init(cpkt_opcua_ReferenceTypeSet *set) {
  if (set)
    memset(set, 0, sizeof(*set));
}
cpkt_opcua_ReferenceTypeSet cpkt_opcua_REFTYPESET(cpkt_opcua_Byte index) {
  return cpkt_store_references_read(index < UA_REFERENCETYPESET_MAX
                                        ? UA_REFTYPESET(index)
                                        : UA_REFERENCETYPESET_NONE);
}
void cpkt_opcua_ReferenceTypeSet_add(cpkt_opcua_ReferenceTypeSet *set,
                                     cpkt_opcua_Byte index) {
  UA_ReferenceTypeSet native;
  if (!set || index >= UA_REFERENCETYPESET_MAX)
    return;
  native = cpkt_store_references_write(*set);
  UA_ReferenceTypeSet_add(&native, index);
  *set = cpkt_store_references_read(native);
}
cpkt_opcua_ReferenceTypeSet
cpkt_opcua_ReferenceTypeSet_union(cpkt_opcua_ReferenceTypeSet a,
                                  cpkt_opcua_ReferenceTypeSet b) {
  return cpkt_store_references_read(UA_ReferenceTypeSet_union(
      cpkt_store_references_write(a), cpkt_store_references_write(b)));
}
cpkt_opcua_Boolean
cpkt_opcua_ReferenceTypeSet_contains(const cpkt_opcua_ReferenceTypeSet *set,
                                     cpkt_opcua_Byte index) {
  UA_ReferenceTypeSet native;
  if (!set || index >= UA_REFERENCETYPESET_MAX)
    return 0;
  native = cpkt_store_references_write(*set);
  return UA_ReferenceTypeSet_contains(&native, index);
}
typedef struct {
  UA_NodestoreVisitor callback;
  void *context;
} cpkt_store_visit_native;
typedef struct {
  cpkt_opcua_NodestoreVisitor callback;
  void *context;
} cpkt_store_visit_public;
static void cpkt_store_visit_to_native(void *context,
                                       const cpkt_opcua_Node *node) {
  cpkt_store_visit_native *visit = (cpkt_store_visit_native *)context;
  visit->callback(visit->context, (const UA_Node *)(const void *)node);
}
static void cpkt_store_visit_from_native(void *context, const UA_Node *node) {
  cpkt_store_visit_public *visit = (cpkt_store_visit_public *)context;
  visit->callback(visit->context, (const cpkt_opcua_Node *)(const void *)node);
}

#include "opcua_nodestore_metadata.inc"

cpkt_opcua_Nodestore *cpkt_opcua_Nodestore_ZipTree(void) {
  UA_Nodestore *native = UA_Nodestore_ZipTree();
  cpkt_opcua_Nodestore *store = cpkt_store_adopt(native);
  if (native && !store)
    native->free(native);
  return store;
}
cpkt_opcua_Nodestore *
cpkt_opcua_Nodestore_fromPlugin(const cpkt_opcua_NodestorePlugin *plugin,
                                void *context) {
  UA_Nodestore *native;
  cpkt_opcua_Nodestore *store;
  if (!plugin || !plugin->free || !plugin->newNode || !plugin->deleteNode ||
      !plugin->getNode || !plugin->getNodeFromPtr || !plugin->getEditNode ||
      !plugin->getEditNodeFromPtr || !plugin->releaseNode ||
      !plugin->getNodeCopy || !plugin->insertNode || !plugin->replaceNode ||
      !plugin->removeNode || !plugin->getReferenceTypeId || !plugin->iterate)
    return NULL;
  native = (UA_Nodestore *)UA_calloc(1, sizeof(*native));
  if (!native)
    return NULL;
  cpkt_store_assign(native);
  store = cpkt_store_adopt(native);
  if (!store) {
    UA_free(native);
    return NULL;
  }
  store->plugin = *plugin;
  store->context = context;
  return store;
}
void *cpkt_opcua_Nodestore_getContext(const cpkt_opcua_Nodestore *store) {
  return store ? store->context : NULL;
}
cpkt_opcua_StatusCode cpkt_opcua_Nodestore_free(cpkt_opcua_Nodestore *store) {
  if (!store)
    return 0;
  if (store->depth || store->configuration_slot)
    return UA_STATUSCODE_BADINVALIDSTATE;
  store->native->free(store->native);
  return 0;
}
cpkt_opcua_Node *cpkt_opcua_Nodestore_newNode(cpkt_opcua_Nodestore *store,
                                              cpkt_opcua_NodeClass node_class) {
  UA_Node *node;
  if (!store || !store->native->newNode)
    return NULL;
  cpkt_store_enter(store);
  node = store->native->newNode(store->native, (UA_NodeClass)node_class);
  cpkt_store_leave(store);
  return (cpkt_opcua_Node *)(void *)node;
}
void cpkt_opcua_Nodestore_deleteNode(cpkt_opcua_Nodestore *store,
                                     cpkt_opcua_Node *node) {
  if (!store || !node || !store->native->deleteNode)
    return;
  cpkt_store_enter(store);
  store->native->deleteNode(store->native, (UA_Node *)(void *)node);
  cpkt_store_leave(store);
}
/* get/getEdit return the actual backend pointer with its native release lease.
 */
#define CPKT_STORE_GET(name, CONST)                                            \
  CONST cpkt_opcua_Node *cpkt_opcua_Nodestore_##name(                          \
      cpkt_opcua_Nodestore *store, const cpkt_opcua_NodeId *id,                \
      cpkt_opcua_UInt32 mask, cpkt_opcua_ReferenceTypeSet refs,                \
      cpkt_opcua_BrowseDirection direction) {                                  \
    UA_NodeId native_id;                                                       \
    CONST UA_Node *node;                                                       \
    if (!store || !cpkt_stock_node_valid(id) || !id || !store->native->name)   \
      return NULL;                                                             \
    native_id = cpkt_nodeid_view(id);                                          \
    cpkt_store_enter(store);                                                   \
    node = store->native->name(store->native, &native_id, mask,                \
                               cpkt_store_references_write(refs),              \
                               (UA_BrowseDirection)direction);                 \
    cpkt_store_leave(store);                                                   \
    return (CONST cpkt_opcua_Node *)(CONST void *)node;                        \
  }
CPKT_STORE_GET(getNode, const)
#define CPKT_STORE_MUTABLE
CPKT_STORE_GET(getEditNode, CPKT_STORE_MUTABLE)
#undef CPKT_STORE_GET
#define CPKT_STORE_GET_PTR(name, CONST)                                        \
  CONST cpkt_opcua_Node *cpkt_opcua_Nodestore_##name(                          \
      cpkt_opcua_Nodestore *store, const cpkt_opcua_NodePointer *ptr,          \
      cpkt_opcua_UInt32 mask, cpkt_opcua_ReferenceTypeSet refs,                \
      cpkt_opcua_BrowseDirection direction) {                                  \
    CONST UA_Node *node;                                                       \
    if (!store || !ptr || !store->native->name) {                              \
      return NULL;                                                             \
    }                                                                          \
    cpkt_store_enter(store);                                                   \
    node = store->native->name(                                                \
        store->native, *(const UA_NodePointer *)(const void *)ptr, mask,       \
        cpkt_store_references_write(refs), (UA_BrowseDirection)direction);     \
    cpkt_store_leave(store);                                                   \
    return (CONST cpkt_opcua_Node *)(CONST void *)node;                        \
  }
CPKT_STORE_GET_PTR(getNodeFromPtr, const)
CPKT_STORE_GET_PTR(getEditNodeFromPtr, CPKT_STORE_MUTABLE)
#undef CPKT_STORE_GET_PTR
#undef CPKT_STORE_MUTABLE
void cpkt_opcua_Nodestore_releaseNode(cpkt_opcua_Nodestore *store,
                                      const cpkt_opcua_Node *node) {
  if (!store || !node || !store->native->releaseNode)
    return;
  cpkt_store_enter(store);
  store->native->releaseNode(store->native,
                             (const UA_Node *)(const void *)node);
  cpkt_store_leave(store);
}
cpkt_opcua_StatusCode
cpkt_opcua_Nodestore_getNodeCopy(cpkt_opcua_Nodestore *store,
                                 const cpkt_opcua_NodeId *id,
                                 cpkt_opcua_Node **out) {
  UA_NodeId native_id;
  UA_Node *node = NULL;
  UA_StatusCode status;
  if (!out)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  *out = NULL;
  if (!store || !id || !cpkt_stock_node_valid(id))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!store->native->getNodeCopy)
    return UA_STATUSCODE_BADNOTSUPPORTED;
  native_id = cpkt_nodeid_view(id);
  cpkt_store_enter(store);
  status = store->native->getNodeCopy(store->native, &native_id, &node);
  cpkt_store_leave(store);
  *out = (cpkt_opcua_Node *)(void *)node;
  return status;
}
cpkt_opcua_StatusCode
cpkt_opcua_Nodestore_insertNode(cpkt_opcua_Nodestore *store,
                                cpkt_opcua_Node *node,
                                cpkt_opcua_NodeId *added) {
  UA_NodeId native_id;
  cpkt_opcua_NodeId value;
  UA_StatusCode status, result;
  if (!store || !node)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!store->native->insertNode)
    return UA_STATUSCODE_BADNOTSUPPORTED;
  UA_NodeId_init(&native_id);
  memset(&value, 0, sizeof(value));
  cpkt_store_enter(store);
  result = store->native->insertNode(store->native, (UA_Node *)(void *)node,
                                     added ? &native_id : NULL);
  cpkt_store_leave(store);
  status = added ? cpkt_convert(&native_id, &value,
                                &cpkt_types[CPKT_OPCUA_TYPES_NODEID], 0, 0)
                 : 0;
  if (!status && added) {
    *added = value;
    memset(&value, 0, sizeof(value));
  }
  cpkt_opcua_NodeId_clear(&value);
  UA_NodeId_clear(&native_id);
  return status ? status : result;
}
cpkt_opcua_StatusCode
cpkt_opcua_Nodestore_replaceNode(cpkt_opcua_Nodestore *store,
                                 cpkt_opcua_Node *node) {
  UA_StatusCode status;
  if (!store || !node)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!store->native->replaceNode)
    return UA_STATUSCODE_BADNOTSUPPORTED;
  cpkt_store_enter(store);
  status = store->native->replaceNode(store->native, (UA_Node *)(void *)node);
  cpkt_store_leave(store);
  return status;
}
cpkt_opcua_StatusCode
cpkt_opcua_Nodestore_removeNode(cpkt_opcua_Nodestore *store,
                                const cpkt_opcua_NodeId *id) {
  UA_NodeId native_id;
  UA_StatusCode status;
  if (!store || !id || !cpkt_stock_node_valid(id))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!store->native->removeNode)
    return UA_STATUSCODE_BADNOTSUPPORTED;
  native_id = cpkt_nodeid_view(id);
  cpkt_store_enter(store);
  status = store->native->removeNode(store->native, &native_id);
  cpkt_store_leave(store);
  return status;
}
const cpkt_opcua_NodeIdStorage *
cpkt_opcua_Nodestore_getReferenceTypeId(cpkt_opcua_Nodestore *store,
                                        cpkt_opcua_Byte index) {
  const UA_NodeId *id;
  if (!store || !store->native->getReferenceTypeId)
    return NULL;
  cpkt_store_enter(store);
  id = store->native->getReferenceTypeId(store->native, index);
  cpkt_store_leave(store);
  return (const cpkt_opcua_NodeIdStorage *)(const void *)id;
}
void cpkt_opcua_Nodestore_iterate(cpkt_opcua_Nodestore *store,
                                  cpkt_opcua_NodestoreVisitor visitor,
                                  void *context) {
  cpkt_store_visit_public visit;
  if (!store || !visitor || !store->native->iterate)
    return;
  visit.callback = visitor;
  visit.context = context;
  cpkt_store_enter(store);
  store->native->iterate(store->native, cpkt_store_visit_from_native, &visit);
  cpkt_store_leave(store);
}
#include "opcua_nodestore_nodes_impl.h"
#endif
