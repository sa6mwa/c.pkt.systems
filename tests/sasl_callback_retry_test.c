#include <cpkt/sasl.h>

#include <sasl/sasl.h>

#include <string.h>

static int mock_connection;
static const sasl_callback_t *mock_callbacks;
static int mock_start_calls;
static int mock_step_calls;
static sasl_interact_t mock_start_interactions[2];
static sasl_interact_t mock_step_interactions[2];

typedef union mock_callback_bridge {
  int (*generic)(void);
  sasl_getsimple_t *simple;
} mock_callback_bridge;

static const sasl_callback_t *mock_callback(unsigned long identifier) {
  const sasl_callback_t *callback;
  callback = mock_callbacks;
  while (callback != 0 && callback->id != SASL_CB_LIST_END) {
    if (callback->id == identifier)
      return callback;
    ++callback;
  }
  return 0;
}

static int mock_simple(void *context, int identifier, const char **result,
                       unsigned long *result_byte_count) {
  int *call_count;
  call_count = (int *)context;
  if (identifier != SASL_CB_AUTHNAME || result == 0)
    return SASL_BADPARAM;
  *call_count += 1;
  *result = "facade-authentication-user";
  if (result_byte_count != 0)
    *result_byte_count = (unsigned long)strlen(*result);
  return SASL_OK;
}

int sasl_client_init(const sasl_callback_t *callbacks) {
  (void)callbacks;
  return SASL_OK;
}

int sasl_server_init(const sasl_callback_t *callbacks,
                     const char *application_name) {
  (void)callbacks;
  (void)application_name;
  return SASL_OK;
}

int sasl_client_done(void) { return SASL_OK; }
int sasl_server_done(void) { return SASL_OK; }

int sasl_client_new(const char *service, const char *server_name,
                    const char *local_endpoint, const char *remote_endpoint,
                    const sasl_callback_t *callbacks, unsigned flags,
                    sasl_conn_t **connection) {
  (void)service;
  (void)server_name;
  (void)local_endpoint;
  (void)remote_endpoint;
  (void)flags;
  mock_callbacks = callbacks;
  if (connection != 0)
    *connection = (sasl_conn_t *)&mock_connection;
  return SASL_OK;
}

int sasl_server_new(const char *service, const char *server_name,
                    const char *user_realm, const char *local_endpoint,
                    const char *remote_endpoint,
                    const sasl_callback_t *callbacks, unsigned flags,
                    sasl_conn_t **connection) {
  (void)service;
  (void)server_name;
  (void)user_realm;
  (void)local_endpoint;
  (void)remote_endpoint;
  (void)callbacks;
  (void)flags;
  if (connection != 0)
    *connection = (sasl_conn_t *)&mock_connection;
  return SASL_OK;
}

int sasl_client_start(sasl_conn_t *connection, const char *mechanisms,
                      sasl_interact_t **interactions, const char **output,
                      unsigned *output_length, const char **mechanism) {
  (void)connection;
  (void)mechanisms;
  if (output != 0)
    *output = 0;
  if (output_length != 0)
    *output_length = 0;
  if (mechanism != 0)
    *mechanism = 0;
  if (mock_start_calls == 0) {
    if (interactions == 0 || *interactions != 0)
      return SASL_BADPARAM;
    mock_start_interactions[0].id = SASL_CB_AUTHNAME;
    mock_start_interactions[0].result = "initial";
    mock_start_interactions[0].len = 7;
    mock_start_interactions[1].id = SASL_CB_LIST_END;
    *interactions = mock_start_interactions;
    mock_start_calls += 1;
    return SASL_INTERACT;
  }
  if (interactions == 0 || *interactions != mock_start_interactions ||
      mock_start_interactions[0].result == 0 ||
      mock_start_interactions[0].len != 6 ||
      memcmp(mock_start_interactions[0].result, "answer", 6) != 0)
    return SASL_BADPARAM;
  *interactions = 0;
  if (output != 0)
    *output = "started";
  if (output_length != 0)
    *output_length = 7;
  mock_start_calls += 1;
  return SASL_OK;
}

int sasl_client_step(sasl_conn_t *connection, const char *input,
                     unsigned input_length, sasl_interact_t **interactions,
                     const char **output, unsigned *output_length) {
  (void)connection;
  (void)input;
  (void)input_length;
  if (output != 0)
    *output = 0;
  if (output_length != 0)
    *output_length = 0;
  if (mock_step_calls == 0) {
    if (interactions == 0 || *interactions != 0)
      return SASL_BADPARAM;
    mock_step_interactions[0].id = SASL_CB_PASS;
    mock_step_interactions[0].result = "initial";
    mock_step_interactions[0].len = 7;
    mock_step_interactions[1].id = SASL_CB_LIST_END;
    *interactions = mock_step_interactions;
    mock_step_calls += 1;
    return SASL_INTERACT;
  }
  if (interactions == 0 || *interactions != mock_step_interactions ||
      mock_step_interactions[0].result == 0 ||
      mock_step_interactions[0].len != 6 ||
      memcmp(mock_step_interactions[0].result, "answer", 6) != 0)
    return SASL_BADPARAM;
  *interactions = 0;
  if (output != 0)
    *output = "stepped";
  if (output_length != 0)
    *output_length = 7;
  mock_step_calls += 1;
  return SASL_OK;
}

