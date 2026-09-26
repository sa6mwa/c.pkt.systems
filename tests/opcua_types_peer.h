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
void cpkt_types_peer_stop(void *peer);
#endif
