#ifndef CPKT_SASL_H
#define CPKT_SASL_H

/**
 * @defgroup cpkt_sasl Cyrus SASL C89 facade
 *
 * Each receiver owns one SASL connection; close it after borrowed output and
 * interaction views are no longer needed. Callback records are copied, while
 * their context pointers remain caller-owned. See
 * docs/sasl-c89-facade-spec.md for callback and interaction lifetimes.
 * @{
 */

#include <stddef.h>

/** Receiver shell for one SASL connection; close with self->close(). */
typedef struct cpkt_sasl cpkt_sasl;

/** C89 Cyrus SASL facade declaration. See docs/sasl-c89-facade-spec.md. */
typedef struct cpkt_sasl_security_properties {
  unsigned long minimum_ssf;
  unsigned long maximum_ssf;
  unsigned long maximum_buffer_bytes;
  unsigned long security_flags;
  const char *const *property_names;
  const char *const *property_values;
} cpkt_sasl_security_properties;

/** Borrowed channel-binding bytes used while configuring a connection. */
typedef struct cpkt_sasl_channel_binding {
  const char *name;
  int critical;
  unsigned long byte_count;
  const unsigned char *data;
} cpkt_sasl_channel_binding;

/** C89 Cyrus SASL facade declaration. See docs/sasl-c89-facade-spec.md. */
typedef struct cpkt_sasl_http_request {
  const char *method;
  const char *uri;
  const unsigned char *entity;
  unsigned long entity_byte_count;
  unsigned long non_persistent;
} cpkt_sasl_http_request;

/** Borrowed interaction fields to fill before retrying the same start/step
 * call. */
typedef struct cpkt_sasl_interaction {
  unsigned long id;
  const char *challenge;
  const char *prompt;
  const char *default_result;
  const void *result;
  unsigned long result_byte_count;
} cpkt_sasl_interaction;

/** Password bytes are borrowed for the callback. The facade copies them into
 * receiver-owned storage before returning to the native client. */
typedef struct cpkt_sasl_secret {
  const unsigned char *data;
  unsigned long byte_count;
} cpkt_sasl_secret;

/** C89 Cyrus SASL facade declaration. See docs/sasl-c89-facade-spec.md. */
typedef int (*cpkt_sasl_option_callback)(void *context, const char *plugin_name,
                                         const char *option,
                                         const char **result,
                                         unsigned long *result_byte_count);
/** C89 Cyrus SASL facade declaration. See docs/sasl-c89-facade-spec.md. */
typedef int (*cpkt_sasl_log_callback)(void *context, int level,
                                      const char *message);
/** C89 Cyrus SASL facade declaration. See docs/sasl-c89-facade-spec.md. */
typedef int (*cpkt_sasl_path_callback)(void *context, const char **path);
/** C89 Cyrus SASL facade declaration. See docs/sasl-c89-facade-spec.md. */
typedef int (*cpkt_sasl_verify_file_callback)(void *context, const char *path,
                                              int type);
/** C89 Cyrus SASL facade declaration. See docs/sasl-c89-facade-spec.md. */
typedef int (*cpkt_sasl_configuration_path_callback)(void *context,
                                                     char **path_out);
/** C89 Cyrus SASL facade declaration. See docs/sasl-c89-facade-spec.md. */
typedef int (*cpkt_sasl_simple_callback)(void *context, int id,
                                         const char **result,
                                         unsigned long *result_byte_count);
/** C89 Cyrus SASL facade declaration. See docs/sasl-c89-facade-spec.md. */
typedef int (*cpkt_sasl_secret_callback)(cpkt_sasl *connection, void *context,
                                         int id,
                                         const cpkt_sasl_secret **secret_out);
/** C89 Cyrus SASL facade declaration. See docs/sasl-c89-facade-spec.md. */
typedef int (*cpkt_sasl_challenge_callback)(void *context, int id,
                                            const char *challenge,
                                            const char *prompt,
                                            const char *default_result,
                                            const char **result,
                                            unsigned long *result_byte_count);
/** C89 Cyrus SASL facade declaration. See docs/sasl-c89-facade-spec.md. */
typedef int (*cpkt_sasl_realm_callback)(void *context, int id,
                                        const char *const *available_realms,
                                        const char **result);
