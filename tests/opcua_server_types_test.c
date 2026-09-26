#include "opcua_callbacks_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void check(int ok, const char *expression, int line) {
  if (!ok) {
    fprintf(stderr, "server type line %d: %s\n", line, expression);
    abort();
  }
}
#define CHECK(e) check(!!(e), #e, __LINE__)
static cpkt_opcua_NodeId number(unsigned int ns, unsigned int id) {
  cpkt_opcua_NodeId node;
  cpkt_opcua_NodeId_init(&node);
  node.namespaceIndex = (cpkt_opcua_UInt16)ns;
  node.identifier.numeric = id;
  return node;
}
static cpkt_opcua_String text(const char *s) {
  cpkt_opcua_String t;
  t.length = strlen(s);
  t.data = (cpkt_opcua_Byte *)s;
  return t;
}
static cpkt_opcua_QualifiedName name(unsigned int ns, const char *s) {
  cpkt_opcua_QualifiedName n;
  n.namespaceIndex = (cpkt_opcua_UInt16)ns;
  n.name = text(s);
  return n;
}
static cpkt_opcua_LocalizedText localized(const char *s) {
  cpkt_opcua_LocalizedText t;
  cpkt_opcua_LocalizedText_init(&t);
  t.text = text(s);
  return t;
}
#define READ_SCALAR(operation, type, node, expected)                           \
  do {                                                                         \
    cpkt_opcua_##type result;                                                  \
    memset(&result, 0, sizeof(result));                                        \
    CHECK(cpkt_opcua_server_read##operation##_typed(server, node, &result) ==  \
          0);                                                                  \
    CHECK(result == (expected));                                               \
  } while (0)
#define WRITE_READ(operation, type, node, value)                               \
  do {                                                                         \
    CHECK(cpkt_opcua_server_write##operation##_typed(server, node, value) ==   \
          0);                                                                  \
    READ_SCALAR(operation, type, node, value);                                 \
  } while (0)