int sasl_server_start(sasl_conn_t *connection, const char *mechanism,
                      const char *input, unsigned input_length,
                      const char **output, unsigned *output_length) {
  (void)connection;
  (void)mechanism;
  (void)input;
  (void)input_length;
  if (output != 0)
    *output = 0;
  if (output_length != 0)
    *output_length = 0;
  return SASL_OK;
}

int sasl_server_step(sasl_conn_t *connection, const char *input,
                     unsigned input_length, const char **output,
                     unsigned *output_length) {
  return sasl_server_start(connection, 0, input, input_length, output,
                           output_length);
}

int sasl_listmech(sasl_conn_t *connection, const char *user, const char *prefix,
                  const char *separator, const char *suffix,
                  const char **result, unsigned *result_length, int *count) {
  (void)connection;
  (void)user;
  (void)prefix;
  (void)separator;
  (void)suffix;
  if (result != 0)
    *result = 0;
  if (result_length != 0)
    *result_length = 0;
  if (count != 0)
    *count = 0;
  return SASL_OK;
}

int sasl_encode(sasl_conn_t *connection, const char *input,
                unsigned input_length, const char **output,
                unsigned *output_length) {
  return sasl_server_start(connection, 0, input, input_length, output,
                           output_length);
}

int sasl_decode(sasl_conn_t *connection, const char *input,
                unsigned input_length, const char **output,
                unsigned *output_length) {
  return sasl_server_start(connection, 0, input, input_length, output,
                           output_length);
}

int sasl_setprop(sasl_conn_t *connection, int property, const void *value) {
  (void)connection;
  (void)property;
  (void)value;
  return SASL_OK;
}

int sasl_set_path(int type, char *path) {
  (void)type;
  (void)path;
  return SASL_OK;
}

const char *sasl_errdetail(sasl_conn_t *connection) {
  (void)connection;
  return "mock SASL detail";
}

const char *sasl_errstring(int status, const char *languages,
                           const char **language) {
  (void)status;
  (void)languages;
  if (language != 0)
    *language = 0;
  return "mock SASL error";
}

void sasl_version_info(const char **implementation, const char **version,
                       int *major, int *minor, int *step, int *patch) {
  if (implementation != 0)
    *implementation = "mock";
  if (version != 0)
    *version = "0";
  if (major != 0)
    *major = 0;
  if (minor != 0)
    *minor = 0;
  if (step != 0)
    *step = 0;
  if (patch != 0)
    *patch = 0;
}

void sasl_dispose(sasl_conn_t **connection) {
  if (connection != 0)
    *connection = 0;
}

int main(void) {
  const sasl_callback_t *callback;
  mock_callback_bridge callback_bridge;
  cpkt_sasl_callbacks callbacks;
  cpkt_sasl *client;
  cpkt_sasl_interaction *interactions;
  const char *result;
  const char *output;
  const char *mechanism;
  unsigned length;
  unsigned long output_length;
  int callback_calls;
  int status;

  memset(&callbacks, 0, sizeof(callbacks));
  callback_calls = 0;
  callbacks.context = &callback_calls;
  callbacks.simple = mock_simple;
  status = CPKT_SASL_FAIL;
  client = cpkt_sasl_client_new("imap", "mail.example.test", 0, 0, &callbacks,
                                0, &status);
  if (client == 0 || status != CPKT_SASL_OK)
    return 1;
  callback = mock_callback(SASL_CB_AUTHNAME);
  if (callback == 0 || callback->proc == 0)
    return 2;
  callback_bridge.generic = callback->proc;
  result = 0;
  length = 0;
  if (callback_bridge.simple(callback->context, SASL_CB_AUTHNAME, &result,
                             &length) != SASL_OK ||
      result == 0 || strcmp(result, "facade-authentication-user") != 0 ||
      length != strlen(result) || callback_calls != 1)
    return 3;
  interactions = 0;
  output = 0;
  output_length = 0;
  mechanism = 0;
  if (client->start(client, "MOCK", &interactions, &output, &output_length,
                    &mechanism) != CPKT_SASL_INTERACT ||
      interactions == 0 || interactions[0].id != SASL_CB_AUTHNAME ||
      interactions[0].result == 0 ||
      strcmp((const char *)interactions[0].result, "initial") != 0 ||
      interactions[0].result_byte_count != 7)
    return 4;
  interactions[0].result = "answer";
  interactions[0].result_byte_count = 6;
  if (client->start(client, "MOCK", &interactions, &output, &output_length,
                    &mechanism) != CPKT_SASL_OK ||
      interactions != 0 || output == 0 || output_length != 7 ||
      mock_start_calls != 2)
    return 5;
  interactions = 0;
  if (client->step(client, "input", 5, &interactions, &output,
                   &output_length) != CPKT_SASL_INTERACT ||
      interactions == 0 || interactions[0].id != SASL_CB_PASS ||
      interactions[0].result == 0 ||
      strcmp((const char *)interactions[0].result, "initial") != 0 ||
      interactions[0].result_byte_count != 7)
    return 6;
  interactions[0].result = "answer";
  interactions[0].result_byte_count = 6;
  if (client->step(client, "input", 5, &interactions, &output,
                   &output_length) != CPKT_SASL_OK ||
      interactions != 0 || output == 0 || output_length != 7 ||
      mock_step_calls != 2)
    return 7;
  client->close(client);
  return 0;
}
