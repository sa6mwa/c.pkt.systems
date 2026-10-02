/* Handwritten OPC UA facade: client async. */
#include "opcua_facade_internal.h"
#include <open62541/client_highlevel_async.h>
#include <open62541/nodeids.h>
#include <stdlib.h>

static void cpkt_async_link(cpkt_opcua_client *client,
                            struct cpkt_opcua_async_context *context) {
  context->owner = client;
  context->next = client->asyncs;
  client->asyncs = context;
}

static void cpkt_async_unlink(struct cpkt_opcua_async_context *context) {
  cpkt_opcua_client *client;
  struct cpkt_opcua_async_context **slot;

  if (context == NULL || context->owner == NULL) {
    return;
  }
  client = context->owner;
  slot = &client->asyncs;
  while (*slot != NULL) {
    if (*slot == context) {
      *slot = context->next;
      context->next = NULL;
      context->owner = NULL;
      return;
    }
    slot = &(*slot)->next;
  }
  context->owner = NULL;
}

static void cpkt_async_finish(struct cpkt_opcua_async_context *context) {
  if (context == NULL) {
    return;
  }
  cpkt_async_unlink(context);
  free(context);
}

static void cpkt_async_read_value_callback(UA_Client *native_client,
                                           void *userdata, UA_UInt32 request_id,
                                           UA_StatusCode status,
                                           UA_DataValue *value) {
  struct cpkt_opcua_async_context *context;
  cpkt_opcua_value out;
  cpkt_opcua_result result;

  (void)native_client;
  context = (struct cpkt_opcua_async_context *)userdata;
  cpkt_opcua_value_clear(&out);
  result =
      status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
  if (result == CPKT_OPCUA_OK) {
    if (value == NULL || !value->hasValue) {
      result = CPKT_OPCUA_ERR_TYPE;
    } else {
      result = cpkt_get_variant(&value->value, &out, context->string_buffer,
                                context->string_buffer_size,
                                context->required_string_size_out);
    }
  }
  context->value_fn((unsigned long)request_id, result,
                    result == CPKT_OPCUA_OK ? &out : NULL, cpkt_status(status),
                    context->user);
  cpkt_async_finish(context);
}

static void cpkt_async_write_callback(UA_Client *native_client, void *userdata,
                                      UA_UInt32 request_id,
                                      UA_WriteResponse *response) {
  struct cpkt_opcua_async_context *context;
  UA_StatusCode status;
  cpkt_opcua_result result;

  (void)native_client;
  context = (struct cpkt_opcua_async_context *)userdata;
  status = response != NULL ? response->responseHeader.serviceResult
                            : UA_STATUSCODE_BADUNEXPECTEDERROR;
  if (status == UA_STATUSCODE_GOOD && response != NULL &&
      response->resultsSize != 0) {
    status = response->results[0];
  }
  result =
      status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
  context->status_fn((unsigned long)request_id, result, cpkt_status(status),
                     context->user);
  cpkt_async_finish(context);
}

static void cpkt_async_browse_callback(UA_Client *native_client, void *userdata,
                                       UA_UInt32 request_id,
                                       UA_BrowseResponse *response) {
  struct cpkt_opcua_async_context *context;
  UA_StatusCode status;
  cpkt_opcua_status browse_status;
  cpkt_opcua_result result;

  (void)native_client;
  context = (struct cpkt_opcua_async_context *)userdata;
  status = response != NULL ? response->responseHeader.serviceResult
                            : UA_STATUSCODE_BADUNEXPECTEDERROR;
  result =
      status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
  if (result == CPKT_OPCUA_OK) {
    if (response == NULL || response->resultsSize != 1) {
      result = CPKT_OPCUA_ERR_TYPE;
    } else {
      browse_status = 0;
      result =
          cpkt_browse_result_each(&response->results[0], context->browse_fn,
                                  context->user, &browse_status);
      status = (UA_StatusCode)browse_status;
    }
  }
  context->browse_done_fn((unsigned long)request_id, result,
                          cpkt_status(status), context->user);
  cpkt_async_finish(context);
}

