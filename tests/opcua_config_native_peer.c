#include "opcua_types_peer.h"
#include <limits.h>
#include <open62541/client.h>
#include <open62541/plugin/accesscontrol_default.h>
#include <open62541/plugin/nodestore.h>
#include <open62541/plugin/nodestore_default.h>
#include <open62541/server.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void check(int condition, const char *expression, int line) {
  if (!condition) {
    fprintf(stderr, "native configuration line %d: %s\n", line, expression);
    abort();
  }
}
#define CHECK(e) check(!!(e), #e, __LINE__)
void cpkt_types_peer_log(void *client) {
  UA_String value = UA_STRING("native schema");
  UA_LOG_WARNING(UA_Client_getConfig((UA_Client *)client)->logging,
                 UA_LOGCATEGORY_CLIENT, "native message %S", value);
}
void cpkt_types_peer_configuration(void) {
  UA_ClientConfig source, destination;
  UA_Client *client;
  UA_String uri = UA_STRING_NULL;
  UA_String expected = UA_STRING("urn:cpkt:configured");
  UA_String expected_token = UA_STRING("native-token");
  int context;
  memset(&source, 0, sizeof(source));
  memset(&destination, 0, sizeof(destination));
  source.sessionName = UA_STRING_ALLOC("native-session");
  source.endpointUrl = UA_STRING_ALLOC("opc.tcp://configured:4840");
  source.applicationUri = UA_STRING_ALLOC("urn:cpkt:application");
  source.authSecurityPolicyUri = UA_STRING_ALLOC("auth-policy");
  source.userTokenPolicy.policyId = UA_STRING_ALLOC("native-token");
  source.namespaces = UA_String_new();
  CHECK(source.namespaces != NULL);
  CHECK(UA_String_copy(&expected, source.namespaces) == 0);
  source.namespacesSize = 1;
  source.noSession = source.noReconnect = source.noNewSession = true;
  source.clientContext = &context;
  CHECK(UA_ClientConfig_copy(&source, &destination) == 0);
  CHECK(destination.userTokenPolicy.policyId.data !=
        source.userTokenPolicy.policyId.data);
  CHECK(destination.namespaces != source.namespaces &&
        destination.namespaces[0].data != source.namespaces[0].data);
  CHECK(destination.clientContext == &context && destination.noSession &&
        destination.noReconnect && destination.noNewSession);
  CHECK(UA_String_equal(&destination.sessionName, &source.sessionName));
  CHECK(UA_String_equal(&destination.endpointUrl, &source.endpointUrl));
  CHECK(UA_String_equal(&destination.applicationUri, &source.applicationUri));
  CHECK(UA_String_equal(&destination.authSecurityPolicyUri,
                        &source.authSecurityPolicyUri));
  UA_ClientConfig_clear(&source);
  CHECK(
      UA_String_equal(&destination.userTokenPolicy.policyId, &expected_token));
  client = UA_Client_newWithConfig(&destination);
  CHECK(client != NULL);
  CHECK(UA_String_equal(destination.namespaces, &expected));
  memset(&destination, 0, sizeof(destination));
  CHECK(UA_Client_getNamespaceUri(client, 2, &uri) == 0 &&
        UA_String_equal(&uri, &expected));
  UA_String_clear(&uri);
  UA_Client_delete(client);
}

/* Compare the actual public native factory result, including the borrowed
 * identifier address; a facade-created identifier copy would fail this test. */
int cpkt_types_peer_nodepointer(const void *pointer, const void *id) {
  UA_NodePointer expected = UA_NodePointer_fromNodeId((const UA_NodeId *)id);
  return ((const UA_NodePointer *)pointer)->immediate == expected.immediate;
}
void cpkt_types_peer_node_copy(void) {
  UA_Nodestore *store = UA_Nodestore_ZipTree();
  UA_Node *source, *destination;
  UA_LocalizedText name = UA_LOCALIZEDTEXT("en", "copied display name");
  CHECK(store != NULL);
  source = store->newNode(store, UA_NODECLASS_OBJECT);
  destination = store->newNode(store, UA_NODECLASS_OBJECT);
  CHECK(source != NULL && destination != NULL);
  source->head.nodeId = UA_NODEID_STRING_ALLOC(4, "source-id");
  source->head.browseName = UA_QUALIFIEDNAME_ALLOC(4, "source-name");
  destination->head.nodeId = UA_NODEID_STRING_ALLOC(4, "destination-id");
  destination->head.browseName = UA_QUALIFIEDNAME_ALLOC(4, "destination-name");
  CHECK(UA_Node_insertOrUpdateDisplayName(source, &name) == 0);
  CHECK(UA_Node_insertOrUpdateDisplayName(destination, &name) == 0);
  CHECK(UA_Node_copy(source, destination) == 0);
  CHECK(UA_NodeId_equal(&source->head.nodeId, &destination->head.nodeId));
  CHECK(source->head.nodeId.identifier.string.data !=
        destination->head.nodeId.identifier.string.data);
  CHECK(UA_Node_copy(source, destination) == 0);
  CHECK(UA_Node_copy(destination, destination) == 0);
  CHECK(UA_NodeId_equal(&source->head.nodeId, &destination->head.nodeId));
  store->deleteNode(store, source);
  store->deleteNode(store, destination);
  store->free(store);
}

