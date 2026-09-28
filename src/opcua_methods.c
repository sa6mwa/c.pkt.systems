/* Handwritten OPC UA facade: methods. */
#include "opcua_facade_internal.h"
#include <open62541/client_highlevel.h>
#include <open62541/nodeids.h>
#include <stdio.h>
#include <stdlib.h>

static UA_Argument *cpkt_make_method_arguments(const int *types,
                                               size_t type_count,
                                               const char *name_prefix) {
  UA_Argument *arguments;
  size_t i;
  char name_buffer[32];

  if (type_count == 0) {
    return NULL;
  }
  if (types == NULL || name_prefix == NULL) {
    return NULL;
  }
  arguments = (UA_Argument *)calloc(type_count, sizeof(*arguments));
  if (arguments == NULL) {
    return NULL;
  }
  for (i = 0; i < type_count; ++i) {
    UA_Argument_init(&arguments[i]);
    if (!cpkt_valid_method_value_type(types[i])) {
      while (i > 0) {
        --i;
        UA_Argument_clear(&arguments[i]);
      }
      free(arguments);
      return NULL;
    }
    snprintf(name_buffer, sizeof(name_buffer), "%s%lu", name_prefix,
             (unsigned long)(i + 1));
    arguments[i].description =
        UA_LOCALIZEDTEXT_ALLOC((char *)"en-US", name_buffer);
    arguments[i].name = UA_STRING_ALLOC(name_buffer);
    if (arguments[i].description.text.data == NULL ||
        arguments[i].name.data == NULL) {
      UA_Argument_clear(&arguments[i]);
      while (i > 0) {
        --i;
        UA_Argument_clear(&arguments[i]);
      }
      free(arguments);
      return NULL;
    }
    arguments[i].dataType = cpkt_data_type_node_id_for_value_type(types[i]);
    arguments[i].valueRank = UA_VALUERANK_SCALAR;
  }
  return arguments;
}

static void cpkt_clear_method_arguments(UA_Argument *arguments,
                                        size_t argument_count) {
  size_t i;

  if (arguments == NULL) {
    return;
  }
  for (i = 0; i < argument_count; ++i) {
    UA_Argument_clear(&arguments[i]);
  }
  free(arguments);
}

static int cpkt_valid_method_argument_direction(int direction) {
  return direction == CPKT_OPCUA_METHOD_ARGUMENT_INPUT ||
         direction == CPKT_OPCUA_METHOD_ARGUMENT_OUTPUT;
}

static const char *cpkt_method_argument_property_name(int direction) {
  return direction == CPKT_OPCUA_METHOD_ARGUMENT_INPUT ? "InputArguments"
                                                       : "OutputArguments";
}

