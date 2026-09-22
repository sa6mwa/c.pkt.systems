#include <cpkt/sasl.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <sasl/sasl.h>

typedef char cpkt_sasl_unsigned_fits_public_type
    [(sizeof(unsigned) <= sizeof(unsigned long)) ? 1 : -1];

typedef struct cpkt_sasl_callback_owner {
  cpkt_sasl_callbacks callbacks;
  cpkt_sasl *public_receiver;
} cpkt_sasl_callback_owner;

typedef struct cpkt_sasl_state {
  sasl_conn_t *native;
  sasl_callback_t native_callbacks[15];
  cpkt_sasl_callback_owner callback_owner;
  sasl_interact_t *pending_native_interactions;
  cpkt_sasl_interaction *pending_public_interactions;
  unsigned long pending_interaction_count;
  int is_server;
} cpkt_sasl_state;

typedef struct cpkt_sasl_global_callbacks {
  cpkt_sasl_callback_owner callback_owner;
  sasl_callback_t native[15];
} cpkt_sasl_global_callbacks;

/* Cyrus SASL intentionally stores heterogeneous callback signatures in one
 * generic function-pointer slot.  Keep that ABI bridge private and explicit. */
typedef union cpkt_sasl_native_callback {
  int (*generic)(void);
  sasl_getopt_t *option;
  sasl_log_t *log;
  sasl_getpath_t *path;
  sasl_verifyfile_t *verify;
  sasl_getconfpath_t *configuration_path;
  sasl_getsimple_t *simple;
  sasl_getsecret_t *secret;
  sasl_chalprompt_t *challenge;
  sasl_getrealm_t *realm;
  sasl_authorize_t *authorize;
  sasl_server_userdb_checkpass_t *check_password;
  sasl_server_userdb_setpass_t *set_password;
  sasl_canon_user_t *canonicalize;
} cpkt_sasl_native_callback;

static cpkt_sasl_global_callbacks cpkt_sasl_client_callbacks;
static cpkt_sasl_global_callbacks cpkt_sasl_server_callbacks;

static cpkt_sasl_state *cpkt_sasl_state_for(const cpkt_sasl *self) {
  return self == NULL ? NULL : (cpkt_sasl_state *)self->internal;
}

static void cpkt_sasl_clear_pending_interactions(cpkt_sasl_state *state) {
  if (state == NULL)
    return;
  free(state->pending_public_interactions);
  state->pending_native_interactions = NULL;
  state->pending_public_interactions = NULL;
  state->pending_interaction_count = 0;
}

static int
cpkt_sasl_store_pending_interactions(cpkt_sasl_state *state,
                                     sasl_interact_t *native_interactions,
                                     cpkt_sasl_interaction **interactions) {
  cpkt_sasl_interaction *public_interactions;
  unsigned long count;
  unsigned long index;
  if (interactions != NULL)
    *interactions = NULL;
  cpkt_sasl_clear_pending_interactions(state);
  if (native_interactions == NULL)
    return SASL_INTERACT;
  count = 0;
  while (native_interactions[count].id != SASL_CB_LIST_END) {
    if (count == (unsigned long)-1 ||
        (size_t)count > ((size_t)-1) / sizeof(*public_interactions) - 1U) {
      return SASL_NOMEM;
    }
    ++count;
  }
  public_interactions = (cpkt_sasl_interaction *)calloc(
      (size_t)(count + 1U), sizeof(*public_interactions));
  if (public_interactions == NULL)
    return SASL_NOMEM;
  for (index = 0; index < count; ++index) {
    public_interactions[index].id = native_interactions[index].id;
    public_interactions[index].challenge = native_interactions[index].challenge;
    public_interactions[index].prompt = native_interactions[index].prompt;
    public_interactions[index].default_result =
        native_interactions[index].defresult;
    public_interactions[index].result = native_interactions[index].result;
    public_interactions[index].result_byte_count =
        (unsigned long)native_interactions[index].len;
  }
  public_interactions[count].id = SASL_CB_LIST_END;
  state->pending_native_interactions = native_interactions;
  state->pending_public_interactions = public_interactions;
  state->pending_interaction_count = count;
  if (interactions != NULL)
    *interactions = public_interactions;
  return SASL_INTERACT;
}

