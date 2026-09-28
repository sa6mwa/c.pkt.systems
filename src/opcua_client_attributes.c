/* Handwritten OPC UA facade: client attributes. */
#include "opcua_facade_internal.h"
#include <open62541/client_highlevel.h>
#include <stdlib.h>

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_node_id(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id *node_id_out, char *identifier_buffer,
    size_t identifier_buffer_size, size_t *required_identifier_size_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_NodeId native_result;
  UA_StatusCode status;
  cpkt_opcua_node_id facade_result;
  struct cpkt_owned_node_id_memory owned;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_identifier_size_out != NULL) {
    *required_identifier_size_out = 0;
  }
  if (node_id_out != NULL) {
    *node_id_out = cpkt_opcua_node_id_null();
  }
  if (client == NULL || client->client == NULL || node_id_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_NodeId_init(&native_result);
  status = UA_Client_readNodeIdAttribute(client->client, native_node_id,
                                         &native_result);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_NodeId_clear(&native_result);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  owned.string = NULL;
  owned.byte_string = NULL;
  if (!cpkt_native_node_id_to_facade(&native_result, &facade_result, &owned)) {
    cpkt_owned_node_id_memory_clear(&owned);
    UA_NodeId_clear(&native_result);
    return CPKT_OPCUA_ERR_TYPE;
  }
  result = cpkt_copy_node_id_to_caller(
      &facade_result, &owned, node_id_out, identifier_buffer,
      identifier_buffer_size, required_identifier_size_out);
  if (result != CPKT_OPCUA_OK) {
    *node_id_out = cpkt_opcua_node_id_null();
  }
  cpkt_owned_node_id_memory_clear(&owned);
  UA_NodeId_clear(&native_result);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_node_class(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    unsigned long *node_class_out, cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_NodeClass node_class;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (node_class_out != NULL) {
    *node_class_out = 0;
  }
  if (client == NULL || client->client == NULL || node_class_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readNodeClassAttribute(client->client, native_node_id,
                                            &node_class);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *node_class_out = (unsigned long)node_class;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_browse_name(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    unsigned short *namespace_index_out, char *buffer, size_t buffer_size,
    size_t *required_size_out, cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_QualifiedName browse_name;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (namespace_index_out != NULL) {
    *namespace_index_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_QualifiedName_init(&browse_name);
  status = UA_Client_readBrowseNameAttribute(client->client, native_node_id,
                                             &browse_name);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_QualifiedName_clear(&browse_name);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (namespace_index_out != NULL) {
    *namespace_index_out = (unsigned short)browse_name.namespaceIndex;
  }
  result = cpkt_copy_ua_string_to_buffer(&browse_name.name, buffer, buffer_size,
                                         required_size_out);
  UA_QualifiedName_clear(&browse_name);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_display_name(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id, char *buffer,
    size_t buffer_size, size_t *required_size_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_LocalizedText display_name;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_LocalizedText_init(&display_name);
  status = UA_Client_readDisplayNameAttribute(client->client, native_node_id,
                                              &display_name);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_LocalizedText_clear(&display_name);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_ua_string_to_buffer(&display_name.text, buffer,
                                         buffer_size, required_size_out);
  UA_LocalizedText_clear(&display_name);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_description(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id, char *buffer,
    size_t buffer_size, size_t *required_size_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_LocalizedText description;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_LocalizedText_init(&description);
  status = UA_Client_readDescriptionAttribute(client->client, native_node_id,
                                              &description);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_LocalizedText_clear(&description);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_ua_string_to_buffer(&description.text, buffer, buffer_size,
                                         required_size_out);
  UA_LocalizedText_clear(&description);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_display_name(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    const char *display_name, cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_LocalizedText native_display_name;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || display_name == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_display_name = UA_LOCALIZEDTEXT((char *)"en-US", (char *)display_name);
  status = UA_Client_writeDisplayNameAttribute(client->client, native_node_id,
                                               &native_display_name);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_description(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    const char *description, cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_LocalizedText native_description;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || description == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_description = UA_LOCALIZEDTEXT((char *)"en-US", (char *)description);
  status = UA_Client_writeDescriptionAttribute(client->client, native_node_id,
                                               &native_description);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_write_mask(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    unsigned long *write_mask_out, cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_UInt32 write_mask;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (write_mask_out != NULL) {
    *write_mask_out = 0;
  }
  if (client == NULL || client->client == NULL || write_mask_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readWriteMaskAttribute(client->client, native_node_id,
                                            &write_mask);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *write_mask_out = (unsigned long)write_mask;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_user_write_mask(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    unsigned long *write_mask_out, cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_UInt32 write_mask;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (write_mask_out != NULL) {
    *write_mask_out = 0;
  }
  if (client == NULL || client->client == NULL || write_mask_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readUserWriteMaskAttribute(client->client, native_node_id,
                                                &write_mask);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *write_mask_out = (unsigned long)write_mask;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_write_mask(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    unsigned long write_mask, cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_UInt32 native_write_mask;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!cpkt_ulong_fits_uint32(write_mask)) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_write_mask = (UA_UInt32)write_mask;
  status = UA_Client_writeWriteMaskAttribute(client->client, native_node_id,
                                             &native_write_mask);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_is_abstract(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id, int *is_abstract_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean is_abstract;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (is_abstract_out != NULL) {
    *is_abstract_out = 0;
  }
  if (client == NULL || client->client == NULL || is_abstract_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readIsAbstractAttribute(client->client, native_node_id,
                                             &is_abstract);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *is_abstract_out = is_abstract ? 1 : 0;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result
cpkt_opcua_client_write_is_abstract(cpkt_opcua_client *client,
                                    cpkt_opcua_node_id node_id, int is_abstract,
                                    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean native_is_abstract;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_is_abstract = is_abstract ? true : false;
  status = UA_Client_writeIsAbstractAttribute(client->client, native_node_id,
                                              &native_is_abstract);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result
cpkt_opcua_client_read_symmetric(cpkt_opcua_client *client,
                                 cpkt_opcua_node_id node_id, int *symmetric_out,
                                 cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean symmetric;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (symmetric_out != NULL) {
    *symmetric_out = 0;
  }
  if (client == NULL || client->client == NULL || symmetric_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readSymmetricAttribute(client->client, native_node_id,
                                            &symmetric);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *symmetric_out = symmetric ? 1 : 0;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result
cpkt_opcua_client_write_symmetric(cpkt_opcua_client *client,
                                  cpkt_opcua_node_id node_id, int symmetric,
                                  cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean native_symmetric;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_symmetric = symmetric ? true : false;
  status = UA_Client_writeSymmetricAttribute(client->client, native_node_id,
                                             &native_symmetric);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_inverse_name(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id, char *buffer,
    size_t buffer_size, size_t *required_size_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_LocalizedText inverse_name;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_LocalizedText_init(&inverse_name);
  status = UA_Client_readInverseNameAttribute(client->client, native_node_id,
                                              &inverse_name);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_LocalizedText_clear(&inverse_name);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_ua_string_to_buffer(&inverse_name.text, buffer,
                                         buffer_size, required_size_out);
  UA_LocalizedText_clear(&inverse_name);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_inverse_name(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    const char *inverse_name, cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_LocalizedText native_inverse_name;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || inverse_name == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_inverse_name = UA_LOCALIZEDTEXT((char *)"en-US", (char *)inverse_name);
  status = UA_Client_writeInverseNameAttribute(client->client, native_node_id,
                                               &native_inverse_name);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_contains_no_loops(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    int *contains_no_loops_out, cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean contains_no_loops;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (contains_no_loops_out != NULL) {
    *contains_no_loops_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      contains_no_loops_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readContainsNoLoopsAttribute(
      client->client, native_node_id, &contains_no_loops);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *contains_no_loops_out = contains_no_loops ? 1 : 0;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_contains_no_loops(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    int contains_no_loops, cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean native_contains_no_loops;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_contains_no_loops = contains_no_loops ? true : false;
  status = UA_Client_writeContainsNoLoopsAttribute(
      client->client, native_node_id, &native_contains_no_loops);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_event_notifier(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    unsigned long *event_notifier_out, cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Byte event_notifier;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (event_notifier_out != NULL) {
    *event_notifier_out = 0;
  }
  if (client == NULL || client->client == NULL || event_notifier_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readEventNotifierAttribute(client->client, native_node_id,
                                                &event_notifier);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *event_notifier_out = (unsigned long)event_notifier;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_event_notifier(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    unsigned long event_notifier, cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Byte native_event_notifier;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (event_notifier > (unsigned long)UCHAR_MAX) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_event_notifier = (UA_Byte)event_notifier;
  status = UA_Client_writeEventNotifierAttribute(client->client, native_node_id,
                                                 &native_event_notifier);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_data_type(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id *data_type_out, cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_NodeId native_data_type;
  UA_StatusCode status;
  struct cpkt_owned_node_id_memory data_type_memory;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (data_type_out != NULL) {
    *data_type_out = cpkt_opcua_node_id_numeric(0, 0);
  }
  if (client == NULL || client->client == NULL || data_type_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  UA_NodeId_init(&native_data_type);
  status = UA_Client_readDataTypeAttribute(client->client, native_node_id,
                                           &native_data_type);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_NodeId_clear(&native_data_type);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  data_type_memory.string = NULL;
  data_type_memory.byte_string = NULL;
  if (!cpkt_native_node_id_to_facade(&native_data_type, data_type_out,
                                     &data_type_memory)) {
    UA_NodeId_clear(&native_data_type);
    cpkt_owned_node_id_memory_clear(&data_type_memory);
    return CPKT_OPCUA_ERR_TYPE;
  }
  if (data_type_memory.string != NULL || data_type_memory.byte_string != NULL) {
    UA_NodeId_clear(&native_data_type);
    cpkt_owned_node_id_memory_clear(&data_type_memory);
    *data_type_out = cpkt_opcua_node_id_numeric(0, 0);
    return CPKT_OPCUA_ERR_TYPE;
  }
  cpkt_owned_node_id_memory_clear(&data_type_memory);
  UA_NodeId_clear(&native_data_type);
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_data_type(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id data_type, cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_NodeId native_data_type;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      !cpkt_valid_node_id(node_id) || !cpkt_valid_node_id(data_type)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_data_type = cpkt_make_node_id(data_type);
  status = UA_Client_writeDataTypeAttribute(client->client, native_node_id,
                                            &native_data_type);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_value_rank(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id, long *value_rank_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Int32 value_rank;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (value_rank_out != NULL) {
    *value_rank_out = 0;
  }
  if (client == NULL || client->client == NULL || value_rank_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readValueRankAttribute(client->client, native_node_id,
                                            &value_rank);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *value_rank_out = (long)value_rank;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result
cpkt_opcua_client_write_value_rank(cpkt_opcua_client *client,
                                   cpkt_opcua_node_id node_id, long value_rank,
                                   cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Int32 native_value_rank;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!cpkt_long_fits_int32(value_rank)) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_value_rank = (UA_Int32)value_rank;
  status = UA_Client_writeValueRankAttribute(client->client, native_node_id,
                                             &native_value_rank);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_array_dimensions(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    unsigned long *dimensions, size_t dimension_count,
    size_t *required_dimension_count_out, cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_UInt32 *native_dimensions;
  size_t native_dimension_count;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_dimension_count_out != NULL) {
    *required_dimension_count_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      required_dimension_count_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_dimensions = NULL;
  native_dimension_count = 0;
  status = UA_Client_readArrayDimensionsAttribute(
      client->client, native_node_id, &native_dimension_count,
      &native_dimensions);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_Array_delete(native_dimensions, native_dimension_count,
                    &UA_TYPES[UA_TYPES_UINT32]);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_array_dimensions_to_buffer(
      native_dimensions, native_dimension_count, dimensions, dimension_count,
      required_dimension_count_out);
  UA_Array_delete(native_dimensions, native_dimension_count,
                  &UA_TYPES[UA_TYPES_UINT32]);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_array_dimensions(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    const unsigned long *dimensions, size_t dimension_count,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_UInt32 *native_dimensions;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_make_array_dimensions(dimensions, dimension_count,
                                      &native_dimensions);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_writeArrayDimensionsAttribute(
      client->client, native_node_id, dimension_count, native_dimensions);
  free(native_dimensions);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_access_level(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    unsigned long *access_level_out, cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Byte access_level;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (access_level_out != NULL) {
    *access_level_out = 0;
  }
  if (client == NULL || client->client == NULL || access_level_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readAccessLevelAttribute(client->client, native_node_id,
                                              &access_level);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *access_level_out = (unsigned long)access_level;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_user_access_level(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    unsigned long *access_level_out, cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Byte access_level;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (access_level_out != NULL) {
    *access_level_out = 0;
  }
  if (client == NULL || client->client == NULL || access_level_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readUserAccessLevelAttribute(
      client->client, native_node_id, &access_level);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *access_level_out = (unsigned long)access_level;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_access_level(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    unsigned long access_level, cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Byte native_access_level;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (access_level > (unsigned long)UCHAR_MAX) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_access_level = (UA_Byte)access_level;
  status = UA_Client_writeAccessLevelAttribute(client->client, native_node_id,
                                               &native_access_level);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_access_level_ex(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    unsigned long *access_level_out, cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_UInt32 access_level;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (access_level_out != NULL) {
    *access_level_out = 0;
  }
  if (client == NULL || client->client == NULL || access_level_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readAccessLevelExAttribute(client->client, native_node_id,
                                                &access_level);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *access_level_out = (unsigned long)access_level;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_access_level_ex(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    unsigned long access_level, cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_UInt32 native_access_level;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!cpkt_ulong_fits_uint32(access_level)) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_access_level = (UA_UInt32)access_level;
  status = UA_Client_writeAccessLevelExAttribute(client->client, native_node_id,
                                                 &native_access_level);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_minimum_sampling_interval(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    double *minimum_sampling_interval_out, cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Double minimum_sampling_interval;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (minimum_sampling_interval_out != NULL) {
    *minimum_sampling_interval_out = 0.0;
  }
  if (client == NULL || client->client == NULL ||
      minimum_sampling_interval_out == NULL || !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readMinimumSamplingIntervalAttribute(
      client->client, native_node_id, &minimum_sampling_interval);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *minimum_sampling_interval_out = (double)minimum_sampling_interval;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_minimum_sampling_interval(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    double minimum_sampling_interval, cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Double native_minimum_sampling_interval;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_minimum_sampling_interval = (UA_Double)minimum_sampling_interval;
  status = UA_Client_writeMinimumSamplingIntervalAttribute(
      client->client, native_node_id, &native_minimum_sampling_interval);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_historizing(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id, int *historizing_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean historizing;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (historizing_out != NULL) {
    *historizing_out = 0;
  }
  if (client == NULL || client->client == NULL || historizing_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readHistorizingAttribute(client->client, native_node_id,
                                              &historizing);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *historizing_out = historizing ? 1 : 0;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result
cpkt_opcua_client_write_historizing(cpkt_opcua_client *client,
                                    cpkt_opcua_node_id node_id, int historizing,
                                    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean native_historizing;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_historizing = historizing ? true : false;
  status = UA_Client_writeHistorizingAttribute(client->client, native_node_id,
                                               &native_historizing);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_executable(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id, int *executable_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean executable;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (executable_out != NULL) {
    *executable_out = 0;
  }
  if (client == NULL || client->client == NULL || executable_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readExecutableAttribute(client->client, native_node_id,
                                             &executable);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *executable_out = executable ? 1 : 0;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_user_executable(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id, int *executable_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean executable;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (executable_out != NULL) {
    *executable_out = 0;
  }
  if (client == NULL || client->client == NULL || executable_out == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  status = UA_Client_readUserExecutableAttribute(client->client, native_node_id,
                                                 &executable);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *executable_out = executable ? 1 : 0;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result
cpkt_opcua_client_write_executable(cpkt_opcua_client *client,
                                   cpkt_opcua_node_id node_id, int executable,
                                   cpkt_opcua_status *status_out) {
  UA_NodeId native_node_id;
  UA_Boolean native_executable;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_node_id = cpkt_make_node_id(node_id);
  native_executable = executable ? true : false;
  status = UA_Client_writeExecutableAttribute(client->client, native_node_id,
                                              &native_executable);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}
