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
  void *owned_context;
  void (*owned_clear)(void *);
};
struct cpkt_opcua_client {
  UA_Client *client;
  int borrowed;
  int destroying;
  int destruction_log_active;
  void (*typed_eventloop_forget_config)(UA_EventLoop **);
  unsigned int typed_config_depth;
  struct cpkt_opcua_ClientConfig *typed_config;
  void (*typed_config_clear)(cpkt_opcua_client *);
  struct cpkt_opcua_logger logger;
  struct cpkt_opcua_monitor_context *monitors;
  struct cpkt_opcua_async_context *asyncs;
  struct cpkt_typed_subscription *typed_subscriptions;
  struct cpkt_typed_monitor *typed_monitors;
  struct cpkt_client_timer *typed_timers;
  void (*typed_callbacks_clear)(cpkt_opcua_client *, UA_EventLoop *);
};
struct cpkt_opcua_server {
  UA_Server *server;
  unsigned int typed_config_depth;
  struct cpkt_opcua_ServerConfig *typed_config;
  void (*typed_config_clear)(cpkt_opcua_server *);
  struct cpkt_opcua_logger logger;
  unsigned short port;
  int started;
  int destroying;
  int destruction_log_active;
  void (*typed_eventloop_forget_config)(UA_EventLoop **);
  unsigned int typed_loop_depth;
  struct cpkt_opcua_server *typed_loop_frame_next;
  int typed_loop_pubsub_held;
  struct cpkt_opcua_method_context *methods;
  struct cpkt_gather_bridge *typed_history;
  void (*history_prepare_delete)(cpkt_opcua_server *);
  struct cpkt_server_timer *typed_timers;
  void (*typed_callbacks_clear)(cpkt_opcua_server *);
  struct cpkt_server_async *typed_asyncs;
  struct cpkt_nodes_owner *typed_nodes;
  void (*typed_nodes_clear)(cpkt_opcua_server *);
  void (*typed_nodes_set_deleting)(cpkt_opcua_server *, int);
  void (*typed_nodes_refresh_eventloop)(cpkt_opcua_server *);
  int (*typed_eventloop_active)(cpkt_opcua_server *);
  void (*typed_producers_refresh)(cpkt_opcua_server *);
  UA_StatusCode (*typed_pubsub_prepare)(cpkt_opcua_server *);
  void (*typed_pubsub_retain)(cpkt_opcua_server *);
  void (*typed_pubsub_finish)(cpkt_opcua_server *);
  char *endpoint_hostname;
  char *access_username;
  unsigned char *access_password;
  cpkt_opcua_login_fn access_login_fn;
  void *access_login_user;
};
#endif