static int
cpkt_sasl_prepare_pending_interactions(cpkt_sasl_state *state,
                                       cpkt_sasl_interaction **interactions,
                                       sasl_interact_t **native_interactions) {
  unsigned long index;
  if (native_interactions != NULL)
    *native_interactions = NULL;
  if (state == NULL)
    return SASL_BADPARAM;
  if (state->pending_native_interactions == NULL) {
    return interactions != NULL && *interactions != NULL ? SASL_BADPARAM
                                                         : SASL_OK;
  }
  if (interactions == NULL ||
      *interactions != state->pending_public_interactions) {
    return SASL_BADPARAM;
  }
  for (index = 0; index < state->pending_interaction_count; ++index) {
    if (state->pending_public_interactions[index].result_byte_count >
        UINT_MAX) {
      return SASL_BADPARAM;
    }
    state->pending_native_interactions[index].result =
        state->pending_public_interactions[index].result;
    state->pending_native_interactions[index].len =
        (unsigned)state->pending_public_interactions[index].result_byte_count;
  }
  if (native_interactions != NULL)
    *native_interactions = state->pending_native_interactions;
  return SASL_OK;
}

static int cpkt_sasl_option_native(void *context, const char *plugin,
                                   const char *option, const char **result,
                                   unsigned *length) {
  cpkt_sasl_callback_owner *owner;
  unsigned long public_length;
  owner = (cpkt_sasl_callback_owner *)context;
  public_length = 0;
  if (owner == NULL || owner->callbacks.option == NULL)
    return SASL_FAIL;
  {
    int status;
    status = owner->callbacks.option(owner->callbacks.context, plugin, option,
                                     result, &public_length);
    if (length != NULL)
      *length = (unsigned)public_length;
    return status;
  }
}

static int cpkt_sasl_log_native(void *context, int level, const char *message) {
  cpkt_sasl_callback_owner *owner;
  owner = (cpkt_sasl_callback_owner *)context;
  return owner == NULL || owner->callbacks.log == NULL
             ? SASL_FAIL
             : owner->callbacks.log(owner->callbacks.context, level, message);
}

static int cpkt_sasl_path_native(void *context, const char **path) {
  cpkt_sasl_callback_owner *owner;
  owner = (cpkt_sasl_callback_owner *)context;
  return owner == NULL || owner->callbacks.plugin_path == NULL
             ? SASL_FAIL
             : owner->callbacks.plugin_path(owner->callbacks.context, path);
}

static int cpkt_sasl_verify_native(void *context, const char *path,
                                   sasl_verify_type_t type) {
  cpkt_sasl_callback_owner *owner;
  owner = (cpkt_sasl_callback_owner *)context;
  return owner == NULL || owner->callbacks.verify_file == NULL
             ? SASL_FAIL
             : owner->callbacks.verify_file(owner->callbacks.context, path,
                                            (int)type);
}

static int cpkt_sasl_configuration_path_native(void *context, char **path) {
  cpkt_sasl_callback_owner *owner;
  owner = (cpkt_sasl_callback_owner *)context;
  return owner == NULL || owner->callbacks.configuration_path == NULL
             ? SASL_FAIL
             : owner->callbacks.configuration_path(owner->callbacks.context,
                                                   path);
}

static int cpkt_sasl_simple_native(void *context, int id, const char **result,
                                   unsigned *length) {
  cpkt_sasl_callback_owner *owner;
  unsigned long public_length;
  int status;
  owner = (cpkt_sasl_callback_owner *)context;
  if (owner == NULL || owner->callbacks.simple == NULL)
    return SASL_FAIL;
  public_length = 0;
  status = owner->callbacks.simple(owner->callbacks.context, id, result,
                                   &public_length);
  if (length != NULL)
    *length = (unsigned)public_length;
  return status;
}

static int cpkt_sasl_secret_native(sasl_conn_t *native, void *context, int id,
                                   sasl_secret_t **secret) {
  cpkt_sasl_callback_owner *owner;
  (void)native;
  owner = (cpkt_sasl_callback_owner *)context;
  return owner == NULL || owner->callbacks.secret == NULL
             ? SASL_FAIL
             : owner->callbacks.secret(owner->public_receiver,
                                       owner->callbacks.context, id,
                                       (void **)secret);
}

