#ifndef CPKT_POSTGRES_H
#define CPKT_POSTGRES_H

/*
 * C89 facade for PostgreSQL.  This header deliberately does not expose
 * native client headers, C99 integer typedefs, or provider-specific TLS/GSS
 * objects.
 */

#include <stddef.h>
#include <stdio.h>

typedef struct cpkt_postgres_connection cpkt_postgres_connection;
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef struct cpkt_postgres_result cpkt_postgres_result;
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef struct cpkt_postgres_cancel cpkt_postgres_cancel;
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef struct cpkt_postgres_cancel_connection cpkt_postgres_cancel_connection;
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef struct cpkt_postgres_tls_object cpkt_postgres_tls_object;
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef struct cpkt_postgres_gss_context cpkt_postgres_gss_context;
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef struct cpkt_postgres cpkt_postgres;

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef void (*cpkt_postgres_notice_receiver)(
    void *context, cpkt_postgres_connection *connection,
    const cpkt_postgres_result *result);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef void (*cpkt_postgres_notice_processor)(
    void *context, cpkt_postgres_connection *connection, const char *message);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef void (*cpkt_postgres_thread_lock)(int acquire);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef int (*cpkt_postgres_ssl_key_password_hook)(
    char *buffer, int buffer_size, cpkt_postgres_connection *connection);

/* PostgreSQL OIDs are unsigned 32-bit protocol identifiers. */
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef unsigned long cpkt_postgres_oid;

/* Exact 64-bit two's-complement bits, represented without a non-C89 scalar. */
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef struct cpkt_postgres_i64 {
  unsigned long high;
  unsigned long low;
} cpkt_postgres_i64;

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef enum cpkt_postgres_connection_status {
  CPKT_POSTGRES_CONNECTION_OK = 0,
  CPKT_POSTGRES_CONNECTION_BAD = 1,
  CPKT_POSTGRES_CONNECTION_STARTED = 2,
  CPKT_POSTGRES_CONNECTION_MADE = 3,
  CPKT_POSTGRES_CONNECTION_AWAITING_RESPONSE = 4,
  CPKT_POSTGRES_CONNECTION_AUTH_OK = 5,
  CPKT_POSTGRES_CONNECTION_SETENV = 6,
  CPKT_POSTGRES_CONNECTION_SSL_STARTUP = 7,
  CPKT_POSTGRES_CONNECTION_NEEDED = 8,
  CPKT_POSTGRES_CONNECTION_CHECK_WRITABLE = 9,
  CPKT_POSTGRES_CONNECTION_CONSUME = 10,
  CPKT_POSTGRES_CONNECTION_GSS_STARTUP = 11,
  CPKT_POSTGRES_CONNECTION_CHECK_TARGET = 12,
  CPKT_POSTGRES_CONNECTION_CHECK_STANDBY = 13,
  CPKT_POSTGRES_CONNECTION_ALLOCATED = 14,
  CPKT_POSTGRES_CONNECTION_AUTHENTICATING = 15
} cpkt_postgres_connection_status;

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef enum cpkt_postgres_poll_status {
  CPKT_POSTGRES_POLL_FAILED = 0,
  CPKT_POSTGRES_POLL_READING = 1,
  CPKT_POSTGRES_POLL_WRITING = 2,
  CPKT_POSTGRES_POLL_OK = 3,
  CPKT_POSTGRES_POLL_ACTIVE = 4
} cpkt_postgres_poll_status;

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef enum cpkt_postgres_auth_data_kind {
  CPKT_POSTGRES_AUTH_DATA_PROMPT_OAUTH_DEVICE = 0,
  CPKT_POSTGRES_AUTH_DATA_OAUTH_BEARER_TOKEN = 1
} cpkt_postgres_auth_data_kind;

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef struct cpkt_postgres_async_socket {
  unsigned long high;
  unsigned long low;
} cpkt_postgres_async_socket;

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef struct cpkt_postgres_oauth_device_prompt {
  const char *verification_uri;
  const char *user_code;
  const char *verification_uri_complete;
  int expires_in;
} cpkt_postgres_oauth_device_prompt;

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef struct cpkt_postgres_oauth_bearer_request
    cpkt_postgres_oauth_bearer_request;
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef cpkt_postgres_poll_status (*cpkt_postgres_oauth_async)(
    cpkt_postgres_connection *connection,
    cpkt_postgres_oauth_bearer_request *request,
    cpkt_postgres_async_socket *alternate_socket);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef void (*cpkt_postgres_oauth_cleanup)(
    cpkt_postgres_connection *connection,
    cpkt_postgres_oauth_bearer_request *request);

