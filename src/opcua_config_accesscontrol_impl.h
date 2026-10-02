/* Stock access-control installation and its C89 login callback. The native
 * backend context stays untouched. Its actual clear owns the login bridge. */
#ifndef CPKT_OPCUA_CONFIG_ACCESSCONTROL_IMPL_H
#define CPKT_OPCUA_CONFIG_ACCESSCONTROL_IMPL_H
#include <open62541/plugin/accesscontrol_default.h>
typedef struct cpkt_cfg_login_bridge cpkt_cfg_login_bridge;
struct cpkt_cfg_login_bridge {
  cpkt_cfg_login_bridge *next;
  void *native_context;
  void (*native_clear)(UA_AccessControl *);
  cpkt_opcua_ServerConfig *config;
  cpkt_opcua_UsernamePasswordLoginCallback callback;
  void *context;
};
static cpkt_cfg_login_bridge *cpkt_cfg_logins;
static pthread_mutex_t cpkt_cfg_login_mutex = PTHREAD_MUTEX_INITIALIZER;
static void cpkt_cfg_login_clear(UA_AccessControl *native) {
  cpkt_cfg_login_bridge **slot, *bridge = NULL;
  (void)pthread_mutex_lock(&cpkt_cfg_login_mutex);
  for (slot = &cpkt_cfg_logins; *slot; slot = &(*slot)->next)
    if ((*slot)->native_context == native->context) {
      bridge = *slot;
      *slot = bridge->next;
      break;
    }
  (void)pthread_mutex_unlock(&cpkt_cfg_login_mutex);
  if (!bridge)
    return;
  cpkt_cfg_server_enter(bridge->config);
  if (bridge->native_clear)
    bridge->native_clear(native);
  cpkt_cfg_server_leave(bridge->config);
  UA_free(bridge);
}
static UA_StatusCode cpkt_cfg_login(const UA_String *userName,
                                    const UA_ByteString *password, size_t count,
                                    const UA_UsernamePasswordLogin *users,
                                    void **sessionContext, void *context) {
  cpkt_cfg_login_bridge *bridge = (cpkt_cfg_login_bridge *)context;
  cpkt_opcua_UsernamePasswordLogin *views = NULL;
  cpkt_opcua_String name, secret;
  UA_StatusCode status;
  size_t i;
  if (count && (!users || count > (size_t)-1 / sizeof(*views)))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (count) {
    views =
        (cpkt_opcua_UsernamePasswordLogin *)UA_calloc(count, sizeof(*views));
    if (!views)
      return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  for (i = 0; i < count; ++i) {
    cpkt_string_take(&views[i].username, users[i].username);
    cpkt_string_take(&views[i].password, users[i].password);
  }
  memset(&name, 0, sizeof(name));
  memset(&secret, 0, sizeof(secret));
  if (userName)
    cpkt_string_take(&name, *userName);
  if (password)
    cpkt_string_take(&secret, *password);
  cpkt_cfg_server_enter(bridge->config);
  status = bridge->callback(userName ? &name : NULL, password ? &secret : NULL,
                            count, views, sessionContext, bridge->context);
  cpkt_cfg_server_leave(bridge->config);
  UA_free(views);
  return status;
}
static UA_StatusCode
cpkt_cfg_access_default(cpkt_opcua_ServerConfig *config,
                        cpkt_opcua_Boolean allowAnonymous,
                        const cpkt_opcua_String *uri, size_t count,
                        const cpkt_opcua_UsernamePasswordLogin *users,
                        cpkt_opcua_UsernamePasswordLoginCallback callback,
                        void *context, int with_callback) {
  UA_UsernamePasswordLogin *values = NULL;
  cpkt_cfg_login_bridge *bridge = NULL;
  UA_String native_uri;
  UA_StatusCode status;
  size_t i;
  if (!config || !config->native ||
      (count && (!users || users == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL)) ||
      count > (size_t)-1 / sizeof(*values) || (uri && !cpkt_string_valid(uri)))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!cpkt_cfg_server_mutable(config))
    return UA_STATUSCODE_BADINVALIDSTATE;
  for (i = 0; i < count; ++i)
    if (!cpkt_string_valid(&users[i].username) ||
        !cpkt_string_valid(&users[i].password))
      return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (count) {
    values = (UA_UsernamePasswordLogin *)UA_calloc(count, sizeof(*values));
    if (!values)
      return UA_STATUSCODE_BADOUTOFMEMORY;
    for (i = 0; i < count; ++i) {
      values[i].username = cpkt_string_view(&users[i].username);
      values[i].password = cpkt_string_view(&users[i].password);
    }
  }
  if (with_callback && callback) {
    bridge = (cpkt_cfg_login_bridge *)UA_calloc(1, sizeof(*bridge));
    if (!bridge) {
      UA_free(values);
      return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    bridge->config = config;
    bridge->callback = callback;
    bridge->context = context;
  }
  memset(&native_uri, 0, sizeof(native_uri));
  if (uri)
    native_uri = cpkt_string_view(uri);
  cpkt_cfg_server_enter(config);
  status =
      with_callback
          ? UA_AccessControl_defaultWithLoginCallback(
                config->native, allowAnonymous, uri ? &native_uri : NULL, count,
                values, callback ? cpkt_cfg_login : NULL,
                bridge ? bridge : context)
          : UA_AccessControl_default(config->native, allowAnonymous,
                                     uri ? &native_uri : NULL, count, values);
  if (!status && bridge) {
    bridge->native_context = config->native->accessControl.context;
    bridge->native_clear = config->native->accessControl.clear;
    (void)pthread_mutex_lock(&cpkt_cfg_login_mutex);
    bridge->next = cpkt_cfg_logins;
    cpkt_cfg_logins = bridge;
    (void)pthread_mutex_unlock(&cpkt_cfg_login_mutex);
    config->native->accessControl.clear = cpkt_cfg_login_clear;
    bridge = NULL;
  }
  cpkt_cfg_server_leave(config);
  UA_free(bridge);
  UA_free(values);
  return status;
}
cpkt_opcua_StatusCode cpkt_opcua_AccessControl_default(
    cpkt_opcua_ServerConfig *config, cpkt_opcua_Boolean allowAnonymous,
    const cpkt_opcua_String *userTokenPolicyUri,
    size_t usernamePasswordLoginSize,
    const cpkt_opcua_UsernamePasswordLogin *usernamePasswordLogin) {
  return cpkt_cfg_access_default(config, allowAnonymous, userTokenPolicyUri,
                                 usernamePasswordLoginSize,
                                 usernamePasswordLogin, NULL, NULL, 0);
}
cpkt_opcua_StatusCode cpkt_opcua_AccessControl_defaultWithLoginCallback(
    cpkt_opcua_ServerConfig *config, cpkt_opcua_Boolean allowAnonymous,
    const cpkt_opcua_String *userTokenPolicyUri,
    size_t usernamePasswordLoginSize,
    const cpkt_opcua_UsernamePasswordLogin *usernamePasswordLogin,
    cpkt_opcua_UsernamePasswordLoginCallback loginCallback,
    void *loginContext) {
  return cpkt_cfg_access_default(
      config, allowAnonymous, userTokenPolicyUri, usernamePasswordLoginSize,
      usernamePasswordLogin, loginCallback, loginContext, 1);
}
#endif
