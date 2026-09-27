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
unsigned int cpkt_types_peer_highlevel_setup(void *peer);
void cpkt_types_peer_highlevel_history_counts(void *peer, unsigned int *pages,
                                              unsigned int *releases);
unsigned int cpkt_types_peer_async_add_output(void *client);
unsigned int cpkt_types_peer_client_timeout(void *client, unsigned int timeout);
void cpkt_types_peer_pause(void *peer, int pause);
void cpkt_types_peer_client_fail_conversion(int kind, size_t count);
unsigned int cpkt_types_peer_event(void *peer);
unsigned int cpkt_types_peer_defaults(int monitored, unsigned char **bytes,
                                      size_t *length);
unsigned int cpkt_types_peer_single_error(void *client,
                                          unsigned int subscription_id,
                                          int events, int missing,
                                          unsigned int *deleted);
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
unsigned int cpkt_types_peer_variant_predicates(size_t type, size_t query,
                                                size_t length, int state);
unsigned int cpkt_types_peer_extension_predicate(int encoding, size_t type,
                                                 size_t query, int populated);
unsigned int cpkt_types_peer_bytes_hash(unsigned int initial,
                                        const unsigned char *bytes,
                                        size_t length);
unsigned int cpkt_types_peer_status_predicates(unsigned int status);
struct cpkt_identifier_peer {
  unsigned int parse_status, print_status, hash;
  int predicate, order;
  unsigned char output[256];
  size_t output_length;
  unsigned char *encoded;
  size_t encoded_length;
};
void cpkt_types_peer_identifier(int kind, int extended, const char *text,
                                size_t capacity,
                                struct cpkt_identifier_peer *result);
void cpkt_types_peer_offset(unsigned int *high, unsigned int *low);
void cpkt_types_peer_random(unsigned int high, unsigned int low,
                            unsigned char *guid, unsigned int *number);
void cpkt_types_peer_time(unsigned int high, unsigned int low,
                          unsigned int *unix_high, unsigned int *unix_low,
                          unsigned short *fields, short *year);
unsigned int cpkt_types_peer_nodes(void *server, int action, void *context);
unsigned int cpkt_types_peer_nodes_parallel(void *first, void *second);
struct cpkt_producer_observed {
  const void *address;
  unsigned int high[2], low[2], borrowed;
};
unsigned int
cpkt_types_peer_producer_direct(void *server, int kind, void *context,
                                struct cpkt_producer_observed *out);
void cpkt_types_peer_maps(unsigned int *results);
void cpkt_types_peer_stop(void *peer);
unsigned int cpkt_types_peer_producer_zero_methods(void *server, void *context);
struct cpkt_creation_observed {
  unsigned int status;
  int assigned, exists;
};
void cpkt_types_peer_creation_failure(int kind,
                                      struct cpkt_creation_observed *out);
struct cpkt_external_observed {
  const void *slot, *storage, *data;
  unsigned int high[2], low[2];
};
unsigned int cpkt_types_peer_external(void *server, unsigned int id,
                                      struct cpkt_external_observed *out);
#endif