static cpkt_opcua_result method(const cpkt_opcua_value *inputs, size_t count,
                                cpkt_opcua_value *output, void *user) {
  (void)inputs;
  CHECK(count == 0 && user == NULL);
  cpkt_opcua_value_integer(output, 42);
  return CPKT_OPCUA_OK;
}
static void test_server_timers_and_iterator(void);
void cpkt_types_test_server(void) {
  cpkt_opcua_server *server;
  cpkt_opcua_NodeId object, variable, variable_type, object_type,
      reference_type, data_type, view, result, missing;
  cpkt_opcua_ObjectAttributes oa;
  cpkt_opcua_VariableAttributes va;
  cpkt_opcua_VariableTypeAttributes vta;
  cpkt_opcua_ObjectTypeAttributes ota;
  cpkt_opcua_ReferenceTypeAttributes rta;
  cpkt_opcua_DataTypeAttributes dta;
  cpkt_opcua_ViewAttributes viewa;
  cpkt_opcua_MethodAttributes ma;
  cpkt_opcua_Int64 numbers[2];
  cpkt_opcua_Variant value, dimensions;
  cpkt_opcua_DataValue dv;
  cpkt_opcua_LocalizedText label;
  cpkt_opcua_QualifiedName browse_name;
  cpkt_opcua_BrowseDescription description;
  cpkt_opcua_BrowseResult browsed, next;
  cpkt_opcua_BrowsePath path;
  cpkt_opcua_RelativePathElement element;
  cpkt_opcua_BrowsePathResult translated;
  cpkt_opcua_CallMethodRequest call;
  cpkt_opcua_CallMethodResult called;
  cpkt_opcua_ExpandedNodeId target;
  cpkt_opcua_String uri;
  size_t namespace_index;
  unsigned short ns;
  cpkt_opcua_UInt32 dim = 2;
  void *context;
  cpkt_opcua_status upstream_status;
  cpkt_opcua_NodeId admin;
  CHECK(cpkt_opcua_server_new(&server, 0) == CPKT_OPCUA_OK);
  CHECK(cpkt_opcua_server_add_namespace(server, "urn:typed-server", &ns) ==
        CPKT_OPCUA_OK);
  object = number(ns, 7000);
  variable = number(ns, 7001);
  variable_type = number(ns, 7002);
  object_type = number(ns, 7003);
  reference_type = number(ns, 7004);
  data_type = number(ns, 7005);
  view = number(ns, 7006);
  missing = number(ns, 7999);
  cpkt_opcua_NodeId_init(&result);
  CHECK(cpkt_opcua_ObjectAttributes_default(&oa) == 0);
  CHECK(cpkt_opcua_VariableAttributes_default(&va) == 0);
  CHECK(cpkt_opcua_VariableTypeAttributes_default(&vta) == 0);
  CHECK(cpkt_opcua_ObjectTypeAttributes_default(&ota) == 0);
  CHECK(cpkt_opcua_ReferenceTypeAttributes_default(&rta) == 0);
  CHECK(cpkt_opcua_DataTypeAttributes_default(&dta) == 0);
  CHECK(cpkt_opcua_ViewAttributes_default(&viewa) == 0);
  CHECK(cpkt_opcua_MethodAttributes_default(&ma) == 0);
  CHECK(ma.executable && ma.userExecutable);
  cpkt_opcua_MethodAttributes_clear(&ma);
  CHECK(cpkt_opcua_server_addObjectNode_typed(
            server, object, number(0, CPKT_OPCUA_NS0ID_OBJECTSFOLDER),
            number(0, CPKT_OPCUA_NS0ID_ORGANIZES), name(ns, "object"),
            number(0, CPKT_OPCUA_NS0ID_BASEOBJECTTYPE), oa, &ns, &result) == 0);
  CHECK(cpkt_opcua_NodeId_equal(&result, &object));
  cpkt_opcua_NodeId_clear(&result);
  cpkt_opcua_ObjectAttributes_clear(&oa);
  numbers[0].high32 = 0x80000000U;
  numbers[0].low32 = 0;
  numbers[1].high32 = 0x7fffffffU;
  numbers[1].low32 = 0xffffffffU;
  va.dataType = number(0, CPKT_OPCUA_NS0ID_INT64);
  va.valueRank = 1;
  va.arrayDimensionsSize = 1;
  va.arrayDimensions = &dim;
  va.value.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
  va.value.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
  va.value.data = numbers;
  va.value.arrayLength = 2;
  CHECK(cpkt_opcua_server_addVariableNode_typed(
            server, variable, object, number(0, CPKT_OPCUA_NS0ID_HASCOMPONENT),
            name(ns, "variable"),
            number(0, CPKT_OPCUA_NS0ID_BASEDATAVARIABLETYPE), va, NULL,
            &result) == 0);
  CHECK(cpkt_opcua_NodeId_equal(&result, &variable));
  cpkt_opcua_NodeId_clear(&result);
  va.arrayDimensions = NULL;
  va.arrayDimensionsSize = 0;
  cpkt_opcua_VariableAttributes_clear(&va);
  CHECK(cpkt_opcua_server_addVariableTypeNode_typed(
            server, variable_type, number(0, CPKT_OPCUA_NS0ID_BASEVARIABLETYPE),
            number(0, CPKT_OPCUA_NS0ID_HASSUBTYPE), name(ns, "variableType"),
            number(0, 0), vta, NULL, NULL) == 0);
  CHECK(cpkt_opcua_server_addObjectTypeNode_typed(
            server, object_type, number(0, CPKT_OPCUA_NS0ID_BASEOBJECTTYPE),
            number(0, CPKT_OPCUA_NS0ID_HASSUBTYPE), name(ns, "objectType"), ota,
            NULL, NULL) == 0);
  CHECK(cpkt_opcua_server_addReferenceTypeNode_typed(
            server, reference_type,
            number(0, CPKT_OPCUA_NS0ID_NONHIERARCHICALREFERENCES),
            number(0, CPKT_OPCUA_NS0ID_HASSUBTYPE), name(ns, "referenceType"),
            rta, NULL, NULL) == 0);
  CHECK(cpkt_opcua_server_addDataTypeNode_typed(
            server, data_type, number(0, CPKT_OPCUA_NS0ID_BASEDATATYPE),
            number(0, CPKT_OPCUA_NS0ID_HASSUBTYPE), name(ns, "dataType"), dta,
            NULL, NULL) == 0);
  CHECK(cpkt_opcua_server_addViewNode_typed(
            server, view, number(0, CPKT_OPCUA_NS0ID_VIEWSFOLDER),
            number(0, CPKT_OPCUA_NS0ID_ORGANIZES), name(ns, "view"), viewa,
            NULL, NULL) == 0);
  cpkt_opcua_VariableTypeAttributes_clear(&vta);
  cpkt_opcua_ObjectTypeAttributes_clear(&ota);
  cpkt_opcua_ReferenceTypeAttributes_clear(&rta);
  cpkt_opcua_DataTypeAttributes_clear(&dta);
  cpkt_opcua_ViewAttributes_clear(&viewa);
  CHECK(cpkt_opcua_server_add_method(
            server, cpkt_opcua_node_id_numeric(ns, 7007),
            cpkt_opcua_node_id_numeric(ns, 7000), "method", "method", NULL, 0,
            CPKT_OPCUA_VALUE_INTEGER, method, NULL,
            &upstream_status) == CPKT_OPCUA_OK &&
        upstream_status == 0);
  WRITE_READ(Executable, Boolean, number(ns, 7007), 1);
  READ_SCALAR(NodeClass, NodeClass, variable, cpkt_opcua_NODECLASS_VARIABLE);
  READ_SCALAR(Symmetric, Boolean, reference_type, 0);
  READ_SCALAR(ContainsNoLoops, Boolean, view, 0);
  WRITE_READ(WriteMask, UInt32, variable, 7);
  WRITE_READ(IsAbstract, Boolean, object_type, 1);
  WRITE_READ(EventNotifier, Byte, object, 1);
  WRITE_READ(ValueRank, Int32, variable, 1);
  WRITE_READ(AccessLevel, Byte, variable, 3);
  WRITE_READ(AccessLevelEx, UInt32, variable, 3);
  WRITE_READ(MinimumSamplingInterval, Double, variable, 12.5);
  WRITE_READ(Historizing, Boolean, variable, 1);
  CHECK(cpkt_opcua_server_readNodeId_typed(server, variable, &result) == 0 &&
        cpkt_opcua_NodeId_equal(&result, &variable));
  cpkt_opcua_NodeId_clear(&result);
  CHECK(cpkt_opcua_server_writeBrowseName_typed(server, variable,
                                                name(ns, "renamed")) ==
        CPKT_OPCUA_STATUSCODE_BADWRITENOTSUPPORTED);
  CHECK(cpkt_opcua_server_readBrowseName_typed(server, variable,
                                               &browse_name) == 0 &&
        browse_name.name.length == 8 &&
        memcmp(browse_name.name.data, "variable", 8) == 0);
  cpkt_opcua_QualifiedName_clear(&browse_name);
  CHECK(cpkt_opcua_server_writeDisplayName_typed(server, variable,
                                                 localized("display")) == 0);
  CHECK(cpkt_opcua_server_readDisplayName_typed(server, variable, &label) ==
            0 &&
        label.text.length == 7);
  cpkt_opcua_LocalizedText_clear(&label);
  CHECK(cpkt_opcua_server_writeDescription_typed(
            server, variable, localized("description")) == 0);
  CHECK(cpkt_opcua_server_readDescription_typed(server, variable, &label) ==
            0 &&
        label.text.length == 11);
  cpkt_opcua_LocalizedText_clear(&label);
  CHECK(cpkt_opcua_server_writeInverseName_typed(server, reference_type,
                                                 localized("inverse")) == 0);
  CHECK(cpkt_opcua_server_readInverseName_typed(server, reference_type,
                                                &label) == 0 &&
        label.text.length == 7);
  cpkt_opcua_LocalizedText_clear(&label);
  CHECK(cpkt_opcua_server_writeDataType_typed(
            server, variable, number(0, CPKT_OPCUA_NS0ID_INT64)) == 0);
  CHECK(cpkt_opcua_server_readDataType_typed(server, variable, &result) == 0 &&
        result.identifier.numeric == CPKT_OPCUA_NS0ID_INT64);
  cpkt_opcua_NodeId_clear(&result);
  cpkt_opcua_Variant_init(&value);
  CHECK(cpkt_opcua_server_readValue_typed(server, variable, &value) == 0);
  CHECK(value.arrayLength == 2 &&
        ((cpkt_opcua_Int64 *)value.data)[0].high32 == 0x80000000U);
  ((cpkt_opcua_Int64 *)value.data)[0].low32 = 123;
  CHECK(cpkt_opcua_server_writeValue_typed(server, variable, value) == 0);
  cpkt_opcua_DataValue_init(&dv);
  dv.hasValue = 1;
  dv.value = value;
  dv.hasSourceTimestamp = 1;
  dv.sourceTimestamp = numbers[1];
  CHECK(cpkt_opcua_server_writeDataValue_typed(server, variable, dv) == 0);
  dv.value.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
  cpkt_opcua_DataValue_clear(&dv);
  cpkt_opcua_Variant_clear(&value);
  cpkt_opcua_Variant_init(&dimensions);
  dimensions.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_UINT32);
  dimensions.arrayLength = 1;
  dimensions.data = &dim;
  dimensions.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
  CHECK(cpkt_opcua_server_writeArrayDimensions_typed(server, variable,
                                                     dimensions) == 0);
  cpkt_opcua_Variant_init(&dimensions);
  CHECK(cpkt_opcua_server_readArrayDimensions_typed(server, variable,
                                                    &dimensions) == 0 &&
        dimensions.arrayLength == 1 &&
        *(cpkt_opcua_UInt32 *)dimensions.data == 2);
  cpkt_opcua_Variant_clear(&dimensions);
  CHECK(cpkt_opcua_server_setVariableNodeDynamic_typed(server, variable, 0) ==
        0);
  CHECK(cpkt_opcua_server_getNodeContext_typed(server, object, &context) == 0 &&
        context == &ns);
  CHECK(cpkt_opcua_server_setNodeContext_typed(server, object,
                                               &namespace_index) == 0);
  CHECK(cpkt_opcua_server_getNodeContext_typed(server, object, &context) == 0 &&
        context == &namespace_index);
  CHECK(cpkt_opcua_server_getNamespaceByName_typed(
            server, text("urn:typed-server"), &namespace_index) == 0 &&
        namespace_index == ns);
  cpkt_opcua_String_init(&uri);
  CHECK(cpkt_opcua_server_getNamespaceByIndex_typed(server, ns, &uri) == 0 &&
        uri.length == 16);
  cpkt_opcua_String_clear(&uri);
  cpkt_opcua_BrowseDescription_init(&description);
  description.nodeId = object;
  description.browseDirection = cpkt_opcua_BROWSEDIRECTION_FORWARD;
  description.resultMask = 63;
  cpkt_opcua_BrowseResult_init(&browsed);
  cpkt_opcua_BrowseResult_init(&next);
  CHECK(cpkt_opcua_server_browse_typed(server, 1, &description, &browsed) ==
            0 &&
        browsed.statusCode == 0 && browsed.referencesSize == 1);
  CHECK(cpkt_opcua_server_browseNext_typed(
            server, 1, &browsed.continuationPoint, &next) == 0);
  cpkt_opcua_BrowseResult_clear(&browsed);
  cpkt_opcua_BrowseResult_clear(&next);
  cpkt_opcua_BrowsePath_init(&path);
  cpkt_opcua_RelativePathElement_init(&element);
  cpkt_opcua_BrowsePathResult_init(&translated);
  path.startingNode = object;
  path.relativePath.elementsSize = 1;
  path.relativePath.elements = &element;
  element.referenceTypeId = number(0, CPKT_OPCUA_NS0ID_HASCOMPONENT);
  element.targetName = name(ns, "variable");
  CHECK(cpkt_opcua_server_translateBrowsePathToNodeIds_typed(
            server, &path, &translated) == 0 &&
        translated.statusCode == 0 && translated.targetsSize == 1 &&
        cpkt_opcua_NodeId_equal(&translated.targets[0].targetId.nodeId,
                                &variable));
  cpkt_opcua_BrowsePathResult_clear(&translated);
  cpkt_opcua_CallMethodRequest_init(&call);
  cpkt_opcua_CallMethodResult_init(&called);
  call.methodId = missing;
  call.objectId = object;
  CHECK(cpkt_opcua_server_call_typed(server, &call, &called) == 0 &&
        called.statusCode != 0);
  cpkt_opcua_CallMethodResult_clear(&called);
  call.methodId = number(ns, 7007);
  CHECK(cpkt_opcua_server_call_typed(server, &call, &called) == 0 &&
        called.statusCode == 0 && called.outputArgumentsSize == 1);
  CHECK(called.outputArguments[0].type ==
            cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT32) &&
        *(cpkt_opcua_Int32 *)called.outputArguments[0].data == 42);
  cpkt_opcua_CallMethodResult_clear(&called);
  CHECK(cpkt_opcua_VariableAttributes_default(&va) == 0);
  va.dataType = number(0, CPKT_OPCUA_NS0ID_INT64);
  va.valueRank = -1;
  va.value.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
  va.value.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
  va.value.data = &numbers[0];
  CHECK(cpkt_opcua_server_addVariableNode_typed(
            server, number(ns, 7008), object,
            number(0, CPKT_OPCUA_NS0ID_HASPROPERTY), name(ns, "property"),
            number(0, CPKT_OPCUA_NS0ID_PROPERTYTYPE), va, NULL, NULL) == 0);
  cpkt_opcua_VariableAttributes_clear(&va);
  cpkt_opcua_Variant_init(&value);
  CHECK(cpkt_opcua_server_readObjectProperty_typed(
            server, object, name(ns, "property"), &value) == 0 &&
        ((cpkt_opcua_Int64 *)value.data)->high32 == 0x80000000U);
  *(cpkt_opcua_Int64 *)value.data = numbers[1];
  CHECK(cpkt_opcua_server_writeObjectProperty_typed(
            server, object, name(ns, "property"), value) == 0);
  cpkt_opcua_Variant_clear(&value);
  CHECK(cpkt_opcua_server_readObjectProperty_typed(
            server, object, name(ns, "property"), &value) == 0 &&
        ((cpkt_opcua_Int64 *)value.data)->high32 == 0x7fffffffU);
  cpkt_opcua_Variant_clear(&value);
  cpkt_opcua_ExpandedNodeId_init(&target);
  target.nodeId = view;
  CHECK(cpkt_opcua_server_addReference_typed(
            server, object, number(0, CPKT_OPCUA_NS0ID_HASCOMPONENT), target,
            1) == 0);
  CHECK(cpkt_opcua_server_deleteReference_typed(
            server, object, number(0, CPKT_OPCUA_NS0ID_HASCOMPONENT), 1, target,
            1) == 0);
  CHECK(cpkt_opcua_server_readValue_typed(server, missing, &value) ==
            CPKT_OPCUA_STATUSCODE_BADNODEIDUNKNOWN &&
        value.data == NULL);
  CHECK(cpkt_opcua_server_readValue_typed(NULL, variable, &value) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_server_readValue_typed(server, variable, NULL) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  cpkt_opcua_NodeId_init(&admin);
  admin.identifierType = CPKT_OPCUA_NODEIDTYPE_GUID;
  admin.identifier.guid.data1 = 1;
  cpkt_opcua_Variant_init(&value);
  value.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
  value.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
  value.data = &numbers[0];
  CHECK(cpkt_opcua_server_setSessionAttribute_typed(
            server, &admin, name(ns, "custom"), &value) == 0);
  cpkt_opcua_Variant_init(&value);
  CHECK(cpkt_opcua_server_getSessionAttributeCopy_typed(
            server, &admin, name(ns, "custom"), &value) == 0);
  CHECK(value.type == cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64) &&
        ((cpkt_opcua_Int64 *)value.data)->high32 == 0x80000000U);
  cpkt_opcua_Variant_clear(&value);
  CHECK(cpkt_opcua_server_deleteSessionAttribute_typed(
            server, &admin, name(ns, "custom")) == 0);
  CHECK(cpkt_opcua_server_getSessionAttributeCopy_typed(
            server, &admin, name(ns, "custom"), &value) != 0 &&
        value.data == NULL);
  CHECK(cpkt_opcua_server_closeSession_typed(server, &missing) != 0);
  CHECK(cpkt_opcua_server_getSessionAttributeCopy_typed(
            server, &missing, name(0, "missing"), &value) != 0 &&
        value.data == NULL);
  CHECK(cpkt_opcua_server_setSessionAttribute_typed(
            server, &missing, name(0, "missing"), &value) != 0);
  CHECK(cpkt_opcua_server_deleteSessionAttribute_typed(
            server, &missing, name(0, "missing")) != 0);
  CHECK(cpkt_opcua_server_deleteNode_typed(server, variable, 1) == 0);
  cpkt_opcua_server_free(server);
  test_server_timers_and_iterator();
}