/** C89 Cyrus SASL facade declaration. See docs/sasl-c89-facade-spec.md. */
typedef int (*cpkt_sasl_authorize_callback)(
    cpkt_sasl *connection, void *context, const char *requested_user,
    unsigned long requested_length, const char *authentication_identity,
    unsigned long authentication_length, const char *default_realm,
    unsigned long realm_length);
/** C89 Cyrus SASL facade declaration. See docs/sasl-c89-facade-spec.md. */
typedef int (*cpkt_sasl_check_password_callback)(cpkt_sasl *connection,
                                                 void *context,
                                                 const char *user,
                                                 const char *password,
                                                 unsigned long password_length);
/** C89 Cyrus SASL facade declaration. See docs/sasl-c89-facade-spec.md. */
typedef int (*cpkt_sasl_set_password_callback)(cpkt_sasl *connection,
                                               void *context, const char *user,
                                               const char *password,
                                               unsigned long password_length,
                                               unsigned long flags);
/** C89 Cyrus SASL facade declaration. See docs/sasl-c89-facade-spec.md. */
typedef int (*cpkt_sasl_canonicalize_callback)(
    cpkt_sasl *connection, void *context, const char *input,
    unsigned long input_length, unsigned long flags, const char *realm,
    char *output, unsigned long output_capacity, unsigned long *output_length);

/** Zero-initialization installs no callbacks. The facade copies the record but
 * does not own context or data returned by callbacks. */
typedef struct cpkt_sasl_callbacks {
  void *context;
  cpkt_sasl_option_callback option;
  cpkt_sasl_log_callback log;
  cpkt_sasl_path_callback plugin_path;
  cpkt_sasl_verify_file_callback verify_file;
  cpkt_sasl_configuration_path_callback configuration_path;
  cpkt_sasl_simple_callback simple;
  cpkt_sasl_secret_callback secret;
  cpkt_sasl_challenge_callback challenge;
  cpkt_sasl_realm_callback realm;
  cpkt_sasl_authorize_callback authorize;
  cpkt_sasl_check_password_callback check_password;
  cpkt_sasl_set_password_callback set_password;
  cpkt_sasl_canonicalize_callback canonicalize;
} cpkt_sasl_callbacks;

enum cpkt_sasl_constants {
  CPKT_SASL_OK = 0,
  CPKT_SASL_CONTINUE = 1,
  CPKT_SASL_INTERACT = 2,
  CPKT_SASL_FAIL = -1,
  CPKT_SASL_NOMEM = -2,
  CPKT_SASL_BADPARAM = -7,
  CPKT_SASL_NOMECH = -4,
  CPKT_SASL_NOTDONE = -13,
  CPKT_SASL_PATH_PLUGIN = 0,
  CPKT_SASL_PATH_CONFIGURATION = 1,
  CPKT_SASL_SECURITY_NO_PLAINTEXT = 0x0001,
  CPKT_SASL_SECURITY_NO_ACTIVE = 0x0002,
  CPKT_SASL_SECURITY_NO_DICTIONARY = 0x0004,
  CPKT_SASL_SECURITY_FORWARD_SECRECY = 0x0008,
  CPKT_SASL_SECURITY_NO_ANONYMOUS = 0x0010,
  CPKT_SASL_SECURITY_PASS_CREDENTIALS = 0x0020,
  CPKT_SASL_SECURITY_MUTUAL_AUTH = 0x0040,
  CPKT_SASL_USE_SUCCESS_DATA = 0x0004,
  CPKT_SASL_NEED_PROXY = 0x0008,
  CPKT_SASL_NEED_HTTP = 0x0010,
  CPKT_SASL_PROPERTY_USERNAME = 0,
  CPKT_SASL_PROPERTY_SSF = 1,
  CPKT_SASL_PROPERTY_MAXIMUM_OUTPUT = 2,
  CPKT_SASL_PROPERTY_DEFAULT_REALM = 3,
  CPKT_SASL_PROPERTY_EXTERNAL_SSF = 100,
  CPKT_SASL_PROPERTY_SECURITY = 101,
  CPKT_SASL_PROPERTY_EXTERNAL_AUTHENTICATION = 102
};

