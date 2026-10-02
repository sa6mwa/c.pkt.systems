#include "opcua_callbacks_test.h"
#include "opcua_types_peer.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void check(int condition, const char *expression, int line) {
  if (!condition) {
    fprintf(stderr, "operations line %d: %s\n", line, expression);
    abort();
  }
}
#define CHECK(e) check(!!(e), #e, __LINE__)
static cpkt_opcua_QualifiedName key(const char *name) {
  cpkt_opcua_QualifiedName out;
  memset(&out, 0, sizeof(out));
  out.name.length = strlen(name);
  out.name.data = (cpkt_opcua_Byte *)name;
  return out;
}
static cpkt_opcua_status borrowed_client(void *native, void *view) {
  return cpkt_types_peer_attribute_borrow(native, view, 1);
}
static cpkt_opcua_status borrowed_server(void *native, void *view) {
  return cpkt_types_peer_attribute_borrow(native, view, 0);
}
static void attributes(void) {
  cpkt_opcua_client *client = NULL;
  cpkt_opcua_server *server = NULL;
  cpkt_opcua_VariantView *view = cpkt_opcua_VariantView_new();
  cpkt_opcua_Variant value;
  cpkt_opcua_ApplicationDescription description;
  cpkt_opcua_Int64 number, scalar;
  cpkt_opcua_NodeId admin;
  const cpkt_opcua_Type *type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
  cpkt_opcua_ConnectionConfig connection;
  cpkt_opcua_DataTypeInfo info;
  CHECK(view != NULL);
  memset(&value, 0, sizeof(value));
  memset(&description, 0, sizeof(description));
  CHECK(cpkt_opcua_client_new(&client) == CPKT_OPCUA_OK);
  CHECK(cpkt_opcua_DataType_getInfo(type, &info) == 0);
  CHECK(cpkt_opcua_client_findDataType_typed(client, &info.typeId) == type);
  CHECK(cpkt_opcua_client_getConnectionAttribute_typed(
            client, key("serverDescription"), view) == 0);
  CHECK(cpkt_opcua_client_native(client, borrowed_client, view) ==
        CPKT_OPCUA_OK);
  CHECK(cpkt_opcua_VariantView_snapshot(view, &value) == 0 &&
        value.type ==
            cpkt_opcua_type_at(CPKT_OPCUA_TYPES_APPLICATIONDESCRIPTION));
  cpkt_opcua_Variant_clear(&value);
  CHECK(cpkt_opcua_client_getConnectionAttributeCopy_typed(
            client, key("serverDescription"), &value) == 0);
  cpkt_opcua_Variant_clear(&value);
  CHECK(cpkt_opcua_client_getConnectionAttribute_scalar_typed(
            client, key("serverDescription"),
            cpkt_opcua_type_at(CPKT_OPCUA_TYPES_APPLICATIONDESCRIPTION),
            &description) == 0);
  cpkt_opcua_ApplicationDescription_clear(&description);
  CHECK(cpkt_opcua_client_getConnectionAttribute_scalar_typed(
            client, key("serverDescription"), type, &scalar) ==
        CPKT_OPCUA_STATUSCODE_BADNOTFOUND);
  CHECK(cpkt_opcua_client_getConnectionAttribute_typed(
            client, key("securityPolicyUri"), view) ==
        CPKT_OPCUA_STATUSCODE_BADNOTCONNECTED);
  CHECK(cpkt_opcua_client_getConnectionAttribute_typed(client, key("unknown"),
                                                       view) ==
        CPKT_OPCUA_STATUSCODE_BADINTERNALERROR);
  CHECK(cpkt_opcua_VariantView_snapshot(view, &value) == 0 &&
        value.type ==
            cpkt_opcua_type_at(CPKT_OPCUA_TYPES_APPLICATIONDESCRIPTION));
  cpkt_opcua_Variant_clear(&value);
  cpkt_opcua_client_free(client);
  /* Deleting an expired NODELETE root never inspects freed payload bytes. */
  cpkt_opcua_VariantView_delete(view);
  view = cpkt_opcua_VariantView_new();
  CHECK(view != NULL);
  CHECK(cpkt_opcua_server_new(&server, 0) == CPKT_OPCUA_OK);
  memset(&admin, 0, sizeof(admin));
  admin.identifierType = CPKT_OPCUA_NODEIDTYPE_GUID;
  admin.identifier.guid.data1 = 1;
  number.high32 = 0x80000000U;
  number.low32 = 7;
  value.type = type;
  value.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
  value.data = &number;
  CHECK(cpkt_opcua_server_setSessionAttribute_typed(
            server, &admin, key("custom"), &value) == 0);
  memset(&value, 0, sizeof(value));
  memset(&scalar, 0, sizeof(scalar));
  CHECK(cpkt_opcua_server_getSessionAttribute_typed(server, &admin,
                                                    key("custom"), view) == 0);
  CHECK(cpkt_opcua_server_native(server, borrowed_server, view) ==
        CPKT_OPCUA_OK);
  CHECK(cpkt_opcua_server_getSessionAttribute_scalar_typed(
            server, &admin, key("custom"), type, &scalar) == 0);
  CHECK(scalar.high32 == number.high32 && scalar.low32 == number.low32);
  CHECK(cpkt_opcua_VariantView_snapshot(view, &value) == 0);
  CHECK(((cpkt_opcua_Int64 *)value.data)->high32 == number.high32 &&
        ((cpkt_opcua_Int64 *)value.data)->low32 == number.low32);
  cpkt_opcua_Variant_clear(&value);
  CHECK(cpkt_opcua_server_getSessionAttribute_scalar_typed(
            server, &admin, key("custom"),
            cpkt_opcua_type_at(CPKT_OPCUA_TYPES_STRING),
            &description) == CPKT_OPCUA_STATUSCODE_BADNOTFOUND);
  CHECK(cpkt_opcua_server_deleteSessionAttribute_typed(server, &admin,
                                                       key("custom")) == 0);
  CHECK(cpkt_opcua_server_getSessionAttribute_typed(server, &admin,
                                                    key("custom"), view) ==
        CPKT_OPCUA_STATUSCODE_BADNOTFOUND);
  cpkt_opcua_VariantView_delete(view);
  CHECK(cpkt_opcua_server_delete_typed(server) == 0);
  CHECK(cpkt_opcua_ConnectionConfig_default(&connection) == 0 &&
        connection.sendBufferSize != 0 && connection.recvBufferSize != 0);
  CHECK(cpkt_opcua_ConnectionConfig_default(NULL) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_SECURITY_POLICY_NONE_URI.length ==
            strlen("http://opcfoundation.org/UA/SecurityPolicy#None") &&
        memcmp(cpkt_opcua_SECURITY_POLICY_NONE_URI.data,
               "http://opcfoundation.org/UA/SecurityPolicy#None",
               cpkt_opcua_SECURITY_POLICY_NONE_URI.length) == 0);
}
struct run_state {
  cpkt_opcua_RunFlag *flag;
  unsigned int calls;
  int interrupt;
};
static void stop_run(cpkt_opcua_server *server, void *context) {
  struct run_state *state = (struct run_state *)context;
  cpkt_opcua_ServerConfig *config = NULL;
  ++state->calls;
  CHECK(cpkt_opcua_server_delete_typed(server) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDSTATE);
  CHECK(cpkt_opcua_server_get_config_typed(server, &config) == 0);
  CHECK(cpkt_opcua_ServerConfig_setLogger(config, NULL) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDSTATE);
  if (state->interrupt)
    CHECK(raise(SIGINT) == 0);
  else
    cpkt_opcua_RunFlag_set(state->flag, 0);
}
static void lifecycle(void) {
  cpkt_opcua_server *server = NULL;
  cpkt_opcua_ServerConfig *config = NULL;
  cpkt_opcua_DateTime past;
  cpkt_opcua_LifecycleState lifecycle_state;
  struct run_state state;
  memset(&state, 0, sizeof(state));
  memset(&past, 0, sizeof(past));
  state.flag = cpkt_opcua_RunFlag_new(1);
  CHECK(state.flag != NULL && cpkt_opcua_RunFlag_get(state.flag));
  CHECK(cpkt_opcua_server_new(&server, 0) == CPKT_OPCUA_OK);
  CHECK(cpkt_opcua_server_get_config_typed(server, &config) == 0);
  CHECK(cpkt_opcua_ServerConfig_setLogger(config, NULL) == 0);
  CHECK(cpkt_opcua_server_startup(server, NULL) == CPKT_OPCUA_OK);
  CHECK(cpkt_opcua_server_delete_typed(server) != 0);
  CHECK(cpkt_opcua_server_getLifecycleState_typed(server, &lifecycle_state) ==
            0 &&
        lifecycle_state == CPKT_OPCUA_LIFECYCLESTATE_STARTED);
  CHECK(cpkt_opcua_server_shutdown(server, NULL) == CPKT_OPCUA_OK);
  CHECK(cpkt_opcua_server_addTimedCallback_typed(server, stop_run, &state, past,
                                                 NULL) == 0);
  CHECK(cpkt_opcua_server_run_typed(server, state.flag) == 0 &&
        state.calls == 1 && !cpkt_opcua_RunFlag_get(state.flag));
  CHECK(cpkt_opcua_server_getLifecycleState_typed(server, &lifecycle_state) ==
            0 &&
        lifecycle_state == CPKT_OPCUA_LIFECYCLESTATE_STOPPED);
  CHECK(cpkt_opcua_server_delete_typed(server) == 0);
  CHECK(cpkt_opcua_server_new(&server, 0) == CPKT_OPCUA_OK);
  CHECK(cpkt_opcua_server_get_config_typed(server, &config) == 0 &&
        cpkt_opcua_ServerConfig_setLogger(config, NULL) == 0);
  state.interrupt = 1;
  CHECK(cpkt_opcua_server_addTimedCallback_typed(server, stop_run, &state, past,
                                                 NULL) == 0);
  CHECK(cpkt_opcua_server_runUntilInterrupt_typed(server) == 0 &&
        state.calls == 2);
  CHECK(cpkt_opcua_server_delete_typed(server) == 0);
  cpkt_opcua_RunFlag_delete(state.flag);
  CHECK(cpkt_opcua_server_run_typed(NULL, NULL) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
}
static cpkt_opcua_server *reverse_server;
static cpkt_opcua_UInt64 reverse_handles[2];
static unsigned int reverse_removals;
static void reverse_log(void *context, cpkt_opcua_log_level level,
                        cpkt_opcua_log_category category, const char *format,
                        va_list args) {
  cpkt_opcua_String message;
  const char prefix[] = "Attempt to reverse reconnect";
  unsigned int i;
  (void)context;
  (void)level;
  (void)category;
  memset(&message, 0, sizeof(message));
  CHECK(cpkt_opcua_String_vformat(&message, format, args) == 0);
  if (!reverse_removals && message.length >= sizeof(prefix) - 1 &&
      memcmp(message.data, prefix, sizeof(prefix) - 1) == 0) {
    CHECK(cpkt_opcua_server_delete_typed(reverse_server) != 0);
    for (i = 0; i < 2; ++i)
      CHECK(cpkt_opcua_server_removeReverseConnect_typed(
                reverse_server, reverse_handles[i]) == 0);
    reverse_removals = 2;
  }
  cpkt_opcua_String_clear(&message);
}
static void reverse_reentrancy(void) {
  cpkt_opcua_ServerConfig *config = cpkt_opcua_ServerConfig_new();
  cpkt_opcua_ServerConfigSettings settings;
  cpkt_opcua_Logger logger;
  cpkt_opcua_status status;
  unsigned int i;
  CHECK(config != NULL);
  CHECK(cpkt_opcua_ServerConfig_setLogger(config, NULL) == 0);
  CHECK(cpkt_opcua_ServerConfig_setMinimal(config, 0, NULL) == 0);
  memset(&settings, 0, sizeof(settings));
  memset(&logger, 0, sizeof(logger));
  CHECK(cpkt_opcua_ServerConfig_getSettings(config, &settings) == 0);
  settings.reverseReconnectInterval = 1;
  CHECK(cpkt_opcua_ServerConfig_setSettings(config, &settings) == 0);
  cpkt_opcua_ServerConfigSettings_clear(&settings);
  CHECK(cpkt_opcua_server_newWithConfig_typed(config, &reverse_server) == 0);
  logger.log = reverse_log;
  CHECK(cpkt_opcua_ServerConfig_setLoggerPlugin(config, &logger) == 0);
  CHECK(cpkt_opcua_server_startup(reverse_server, &status) == CPKT_OPCUA_OK &&
        status == 0);
  memset(reverse_handles, 0, sizeof(reverse_handles));
  reverse_removals = 0;
  for (i = 0; i < 2; ++i) {
    (void)cpkt_opcua_server_addReverseConnect_typed(
        reverse_server, cpkt_opcua_STRING("opc.tcp://127.0.0.1:1"), NULL, NULL,
        &reverse_handles[i]);
    CHECK(reverse_handles[i].high32 || reverse_handles[i].low32);
  }
  for (i = 0; i < 100 && !reverse_removals; ++i)
    CHECK(cpkt_opcua_server_iterate(reverse_server, 1, NULL) == CPKT_OPCUA_OK);
  CHECK(reverse_removals == 2);
  CHECK(cpkt_opcua_server_shutdown(reverse_server, &status) == CPKT_OPCUA_OK &&
        status == 0);
  CHECK(cpkt_opcua_ServerConfig_setLogger(config, NULL) == 0);
  CHECK(cpkt_opcua_server_delete_typed(reverse_server) == 0);
  reverse_server = NULL;
}
void cpkt_types_test_operations(void) {
  attributes();
  reverse_reentrancy();
  lifecycle();
}
