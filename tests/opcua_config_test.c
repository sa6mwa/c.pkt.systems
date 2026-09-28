#include "opcua_callbacks_test.h"
#include "opcua_types_peer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check(int condition, const char *expression, int line) {
  if (!condition) {
    fprintf(stderr, "configuration line %d: %s\n", line, expression);
    abort();
  }
}
#define CHECK(e) check(!!(e), #e, __LINE__)

#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
void cpkt_types_fail_after(size_t count);
int cpkt_types_fail_stop(void);
#endif

static cpkt_opcua_ClientConfig *expected_client;
static cpkt_opcua_ServerConfig *expected_server;
static cpkt_opcua_server *constructed_server;
static size_t constructions;
static size_t destructions;

static cpkt_opcua_String text(const char *value) {
  cpkt_opcua_String string;
  string.length = strlen(value);
  string.data = (cpkt_opcua_Byte *)value;
  return string;
}
static cpkt_opcua_StatusCode password(cpkt_opcua_ClientConfig *config,
                                      cpkt_opcua_ByteString *value) {
  cpkt_opcua_String source = text("secret");
  CHECK(config == expected_client);
  return cpkt_opcua_String_copy(&source, value);
}
static cpkt_opcua_StatusCode construct(cpkt_opcua_server *server,
                                       const cpkt_opcua_NodeId *session,
                                       void *session_context,
                                       const cpkt_opcua_NodeId *node,
                                       void **node_context) {
  cpkt_opcua_ServerConfig *config = NULL;
  (void)session;
  (void)session_context;
  (void)node_context;
  CHECK(node != NULL);
  CHECK(cpkt_opcua_server_get_config_typed(server, &config) == 0);
  CHECK(config == expected_server);
  CHECK(cpkt_opcua_ServerConfig_delete(config) != 0);
  if (constructed_server)
    CHECK(constructed_server == server);
  constructed_server = server;
  ++constructions;
  return 0;
}
static void destruct(cpkt_opcua_server *server,
                     const cpkt_opcua_NodeId *session, void *session_context,
                     const cpkt_opcua_NodeId *node, void *node_context) {
  (void)session;
  (void)session_context;
  (void)node_context;
  CHECK(server == constructed_server && node != NULL);
  ++destructions;
}
static void copied_configuration(void) {
  cpkt_opcua_ClientConfig *source = cpkt_opcua_ClientConfig_new();
  cpkt_opcua_ClientConfig *destination = cpkt_opcua_ClientConfig_new();
  cpkt_opcua_ClientConfigSettings input, readback;
  cpkt_opcua_ClientConfigCallbacks callbacks, copied;
  cpkt_opcua_String namespace_uri = text("urn:cpkt:configured");
  cpkt_opcua_client *client = NULL;
  cpkt_opcua_ClientConfig *borrowed = NULL;
  CHECK(source != NULL && destination != NULL);
  memset(&input, 0, sizeof(input));
  memset(&readback, 0, sizeof(readback));
  memset(&callbacks, 0, sizeof(callbacks));
  input.sessionName = text("copy-independent");
  input.userTokenPolicy.policyId = text("token-independent");
  input.namespaces = &namespace_uri;
  input.namespacesSize = 1;
  input.noSession = input.noReconnect = input.noNewSession = 1;
  input.timeout = 4321;
  callbacks.privateKeyPasswordCallback = password;
  CHECK(cpkt_opcua_ClientConfig_setSettings(source, &input) == 0);
  CHECK(cpkt_opcua_ClientConfig_setCallbacks(source, &callbacks) == 0);
  CHECK(cpkt_opcua_ClientConfig_copy(source, destination) == 0);
  CHECK(cpkt_opcua_ClientConfig_copy(source, destination) != 0);
  CHECK(cpkt_opcua_ClientConfig_getCallbacks(destination, &copied) == 0);
  CHECK(copied.privateKeyPasswordCallback == password);
  CHECK(cpkt_opcua_ClientConfig_delete(source) == 0);
  CHECK(cpkt_opcua_ClientConfig_getSettings(destination, &readback) == 0);
  CHECK(readback.timeout == 4321 && readback.noSession &&
        readback.noReconnect && readback.noNewSession);
  CHECK(cpkt_opcua_String_equal(&readback.sessionName, &input.sessionName));
  CHECK(cpkt_opcua_String_equal(&readback.userTokenPolicy.policyId,
                                &input.userTokenPolicy.policyId));
  CHECK(readback.namespacesSize == 1 &&
        cpkt_opcua_String_equal(readback.namespaces, &namespace_uri));
  cpkt_opcua_ClientConfigSettings_clear(&readback);
  expected_client = destination;
  CHECK(cpkt_opcua_client_newWithConfig_typed(destination, &client) == 0 &&
        client != NULL);
  CHECK(cpkt_opcua_client_get_config_typed(client, &borrowed) == 0 &&
        borrowed == destination);
  CHECK(cpkt_opcua_ClientConfig_delete(borrowed) != 0);
  cpkt_opcua_client_free(client);
  expected_client = NULL;
}
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
static void copy_failures(void) {
  cpkt_opcua_ClientConfig *source = cpkt_opcua_ClientConfig_new();
  cpkt_opcua_ClientConfig *destination;
  cpkt_opcua_ClientConfigSettings input, readback;
  cpkt_opcua_String namespace_uri = text("urn:cpkt:allocation");
  cpkt_opcua_StatusCode status;
  size_t i;
  int injected;
  CHECK(source != NULL);
  memset(&input, 0, sizeof(input));
  input.sessionName = text("retained-session");
  input.userTokenPolicy.policyId = text("retained-token");
  input.namespaces = &namespace_uri;
  input.namespacesSize = 1;
  input.timeout = 7654;
  CHECK(cpkt_opcua_ClientConfig_setSettings(source, &input) == 0);
  for (i = 0; i < 128; ++i) {
    destination = cpkt_opcua_ClientConfig_new();
    CHECK(destination != NULL);
    cpkt_types_fail_after(i);
    status = cpkt_opcua_ClientConfig_copy(source, destination);
    injected = cpkt_types_fail_stop();
    memset(&readback, 0, sizeof(readback));
    CHECK(cpkt_opcua_ClientConfig_getSettings(source, &readback) == 0);
    CHECK(readback.timeout == 7654 && readback.namespacesSize == 1);
    CHECK(cpkt_opcua_String_equal(&readback.userTokenPolicy.policyId,
                                  &input.userTokenPolicy.policyId));
    CHECK(cpkt_opcua_String_equal(readback.namespaces, &namespace_uri));
    cpkt_opcua_ClientConfigSettings_clear(&readback);
    memset(&readback, 0, sizeof(readback));
    CHECK(cpkt_opcua_ClientConfig_getSettings(destination, &readback) == 0);
    if (injected) {
      CHECK(status != 0 && readback.timeout == 0 &&
            readback.namespacesSize == 0 &&
            readback.userTokenPolicy.policyId.data == NULL);
    } else {
      CHECK(status == 0 && readback.timeout == 7654 &&
            readback.namespacesSize == 1);
    }
    cpkt_opcua_ClientConfigSettings_clear(&readback);
    CHECK(cpkt_opcua_ClientConfig_delete(destination) == 0);
    if (!injected)
      break;
  }
  CHECK(i > 0 && i < 128);
  CHECK(cpkt_opcua_ClientConfig_delete(source) == 0);
}
#endif
static void native_security_views(void) {
  cpkt_opcua_ClientConfig *config = cpkt_opcua_ClientConfig_new();
  cpkt_opcua_SecurityPolicy *view = NULL, *after_growth = NULL;
  cpkt_opcua_SecurityPolicy policy;
  cpkt_opcua_ByteString empty;
  void *channel = NULL;
  size_t count;
  CHECK(config != NULL);
  memset(&policy, 0, sizeof(policy));
  memset(&empty, 0, sizeof(empty));
  CHECK(cpkt_opcua_ClientConfig_setLogger(config, NULL) == 0);
  CHECK(cpkt_opcua_ClientConfig_setDefault(config) == 0);
  CHECK(cpkt_opcua_ClientConfig_getSecurityPoliciesSize(config, 0, &count) ==
            0 &&
        count > 0);
  CHECK(cpkt_opcua_ClientConfig_getSecurityPolicy(config, 0, 0, &view) == 0);
  CHECK(view->newChannelContext != NULL && view->deleteChannelContext != NULL);
  CHECK(view->newChannelContext(view, &empty, &channel) == 0);
  view->deleteChannelContext(view, channel);
  CHECK(cpkt_opcua_ClientConfig_addSecurityPolicy(config, 0, view) != 0);
  CHECK(cpkt_opcua_SecurityPolicy_None(&policy, empty, NULL) == 0);
  CHECK(cpkt_opcua_ClientConfig_addSecurityPolicy(config, 0, &policy) == 0);
  CHECK(policy.clear == NULL && policy.policyContext == NULL);
  CHECK(cpkt_opcua_ClientConfig_getSecurityPolicy(config, 0, 0,
                                                  &after_growth) == 0 &&
        after_growth == view);
  CHECK(view->newChannelContext(view, &empty, &channel) == 0);
  view->deleteChannelContext(view, channel);
  CHECK(cpkt_opcua_ClientConfig_getSecurityPoliciesSize(config, 0, &count) ==
            0 &&
        count > 1);
  CHECK(cpkt_opcua_ClientConfig_delete(config) == 0);
}
static void construction_identity(void) {
  cpkt_opcua_ServerConfig *config = cpkt_opcua_ServerConfig_new();
  cpkt_opcua_ServerConfig *borrowed = NULL;
  cpkt_opcua_GlobalNodeLifecycle lifecycle;
  cpkt_opcua_GlobalNodeLifecycle *view;
  cpkt_opcua_NodeId typed_node = cpkt_opcua_NODEID_NUMERIC(1, 64001);
  void *node_context = NULL;
  cpkt_opcua_server *server = NULL;
  cpkt_opcua_node_id node, parent;
  cpkt_opcua_status status = 0;
  CHECK(config != NULL);
  memset(&lifecycle, 0, sizeof(lifecycle));
  memset(&node, 0, sizeof(node));
  memset(&parent, 0, sizeof(parent));
  lifecycle.constructor = construct;
  lifecycle.destructor = destruct;
  constructions = destructions = 0;
  constructed_server = NULL;
  expected_server = config;
  CHECK(cpkt_opcua_ServerConfig_setLogger(config, NULL) == 0);
  CHECK(cpkt_opcua_ServerConfig_setMinimal(config, 0, NULL) == 0);
  CHECK(cpkt_opcua_ServerConfig_setGlobalNodeLifecycle(config, &lifecycle) ==
        0);
  CHECK(cpkt_opcua_server_newWithConfig_typed(config, &server) == 0 &&
        server != NULL);
  CHECK(cpkt_opcua_server_get_config_typed(server, &borrowed) == 0 &&
        borrowed == config);
  node = cpkt_opcua_node_id_numeric(1, 64001);
  parent = cpkt_opcua_node_id_numeric(0, 85);
  CHECK(cpkt_opcua_server_add_object(server, node, parent, "configured",
                                     "Configured", &status) == CPKT_OPCUA_OK &&
        status == 0);
  CHECK(constructions > 0 && constructed_server == server);
  CHECK(cpkt_opcua_ServerConfig_getGlobalNodeLifecycle(config, &view) == 0 &&
        view != NULL && view->constructor != NULL && view->destructor != NULL);
  CHECK(cpkt_opcua_ServerConfig_setGlobalNodeLifecycle(config, view) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(view->constructor(server, NULL, NULL, &typed_node, &node_context) == 0);
  view->destructor(server, NULL, NULL, &typed_node, node_context);
  cpkt_opcua_server_free(server);
  CHECK(destructions > 0);
  expected_server = NULL;
  constructed_server = NULL;
}
static unsigned int logins;
static cpkt_opcua_ServerConfig *login_configuration;
static cpkt_opcua_StatusCode
login_callback(const cpkt_opcua_String *name,
               const cpkt_opcua_ByteString *secret, size_t count,
               const cpkt_opcua_UsernamePasswordLogin *users, void **session,
               void *context) {
  CHECK(name != NULL && secret != NULL && count == 2 && users != NULL);
  CHECK(context == &logins && session != NULL);
  CHECK(users[0].username.length == 5 &&
        memcmp(users[0].username.data, "first", 5) == 0);
  CHECK(users[1].password.length == 10 &&
        memcmp(users[1].password.data, "secret-two", 10) == 0);
  CHECK(cpkt_opcua_ServerConfig_delete(login_configuration) != 0);
  ++logins;
  *session = context;
  return name->length ? CPKT_OPCUA_STATUSCODE_BADUSERACCESSDENIED : 0;
}
static void access_control(void) {
  cpkt_opcua_ServerConfig *config = cpkt_opcua_ServerConfig_new();
  cpkt_opcua_UsernamePasswordLogin users[2];
  cpkt_opcua_AccessControl *view;
  cpkt_opcua_server *server = NULL;
  cpkt_opcua_NodeId admin;
  cpkt_opcua_ExtensionObject token;
  cpkt_opcua_UserNameIdentityToken user_token;
  void *session = NULL;
  cpkt_opcua_String uri =
      text("http://opcfoundation.org/UA/SecurityPolicy#None");
  CHECK(config != NULL);
  CHECK(cpkt_opcua_ServerConfig_setLogger(config, NULL) == 0);
  CHECK(cpkt_opcua_ServerConfig_setMinimal(config, 0, NULL) == 0);
  users[0].username = text("first");
  users[0].password = text("secret-one");
  users[1].username = text("second");
  users[1].password = text("secret-two");
  logins = 0;
  login_configuration = config;
  CHECK(cpkt_opcua_AccessControl_defaultWithLoginCallback(
            config, 1, &uri, 2, users, login_callback, &logins) == 0);
  CHECK(cpkt_opcua_server_newWithConfig_typed(config, &server) == 0 &&
        server != NULL);
  CHECK(cpkt_opcua_ServerConfig_getAccessControl(config, &view) == 0 &&
        view != NULL && view->activateSession != NULL);
  memset(&admin, 0, sizeof(admin));
  admin.identifierType = CPKT_OPCUA_NODEIDTYPE_GUID;
  admin.identifier.guid.data1 = 1;
  memset(&token, 0, sizeof(token));
  memset(&user_token, 0, sizeof(user_token));
  CHECK(view->activateSession(server, view, NULL, NULL, &admin, &token,
                              &session) == 0);
  CHECK(logins == 1 && session == &logins);
  token.encoding = CPKT_OPCUA_EXTENSIONOBJECT_DECODED_NODELETE;
  token.content.decoded.type =
      cpkt_opcua_type_at(CPKT_OPCUA_TYPES_USERNAMEIDENTITYTOKEN);
  token.content.decoded.data = &user_token;
  user_token.userName = text("first");
  user_token.password = text("secret-one");
  CHECK(view->activateSession(server, view, NULL, NULL, &admin, &token,
                              &session) ==
        CPKT_OPCUA_STATUSCODE_BADUSERACCESSDENIED);
  CHECK(logins == 2);
  CHECK(cpkt_opcua_AccessControl_default(config, 1, &uri, 2, users) == 0);
  CHECK(cpkt_opcua_ServerConfig_getAccessControl(config, &view) == 0);
  CHECK(view->activateSession(server, view, NULL, NULL, &admin, &token,
                              &session) == 0);
  user_token.password = text("wrong");
  CHECK(view->activateSession(server, view, NULL, NULL, &admin, &token,
                              &session) ==
        CPKT_OPCUA_STATUSCODE_BADUSERACCESSDENIED);
  CHECK(logins == 2);
  cpkt_opcua_server_free(server);
  login_configuration = NULL;
  cpkt_types_peer_accesscontrol_allocations();
}
static cpkt_opcua_server *discovery_parent;
static cpkt_opcua_ClientConfig *discovery_configuration;
static unsigned int discovery_notifications, discovery_stops;
static void discovery_notify(cpkt_opcua_client *client,
                             cpkt_opcua_ApplicationNotificationType type,
                             cpkt_opcua_StatusCode conversion,
                             const cpkt_opcua_KeyValueMap *payload) {
  cpkt_opcua_ClientConfig *config = NULL;
  CHECK(conversion == 0 && payload != NULL);
  CHECK(cpkt_opcua_client_get_config_typed(client, &config) == 0);
  CHECK(config == discovery_configuration);
  CHECK(cpkt_opcua_ClientConfig_delete(config) != 0);
  CHECK(cpkt_opcua_server_delete_typed(discovery_parent) != 0);
  cpkt_opcua_client_free(client); /* Internal discovery identity is borrowed. */
  ++discovery_notifications;
  if (type == CPKT_OPCUA_APPLICATIONNOTIFICATIONTYPE_LIFECYCLE_STOPPED)
    ++discovery_stops;
}
static cpkt_opcua_ClientConfig *discovery_config(void) {
  cpkt_opcua_ClientConfig *config = cpkt_opcua_ClientConfig_new();
  cpkt_opcua_ClientConfigCallbacks callbacks;
  CHECK(config != NULL);
  CHECK(cpkt_opcua_ClientConfig_setLogger(config, NULL) == 0);
  CHECK(cpkt_opcua_ClientConfig_setDefault(config) == 0);
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.globalNotificationCallback = discovery_notify;
  CHECK(cpkt_opcua_ClientConfig_setCallbacks(config, &callbacks) == 0);
  return config;
}
static void discovery_ownership(void) {
  cpkt_opcua_ServerConfig *settings = cpkt_opcua_ServerConfig_new();
  cpkt_opcua_ClientConfig *config, *saved;
  cpkt_opcua_ClientConfigSettings readback;
  cpkt_opcua_ClientConfigCallbacks callbacks;
  cpkt_opcua_EventLoop *loop, *parent_loop, *after;
  cpkt_opcua_EventLoopState state;
  cpkt_opcua_Boolean external;
  cpkt_opcua_status status;
  cpkt_opcua_String url = text("opc.tcp://127.0.0.1:1"), empty = text("");
  unsigned int i;
  CHECK(settings != NULL);
  CHECK(cpkt_opcua_ServerConfig_setLogger(settings, NULL) == 0);
  CHECK(cpkt_opcua_ServerConfig_setMinimal(settings, 0, NULL) == 0);
  CHECK(cpkt_opcua_server_newWithConfig_typed(settings, &discovery_parent) ==
        0);
  config = discovery_config();
  saved = config;
  CHECK(cpkt_opcua_server_registerDiscovery_typed(discovery_parent, &config,
                                                  url, empty) != 0);
  CHECK(config == saved); /* Early native failure clears but never hands off. */
  memset(&readback, 0, sizeof(readback));
  CHECK(cpkt_opcua_ClientConfig_getSettings(config, &readback) == 0);
  CHECK(readback.endpointUrl.length == 0 && readback.sessionName.length == 0);
  cpkt_opcua_ClientConfigSettings_clear(&readback);
  CHECK(cpkt_opcua_ClientConfig_delete(config) == 0);
  CHECK(cpkt_opcua_server_startup(discovery_parent, &status) == CPKT_OPCUA_OK &&
        status == 0);
  CHECK(cpkt_opcua_server_get_event_loop_typed(discovery_parent,
                                               &parent_loop) == 0);
  config = discovery_config();
  saved = config;
  CHECK(cpkt_opcua_ClientConfig_getEventLoop(config, &loop, &external) == 0 &&
        loop != NULL && !external);
  CHECK(cpkt_opcua_EventLoop_start(loop) == 0);
  CHECK(cpkt_opcua_server_registerDiscovery_typed(discovery_parent, &config,
                                                  url, empty) != 0);
  CHECK(config == saved); /* A running owned loop cannot be discarded. */
  CHECK(cpkt_opcua_ClientConfig_getEventLoop(config, &after, &external) == 0 &&
        after == loop && !external);
  CHECK(cpkt_opcua_ClientConfig_getCallbacks(config, &callbacks) == 0 &&
        callbacks.globalNotificationCallback == discovery_notify);
  cpkt_opcua_EventLoop_stop(loop);
  for (i = 0; i < 100; ++i) {
    CHECK(cpkt_opcua_EventLoop_getState(loop, &state) == 0);
    if (state == CPKT_OPCUA_EVENTLOOPSTATE_STOPPED)
      break;
    CHECK(cpkt_opcua_EventLoop_run(loop, 0) == 0);
  }
  CHECK(state == CPKT_OPCUA_EVENTLOOPSTATE_STOPPED);
  discovery_configuration = config;
  discovery_notifications = discovery_stops = 0;
  (void)cpkt_opcua_server_registerDiscovery_typed(discovery_parent, &config,
                                                  url, empty);
  CHECK(config ==
        NULL); /* Native owns the asynchronous child, even on error. */
  for (i = 0; i < 20; ++i)
    CHECK(cpkt_opcua_server_iterate(discovery_parent, 0, NULL) ==
          CPKT_OPCUA_OK);
  CHECK(cpkt_opcua_server_shutdown(discovery_parent, &status) ==
            CPKT_OPCUA_OK &&
        status == 0);
  CHECK(discovery_notifications > 0 && discovery_stops == 1);
  CHECK(cpkt_opcua_server_delete_typed(discovery_parent) == 0);
  discovery_parent = NULL;
  discovery_configuration = NULL;
}

/* Own native descriptor metadata while retaining independent C89 inputs. */
static cpkt_opcua_Type *custom_type(void) {
  cpkt_opcua_StructureDescription description;
  cpkt_opcua_StructureField fields[2];
  cpkt_opcua_ExtensionObject object;
  cpkt_opcua_Type *type = NULL;
  memset(&description, 0, sizeof(description));
  memset(fields, 0, sizeof(fields));
  memset(&object, 0, sizeof(object));
  description.dataTypeId = cpkt_opcua_NODEID_NUMERIC(2, 64010);
  description.name = cpkt_opcua_QUALIFIEDNAME(2, "ConfiguredPair");
  description.structureDefinition.defaultEncodingId =
      cpkt_opcua_NODEID_NUMERIC(2, 64011);
  description.structureDefinition.baseDataType =
      cpkt_opcua_NODEID_NUMERIC(0, 22);
  description.structureDefinition.structureType =
      cpkt_opcua_STRUCTURETYPE_STRUCTURE;
  description.structureDefinition.fieldsSize = 2;
  description.structureDefinition.fields = fields;
  fields[0].name = text("number");
  fields[0].dataType = cpkt_opcua_NODEID_NUMERIC(0, 8);
  fields[0].valueRank = -1;
  fields[1].name = text("label");
  fields[1].dataType = cpkt_opcua_NODEID_NUMERIC(0, 12);
  fields[1].valueRank = -1;
  object.encoding = CPKT_OPCUA_EXTENSIONOBJECT_DECODED_NODELETE;
  object.content.decoded.type =
      cpkt_opcua_type_at(CPKT_OPCUA_TYPES_STRUCTUREDESCRIPTION);
  object.content.decoded.data = &description;
  CHECK(cpkt_opcua_DataType_fromDescription(&type, &object, NULL) == 0 &&
        type != NULL);
  return type;
}
struct configured_pair {
  cpkt_opcua_Int64 number;
  cpkt_opcua_String label;
};
struct codec_arena {
  void *allocations[128];
  size_t count;
  const cpkt_opcua_ByteString *encoded;
  const cpkt_opcua_DataTypeArray *nested_types;
  int nested;
};
static void *codec_arena_allocate(void *context, size_t count, size_t size) {
  struct codec_arena *arena = (struct codec_arena *)context;
  void *allocated;
  if (!arena->nested) {
    cpkt_opcua_DecodeBinaryOptions options;
    cpkt_opcua_ExtensionObject decoded;
    arena->nested = 1;
    memset(&options, 0, sizeof(options));
    memset(&decoded, 0, sizeof(decoded));
    options.customTypes = arena->nested_types;
    CHECK(cpkt_opcua_decodeBinary(
              arena->encoded, &decoded,
              cpkt_opcua_type_at(CPKT_OPCUA_TYPES_EXTENSIONOBJECT),
              &options) == 0);
    CHECK(decoded.content.decoded.type == arena->nested_types->types[0]);
    cpkt_opcua_ExtensionObject_clear(&decoded);
  }
  allocated = calloc(count, size);
  CHECK(allocated != NULL && arena->count < 128);
  arena->allocations[arena->count++] = allocated;
  return allocated;
}
static void custom_types(void) {
  cpkt_opcua_ClientConfig *config = cpkt_opcua_ClientConfig_new();
  cpkt_opcua_Type *type = custom_type();
  const cpkt_opcua_Type *types[1], *found;
  const cpkt_opcua_DataTypeArray *first, *second;
  cpkt_opcua_DataTypeArray array;
  cpkt_opcua_DataTypeInfo info;
  cpkt_opcua_DataTypeMemberInfo member;
  struct configured_pair pair, copied;
  cpkt_opcua_NodeId id = cpkt_opcua_NODEID_NUMERIC(2, 64010);
  CHECK(config != NULL);
  types[0] = type;
  memset(&array, 0, sizeof(array));
  array.types = types;
  array.typesSize = 1;
  array.cleanup = 1;
  CHECK(cpkt_opcua_ClientConfig_setCustomDataTypes(config, &array) == 0);
  CHECK(cpkt_opcua_DataType_delete(type) ==
        0); /* Owned metadata is independent. */
  CHECK(cpkt_opcua_ClientConfig_getCustomDataTypes(config, &first) == 0 &&
        first != NULL && first->cleanup);
  CHECK(cpkt_opcua_ClientConfig_getCustomDataTypes(config, &second) == 0 &&
        second != NULL && second != first);
  found = cpkt_opcua_findDataTypeWithCustom(&id, first);
  CHECK(found == first->types[0] && found != second->types[0]);
  CHECK(cpkt_opcua_DataType_delete((cpkt_opcua_Type *)found) != 0);
  CHECK(cpkt_opcua_DataType_getInfo(found, &info) == 0 &&
        info.publicSize == sizeof(pair) && info.membersSize == 2);
  CHECK(cpkt_opcua_DataType_getMember(found, 0, &member) == 0 &&
        member.memberType == cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64));
  memset(&pair, 0, sizeof(pair));
  memset(&copied, 0, sizeof(copied));
  pair.number.high32 = 0x80000000U;
  pair.number.low32 = 7;
  pair.label = text("source-freed");
  CHECK(cpkt_opcua_type_copy(&pair, &copied, found) == 0);
  CHECK(copied.number.high32 == pair.number.high32 &&
        copied.number.low32 == 7 &&
        cpkt_opcua_String_equal(&pair.label, &copied.label));
  cpkt_opcua_type_clear(&copied, found);
  /* Equivalent snapshots must not steal the codec's descriptor ownership. */
  {
    cpkt_opcua_ExtensionObject input, decoded;
    cpkt_opcua_ByteString encoded;
    cpkt_opcua_DecodeBinaryOptions options;
    memset(&input, 0, sizeof(input));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    memset(&options, 0, sizeof(options));
    input.encoding = CPKT_OPCUA_EXTENSIONOBJECT_DECODED_NODELETE;
    input.content.decoded.type = found;
    input.content.decoded.data = &pair;
    options.customTypes = first;
    CHECK(cpkt_opcua_encodeBinary(
              &input, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_EXTENSIONOBJECT),
              &encoded, NULL) == 0);
    CHECK(cpkt_opcua_decodeBinary(
              &encoded, &decoded,
              cpkt_opcua_type_at(CPKT_OPCUA_TYPES_EXTENSIONOBJECT),
              &options) == 0);
    CHECK(decoded.encoding == CPKT_OPCUA_EXTENSIONOBJECT_DECODED &&
          decoded.content.decoded.type == found);
    CHECK(((struct configured_pair *)decoded.content.decoded.data)
              ->number.high32 == pair.number.high32);
    cpkt_opcua_ExtensionObject_clear(&decoded);
    /* Reentrant allocation decodes using the other descriptor owner. The
     * outer codec must restore its own mapping and arena-owned output. */
    {
      struct codec_arena arena;
      size_t i;
      memset(&arena, 0, sizeof(arena));
      memset(&decoded, 0, sizeof(decoded));
      arena.encoded = &encoded;
      arena.nested_types = second;
      options.callocContext = &arena;
      options.calloc = codec_arena_allocate;
      CHECK(cpkt_opcua_decodeBinary(
                &encoded, &decoded,
                cpkt_opcua_type_at(CPKT_OPCUA_TYPES_EXTENSIONOBJECT),
                &options) == 0);
      CHECK(arena.nested && decoded.content.decoded.type == found);
      CHECK(((struct configured_pair *)decoded.content.decoded.data)
                ->number.high32 == pair.number.high32);
      /* Both native and C89 allocations belong to the caller's arena. */
      for (i = 0; i < arena.count; ++i)
        free(arena.allocations[i]);
    }
    cpkt_opcua_ByteString_clear(&encoded);
  }
  CHECK(cpkt_opcua_ClientConfig_setCustomDataTypes(config, NULL) == 0);
  CHECK(cpkt_opcua_ClientConfig_getCustomDataTypes(config, &first) == 0 &&
        first == NULL);
  type = custom_type();
  types[0] = type;
  array.cleanup = 0;
  CHECK(cpkt_opcua_ClientConfig_setCustomDataTypes(config, &array) == 0);
  CHECK(cpkt_opcua_ClientConfig_getCustomDataTypes(config, &first) == 0 &&
        !first->cleanup);
  CHECK(cpkt_opcua_ClientConfig_delete(config) == 0);
  CHECK(cpkt_opcua_DataType_getInfo(type, &info) == 0 && info.membersSize == 2);
  CHECK(cpkt_opcua_DataType_delete(type) == 0);
}

