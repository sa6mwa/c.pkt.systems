#ifndef CPKT_OPCUA_TYPES_PEER_H
#define CPKT_OPCUA_TYPES_PEER_H
#include <stddef.h>
unsigned int cpkt_types_native_empty(size_t index, unsigned char **bytes,
                                     size_t *length);
unsigned int cpkt_types_native_populated(size_t index, unsigned char **bytes,
                                         size_t *length);
void cpkt_types_native_free(unsigned char *bytes);
void *cpkt_types_peer_start(unsigned short *port);
int cpkt_types_peer_check(void *peer);
unsigned int cpkt_types_peer_event(void *peer);
unsigned int cpkt_types_peer_access_control(void *native_server);
unsigned int cpkt_types_peer_history(void *native_server);
unsigned int cpkt_types_peer_history_failure(void *native_server);
unsigned int cpkt_types_peer_close_session_failure(void *native_server,
                                                   void *context);
unsigned int cpkt_types_peer_history_backend(void *native_server, int mode);
unsigned int cpkt_types_peer_history_poll_node(void *native_server);
struct cpkt_async_peer {
  int deferred;
  int cancelled;
  void *read_result;
  const void *write_result;
  void *call_result;
};
unsigned int cpkt_types_peer_async_install(void *server, void *state);
unsigned int cpkt_types_peer_async_complete(void *server, void *state);
void cpkt_types_peer_stop(void *peer);
#endif
