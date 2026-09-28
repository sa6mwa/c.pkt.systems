/* Public native node helpers. Opaque objects preserve native identity and all
 * backend/runtime fields; schema reads/writes use the shared C89 converter. */
#ifndef CPKT_OPCUA_NODESTORE_NODES_IMPL_H
#define CPKT_OPCUA_NODESTORE_NODES_IMPL_H
cpkt_opcua_NodeIdStorage *cpkt_opcua_NodeIdStorage_new(void) {
  return (cpkt_opcua_NodeIdStorage *)(void *)UA_NodeId_new();
}
void cpkt_opcua_NodeIdStorage_delete(cpkt_opcua_NodeIdStorage *storage) {
  UA_NodeId_delete((UA_NodeId *)(void *)storage);
}
cpkt_opcua_StatusCode
cpkt_opcua_NodeIdStorage_get(const cpkt_opcua_NodeIdStorage *storage,
                             cpkt_opcua_NodeId *value) {
  if (!storage || !value)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  return cpkt_convert((const UA_NodeId *)(const void *)storage, value,
                      &cpkt_types[CPKT_OPCUA_TYPES_NODEID], 0, 0);
}
cpkt_opcua_StatusCode
cpkt_opcua_NodeIdStorage_set(cpkt_opcua_NodeIdStorage *storage,
                             const cpkt_opcua_NodeId *value) {
  UA_NodeId staged;
  UA_StatusCode status;
  if (!storage || !value)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  UA_NodeId_init(&staged);
  status =
      cpkt_convert(value, &staged, &cpkt_types[CPKT_OPCUA_TYPES_NODEID], 1, 0);
  if (!status) {
    UA_NodeId_clear((UA_NodeId *)(void *)storage);
    *(UA_NodeId *)(void *)storage = staged;
  } else
    UA_NodeId_clear(&staged);
  return status;
}
cpkt_opcua_ExpandedNodeIdStorage *cpkt_opcua_ExpandedNodeIdStorage_new(void) {
  return (cpkt_opcua_ExpandedNodeIdStorage *)(void *)UA_ExpandedNodeId_new();
}
void cpkt_opcua_ExpandedNodeIdStorage_delete(
    cpkt_opcua_ExpandedNodeIdStorage *storage) {
  UA_ExpandedNodeId_delete((UA_ExpandedNodeId *)(void *)storage);
}
cpkt_opcua_StatusCode cpkt_opcua_ExpandedNodeIdStorage_get(
    const cpkt_opcua_ExpandedNodeIdStorage *storage,
    cpkt_opcua_ExpandedNodeId *value) {
  if (!storage || !value)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  return cpkt_convert((const UA_ExpandedNodeId *)(const void *)storage, value,
                      &cpkt_types[CPKT_OPCUA_TYPES_EXPANDEDNODEID], 0, 0);
}
cpkt_opcua_StatusCode
cpkt_opcua_ExpandedNodeIdStorage_set(cpkt_opcua_ExpandedNodeIdStorage *storage,
                                     const cpkt_opcua_ExpandedNodeId *value) {
  UA_ExpandedNodeId staged;
  UA_StatusCode status;
  if (!storage || !value)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  UA_ExpandedNodeId_init(&staged);
  status = cpkt_convert(value, &staged,
                        &cpkt_types[CPKT_OPCUA_TYPES_EXPANDEDNODEID], 1, 0);
  if (!status) {
    UA_ExpandedNodeId_clear((UA_ExpandedNodeId *)(void *)storage);
    *(UA_ExpandedNodeId *)(void *)storage = staged;
  } else
    UA_ExpandedNodeId_clear(&staged);
  return status;
}
cpkt_opcua_NodePointer *cpkt_opcua_NodePointer_new(void) {
  return (cpkt_opcua_NodePointer *)(void *)UA_calloc(1, sizeof(UA_NodePointer));
}
void cpkt_opcua_NodePointer_deleteBacking(cpkt_opcua_NodePointer *pointer) {
  UA_free(pointer);
}
void cpkt_opcua_NodePointer_init(cpkt_opcua_NodePointer *pointer) {
  if (pointer)
    UA_NodePointer_init((UA_NodePointer *)(void *)pointer);
}
void cpkt_opcua_NodePointer_clear(cpkt_opcua_NodePointer *pointer) {
  if (pointer)
    UA_NodePointer_clear((UA_NodePointer *)(void *)pointer);
}
cpkt_opcua_StatusCode
cpkt_opcua_NodePointer_copy(const cpkt_opcua_NodePointer *source,
                            cpkt_opcua_NodePointer *destination) {
  if (!source || !destination)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  return UA_NodePointer_copy(*(const UA_NodePointer *)(const void *)source,
                             (UA_NodePointer *)(void *)destination);
}
cpkt_opcua_Boolean
cpkt_opcua_NodePointer_isLocal(const cpkt_opcua_NodePointer *pointer) {
  return pointer ? UA_NodePointer_isLocal(
                       *(const UA_NodePointer *)(const void *)pointer)
                 : 0;
}
cpkt_opcua_Order cpkt_opcua_NodePointer_order(const cpkt_opcua_NodePointer *a,
                                              const cpkt_opcua_NodePointer *b) {
  if (!a || !b)
    return a == b ? CPKT_OPCUA_ORDER_EQ
           : a    ? CPKT_OPCUA_ORDER_MORE
                  : CPKT_OPCUA_ORDER_LESS;
  return (cpkt_opcua_Order)UA_NodePointer_order(
      *(const UA_NodePointer *)(const void *)a,
      *(const UA_NodePointer *)(const void *)b);
}
cpkt_opcua_Boolean
cpkt_opcua_NodePointer_equal(const cpkt_opcua_NodePointer *a,
                             const cpkt_opcua_NodePointer *b) {
  if (!a || !b)
    return a == b;
  return UA_NodePointer_equal(*(const UA_NodePointer *)(const void *)a,
                              *(const UA_NodePointer *)(const void *)b);
}
void cpkt_opcua_NodePointer_fromNodeId(const cpkt_opcua_NodeIdStorage *id,
                                       cpkt_opcua_NodePointer *pointer) {
  if (id && pointer)
    *(UA_NodePointer *)(void *)pointer =
        UA_NodePointer_fromNodeId((const UA_NodeId *)(const void *)id);
}
void cpkt_opcua_NodePointer_fromExpandedNodeId(
    const cpkt_opcua_ExpandedNodeIdStorage *id,
    cpkt_opcua_NodePointer *pointer) {
  if (id && pointer)
    *(UA_NodePointer *)(void *)pointer = UA_NodePointer_fromExpandedNodeId(
        (const UA_ExpandedNodeId *)(const void *)id);
}
cpkt_opcua_StatusCode
cpkt_opcua_NodePointer_toNodeId(const cpkt_opcua_NodePointer *pointer,
                                cpkt_opcua_NodeId *id) {
  UA_NodeId native;
  if (!pointer || !id)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  native =
      UA_NodePointer_toNodeId(*(const UA_NodePointer *)(const void *)pointer);
  return cpkt_convert(&native, id, &cpkt_types[CPKT_OPCUA_TYPES_NODEID], 0, 0);
}
cpkt_opcua_StatusCode
cpkt_opcua_NodePointer_toExpandedNodeId(const cpkt_opcua_NodePointer *pointer,
                                        cpkt_opcua_ExpandedNodeId *id) {
  UA_ExpandedNodeId native;
  if (!pointer || !id)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  native = UA_NodePointer_toExpandedNodeId(
      *(const UA_NodePointer *)(const void *)pointer);
  return cpkt_convert(&native, id, &cpkt_types[CPKT_OPCUA_TYPES_EXPANDEDNODEID],
                      0, 0);
}
void cpkt_opcua_NodeInfo_clear(cpkt_opcua_NodeInfo *info) {
  if (!info)
    return;
  cpkt_opcua_NodeId_clear(&info->nodeId);
  cpkt_opcua_QualifiedName_clear(&info->browseName);
  memset(info, 0, sizeof(*info));
}
cpkt_opcua_StatusCode cpkt_opcua_Node_getInfo(const cpkt_opcua_Node *node,
                                              cpkt_opcua_NodeInfo *info) {
  const UA_Node *native = (const UA_Node *)(const void *)node;
  cpkt_opcua_NodeInfo staged;
  UA_StatusCode status;
  if (!node || !info)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memset(&staged, 0, sizeof(staged));
  status = cpkt_convert(&native->head.nodeId, &staged.nodeId,
                        &cpkt_types[CPKT_OPCUA_TYPES_NODEID], 0, 0);
  if (!status)
    status = cpkt_convert(&native->head.browseName, &staged.browseName,
                          &cpkt_types[CPKT_OPCUA_TYPES_QUALIFIEDNAME], 0, 0);
  if (status) {
    cpkt_opcua_NodeInfo_clear(&staged);
    return status;
  }
  staged.nodeClass = (cpkt_opcua_NodeClass)native->head.nodeClass;
  staged.writeMask = native->head.writeMask;
  staged.referencesSize = native->head.referencesSize;
  staged.context = native->head.context;
  staged.constructed = native->head.constructed;
  *info = staged;
  return 0;
}
cpkt_opcua_StatusCode cpkt_opcua_Node_setNodeId(cpkt_opcua_Node *node,
                                                const cpkt_opcua_NodeId *id) {
  if (!node)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  return cpkt_opcua_NodeIdStorage_set(
      (cpkt_opcua_NodeIdStorage *)(void *)&((UA_Node *)(void *)node)
          ->head.nodeId,
      id);
}
cpkt_opcua_StatusCode
cpkt_opcua_Node_setBrowseName(cpkt_opcua_Node *node,
                              const cpkt_opcua_QualifiedName *name) {
  UA_Node *native = (UA_Node *)(void *)node;
  UA_QualifiedName staged;
  UA_StatusCode status;
  if (!node || !name)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  UA_QualifiedName_init(&staged);
  status = cpkt_convert(name, &staged,
                        &cpkt_types[CPKT_OPCUA_TYPES_QUALIFIEDNAME], 1, 0);
  if (!status) {
    UA_QualifiedName_clear(&native->head.browseName);
    native->head.browseName = staged;
  } else
    UA_QualifiedName_clear(&staged);
  return status;
}
void cpkt_opcua_Node_setContext(cpkt_opcua_Node *node, void *context) {
  if (node)
    ((UA_Node *)(void *)node)->head.context = context;
}
void cpkt_opcua_Node_setConstructed(cpkt_opcua_Node *node,
                                    cpkt_opcua_Boolean value) {
  if (node)
    ((UA_Node *)(void *)node)->head.constructed = value;
}
void cpkt_opcua_Node_setWriteMask(cpkt_opcua_Node *node,
                                  cpkt_opcua_UInt32 value) {
  if (node)
    ((UA_Node *)(void *)node)->head.writeMask = value;
}
const cpkt_opcua_NodeIdStorage *
cpkt_opcua_Node_getNodeIdStorage(const cpkt_opcua_Node *node) {
  return node ? (const cpkt_opcua_NodeIdStorage *)(const void *)&(
                    (const UA_Node *)(const void *)node)
                    ->head.nodeId
              : NULL;
}
const cpkt_opcua_NodeReferenceKind *
cpkt_opcua_Node_getReference(const cpkt_opcua_Node *node, size_t index) {
  const UA_Node *native = (const UA_Node *)(const void *)node;
  return node && index < native->head.referencesSize
             ? (const cpkt_opcua_NodeReferenceKind *)(const void *)&native->head
                   .references[index]
             : NULL;
}
cpkt_opcua_NodeReferenceKind *
cpkt_opcua_Node_getEditReference(cpkt_opcua_Node *node, size_t index) {
  UA_Node *native = (UA_Node *)(void *)node;
  return node && index < native->head.referencesSize
             ? (cpkt_opcua_NodeReferenceKind *)(void *)&native->head
                   .references[index]
             : NULL;
}
const cpkt_opcua_LocalizedTextListEntry *
cpkt_opcua_Node_getDisplayName(const cpkt_opcua_Node *node) {
  return node ? (const cpkt_opcua_LocalizedTextListEntry
                     *)(const void *)((const UA_Node *)(const void *)node)
                    ->head.displayName
              : NULL;
}
const cpkt_opcua_LocalizedTextListEntry *
cpkt_opcua_Node_getDescription(const cpkt_opcua_Node *node) {
  return node ? (const cpkt_opcua_LocalizedTextListEntry
                     *)(const void *)((const UA_Node *)(const void *)node)
                    ->head.description
              : NULL;
}
const cpkt_opcua_LocalizedTextListEntry *cpkt_opcua_LocalizedTextListEntry_next(
    const cpkt_opcua_LocalizedTextListEntry *entry) {
  return entry ? (const cpkt_opcua_LocalizedTextListEntry
                      *)(const void *)((const UA_LocalizedTextListEntry
                                            *)(const void *)entry)
                     ->next
               : NULL;
}
cpkt_opcua_StatusCode cpkt_opcua_LocalizedTextListEntry_get(
    const cpkt_opcua_LocalizedTextListEntry *entry,
    cpkt_opcua_LocalizedText *value) {
  if (!entry || !value)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  return cpkt_convert(
      &((const UA_LocalizedTextListEntry *)(const void *)entry)->localizedText,
      value, &cpkt_types[CPKT_OPCUA_TYPES_LOCALIZEDTEXT], 0, 0);
}
cpkt_opcua_StatusCode
cpkt_opcua_Node_setAttributes(cpkt_opcua_Node *node, const void *attributes,
                              const cpkt_opcua_Type *type) {
  void *native;
  const UA_DataType *canonical;
  UA_StatusCode status;
  type = cpkt_valid_type(type);
  if (!node || !attributes || !type)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  canonical = UA_findDataType(&type->native->typeId);
  if (!canonical || !cpkt_native_type_equal(canonical, type->native, 0))
    return UA_STATUSCODE_BADNODEATTRIBUTESINVALID;
  native = UA_new(type->native);
  if (!native)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  status = cpkt_convert(attributes, native, type, 1, 0);
  if (!status)
    status = UA_Node_setAttributes((UA_Node *)(void *)node, native, canonical);
  UA_delete(native, type->native);
  return status;
}
#define CPKT_NODE_LOCALIZED(name)                                              \
  cpkt_opcua_StatusCode cpkt_opcua_Node_##name(                                \
      cpkt_opcua_Node *node, const cpkt_opcua_LocalizedText *value) {          \
    UA_LocalizedText native;                                                   \
    UA_StatusCode status;                                                      \
    if (!node || !value) {                                                     \
      return UA_STATUSCODE_BADINVALIDARGUMENT;                                 \
    }                                                                          \
    UA_LocalizedText_init(&native);                                            \
    status = cpkt_convert(value, &native,                                      \
                          &cpkt_types[CPKT_OPCUA_TYPES_LOCALIZEDTEXT], 1, 0);  \
    if (!status) {                                                             \
      status = UA_Node_##name((UA_Node *)(void *)node, &native);               \
    }                                                                          \
    UA_LocalizedText_clear(&native);                                           \
    return status;                                                             \
  }
