#include "opcua_callbacks_test.h"
#include "opcua_types_peer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void check(int ok, const char *expression, int line) {
  if (!ok) {
    fprintf(stderr, "client high-level line %d: %s\n", line, expression);
    abort();
  }
}
#define CHECK(e) check(!!(e), #e, __LINE__)
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
void cpkt_types_fail_after(size_t count);
int cpkt_types_fail_stop(void);
#endif
static cpkt_opcua_NodeId node(unsigned short ns, unsigned int id) {
  cpkt_opcua_NodeId n;
  cpkt_opcua_NodeId_init(&n);
  n.namespaceIndex = ns;
  n.identifier.numeric = id;
  return n;
}
static cpkt_opcua_String text(const char *s) {
  cpkt_opcua_String result;
  result.length = strlen(s);
  result.data = (cpkt_opcua_Byte *)s;
  return result;
}
static cpkt_opcua_QualifiedName name(const char *s) {
  cpkt_opcua_QualifiedName result;
  result.namespaceIndex = 1;
  result.name = text(s);
  return result;
}
static cpkt_opcua_LocalizedText label(const char *s) {
  cpkt_opcua_LocalizedText result;
  cpkt_opcua_LocalizedText_init(&result);
  result.text = text(s);
  return result;
}
#define READ_SCALAR(method, type, id, expected)                                \
  do {                                                                         \
    cpkt_opcua_##type out;                                                     \
    memset(&out, 0, sizeof(out));                                              \
    CHECK(!cpkt_opcua_client_read##method##Attribute_typed(client, id, &out)); \
    CHECK(out == (expected));                                                  \
  } while (0)
#define WRITE_READ(method, type, id, expected)                                 \
  do {                                                                         \
    cpkt_opcua_##type in = (expected);                                         \
    CHECK(!cpkt_opcua_client_write##method##Attribute_typed(client, id, &in)); \
    READ_SCALAR(method, type, id, expected);                                   \
  } while (0)
#define LABEL_RW(method, id, value)                                            \
  do {                                                                         \
    cpkt_opcua_LocalizedText in = label(value), out;                           \
    CHECK(!cpkt_opcua_client_write##method##Attribute_typed(client, id, &in)); \
    CHECK(!cpkt_opcua_client_read##method##Attribute_typed(client, id, &out)); \
    CHECK(cpkt_opcua_String_equal(&in.text, &out.text));                       \
    cpkt_opcua_LocalizedText_clear(&out);                                      \
  } while (0)
struct history_state {
  cpkt_opcua_client *client;
  int kind, calls, stop;
};
static cpkt_opcua_Boolean history_page(cpkt_opcua_client *client,
                                       const cpkt_opcua_NodeId *id,
                                       cpkt_opcua_Boolean more,
                                       const cpkt_opcua_ExtensionObject *data,
                                       void *context) {
  struct history_state *s = context;
  const cpkt_opcua_Variant *value;
  const cpkt_opcua_DataValue *dv = NULL;
  cpkt_opcua_Int64 *integer;
  CHECK(s && s->client == client && id && id->namespaceIndex == 1 &&
        id->identifier.numeric == 6001 && data &&
        data->encoding == CPKT_OPCUA_EXTENSIONOBJECT_DECODED);
  if (s->kind == 0) {
    const cpkt_opcua_HistoryData *items = data->content.decoded.data;
    CHECK(data->content.decoded.type ==
              cpkt_opcua_type_at(CPKT_OPCUA_TYPES_HISTORYDATA) &&
          items->dataValuesSize == 1);
    dv = items->dataValues;
    value = &dv->value;
  } else if (s->kind == 1) {
    const cpkt_opcua_HistoryModifiedData *items = data->content.decoded.data;
    CHECK(data->content.decoded.type ==
              cpkt_opcua_type_at(CPKT_OPCUA_TYPES_HISTORYMODIFIEDDATA) &&
          items->dataValuesSize == 1 && items->modificationInfosSize == 1);
    CHECK(items->modificationInfos[0].modificationTime.high32 == 0x7fffffffU &&
          items->modificationInfos[0].modificationTime.low32 == 0xffffffffU);
    dv = items->dataValues;
    value = &dv->value;
  } else {
    const cpkt_opcua_HistoryEvent *items = data->content.decoded.data;
    CHECK(data->content.decoded.type ==
              cpkt_opcua_type_at(CPKT_OPCUA_TYPES_HISTORYEVENT) &&
          items->eventsSize == 1 && items->events[0].eventFieldsSize == 1);
    value = items->events[0].eventFields;
  }
  CHECK(value->type == cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64) &&
        value->data);
  integer = value->data;
  CHECK(integer->high32 == (s->calls ? 0x7fffffffU : 0x80000000U) &&
        integer->low32 == (s->calls ? 0xffffffffU : 0));
  if (dv)
    CHECK(dv->hasSourceTimestamp &&
          dv->sourceTimestamp.high32 == integer->high32 &&
          dv->sourceTimestamp.low32 == integer->low32);
  CHECK(!!more == !s->calls);
  ++s->calls;
  return !s->stop;
}
struct iterator_state {
  int calls, forward, inverse;
  cpkt_opcua_StatusCode result;
};
static cpkt_opcua_StatusCode child(cpkt_opcua_NodeId id,
                                   cpkt_opcua_Boolean inverse,
                                   cpkt_opcua_NodeId reference, void *context) {
  struct iterator_state *s = context;
  CHECK(id.identifier.numeric && reference.identifier.numeric);
  ++s->calls;
  if (inverse)
    ++s->inverse;
  else
    ++s->forward;
  return s->result;
}
void cpkt_types_test_client(cpkt_opcua_client *client, void *peer) {
  cpkt_opcua_NodeId object = node(1, 6200), variable = node(1, 6201),
                    vtype = node(1, 6202), otype = node(1, 6203),
                    rtype = node(1, 6204), dtype = node(1, 6205),
                    view = node(1, 6206), method = node(1, 6207), out;
  cpkt_opcua_ObjectAttributes oa;
  cpkt_opcua_VariableAttributes va;
  cpkt_opcua_VariableTypeAttributes vta;
  cpkt_opcua_ObjectTypeAttributes ota;
  cpkt_opcua_ReferenceTypeAttributes rta;
  cpkt_opcua_DataTypeAttributes dta;
  cpkt_opcua_ViewAttributes viewa;
  cpkt_opcua_MethodAttributes ma;
  cpkt_opcua_Variant value, output, *outputs;
  cpkt_opcua_DataValue data;
  cpkt_opcua_Int64 integers[2];
  cpkt_opcua_UInt32 dimension = 2, *dimensions;
  cpkt_opcua_QualifiedName qualified;
  cpkt_opcua_ExpandedNodeId target;
  cpkt_opcua_String uri;
  cpkt_opcua_UInt16 ns;
  cpkt_opcua_ReadValueId read;
  cpkt_opcua_WriteValue write;
  cpkt_opcua_BrowseDescription description;
  cpkt_opcua_BrowseResult browsed, next;
  cpkt_opcua_ViewDescription vd;
  cpkt_opcua_BrowsePath path;
  cpkt_opcua_RelativePathElement element;
  cpkt_opcua_BrowsePathResult translated;
  cpkt_opcua_StructureDefinition definition;
  struct history_state history;
  struct iterator_state iterator;
  cpkt_opcua_DateTime start, end;
  cpkt_opcua_EventFilter filter;
  cpkt_opcua_SimpleAttributeOperand operand;
  size_t count, i;
  unsigned int pages[3], releases[3];
  cpkt_opcua_StatusCode status;
  CHECK(!cpkt_types_peer_highlevel_setup(peer));
  CHECK(!cpkt_opcua_ObjectAttributes_default(&oa));
  CHECK(!cpkt_opcua_VariableAttributes_default(&va));
  CHECK(!cpkt_opcua_VariableTypeAttributes_default(&vta));
  CHECK(!cpkt_opcua_ObjectTypeAttributes_default(&ota));
  CHECK(!cpkt_opcua_ReferenceTypeAttributes_default(&rta));
  CHECK(!cpkt_opcua_DataTypeAttributes_default(&dta));
  CHECK(!cpkt_opcua_ViewAttributes_default(&viewa));
  CHECK(!cpkt_opcua_MethodAttributes_default(&ma));
  oa.writeMask = va.writeMask = vta.writeMask = ota.writeMask = rta.writeMask =
      dta.writeMask = viewa.writeMask = ma.writeMask = 0xffffffffU;
  integers[0].high32 = 0x80000000U;
  integers[0].low32 = 0;
  integers[1].high32 = 0x7fffffffU;
  integers[1].low32 = 0xffffffffU;
  cpkt_opcua_Variant_init(&value);
  value.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
  value.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
  value.arrayLength = 2;
  value.data = integers;
  va.dataType = node(0, CPKT_OPCUA_NS0ID_INT64);
  va.valueRank = 1;
  va.accessLevel = 0xff;
  va.arrayDimensionsSize = 1;
  va.arrayDimensions = &dimension;
  va.value = value;
  CHECK(!cpkt_opcua_client_addObjectNode_typed(
      client, object, node(0, CPKT_OPCUA_NS0ID_OBJECTSFOLDER),
      node(0, CPKT_OPCUA_NS0ID_ORGANIZES), name("client-object"),
      node(0, CPKT_OPCUA_NS0ID_BASEOBJECTTYPE), oa, &out));
  CHECK(cpkt_opcua_NodeId_equal(&out, &object));
  cpkt_opcua_NodeId_clear(&out);
  CHECK(!cpkt_opcua_client_addVariableNode_typed(
      client, variable, object, node(0, CPKT_OPCUA_NS0ID_HASCOMPONENT),
      name("client-variable"), node(0, CPKT_OPCUA_NS0ID_BASEDATAVARIABLETYPE),
      va, &out));
  CHECK(cpkt_opcua_NodeId_equal(&out, &variable));
  cpkt_opcua_NodeId_clear(&out);
  CHECK(!cpkt_opcua_client_addVariableTypeNode_typed(
      client, vtype, node(0, CPKT_OPCUA_NS0ID_BASEVARIABLETYPE),
      node(0, CPKT_OPCUA_NS0ID_HASSUBTYPE), name("client-variable-type"), vta,
      NULL));
  CHECK(!cpkt_opcua_client_addObjectTypeNode_typed(
      client, otype, node(0, CPKT_OPCUA_NS0ID_BASEOBJECTTYPE),
      node(0, CPKT_OPCUA_NS0ID_HASSUBTYPE), name("client-object-type"), ota,
      NULL));
  CHECK(!cpkt_opcua_client_addReferenceTypeNode_typed(
      client, rtype, node(0, CPKT_OPCUA_NS0ID_NONHIERARCHICALREFERENCES),
      node(0, CPKT_OPCUA_NS0ID_HASSUBTYPE), name("client-reference-type"), rta,
      NULL));
  CHECK(!cpkt_opcua_client_addDataTypeNode_typed(
      client, dtype, node(0, CPKT_OPCUA_NS0ID_BASEDATATYPE),
      node(0, CPKT_OPCUA_NS0ID_HASSUBTYPE), name("client-data-type"), dta,
      NULL));
  CHECK(!cpkt_opcua_client_addViewNode_typed(
      client, view, node(0, CPKT_OPCUA_NS0ID_VIEWSFOLDER),
      node(0, CPKT_OPCUA_NS0ID_ORGANIZES), name("client-view"), viewa, NULL));
  CHECK(!cpkt_opcua_client_addMethodNode_typed(
      client, method, object, node(0, CPKT_OPCUA_NS0ID_HASCOMPONENT),
      name("client-method"), ma, NULL));
  memset(&va.value, 0, sizeof(va.value));
  va.arrayDimensions = NULL;
  va.arrayDimensionsSize = 0;
  cpkt_opcua_ObjectAttributes_clear(&oa);
  cpkt_opcua_VariableAttributes_clear(&va);
  cpkt_opcua_VariableTypeAttributes_clear(&vta);
  cpkt_opcua_ObjectTypeAttributes_clear(&ota);
  cpkt_opcua_ReferenceTypeAttributes_clear(&rta);
  cpkt_opcua_DataTypeAttributes_clear(&dta);
  cpkt_opcua_ViewAttributes_clear(&viewa);
  cpkt_opcua_MethodAttributes_clear(&ma);
  READ_SCALAR(NodeClass, NodeClass, variable, cpkt_opcua_NODECLASS_VARIABLE);
  CHECK(!cpkt_opcua_client_readNodeIdAttribute_typed(client, variable, &out));
  CHECK(cpkt_opcua_NodeId_equal(&out, &variable));
  cpkt_opcua_NodeId_clear(&out);
  CHECK(!cpkt_opcua_client_readBrowseNameAttribute_typed(client, variable,
                                                         &qualified));
  CHECK(qualified.name.length == 15 &&
        !memcmp(qualified.name.data, "client-variable", 15));
  CHECK(cpkt_opcua_client_writeBrowseNameAttribute_typed(client, variable,
                                                         &qualified) ==
        CPKT_OPCUA_STATUSCODE_BADWRITENOTSUPPORTED);
  cpkt_opcua_QualifiedName_clear(&qualified);
  LABEL_RW(DisplayName, variable, "client-display");
  LABEL_RW(Description, variable, "client-description");
  LABEL_RW(InverseName, rtype, "client-inverse");
  WRITE_READ(WriteMask, UInt32, variable, 0xffffffffU);
  READ_SCALAR(UserWriteMask, UInt32, variable, 0xffffffffU);
  WRITE_READ(IsAbstract, Boolean, otype, 1);
  WRITE_READ(Symmetric, Boolean, rtype, 1);
  WRITE_READ(ContainsNoLoops, Boolean, view, 1);
  WRITE_READ(EventNotifier, Byte, object, 1);
  WRITE_READ(ValueRank, Int32, variable, 1);
  WRITE_READ(AccessLevel, Byte, variable, 0xff);
  WRITE_READ(AccessLevelEx, UInt32, variable, 0xff);
  READ_SCALAR(UserAccessLevel, Byte, variable, 0xff);
  WRITE_READ(MinimumSamplingInterval, Double, variable, 12.5);
  WRITE_READ(Historizing, Boolean, variable, 1);
  WRITE_READ(Executable, Boolean, method, 0);
  READ_SCALAR(UserExecutable, Boolean, method, 0);
  CHECK(!cpkt_opcua_client_readDataTypeAttribute_typed(client, variable, &out));
  CHECK(out.identifier.numeric == CPKT_OPCUA_NS0ID_INT64);
  CHECK(
      !cpkt_opcua_client_writeDataTypeAttribute_typed(client, variable, &out));
  cpkt_opcua_NodeId_clear(&out);
  CHECK(!cpkt_opcua_client_readDatatypeDefinitionAttribute_typed(
      client, node(0, CPKT_OPCUA_NS0ID_ARGUMENT), &definition));
  CHECK(definition.fieldsSize == 5);
  cpkt_opcua_StructureDefinition_clear(&definition);
  CHECK(!cpkt_opcua_client_writeArrayDimensionsAttribute_typed(client, variable,
                                                               1, &dimension));
  CHECK(!cpkt_opcua_client_readArrayDimensionsAttribute_typed(
      client, variable, &count, &dimensions));
  CHECK(count == 1 && dimensions[0] == 2);
  cpkt_opcua_array_delete(dimensions, count,
                          cpkt_opcua_type_at(CPKT_OPCUA_TYPES_UINT32));
  {
    cpkt_opcua_NodeClass cls = cpkt_opcua_NODECLASS_VARIABLE;
    cpkt_opcua_UInt32 mask = 1;
    cpkt_opcua_Byte access = 3;
    cpkt_opcua_Boolean executable = 1;
    CHECK(cpkt_opcua_client_writeNodeIdAttribute_typed(client, variable,
                                                       &variable) ==
          CPKT_OPCUA_STATUSCODE_BADWRITENOTSUPPORTED);
    CHECK(cpkt_opcua_client_writeNodeClassAttribute_typed(client, variable,
                                                          &cls) ==
          CPKT_OPCUA_STATUSCODE_BADWRITENOTSUPPORTED);
    CHECK(cpkt_opcua_client_writeUserWriteMaskAttribute_typed(client, variable,
                                                              &mask) ==
          CPKT_OPCUA_STATUSCODE_BADWRITENOTSUPPORTED);
    CHECK(cpkt_opcua_client_writeUserAccessLevelAttribute_typed(
              client, variable, &access) ==
          CPKT_OPCUA_STATUSCODE_BADWRITENOTSUPPORTED);
    CHECK(cpkt_opcua_client_writeUserExecutableAttribute_typed(client, method,
                                                               &executable) ==
          CPKT_OPCUA_STATUSCODE_BADWRITENOTSUPPORTED);
  }
  CHECK(!cpkt_opcua_client_readValueAttribute_typed(client, variable, &output));
  CHECK(output.arrayLength == 2 && output.type == value.type &&
        !memcmp(output.data, integers, sizeof(integers)));
  cpkt_opcua_Variant_clear(&output);
  integers[0].low32 = 17;
  CHECK(!cpkt_opcua_client_writeValueAttribute_typed(client, variable, &value));
  cpkt_opcua_WriteValue_init(&write);
  write.nodeId = variable;
  write.attributeId = 13;
  write.value.hasValue = 1;
  write.value.value = value;
  CHECK(!cpkt_opcua_client_write_typed(client, &write));
  CHECK(!cpkt_opcua_client_writeValueAttributeEx_typed(client, variable,
                                                       &write.value));
  write.value.hasStatus = 1;
  write.value.status = CPKT_OPCUA_STATUSCODE_GOODCLAMPED;
  CHECK(!cpkt_opcua_client_writeValueAttributeEx_typed(client, variable,
                                                       &write.value));
  CHECK(cpkt_opcua_client_readValueAttribute_typed(client, variable, &output) ==
            CPKT_OPCUA_STATUSCODE_GOODCLAMPED &&
        !output.type && !output.data);
  write.value.status = 0x00000480U;
  CHECK(!cpkt_opcua_client_writeValueAttributeEx_typed(client, variable,
                                                       &write.value));
  CHECK(cpkt_opcua_client_readValueAttribute_typed(client, variable, &output) ==
            write.value.status &&
        output.type == value.type && output.arrayLength == 2 &&
        !memcmp(output.data, integers, sizeof(integers)));
  cpkt_opcua_Variant_clear(&output);
  write.value.status = CPKT_OPCUA_STATUSCODE_UNCERTAIN;
  CHECK(!cpkt_opcua_client_writeValueAttributeEx_typed(client, variable,
                                                       &write.value));
  CHECK(cpkt_opcua_client_readValueAttribute_typed(client, variable, &output) ==
            CPKT_OPCUA_STATUSCODE_UNCERTAIN &&
        !output.type && !output.data);
  write.value.status = CPKT_OPCUA_STATUSCODE_GOOD;
  CHECK(!cpkt_opcua_client_writeValueAttributeEx_typed(client, variable,
                                                       &write.value));
  cpkt_opcua_ReadValueId_init(&read);
  read.nodeId = variable;
  read.attributeId = 13;
  read.indexRange = text("0");
  CHECK(!cpkt_opcua_client_read_typed(client, &read, &data));
  CHECK(data.hasValue && data.value.arrayLength == 1 &&
        ((cpkt_opcua_Int64 *)data.value.data)->low32 == 17);
  cpkt_opcua_DataValue_clear(&data);
  /* Dynamic scalar input can carry an exact C89 Int64 on every target. */
  read.nodeId = node(1, 6300);
  read.indexRange = text("");
  CHECK(!cpkt_opcua_VariableAttributes_default(&va));
  va.dataType = node(0, CPKT_OPCUA_NS0ID_INT64);
  va.valueRank = -1;
  va.accessLevel = 3;
  va.value = value;
  va.value.arrayLength = 0;
  CHECK(!cpkt_opcua_client_addVariableNode_typed(
      client, read.nodeId, object, node(0, CPKT_OPCUA_NS0ID_HASCOMPONENT),
      name("client-scalar"), node(0, CPKT_OPCUA_NS0ID_BASEDATAVARIABLETYPE), va,
      NULL));
  memset(&va.value, 0, sizeof(va.value));
  cpkt_opcua_VariableAttributes_clear(&va);
  CHECK(!cpkt_opcua_client_writeValueAttribute_scalar_typed(
      client, read.nodeId, &integers[1], value.type));
  CHECK(!cpkt_opcua_client_read_typed(client, &read, &data));
  CHECK(data.hasValue &&
        ((cpkt_opcua_Int64 *)data.value.data)->high32 == 0x7fffffffU &&
        ((cpkt_opcua_Int64 *)data.value.data)->low32 == 0xffffffffU);
  cpkt_opcua_DataValue_clear(&data);
  CHECK(!cpkt_opcua_client_deleteNode_typed(client, read.nodeId, 1));
  CHECK(cpkt_opcua_client_call_typed(
            client, node(0, CPKT_OPCUA_NS0ID_OBJECTSFOLDER), node(1, 6100), 1,
            &value, &count, &outputs) == CPKT_OPCUA_STATUSCODE_GOODCLAMPED);
  CHECK(count == 1 && outputs[0].arrayLength == 2 &&
        !memcmp(outputs[0].data, integers, sizeof(integers)));
  cpkt_opcua_array_delete(outputs, count,
                          cpkt_opcua_type_at(CPKT_OPCUA_TYPES_VARIANT));
  count = 99;
  CHECK(cpkt_opcua_client_call_typed(
            client, node(0, CPKT_OPCUA_NS0ID_OBJECTSFOLDER), node(1, 6100), 1,
            &value, &count, NULL) == CPKT_OPCUA_STATUSCODE_GOODCLAMPED &&
        count == 99);
  outputs = &value;
  CHECK(cpkt_opcua_client_call_typed(
            client, node(0, CPKT_OPCUA_NS0ID_OBJECTSFOLDER), node(1, 6100), 1,
            &value, NULL, &outputs) == CPKT_OPCUA_STATUSCODE_GOODCLAMPED &&
        outputs == &value);
  CHECK(!cpkt_opcua_client_call_typed(
            client, node(0, CPKT_OPCUA_NS0ID_OBJECTSFOLDER), node(1, 6101), 0,
            NULL, &count, &outputs) &&
        count == 0);
  cpkt_opcua_array_delete(outputs, count,
                          cpkt_opcua_type_at(CPKT_OPCUA_TYPES_VARIANT));
  cpkt_opcua_BrowseDescription_init(&description);
  description.nodeId = object;
  description.browseDirection = cpkt_opcua_BROWSEDIRECTION_FORWARD;
  description.resultMask = 63;
  cpkt_opcua_ViewDescription_init(&vd);
  CHECK(
      !cpkt_opcua_client_browse_typed(client, &vd, 1, &description, &browsed));
  CHECK(!browsed.statusCode && browsed.referencesSize == 1 &&
        browsed.continuationPoint.length);
  CHECK(!cpkt_opcua_client_browseNext_typed(client, 0,
                                            browsed.continuationPoint, &next));
  CHECK(!next.statusCode && next.referencesSize == 1);
  cpkt_opcua_BrowseResult_clear(&browsed);
  if (next.continuationPoint.length) {
    CHECK(!cpkt_opcua_client_browseNext_typed(client, 1, next.continuationPoint,
                                              &browsed));
    cpkt_opcua_BrowseResult_clear(&browsed);
  }
  cpkt_opcua_BrowseResult_clear(&next);
  CHECK(!cpkt_opcua_client_browse_typed(client, NULL, 0, NULL, &browsed));
  CHECK(browsed.statusCode == CPKT_OPCUA_STATUSCODE_BADINTERNALERROR);
  cpkt_opcua_BrowseResult_clear(&browsed);
  cpkt_opcua_BrowsePath_init(&path);
  cpkt_opcua_RelativePathElement_init(&element);
  path.startingNode = object;
  element.targetName = name("client-variable");
  element.referenceTypeId = node(0, CPKT_OPCUA_NS0ID_HASCOMPONENT);
  path.relativePath.elementsSize = 1;
  path.relativePath.elements = &element;
  CHECK(!cpkt_opcua_client_translateBrowsePathToNodeIds_typed(client, &path,
                                                              &translated));
  CHECK(!translated.statusCode && translated.targetsSize == 1 &&
        cpkt_opcua_NodeId_equal(&translated.targets[0].targetId.nodeId,
                                &variable));
  cpkt_opcua_BrowsePathResult_clear(&translated);
  cpkt_opcua_ExpandedNodeId_init(&target);
  target.nodeId = variable;
  CHECK(!cpkt_opcua_client_addReference_typed(
      client, object, node(0, CPKT_OPCUA_NS0ID_ORGANIZES), 1, text(""), target,
      cpkt_opcua_NODECLASS_VARIABLE));
  CHECK(!cpkt_opcua_client_deleteReference_typed(
      client, object, node(0, CPKT_OPCUA_NS0ID_ORGANIZES), 1, target, 1));
  memset(&iterator, 0, sizeof(iterator));
  CHECK(!cpkt_opcua_client_forEachChildNodeCall_typed(client, object, child,
                                                      &iterator));
  CHECK(iterator.calls >= 3 && iterator.forward && iterator.inverse);
  count = (size_t)iterator.calls;
  iterator.calls = 0;
  iterator.result = CPKT_OPCUA_STATUSCODE_BADINTERNALERROR;
  CHECK(cpkt_opcua_client_forEachChildNodeCall_typed(
            client, object, child, &iterator) == iterator.result &&
        (size_t)iterator.calls == count);
  uri = text("http://opcfoundation.org/UA/");
  ns = 999;
  CHECK(!cpkt_opcua_client_NamespaceGetIndex_typed(client, &uri, &ns) &&
        ns == 0);
  uri = text("urn:absent-namespace");
  ns = 999;
  CHECK(cpkt_opcua_client_NamespaceGetIndex_typed(client, &uri, &ns) ==
            CPKT_OPCUA_STATUSCODE_BADNOTFOUND &&
        ns == 999);
  start.high32 = 0x80000000U;
  start.low32 = 0;
  end.high32 = 0x7fffffffU;
  end.low32 = 0xffffffffU;
  cpkt_opcua_EventFilter_init(&filter);
  cpkt_opcua_SimpleAttributeOperand_init(&operand);
  operand.attributeId = 13;
  filter.selectClausesSize = 1;
  filter.selectClauses = &operand;
  for (i = 0; i < 3; ++i) {
    for (count = 0; count < 2; ++count) {
      memset(&history, 0, sizeof(history));
      history.client = client;
      history.kind = (int)i;
      history.stop = (int)count;
      out = node(1, 6001);
      if (i == 0)
        status = cpkt_opcua_client_HistoryRead_raw_typed(
            client, &out, history_page, start, end, text(""), 0, 1,
            cpkt_opcua_TIMESTAMPSTORETURN_BOTH, &history);
      else if (i == 1)
        status = cpkt_opcua_client_HistoryRead_modified_typed(
            client, &out, history_page, start, end, text(""), 0, 1,
            cpkt_opcua_TIMESTAMPSTORETURN_BOTH, &history);
      else
        status = cpkt_opcua_client_HistoryRead_events_typed(
            client, &out, history_page, start, end, text(""), filter, 1,
            cpkt_opcua_TIMESTAMPSTORETURN_BOTH, &history);
      CHECK(!status && history.calls == (count ? 1 : 2));
    }
  }
  cpkt_types_peer_highlevel_history_counts(peer, pages, releases);
  for (i = 0; i < 3; ++i)
    CHECK(pages[i] == 3 && releases[i] == 1);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  memset(&history, 0, sizeof(history));
  history.client = client;
  out = node(1, 6001);
  cpkt_types_peer_client_fail_conversion(4, 0);
  status = cpkt_opcua_client_HistoryRead_raw_typed(
      client, &out, history_page, start, end, text(""), 0, 1,
      cpkt_opcua_TIMESTAMPSTORETURN_BOTH, &history);
  CHECK(cpkt_types_fail_stop() && !history.calls &&
        status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY);
  cpkt_types_peer_highlevel_history_counts(peer, pages, releases);
  CHECK(pages[0] == 4 && releases[0] == 2);
#endif
  cpkt_opcua_DataValue_init(&data);
  data.hasValue = 1;
  data.value = value;
  data.value.arrayLength = 0;
  integers[0].low32 = 0;
  out = node(1, 6001);
  CHECK(cpkt_opcua_client_HistoryUpdate_insert_typed(client, &out, &data) ==
        CPKT_OPCUA_STATUSCODE_GOODCLAMPED);
  CHECK(cpkt_opcua_client_HistoryUpdate_replace_typed(client, &out, &data) ==
        CPKT_OPCUA_STATUSCODE_GOODCLAMPED);
  CHECK(cpkt_opcua_client_HistoryUpdate_update_typed(client, &out, &data) ==
        CPKT_OPCUA_STATUSCODE_GOODCLAMPED);
  CHECK(cpkt_opcua_client_HistoryUpdate_deleteRaw_typed(
            client, &out, start, end) == CPKT_OPCUA_STATUSCODE_GOODCLAMPED);
  CHECK(cpkt_opcua_client_writeValueAttribute_scalar_typed(client, variable,
                                                           integers, NULL) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_client_writeArrayDimensionsAttribute_typed(client, variable,
                                                              0, NULL) ==
        CPKT_OPCUA_STATUSCODE_BADTYPEMISMATCH);
  CHECK(cpkt_opcua_client_writeArrayDimensionsAttribute_typed(
            client, variable, (size_t)-1, &dimension) ==
        CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY);
  CHECK(cpkt_opcua_client_readValueAttribute_typed(NULL, variable, &output) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_client_readValueAttribute_typed(client, variable, NULL) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(!cpkt_opcua_client_read_typed(client, &(read), &data));
  CHECK(data.hasStatus &&
        data.status == CPKT_OPCUA_STATUSCODE_BADNODEIDUNKNOWN);
  cpkt_opcua_DataValue_clear(&data);
  CHECK(cpkt_opcua_client_readValueAttribute_typed(client, read.nodeId,
                                                   &output) ==
            CPKT_OPCUA_STATUSCODE_BADNODEIDUNKNOWN &&
        !output.data);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  cpkt_types_fail_after(0);
  status = cpkt_opcua_client_writeValueAttribute_scalar_typed(
      client, variable, integers, value.type);
  CHECK(cpkt_types_fail_stop() &&
        status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY);
#endif
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  /* Arm failures only AFTER the real native client has returned its owned
   * output. This isolates the C89 conversion from network/native allocation. */
  for (i = 0; i < 32; ++i) {
    cpkt_types_peer_client_fail_conversion(1, i);
    status =
        cpkt_opcua_client_readValueAttribute_typed(client, variable, &output);
    if (!cpkt_types_fail_stop()) {
      CHECK(!status);
      cpkt_opcua_Variant_clear(&output);
      break;
    }
    CHECK(status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY && !output.data &&
          !output.type);
  }
  CHECK(i > 0 && i < 32);
  for (i = 0; i < 32; ++i) {
    cpkt_types_peer_client_fail_conversion(2, i);
    status = cpkt_opcua_client_call_typed(
        client, node(0, CPKT_OPCUA_NS0ID_OBJECTSFOLDER), node(1, 6100), 1,
        &value, &count, &outputs);
    if (!cpkt_types_fail_stop()) {
      CHECK(status == CPKT_OPCUA_STATUSCODE_GOODCLAMPED);
      cpkt_opcua_array_delete(outputs, count,
                              cpkt_opcua_type_at(CPKT_OPCUA_TYPES_VARIANT));
      break;
    }
    CHECK(status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY && !outputs && !count);
  }
  CHECK(i > 0 && i < 32);
  {
    cpkt_opcua_NodeId requested = node(1, 0);
    requested.identifierType = CPKT_OPCUA_NODEIDTYPE_STRING;
    requested.identifier.string = text("created-before-conversion-error");
    CHECK(!cpkt_opcua_VariableAttributes_default(&va));
    va.dataType = node(0, CPKT_OPCUA_NS0ID_INT64);
    va.valueRank = -1;
    va.value = value;
    va.value.arrayLength = 0;
    cpkt_types_peer_client_fail_conversion(3, 0);
    status = cpkt_opcua_client_addVariableNode_typed(
        client, requested, object, node(0, CPKT_OPCUA_NS0ID_HASCOMPONENT),
        name("conversion-error"),
        node(0, CPKT_OPCUA_NS0ID_BASEDATAVARIABLETYPE), va, &out);
    CHECK(cpkt_types_fail_stop() &&
          status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY &&
          cpkt_opcua_NodeId_isNull(&out));
    CHECK(
        !cpkt_opcua_client_readNodeIdAttribute_typed(client, requested, &out));
    CHECK(cpkt_opcua_NodeId_equal(&out, &requested));
    cpkt_opcua_NodeId_clear(&out);
    CHECK(!cpkt_opcua_client_deleteNode_typed(client, requested, 1));
    memset(&va.value, 0, sizeof(va.value));
    cpkt_opcua_VariableAttributes_clear(&va);
  }
#endif
  /* Delete children before their parent: native deletion removes owned
   * aggregate children together with the object. */
  for (i = 8; i > 0; --i)
    CHECK(!cpkt_opcua_client_deleteNode_typed(
        client, node(1, 6199 + (unsigned int)i), 1));
  /* The caller of this test owns the connected client and peer. */
}
