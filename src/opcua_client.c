/* Handwritten OPC UA facade: client. */
#include "opcua_facade_internal.h"
#include <open62541/client_config_default.h>
#include <open62541/client_highlevel.h>
#include <stdlib.h>
#include <string.h>

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_new(cpkt_opcua_client **out) {
  return cpkt_opcua_client_new_with_logger(out, NULL);
}

/** Install logging before upstream defaults create dependent plugins. */
cpkt_opcua_result
cpkt_opcua_client_new_with_logger(cpkt_opcua_client **out,
                                  const cpkt_opcua_log_config *logger) {
  cpkt_opcua_client *client;
  UA_StatusCode status;
  UA_ClientConfig config;

  if (out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  *out = NULL;
  if (!cpkt_logger_valid(logger)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  client = (cpkt_opcua_client *)calloc(1, sizeof(*client));
  if (client == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  memset(&config, 0, sizeof(config));
  if (logger != NULL) {
    cpkt_logger_set(&client->logger, &client->logger.native, logger);
    config.logging = &client->logger.native;
  }
  status = UA_ClientConfig_setDefault(&config);
  if (status != UA_STATUSCODE_GOOD) {
    UA_ClientConfig_clear(&config);
    free(client);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  client->client = UA_Client_newWithConfig(&config);
  if (client->client == NULL) {
    UA_ClientConfig_clear(&config);
    free(client);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  status = UA_ClientConfig_setDefault(UA_Client_getConfig(client->client));
  if (status != UA_STATUSCODE_GOOD) {
    UA_Client_delete(client->client);
    free(client);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *out = client;
  return CPKT_OPCUA_OK;
}

/** Retain the logger address shared with upstream event-loop/security plugins.
 */
cpkt_opcua_result
cpkt_opcua_client_set_logger(cpkt_opcua_client *client,
                             const cpkt_opcua_log_config *logger) {
  UA_Logger *native;
  if (client == NULL || client->destroying || client->typed_config_depth ||
      logger == NULL || !cpkt_logger_valid(logger)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (client->typed_config)
    return cpkt_opcua_ClientConfig_setLogger(client->typed_config, logger) ==
                   UA_STATUSCODE_GOOD
               ? CPKT_OPCUA_OK
               : CPKT_OPCUA_ERR_UPSTREAM;
  native = UA_Client_getConfig(client->client)->logging;
  if (native == NULL) {
    native = &client->logger.native;
    UA_Client_getConfig(client->client)->logging = native;
  }
  ++client->typed_config_depth;
  cpkt_logger_set(&client->logger, native, logger);
  --client->typed_config_depth;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_client_free(cpkt_opcua_client *client) {
  struct cpkt_opcua_monitor_context *monitor;
  struct cpkt_opcua_monitor_context *next;
  struct cpkt_opcua_async_context *async_context;
  struct cpkt_opcua_async_context *async_next;
  UA_EventLoop *external_loop = NULL;

  if (client == NULL || client->destroying || client->borrowed) {
    return;
  }
  if (client->typed_config_depth) {
    if (client->client && !client->destruction_log_active) {
      client->destruction_log_active = 1;
      UA_LOG_ERROR(UA_Client_getConfig(client->client)->logging,
                   UA_LOGCATEGORY_CLIENT,
                   "Cannot destroy a client during an active configuration "
                   "callback frame");
      client->destruction_log_active = 0;
    }
    return;
  }
  client->destroying = 1;
  if (client->client != NULL) {
    if (UA_Client_getConfig(client->client)->externalEventLoop)
      external_loop = UA_Client_getConfig(client->client)->eventLoop;
    if (client->typed_eventloop_forget_config)
      client->typed_eventloop_forget_config(
          &UA_Client_getConfig(client->client)->eventLoop);
    UA_Client_delete(client->client);
    client->client = NULL;
    if (client->typed_callbacks_clear)
      client->typed_callbacks_clear(client, external_loop);
  }
  monitor = client->monitors;
  while (monitor != NULL) {
    next = monitor->next;
    free(monitor);
    monitor = next;
  }
  async_context = client->asyncs;
  while (async_context != NULL) {
    async_next = async_context->next;
    free(async_context);
    async_context = async_next;
  }
  if (client->typed_config_clear)
    client->typed_config_clear(client);
  free(client);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_connect(cpkt_opcua_client *client,
                                            const char *endpoint_url,
                                            cpkt_opcua_status *status_out) {
  UA_StatusCode status;
  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || endpoint_url == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  status = UA_Client_connect(client->client, endpoint_url);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_connect_username(
    cpkt_opcua_client *client, const char *endpoint_url, const char *username,
    const char *password, cpkt_opcua_status *status_out) {
  UA_StatusCode status;
  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || endpoint_url == NULL ||
      username == NULL || password == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  UA_Client_getConfig(client->client)->allowNonePolicyPassword = true;
  status = UA_Client_connectUsername(client->client, endpoint_url, username,
                                     password);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result
cpkt_opcua_client_native_config(cpkt_opcua_client *client,
                                cpkt_opcua_client_native_config_fn fn,
                                void *user, cpkt_opcua_status *status_out) {
  cpkt_opcua_status callback_status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || fn == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  callback_status = fn(UA_Client_getConfig(client->client), user);
  if (status_out != NULL) {
    *status_out = callback_status;
  }
  return callback_status == 0 ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_security_plugin_native_config(
    cpkt_opcua_client *client, cpkt_opcua_client_native_config_fn fn,
    void *user, cpkt_opcua_status *status_out) {
  return cpkt_opcua_client_native_config(client, fn, user, status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_set_default_encryption(
    cpkt_opcua_client *client, const unsigned char *certificate,
    size_t certificate_length, const unsigned char *private_key,
    size_t private_key_length, const cpkt_opcua_byte_string_view *trust_list,
    size_t trust_list_count, const cpkt_opcua_byte_string_view *revocation_list,
    size_t revocation_list_count, cpkt_opcua_status *status_out) {
  UA_StatusCode status;
#ifdef UA_ENABLE_ENCRYPTION
  UA_ByteString native_certificate;
  UA_ByteString native_private_key;
  UA_ByteString *native_trust_list;
  UA_ByteString *native_revocation_list;
  cpkt_opcua_result result;

  native_trust_list = NULL;
  native_revocation_list = NULL;
#endif

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL || certificate == NULL ||
      certificate_length == 0 || private_key == NULL ||
      private_key_length == 0) {
    return CPKT_OPCUA_ERR_ARG;
  }
#ifndef UA_ENABLE_ENCRYPTION
  status = UA_STATUSCODE_BADNOTSUPPORTED;
#else
  native_certificate =
      cpkt_make_borrowed_byte_string(certificate, certificate_length);
  native_private_key =
      cpkt_make_borrowed_byte_string(private_key, private_key_length);
  result = cpkt_make_borrowed_byte_string_array(trust_list, trust_list_count,
                                                &native_trust_list);
  if (result == CPKT_OPCUA_OK) {
    result = cpkt_make_borrowed_byte_string_array(
        revocation_list, revocation_list_count, &native_revocation_list);
  }
  if (result != CPKT_OPCUA_OK) {
    free(native_trust_list);
    free(native_revocation_list);
    return result;
  }
  status = UA_ClientConfig_setDefaultEncryption(
      UA_Client_getConfig(client->client), native_certificate,
      native_private_key, native_trust_list, trust_list_count,
      native_revocation_list, revocation_list_count);
  free(native_trust_list);
  free(native_revocation_list);
#endif
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_disconnect(cpkt_opcua_client *client,
                                               cpkt_opcua_status *status_out) {
  UA_StatusCode status;
  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  status = UA_Client_disconnect(client->client);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_run_iterate(cpkt_opcua_client *client,
                                                unsigned long timeout_ms,
                                                cpkt_opcua_status *status_out) {
  UA_StatusCode status;
  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      timeout_ms > (unsigned long)UINT_MAX) {
    return CPKT_OPCUA_ERR_ARG;
  }
  status = UA_Client_run_iterate(client->client, (UA_UInt32)timeout_ms);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_get_namespace_index(
    cpkt_opcua_client *client, const char *namespace_uri,
    unsigned short *namespace_index_out, cpkt_opcua_status *status_out) {
  UA_String native_namespace_uri;
  UA_UInt16 namespace_index;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (namespace_index_out != NULL) {
    *namespace_index_out = 0;
  }
  if (client == NULL || client->client == NULL || namespace_uri == NULL ||
      namespace_index_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_namespace_uri = UA_STRING((char *)namespace_uri);
  status = UA_Client_getNamespaceIndex(client->client, native_namespace_uri,
                                       &namespace_index);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *namespace_index_out = (unsigned short)namespace_index;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_get_namespace_uri(
    cpkt_opcua_client *client, unsigned short namespace_index, char *buffer,
    size_t buffer_size, size_t *required_size_out,
    cpkt_opcua_status *status_out) {
  UA_String namespace_uri;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (client == NULL || client->client == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  UA_String_init(&namespace_uri);
  status = UA_Client_getNamespaceUri(client->client, (UA_UInt16)namespace_index,
                                     &namespace_uri);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_String_clear(&namespace_uri);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  result = cpkt_copy_ua_string_to_buffer(&namespace_uri, buffer, buffer_size,
                                         required_size_out);
  UA_String_clear(&namespace_uri);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_get_endpoint_count(
    cpkt_opcua_client *client, const char *server_url,
    size_t *endpoint_count_out, cpkt_opcua_status *status_out) {
  UA_EndpointDescription *endpoints;
  size_t endpoint_count;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (endpoint_count_out != NULL) {
    *endpoint_count_out = 0;
  }
  if (client == NULL || client->client == NULL || server_url == NULL ||
      endpoint_count_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  endpoints = NULL;
  endpoint_count = 0;
  status = UA_Client_getEndpoints(client->client, server_url, &endpoint_count,
                                  &endpoints);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_Array_delete(endpoints, endpoint_count,
                    &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *endpoint_count_out = endpoint_count;
  UA_Array_delete(endpoints, endpoint_count,
                  &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_get_endpoint_url(
    cpkt_opcua_client *client, const char *server_url, size_t endpoint_index,
    char *buffer, size_t buffer_size, size_t *required_size_out,
    cpkt_opcua_status *status_out) {
  UA_EndpointDescription *endpoints;
  size_t endpoint_count;
  UA_StatusCode status;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  if (client == NULL || client->client == NULL || server_url == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  endpoints = NULL;
  endpoint_count = 0;
  status = UA_Client_getEndpoints(client->client, server_url, &endpoint_count,
                                  &endpoints);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_Array_delete(endpoints, endpoint_count,
                    &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (endpoint_index >= endpoint_count) {
    UA_Array_delete(endpoints, endpoint_count,
                    &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);
    return CPKT_OPCUA_ERR_RANGE;
  }
  result =
      cpkt_copy_ua_string_to_buffer(&endpoints[endpoint_index].endpointUrl,
                                    buffer, buffer_size, required_size_out);
  UA_Array_delete(endpoints, endpoint_count,
                  &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);
  return result;
}

static cpkt_opcua_result cpkt_client_find_servers(
    cpkt_opcua_client *client, const char *server_url, size_t *server_count_out,
    UA_ApplicationDescription **servers_out, cpkt_opcua_status *status_out) {
  UA_ApplicationDescription *servers;
  size_t server_count;
  UA_StatusCode status;

  if (server_count_out != NULL) {
    *server_count_out = 0;
  }
  if (servers_out != NULL) {
    *servers_out = NULL;
  }
  if (client == NULL || client->client == NULL || server_url == NULL ||
      server_count_out == NULL || servers_out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  servers = NULL;
  server_count = 0;
  status = UA_Client_findServers(client->client, server_url, 0, NULL, 0, NULL,
                                 &server_count, &servers);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    UA_Array_delete(servers, server_count,
                    &UA_TYPES[UA_TYPES_APPLICATIONDESCRIPTION]);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *server_count_out = server_count;
  *servers_out = servers;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_find_server_count(
    cpkt_opcua_client *client, const char *server_url, size_t *server_count_out,
    cpkt_opcua_status *status_out) {
  UA_ApplicationDescription *servers;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  result = cpkt_client_find_servers(client, server_url, server_count_out,
                                    &servers, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  UA_Array_delete(servers, *server_count_out,
                  &UA_TYPES[UA_TYPES_APPLICATIONDESCRIPTION]);
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_find_server_application_uri(
    cpkt_opcua_client *client, const char *server_url, size_t server_index,
    char *buffer, size_t buffer_size, size_t *required_size_out,
    cpkt_opcua_status *status_out) {
  UA_ApplicationDescription *servers;
  size_t server_count;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  result = cpkt_client_find_servers(client, server_url, &server_count, &servers,
                                    status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  if (server_index >= server_count) {
    UA_Array_delete(servers, server_count,
                    &UA_TYPES[UA_TYPES_APPLICATIONDESCRIPTION]);
    return CPKT_OPCUA_ERR_RANGE;
  }
  result =
      cpkt_copy_ua_string_to_buffer(&servers[server_index].applicationUri,
                                    buffer, buffer_size, required_size_out);
  UA_Array_delete(servers, server_count,
                  &UA_TYPES[UA_TYPES_APPLICATIONDESCRIPTION]);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_find_server_application_name(
    cpkt_opcua_client *client, const char *server_url, size_t server_index,
    char *buffer, size_t buffer_size, size_t *required_size_out,
    cpkt_opcua_status *status_out) {
  UA_ApplicationDescription *servers;
  size_t server_count;
  cpkt_opcua_result result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (required_size_out != NULL) {
    *required_size_out = 0;
  }
  result = cpkt_client_find_servers(client, server_url, &server_count, &servers,
                                    status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  if (server_index >= server_count) {
    UA_Array_delete(servers, server_count,
                    &UA_TYPES[UA_TYPES_APPLICATIONDESCRIPTION]);
    return CPKT_OPCUA_ERR_RANGE;
  }
  result =
      cpkt_copy_ua_string_to_buffer(&servers[server_index].applicationName.text,
                                    buffer, buffer_size, required_size_out);
  UA_Array_delete(servers, server_count,
                  &UA_TYPES[UA_TYPES_APPLICATIONDESCRIPTION]);
  return result;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_native(cpkt_opcua_client *client,
                                           cpkt_opcua_client_native_fn fn,
                                           void *user) {
  if (client == NULL || client->client == NULL || fn == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  return fn(client->client, user) == 0 ? CPKT_OPCUA_OK
                                       : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_async_native(cpkt_opcua_client *client,
                                                 cpkt_opcua_client_native_fn fn,
                                                 void *user) {
  return cpkt_opcua_client_native(client, fn, user);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result
cpkt_opcua_client_history_native(cpkt_opcua_client *client,
                                 cpkt_opcua_client_native_fn fn, void *user) {
  return cpkt_opcua_client_native(client, fn, user);
}