CPKT_NODE_LOCALIZED(insertOrUpdateDisplayName)
CPKT_NODE_LOCALIZED(insertOrUpdateDescription)
#undef CPKT_NODE_LOCALIZED
cpkt_opcua_StatusCode cpkt_opcua_Node_copy(const cpkt_opcua_Node *source,
                                           cpkt_opcua_Node *destination) {
  if (!source || !destination)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  return UA_Node_copy((const UA_Node *)(const void *)source,
                      (UA_Node *)(void *)destination);
}
void cpkt_opcua_Node_moveMonitoredItems(cpkt_opcua_Node *source,
                                        cpkt_opcua_Node *destination) {
  if (source && destination)
    UA_Node_moveMonitoredItems((UA_Node *)(void *)source,
                               (UA_Node *)(void *)destination);
}
cpkt_opcua_Node *cpkt_opcua_Node_copy_alloc(const cpkt_opcua_Node *source) {
  return source ? (cpkt_opcua_Node *)(void *)UA_Node_copy_alloc(
                      (const UA_Node *)(const void *)source)
                : NULL;
}
void cpkt_opcua_Node_delete_alloc(cpkt_opcua_Node *node) {
  if (node) {
    UA_Node_clear((UA_Node *)(void *)node);
    UA_free(node);
  }
}
cpkt_opcua_StatusCode cpkt_opcua_Node_addReference(
    cpkt_opcua_Node *node, cpkt_opcua_Byte type, cpkt_opcua_Boolean forward,
    const cpkt_opcua_ExpandedNodeId *target, cpkt_opcua_UInt32 hash) {
  UA_ExpandedNodeId native;
  UA_StatusCode status;
  if (!node || !target)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  UA_ExpandedNodeId_init(&native);
  status = cpkt_convert(target, &native,
                        &cpkt_types[CPKT_OPCUA_TYPES_EXPANDEDNODEID], 1, 0);
  if (!status)
    status = UA_Node_addReference((UA_Node *)(void *)node, type, forward,
                                  &native, hash);
  UA_ExpandedNodeId_clear(&native);
  return status;
}
cpkt_opcua_StatusCode
cpkt_opcua_Node_deleteReference(cpkt_opcua_Node *node, cpkt_opcua_Byte type,
                                cpkt_opcua_Boolean forward,
                                const cpkt_opcua_ExpandedNodeId *target) {
  UA_ExpandedNodeId native;
  UA_StatusCode status;
  if (!node || !target)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  UA_ExpandedNodeId_init(&native);
  status = cpkt_convert(target, &native,
                        &cpkt_types[CPKT_OPCUA_TYPES_EXPANDEDNODEID], 1, 0);
  if (!status)
    status = UA_Node_deleteReference((UA_Node *)(void *)node, type, forward,
                                     &native);
  UA_ExpandedNodeId_clear(&native);
  return status;
}
void cpkt_opcua_Node_deleteReferencesSubset(
    cpkt_opcua_Node *node, const cpkt_opcua_ReferenceTypeSet *keep) {
  UA_ReferenceTypeSet native;
  if (!node || !keep)
    return;
  native = cpkt_store_references_write(*keep);
  UA_Node_deleteReferencesSubset((UA_Node *)(void *)node, &native);
}
void cpkt_opcua_Node_deleteReferences(cpkt_opcua_Node *node) {
  if (node)
    UA_Node_deleteReferences((UA_Node *)(void *)node);
}
void cpkt_opcua_Node_clear(cpkt_opcua_Node *node) {
  if (node)
    UA_Node_clear((UA_Node *)(void *)node);
}
cpkt_opcua_StatusCode
cpkt_opcua_NodeReferenceKind_getInfo(const cpkt_opcua_NodeReferenceKind *kind,
                                     cpkt_opcua_NodeReferenceKindInfo *info) {
  const UA_NodeReferenceKind *native =
      (const UA_NodeReferenceKind *)(const void *)kind;
  if (!kind || !info)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  info->targetsSize = native->targetsSize;
  info->hasRefTree = native->hasRefTree;
  info->referenceTypeIndex = native->referenceTypeIndex;
  info->isInverse = native->isInverse;
  return 0;
}
typedef struct {
  cpkt_opcua_NodeReferenceKind_iterateCallback callback;
  void *context;
} cpkt_node_reference_visit;
static void *cpkt_node_reference_invoke(void *context,
                                        UA_ReferenceTarget *target) {
  cpkt_node_reference_visit *visit = (cpkt_node_reference_visit *)context;
  return visit->callback(visit->context,
                         (cpkt_opcua_ReferenceTarget *)(void *)target);
}
void *cpkt_opcua_NodeReferenceKind_iterate(
    cpkt_opcua_NodeReferenceKind *kind,
    cpkt_opcua_NodeReferenceKind_iterateCallback callback, void *context) {
  cpkt_node_reference_visit visit;
  if (!kind || !callback)
    return NULL;
  visit.callback = callback;
  visit.context = context;
  return UA_NodeReferenceKind_iterate((UA_NodeReferenceKind *)(void *)kind,
                                      cpkt_node_reference_invoke, &visit);
}
const cpkt_opcua_ReferenceTarget *cpkt_opcua_NodeReferenceKind_findTarget(
    const cpkt_opcua_NodeReferenceKind *kind,
    const cpkt_opcua_ExpandedNodeId *target) {
  UA_ExpandedNodeId native;
  const UA_ReferenceTarget *found;
  if (!kind || !target || !cpkt_stock_node_valid(&target->nodeId) ||
      !cpkt_string_valid(&target->namespaceUri))
    return NULL;
  UA_ExpandedNodeId_init(&native);
  native.nodeId = cpkt_nodeid_view(&target->nodeId);
  native.namespaceUri = cpkt_string_view(&target->namespaceUri);
  native.serverIndex = target->serverIndex;
  found = UA_NodeReferenceKind_findTarget(
      (const UA_NodeReferenceKind *)(const void *)kind, &native);
  return (const cpkt_opcua_ReferenceTarget *)(const void *)found;
}
cpkt_opcua_StatusCode
cpkt_opcua_NodeReferenceKind_switch(cpkt_opcua_NodeReferenceKind *kind) {
  return kind
             ? UA_NodeReferenceKind_switch((UA_NodeReferenceKind *)(void *)kind)
             : UA_STATUSCODE_BADINVALIDARGUMENT;
}
const cpkt_opcua_NodePointer *
cpkt_opcua_ReferenceTarget_getId(const cpkt_opcua_ReferenceTarget *target) {
  return target ? (const cpkt_opcua_NodePointer *)(const void *)&(
                      (const UA_ReferenceTarget *)(const void *)target)
                      ->targetId
                : NULL;
}
cpkt_opcua_UInt32 cpkt_opcua_ReferenceTarget_getNameHash(
    const cpkt_opcua_ReferenceTarget *target) {
  return target ? ((const UA_ReferenceTarget *)(const void *)target)
                      ->targetNameHash
                : 0;
}
void cpkt_opcua_ReferenceTarget_setNameHash(cpkt_opcua_ReferenceTarget *target,
                                            cpkt_opcua_UInt32 hash) {
  if (target)
    ((UA_ReferenceTarget *)(void *)target)->targetNameHash = hash;
}
cpkt_opcua_ValueSourceType
cpkt_opcua_Node_getValueSourceType(const cpkt_opcua_Node *node) {
  const UA_Node *native = (const UA_Node *)(const void *)node;
  if (!node)
    return CPKT_OPCUA_VALUESOURCETYPE_CALLBACK;
  if (native->head.nodeClass == UA_NODECLASS_VARIABLE)
    return (cpkt_opcua_ValueSourceType)native->variableNode.valueSourceType;
  if (native->head.nodeClass == UA_NODECLASS_VARIABLETYPE)
    return (cpkt_opcua_ValueSourceType)native->variableTypeNode.valueSourceType;
  return CPKT_OPCUA_VALUESOURCETYPE_CALLBACK;
}
const cpkt_opcua_history_value *
cpkt_opcua_Node_getInternalValue(const cpkt_opcua_Node *node) {
  const UA_Node *native = (const UA_Node *)(const void *)node;
  if (!node || cpkt_opcua_Node_getValueSourceType(node) !=
                   CPKT_OPCUA_VALUESOURCETYPE_INTERNAL)
    return NULL;
  if (native->head.nodeClass == UA_NODECLASS_VARIABLE)
    return (const cpkt_opcua_history_value *)(const void *)&native->variableNode
        .valueSource.internal.value;
  if (native->head.nodeClass == UA_NODECLASS_VARIABLETYPE)
    return (const cpkt_opcua_history_value *)(const void *)&native
        ->variableTypeNode.valueSource.internal.value;
  return NULL;
}
cpkt_opcua_history_value *
cpkt_opcua_Node_getEditInternalValue(cpkt_opcua_Node *node) {
  UA_Node *native = (UA_Node *)(void *)node;
  if (!node || cpkt_opcua_Node_getValueSourceType(node) !=
                   CPKT_OPCUA_VALUESOURCETYPE_INTERNAL)
    return NULL;
  if (native->head.nodeClass == UA_NODECLASS_VARIABLE)
    return (cpkt_opcua_history_value *)(void *)&native->variableNode.valueSource
        .internal.value;
  if (native->head.nodeClass == UA_NODECLASS_VARIABLETYPE)
    return (cpkt_opcua_history_value *)(void *)&native->variableTypeNode
        .valueSource.internal.value;
  return NULL;
}
cpkt_opcua_Boolean cpkt_opcua_Node_getIsDynamic(const cpkt_opcua_Node *node) {
  const UA_Node *native = (const UA_Node *)(const void *)node;
  return node && native->head.nodeClass == UA_NODECLASS_VARIABLE
             ? native->variableNode.isDynamic
             : 0;
}
cpkt_opcua_StatusCode cpkt_opcua_Node_setIsDynamic(cpkt_opcua_Node *node,
                                                   cpkt_opcua_Boolean value) {
  UA_Node *native = (UA_Node *)(void *)node;
  if (!node)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (native->head.nodeClass != UA_NODECLASS_VARIABLE)
    return UA_STATUSCODE_BADNODECLASSINVALID;
  native->variableNode.isDynamic = value;
  return 0;
}
cpkt_opcua_StatusCode
cpkt_opcua_Node_getReferenceTypeState(const cpkt_opcua_Node *node,
                                      cpkt_opcua_Byte *index,
                                      cpkt_opcua_ReferenceTypeSet *subtypes) {
  const UA_Node *native = (const UA_Node *)(const void *)node;
  if (!node)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (native->head.nodeClass != UA_NODECLASS_REFERENCETYPE)
    return UA_STATUSCODE_BADNODECLASSINVALID;
  if (index)
    *index = native->referenceTypeNode.referenceTypeIndex;
  if (subtypes)
    *subtypes = cpkt_store_references_read(native->referenceTypeNode.subTypes);
  return 0;
}
cpkt_opcua_StatusCode
cpkt_opcua_Node_setReferenceTypeState(cpkt_opcua_Node *node,
                                      cpkt_opcua_Byte index,
                                      cpkt_opcua_ReferenceTypeSet subtypes) {
  UA_Node *native = (UA_Node *)(void *)node;
  if (!node || index >= UA_REFERENCETYPESET_MAX)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (native->head.nodeClass != UA_NODECLASS_REFERENCETYPE)
    return UA_STATUSCODE_BADNODECLASSINVALID;
  native->referenceTypeNode.referenceTypeIndex = index;
  native->referenceTypeNode.subTypes = cpkt_store_references_write(subtypes);
  return 0;
}
#endif