static int cpkt_sasl_challenge_native(void *context, int id,
                                      const char *challenge, const char *prompt,
                                      const char *default_result,
                                      const char **result, unsigned *length) {
  cpkt_sasl_callback_owner *owner;
  unsigned long public_length;
  int status;
  owner = (cpkt_sasl_callback_owner *)context;
  if (owner == NULL || owner->callbacks.challenge == NULL)
    return SASL_FAIL;
  public_length = 0;
  status = owner->callbacks.challenge(owner->callbacks.context, id, challenge,
                                      prompt, default_result, result,
                                      &public_length);
  if (length != NULL)
    *length = (unsigned)public_length;
  return status;
}

static int cpkt_sasl_realm_native(void *context, int id, const char **available,
                                  const char **result) {
  cpkt_sasl_callback_owner *owner;
  owner = (cpkt_sasl_callback_owner *)context;
  return owner == NULL || owner->callbacks.realm == NULL
             ? SASL_FAIL
             : owner->callbacks.realm(owner->callbacks.context, id,
                                      (const char *const *)available, result);
}

static int cpkt_sasl_authorize_native(
    sasl_conn_t *native, void *context, const char *requested,
    unsigned requested_length, const char *identity, unsigned identity_length,
    const char *realm, unsigned realm_length, struct propctx *properties) {
  cpkt_sasl_callback_owner *owner;
  (void)properties;
  (void)native;
  owner = (cpkt_sasl_callback_owner *)context;
  return owner == NULL || owner->callbacks.authorize == NULL
             ? SASL_FAIL
             : owner->callbacks.authorize(
                   owner->public_receiver, owner->callbacks.context, requested,
                   (unsigned long)requested_length, identity,
                   (unsigned long)identity_length, realm,
                   (unsigned long)realm_length);
}

static int cpkt_sasl_check_password_native(sasl_conn_t *native, void *context,
                                           const char *user,
                                           const char *password,
                                           unsigned password_length,
                                           struct propctx *properties) {
  cpkt_sasl_callback_owner *owner;
  (void)properties;
  (void)native;
  owner = (cpkt_sasl_callback_owner *)context;
  return owner == NULL || owner->callbacks.check_password == NULL
             ? SASL_FAIL
             : owner->callbacks.check_password(
                   owner->public_receiver, owner->callbacks.context, user,
                   password, (unsigned long)password_length);
}

static int cpkt_sasl_set_password_native(sasl_conn_t *native, void *context,
                                         const char *user, const char *password,
                                         unsigned password_length,
                                         struct propctx *properties,
                                         unsigned flags) {
  cpkt_sasl_callback_owner *owner;
  (void)properties;
  (void)native;
  owner = (cpkt_sasl_callback_owner *)context;
  return owner == NULL || owner->callbacks.set_password == NULL
             ? SASL_FAIL
             : owner->callbacks.set_password(
                   owner->public_receiver, owner->callbacks.context, user,
                   password, (unsigned long)password_length,
                   (unsigned long)flags);
}

static int cpkt_sasl_canonicalize_native(sasl_conn_t *native, void *context,
                                         const char *input,
                                         unsigned input_length, unsigned flags,
                                         const char *realm, char *output,
                                         unsigned output_capacity,
                                         unsigned *output_length) {
  cpkt_sasl_callback_owner *owner;
  unsigned long public_length;
  int status;
  (void)native;
  owner = (cpkt_sasl_callback_owner *)context;
  if (owner == NULL || owner->callbacks.canonicalize == NULL)
    return SASL_FAIL;
  public_length = 0;
  status = owner->callbacks.canonicalize(
      owner->public_receiver, owner->callbacks.context, input,
      (unsigned long)input_length, (unsigned long)flags, realm, output,
      (unsigned long)output_capacity, &public_length);
  if (output_length != NULL)
    *output_length = (unsigned)public_length;
  return status;
}