static size_t policy_clears;
static void
configuration_policy_clear(cpkt_opcua_PubSubSecurityPolicy *policy) {
  ++policy_clears;
  memset(policy, 0, sizeof(*policy));
}
static void pubsub_configuration(void) {
  cpkt_opcua_ServerConfig *config = cpkt_opcua_ServerConfig_new();
  cpkt_opcua_PubSubConfiguration input, *view;
  cpkt_opcua_PubSubSecurityPolicy policy, *direct;
  cpkt_opcua_String borrowed;
  cpkt_opcua_server *server = NULL;
  CHECK(config != NULL);
  CHECK(cpkt_opcua_ServerConfig_setLogger(config, NULL) == 0);
  CHECK(cpkt_opcua_ServerConfig_setMinimal(config, 0, NULL) == 0);
  memset(&input, 0, sizeof(input));
  memset(&policy, 0, sizeof(policy));
  policy.policyUri = text("urn:cpkt:test-pubsub-policy");
  policy.clear = configuration_policy_clear;
  policy_clears = 0;
  input.enableDeltaFrames = 1;
  input.securityPoliciesSize = 1;
  input.securityPolicies = &policy;
  CHECK(cpkt_opcua_ServerConfig_setPubSubConfiguration(config, &input) == 0);
  CHECK(policy.policyContext == NULL && policy.clear == NULL);
  CHECK(cpkt_opcua_ServerConfig_getPubSubConfiguration(config, &view) == 0 &&
        view != NULL);
  CHECK(view->enableDeltaFrames && view->securityPoliciesSize == 1 &&
        view->securityPolicies != NULL);
  borrowed = view->securityPolicies[0].policyUri;
  CHECK(borrowed.length > 0 && borrowed.data != NULL);
  CHECK(cpkt_opcua_ServerConfig_getPubSubSecurityPolicy(config, 0, &direct) ==
        0);
  CHECK(cpkt_opcua_String_equal(&borrowed, &direct->policyUri));
  /* Refreshing the direct plugin cannot invalidate the earlier table view. */
  CHECK(cpkt_opcua_ServerConfig_getPubSubSecurityPolicy(config, 0, &direct) ==
        0);
  CHECK(cpkt_opcua_String_equal(&borrowed, &direct->policyUri));
  CHECK(cpkt_opcua_ServerConfig_setPubSubConfiguration(config, view) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_server_newWithConfig_typed(config, &server) == 0 &&
        server != NULL);
  CHECK(cpkt_opcua_ServerConfig_getPubSubConfiguration(config, &view) == 0);
  CHECK(view->enableDeltaFrames && view->securityPoliciesSize == 1);
  cpkt_opcua_server_free(server);
  CHECK(policy_clears == 1);
}

