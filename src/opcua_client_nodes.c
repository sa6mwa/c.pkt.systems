/* Handwritten OPC UA facade: client nodes. */
#include "opcua_facade_internal.h"
#include <open62541/client_highlevel.h>
#include <open62541/nodeids.h>

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_add_object(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id, const char *browse_name,
    const char *display_name, cpkt_opcua_status *status_out) {
  UA_ObjectAttributes attr;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || browse_name == NULL ||
      !cpkt_valid_node_id(node_id) || !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  attr = UA_ObjectAttributes_default;
  attr.displayName = UA_LOCALIZEDTEXT(
      (char *)"en-US",
      (char *)(display_name != NULL ? display_name : browse_name));
  attr.writeMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION |
                   UA_WRITEMASK_EVENTNOTIFIER;
  attr.userWriteMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION |
                       UA_WRITEMASK_EVENTNOTIFIER;
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  status = UA_Client_addObjectNode(
      client->client, requested_node_id, parent_native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE), attr, NULL);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_add_variable(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    const char *browse_name, const char *display_name,
    const cpkt_opcua_value *value, cpkt_opcua_status *status_out) {
  return cpkt_opcua_client_add_variable_under(
      client, node_id, cpkt_opcua_node_id_numeric(0, UA_NS0ID_OBJECTSFOLDER),
      browse_name, display_name, value, status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_add_variable_under(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id, const char *browse_name,
    const char *display_name, const cpkt_opcua_value *value,
    cpkt_opcua_status *status_out) {
  UA_VariableAttributes attr;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || browse_name == NULL ||
      value == NULL || !cpkt_valid_node_id(node_id) ||
      !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  attr = UA_VariableAttributes_default;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.writeMask = UA_WRITEMASK_ACCESSLEVEL | UA_WRITEMASK_HISTORIZING |
                   UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION |
                   UA_WRITEMASK_VALUERANK | UA_WRITEMASK_ARRAYDIMENSIONS;
  attr.userWriteMask = UA_WRITEMASK_ACCESSLEVEL | UA_WRITEMASK_HISTORIZING |
                       UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION |
                       UA_WRITEMASK_VALUERANK | UA_WRITEMASK_ARRAYDIMENSIONS;
  attr.dataType = cpkt_data_type_node_id_for_value_type(value->type);
  attr.displayName = UA_LOCALIZEDTEXT(
      (char *)"en-US",
      (char *)(display_name != NULL ? display_name : browse_name));
  result = cpkt_apply_value_shape_to_variable_attributes(&attr, value);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_set_variant(&attr.value, value);
  if (result != CPKT_OPCUA_OK) {
    UA_Array_delete(attr.arrayDimensions, attr.arrayDimensionsSize,
                    &UA_TYPES[UA_TYPES_UINT32]);
    return result;
  }
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  status = UA_Client_addVariableNode(
      client->client, requested_node_id, parent_native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL);
  UA_Variant_clear(&attr.value);
  UA_Array_delete(attr.arrayDimensions, attr.arrayDimensionsSize,
                  &UA_TYPES[UA_TYPES_UINT32]);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_add_object_type(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id, const char *browse_name,
    const char *display_name, int is_abstract, cpkt_opcua_status *status_out) {
  UA_ObjectTypeAttributes attr;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || browse_name == NULL ||
      !cpkt_valid_node_id(node_id) || !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  attr = UA_ObjectTypeAttributes_default;
  attr.displayName = UA_LOCALIZEDTEXT(
      (char *)"en-US",
      (char *)(display_name != NULL ? display_name : browse_name));
  attr.writeMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION |
                   UA_WRITEMASK_ISABSTRACT;
  attr.userWriteMask = attr.writeMask;
  attr.isAbstract = is_abstract ? true : false;
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  status = UA_Client_addObjectTypeNode(
      client->client, requested_node_id, parent_native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      attr, NULL);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_add_variable_type(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id, const char *browse_name,
    const char *display_name, const cpkt_opcua_value *value, int is_abstract,
    cpkt_opcua_status *status_out) {
  UA_VariableTypeAttributes attr;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || browse_name == NULL ||
      value == NULL || !cpkt_valid_node_id(node_id) ||
      !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  attr = UA_VariableTypeAttributes_default;
  attr.displayName = UA_LOCALIZEDTEXT(
      (char *)"en-US",
      (char *)(display_name != NULL ? display_name : browse_name));
  attr.writeMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION |
                   UA_WRITEMASK_ISABSTRACT;
  attr.userWriteMask = attr.writeMask;
  attr.dataType = cpkt_data_type_node_id_for_value_type(value->type);
  attr.isAbstract = is_abstract ? true : false;
  result = cpkt_apply_value_shape_to_variable_type_attributes(&attr, value);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_set_variant(&attr.value, value);
  if (result != CPKT_OPCUA_OK) {
    UA_Array_delete(attr.arrayDimensions, attr.arrayDimensionsSize,
                    &UA_TYPES[UA_TYPES_UINT32]);
    return result;
  }
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  status = UA_Client_addVariableTypeNode(
      client->client, requested_node_id, parent_native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      attr, NULL);
  UA_Variant_clear(&attr.value);
  UA_Array_delete(attr.arrayDimensions, attr.arrayDimensionsSize,
                  &UA_TYPES[UA_TYPES_UINT32]);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_add_reference_type(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id, const char *browse_name,
    const char *display_name, const char *inverse_name, int is_abstract,
    int symmetric, cpkt_opcua_status *status_out) {
  UA_ReferenceTypeAttributes attr;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || browse_name == NULL ||
      inverse_name == NULL || !cpkt_valid_node_id(node_id) ||
      !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  attr = UA_ReferenceTypeAttributes_default;
  attr.displayName = UA_LOCALIZEDTEXT(
      (char *)"en-US",
      (char *)(display_name != NULL ? display_name : browse_name));
  attr.inverseName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)inverse_name);
  attr.writeMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION |
                   UA_WRITEMASK_INVERSENAME | UA_WRITEMASK_ISABSTRACT;
  attr.userWriteMask = attr.writeMask;
  attr.isAbstract = is_abstract ? true : false;
  attr.symmetric = symmetric ? true : false;
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  status = UA_Client_addReferenceTypeNode(
      client->client, requested_node_id, parent_native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      attr, NULL);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_add_data_type(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id, const char *browse_name,
    const char *display_name, int is_abstract, cpkt_opcua_status *status_out) {
  UA_DataTypeAttributes attr;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || browse_name == NULL ||
      !cpkt_valid_node_id(node_id) || !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  attr = UA_DataTypeAttributes_default;
  attr.displayName = UA_LOCALIZEDTEXT(
      (char *)"en-US",
      (char *)(display_name != NULL ? display_name : browse_name));
  attr.writeMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION |
                   UA_WRITEMASK_ISABSTRACT;
  attr.userWriteMask = attr.writeMask;
  attr.isAbstract = is_abstract ? true : false;
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  status = UA_Client_addDataTypeNode(
      client->client, requested_node_id, parent_native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      attr, NULL);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_add_view(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id, const char *browse_name,
    const char *display_name, int contains_no_loops,
    unsigned long event_notifier, cpkt_opcua_status *status_out) {
  UA_ViewAttributes attr;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || browse_name == NULL ||
      !cpkt_valid_node_id(node_id) || !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (event_notifier > (unsigned long)UCHAR_MAX) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  attr = UA_ViewAttributes_default;
  attr.displayName = UA_LOCALIZEDTEXT(
      (char *)"en-US",
      (char *)(display_name != NULL ? display_name : browse_name));
  attr.writeMask = UA_WRITEMASK_DISPLAYNAME | UA_WRITEMASK_DESCRIPTION |
                   UA_WRITEMASK_EVENTNOTIFIER;
  attr.userWriteMask = attr.writeMask;
  attr.containsNoLoops = contains_no_loops ? true : false;
  attr.eventNotifier = (UA_Byte)event_notifier;
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  status = UA_Client_addViewNode(
      client->client, requested_node_id, parent_native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      attr, NULL);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_delete_node(cpkt_opcua_client *client,
                                                cpkt_opcua_node_id node_id,
                                                int delete_target_refs,
                                                cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_deleteNode(client->client, native_node_id,
                                delete_target_refs ? true : false);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_add_reference(
    cpkt_opcua_client *client, cpkt_opcua_node_id source_node_id,
    cpkt_opcua_node_id reference_type_id, int is_forward,
    cpkt_opcua_node_id target_node_id, unsigned long target_node_class,
    cpkt_opcua_status *status_out) {
  return cpkt_opcua_client_add_reference_ex(
      client, source_node_id, reference_type_id, is_forward,
      cpkt_opcua_expanded_node_id_local(target_node_id), target_node_class,
      status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_add_reference_ex(
    cpkt_opcua_client *client, cpkt_opcua_node_id source_node_id,
    cpkt_opcua_node_id reference_type_id, int is_forward,
    cpkt_opcua_expanded_node_id target_node_id, unsigned long target_node_class,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_source_node_id;
  UA_NodeId native_reference_type_id;
  UA_ExpandedNodeId native_target_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      !cpkt_valid_node_id(source_node_id) ||
      !cpkt_valid_node_id(reference_type_id) ||
      !cpkt_valid_expanded_node_id(target_node_id) ||
      !cpkt_valid_node_class(target_node_class)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_source_node_id = cpkt_make_node_id(source_node_id);
  native_reference_type_id = cpkt_make_node_id(reference_type_id);
  native_target_node_id = cpkt_make_expanded_node_id(target_node_id);
  status = UA_Client_addReference(
      client->client, native_source_node_id, native_reference_type_id,
      is_forward ? true : false, UA_STRING_NULL, native_target_node_id,
      (UA_NodeClass)target_node_class);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_delete_reference(
    cpkt_opcua_client *client, cpkt_opcua_node_id source_node_id,
    cpkt_opcua_node_id reference_type_id, int is_forward,
    cpkt_opcua_node_id target_node_id, int delete_bidirectional,
    cpkt_opcua_status *status_out) {
  return cpkt_opcua_client_delete_reference_ex(
      client, source_node_id, reference_type_id, is_forward,
      cpkt_opcua_expanded_node_id_local(target_node_id), delete_bidirectional,
      status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_delete_reference_ex(
    cpkt_opcua_client *client, cpkt_opcua_node_id source_node_id,
    cpkt_opcua_node_id reference_type_id, int is_forward,
    cpkt_opcua_expanded_node_id target_node_id, int delete_bidirectional,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_source_node_id;
  UA_NodeId native_reference_type_id;
  UA_ExpandedNodeId native_target_node_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      !cpkt_valid_node_id(source_node_id) ||
      !cpkt_valid_node_id(reference_type_id) ||
      !cpkt_valid_expanded_node_id(target_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_source_node_id = cpkt_make_node_id(source_node_id);
  native_reference_type_id = cpkt_make_node_id(reference_type_id);
  native_target_node_id = cpkt_make_expanded_node_id(target_node_id);
  status = UA_Client_deleteReference(
      client->client, native_source_node_id, native_reference_type_id,
      is_forward ? true : false, native_target_node_id,
      delete_bidirectional ? true : false);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}