/* Receiver shell. Returned text is provider-owned until the next matching
 * operation or receiver close, exactly as documented by Cyrus SASL. When
 * start or step returns CPKT_SASL_INTERACT, fill the returned interaction
 * records and pass that same pointer back to the same operation. */
struct cpkt_sasl {
  int (*start)(cpkt_sasl *self, const char *mechanisms,
               cpkt_sasl_interaction **interactions_out,
               const char **output_out, unsigned long *output_byte_count,
               const char **mechanism_out);
  int (*step)(cpkt_sasl *self, const char *input,
              unsigned long input_byte_count,
              cpkt_sasl_interaction **interactions_out, const char **output_out,
              unsigned long *output_byte_count);
  int (*server_start)(cpkt_sasl *self, const char *mechanism, const char *input,
                      unsigned long input_byte_count, const char **output_out,
                      unsigned long *output_byte_count);
  int (*list_mechanisms)(cpkt_sasl *self, const char *user, const char *prefix,
                         const char *separator, const char *suffix,
                         const char **result_out,
                         unsigned long *result_byte_count, int *count_out);
  int (*encode)(cpkt_sasl *self, const char *input,
                unsigned long input_byte_count, const char **output_out,
                unsigned long *output_byte_count);
  int (*decode)(cpkt_sasl *self, const char *input,
                unsigned long input_byte_count, const char **output_out,
                unsigned long *output_byte_count);
  int (*set_external_ssf)(cpkt_sasl *self, unsigned long value);
  int (*set_security_properties)(
      cpkt_sasl *self, const cpkt_sasl_security_properties *properties);
  int (*set_external_authentication)(cpkt_sasl *self, const char *identity);
  const char *(*error_detail)(const cpkt_sasl *self);
  void (*close)(cpkt_sasl *self);
  void *internal;
};

/** C89 Cyrus SASL facade declaration. See docs/sasl-c89-facade-spec.md. */
void cpkt_sasl_version(const char **implementation_out,
                       const char **version_out, int *major_out, int *minor_out,
                       int *step_out, int *patch_out);
/** C89 Cyrus SASL facade declaration. See docs/sasl-c89-facade-spec.md. */
const char *cpkt_sasl_error_string(int status, const char *languages,
                                   const char **language_out);
/** C89 Cyrus SASL facade declaration. See docs/sasl-c89-facade-spec.md. */
int cpkt_sasl_set_path(int type, const char *path);
/* Global initialization accepts process callbacks only. Receiver callbacks
 * (secret, authorize, password, and canonicalize) belong on a new receiver.
 * Repeated initialization retains the first successful callback record until
 * its matching final finish call. */
/** C89 Cyrus SASL facade declaration. See docs/sasl-c89-facade-spec.md. */
int cpkt_sasl_client_initialize(const cpkt_sasl_callbacks *callbacks);
/** C89 Cyrus SASL facade declaration. See docs/sasl-c89-facade-spec.md. */
int cpkt_sasl_server_initialize(const cpkt_sasl_callbacks *callbacks,
                                const char *application_name);
/** C89 Cyrus SASL facade declaration. See docs/sasl-c89-facade-spec.md. */
int cpkt_sasl_client_finish(void);
/** C89 Cyrus SASL facade declaration. See docs/sasl-c89-facade-spec.md. */
int cpkt_sasl_server_finish(void);
/** Creates an owned client receiver; inspect status_out and close when done. */
cpkt_sasl *cpkt_sasl_client_new(const char *service, const char *server_name,
                                const char *local_endpoint,
                                const char *remote_endpoint,
                                const cpkt_sasl_callbacks *callbacks,
                                unsigned long flags, int *status_out);
/** Creates an owned server receiver; inspect status_out and close when done. */
cpkt_sasl *cpkt_sasl_server_new(const char *service, const char *server_name,
                                const char *user_realm,
                                const char *local_endpoint,
                                const char *remote_endpoint,
                                const cpkt_sasl_callbacks *callbacks,
                                unsigned long flags, int *status_out);
/** Closes a receiver and invalidates its borrowed outputs. NULL is accepted. */
void cpkt_sasl_close(cpkt_sasl *self);

/** @} */
#endif