static void cpkt_sasl_callbacks_build(sasl_callback_t *native,
                                      cpkt_sasl_callbacks *stored,
                                      const cpkt_sasl_callbacks *source,
                                      void *context) {
  int count;
  cpkt_sasl_native_callback callback;
  if (source == NULL)
    memset(stored, 0, sizeof(*stored));
  else
    *stored = *source;
  memset(native, 0, sizeof(sasl_callback_t) * 15U);
  count = 0;
#define CPKT_SASL_ADD_CALLBACK(identifier, public_field, native_field)         \
  if (stored->public_field != NULL) {                                          \
    native[count].id = identifier;                                             \
    callback.native_field = cpkt_sasl_##native_field##_native;                 \
    native[count].proc = callback.generic;                                     \
    native[count].context = context;                                           \
    ++count;                                                                   \
  }
  CPKT_SASL_ADD_CALLBACK(SASL_CB_GETOPT, option, option)
  CPKT_SASL_ADD_CALLBACK(SASL_CB_LOG, log, log)
  CPKT_SASL_ADD_CALLBACK(SASL_CB_GETPATH, plugin_path, path)
  CPKT_SASL_ADD_CALLBACK(SASL_CB_VERIFYFILE, verify_file, verify)
  CPKT_SASL_ADD_CALLBACK(SASL_CB_GETCONFPATH, configuration_path,
                         configuration_path)
  CPKT_SASL_ADD_CALLBACK(SASL_CB_USER, simple, simple)
  CPKT_SASL_ADD_CALLBACK(SASL_CB_AUTHNAME, simple, simple)
  CPKT_SASL_ADD_CALLBACK(SASL_CB_PASS, secret, secret)
  CPKT_SASL_ADD_CALLBACK(SASL_CB_ECHOPROMPT, challenge, challenge)
  CPKT_SASL_ADD_CALLBACK(SASL_CB_GETREALM, realm, realm)
  CPKT_SASL_ADD_CALLBACK(SASL_CB_PROXY_POLICY, authorize, authorize)
  CPKT_SASL_ADD_CALLBACK(SASL_CB_SERVER_USERDB_CHECKPASS, check_password,
                         check_password)
  CPKT_SASL_ADD_CALLBACK(SASL_CB_SERVER_USERDB_SETPASS, set_password,
                         set_password)
  CPKT_SASL_ADD_CALLBACK(SASL_CB_CANON_USER, canonicalize, canonicalize)
#undef CPKT_SASL_ADD_CALLBACK
  native[count].id = SASL_CB_LIST_END;
  native[count].proc = NULL;
  native[count].context = NULL;
}

static int cpkt_sasl_start(cpkt_sasl *self, const char *mechanisms,
                           cpkt_sasl_interaction **interactions,
                           const char **output, unsigned long *output_length,
                           const char **mechanism) {
  cpkt_sasl_state *state;
  unsigned native_length;
  state = cpkt_sasl_state_for(self);
  if (output != NULL)
    *output = NULL;
  if (output_length != NULL)
    *output_length = 0;
  if (mechanism != NULL)
    *mechanism = NULL;
  if (state == NULL || state->native == NULL)
    return SASL_BADPARAM;
  native_length = 0;
  if (state->is_server) {
    return SASL_BADPARAM;
  }
  {
    sasl_interact_t *native_interactions;
    int status;
    status = cpkt_sasl_prepare_pending_interactions(state, interactions,
                                                    &native_interactions);
    if (status != SASL_OK)
      return status;
    status = sasl_client_start(state->native, mechanisms, &native_interactions,
                               output, &native_length, mechanism);
    if (status == SASL_INTERACT) {
      status = cpkt_sasl_store_pending_interactions(state, native_interactions,
                                                    interactions);
    } else {
      cpkt_sasl_clear_pending_interactions(state);
      if (interactions != NULL)
        *interactions = NULL;
    }
    if (output_length != NULL)
      *output_length = (unsigned long)native_length;
    return status;
  }
}

