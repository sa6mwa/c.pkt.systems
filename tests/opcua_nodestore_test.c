#include "opcua_callbacks_test.h"
#include "opcua_types_peer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void check(int condition, const char *expression, int line) {
  if (!condition) {
    fprintf(stderr, "nodestore line %d: %s\n", line, expression);
    abort();
  }
}
#define CHECK(e) check(!!(e), #e, __LINE__)
typedef struct {
  cpkt_opcua_Nodestore *backend;
  size_t calls, freed;
} backend;
static backend *context(cpkt_opcua_Nodestore *store) {
  backend *value = (backend *)cpkt_opcua_Nodestore_getContext(store);
  CHECK(value != NULL);
  ++value->calls;
  return value;
}
static void backend_free(cpkt_opcua_Nodestore *store) {
  backend *value = context(store);
  CHECK(cpkt_opcua_Nodestore_free(store) != 0);
  CHECK(cpkt_opcua_Nodestore_free(value->backend) == 0);
  value->backend = NULL;
  ++value->freed;
}
static cpkt_opcua_Node *backend_new(cpkt_opcua_Nodestore *store,
                                    cpkt_opcua_NodeClass cls) {
  return cpkt_opcua_Nodestore_newNode(context(store)->backend, cls);
}
static void backend_delete(cpkt_opcua_Nodestore *store, cpkt_opcua_Node *node) {
  cpkt_opcua_Nodestore_deleteNode(context(store)->backend, node);
}
static const cpkt_opcua_Node *
backend_get(cpkt_opcua_Nodestore *store, const cpkt_opcua_NodeId *id,
            cpkt_opcua_UInt32 mask, cpkt_opcua_ReferenceTypeSet refs,
            cpkt_opcua_BrowseDirection direction) {
  return cpkt_opcua_Nodestore_getNode(context(store)->backend, id, mask, refs,
                                      direction);
}
static cpkt_opcua_Node *backend_edit(cpkt_opcua_Nodestore *store,
                                     const cpkt_opcua_NodeId *id,
                                     cpkt_opcua_UInt32 mask,
                                     cpkt_opcua_ReferenceTypeSet refs,
                                     cpkt_opcua_BrowseDirection direction) {
  return cpkt_opcua_Nodestore_getEditNode(context(store)->backend, id, mask,
                                          refs, direction);
}
static const cpkt_opcua_Node *
backend_get_ptr(cpkt_opcua_Nodestore *store, const cpkt_opcua_NodePointer *ptr,
                cpkt_opcua_UInt32 mask, cpkt_opcua_ReferenceTypeSet refs,
                cpkt_opcua_BrowseDirection direction) {
  return cpkt_opcua_Nodestore_getNodeFromPtr(context(store)->backend, ptr, mask,
                                             refs, direction);
}
static cpkt_opcua_Node *backend_edit_ptr(cpkt_opcua_Nodestore *store,
                                         const cpkt_opcua_NodePointer *ptr,
                                         cpkt_opcua_UInt32 mask,
                                         cpkt_opcua_ReferenceTypeSet refs,
                                         cpkt_opcua_BrowseDirection direction) {
  return cpkt_opcua_Nodestore_getEditNodeFromPtr(context(store)->backend, ptr,
                                                 mask, refs, direction);
}
static void backend_release(cpkt_opcua_Nodestore *store,
                            const cpkt_opcua_Node *node) {
  cpkt_opcua_Nodestore_releaseNode(context(store)->backend, node);
}
static cpkt_opcua_StatusCode backend_copy(cpkt_opcua_Nodestore *store,
                                          const cpkt_opcua_NodeId *id,
                                          cpkt_opcua_Node **out) {
  return cpkt_opcua_Nodestore_getNodeCopy(context(store)->backend, id, out);
}
static cpkt_opcua_StatusCode backend_insert(cpkt_opcua_Nodestore *store,
                                            cpkt_opcua_Node *node,
                                            cpkt_opcua_NodeId *id) {
  return cpkt_opcua_Nodestore_insertNode(context(store)->backend, node, id);
}
static cpkt_opcua_StatusCode backend_replace(cpkt_opcua_Nodestore *store,
                                             cpkt_opcua_Node *node) {
  return cpkt_opcua_Nodestore_replaceNode(context(store)->backend, node);
}
static cpkt_opcua_StatusCode backend_remove(cpkt_opcua_Nodestore *store,
                                            const cpkt_opcua_NodeId *id) {
  return cpkt_opcua_Nodestore_removeNode(context(store)->backend, id);
}
static const cpkt_opcua_NodeIdStorage *
backend_reference(cpkt_opcua_Nodestore *store, cpkt_opcua_Byte index) {
  return cpkt_opcua_Nodestore_getReferenceTypeId(context(store)->backend,
                                                 index);
}
static void backend_iterate(cpkt_opcua_Nodestore *store,
                            cpkt_opcua_NodestoreVisitor visitor, void *user) {
  cpkt_opcua_Nodestore_iterate(context(store)->backend, visitor, user);
}
static cpkt_opcua_Nodestore *proxy(backend *value) {
  cpkt_opcua_NodestorePlugin plugin;
  memset(&plugin, 0, sizeof(plugin));
  value->backend = cpkt_opcua_Nodestore_ZipTree();
  value->calls = value->freed = 0;
  CHECK(value->backend != NULL);
  plugin.free = backend_free;
  plugin.newNode = backend_new;
  plugin.deleteNode = backend_delete;
  plugin.getNode = backend_get;
  plugin.getNodeFromPtr = backend_get_ptr;
  plugin.getEditNode = backend_edit;
  plugin.getEditNodeFromPtr = backend_edit_ptr;
  plugin.releaseNode = backend_release;
  plugin.getNodeCopy = backend_copy;
  plugin.insertNode = backend_insert;
  plugin.replaceNode = backend_replace;
  plugin.removeNode = backend_remove;
  plugin.getReferenceTypeId = backend_reference;
  plugin.iterate = backend_iterate;
  return cpkt_opcua_Nodestore_fromPlugin(&plugin, value);
}
static cpkt_opcua_String text(const char *value) {
  cpkt_opcua_String string;
  string.length = strlen(value);
  string.data = (cpkt_opcua_Byte *)value;
  return string;
}
static void pointers(void) {
  cpkt_opcua_NodeIdStorage *id = cpkt_opcua_NodeIdStorage_new();
  cpkt_opcua_NodePointer *borrowed = cpkt_opcua_NodePointer_new();
  cpkt_opcua_NodePointer *copy = cpkt_opcua_NodePointer_new();
  cpkt_opcua_NodeId source, result;
  CHECK(id != NULL && borrowed != NULL && copy != NULL);
  memset(&source, 0, sizeof(source));
  memset(&result, 0, sizeof(result));
  source.namespaceIndex = 9;
  source.identifierType = CPKT_OPCUA_NODEIDTYPE_STRING;
  source.identifier.string = text("borrowed-id");
  CHECK(cpkt_opcua_NodeIdStorage_set(id, &source) == 0);
  cpkt_opcua_NodePointer_fromNodeId(id, borrowed);
  CHECK(cpkt_types_peer_nodepointer(borrowed, id));
  CHECK(cpkt_opcua_NodePointer_isLocal(borrowed));
  CHECK(cpkt_opcua_NodePointer_copy(borrowed, copy) == 0);
  CHECK(cpkt_opcua_NodePointer_equal(borrowed, copy));
  cpkt_opcua_NodePointer_deleteBacking(borrowed);
  cpkt_opcua_NodeIdStorage_delete(id);
  CHECK(cpkt_opcua_NodePointer_toNodeId(copy, &result) == 0 &&
        cpkt_opcua_NodeId_equal(&source, &result));
  cpkt_opcua_NodeId_clear(&result);
  cpkt_opcua_NodePointer_clear(copy);
  cpkt_opcua_NodePointer_deleteBacking(copy);
}
typedef struct {
  cpkt_opcua_Nodestore *store;
  size_t count;
} visit;
static void visited(void *user, const cpkt_opcua_Node *node) {
  visit *value = (visit *)user;
  CHECK(node != NULL && cpkt_opcua_Nodestore_free(value->store) != 0);
  ++value->count;
}
static void *reference_visited(void *user, cpkt_opcua_ReferenceTarget *target) {
  cpkt_opcua_ExpandedNodeId id;
  size_t *count = (size_t *)user;
  memset(&id, 0, sizeof(id));
  CHECK(cpkt_opcua_NodePointer_toExpandedNodeId(
            cpkt_opcua_ReferenceTarget_getId(target), &id) == 0);
  CHECK(id.nodeId.identifier.numeric == 85);
  cpkt_opcua_ExpandedNodeId_clear(&id);
  CHECK(cpkt_opcua_ReferenceTarget_getNameHash(target) == 1234);
  ++*count;
  return user;
}
static void operations(cpkt_opcua_Nodestore *store) {
  cpkt_opcua_Node *node, *copy, *edit;
  const cpkt_opcua_Node *read;
  cpkt_opcua_NodePointer *ptr = cpkt_opcua_NodePointer_new();
  cpkt_opcua_NodeId id = cpkt_opcua_NODEID_NUMERIC(1, 51001), added;
  cpkt_opcua_QualifiedName name = cpkt_opcua_QUALIFIEDNAME(1, "typed-value");
  cpkt_opcua_VariableAttributes attributes, output;
  cpkt_opcua_DataValue value;
  cpkt_opcua_Int64 number;
  cpkt_opcua_NodeInfo info;
  cpkt_opcua_ExpandedNodeId target = cpkt_opcua_EXPANDEDNODEID_NUMERIC(0, 85);
  cpkt_opcua_NodeReferenceKind *kind;
  cpkt_opcua_NodeReferenceKindInfo reference_info;
  cpkt_opcua_ReferenceTypeSet keep;
  visit visit_state;
  size_t count = 0;
  int user;
  CHECK(store != NULL && ptr != NULL);
  memset(&added, 0, sizeof(added));
  memset(&output, 0, sizeof(output));
  memset(&info, 0, sizeof(info));
  memset(&value, 0, sizeof(value));
  CHECK(cpkt_opcua_VariableAttributes_default(&attributes) == 0);
  number.high32 = 0x7fffffffU;
  number.low32 = 0xfffffffdU;
  attributes.value.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
  attributes.value.data = &number;
  attributes.value.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
  node = cpkt_opcua_Nodestore_newNode(store, cpkt_opcua_NODECLASS_VARIABLE);
  CHECK(node != NULL);
  CHECK(cpkt_opcua_Node_setNodeId(node, &id) == 0);
  CHECK(cpkt_opcua_Node_setBrowseName(node, &name) == 0);
  CHECK(cpkt_opcua_Node_setAttributes(
            node, &attributes,
            cpkt_opcua_type_at(CPKT_OPCUA_TYPES_VARIABLEATTRIBUTES)) == 0);
  cpkt_opcua_VariableAttributes_clear(&attributes);
  cpkt_opcua_Node_setContext(node, &user);
  CHECK(cpkt_opcua_Node_setIsDynamic(node, 1) == 0 &&
        cpkt_opcua_Node_getIsDynamic(node));
  CHECK(cpkt_opcua_history_value_get(cpkt_opcua_Node_getInternalValue(node),
                                     &value) == 0);
  CHECK(value.hasValue &&
        ((cpkt_opcua_Int64 *)value.value.data)->high32 == number.high32);
  cpkt_opcua_DataValue_clear(&value);
  CHECK(cpkt_opcua_Node_getStoredAttributes(
            node, &output,
            cpkt_opcua_type_at(CPKT_OPCUA_TYPES_VARIABLEATTRIBUTES)) == 0);
  CHECK(((cpkt_opcua_Int64 *)output.value.data)->low32 == number.low32);
  cpkt_opcua_VariableAttributes_clear(&output);
  CHECK(cpkt_opcua_Node_addReference(node,
                                     CPKT_OPCUA_REFERENCETYPEINDEX_ORGANIZES, 1,
                                     &target, 1234) == 0);
  kind = cpkt_opcua_Node_getEditReference(node, 0);
  CHECK(kind != NULL &&
        cpkt_opcua_NodeReferenceKind_getInfo(kind, &reference_info) == 0 &&
        reference_info.targetsSize == 1);
  CHECK(cpkt_opcua_NodeReferenceKind_findTarget(kind, &target) != NULL);
  CHECK(cpkt_opcua_NodeReferenceKind_iterate(kind, reference_visited, &count) ==
            &count &&
        count == 1);
  CHECK(cpkt_opcua_NodeReferenceKind_switch(kind) == 0);
  CHECK(cpkt_opcua_NodeReferenceKind_findTarget(kind, &target) != NULL);
  keep = cpkt_opcua_REFTYPESET(CPKT_OPCUA_REFERENCETYPEINDEX_ORGANIZES);
  CHECK(cpkt_opcua_ReferenceTypeSet_contains(
      &keep, CPKT_OPCUA_REFERENCETYPEINDEX_ORGANIZES));
  cpkt_opcua_Node_deleteReferencesSubset(node, &keep);
  CHECK(cpkt_opcua_Node_getInfo(node, &info) == 0 && info.referencesSize == 1 &&
        info.context == &user);
  cpkt_opcua_NodeInfo_clear(&info);
  CHECK(cpkt_opcua_Nodestore_insertNode(store, node, &added) == 0 &&
        cpkt_opcua_NodeId_equal(&added, &id));
  cpkt_opcua_NodeId_clear(&added);
  read = cpkt_opcua_Nodestore_getNode(store, &id, 0xffffffffU, keep,
                                      cpkt_opcua_BROWSEDIRECTION_BOTH);
  CHECK(read != NULL);
  cpkt_opcua_NodePointer_fromNodeId(cpkt_opcua_Node_getNodeIdStorage(read),
                                    ptr);
  CHECK(cpkt_opcua_Nodestore_getNodeFromPtr(store, ptr, 0xffffffffU, keep,
                                            cpkt_opcua_BROWSEDIRECTION_BOTH) ==
        read);
  cpkt_opcua_Nodestore_releaseNode(store, read);
  cpkt_opcua_Nodestore_releaseNode(store, read);
  edit = cpkt_opcua_Nodestore_getEditNodeFromPtr(
      store, ptr, 0xffffffffU, keep, cpkt_opcua_BROWSEDIRECTION_BOTH);
  CHECK(edit != NULL);
  cpkt_opcua_Node_setWriteMask(edit, 4321);
  cpkt_opcua_Nodestore_releaseNode(store, edit);
  edit = cpkt_opcua_Nodestore_getEditNode(store, &id, 0xffffffffU, keep,
                                          cpkt_opcua_BROWSEDIRECTION_BOTH);
  CHECK(edit != NULL);
  CHECK(cpkt_opcua_Node_getInfo(edit, &info) == 0 && info.writeMask == 4321);
  cpkt_opcua_NodeInfo_clear(&info);
  cpkt_opcua_Nodestore_releaseNode(store, edit);
  cpkt_opcua_NodePointer_deleteBacking(ptr);
  CHECK(cpkt_opcua_Nodestore_getNodeCopy(store, &id, &copy) == 0 &&
        copy != NULL);
  CHECK(cpkt_opcua_Nodestore_replaceNode(store, copy) == 0);
  visit_state.store = store;
  visit_state.count = 0;
  cpkt_opcua_Nodestore_iterate(store, visited, &visit_state);
  CHECK(visit_state.count == 1);
  CHECK(cpkt_opcua_Nodestore_removeNode(store, &id) == 0);
  CHECK(cpkt_opcua_Nodestore_getNode(store, &id, 0, keep,
                                     cpkt_opcua_BROWSEDIRECTION_BOTH) == NULL);
  node = cpkt_opcua_Nodestore_newNode(store, cpkt_opcua_NODECLASS_OBJECT);
  CHECK(node != NULL);
  cpkt_opcua_Nodestore_deleteNode(store, node);
}
static void configuration(void) {
  backend value;
  cpkt_opcua_Nodestore *store = proxy(&value), *borrowed = NULL;
  cpkt_opcua_ServerConfig *config = cpkt_opcua_ServerConfig_new();
  cpkt_opcua_server *server = NULL;
  cpkt_opcua_NodeId reference;
  CHECK(store != NULL && config != NULL);
  CHECK(cpkt_opcua_ServerConfig_setNodestore(config, store) == 0);
  CHECK(cpkt_opcua_Nodestore_free(store) != 0);
  CHECK(cpkt_opcua_ServerConfig_setLogger(config, NULL) == 0);
  CHECK(cpkt_opcua_ServerConfig_setMinimal(config, 0, NULL) == 0);
  CHECK(cpkt_opcua_server_newWithConfig_typed(config, &server) == 0);
  CHECK(cpkt_opcua_ServerConfig_getNodestore(config, &borrowed) == 0 &&
        borrowed == store);
  CHECK(value.calls > 10 && value.freed == 0);
  memset(&reference, 0, sizeof(reference));
  CHECK(cpkt_opcua_NodeIdStorage_get(
            cpkt_opcua_Nodestore_getReferenceTypeId(
                store, CPKT_OPCUA_REFERENCETYPEINDEX_ORGANIZES),
            &reference) == 0);
  CHECK(reference.identifier.numeric == 35 && reference.namespaceIndex == 0);
  cpkt_opcua_NodeId_clear(&reference);
  cpkt_opcua_server_free(server);
  CHECK(value.freed == 1 && value.backend == NULL);
}
void cpkt_types_test_nodestore(void) {
  cpkt_opcua_Nodestore *store;
  backend value;
  cpkt_opcua_ReferenceTypeSet set;
  cpkt_types_peer_node_copy();
  pointers();
  set = cpkt_opcua_REFTYPESET(127);
  cpkt_opcua_ReferenceTypeSet_add(&set, 32);
  set = cpkt_opcua_ReferenceTypeSet_union(set, cpkt_opcua_REFTYPESET(0));
  CHECK(cpkt_opcua_ReferenceTypeSet_contains(&set, 0) &&
        cpkt_opcua_ReferenceTypeSet_contains(&set, 32) &&
        cpkt_opcua_ReferenceTypeSet_contains(&set, 127));
  store = cpkt_opcua_Nodestore_ZipTree();
  operations(store);
  CHECK(cpkt_opcua_Nodestore_free(store) == 0);
  store = proxy(&value);
  operations(store);
  CHECK(value.calls > 10 && cpkt_opcua_Nodestore_free(store) == 0 &&
        value.freed == 1);
  configuration();
}