struct timer_state {
  cpkt_opcua_server *server;
  cpkt_opcua_UInt64 id, other;
  unsigned int calls;
  int cancel_self;
  int cancel_other;
  int rearm_once;
};
static void timer_callback(cpkt_opcua_server *server, void *context) {
  struct timer_state *state = (struct timer_state *)context;
  CHECK(server == state->server);
  ++state->calls;
  if (state->rearm_once && state->calls == 1) {
    CHECK(cpkt_opcua_server_changeRepeatedCallbackInterval_typed(
              server, state->id, 1) == 0);
    return;
  }
  if (state->cancel_other)
    cpkt_opcua_server_removeCallback_typed(server, state->other);
  if (state->cancel_self)
    cpkt_opcua_server_removeCallback_typed(server, state->id);
}
struct iterator_state {
  unsigned int calls, forward, inverse;
  cpkt_opcua_StatusCode stop;
};
static cpkt_opcua_StatusCode child_callback(cpkt_opcua_NodeId child,
                                            cpkt_opcua_Boolean inverse,
                                            cpkt_opcua_NodeId reference,
                                            void *context) {
  struct iterator_state *state = (struct iterator_state *)context;
  CHECK(child.identifierType == CPKT_OPCUA_NODEIDTYPE_NUMERIC);
  CHECK(reference.identifierType == CPKT_OPCUA_NODEIDTYPE_NUMERIC);
  ++state->calls;
  if (inverse)
    ++state->inverse;
  else
    ++state->forward;
  return state->stop;
}
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
extern void cpkt_types_fail_after(size_t count);
extern int cpkt_types_fail_stop(void);
#endif
static void test_server_timers_and_iterator(void) {
  cpkt_opcua_server *server;
  cpkt_opcua_DateTime past, future;
  cpkt_opcua_UInt64 unknown;
  struct timer_state once, repeat, victim, rearmed, converted;
  struct iterator_state iterator;
  unsigned int i;
  CHECK(cpkt_opcua_server_new(&server, 0) == CPKT_OPCUA_OK);
  memset(&once, 0, sizeof(once));
  memset(&repeat, 0, sizeof(repeat));
  memset(&victim, 0, sizeof(victim));
  memset(&rearmed, 0, sizeof(rearmed));
  memset(&converted, 0, sizeof(converted));
  once.server = repeat.server = victim.server = server;
  rearmed.server = converted.server = server;
  memset(&past, 0, sizeof(past));
  future.high32 = 0x7fffffffU;
  future.low32 = 0xffffffffU;
  CHECK(cpkt_opcua_server_addTimedCallback_typed(NULL, timer_callback, &once,
                                                 past, &once.id) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(!once.id.high32 && !once.id.low32);
  CHECK(cpkt_opcua_server_addTimedCallback_typed(server, NULL, &once, past,
                                                 NULL) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_server_addRepeatedCallback_typed(
            server, timer_callback, &repeat, 0, &repeat.id) != 0);
  CHECK(!repeat.id.high32 && !repeat.id.low32);
  CHECK(cpkt_opcua_server_addTimedCallback_typed(
            server, timer_callback, &victim, future, &victim.id) == 0);
  once.cancel_self = once.cancel_other = 1;
  once.other = victim.id;
  CHECK(cpkt_opcua_server_addTimedCallback_typed(server, timer_callback, &once,
                                                 past, &once.id) == 0);
  unknown = once.id;
  unknown.high32 ^= 0x80000000U;
  cpkt_opcua_server_removeCallback_typed(server, unknown);
  CHECK(cpkt_opcua_server_startup(server, NULL) == CPKT_OPCUA_OK);
  for (i = 0; i < 16 && !once.calls; ++i)
    CHECK(cpkt_opcua_server_iterate(server, 1, NULL) == CPKT_OPCUA_OK);
  CHECK(once.calls == 1 && victim.calls == 0);
  repeat.cancel_self = 1;
  CHECK(cpkt_opcua_server_addRepeatedCallback_typed(
            server, timer_callback, &repeat, 100, &repeat.id) == 0);
  CHECK(cpkt_opcua_server_changeRepeatedCallbackInterval_typed(
            server, repeat.id, 1) == 0);
  for (i = 0; i < 128 && !repeat.calls; ++i)
    CHECK(cpkt_opcua_server_iterate(server, 1, NULL) == CPKT_OPCUA_OK);
  CHECK(repeat.calls == 1);
  for (i = 0; i < 3; ++i)
    CHECK(cpkt_opcua_server_iterate(server, 0, NULL) == CPKT_OPCUA_OK);
  CHECK(repeat.calls == 1 && once.calls == 1 && victim.calls == 0);
  rearmed.rearm_once = rearmed.cancel_self = 1;
  CHECK(cpkt_opcua_server_addTimedCallback_typed(
            server, timer_callback, &rearmed, past, &rearmed.id) == 0);
  for (i = 0; i < 128 && rearmed.calls < 2; ++i)
    CHECK(cpkt_opcua_server_iterate(server, 1, NULL) == CPKT_OPCUA_OK);
  CHECK(rearmed.calls == 2);
  converted.cancel_self = 1;
  CHECK(cpkt_opcua_server_addTimedCallback_typed(
            server, timer_callback, &converted, future, &converted.id) == 0);
  CHECK(cpkt_opcua_server_changeRepeatedCallbackInterval_typed(
            server, converted.id, 1) == 0);
  for (i = 0; i < 128 && !converted.calls; ++i)
    CHECK(cpkt_opcua_server_iterate(server, 1, NULL) == CPKT_OPCUA_OK);
  CHECK(converted.calls == 1);
  CHECK(cpkt_opcua_server_changeRepeatedCallbackInterval_typed(
            NULL, repeat.id, 1) == CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  cpkt_opcua_server_removeCallback_typed(server, victim.id);
  cpkt_opcua_server_removeCallback_typed(NULL, victim.id);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  for (i = 0; i < 8; ++i) {
    cpkt_opcua_StatusCode status;
    int injected;
    cpkt_types_fail_after(i);
    status = cpkt_opcua_server_addTimedCallback_typed(
        server, timer_callback, &victim, future, &victim.id);
    injected = cpkt_types_fail_stop();
    if (!status) {
      CHECK(!injected);
      cpkt_opcua_server_removeCallback_typed(server, victim.id);
      break;
    }
    CHECK(injected && status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY);
    CHECK(!victim.id.high32 && !victim.id.low32);
  }
  CHECK(i < 8);
#endif
  memset(&iterator, 0, sizeof(iterator));
  CHECK(cpkt_opcua_server_forEachChildNodeCall_typed(
            server, number(0, CPKT_OPCUA_NS0ID_OBJECTSFOLDER), child_callback,
            &iterator) == 0);
  CHECK(iterator.calls > 1 && iterator.forward && iterator.inverse);
  memset(&iterator, 0, sizeof(iterator));
  iterator.stop = CPKT_OPCUA_STATUSCODE_BADUNEXPECTEDERROR;
  CHECK(cpkt_opcua_server_forEachChildNodeCall_typed(
            server, number(0, CPKT_OPCUA_NS0ID_OBJECTSFOLDER), child_callback,
            &iterator) == iterator.stop);
  CHECK(iterator.calls == 1);
  CHECK(cpkt_opcua_server_forEachChildNodeCall_typed(
            server, number(0, 0xffffffffU), child_callback, &iterator) ==
        CPKT_OPCUA_STATUSCODE_BADNODEIDUNKNOWN);
  CHECK(cpkt_opcua_server_forEachChildNodeCall_typed(
            server, number(0, CPKT_OPCUA_NS0ID_OBJECTSFOLDER), NULL,
            &iterator) == CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_server_addTimedCallback_typed(server, timer_callback,
                                                 &victim, future, NULL) == 0);
  CHECK(cpkt_opcua_server_addRepeatedCallback_typed(server, timer_callback,
                                                    &victim, 60000, NULL) == 0);
  CHECK(cpkt_opcua_server_shutdown(server, NULL) == CPKT_OPCUA_OK);
  cpkt_opcua_server_free(server);
  CHECK(victim.calls == 0);
}
