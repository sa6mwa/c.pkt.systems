/* Public security plugin storage; never duplicates private crypto/store types.
 */
typedef struct {
  UA_CertificateGroup native;
  struct cpkt_opcua_logger logger;
} cpkt_cg_stock;
typedef struct {
  cpkt_opcua_CertificateGroup plugin;
  struct cpkt_opcua_logger logger;
} cpkt_cg_custom;
static void cpkt_cg_custom_assign(UA_CertificateGroup *,
                                  const cpkt_opcua_CertificateGroup *);
static void cpkt_cg_stock_assign(cpkt_opcua_CertificateGroup *,
                                 cpkt_cg_stock *);
static UA_StatusCode
cpkt_cg_sync_metadata(UA_CertificateGroup *native,
                      const cpkt_opcua_CertificateGroup *group,
                      struct cpkt_opcua_logger *logger) {
  UA_NodeId id, view;
  UA_StatusCode status;
  cpkt_opcua_log_config quiet;
  if (!cpkt_logger_valid(group->logging))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if ((group->certificateGroupId.identifierType ==
           CPKT_OPCUA_NODEIDTYPE_STRING &&
       group->certificateGroupId.identifier.string.length &&
       !group->certificateGroupId.identifier.string.data) ||
      (group->certificateGroupId.identifierType ==
           CPKT_OPCUA_NODEIDTYPE_BYTESTRING &&
       group->certificateGroupId.identifier.byteString.length &&
       !group->certificateGroupId.identifier.byteString.data))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  view = cpkt_nodeid_view(&group->certificateGroupId);
  if (!UA_NodeId_equal(&view, &native->certificateGroupId)) {
    UA_NodeId_init(&id);
    status = cpkt_convert(&group->certificateGroupId, &id,
                          &cpkt_types[CPKT_OPCUA_TYPES_NODEID], 1, 0);
    if (status) {
      UA_NodeId_clear(&id);
      return status;
    }
    UA_NodeId_clear(&native->certificateGroupId);
    native->certificateGroupId = id;
  }
  memset(&quiet, 0, sizeof(quiet));
  cpkt_logger_set(logger, &logger->native,
                  group->logging ? group->logging : &quiet);
  native->logging = &logger->native;
  return 0;
}
static UA_StatusCode
cpkt_cg_stock_sync(cpkt_cg_stock *store,
                   const cpkt_opcua_CertificateGroup *group) {
  return cpkt_cg_sync_metadata(&store->native, group, &store->logger);
}
static void cpkt_cg_stock_clear(cpkt_opcua_CertificateGroup *group) {
  cpkt_cg_stock *store;
  if (!group)
    return;
  store = (cpkt_cg_stock *)group->context;
  if (store) {
    if (store->native.clear)
      store->native.clear(&store->native);
    UA_NodeId_clear(&store->native.certificateGroupId);
    UA_free(store);
  }
  cpkt_opcua_NodeId_clear(&group->certificateGroupId);
  memset(group, 0, sizeof(*group));
}
static void cpkt_cg_custom_clear(UA_CertificateGroup *native) {
  cpkt_cg_custom *store = (cpkt_cg_custom *)native->context;
  if (store) {
    if (store->plugin.clear)
      store->plugin.clear(&store->plugin);
    UA_free(store);
  }
  UA_NodeId_clear(&native->certificateGroupId);
  memset(native, 0, sizeof(*native));
}
static int cpkt_string_valid(const cpkt_opcua_String *);
/* Installation consumes an entire owned public record, including its metadata.
 * Native and C89 NodeId copies have separate ownership at this ABI boundary. */
static void cpkt_cfg_view_cg_clear(cpkt_opcua_CertificateGroup *);
static void cpkt_cfg_view_sp_clear(cpkt_opcua_SecurityPolicy *);
static void cpkt_cfg_view_psp_clear(cpkt_opcua_PubSubSecurityPolicy *);
static UA_StatusCode cpkt_cg_install(UA_CertificateGroup *destination,
                                     cpkt_opcua_CertificateGroup *group) {
  cpkt_cg_custom *store;
  UA_CertificateGroup native;
  UA_StatusCode status;
  if (!group || !group->verifyCertificate || !group->clear ||
      group->clear == cpkt_cfg_view_cg_clear ||
      !cpkt_logger_valid(group->logging))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memset(&native, 0, sizeof(native));
  store = (cpkt_cg_custom *)UA_calloc(1, sizeof(*store));
  if (!store)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  status = cpkt_cg_sync_metadata(&native, group, &store->logger);
  if (status) {
    UA_NodeId_clear(&native.certificateGroupId);
    UA_free(store);
    return status;
  }
  store->plugin = *group;
  native.context = store;
  cpkt_cg_custom_assign(&native, group);
  if (destination->clear)
    destination->clear(destination);
  *destination = native;
  memset(group, 0, sizeof(*group));
  return 0;
}
cpkt_opcua_StatusCode
cpkt_opcua_server_set_certificate_group(cpkt_opcua_server *server,
                                        cpkt_opcua_Boolean session,
                                        cpkt_opcua_CertificateGroup *group) {
  UA_ServerConfig *config;
  if (!server || !server->server || server->started || server->destroying)
    return UA_STATUSCODE_BADINVALIDSTATE;
  config = UA_Server_getConfig(server->server);
  return cpkt_cg_install(
      session ? &config->sessionPKI : &config->secureChannelPKI, group);
}
cpkt_opcua_StatusCode
cpkt_opcua_client_set_certificate_group(cpkt_opcua_client *client,
                                        cpkt_opcua_CertificateGroup *group) {
  UA_SecureChannelState state;
  if (!client || !client->client)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  UA_Client_getState(client->client, &state, NULL, NULL);
  if (state != UA_SECURECHANNELSTATE_CLOSED)
    return UA_STATUSCODE_BADINVALIDSTATE;
  return cpkt_cg_install(
      &UA_Client_getConfig(client->client)->certificateVerification, group);
}
