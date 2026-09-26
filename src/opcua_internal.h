#ifndef CPKT_OPCUA_INTERNAL_H
#define CPKT_OPCUA_INTERNAL_H
#include <cpkt/opcua.h>
#include <open62541/client.h>
#include <open62541/server.h>

/* Shared private handle layout. Public headers keep both handles opaque. */
struct cpkt_opcua_logger {
  UA_Logger native;
  UA_Logger original;
  cpkt_opcua_log_config config;
  int configured;
};
struct cpkt_opcua_client {
  UA_Client *client;
  struct cpkt_opcua_logger logger;
  struct cpkt_opcua_monitor_context *monitors;
  struct cpkt_opcua_async_context *asyncs;
  struct cpkt_typed_subscription *typed_subscriptions;
  struct cpkt_typed_monitor *typed_monitors;
};
struct cpkt_opcua_server {
  UA_Server *server;
  struct cpkt_opcua_logger logger;
  unsigned short port;
  int started;
  int destroying;
  struct cpkt_opcua_method_context *methods;
  struct cpkt_gather_bridge *typed_history;
  void (*history_prepare_delete)(cpkt_opcua_server *);
  struct cpkt_server_timer *typed_timers;
  void (*typed_callbacks_clear)(cpkt_opcua_server *);
  struct cpkt_server_async *typed_asyncs;
  char *endpoint_hostname;
  char *access_username;
  unsigned char *access_password;
  cpkt_opcua_login_fn access_login_fn;
  void *access_login_user;
};
#endif