static int cpkt_sasl_step(cpkt_sasl *self, const char *input,
                          unsigned long input_length,
                          cpkt_sasl_interaction **interactions,
                          const char **output, unsigned long *output_length) {
  cpkt_sasl_state *state;
  unsigned native_length;
  state = cpkt_sasl_state_for(self);
  if (output != NULL)
    *output = NULL;
  if (output_length != NULL)
    *output_length = 0;
  if (state == NULL || state->native == NULL || input_length > UINT_MAX)
    return SASL_BADPARAM;
  native_length = 0;
  if (state->is_server) {
    int status;
    status = sasl_server_step(state->native, input, (unsigned)input_length,
                              output, &native_length);
    if (output_length != NULL)
      *output_length = (unsigned long)native_length;
    return status;
  }
  {
    sasl_interact_t *native_interactions;
    int status;
    status = cpkt_sasl_prepare_pending_interactions(state, interactions,
                                                    &native_interactions);
    if (status != SASL_OK)
      return status;
    status = sasl_client_step(state->native, input, (unsigned)input_length,
                              &native_interactions, output, &native_length);
    if (status == SASL_INTERACT) {
      status = cpkt_sasl_store_pending_interactions(state, native_interactions,
                                                    interactions);
    } else {
      cpkt_sasl_clear_pending_interactions(state);
      if (interactions != NULL)
        *interactions = NULL;
    }
    if (output_length != NULL)
      *output_length = (unsigned long)native_length;
    return status;
  }
}

static int cpkt_sasl_server_start(cpkt_sasl *self, const char *mechanism,
                                  const char *input, unsigned long input_length,
                                  const char **output,
                                  unsigned long *output_length) {
  cpkt_sasl_state *state;
  unsigned native_length;
  int status;
  state = cpkt_sasl_state_for(self);
  if (output != NULL)
    *output = NULL;
  if (output_length != NULL)
    *output_length = 0;
  if (state == NULL || state->native == NULL || !state->is_server ||
      mechanism == NULL || input_length > UINT_MAX)
    return SASL_BADPARAM;
  native_length = 0;
  status = sasl_server_start(state->native, mechanism, input,
                             (unsigned)input_length, output, &native_length);
  if (output_length != NULL)
    *output_length = (unsigned long)native_length;
  return status;
}

static int cpkt_sasl_list_mechanisms(cpkt_sasl *self, const char *user,
                                     const char *prefix, const char *separator,
                                     const char *suffix, const char **result,
                                     unsigned long *length, int *count) {
  cpkt_sasl_state *state;
  unsigned native_length;
  int status;
  state = cpkt_sasl_state_for(self);
  if (result != NULL)
    *result = NULL;
  if (length != NULL)
    *length = 0;
  if (count != NULL)
    *count = 0;
  if (state == NULL || state->native == NULL)
    return SASL_BADPARAM;
  native_length = 0;
  status = sasl_listmech(state->native, user, prefix, separator, suffix, result,
                         &native_length, count);
  if (length != NULL)
    *length = (unsigned long)native_length;
  return status;
}

static int cpkt_sasl_encode(cpkt_sasl *self, const char *input,
                            unsigned long input_length, const char **output,
                            unsigned long *output_length) {
  cpkt_sasl_state *state;
  unsigned native_length;
  int status;
  state = cpkt_sasl_state_for(self);
  if (output != NULL)
    *output = NULL;
  if (output_length != NULL)
    *output_length = 0;
  if (state == NULL || state->native == NULL || input_length > UINT_MAX)
    return SASL_BADPARAM;
  native_length = 0;
  status = sasl_encode(state->native, input, (unsigned)input_length, output,
                       &native_length);
  if (output_length != NULL)
    *output_length = (unsigned long)native_length;
  return status;
}

static int cpkt_sasl_decode(cpkt_sasl *self, const char *input,
                            unsigned long input_length, const char **output,
                            unsigned long *output_length) {
  cpkt_sasl_state *state;
  unsigned native_length;
  int status;
  state = cpkt_sasl_state_for(self);
  if (output != NULL)
    *output = NULL;
  if (output_length != NULL)
    *output_length = 0;
  if (state == NULL || state->native == NULL || input_length > UINT_MAX)
    return SASL_BADPARAM;
  native_length = 0;
  status = sasl_decode(state->native, input, (unsigned)input_length, output,
                       &native_length);
  if (output_length != NULL)
    *output_length = (unsigned long)native_length;
  return status;
}

static int cpkt_sasl_set_external_ssf(cpkt_sasl *self, unsigned long value) {
  cpkt_sasl_state *state;
  unsigned native_value;
  state = cpkt_sasl_state_for(self);
  if (state == NULL || state->native == NULL || value > UINT_MAX)
    return SASL_BADPARAM;
  native_value = (unsigned)value;
  return sasl_setprop(state->native, SASL_SSF_EXTERNAL, &native_value);
}

