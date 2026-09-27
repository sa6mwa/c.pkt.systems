#ifndef CPKT_OPCUA_CALLBACKS_TEST_H
#define CPKT_OPCUA_CALLBACKS_TEST_H
#include <cpkt/opcua_types.h>
void cpkt_types_test_callbacks(cpkt_opcua_client *client, void *peer);
void cpkt_types_test_plugins(void);
void cpkt_types_test_identifiers(void);
void cpkt_types_test_utilities(void);
void cpkt_types_test_producers(void);
void cpkt_types_test_nodes(void);
void cpkt_types_test_creation(void);
void cpkt_types_test_external(void);
void cpkt_types_test_client(cpkt_opcua_client *client, void *peer);
void cpkt_types_test_server(void);
void cpkt_types_test_history_backend(void);
#endif