struct cpkt_postgres_oauth_bearer_request {
  const char *openid_configuration;
  const char *scope;
  cpkt_postgres_oauth_async async;
  cpkt_postgres_oauth_cleanup cleanup;
  char *token;
  void *user;
};

/* The data argument is a device prompt or bearer request, by kind. */
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef int (*cpkt_postgres_auth_data_hook)(
    cpkt_postgres_auth_data_kind kind, cpkt_postgres_connection *connection,
    void *data, void *context);

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef enum cpkt_postgres_result_status {
  CPKT_POSTGRES_RESULT_EMPTY_QUERY = 0,
  CPKT_POSTGRES_RESULT_COMMAND_OK = 1,
  CPKT_POSTGRES_RESULT_TUPLES_OK = 2,
  CPKT_POSTGRES_RESULT_COPY_OUT = 3,
  CPKT_POSTGRES_RESULT_COPY_IN = 4,
  CPKT_POSTGRES_RESULT_BAD_RESPONSE = 5,
  CPKT_POSTGRES_RESULT_NONFATAL_ERROR = 6,
  CPKT_POSTGRES_RESULT_FATAL_ERROR = 7,
  CPKT_POSTGRES_RESULT_COPY_BOTH = 8,
  CPKT_POSTGRES_RESULT_SINGLE_TUPLE = 9,
  CPKT_POSTGRES_RESULT_PIPELINE_SYNC = 10,
  CPKT_POSTGRES_RESULT_PIPELINE_ABORTED = 11,
  CPKT_POSTGRES_RESULT_TUPLES_CHUNK = 12
} cpkt_postgres_result_status;

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef enum cpkt_postgres_transaction_status {
  CPKT_POSTGRES_TRANSACTION_IDLE = 0,
  CPKT_POSTGRES_TRANSACTION_ACTIVE = 1,
  CPKT_POSTGRES_TRANSACTION_IN_TRANSACTION = 2,
  CPKT_POSTGRES_TRANSACTION_IN_ERROR = 3,
  CPKT_POSTGRES_TRANSACTION_UNKNOWN = 4
} cpkt_postgres_transaction_status;

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef enum cpkt_postgres_verbosity {
  CPKT_POSTGRES_ERRORS_TERSE = 0,
  CPKT_POSTGRES_ERRORS_DEFAULT = 1,
  CPKT_POSTGRES_ERRORS_VERBOSE = 2,
  CPKT_POSTGRES_ERRORS_SQLSTATE = 3
} cpkt_postgres_verbosity;

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef enum cpkt_postgres_context_visibility {
  CPKT_POSTGRES_CONTEXT_NEVER = 0,
  CPKT_POSTGRES_CONTEXT_ERRORS = 1,
  CPKT_POSTGRES_CONTEXT_ALWAYS = 2
} cpkt_postgres_context_visibility;

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef enum cpkt_postgres_ping_status {
  CPKT_POSTGRES_PING_OK = 0,
  CPKT_POSTGRES_PING_REJECT = 1,
  CPKT_POSTGRES_PING_NO_RESPONSE = 2,
  CPKT_POSTGRES_PING_NO_ATTEMPT = 3
} cpkt_postgres_ping_status;

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef enum cpkt_postgres_pipeline_status {
  CPKT_POSTGRES_PIPELINE_OFF = 0,
  CPKT_POSTGRES_PIPELINE_ON = 1,
  CPKT_POSTGRES_PIPELINE_ABORTED = 2
} cpkt_postgres_pipeline_status;

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef struct cpkt_postgres_connection_option {
  char *keyword;
  char *environment_variable;
  char *compiled_default;
  char *value;
  char *label;
  char *display_character;
  int display_size;
} cpkt_postgres_connection_option;

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef struct cpkt_postgres_result_attribute {
  char *name;
  cpkt_postgres_oid table_oid;
  int column_id;
  int format;
  cpkt_postgres_oid type_oid;
  int type_size;
  int type_modifier;
} cpkt_postgres_result_attribute;

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef struct cpkt_postgres_fastpath_argument {
  int length;
  int is_integer;
  int *integer_pointer;
  int integer_value;
} cpkt_postgres_fastpath_argument;

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef struct cpkt_postgres_notification {
  char *channel;
  int backend_pid;
  char *payload;
} cpkt_postgres_notification;

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
typedef struct cpkt_postgres_print_options {
  char header;
  char align;
  char standard;
  char html;
  char expanded;
  char pager;
  char *field_separator;
  char *table_options;
  char *caption;
  char **field_names;
} cpkt_postgres_print_options;

