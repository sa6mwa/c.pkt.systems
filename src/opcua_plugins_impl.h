/* Private public-plugin bridge, included by the type converter's C TU. */
#include <open62541/plugin/accesscontrol.h>
#include <open62541/plugin/historydatabase.h>
typedef struct {
  cpkt_opcua_server *owner;
  cpkt_opcua_AccessControl plugin;
} cpkt_ac_bridge;
typedef struct {
  cpkt_opcua_server *owner;
  cpkt_opcua_HistoryDatabase plugin;
} cpkt_hdb_bridge;
#include "opcua_plugins_metadata.inc"
static void cpkt_ac_clear(UA_AccessControl *native) {
  cpkt_ac_bridge *bridge = (cpkt_ac_bridge *)native->context;
  cpkt_opcua_UserTokenPolicy *policies = bridge->plugin.userTokenPolicies;
  size_t count = bridge->plugin.userTokenPoliciesSize;
  if (bridge->plugin.clear)
    bridge->plugin.clear(&bridge->plugin);
  cpkt_opcua_array_delete(policies, count,
                          &cpkt_types[CPKT_OPCUA_TYPES_USERTOKENPOLICY]);
  UA_Array_delete(native->userTokenPolicies, native->userTokenPoliciesSize,
                  cpkt_types[CPKT_OPCUA_TYPES_USERTOKENPOLICY].native);
  memset(native, 0, sizeof(*native));
  UA_free(bridge);
}
/** Installs the generated C89 access-control plugin before server startup. */
cpkt_opcua_StatusCode cpkt_opcua_server_set_access_control_plugin(
    cpkt_opcua_server *server, const cpkt_opcua_AccessControl *plugin) {
  cpkt_ac_bridge *bridge;
  UA_ServerConfig *configuration;
  UA_AccessControl native;
  UA_StatusCode status;
  void *policies = NULL;
  void *native_policies = NULL;
  if (!server || !plugin || server->started || !plugin->activateSession ||
      !plugin->getUserRightsMask || !plugin->getUserAccessLevel ||
      !plugin->getUserExecutable || !plugin->getUserExecutableOnObject)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  bridge = (cpkt_ac_bridge *)UA_calloc(1, sizeof(*bridge));
  if (!bridge)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  status = cpkt_opcua_array_copy(plugin->userTokenPolicies,
                                 plugin->userTokenPoliciesSize, &policies,
                                 &cpkt_types[CPKT_OPCUA_TYPES_USERTOKENPOLICY]);
  if (!status)
    status = cpkt_array(plugin->userTokenPolicies,
                        plugin->userTokenPoliciesSize, &native_policies,
                        &cpkt_types[CPKT_OPCUA_TYPES_USERTOKENPOLICY], 1, 0);
  if (status) {
    cpkt_opcua_array_delete(policies, plugin->userTokenPoliciesSize,
                            &cpkt_types[CPKT_OPCUA_TYPES_USERTOKENPOLICY]);
    if (native_policies)
      UA_Array_delete(native_policies, plugin->userTokenPoliciesSize,
                      cpkt_types[CPKT_OPCUA_TYPES_USERTOKENPOLICY].native);
    UA_free(bridge);
    return status;
  }
  bridge->owner = server;
  bridge->plugin = *plugin;
  bridge->plugin.userTokenPolicies = (cpkt_opcua_UserTokenPolicy *)policies;
  cpkt_ac_assign(&native, plugin);
  native.context = bridge;
  native.clear = cpkt_ac_clear;
  native.userTokenPoliciesSize = plugin->userTokenPoliciesSize;
  native.userTokenPolicies = (UA_UserTokenPolicy *)native_policies;
  configuration = UA_Server_getConfig(server->server);
  if (configuration->accessControl.clear)
    configuration->accessControl.clear(&configuration->accessControl);
  configuration->accessControl = native;
  return 0;
}
static void cpkt_hdb_clear(UA_HistoryDatabase *native) {
  cpkt_hdb_bridge *bridge = (cpkt_hdb_bridge *)native->context;
  if (bridge->plugin.clear)
    bridge->plugin.clear(&bridge->plugin);
  memset(native, 0, sizeof(*native));
  UA_free(bridge);
}
/** Installs the full C89 history plugin using generated native trampolines. */
cpkt_opcua_StatusCode cpkt_opcua_server_set_history_database_plugin(
    cpkt_opcua_server *server, const cpkt_opcua_HistoryDatabase *plugin) {
  cpkt_hdb_bridge *bridge;
  UA_ServerConfig *configuration;
  UA_HistoryDatabase native;
  if (!server || !plugin || server->started)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  bridge = (cpkt_hdb_bridge *)UA_calloc(1, sizeof(*bridge));
  if (!bridge)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  bridge->owner = server;
  bridge->plugin = *plugin;
  cpkt_hdb_assign(&native, plugin);
  native.context = bridge;
  native.clear = cpkt_hdb_clear;
  configuration = UA_Server_getConfig(server->server);
  if (configuration->historyDatabase.clear)
    configuration->historyDatabase.clear(&configuration->historyDatabase);
  configuration->historyDatabase = native;
  return 0;
}