static cpkt_opcua_result
cpkt_decode_method_argument_count(const UA_Variant *variant,
                                  size_t *argument_count_out) {
  if (argument_count_out != NULL) {
    *argument_count_out = 0;
  }
  if (variant == NULL || argument_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!UA_Variant_hasArrayType(variant, &UA_TYPES[UA_TYPES_ARGUMENT])) {
    return CPKT_OPCUA_ERR_TYPE;
  }
  *argument_count_out = variant->arrayLength;
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_decode_method_argument(
    const UA_Variant *variant, size_t argument_index,
    cpkt_opcua_node_id *data_type_out, long *value_rank_out, char *name_buffer,
    size_t name_buffer_size, size_t *required_name_size_out) {
  const UA_Argument *arguments;
  const UA_Argument *argument;
  struct cpkt_owned_node_id_memory data_type_memory;
  cpkt_opcua_node_id data_type;
  cpkt_opcua_result result;

  if (required_name_size_out != NULL) {
    *required_name_size_out = 0;
  }
  if (data_type_out != NULL) {
    *data_type_out = cpkt_opcua_node_id_null();
  }
  if (value_rank_out != NULL) {
    *value_rank_out = 0;
  }
  if (variant == NULL || data_type_out == NULL || value_rank_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!UA_Variant_hasArrayType(variant, &UA_TYPES[UA_TYPES_ARGUMENT])) {
    return CPKT_OPCUA_ERR_TYPE;
  }
  if (argument_index >= variant->arrayLength) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  arguments = (const UA_Argument *)variant->data;
  argument = &arguments[argument_index];
  data_type_memory.string = NULL;
  data_type_memory.byte_string = NULL;
  if (!cpkt_native_node_id_to_facade(&argument->dataType, &data_type,
                                     &data_type_memory)) {
    cpkt_owned_node_id_memory_clear(&data_type_memory);
    return CPKT_OPCUA_ERR_TYPE;
  }
  if (data_type.identifier_type != CPKT_OPCUA_NODE_ID_NUMERIC &&
      data_type.identifier_type != CPKT_OPCUA_NODE_ID_NULL) {
    cpkt_owned_node_id_memory_clear(&data_type_memory);
    return CPKT_OPCUA_ERR_TYPE;
  }
  *data_type_out = data_type;
  *value_rank_out = (long)argument->valueRank;
  cpkt_owned_node_id_memory_clear(&data_type_memory);
  result = cpkt_copy_ua_string_to_buffer(
      &argument->name, name_buffer, name_buffer_size, required_name_size_out);
  return result;
}

static cpkt_opcua_result
cpkt_browse_path_result_native_node_id(UA_BrowsePathResult *browse_path_result,
                                       UA_NodeId *target_node_id_out,
                                       cpkt_opcua_status *status_out) {
  if (target_node_id_out != NULL) {
    UA_NodeId_init(target_node_id_out);
  }
  if (browse_path_result == NULL || target_node_id_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (status_out != NULL) {
    *status_out = cpkt_status(browse_path_result->statusCode);
  }
  if (browse_path_result->statusCode != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (browse_path_result->targetsSize == 0) {
    if (status_out != NULL) {
      *status_out = cpkt_status(UA_STATUSCODE_BADNOMATCH);
    }
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (browse_path_result->targets[0].targetId.serverIndex != 0 ||
      browse_path_result->targets[0].targetId.namespaceUri.length != 0 ||
      browse_path_result->targets[0].remainingPathIndex !=
          (UA_UInt32)UINT_MAX) {
    return CPKT_OPCUA_ERR_TYPE;
  }
  *target_node_id_out = browse_path_result->targets[0].targetId.nodeId;
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_server_read_method_argument_variant(
    UA_Server *server, cpkt_opcua_node_id method_node_id, int direction,
    UA_Variant *variant_out, cpkt_opcua_status *status_out) {
  cpkt_opcua_browse_path_element element;
  UA_BrowsePath browse_path;
  UA_BrowsePathResult browse_path_result;
  UA_NodeId argument_node_id;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (variant_out != NULL) {
    UA_Variant_init(variant_out);
  }
  if (server == NULL || variant_out == NULL ||
      !cpkt_valid_node_id(method_node_id) ||
      !cpkt_valid_method_argument_direction(direction)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  element.namespace_index = 0;
  element.browse_name = cpkt_method_argument_property_name(direction);
  result = cpkt_fill_browse_path(&browse_path, method_node_id, &element, 1);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  browse_path_result =
      UA_Server_translateBrowsePathToNodeIds(server, &browse_path);
  result = cpkt_browse_path_result_native_node_id(
      &browse_path_result, &argument_node_id, status_out);
  if (result == CPKT_OPCUA_OK) {
    status = UA_Server_readValue(server, argument_node_id, variant_out);
    if (status_out != NULL) {
      *status_out = cpkt_status(status);
    }
    if (status != UA_STATUSCODE_GOOD) {
      result = CPKT_OPCUA_ERR_UPSTREAM;
    }
  }
  UA_BrowsePathResult_clear(&browse_path_result);
  UA_BrowsePath_clear(&browse_path);
  return result;
}

static cpkt_opcua_result cpkt_client_read_method_argument_variant(
    UA_Client *client, cpkt_opcua_node_id method_node_id, int direction,
    UA_Variant *variant_out, cpkt_opcua_status *status_out) {
  cpkt_opcua_browse_path_element element;
  UA_BrowsePath browse_path;
  UA_BrowsePathResult browse_path_result;
  UA_NodeId argument_node_id;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (variant_out != NULL) {
    UA_Variant_init(variant_out);
  }
  if (client == NULL || variant_out == NULL ||
      !cpkt_valid_node_id(method_node_id) ||
      !cpkt_valid_method_argument_direction(direction)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  element.namespace_index = 0;
  element.browse_name = cpkt_method_argument_property_name(direction);
  result = cpkt_fill_browse_path(&browse_path, method_node_id, &element, 1);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  browse_path_result =
      UA_Client_translateBrowsePathToNodeIds(client, &browse_path);
  result = cpkt_browse_path_result_native_node_id(
      &browse_path_result, &argument_node_id, status_out);
  if (result == CPKT_OPCUA_OK) {
    status =
        UA_Client_readValueAttribute(client, argument_node_id, variant_out);
    if (status_out != NULL) {
      *status_out = cpkt_status(status);
    }
    if (status != UA_STATUSCODE_GOOD) {
      result = CPKT_OPCUA_ERR_UPSTREAM;
    }
  }
  UA_BrowsePathResult_clear(&browse_path_result);
  UA_BrowsePath_clear(&browse_path);
  return result;
}

static UA_StatusCode cpkt_method_callback(
    UA_Server *native_server, const UA_NodeId *session_id,
    void *session_context, const UA_NodeId *method_id, void *method_context,
    const UA_NodeId *object_id, void *object_context, size_t input_size,
    const UA_Variant *input, size_t output_size, UA_Variant *output) {
  struct cpkt_opcua_method_context *context;
  cpkt_opcua_value *inputs;
  cpkt_opcua_value *outputs;
  char **string_buffers;
  cpkt_opcua_result result;
  size_t required;
  size_t i;
  size_t j;

  (void)native_server;
  (void)session_id;
  (void)session_context;
  (void)method_id;
  (void)object_id;
  (void)object_context;
  context = (struct cpkt_opcua_method_context *)method_context;
  if (context == NULL || (context->fn == NULL && context->single_fn == NULL) ||
      input_size != context->input_count ||
      output_size != context->output_count || output == NULL) {
    return UA_STATUSCODE_BADINTERNALERROR;
  }
  inputs = NULL;
  outputs = NULL;
  string_buffers = NULL;
  if (output_size == 0) {
    return UA_STATUSCODE_BADINTERNALERROR;
  }
  if (input_size != 0) {
    inputs = (cpkt_opcua_value *)calloc(input_size, sizeof(*inputs));
    if (inputs == NULL) {
      return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    string_buffers = (char **)calloc(input_size, sizeof(*string_buffers));
    if (string_buffers == NULL) {
      free(inputs);
      return UA_STATUSCODE_BADOUTOFMEMORY;
    }
  }
  outputs = (cpkt_opcua_value *)calloc(output_size, sizeof(*outputs));
  if (outputs == NULL) {
    free(string_buffers);
    free(inputs);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  for (i = 0; i < input_size; ++i) {
    result = cpkt_get_variant(&input[i], &inputs[i], NULL, 0, &required);
    if (result == CPKT_OPCUA_ERR_RANGE &&
        cpkt_value_type_needs_buffer(inputs[i].type) && required != 0) {
      string_buffers[i] = (char *)malloc(required);
      if (string_buffers[i] == NULL) {
        for (j = 0; j < i; ++j) {
          free(string_buffers[j]);
        }
        free(string_buffers);
        free(inputs);
        free(outputs);
        return UA_STATUSCODE_BADOUTOFMEMORY;
      }
      result = cpkt_get_variant(&input[i], &inputs[i], string_buffers[i],
                                required, &required);
    }
    if (result != CPKT_OPCUA_OK || inputs[i].type != context->input_types[i]) {
      for (j = 0; j <= i; ++j) {
        free(string_buffers[j]);
      }
      free(string_buffers);
      free(inputs);
      free(outputs);
      return UA_STATUSCODE_BADTYPEMISMATCH;
    }
  }
  for (i = 0; i < output_size; ++i) {
    cpkt_opcua_value_clear(&outputs[i]);
  }
  if (context->single_fn != NULL) {
    if (output_size != 1) {
      result = CPKT_OPCUA_ERR_ARG;
    } else {
      result =
          context->single_fn(inputs, input_size, &outputs[0], context->user);
    }
  } else {
    result =
        context->fn(inputs, input_size, outputs, output_size, context->user);
  }
  if (result == CPKT_OPCUA_OK) {
    for (i = 0; i < output_size; ++i) {
      if (outputs[i].type != context->output_types[i]) {
        result = CPKT_OPCUA_ERR_TYPE;
        break;
      }
    }
  }
  if (result == CPKT_OPCUA_OK) {
    for (i = 0; i < output_size; ++i) {
      result = cpkt_set_variant(&output[i], &outputs[i]);
      if (result != CPKT_OPCUA_OK) {
        break;
      }
    }
  }
  for (i = 0; i < input_size; ++i) {
    free(string_buffers[i]);
  }
  free(string_buffers);
  free(inputs);
  free(outputs);
  if (result != CPKT_OPCUA_OK) {
    if (result == CPKT_OPCUA_ERR_ALLOC) {
      return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    if (result == CPKT_OPCUA_ERR_TYPE || result == CPKT_OPCUA_ERR_RANGE) {
      return UA_STATUSCODE_BADTYPEMISMATCH;
    }
    return UA_STATUSCODE_BADINTERNALERROR;
  }
  return UA_STATUSCODE_GOOD;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_method(
    cpkt_opcua_server *server, cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id, const char *browse_name,
    const char *display_name, const int *input_types, size_t input_count,
    int output_type, cpkt_opcua_method_fn fn, void *user,
    cpkt_opcua_status *status_out) {
  struct cpkt_opcua_method_context *context;
  UA_MethodAttributes attr;
  UA_Argument *input_arguments;
  UA_Argument *output_arguments;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_StatusCode status;
  size_t i;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || browse_name == NULL ||
      fn == NULL || !cpkt_valid_node_id(node_id) ||
      !cpkt_valid_node_id(parent_node_id) ||
      (input_count != 0 && input_types == NULL)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!cpkt_valid_method_value_type(output_type)) {
    return CPKT_OPCUA_ERR_TYPE;
  }
  for (i = 0; i < input_count; ++i) {
    if (!cpkt_valid_method_value_type(input_types[i])) {
      return CPKT_OPCUA_ERR_TYPE;
    }
  }
  context = (struct cpkt_opcua_method_context *)calloc(1, sizeof(*context));
  if (context == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->single_fn = fn;
  context->user = user;
  context->input_count = input_count;
  context->output_count = 1;
  if (input_count != 0) {
    context->input_types =
        (int *)calloc(input_count, sizeof(*context->input_types));
    if (context->input_types == NULL) {
      free(context);
      return CPKT_OPCUA_ERR_ALLOC;
    }
    for (i = 0; i < input_count; ++i) {
      context->input_types[i] = input_types[i];
    }
  }
  context->output_types = (int *)calloc(1, sizeof(*context->output_types));
  if (context->output_types == NULL) {
    free(context->input_types);
    free(context);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->output_types[0] = output_type;

  input_arguments =
      cpkt_make_method_arguments(input_types, input_count, "input");
  if (input_count != 0 && input_arguments == NULL) {
    free(context->output_types);
    free(context->input_types);
    free(context);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  output_arguments = cpkt_make_method_arguments(&output_type, 1, "output");
  if (output_arguments == NULL) {
    cpkt_clear_method_arguments(input_arguments, input_count);
    free(context->output_types);
    free(context->input_types);
    free(context);
    return CPKT_OPCUA_ERR_ALLOC;
  }

  attr = UA_MethodAttributes_default;
  attr.executable = true;
  attr.userExecutable = true;
  attr.writeMask = UA_WRITEMASK_EXECUTABLE | UA_WRITEMASK_DISPLAYNAME |
                   UA_WRITEMASK_DESCRIPTION;
  attr.userWriteMask = UA_WRITEMASK_EXECUTABLE | UA_WRITEMASK_DISPLAYNAME |
                       UA_WRITEMASK_DESCRIPTION;
  attr.displayName = UA_LOCALIZEDTEXT(
      (char *)"en-US",
      (char *)(display_name != NULL ? display_name : browse_name));
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  status = UA_Server_addMethodNode(
      server->server, requested_node_id, parent_native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      attr, cpkt_method_callback, input_count, input_arguments, 1,
      output_arguments, context, NULL);
  cpkt_clear_method_arguments(input_arguments, input_count);
  cpkt_clear_method_arguments(output_arguments, 1);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    free(context->output_types);
    free(context->input_types);
    free(context);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  context->next = server->methods;
  server->methods = context;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_add_method_many(
    cpkt_opcua_server *server, cpkt_opcua_node_id node_id,
    cpkt_opcua_node_id parent_node_id, const char *browse_name,
    const char *display_name, const int *input_types, size_t input_count,
    const int *output_types, size_t output_count, cpkt_opcua_method_many_fn fn,
    void *user, cpkt_opcua_status *status_out) {
  struct cpkt_opcua_method_context *context;
  UA_MethodAttributes attr;
  UA_Argument *input_arguments;
  UA_Argument *output_arguments;
  UA_NodeId requested_node_id;
  UA_NodeId parent_native_node_id;
  UA_StatusCode status;
  size_t i;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || browse_name == NULL ||
      fn == NULL || !cpkt_valid_node_id(node_id) ||
      !cpkt_valid_node_id(parent_node_id) ||
      (input_count != 0 && input_types == NULL) || output_types == NULL ||
      output_count == 0) {
    return CPKT_OPCUA_ERR_ARG;
  }
  for (i = 0; i < input_count; ++i) {
    if (!cpkt_valid_method_value_type(input_types[i])) {
      return CPKT_OPCUA_ERR_TYPE;
    }
  }
  for (i = 0; i < output_count; ++i) {
    if (!cpkt_valid_method_value_type(output_types[i])) {
      return CPKT_OPCUA_ERR_TYPE;
    }
  }
  context = (struct cpkt_opcua_method_context *)calloc(1, sizeof(*context));
  if (context == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->fn = fn;
  context->user = user;
  context->input_count = input_count;
  context->output_count = output_count;
  if (input_count != 0) {
    context->input_types =
        (int *)calloc(input_count, sizeof(*context->input_types));
    if (context->input_types == NULL) {
      free(context);
      return CPKT_OPCUA_ERR_ALLOC;
    }
    for (i = 0; i < input_count; ++i) {
      context->input_types[i] = input_types[i];
    }
  }
  context->output_types =
      (int *)calloc(output_count, sizeof(*context->output_types));
  if (context->output_types == NULL) {
    free(context->input_types);
    free(context);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  for (i = 0; i < output_count; ++i) {
    context->output_types[i] = output_types[i];
  }

  input_arguments =
      cpkt_make_method_arguments(input_types, input_count, "input");
  if (input_count != 0 && input_arguments == NULL) {
    free(context->output_types);
    free(context->input_types);
    free(context);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  output_arguments =
      cpkt_make_method_arguments(output_types, output_count, "output");
  if (output_arguments == NULL) {
    cpkt_clear_method_arguments(input_arguments, input_count);
    free(context->output_types);
    free(context->input_types);
    free(context);
    return CPKT_OPCUA_ERR_ALLOC;
  }

  attr = UA_MethodAttributes_default;
  attr.executable = true;
  attr.userExecutable = true;
  attr.writeMask = UA_WRITEMASK_EXECUTABLE | UA_WRITEMASK_DISPLAYNAME |
                   UA_WRITEMASK_DESCRIPTION;
  attr.userWriteMask = UA_WRITEMASK_EXECUTABLE | UA_WRITEMASK_DISPLAYNAME |
                       UA_WRITEMASK_DESCRIPTION;
  attr.displayName = UA_LOCALIZEDTEXT(
      (char *)"en-US",
      (char *)(display_name != NULL ? display_name : browse_name));
  requested_node_id = cpkt_make_node_id(node_id);
  parent_native_node_id = cpkt_make_node_id(parent_node_id);
  status = UA_Server_addMethodNode(
      server->server, requested_node_id, parent_native_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
      UA_QUALIFIEDNAME((UA_UInt16)node_id.namespace_index, (char *)browse_name),
      attr, cpkt_method_callback, input_count, input_arguments, output_count,
      output_arguments, context, NULL);
  cpkt_clear_method_arguments(input_arguments, input_count);
  cpkt_clear_method_arguments(output_arguments, output_count);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    free(context->output_types);
    free(context->input_types);
    free(context);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  context->next = server->methods;
  server->methods = context;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_method_argument_count(
    cpkt_opcua_server *server, cpkt_opcua_node_id method_node_id, int direction,
    size_t *argument_count_out, cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (argument_count_out != NULL) {
    *argument_count_out = 0;
  }
  if (server == NULL || server->server == NULL || argument_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_server_read_method_argument_variant(
      server->server, method_node_id, direction, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_decode_method_argument_count(&variant, argument_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_read_method_argument(
    cpkt_opcua_server *server, cpkt_opcua_node_id method_node_id, int direction,
    size_t argument_index, cpkt_opcua_node_id *data_type_out,
    long *value_rank_out, char *name_buffer, size_t name_buffer_size,
    size_t *required_name_size_out, cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (required_name_size_out != NULL) {
    *required_name_size_out = 0;
  }
  if (data_type_out != NULL) {
    *data_type_out = cpkt_opcua_node_id_null();
  }
  if (value_rank_out != NULL) {
    *value_rank_out = 0;
  }
  if (server == NULL || server->server == NULL || data_type_out == NULL ||
      value_rank_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_server_read_method_argument_variant(
      server->server, method_node_id, direction, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_decode_method_argument(
      &variant, argument_index, data_type_out, value_rank_out, name_buffer,
      name_buffer_size, required_name_size_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_method_argument_count(
    cpkt_opcua_client *client, cpkt_opcua_node_id method_node_id, int direction,
    size_t *argument_count_out, cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (argument_count_out != NULL) {
    *argument_count_out = 0;
  }
  if (client == NULL || client->client == NULL || argument_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_client_read_method_argument_variant(
      client->client, method_node_id, direction, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_decode_method_argument_count(&variant, argument_count_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_read_method_argument(
    cpkt_opcua_client *client, cpkt_opcua_node_id method_node_id, int direction,
    size_t argument_index, cpkt_opcua_node_id *data_type_out,
    long *value_rank_out, char *name_buffer, size_t name_buffer_size,
    size_t *required_name_size_out, cpkt_opcua_status *status_out) {
  UA_Variant variant;
  cpkt_opcua_result result;

  if (required_name_size_out != NULL) {
    *required_name_size_out = 0;
  }
  if (data_type_out != NULL) {
    *data_type_out = cpkt_opcua_node_id_null();
  }
  if (value_rank_out != NULL) {
    *value_rank_out = 0;
  }
  if (client == NULL || client->client == NULL || data_type_out == NULL ||
      value_rank_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_client_read_method_argument_variant(
      client->client, method_node_id, direction, &variant, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_decode_method_argument(
      &variant, argument_index, data_type_out, value_rank_out, name_buffer,
      name_buffer_size, required_name_size_out);
  UA_Variant_clear(&variant);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_call_method(
    cpkt_opcua_client *client, cpkt_opcua_node_id object_node_id,
    cpkt_opcua_node_id method_node_id, const cpkt_opcua_value *inputs,
    size_t input_count, cpkt_opcua_value *output, char *string_buffer,
    size_t string_buffer_size, size_t *required_string_size_out,
    cpkt_opcua_status *status_out) {
  UA_NodeId native_object_node_id;
  UA_NodeId native_method_node_id;
  UA_Variant *input_variants;
  UA_Variant *output_variants;
  size_t output_count;
  UA_StatusCode status;
  cpkt_opcua_result result;
  size_t i;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_string_size_out != NULL) {
    *required_string_size_out = 0;
  }
  if (client == NULL || client->client == NULL || output == NULL ||
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
  native_object_node_id = cpkt_make_node_id(object_node_id);
  native_method_node_id = cpkt_make_node_id(method_node_id);
  output_count = 0;
  output_variants = NULL;
  status = UA_Client_call(client->client, native_object_node_id,
                          native_method_node_id, input_count, input_variants,
                          &output_count, &output_variants);
  for (i = 0; i < input_count; ++i) {
    UA_Variant_clear(&input_variants[i]);
  }
  free(input_variants);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    if (output_variants != NULL) {
      UA_Array_delete(output_variants, output_count,
                      &UA_TYPES[UA_TYPES_VARIANT]);
    }
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (output_count != 1 || output_variants == NULL) {
    if (output_variants != NULL) {
      UA_Array_delete(output_variants, output_count,
                      &UA_TYPES[UA_TYPES_VARIANT]);
    }
    return CPKT_OPCUA_ERR_TYPE;
  }
  result = cpkt_get_variant(&output_variants[0], output, string_buffer,
                            string_buffer_size, required_string_size_out);
  UA_Array_delete(output_variants, output_count, &UA_TYPES[UA_TYPES_VARIANT]);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_call_method_many(
    cpkt_opcua_client *client, cpkt_opcua_node_id object_node_id,
    cpkt_opcua_node_id method_node_id, const cpkt_opcua_value *inputs,
    size_t input_count, cpkt_opcua_value *outputs, size_t expected_output_count,
    char **string_buffers, const size_t *string_buffer_sizes,
    size_t *required_string_sizes_out, cpkt_opcua_status *status_out) {
  UA_NodeId native_object_node_id;
  UA_NodeId native_method_node_id;
  UA_Variant *input_variants;
  UA_Variant *output_variants;
  size_t output_count;
  UA_StatusCode status;
  cpkt_opcua_result result;
  cpkt_opcua_result output_result;
  char *string_buffer;
  size_t string_buffer_size;
  size_t i;

  if (status_out != NULL) {
    *status_out = 0;
  }
  for (i = 0; required_string_sizes_out != NULL && i < expected_output_count;
       ++i) {
    required_string_sizes_out[i] = 0;
  }
  if (client == NULL || client->client == NULL || outputs == NULL ||
      expected_output_count == 0 || (input_count != 0 && inputs == NULL) ||
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
  native_object_node_id = cpkt_make_node_id(object_node_id);
  native_method_node_id = cpkt_make_node_id(method_node_id);
  output_count = 0;
  output_variants = NULL;
  status = UA_Client_call(client->client, native_object_node_id,
                          native_method_node_id, input_count, input_variants,
                          &output_count, &output_variants);
  for (i = 0; i < input_count; ++i) {
    UA_Variant_clear(&input_variants[i]);
  }
  free(input_variants);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    if (output_variants != NULL) {
      UA_Array_delete(output_variants, output_count,
                      &UA_TYPES[UA_TYPES_VARIANT]);
    }
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (output_count != expected_output_count || output_variants == NULL) {
    if (output_variants != NULL) {
      UA_Array_delete(output_variants, output_count,
                      &UA_TYPES[UA_TYPES_VARIANT]);
    }
    return CPKT_OPCUA_ERR_TYPE;
  }
  result = CPKT_OPCUA_OK;
  for (i = 0; i < output_count; ++i) {
    string_buffer = string_buffers != NULL ? string_buffers[i] : NULL;
    string_buffer_size =
        string_buffer_sizes != NULL ? string_buffer_sizes[i] : 0;
    output_result = cpkt_get_variant(
        &output_variants[i], &outputs[i], string_buffer, string_buffer_size,
        required_string_sizes_out != NULL ? &required_string_sizes_out[i]
                                          : NULL);
    if (output_result != CPKT_OPCUA_OK && result == CPKT_OPCUA_OK) {
      result = output_result;
    }
  }
  UA_Array_delete(output_variants, output_count, &UA_TYPES[UA_TYPES_VARIANT]);
  return result;
}