/*
 * C89 receiver shell over a PostgreSQL connection.  Every method receives its
 * receiver explicitly, making ownership and connection affinity visible at
 * every call site: pg->tx(pg, "select 1"), pg->close(pg).
 */
struct cpkt_postgres {
  cpkt_postgres_result *(*tx)(cpkt_postgres *self, const char *query);
  cpkt_postgres_result *(*tx_params)(cpkt_postgres *self, const char *command,
                                     int parameter_count,
                                     const cpkt_postgres_oid *parameter_types,
                                     const char *const *parameter_values,
                                     const int *parameter_lengths,
                                     const int *parameter_formats,
                                     int result_format);
  int (*send)(cpkt_postgres *self, const char *query);
  int (*send_params)(cpkt_postgres *self, const char *command,
                     int parameter_count,
                     const cpkt_postgres_oid *parameter_types,
                     const char *const *parameter_values,
                     const int *parameter_lengths, const int *parameter_formats,
                     int result_format);
  cpkt_postgres_result *(*receive)(cpkt_postgres *self);
  int (*consume)(cpkt_postgres *self);
  int (*busy)(cpkt_postgres *self);
  int (*flush)(cpkt_postgres *self);
  int (*set_nonblocking)(cpkt_postgres *self, int enabled);
  int (*is_nonblocking)(const cpkt_postgres *self);
  int (*begin_pipeline)(cpkt_postgres *self);
  int (*end_pipeline)(cpkt_postgres *self);
  int (*pipeline_sync)(cpkt_postgres *self);
  int (*copy_write)(cpkt_postgres *self, const char *bytes, int byte_count);
  int (*copy_finish)(cpkt_postgres *self, const char *error_message);
  int (*copy_read)(cpkt_postgres *self, char **bytes_out, int asynchronous);
  int (*reset_start)(cpkt_postgres *self);
  cpkt_postgres_poll_status (*reset_poll)(cpkt_postgres *self);
  void (*reset)(cpkt_postgres *self);
  cpkt_postgres_connection_status (*status)(const cpkt_postgres *self);
  char *(*error)(const cpkt_postgres *self);
  void (*close)(cpkt_postgres *self);
  cpkt_postgres_connection *connection;
};

#define CPKT_POSTGRES_COPY_RESULT_ATTRIBUTES 1
#define CPKT_POSTGRES_COPY_RESULT_TUPLES 2
#define CPKT_POSTGRES_COPY_RESULT_EVENTS 4
#define CPKT_POSTGRES_COPY_RESULT_NOTICE_HOOKS 8
#define CPKT_POSTGRES_QUERY_PARAMETER_MAXIMUM 65535
#define CPKT_POSTGRES_TRACE_SUPPRESS_TIMESTAMPS 1
#define CPKT_POSTGRES_TRACE_REGRESSION_MODE 2

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres *cpkt_postgres_new(const char *connection_info);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres *cpkt_postgres_new_params(const char *const *keywords,
                                        const char *const *values,
                                        int expand_database_name);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
void cpkt_postgres_close(cpkt_postgres *self);

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_connection *
cpkt_postgres_connect_start(const char *connection_info);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_connection *
cpkt_postgres_connect_start_params(const char *const *keywords,
                                   const char *const *values,
                                   int expand_database_name);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_poll_status