static void cpkt_async_call_callback(UA_Client *native_client, void *userdata,
                                     UA_UInt32 request_id,
                                     UA_CallResponse *response) {
  struct cpkt_opcua_async_context *context;
  UA_StatusCode status;
  cpkt_opcua_result result;
  cpkt_opcua_result output_result;
  UA_CallMethodResult *method_result;
  char *string_buffer;
  size_t string_buffer_size;
  size_t i;

  (void)native_client;
  context = (struct cpkt_opcua_async_context *)userdata;
  status = response != NULL ? response->responseHeader.serviceResult
                            : UA_STATUSCODE_BADUNEXPECTEDERROR;
  result =
      status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
  if (result == CPKT_OPCUA_OK) {
    if (response == NULL || response->resultsSize != 1 ||
        response->results == NULL) {
      result = CPKT_OPCUA_ERR_TYPE;
    } else {
      method_result = &response->results[0];
      status = method_result->statusCode;
      result = status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK
                                            : CPKT_OPCUA_ERR_UPSTREAM;
      if (result == CPKT_OPCUA_OK) {
        if (method_result->outputArgumentsSize !=
                context->expected_output_count ||
            (context->expected_output_count != 0 &&
             method_result->outputArguments == NULL)) {
          result = CPKT_OPCUA_ERR_TYPE;
        } else {
          for (i = 0; i < context->expected_output_count; ++i) {
            string_buffer = context->string_buffers != NULL
                                ? context->string_buffers[i]
                                : NULL;
            string_buffer_size = context->string_buffer_sizes != NULL
                                     ? context->string_buffer_sizes[i]
                                     : 0;
            output_result = cpkt_get_variant(
                &method_result->outputArguments[i], &context->outputs[i],
                string_buffer, string_buffer_size,
                context->required_string_sizes_out != NULL
                    ? &context->required_string_sizes_out[i]
                    : NULL);
            if (output_result != CPKT_OPCUA_OK && result == CPKT_OPCUA_OK) {
              result = output_result;
            }
          }
        }
      }
    }
  }
  context->call_fn((unsigned long)request_id, result,
                   result == CPKT_OPCUA_OK ? context->outputs : NULL,
                   result == CPKT_OPCUA_OK ? context->expected_output_count : 0,
                   cpkt_status(status), context->user);
  cpkt_async_finish(context);
}