unsigned int cpkt_types_peer_format(unsigned int kind, unsigned char **bytes,
                                    size_t *length) {
  UA_String out = UA_STRING_NULL;
  UA_StatusCode status;
  if (kind == 0) {
    status = UA_String_format(&out, "[%S|%N|%Q]", UA_STRING("schema"),
                              UA_NODEID_STRING(2, "node"),
                              UA_QUALIFIEDNAME(3, "qualified"));
  } else if (kind == 1) {
    status = UA_String_format(&out, "%*.*s %hhd %hu %ld %lu %x %c %% %p", -8, 3,
                              "abcdef", -7, (unsigned int)65000, -1234L, 3456UL,
                              (unsigned int)0xabc, 'Z', (void *)NULL);
  } else {
    status = UA_String_format(&out, "%lld %llu %d", (long long)UA_INT64_MIN,
                              (unsigned long long)UA_UINT64_MAX, INT_MIN);
  }
  if (status) {
    UA_String_clear(&out);
    return status;
  }
  *bytes = out.data;
  *length = out.length;
  return 0;
}

unsigned int cpkt_types_peer_attribute_borrow(void *owner, const void *view,
                                              int client) {
  const UA_Variant *borrowed = (const UA_Variant *)view;
  UA_Variant actual;
  UA_StatusCode status;
  UA_Variant_init(&actual);
  if (client)
    status = UA_Client_getConnectionAttribute(
        (UA_Client *)owner, UA_QUALIFIEDNAME(0, "serverDescription"), &actual);
  else {
    UA_NodeId admin = UA_NODEID_GUID(0, UA_GUID_NULL);
    admin.identifier.guid.data1 = 1;
    status = UA_Server_getSessionAttribute(
        (UA_Server *)owner, &admin, UA_QUALIFIEDNAME(0, "custom"), &actual);
  }
  if (status)
    return status;
  return borrowed->data == actual.data && borrowed->type == actual.type &&
                 borrowed->storageType == UA_VARIANT_DATA_NODELETE
             ? 0
             : UA_STATUSCODE_BADINTERNALERROR;
}

#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
void cpkt_types_fail_after(size_t count);
int cpkt_types_fail_stop(void);
static size_t access_clears;
static void access_clear_without_reset(UA_AccessControl *ac) {
  UA_free(ac->context);
  ++access_clears;
}
#endif
void cpkt_types_peer_accesscontrol_allocations(void) {
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  size_t i;
  UA_UsernamePasswordLogin users[2] = {
      {UA_STRING_STATIC("first"), UA_STRING_STATIC("secret-one")},
      {UA_STRING_STATIC("second"), UA_STRING_STATIC("secret-two")}};
  UA_String uri = UA_STRING("http://opcfoundation.org/UA/SecurityPolicy#None");
  access_clears = 0;
  for (i = 0; i < 100; ++i) {
    UA_ServerConfig config;
    UA_StatusCode status;
    int failed;
    memset(&config, 0, sizeof(config));
    config.accessControl.context = UA_malloc(1);
    CHECK(config.accessControl.context != NULL);
    config.accessControl.clear = access_clear_without_reset;
    cpkt_types_fail_after(i);
    status = UA_AccessControl_default(&config, true, &uri, 2, users);
    failed = cpkt_types_fail_stop();
    CHECK(access_clears == i + 1);
    CHECK(status ==
          (failed ? UA_STATUSCODE_BADOUTOFMEMORY : UA_STATUSCODE_GOOD));
    UA_ServerConfig_clear(&config);
    if (!failed)
      break;
  }
  CHECK(i > 5 && i < 100);
#endif
}