cpkt_postgres_connect_poll(cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_connection *cpkt_postgres_connect(const char *connection_info);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_connection *
cpkt_postgres_connect_params(const char *const *keywords,
                             const char *const *values,
                             int expand_database_name);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_connection *cpkt_postgres_connect_login(
    const char *host, const char *port, const char *options, const char *tty,
    const char *database, const char *user, const char *password);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
void cpkt_postgres_connection_free(cpkt_postgres_connection *connection);

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_connection_option *cpkt_postgres_connection_defaults(void);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_connection_option *
cpkt_postgres_connection_parse(const char *connection_info,
                               char **error_message);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_connection_option *
cpkt_postgres_connection_options(cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
void cpkt_postgres_connection_options_free(
    cpkt_postgres_connection_option *options);

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_reset_start(cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_poll_status
cpkt_postgres_reset_poll(cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
void cpkt_postgres_reset(cpkt_postgres_connection *connection);

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_cancel_connection *
cpkt_postgres_cancel_connection_create(cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_cancel_start(
    cpkt_postgres_cancel_connection *cancel_connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_cancel_blocking(
    cpkt_postgres_cancel_connection *cancel_connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_poll_status
cpkt_postgres_cancel_poll(cpkt_postgres_cancel_connection *cancel_connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_connection_status cpkt_postgres_cancel_status(
    const cpkt_postgres_cancel_connection *cancel_connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_cancel_socket(
    const cpkt_postgres_cancel_connection *cancel_connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
char *cpkt_postgres_cancel_error_message(
    cpkt_postgres_cancel_connection *cancel_connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
void cpkt_postgres_cancel_reset(
    cpkt_postgres_cancel_connection *cancel_connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
void cpkt_postgres_cancel_connection_free(
    cpkt_postgres_cancel_connection *cancel_connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_cancel *
cpkt_postgres_cancel_create(cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
void cpkt_postgres_cancel_free(cpkt_postgres_cancel *cancel);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_cancel_request(cpkt_postgres_cancel *cancel,
                                 char *error_buffer, int error_buffer_size);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_request_cancel(cpkt_postgres_connection *connection);

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
char *cpkt_postgres_database(const cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
char *cpkt_postgres_user(const cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
char *cpkt_postgres_password(const cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
char *cpkt_postgres_host(const cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
char *cpkt_postgres_host_address(const cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
char *cpkt_postgres_port(const cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
char *cpkt_postgres_tty(const cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
char *cpkt_postgres_options(const cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_connection_status
cpkt_postgres_connection_status_get(const cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_transaction_status cpkt_postgres_transaction_status_get(
    const cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
const char *
cpkt_postgres_parameter_status(const cpkt_postgres_connection *connection,
                               const char *name);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_protocol_version(const cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_full_protocol_version(
    const cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_server_version(const cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
char *cpkt_postgres_error_message(const cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_socket(const cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_backend_pid(const cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_pipeline_status
cpkt_postgres_pipeline_status_get(const cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_connection_needs_password(
    const cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_connection_used_password(
    const cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_connection_used_gssapi(
    const cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_client_encoding(const cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_set_client_encoding(cpkt_postgres_connection *connection,
                                      const char *encoding);

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_tls_in_use(cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_tls_object *
cpkt_postgres_tls_object_get(cpkt_postgres_connection *connection,
                             const char *object_name);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
const char *cpkt_postgres_tls_attribute(cpkt_postgres_connection *connection,
                                        const char *attribute_name);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
const char *const *
cpkt_postgres_tls_attribute_names(cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_tls_object *
cpkt_postgres_openssl_get(cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
void cpkt_postgres_init_tls(int initialize);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
void cpkt_postgres_init_openssl(int initialize_tls, int initialize_crypto);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_gss_encryption_in_use(cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_gss_context *
cpkt_postgres_gss_context_get(cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_verbosity
cpkt_postgres_set_error_verbosity(cpkt_postgres_connection *connection,
                                  cpkt_postgres_verbosity verbosity);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_context_visibility cpkt_postgres_set_error_context_visibility(
    cpkt_postgres_connection *connection,
    cpkt_postgres_context_visibility visibility);

/*
 * Callback registration is connection-affine.  The old callback and context
 * are returned when their output pointers are non-NULL.  Passing a NULL
 * callback selects the native client's no-op callback behavior. A result
 * retains the callback and context present when the client creates it. Keep
 * that context valid until the result is freed. If a result outlives its
 * connection, the callback receives a NULL connection pointer.
 */
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
void cpkt_postgres_set_notice_receiver(
    cpkt_postgres_connection *connection,
    cpkt_postgres_notice_receiver callback, void *context,
    cpkt_postgres_notice_receiver *old_callback_out, void **old_context_out);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
void cpkt_postgres_set_notice_processor(
    cpkt_postgres_connection *connection,
    cpkt_postgres_notice_processor callback, void *context,
    cpkt_postgres_notice_processor *old_callback_out, void **old_context_out);

/* Process-global client hooks.  Register them before starting worker threads.
 */
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_thread_lock
cpkt_postgres_register_thread_lock(cpkt_postgres_thread_lock callback);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_ssl_key_password_hook cpkt_postgres_set_ssl_key_password_hook(
    cpkt_postgres_ssl_key_password_hook callback);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_ssl_key_password_hook
cpkt_postgres_get_ssl_key_password_hook(void);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_default_ssl_key_password_hook(
    char *buffer, int buffer_size, cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
void cpkt_postgres_set_auth_data_hook(cpkt_postgres_auth_data_hook callback,
                                      void *context);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_auth_data_hook
cpkt_postgres_get_auth_data_hook(void **context_out);

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
void cpkt_postgres_trace(cpkt_postgres_connection *connection, FILE *stream);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
void cpkt_postgres_untrace(cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
void cpkt_postgres_set_trace_flags(cpkt_postgres_connection *connection,
                                   int flags);

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_result *
cpkt_postgres_execute(cpkt_postgres_connection *connection, const char *query);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_result *cpkt_postgres_execute_params(
    cpkt_postgres_connection *connection, const char *command,
    int parameter_count, const cpkt_postgres_oid *parameter_types,
    const char *const *parameter_values, const int *parameter_lengths,
    const int *parameter_formats, int result_format);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_result *
cpkt_postgres_prepare(cpkt_postgres_connection *connection,
                      const char *statement_name, const char *query,
                      int parameter_count,
                      const cpkt_postgres_oid *parameter_types);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_result *
cpkt_postgres_execute_prepared(cpkt_postgres_connection *connection,
                               const char *statement_name, int parameter_count,
                               const char *const *parameter_values,
                               const int *parameter_lengths,
                               const int *parameter_formats, int result_format);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_send_query(cpkt_postgres_connection *connection,
                             const char *query);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_send_query_params(cpkt_postgres_connection *connection,
                                    const char *command, int parameter_count,
                                    const cpkt_postgres_oid *parameter_types,
                                    const char *const *parameter_values,
                                    const int *parameter_lengths,
                                    const int *parameter_formats,
                                    int result_format);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_send_prepare(cpkt_postgres_connection *connection,
                               const char *statement_name, const char *query,
                               int parameter_count,
                               const cpkt_postgres_oid *parameter_types);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_send_query_prepared(cpkt_postgres_connection *connection,
                                      const char *statement_name,
                                      int parameter_count,
                                      const char *const *parameter_values,
                                      const int *parameter_lengths,
                                      const int *parameter_formats,
                                      int result_format);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_set_single_row_mode(cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_set_chunked_rows_mode(cpkt_postgres_connection *connection,
                                        int chunk_size);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_result *
cpkt_postgres_get_result(cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_is_busy(cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_consume_input(cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_enter_pipeline_mode(cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_exit_pipeline_mode(cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_pipeline_sync(cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_send_flush_request(cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_send_pipeline_sync(cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_notification *
cpkt_postgres_notification_next(cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
void cpkt_postgres_notification_free(cpkt_postgres_notification *notification);

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_put_copy_data(cpkt_postgres_connection *connection,
                                const char *buffer, int byte_count);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_put_copy_end(cpkt_postgres_connection *connection,
                               const char *error_message);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_get_copy_data(cpkt_postgres_connection *connection,
                                char **buffer_out, int asynchronous);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_getline(cpkt_postgres_connection *connection, char *buffer,
                          int length);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_putline(cpkt_postgres_connection *connection,
                          const char *line);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_getline_async(cpkt_postgres_connection *connection,
                                char *buffer, int buffer_size);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_put_bytes(cpkt_postgres_connection *connection,
                            const char *buffer, int byte_count);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_end_copy(cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_set_nonblocking(cpkt_postgres_connection *connection,
                                  int enabled);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_is_nonblocking(const cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_is_threadsafe(void);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_ping_status cpkt_postgres_ping(const char *connection_info);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_ping_status cpkt_postgres_ping_params(const char *const *keywords,
                                                    const char *const *values,
                                                    int expand_database_name);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_flush(cpkt_postgres_connection *connection);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_result *cpkt_postgres_fastpath(
    cpkt_postgres_connection *connection, int function_id, int *result_buffer,
    int *result_length_out, int result_is_integer,
    const cpkt_postgres_fastpath_argument *arguments, int argument_count);

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_result_status
cpkt_postgres_result_status_get(const cpkt_postgres_result *result);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
char *cpkt_postgres_result_status_text(cpkt_postgres_result_status status);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
char *cpkt_postgres_result_error_message(const cpkt_postgres_result *result);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
char *cpkt_postgres_result_verbose_error_message(
    const cpkt_postgres_result *result, cpkt_postgres_verbosity verbosity,
    cpkt_postgres_context_visibility visibility);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
char *cpkt_postgres_result_error_field(const cpkt_postgres_result *result,
                                       int field_code);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_result_row_count(const cpkt_postgres_result *result);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_result_field_count(const cpkt_postgres_result *result);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_result_is_binary(const cpkt_postgres_result *result);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
char *cpkt_postgres_result_field_name(const cpkt_postgres_result *result,
                                      int field_index);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_result_field_number(const cpkt_postgres_result *result,
                                      const char *field_name);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_oid
cpkt_postgres_result_field_table_oid(const cpkt_postgres_result *result,
                                     int field_index);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_result_field_table_column(const cpkt_postgres_result *result,
                                            int field_index);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_result_field_format(const cpkt_postgres_result *result,
                                      int field_index);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_oid
cpkt_postgres_result_field_type(const cpkt_postgres_result *result,
                                int field_index);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_result_field_size(const cpkt_postgres_result *result,
                                    int field_index);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_result_field_modifier(const cpkt_postgres_result *result,
                                        int field_index);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
char *cpkt_postgres_result_command_status(cpkt_postgres_result *result);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
char *cpkt_postgres_result_oid_status(const cpkt_postgres_result *result);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_oid
cpkt_postgres_result_oid_value(const cpkt_postgres_result *result);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
char *cpkt_postgres_result_command_tuples(cpkt_postgres_result *result);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
char *cpkt_postgres_result_value(const cpkt_postgres_result *result,
                                 int row_index, int field_index);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_result_value_length(const cpkt_postgres_result *result,
                                      int row_index, int field_index);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_result_value_is_null(const cpkt_postgres_result *result,
                                       int row_index, int field_index);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_result_parameter_count(const cpkt_postgres_result *result);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_oid
cpkt_postgres_result_parameter_type(const cpkt_postgres_result *result,
                                    int parameter_index);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_result *
cpkt_postgres_describe_prepared(cpkt_postgres_connection *connection,
                                const char *statement_name);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_result *
cpkt_postgres_describe_portal(cpkt_postgres_connection *connection,
                              const char *portal_name);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_send_describe_prepared(cpkt_postgres_connection *connection,
                                         const char *statement_name);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_send_describe_portal(cpkt_postgres_connection *connection,
                                       const char *portal_name);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_result *
cpkt_postgres_close_prepared(cpkt_postgres_connection *connection,
                             const char *statement_name);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_result *
cpkt_postgres_close_portal(cpkt_postgres_connection *connection,
                           const char *portal_name);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_send_close_prepared(cpkt_postgres_connection *connection,
                                      const char *statement_name);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_send_close_portal(cpkt_postgres_connection *connection,
                                    const char *portal_name);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
void cpkt_postgres_result_free(cpkt_postgres_result *result);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
void cpkt_postgres_text_free(char *memory);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
void cpkt_postgres_bytea_free(unsigned char *memory);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_result *
cpkt_postgres_result_new_empty(cpkt_postgres_connection *connection,
                               cpkt_postgres_result_status status);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_result *
cpkt_postgres_result_copy(const cpkt_postgres_result *source, int flags);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_result_set_attributes(
    cpkt_postgres_result *result, int attribute_count,
    cpkt_postgres_result_attribute *attributes);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
unsigned char *cpkt_postgres_result_allocate(cpkt_postgres_result *result,
                                             size_t byte_count);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
size_t cpkt_postgres_result_memory_size(const cpkt_postgres_result *result);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_result_set_value(cpkt_postgres_result *result, int row_index,
                                   int field_index, char *value,
                                   int value_length);

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
size_t
cpkt_postgres_escape_string_connection(cpkt_postgres_connection *connection,
                                       char *destination, const char *source,
                                       size_t source_length, int *error_out);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
char *cpkt_postgres_escape_literal(cpkt_postgres_connection *connection,
                                   const char *source, size_t source_length);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
char *cpkt_postgres_escape_identifier(cpkt_postgres_connection *connection,
                                      const char *source, size_t source_length);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
unsigned char *cpkt_postgres_escape_bytea_connection(
    cpkt_postgres_connection *connection, const unsigned char *source,
    size_t source_length, size_t *destination_length_out);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
unsigned char *cpkt_postgres_unescape_bytea(const unsigned char *source,
                                            size_t *destination_length_out);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
size_t cpkt_postgres_escape_string(char *destination, const char *source,
                                   size_t source_length);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
unsigned char *cpkt_postgres_escape_bytea(const unsigned char *source,
                                          size_t source_length,
                                          size_t *destination_length_out);

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
void cpkt_postgres_print(FILE *stream, const cpkt_postgres_result *result,
                         const cpkt_postgres_print_options *options);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
void cpkt_postgres_display_tuples(const cpkt_postgres_result *result,
                                  FILE *stream, int fill_align,
                                  const char *field_separator, int print_header,
                                  int quiet);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
void cpkt_postgres_print_tuples(const cpkt_postgres_result *result,
                                FILE *stream, int print_attribute_names,
                                int terse_output, int column_width);

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_large_object_open(cpkt_postgres_connection *connection,
                                    cpkt_postgres_oid object_oid, int mode);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_large_object_close(cpkt_postgres_connection *connection,
                                     int descriptor);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_large_object_read(cpkt_postgres_connection *connection,
                                    int descriptor, char *buffer,
                                    size_t byte_count);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_large_object_write(cpkt_postgres_connection *connection,
                                     int descriptor, const char *buffer,
                                     size_t byte_count);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_large_object_seek(cpkt_postgres_connection *connection,
                                    int descriptor, int offset, int whence);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_i64
cpkt_postgres_large_object_seek64(cpkt_postgres_connection *connection,
                                  int descriptor, cpkt_postgres_i64 offset,
                                  int whence);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_oid
cpkt_postgres_large_object_create_legacy(cpkt_postgres_connection *connection,
                                         int mode);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_oid
cpkt_postgres_large_object_create(cpkt_postgres_connection *connection,
                                  cpkt_postgres_oid object_oid);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_large_object_tell(cpkt_postgres_connection *connection,
                                    int descriptor);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_i64
cpkt_postgres_large_object_tell64(cpkt_postgres_connection *connection,
                                  int descriptor);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_large_object_truncate(cpkt_postgres_connection *connection,
                                        int descriptor, size_t byte_count);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_large_object_truncate64(cpkt_postgres_connection *connection,
                                          int descriptor,
                                          cpkt_postgres_i64 byte_count);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_large_object_unlink(cpkt_postgres_connection *connection,
                                      cpkt_postgres_oid object_oid);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_oid
cpkt_postgres_large_object_import(cpkt_postgres_connection *connection,
                                  const char *file_name);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_oid
cpkt_postgres_large_object_import_with_oid(cpkt_postgres_connection *connection,
                                           const char *file_name,
                                           cpkt_postgres_oid object_oid);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_large_object_export(cpkt_postgres_connection *connection,
                                      cpkt_postgres_oid object_oid,
                                      const char *file_name);

/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_library_version(void);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_socket_poll(int socket_descriptor, int wait_for_read,
                              int wait_for_write, cpkt_postgres_i64 end_time);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_i64 cpkt_postgres_current_time_microseconds(void);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_multibyte_length(const char *text, int encoding);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_multibyte_length_bounded(const char *text, int encoding);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_multibyte_display_length(const char *text, int encoding);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_environment_encoding(void);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
char *cpkt_postgres_encrypt_password(const char *password, const char *user);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
char *cpkt_postgres_encrypt_password_connection(
    cpkt_postgres_connection *connection, const char *password,
    const char *user, const char *algorithm);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
cpkt_postgres_result *
cpkt_postgres_change_password(cpkt_postgres_connection *connection,
                              const char *user, const char *password);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_encoding_from_name(const char *name);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
const char *cpkt_postgres_encoding_name(int encoding);
/** C89 PostgreSQL facade declaration. See docs/postgres-c89-facade-spec.md. */
int cpkt_postgres_server_encoding_is_valid(int encoding);

#endif