static void cpkt_async_add_node_callback(UA_Client *native_client,
                                         void *userdata, UA_UInt32 request_id,
                                         UA_AddNodesResponse *response) {
  struct cpkt_opcua_async_context *context;
  struct cpkt_owned_node_id_memory owned;
  cpkt_opcua_node_id node_id;
  UA_StatusCode status;
  cpkt_opcua_result result;

  (void)native_client;
  context = (struct cpkt_opcua_async_context *)userdata;
  status = response != NULL ? response->responseHeader.serviceResult
                            : UA_STATUSCODE_BADUNEXPECTEDERROR;
  result =
      status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
  if (result == CPKT_OPCUA_OK) {
    if (response == NULL || response->resultsSize != 1 ||
        response->results == NULL) {
      result = CPKT_OPCUA_ERR_TYPE;
    } else {
      status = response->results[0].statusCode;
      result = status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK
                                            : CPKT_OPCUA_ERR_UPSTREAM;
      if (result == CPKT_OPCUA_OK) {
        if (!cpkt_native_node_id_to_facade(&response->results[0].addedNodeId,
                                           &node_id, &owned)) {
          result = CPKT_OPCUA_ERR_ALLOC;
        } else {
          result = cpkt_copy_node_id_to_caller(
              &node_id, &owned, context->node_id_out, context->node_id_buffer,
              context->node_id_buffer_size, context->required_node_id_size_out);
          cpkt_owned_node_id_memory_clear(&owned);
        }
      }
    }
  }
  context->node_fn((unsigned long)request_id, result,
                   result == CPKT_OPCUA_OK ? context->node_id_out : NULL,
                   cpkt_status(status), context->user);
  cpkt_async_finish(context);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_async(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_async_value_fn fn, void *user,
    cpkt_opcua_request_id *request_id_out, char *string_buffer,
    size_t string_buffer_size, size_t *required_string_size_out,
    cpkt_opcua_status *status_out) {
  struct cpkt_opcua_async_context *context;
  UA_NodeId native_node_id;
  UA_UInt32 request_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (request_id_out != NULL) {
    *request_id_out = 0;
  }
  if (required_string_size_out != NULL) {
    *required_string_size_out = 0;
  }
  if (client == NULL || client->client == NULL || fn == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  context = (struct cpkt_opcua_async_context *)calloc(1, sizeof(*context));
  if (context == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->value_fn = fn;
  context->user = user;
  context->string_buffer = string_buffer;
  context->string_buffer_size = string_buffer_size;
  context->required_string_size_out = required_string_size_out;
  cpkt_async_link(client, context);
  native_node_id = cpkt_make_node_id(node_id);
  request_id = 0;
  status = UA_Client_readValueAttribute_async(client->client, native_node_id,
                                              cpkt_async_read_value_callback,
                                              context, &request_id);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    cpkt_async_finish(context);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (request_id_out != NULL) {
    *request_id_out = (cpkt_opcua_request_id)request_id;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_write_async(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    const cpkt_opcua_value *value, cpkt_opcua_async_status_fn fn, void *user,
    cpkt_opcua_request_id *request_id_out, cpkt_opcua_status *status_out) {
  struct cpkt_opcua_async_context *context;
  UA_NodeId native_node_id;
  UA_Variant variant;
  UA_UInt32 request_id;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (request_id_out != NULL) {
    *request_id_out = 0;
  }
  if (client == NULL || client->client == NULL || value == NULL || fn == NULL ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  UA_Variant_init(&variant);
  result = cpkt_set_variant(&variant, value);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  context = (struct cpkt_opcua_async_context *)calloc(1, sizeof(*context));
  if (context == NULL) {
    UA_Variant_clear(&variant);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->status_fn = fn;
  context->user = user;
  cpkt_async_link(client, context);
  native_node_id = cpkt_make_node_id(node_id);
  request_id = 0;
  status = UA_Client_writeValueAttribute_async(
      client->client, native_node_id, &variant, cpkt_async_write_callback,
      context, &request_id);
  UA_Variant_clear(&variant);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    cpkt_async_finish(context);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (request_id_out != NULL) {
    *request_id_out = (cpkt_opcua_request_id)request_id;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_browse_children_async(
    cpkt_opcua_client *client, cpkt_opcua_node_id parent_node_id,
    const cpkt_opcua_browse_options *options, cpkt_opcua_browse_fn browse_fn,
    cpkt_opcua_async_browse_fn done_fn, void *user,
    cpkt_opcua_request_id *request_id_out, cpkt_opcua_status *status_out) {
  struct cpkt_opcua_async_context *context;
  UA_BrowseRequest request;
  UA_UInt32 request_id;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (request_id_out != NULL) {
    *request_id_out = 0;
  }
  if (client == NULL || client->client == NULL || browse_fn == NULL ||
      done_fn == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  UA_BrowseRequest_init(&request);
  request.nodesToBrowse =
      (UA_BrowseDescription *)UA_calloc(1, sizeof(*request.nodesToBrowse));
  if (request.nodesToBrowse == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  request.nodesToBrowseSize = 1;
  result = cpkt_fill_browse_description(&request.nodesToBrowse[0],
                                        parent_node_id, options);
  if (result != CPKT_OPCUA_OK) {
    UA_BrowseRequest_clear(&request);
    return result;
  }
  request.requestedMaxReferencesPerNode =
      options != NULL ? (UA_UInt32)options->max_references : (UA_UInt32)0;
  context = (struct cpkt_opcua_async_context *)calloc(1, sizeof(*context));
  if (context == NULL) {
    UA_BrowseRequest_clear(&request);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->browse_fn = browse_fn;
  context->browse_done_fn = done_fn;
  context->user = user;
  cpkt_async_link(client, context);
  request_id = 0;
  status = UA_Client_sendAsyncBrowseRequest(client->client, &request,
                                            cpkt_async_browse_callback, context,
                                            &request_id);
  UA_BrowseRequest_clear(&request);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    cpkt_async_finish(context);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (request_id_out != NULL) {
    *request_id_out = (cpkt_opcua_request_id)request_id;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_call_method_async(
    cpkt_opcua_client *client, cpkt_opcua_node_id object_node_id,
    cpkt_opcua_node_id method_node_id, const cpkt_opcua_value *inputs,
    size_t input_count, size_t expected_output_count,
    cpkt_opcua_async_call_fn fn, void *user,
    cpkt_opcua_request_id *request_id_out, cpkt_opcua_value *outputs,
    char **string_buffers, const size_t *string_buffer_sizes,
    size_t *required_string_sizes_out, cpkt_opcua_status *status_out) {
  struct cpkt_opcua_async_context *context;
  UA_NodeId native_object_node_id;
  UA_NodeId native_method_node_id;
  UA_Variant *input_variants;
  UA_UInt32 request_id;
  UA_StatusCode status;
  cpkt_opcua_result result;
  size_t i;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (request_id_out != NULL) {
    *request_id_out = 0;
  }
  for (i = 0; required_string_sizes_out != NULL && i < expected_output_count;
       ++i) {
    required_string_sizes_out[i] = 0;
  }
  if (client == NULL || client->client == NULL || fn == NULL ||
      (expected_output_count != 0 && outputs == NULL) ||
      (input_count != 0 && inputs == NULL) ||
      !cpkt_valid_node_id(object_node_id) ||
      !cpkt_valid_node_id(method_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  input_variants = NULL;
  if (input_count != 0) {
    input_variants = (UA_Variant *)calloc(input_count, sizeof(*input_variants));
    if (input_variants == NULL) {
      return CPKT_OPCUA_ERR_ALLOC;
    }
    result = cpkt_set_variant_array(input_variants, inputs, input_count);
    if (result != CPKT_OPCUA_OK) {
      free(input_variants);
      return result;
    }
  }
  context = (struct cpkt_opcua_async_context *)calloc(1, sizeof(*context));
  if (context == NULL) {
    for (i = 0; i < input_count; ++i) {
      UA_Variant_clear(&input_variants[i]);
    }
    free(input_variants);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->call_fn = fn;
  context->user = user;
  context->outputs = outputs;
  context->expected_output_count = expected_output_count;
  context->string_buffers = string_buffers;
  context->string_buffer_sizes = string_buffer_sizes;
  context->required_string_sizes_out = required_string_sizes_out;
  cpkt_async_link(client, context);
  native_object_node_id = cpkt_make_node_id(object_node_id);
  native_method_node_id = cpkt_make_node_id(method_node_id);
  request_id = 0;
  status = UA_Client_call_async(
      client->client, native_object_node_id, native_method_node_id, input_count,
      input_variants, cpkt_async_call_callback, context, &request_id);
  for (i = 0; i < input_count; ++i) {
    UA_Variant_clear(&input_variants[i]);
  }
  free(input_variants);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    cpkt_async_finish(context);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (request_id_out != NULL) {
    *request_id_out = (cpkt_opcua_request_id)request_id;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_add_object_async(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id, const char *browse_name,
    const char *display_name, cpkt_opcua_async_node_fn fn, void *user,
    cpkt_opcua_request_id *request_id_out, cpkt_opcua_node_id *node_id_out,
    char *node_id_buffer, size_t node_id_buffer_size,
    size_t *required_node_id_size_out, cpkt_opcua_status *status_out) {
  struct cpkt_opcua_async_context *context;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_NodeId reference_type_id;
  UA_NodeId type_definition;
  UA_ObjectAttributes attr;
  UA_UInt32 request_id;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (request_id_out != NULL) {
    *request_id_out = 0;
  }
  if (required_node_id_size_out != NULL) {
    *required_node_id_size_out = 0;
  }
  if (client == NULL || client->client == NULL || browse_name == NULL ||
      fn == NULL || node_id_out == NULL || !cpkt_valid_node_id(node_id) ||
      !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  context = (struct cpkt_opcua_async_context *)calloc(1, sizeof(*context));
  if (context == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->node_fn = fn;
  context->user = user;
  context->node_id_out = node_id_out;
  context->node_id_buffer = node_id_buffer;
  context->node_id_buffer_size = node_id_buffer_size;
  context->required_node_id_size_out = required_node_id_size_out;
  cpkt_async_link(client, context);
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  reference_type_id = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
  type_definition = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE);
  attr = UA_ObjectAttributes_default;
  attr.displayName = UA_LOCALIZEDTEXT(
      (char *)"en-US",
      (char *)(display_name != NULL ? display_name : browse_name));
  request_id = 0;
  status = UA_Client_addObjectNode_async(
      client->client, requested_node_id, parent_native_node_id,
      reference_type_id,
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      type_definition, attr, NULL, cpkt_async_add_node_callback, context,
      &request_id);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    cpkt_async_finish(context);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (request_id_out != NULL) {
    *request_id_out = (cpkt_opcua_request_id)request_id;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_add_variable_async(
    cpkt_opcua_client *client, cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id, const char *browse_name,
    const char *display_name, const cpkt_opcua_value *value,
    cpkt_opcua_async_node_fn fn, void *user,
    cpkt_opcua_request_id *request_id_out, cpkt_opcua_node_id *node_id_out,
    char *node_id_buffer, size_t node_id_buffer_size,
    size_t *required_node_id_size_out, cpkt_opcua_status *status_out) {
  struct cpkt_opcua_async_context *context;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_NodeId reference_type_id;
  UA_NodeId type_definition;
  UA_VariableAttributes attr;
  UA_UInt32 request_id;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (request_id_out != NULL) {
    *request_id_out = 0;
  }
  if (required_node_id_size_out != NULL) {
    *required_node_id_size_out = 0;
  }
  if (client == NULL || client->client == NULL || browse_name == NULL ||
      value == NULL || fn == NULL || node_id_out == NULL ||
      !cpkt_valid_node_id(node_id) || !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  attr = UA_VariableAttributes_default;
  result = cpkt_set_variant(&attr.value, value);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  context = (struct cpkt_opcua_async_context *)calloc(1, sizeof(*context));
  if (context == NULL) {
    UA_Variant_clear(&attr.value);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->node_fn = fn;
  context->user = user;
  context->node_id_out = node_id_out;
  context->node_id_buffer = node_id_buffer;
  context->node_id_buffer_size = node_id_buffer_size;
  context->required_node_id_size_out = required_node_id_size_out;
  cpkt_async_link(client, context);
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  reference_type_id = UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT);
  type_definition = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE);
  attr.displayName = UA_LOCALIZEDTEXT(
      (char *)"en-US",
      (char *)(display_name != NULL ? display_name : browse_name));
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  request_id = 0;
  status = UA_Client_addVariableNode_async(
      client->client, requested_node_id, parent_native_node_id,
      reference_type_id,
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      type_definition, attr, NULL, cpkt_async_add_node_callback, context,
      &request_id);
  UA_Variant_clear(&attr.value);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    cpkt_async_finish(context);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (request_id_out != NULL) {
    *request_id_out = (cpkt_opcua_request_id)request_id;
  }
  return CPKT_OPCUA_OK;
}