#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
static void custom_type_failures(void) {
  cpkt_opcua_ClientConfig *config = cpkt_opcua_ClientConfig_new();
  cpkt_opcua_Type *type = custom_type();
  const cpkt_opcua_Type *types[1];
  const cpkt_opcua_DataTypeArray *view;
  cpkt_opcua_DataTypeArray array;
  cpkt_opcua_StatusCode status;
  size_t i;
  int injected;
  CHECK(config != NULL);
  types[0] = type;
  memset(&array, 0, sizeof(array));
  array.types = types;
  array.typesSize = 1;
  array.cleanup = 1;
  for (i = 0; i < 250; ++i) {
    cpkt_types_fail_after(i);
    status = cpkt_opcua_ClientConfig_setCustomDataTypes(config, &array);
    injected = cpkt_types_fail_stop();
    if (!injected) {
      CHECK(status == 0);
      break;
    }
    CHECK(status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY);
    CHECK(cpkt_opcua_ClientConfig_getCustomDataTypes(config, &view) == 0 &&
          view == NULL);
  }
  CHECK(i < 250);
  CHECK(cpkt_opcua_DataType_delete(type) == 0);
  for (i = 0; i < 250; ++i) {
    cpkt_types_fail_after(i);
    status = cpkt_opcua_ClientConfig_getCustomDataTypes(config, &view);
    injected = cpkt_types_fail_stop();
    if (!injected) {
      CHECK(status == 0 && view != NULL);
      break;
    }
    CHECK(status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY && view == NULL);
  }
  CHECK(i < 250 && cpkt_opcua_ClientConfig_delete(config) == 0);
}
#endif

void cpkt_types_test_configuration(void) {
  cpkt_types_peer_configuration();
  copied_configuration();
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  copy_failures();
#endif
  native_security_views();
  construction_identity();
  access_control();
  custom_types();
  pubsub_configuration();
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  custom_type_failures();
#endif
  discovery_ownership();
}