static int cpkt_sasl_set_security_properties(
    cpkt_sasl *self, const cpkt_sasl_security_properties *properties) {
  cpkt_sasl_state *state;
  sasl_security_properties_t native;
  state = cpkt_sasl_state_for(self);
  if (state == NULL || state->native == NULL || properties == NULL ||
      properties->minimum_ssf > UINT_MAX ||
      properties->maximum_ssf > UINT_MAX ||
      properties->maximum_buffer_bytes > UINT_MAX ||
      properties->security_flags > UINT_MAX)
    return SASL_BADPARAM;
  native.min_ssf = (sasl_ssf_t)properties->minimum_ssf;
  native.max_ssf = (sasl_ssf_t)properties->maximum_ssf;
  native.maxbufsize = (unsigned)properties->maximum_buffer_bytes;
  native.security_flags = (unsigned)properties->security_flags;
  native.property_names = (const char **)properties->property_names;
  native.property_values = (const char **)properties->property_values;
  return sasl_setprop(state->native, SASL_SEC_PROPS, &native);
}

static int cpkt_sasl_set_external_authentication(cpkt_sasl *self,
                                                 const char *identity) {
  cpkt_sasl_state *state;
  state = cpkt_sasl_state_for(self);
  if (state == NULL || state->native == NULL)
    return SASL_BADPARAM;
  return sasl_setprop(state->native, SASL_AUTH_EXTERNAL, identity);
}

static const char *cpkt_sasl_error_detail(const cpkt_sasl *self) {
  cpkt_sasl_state *state;
  state = cpkt_sasl_state_for(self);
  return state == NULL || state->native == NULL ? NULL
                                                : sasl_errdetail(state->native);
}

/** Implements the documented public C89 SASL facade operation cpkt_sasl_close.
 */
void cpkt_sasl_close(cpkt_sasl *self) {
  cpkt_sasl_state *state;
  if (self == NULL)
    return;
  state = cpkt_sasl_state_for(self);
  if (state != NULL) {
    cpkt_sasl_clear_pending_interactions(state);
    if (state->native != NULL)
      sasl_dispose(&state->native);
  }
  free(state);
  self->internal = NULL;
  free(self);
}

/** Implements the documented public C89 SASL facade operation
 * cpkt_sasl_version. */
void cpkt_sasl_version(const char **implementation, const char **version,
                       int *major, int *minor, int *step, int *patch) {
  sasl_version_info(implementation, version, major, minor, step, patch);
}

/** Implements the documented public C89 SASL facade operation
 * cpkt_sasl_error_string. */
const char *cpkt_sasl_error_string(int status, const char *languages,
                                   const char **language) {
  return sasl_errstring(status, languages, language);
}

/** Implements the documented public C89 SASL facade operation
 * cpkt_sasl_set_path. */
int cpkt_sasl_set_path(int type, const char *path) {
  return sasl_set_path(type, (char *)path);
}

/** Implements the documented public C89 SASL facade operation
 * cpkt_sasl_client_initialize. */
int cpkt_sasl_client_initialize(const cpkt_sasl_callbacks *callbacks) {
  if (callbacks != NULL &&
      (callbacks->secret != NULL || callbacks->authorize != NULL ||
       callbacks->check_password != NULL || callbacks->set_password != NULL ||
       callbacks->canonicalize != NULL)) {
    return SASL_BADPARAM;
  }
  cpkt_sasl_callbacks_build(
      cpkt_sasl_client_callbacks.native,
      &cpkt_sasl_client_callbacks.callback_owner.callbacks, callbacks,
      &cpkt_sasl_client_callbacks.callback_owner);
  return sasl_client_init(
      callbacks == NULL ? NULL : cpkt_sasl_client_callbacks.native);
}

/** Implements the documented public C89 SASL facade operation
 * cpkt_sasl_server_initialize. */
