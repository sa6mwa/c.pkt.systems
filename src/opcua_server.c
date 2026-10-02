/* Handwritten OPC UA facade: server. */
#include "opcua_facade_internal.h"
#include <open62541/plugin/accesscontrol_default.h>
#include <open62541/server_config_default.h>
#include <open62541/server_config_file_based.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_new(cpkt_opcua_server **out,
                                        unsigned short port) {
  return cpkt_opcua_server_new_with_logger(out, port, NULL);
}

/** Install the logger before upstream defaults and server initialization. */
cpkt_opcua_result
cpkt_opcua_server_new_with_logger(cpkt_opcua_server **out, unsigned short port,
                                  const cpkt_opcua_log_config *logger) {
  cpkt_opcua_server *server;
  UA_StatusCode status;
  UA_ServerConfig config;

  if (out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  *out = NULL;
  if (!cpkt_logger_valid(logger)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  server = (cpkt_opcua_server *)calloc(1, sizeof(*server));
  if (server == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  memset(&config, 0, sizeof(config));
  if (logger != NULL) {
    cpkt_logger_set(&server->logger, &server->logger.native, logger);
    config.logging = &server->logger.native;
  }
  status = UA_ServerConfig_setDefault(&config);
  if (status != UA_STATUSCODE_GOOD) {
    UA_ServerConfig_clear(&config);
    free(server);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  server->server = UA_Server_newWithConfig(&config);
  if (server->server == NULL) {
    free(server);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  status = UA_ServerConfig_setMinimal(UA_Server_getConfig(server->server),
                                      (UA_UInt16)port, NULL);
  if (status != UA_STATUSCODE_GOOD) {
    UA_Server_delete(server->server);
    free(server);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  server->port = port;
  *out = server;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result
cpkt_opcua_server_new_from_json(cpkt_opcua_server **out,
                                const unsigned char *json, size_t json_length,
                                cpkt_opcua_status *status_out) {
  return cpkt_opcua_server_new_from_json_with_logger(out, json, json_length,
                                                     NULL, status_out);
}

/** Preserve upstream's JSON construction sequence with an early logger. */
cpkt_opcua_result cpkt_opcua_server_new_from_json_with_logger(
    cpkt_opcua_server **out, const unsigned char *json, size_t json_length,
    const cpkt_opcua_log_config *logger, cpkt_opcua_status *status_out) {
  UA_ByteString json_config;
  UA_ServerConfig config;
  cpkt_opcua_server *server;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  *out = NULL;
  if (json == NULL || json_length == 0 || !cpkt_logger_valid(logger)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  server = (cpkt_opcua_server *)calloc(1, sizeof(*server));
  if (server == NULL) {
    if (status_out != NULL) {
      *status_out = (cpkt_opcua_status)UA_STATUSCODE_BADOUTOFMEMORY;
    }
    return CPKT_OPCUA_ERR_ALLOC;
  }
  memset(&config, 0, sizeof(config));
  if (logger != NULL) {
    cpkt_logger_set(&server->logger, &server->logger.native, logger);
    config.logging = &server->logger.native;
  }
  json_config.length = json_length;
  json_config.data = (UA_Byte *)(const void *)json;
  status = UA_ServerConfig_setDefault(&config);
  status |= UA_ServerConfig_updateFromFile(&config, json_config);
  if (status != UA_STATUSCODE_GOOD) {
    UA_ServerConfig_clear(&config);
    free(server);
    if (status_out != NULL) {
      *status_out = cpkt_status(UA_STATUSCODE_BADCONFIGURATIONERROR);
    }
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  server->server = UA_Server_newWithConfig(&config);
  if (server->server == NULL) {
    free(server);
    if (status_out != NULL) {
      *status_out = cpkt_status(UA_STATUSCODE_BADCONFIGURATIONERROR);
    }
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  *out = server;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result
cpkt_opcua_server_new_from_json_file(cpkt_opcua_server **out, const char *path,
                                     cpkt_opcua_status *status_out) {
  return cpkt_opcua_server_new_from_json_file_with_logger(out, path, NULL,
                                                          status_out);
}

/** Read the named JSON file and use the logger-aware bytes constructor. */
cpkt_opcua_result cpkt_opcua_server_new_from_json_file_with_logger(
    cpkt_opcua_server **out, const char *path,
    const cpkt_opcua_log_config *logger, cpkt_opcua_status *status_out) {
  UA_ByteString json_config;
  cpkt_opcua_result result;

  if (out == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  *out = NULL;
  result = cpkt_read_file_bytes(path, &json_config, status_out);
  if (result != CPKT_OPCUA_OK) {
    return result;
  }
  result = cpkt_opcua_server_new_from_json_with_logger(
      out, json_config.data, json_config.length, logger, status_out);
  cpkt_byte_string_clear_malloc(&json_config);
  return result;
}

/** Update the existing plugin in place so every upstream reference follows it.
 */
cpkt_opcua_result
cpkt_opcua_server_set_logger(cpkt_opcua_server *server,
                             const cpkt_opcua_log_config *logger) {
  UA_Logger *native;
  if (server == NULL || server->destroying || server->typed_config_depth ||
      logger == NULL || !cpkt_logger_valid(logger)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (server->typed_config)
    return cpkt_opcua_ServerConfig_setLogger(server->typed_config, logger) ==
                   UA_STATUSCODE_GOOD
               ? CPKT_OPCUA_OK
               : CPKT_OPCUA_ERR_UPSTREAM;
  native = UA_Server_getConfig(server->server)->logging;
  if (native == NULL) {
    native = &server->logger.native;
    UA_Server_getConfig(server->server)->logging = native;
  }
  ++server->typed_config_depth;
  cpkt_logger_set(&server->logger, native, logger);
  --server->typed_config_depth;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
static UA_StatusCode cpkt_server_delete(cpkt_opcua_server *server,
                                        int shutdown) {
  struct cpkt_opcua_method_context *method;
  struct cpkt_opcua_method_context *next;
  UA_StatusCode status = 0;

  if (server == NULL) {
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  }
  if (server->destroying)
    return UA_STATUSCODE_BADINVALIDSTATE;
  if (server->typed_loop_depth || server->typed_config_depth ||
      (server->typed_eventloop_active &&
       server->typed_eventloop_active(server))) {
    if (server->server && !server->destruction_log_active) {
      server->destruction_log_active = 1;
      UA_LOG_ERROR(UA_Server_getConfig(server->server)->logging,
                   UA_LOGCATEGORY_SERVER,
                   "Cannot destroy a server during an active configuration or "
                   "EventLoop callback frame");
      server->destruction_log_active = 0;
    }
    return UA_STATUSCODE_BADINVALIDSTATE;
  }
  if (server->typed_nodes_set_deleting)
    server->typed_nodes_set_deleting(server, 1);
  else
    server->destroying = 1;
  if (server->server != NULL) {
    /* Hold dispatch metadata through shutdown and native node deletion. The
     * final owner cleanup releases it after UA_Server_delete; do not query a
     * destroyed native server to prune obsolete PubSub entries. */
    if (server->typed_pubsub_retain)
      server->typed_pubsub_retain(server);
    if (shutdown && UA_Server_getLifecycleState(server->server) ==
                        UA_LIFECYCLESTATE_STARTED)
      status = UA_Server_run_shutdown(server->server);
    if (status || UA_Server_getLifecycleState(server->server) !=
                      UA_LIFECYCLESTATE_STOPPED) {
      /* External loops finish shutdown asynchronously. Native delete requires
       * STOPPED; retain the live facade and its metadata for caller iteration.
       */
      UA_LOG_ERROR(
          UA_Server_getConfig(server->server)->logging, UA_LOGCATEGORY_SERVER,
          "Server deletion requires completed native shutdown: %08lx",
          (unsigned long)(status ? status : UA_STATUSCODE_BADINVALIDSTATE));
      if (server->typed_pubsub_finish)
        server->typed_pubsub_finish(server);
      if (server->typed_nodes_set_deleting)
        server->typed_nodes_set_deleting(server, 0);
      else
        server->destroying = 0;
      return status ? status : UA_STATUSCODE_BADINTERNALERROR;
    }
    if (server->history_prepare_delete)
      server->history_prepare_delete(server);
    if (server->typed_callbacks_clear)
      server->typed_callbacks_clear(server);
    if (server->typed_eventloop_forget_config)
      server->typed_eventloop_forget_config(
          &UA_Server_getConfig(server->server)->eventLoop);
    status = UA_Server_delete(server->server);
    if (status) {
      UA_LOG_ERROR(
          UA_Server_getConfig(server->server)->logging, UA_LOGCATEGORY_SERVER,
          "Native server deletion failed: %08lx", (unsigned long)status);
      if (server->typed_pubsub_finish)
        server->typed_pubsub_finish(server);
      if (server->typed_nodes_set_deleting)
        server->typed_nodes_set_deleting(server, 0);
      else
        server->destroying = 0;
      return status ? status : UA_STATUSCODE_BADINTERNALERROR;
    }
    if (server->typed_nodes_clear)
      server->typed_nodes_clear(server);
  }
  method = server->methods;
  while (method != NULL) {
    next = method->next;
    free(method->input_types);
    free(method->output_types);
    free(method);
    method = next;
  }
  free(server->access_username);
  free(server->access_password);
  free(server->endpoint_hostname);
  if (server->typed_config_clear)
    server->typed_config_clear(server);
  free(server);
  return UA_STATUSCODE_GOOD;
}

/** Report native deletion status; the caller must finish shutdown first. */
cpkt_opcua_StatusCode
cpkt_opcua_server_delete_typed(cpkt_opcua_server *server) {
  return cpkt_server_delete(server, 0);
}

/** Request native shutdown and release the server once it is stopped. */
void cpkt_opcua_server_free(cpkt_opcua_server *server) {
  (void)cpkt_server_delete(server, 1);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_startup(cpkt_opcua_server *server,
                                            cpkt_opcua_status *status_out) {
  UA_StatusCode status;
  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (server->started) {
    return CPKT_OPCUA_OK;
  }
  status =
      server->typed_pubsub_prepare ? server->typed_pubsub_prepare(server) : 0;
  if (!status) {
    status = UA_Server_run_startup(server->server);
    if (server->typed_pubsub_finish)
      server->typed_pubsub_finish(server);
  }
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  server->started = 1;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_iterate(cpkt_opcua_server *server,
                                            int wait_internal,
                                            unsigned short *wait_ms_out) {
  UA_UInt16 wait_ms;
  if (server == NULL || server->server == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (server->typed_pubsub_prepare && server->typed_pubsub_prepare(server))
    return CPKT_OPCUA_ERR_UPSTREAM;
  wait_ms = UA_Server_run_iterate(server->server, wait_internal ? true : false);
  if (server->typed_pubsub_finish)
    server->typed_pubsub_finish(server);
  if (server->typed_producers_refresh)
    server->typed_producers_refresh(server);
  if (wait_ms_out != NULL) {
    *wait_ms_out = (unsigned short)wait_ms;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_shutdown(cpkt_opcua_server *server,
                                             cpkt_opcua_status *status_out) {
  UA_StatusCode status;
  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  if (!server->started) {
    return CPKT_OPCUA_OK;
  }
  status = UA_Server_run_shutdown(server->server);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  server->started = 0;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result
cpkt_opcua_server_endpoint_url(const cpkt_opcua_server *server, char *buffer,
                               size_t buffer_size, size_t *required_size_out) {
  char port_text[6];
  const char *hostname;
  int written;
  size_t required;

  if (server == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  hostname = server->endpoint_hostname != NULL ? server->endpoint_hostname
                                               : "127.0.0.1";
  written =
      snprintf(port_text, sizeof(port_text), "%u", (unsigned)server->port);
  if (written < 0 || (size_t)written >= sizeof(port_text)) {
    return CPKT_OPCUA_ERR_RANGE;
  }
  required =
      strlen("opc.tcp://") + strlen(hostname) + 1 + strlen(port_text) + 1;
  if (required_size_out != NULL) {
    *required_size_out = required;
  }
  if (buffer == NULL || buffer_size < required) {
    if (buffer != NULL && buffer_size != 0) {
      buffer[0] = '\0';
    }
    return CPKT_OPCUA_ERR_RANGE;
  }
  written =
      snprintf(buffer, buffer_size, "opc.tcp://%s:%s", hostname, port_text);
  if (written < 0 || (size_t)written >= buffer_size) {
    buffer[0] = '\0';
    return CPKT_OPCUA_ERR_RANGE;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_set_endpoint(cpkt_opcua_server *server,
                                                 const char *hostname,
                                                 unsigned short port) {
  UA_ServerConfig *config;
  UA_String *server_urls;
  UA_String server_url;
  char *hostname_copy;
  char *endpoint;
  size_t hostname_length;
  size_t endpoint_length;
  int written;

  if (server == NULL || server->server == NULL || server->started ||
      hostname == NULL || hostname[0] == '\0') {
    return CPKT_OPCUA_ERR_ARG;
  }
  hostname_length = strlen(hostname);
  endpoint_length =
      strlen("opc.tcp://") + hostname_length + strlen(":65535") + 1;
  endpoint = (char *)calloc(endpoint_length, 1);
  if (endpoint == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  written = snprintf(endpoint, endpoint_length, "opc.tcp://%s:%u", hostname,
                     (unsigned)port);
  if (written < 0 || (size_t)written >= endpoint_length) {
    free(endpoint);
    return CPKT_OPCUA_ERR_RANGE;
  }
  hostname_copy = cpkt_strdup_c89(hostname);
  if (hostname_copy == NULL) {
    free(endpoint);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  server_url = UA_STRING_ALLOC(endpoint);
  free(endpoint);
  if (server_url.data == NULL) {
    free(hostname_copy);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  server_urls = (UA_String *)UA_Array_new(1, &UA_TYPES[UA_TYPES_STRING]);
  if (server_urls == NULL) {
    UA_String_clear(&server_url);
    free(hostname_copy);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  server_urls[0] = server_url;
  config = UA_Server_getConfig(server->server);
  UA_Array_delete(config->serverUrls, config->serverUrlsSize,
                  &UA_TYPES[UA_TYPES_STRING]);
  config->serverUrls = server_urls;
  config->serverUrlsSize = 1;
  free(server->endpoint_hostname);
  server->endpoint_hostname = hostname_copy;
  server->port = port;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_set_application_identity(
    cpkt_opcua_server *server, const char *application_uri,
    const char *product_uri, const char *application_name) {
  UA_ServerConfig *config;
  UA_String native_application_uri;
  UA_String native_product_uri;
  UA_LocalizedText native_application_name;

  if (server == NULL || server->server == NULL || server->started ||
      application_uri == NULL || product_uri == NULL ||
      application_name == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_application_uri = UA_STRING_ALLOC((char *)application_uri);
  native_product_uri = UA_STRING_ALLOC((char *)product_uri);
  native_application_name =
      UA_LOCALIZEDTEXT_ALLOC((char *)"en-US", (char *)application_name);
  if ((application_uri[0] != '\0' && native_application_uri.data == NULL) ||
      (product_uri[0] != '\0' && native_product_uri.data == NULL) ||
      (application_name[0] != '\0' &&
       native_application_name.text.data == NULL)) {
    UA_String_clear(&native_application_uri);
    UA_String_clear(&native_product_uri);
    UA_LocalizedText_clear(&native_application_name);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  config = UA_Server_getConfig(server->server);
  UA_String_clear(&config->applicationDescription.applicationUri);
  UA_String_clear(&config->applicationDescription.productUri);
  UA_LocalizedText_clear(&config->applicationDescription.applicationName);
  config->applicationDescription.applicationUri = native_application_uri;
  config->applicationDescription.productUri = native_product_uri;
  config->applicationDescription.applicationName = native_application_name;
  config->applicationDescription.applicationType = UA_APPLICATIONTYPE_SERVER;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result
cpkt_opcua_server_native_config(cpkt_opcua_server *server,
                                cpkt_opcua_server_native_config_fn fn,
                                void *user, cpkt_opcua_status *status_out) {
  cpkt_opcua_status callback_status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || server->started ||
      fn == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  callback_status = fn(UA_Server_getConfig(server->server), user);
  if (server->typed_nodes_refresh_eventloop)
    server->typed_nodes_refresh_eventloop(server);
  if (status_out != NULL) {
    *status_out = callback_status;
  }
  return callback_status == 0 ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_file_config_native_config(
    cpkt_opcua_server *server, cpkt_opcua_server_native_config_fn fn,
    void *user, cpkt_opcua_status *status_out) {
  return cpkt_opcua_server_native_config(server, fn, user, status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_security_plugin_native_config(
    cpkt_opcua_server *server, cpkt_opcua_server_native_config_fn fn,
    void *user, cpkt_opcua_status *status_out) {
  return cpkt_opcua_server_native_config(server, fn, user, status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_set_default_security(
    cpkt_opcua_server *server, int secure_only,
    const unsigned char *certificate, size_t certificate_length,
    const unsigned char *private_key, size_t private_key_length,
    const cpkt_opcua_byte_string_view *trust_list, size_t trust_list_count,
    const cpkt_opcua_byte_string_view *issuer_list, size_t issuer_list_count,
    const cpkt_opcua_byte_string_view *revocation_list,
    size_t revocation_list_count, cpkt_opcua_status *status_out) {
  UA_StatusCode status;
#ifdef UA_ENABLE_ENCRYPTION
  UA_ByteString native_certificate;
  UA_ByteString native_private_key;
  UA_ByteString *native_trust_list;
  UA_ByteString *native_issuer_list;
  UA_ByteString *native_revocation_list;
  cpkt_opcua_result result;

  native_trust_list = NULL;
  native_issuer_list = NULL;
  native_revocation_list = NULL;
#endif

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || server->started ||
      certificate == NULL || certificate_length == 0 || private_key == NULL ||
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
        issuer_list, issuer_list_count, &native_issuer_list);
  }
  if (result == CPKT_OPCUA_OK) {
    result = cpkt_make_borrowed_byte_string_array(
        revocation_list, revocation_list_count, &native_revocation_list);
  }
  if (result != CPKT_OPCUA_OK) {
    free(native_trust_list);
    free(native_issuer_list);
    free(native_revocation_list);
    return result;
  }
  if (secure_only) {
    status = UA_ServerConfig_setDefaultWithSecureSecurityPolicies(
        UA_Server_getConfig(server->server), (UA_UInt16)server->port,
        &native_certificate, &native_private_key, native_trust_list,
        trust_list_count, native_issuer_list, issuer_list_count,
        native_revocation_list, revocation_list_count);
  } else {
    status = UA_ServerConfig_setDefaultWithSecurityPolicies(
        UA_Server_getConfig(server->server), (UA_UInt16)server->port,
        &native_certificate, &native_private_key, native_trust_list,
        trust_list_count, native_issuer_list, issuer_list_count,
        native_revocation_list, revocation_list_count);
  }
  free(native_trust_list);
  free(native_issuer_list);
  free(native_revocation_list);
  /* Upstream clears its configuration on some failures. Keep the selected
   * destination available for cleanup and the next configuration attempt. */
  if (server->logger.configured &&
      UA_Server_getConfig(server->server)->logging == NULL) {
    cpkt_logger_set(&server->logger, &server->logger.native,
                    &server->logger.config);
    UA_Server_getConfig(server->server)->logging = &server->logger.native;
  }
#endif
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_set_access_control(
    cpkt_opcua_server *server, int allow_anonymous, const char *username,
    const char *password, cpkt_opcua_status *status_out) {
  UA_ServerConfig *config;
  UA_UsernamePasswordLogin login;
  char *username_copy;
  unsigned char *password_copy;
  size_t password_length;
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || server->started ||
      ((username == NULL) != (password == NULL))) {
    return CPKT_OPCUA_ERR_ARG;
  }
  username_copy = NULL;
  password_copy = NULL;
  password_length = 0;
  if (username != NULL) {
    password_length = strlen(password);
    username_copy = cpkt_strdup_c89(username);
    password_copy =
        cpkt_memdup_c89((const unsigned char *)password, password_length);
    if (username_copy == NULL ||
        (password_length != 0 && password_copy == NULL)) {
      free(username_copy);
      free(password_copy);
      return CPKT_OPCUA_ERR_ALLOC;
    }
  }

  config = UA_Server_getConfig(server->server);
  login.username = UA_STRING_NULL;
  login.password = UA_BYTESTRING_NULL;
  if (username_copy != NULL) {
    login.username = UA_STRING(username_copy);
    login.password.data = (UA_Byte *)password_copy;
    login.password.length = password_length;
  }
  config->allowNonePolicyPassword = username_copy != NULL ? true : false;
  status = UA_AccessControl_default(config, allow_anonymous ? true : false,
                                    NULL, username_copy != NULL ? 1 : 0,
                                    username_copy != NULL ? &login : NULL);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    free(username_copy);
    free(password_copy);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  free(server->access_username);
  free(server->access_password);
  server->access_username = username_copy;
  server->access_password = password_copy;
  server->access_login_fn = NULL;
  server->access_login_user = NULL;
  return CPKT_OPCUA_OK;
}

static UA_StatusCode cpkt_opcua_access_control_login_callback(
    const UA_String *userName, const UA_ByteString *password,
    size_t usernamePasswordLoginSize,
    const UA_UsernamePasswordLogin *usernamePasswordLogin,
    void **sessionContext, void *loginContext) {
  cpkt_opcua_server *server;
  const char *username_data;
  const unsigned char *password_data;
  size_t username_length;
  size_t password_length;

  (void)usernamePasswordLoginSize;
  (void)usernamePasswordLogin;
  if (sessionContext != NULL) {
    *sessionContext = NULL;
  }
  server = (cpkt_opcua_server *)loginContext;
  if (server == NULL || server->access_login_fn == NULL) {
    return UA_STATUSCODE_BADUSERACCESSDENIED;
  }
  /* open62541 calls this for anonymous sessions only after checking
   * allowAnonymous. Empty username/password tokens are rejected upstream.
   * Keep the facade callback limited to username/password authentication. */
  if (userName != NULL && password != NULL && userName->length == 0 &&
      password->length == 0) {
    return UA_STATUSCODE_GOOD;
  }
  username_data = NULL;
  username_length = 0;
  if (userName != NULL) {
    username_data = (const char *)userName->data;
    username_length = userName->length;
  }
  password_data = NULL;
  password_length = 0;
  if (password != NULL) {
    password_data = (const unsigned char *)password->data;
    password_length = password->length;
  }
  return (UA_StatusCode)server->access_login_fn(username_data, username_length,
                                                password_data, password_length,
                                                server->access_login_user);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_set_access_control_callback(
    cpkt_opcua_server *server, int allow_anonymous, cpkt_opcua_login_fn fn,
    void *user, cpkt_opcua_status *status_out) {
  UA_ServerConfig *config;
  UA_UsernamePasswordLogin login;
  UA_StatusCode status;
  char callback_username[] = "cpkt-callback";
  unsigned char callback_password[] = "cpkt-callback";

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (server == NULL || server->server == NULL || server->started ||
      fn == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }

  login.username = UA_STRING(callback_username);
  login.password.data = (UA_Byte *)callback_password;
  login.password.length = strlen((const char *)callback_password);
  config = UA_Server_getConfig(server->server);
  config->allowNonePolicyPassword = true;
  status = UA_AccessControl_defaultWithLoginCallback(
      config, allow_anonymous ? true : false, NULL, 1, &login,
      cpkt_opcua_access_control_login_callback, server);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  if (status != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  free(server->access_username);
  free(server->access_password);
  server->access_username = NULL;
  server->access_password = NULL;
  server->access_login_fn = fn;
  server->access_login_user = user;
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_server_native(cpkt_opcua_server *server,
                                           cpkt_opcua_server_native_fn fn,
                                           void *user) {
  if (server == NULL || server->server == NULL || fn == NULL) {
    return CPKT_OPCUA_ERR_ARG;
  }
  return fn(server->server, user) == 0 ? CPKT_OPCUA_OK
                                       : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result
cpkt_opcua_server_pubsub_native(cpkt_opcua_server *server,
                                cpkt_opcua_server_native_fn fn, void *user) {
  return cpkt_opcua_server_native(server, fn, user);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result
cpkt_opcua_server_history_native(cpkt_opcua_server *server,
                                 cpkt_opcua_server_native_fn fn, void *user) {
  return cpkt_opcua_server_native(server, fn, user);
}
