/* Handwritten OPC UA facade: browse. */
#include "opcua_facade_internal.h"
#include <open62541/client_highlevel.h>
#include <open62541/nodeids.h>
#include <stdlib.h>

cpkt_opcua_result cpkt_browse_result_each(UA_BrowseResult *browse_result,
                                          cpkt_opcua_browse_fn fn, void *user,
                                          cpkt_opcua_status *status_out) {
  size_t i;
  cpkt_opcua_browse_entry entry;
  UA_ReferenceDescription *reference;
  struct cpkt_owned_node_id_memory node_id_memory;
  char *browse_name;
  char *display_name;

  if (status_out != NULL) {
    *status_out = cpkt_status(browse_result->statusCode);
  }
  if (browse_result->statusCode != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }

  for (i = 0; i < browse_result->referencesSize; ++i) {
    reference = &browse_result->references[i];
    node_id_memory.string = NULL;
    node_id_memory.byte_string = NULL;
    browse_name = NULL;
    display_name = NULL;
    if (reference->nodeId.serverIndex != 0 ||
        !cpkt_native_node_id_to_facade(&reference->nodeId.nodeId,
                                       &entry.target_node_id,
                                       &node_id_memory)) {
      cpkt_owned_node_id_memory_clear(&node_id_memory);
      return CPKT_OPCUA_ERR_TYPE;
    }
    browse_name = cpkt_copy_ua_string(&reference->browseName.name);
    if (browse_name == NULL) {
      cpkt_owned_node_id_memory_clear(&node_id_memory);
      return CPKT_OPCUA_ERR_ALLOC;
    }
    display_name = cpkt_copy_ua_string(&reference->displayName.text);
    if (display_name == NULL) {
      free(browse_name);
      cpkt_owned_node_id_memory_clear(&node_id_memory);
      return CPKT_OPCUA_ERR_ALLOC;
    }
    entry.node_class = (unsigned long)reference->nodeClass;
    entry.browse_name_namespace_index =
        (unsigned short)reference->browseName.namespaceIndex;
    entry.browse_name = browse_name;
    entry.display_name = display_name;
    entry.is_forward = reference->isForward ? 1 : 0;
    if (fn(&entry, user) != 0) {
      free(display_name);
      free(browse_name);
      cpkt_owned_node_id_memory_clear(&node_id_memory);
      return CPKT_OPCUA_ERR_CALLBACK;
    }
    free(display_name);
    free(browse_name);
    cpkt_owned_node_id_memory_clear(&node_id_memory);
  }

  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result
cpkt_browse_result_page(UA_BrowseResult *browse_result, cpkt_opcua_browse_fn fn,
                        void *user, unsigned char *continuation_point_buffer,
                        size_t continuation_point_buffer_size,
                        size_t *required_continuation_point_size_out,
                        cpkt_opcua_status *status_out) {
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = cpkt_status(browse_result->statusCode);
  }
  if (required_continuation_point_size_out != NULL) {
    *required_continuation_point_size_out = 0;
  }
  if (browse_result->statusCode != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_ua_byte_string_to_buffer(
      &browse_result->continuationPoint, continuation_point_buffer,
      continuation_point_buffer_size, required_continuation_point_size_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  if (fn == NULL) {
    return CPKT_OPCUA_OK;
  }
  return cpkt_browse_result_each(browse_result, fn, user, status_out);
}

static void cpkt_server_release_browse_continuation_point(
    UA_Server *server, const UA_ByteString *continuation_point) {
  UA_BrowseResult release_result;

  if (server == NULL || continuation_point == NULL ||
      continuation_point->length == 0) {
    return;
  }
  release_result = UA_Server_browseNext(server, true, continuation_point);
  UA_BrowseResult_clear(&release_result);
}

static void cpkt_client_release_browse_continuation_point(
    UA_Client *client, UA_ByteString continuation_point) {
  UA_BrowseResult release_result;

  if (client == NULL || continuation_point.length == 0) {
    return;
  }
  release_result = UA_Client_browseNext(client, true, continuation_point);
  UA_BrowseResult_clear(&release_result);
}

cpkt_opcua_result
cpkt_fill_browse_description(UA_BrowseDescription *browse_description,
                             cpkt_opcua_node_id parent_node_id,
                             const cpkt_opcua_browse_options *options) {
  UA_NodeId reference_type_id;

  if (browse_description == NULL || !cpkt_valid_node_id(parent_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (options != NULL) {
    if (options->browse_direction != CPKT_OPCUA_BROWSE_FORWARD &&
        options->browse_direction != CPKT_OPCUA_BROWSE_INVERSE &&
        options->browse_direction != CPKT_OPCUA_BROWSE_BOTH) {
      return CPKT_OPCUA_ERR_ARG;
    }
    if (options->has_reference_type &&
        !cpkt_valid_node_id(options->reference_type_id)) {
      return CPKT_OPCUA_ERR_ARG;
    }
    if (options->max_references > (unsigned long)UINT_MAX ||
        options->node_class_mask > (unsigned long)UINT_MAX ||
        options->result_mask > (unsigned long)UINT_MAX) {
      return CPKT_OPCUA_ERR_RANGE;
    }
  }

  UA_BrowseDescription_init(browse_description);
  browse_description->nodeId = cpkt_make_node_id(parent_node_id);
  browse_description->browseDirection = UA_BROWSEDIRECTION_FORWARD;
  browse_description->referenceTypeId =
      UA_NODEID_NUMERIC(0, UA_NS0ID_HIERARCHICALREFERENCES);
  browse_description->includeSubtypes = true;
  browse_description->nodeClassMask = 0;
  browse_description->resultMask = UA_BROWSERESULTMASK_ALL;

  if (options == NULL) {
    return CPKT_OPCUA_OK;
  }
  if (options->browse_direction == CPKT_OPCUA_BROWSE_INVERSE) {
    browse_description->browseDirection = UA_BROWSEDIRECTION_INVERSE;
  } else if (options->browse_direction == CPKT_OPCUA_BROWSE_BOTH) {
    browse_description->browseDirection = UA_BROWSEDIRECTION_BOTH;
  }
  if (options->has_reference_type) {
    reference_type_id = cpkt_make_node_id(options->reference_type_id);
    browse_description->referenceTypeId = reference_type_id;
  }
  browse_description->includeSubtypes =
      options->include_subtypes ? true : false;
  browse_description->nodeClassMask = (UA_UInt32)options->node_class_mask;
  browse_description->resultMask = (UA_UInt32)options->result_mask;
  return CPKT_OPCUA_OK;
}

cpkt_opcua_result cpkt_fill_browse_path(
    UA_BrowsePath *browse_path, cpkt_opcua_node_id start_node_id,
    const cpkt_opcua_browse_path_element *elements, size_t element_count) {
  UA_NodeId native_start_node_id;
  size_t i;

  if (browse_path == NULL || elements == NULL || element_count == 0 ||
      !cpkt_valid_node_id(start_node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  UA_BrowsePath_init(browse_path);
  native_start_node_id = cpkt_make_node_id(start_node_id);
  if (UA_NodeId_copy(&native_start_node_id, &browse_path->startingNode) !=
      UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  browse_path->relativePath.elements = (UA_RelativePathElement *)calloc(
      element_count, sizeof(*browse_path->relativePath.elements));
  if (browse_path->relativePath.elements == NULL) {
    UA_BrowsePath_clear(browse_path);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  browse_path->relativePath.elementsSize = element_count;
  for (i = 0; i < element_count; ++i) {
    if (elements[i].browse_name == NULL) {
      UA_BrowsePath_clear(browse_path);
      return CPKT_OPCUA_ERR_ARG;
    }
    UA_RelativePathElement_init(&browse_path->relativePath.elements[i]);
    browse_path->relativePath.elements[i].referenceTypeId =
        UA_NODEID_NUMERIC(0, UA_NS0ID_HIERARCHICALREFERENCES);
    browse_path->relativePath.elements[i].isInverse = false;
    browse_path->relativePath.elements[i].includeSubtypes = true;
    browse_path->relativePath.elements[i].targetName.namespaceIndex =
        (UA_UInt16)elements[i].namespace_index;
    browse_path->relativePath.elements[i].targetName.name =
        UA_STRING_ALLOC((char *)elements[i].browse_name);
    if (elements[i].browse_name[0] != '\0' &&
        browse_path->relativePath.elements[i].targetName.name.data == NULL) {
      UA_BrowsePath_clear(browse_path);
      return CPKT_OPCUA_ERR_ALLOC;
    }
  }
  return CPKT_OPCUA_OK;
}

static cpkt_opcua_result cpkt_translate_browse_path_result_to_target(
    UA_BrowsePathResult *browse_path_result,
    cpkt_opcua_node_id *target_node_id_out, char *target_buffer,
    size_t target_buffer_size, size_t *required_target_size_out,
    cpkt_opcua_status *status_out) {
  struct cpkt_owned_node_id_memory target_memory;
  cpkt_opcua_node_id target;
  cpkt_opcua_result result;

  if (required_target_size_out != NULL) {
    *required_target_size_out = 0;
  }
  if (target_node_id_out != NULL) {
    *target_node_id_out = cpkt_opcua_node_id_null();
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
  target_memory.string = NULL;
  target_memory.byte_string = NULL;
  if (!cpkt_native_node_id_to_facade(
          &browse_path_result->targets[0].targetId.nodeId, &target,
          &target_memory)) {
    cpkt_owned_node_id_memory_clear(&target_memory);
    return CPKT_OPCUA_ERR_TYPE;
  }
  result = cpkt_copy_node_id_to_caller(
      &target, &target_memory, target_node_id_out, target_buffer,
      target_buffer_size, required_target_size_out);
  cpkt_owned_node_id_memory_clear(&target_memory);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_browse_options_default(cpkt_opcua_browse_options *options) {
  if (options == NULL) {
    return;
  }
  options->browse_direction = CPKT_OPCUA_BROWSE_FORWARD;
  options->include_subtypes = 1;
  options->has_reference_type = 1;
  options->reference_type_id =
      cpkt_opcua_node_id_numeric(0, UA_NS0ID_HIERARCHICALREFERENCES);
  options->node_class_mask = 0;
  options->result_mask = CPKT_OPCUA_BROWSE_RESULT_ALL;
  options->max_references = 0;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_browse_children(
    cpkt_opcua_server *server, cpkt_opcua_node_id parent_node_id,
    cpkt_opcua_browse_fn fn, void *user, cpkt_opcua_status *status_out) {
  cpkt_opcua_browse_options options;

  cpkt_opcua_browse_options_default(&options);
  return cpkt_opcua_server_browse_children_ex(server, parent_node_id, &options,
                                              fn, user, status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_browse_children_ex(
    cpkt_opcua_server *server, cpkt_opcua_node_id parent_node_id,
    const cpkt_opcua_browse_options *options, cpkt_opcua_browse_fn fn,
    void *user, cpkt_opcua_status *status_out) {
  UA_BrowseDescription browse_description;
  UA_BrowseResult browse_result;
  cpkt_opcua_result result;
  unsigned long max_references;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || fn == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_fill_browse_description(&browse_description, parent_node_id,
                                        options);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  max_references = options != NULL ? options->max_references : 0;
  browse_result = UA_Server_browse(server->server, (UA_UInt32)max_references,
                                   &browse_description);
  result = cpkt_browse_result_each(&browse_result, fn, user, status_out);
  UA_BrowseResult_clear(&browse_result);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_browse_children_page(
    cpkt_opcua_server *server, cpkt_opcua_node_id parent_node_id,
    const cpkt_opcua_browse_options *options, cpkt_opcua_browse_fn fn,
    void *user, unsigned char *continuation_point_buffer,
    size_t continuation_point_buffer_size,
    size_t *required_continuation_point_size_out,
    cpkt_opcua_status *status_out) {
  UA_BrowseDescription browse_description;
  UA_BrowseResult browse_result;
  cpkt_opcua_result result;
  unsigned long max_references;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_continuation_point_size_out != NULL) {
    *required_continuation_point_size_out = 0;
  }
  if (server == NULL || server->server == NULL || fn == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_fill_browse_description(&browse_description, parent_node_id,
                                        options);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  max_references = options != NULL ? options->max_references : 0;
  browse_result = UA_Server_browse(server->server, (UA_UInt32)max_references,
                                   &browse_description);
  result = cpkt_browse_result_page(
      &browse_result, fn, user, continuation_point_buffer,
      continuation_point_buffer_size, required_continuation_point_size_out,
      status_out);
  if (result == CPKT_OPCUA_ERR_RANGE) {
    cpkt_server_release_browse_continuation_point(
        server->server, &browse_result.continuationPoint);
  }
  UA_BrowseResult_clear(&browse_result);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_browse_next(
    cpkt_opcua_server *server, const unsigned char *continuation_point,
    size_t continuation_point_size, int release_continuation_point,
    cpkt_opcua_browse_fn fn, void *user,
    unsigned char *next_continuation_point_buffer,
    size_t next_continuation_point_buffer_size,
    size_t *required_next_continuation_point_size_out,
    cpkt_opcua_status *status_out) {
  UA_ByteString native_continuation_point;
  UA_BrowseResult browse_result;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_next_continuation_point_size_out != NULL) {
    *required_next_continuation_point_size_out = 0;
  }
  if (server == NULL || server->server == NULL ||
      (continuation_point_size != 0 && continuation_point == NULL) ||
      (!release_continuation_point && fn == NULL)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_continuation_point.length = continuation_point_size;
  native_continuation_point.data = (UA_Byte *)continuation_point;
  browse_result = UA_Server_browseNext(
      server->server, release_continuation_point ? true : false,
      &native_continuation_point);
  result = cpkt_browse_result_page(
      &browse_result, release_continuation_point ? NULL : fn, user,
      next_continuation_point_buffer, next_continuation_point_buffer_size,
      required_next_continuation_point_size_out, status_out);
  if (result == CPKT_OPCUA_ERR_RANGE) {
    cpkt_server_release_browse_continuation_point(
        server->server, &browse_result.continuationPoint);
  }
  UA_BrowseResult_clear(&browse_result);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_translate_browse_path(
    cpkt_opcua_server *server, cpkt_opcua_node_id start_node_id,
    const cpkt_opcua_browse_path_element *elements, size_t element_count,
    cpkt_opcua_node_id *target_node_id_out, char *target_buffer,
    size_t target_buffer_size, size_t *required_target_size_out,
    cpkt_opcua_status *status_out) {
  UA_BrowsePath browse_path;
  UA_BrowsePathResult browse_path_result;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || target_node_id_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_fill_browse_path(&browse_path, start_node_id, elements,
                                 element_count);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  browse_path_result =
      UA_Server_translateBrowsePathToNodeIds(server->server, &browse_path);
  result = cpkt_translate_browse_path_result_to_target(
      &browse_path_result, target_node_id_out, target_buffer,
      target_buffer_size, required_target_size_out, status_out);
  UA_BrowsePathResult_clear(&browse_path_result);
  UA_BrowsePath_clear(&browse_path);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_browse_children(
    cpkt_opcua_client *client, cpkt_opcua_node_id parent_node_id,
    cpkt_opcua_browse_fn fn, void *user, cpkt_opcua_status *status_out) {
  cpkt_opcua_browse_options options;

  cpkt_opcua_browse_options_default(&options);
  return cpkt_opcua_client_browse_children_ex(client, parent_node_id, &options,
                                              fn, user, status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_browse_children_ex(
    cpkt_opcua_client *client, cpkt_opcua_node_id parent_node_id,
    const cpkt_opcua_browse_options *options, cpkt_opcua_browse_fn fn,
    void *user, cpkt_opcua_status *status_out) {
  UA_BrowseDescription browse_description;
  UA_BrowseResult browse_result;
  cpkt_opcua_result result;
  unsigned long max_references;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || fn == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_fill_browse_description(&browse_description, parent_node_id,
                                        options);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  max_references = options != NULL ? options->max_references : 0;
  browse_result = UA_Client_browse(
      client->client, NULL, (UA_UInt32)max_references, &browse_description);
  result = cpkt_browse_result_each(&browse_result, fn, user, status_out);
  UA_BrowseResult_clear(&browse_result);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_browse_children_page(
    cpkt_opcua_client *client, cpkt_opcua_node_id parent_node_id,
    const cpkt_opcua_browse_options *options, cpkt_opcua_browse_fn fn,
    void *user, unsigned char *continuation_point_buffer,
    size_t continuation_point_buffer_size,
    size_t *required_continuation_point_size_out,
    cpkt_opcua_status *status_out) {
  UA_BrowseDescription browse_description;
  UA_BrowseResult browse_result;
  cpkt_opcua_result result;
  unsigned long max_references;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_continuation_point_size_out != NULL) {
    *required_continuation_point_size_out = 0;
  }
  if (client == NULL || client->client == NULL || fn == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_fill_browse_description(&browse_description, parent_node_id,
                                        options);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  max_references = options != NULL ? options->max_references : 0;
  browse_result = UA_Client_browse(
      client->client, NULL, (UA_UInt32)max_references, &browse_description);
  result = cpkt_browse_result_page(
      &browse_result, fn, user, continuation_point_buffer,
      continuation_point_buffer_size, required_continuation_point_size_out,
      status_out);
  if (result == CPKT_OPCUA_ERR_RANGE) {
    cpkt_client_release_browse_continuation_point(
        client->client, browse_result.continuationPoint);
  }
  UA_BrowseResult_clear(&browse_result);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_browse_next(
    cpkt_opcua_client *client, const unsigned char *continuation_point,
    size_t continuation_point_size, int release_continuation_point,
    cpkt_opcua_browse_fn fn, void *user,
    unsigned char *next_continuation_point_buffer,
    size_t next_continuation_point_buffer_size,
    size_t *required_next_continuation_point_size_out,
    cpkt_opcua_status *status_out) {
  UA_ByteString native_continuation_point;
  UA_BrowseResult browse_result;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_next_continuation_point_size_out != NULL) {
    *required_next_continuation_point_size_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      (continuation_point_size != 0 && continuation_point == NULL) ||
      (!release_continuation_point && fn == NULL)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_continuation_point.length = continuation_point_size;
  native_continuation_point.data = (UA_Byte *)continuation_point;
  browse_result = UA_Client_browseNext(
      client->client, release_continuation_point ? true : false,
      native_continuation_point);
  result = cpkt_browse_result_page(
      &browse_result, release_continuation_point ? NULL : fn, user,
      next_continuation_point_buffer, next_continuation_point_buffer_size,
      required_next_continuation_point_size_out, status_out);
  if (result == CPKT_OPCUA_ERR_RANGE) {
    cpkt_client_release_browse_continuation_point(
        client->client, browse_result.continuationPoint);
  }
  UA_BrowseResult_clear(&browse_result);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_translate_browse_path(
    cpkt_opcua_client *client, cpkt_opcua_node_id start_node_id,
    const cpkt_opcua_browse_path_element *elements, size_t element_count,
    cpkt_opcua_node_id *target_node_id_out, char *target_buffer,
    size_t target_buffer_size, size_t *required_target_size_out,
    cpkt_opcua_status *status_out) {
  UA_BrowsePath browse_path;
  UA_BrowsePathResult browse_path_result;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || target_node_id_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  result = cpkt_fill_browse_path(&browse_path, start_node_id, elements,
                                 element_count);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  browse_path_result =
      UA_Client_translateBrowsePathToNodeIds(client->client, &browse_path);
  result = cpkt_translate_browse_path_result_to_target(
      &browse_path_result, target_node_id_out, target_buffer,
      target_buffer_size, required_target_size_out, status_out);
  UA_BrowsePathResult_clear(&browse_path_result);
  UA_BrowsePath_clear(&browse_path);
  return result;
}