int cpkt_sasl_server_initialize(const cpkt_sasl_callbacks *callbacks,
                                const char *application_name) {
  if (callbacks != NULL &&
      (callbacks->secret != NULL || callbacks->authorize != NULL ||
       callbacks->check_password != NULL || callbacks->set_password != NULL ||
       callbacks->canonicalize != NULL)) {
    return SASL_BADPARAM;
  }
  cpkt_sasl_callbacks_build(
      cpkt_sasl_server_callbacks.native,
      &cpkt_sasl_server_callbacks.callback_owner.callbacks, callbacks,
      &cpkt_sasl_server_callbacks.callback_owner);
  return sasl_server_init(callbacks == NULL ? NULL
                                            : cpkt_sasl_server_callbacks.native,
                          application_name);
}

/** Implements the documented public C89 SASL facade operation
 * cpkt_sasl_client_finish. */
int cpkt_sasl_client_finish(void) { return sasl_client_done(); }
/** Implements the documented public C89 SASL facade operation
 * cpkt_sasl_server_finish. */
int cpkt_sasl_server_finish(void) { return sasl_server_done(); }

static cpkt_sasl *cpkt_sasl_new(int is_server, const char *service,
                                const char *server_name, const char *realm,
                                const char *local_endpoint,
                                const char *remote_endpoint,
                                const cpkt_sasl_callbacks *callbacks,
                                unsigned long flags, int *status_out) {
  cpkt_sasl *self;
  cpkt_sasl_state *state;
  int status;
  if (status_out != NULL)
    *status_out = SASL_BADPARAM;
  if (service == NULL || flags > UINT_MAX)
    return NULL;
  self = (cpkt_sasl *)calloc(1, sizeof(*self));
  state = (cpkt_sasl_state *)calloc(1, sizeof(*state));
  if (self == NULL || state == NULL) {
    free(self);
    free(state);
    if (status_out != NULL)
      *status_out = SASL_NOMEM;
    return NULL;
  }
  state->is_server = is_server;
  state->callback_owner.public_receiver = self;
  cpkt_sasl_callbacks_build(state->native_callbacks,
                            &state->callback_owner.callbacks, callbacks,
                            &state->callback_owner);
  if (is_server) {
    status = sasl_server_new(service, server_name, realm, local_endpoint,
                             remote_endpoint,
                             callbacks == NULL ? NULL : state->native_callbacks,
                             (unsigned)flags, &state->native);
  } else {
    status =
        sasl_client_new(service, server_name, local_endpoint, remote_endpoint,
                        callbacks == NULL ? NULL : state->native_callbacks,
                        (unsigned)flags, &state->native);
  }
  if (status != SASL_OK) {
    free(state);
    free(self);
    if (status_out != NULL)
      *status_out = status;
    return NULL;
  }
  self->start = cpkt_sasl_start;
  self->step = cpkt_sasl_step;
  self->server_start = cpkt_sasl_server_start;
  self->list_mechanisms = cpkt_sasl_list_mechanisms;
  self->encode = cpkt_sasl_encode;
  self->decode = cpkt_sasl_decode;
  self->set_external_ssf = cpkt_sasl_set_external_ssf;
  self->set_security_properties = cpkt_sasl_set_security_properties;
  self->set_external_authentication = cpkt_sasl_set_external_authentication;
  self->error_detail = cpkt_sasl_error_detail;
  self->close = cpkt_sasl_close;
  self->internal = state;
  if (status_out != NULL)
    *status_out = SASL_OK;
  return self;
}

/** Implements the documented public C89 SASL facade operation
 * cpkt_sasl_client_new. */
cpkt_sasl *cpkt_sasl_client_new(const char *service, const char *server_name,
                                const char *local_endpoint,
                                const char *remote_endpoint,
                                const cpkt_sasl_callbacks *callbacks,
                                unsigned long flags, int *status_out) {
  return cpkt_sasl_new(0, service, server_name, NULL, local_endpoint,
                       remote_endpoint, callbacks, flags, status_out);
}

/** Implements the documented public C89 SASL facade operation
 * cpkt_sasl_server_new. */
cpkt_sasl *cpkt_sasl_server_new(const char *service, const char *server_name,
                                const char *user_realm,
                                const char *local_endpoint,
                                const char *remote_endpoint,
                                const cpkt_sasl_callbacks *callbacks,
                                unsigned long flags, int *status_out) {
  return cpkt_sasl_new(1, service, server_name, user_realm, local_endpoint,
                       remote_endpoint, callbacks, flags, status_out);
}
