#include "opcua_logging_peer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "logging check failed at %d: %s\n", __LINE__, #expr);    \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

struct callback_state {
  struct cpkt_log_sink sink;
  const char *expected;
  size_t expected_length;
  int check_message;
  int expect_alloc_failure;
};

/* This callback and all public facade calls compile as strict C89. */
static void receive(const cpkt_opcua_log_record *record, void *user) {
  struct callback_state *state;
  state = (struct callback_state *)user;
  CHECK(record != NULL && record->message != NULL);
  if (state->expect_alloc_failure) {
    CHECK(record->format_status != 0);
    CHECK(strcmp(cpkt_opcua_status_name(record->format_status),
                 "BadOutOfMemory") == 0);
  } else {
    CHECK(record->format_status == 0);
  }
  CHECK(record->message[record->message_length] == '\0');
  if (state->check_message) {
    CHECK(record->message_length == state->expected_length);
    CHECK(memcmp(record->message, state->expected, state->expected_length) ==
          0);
  }
  cpkt_log_sink_emit(&state->sink, record);
}

static void exercise(cpkt_opcua_server *server, cpkt_opcua_client *client,
                     struct callback_state *state) {
  struct cpkt_log_request request;
  cpkt_opcua_log_config logger;
  cpkt_opcua_status status;
  size_t before;
  int level;
  int category;
  int plugin;
  char *large;
  static const char embedded[] = {'a', '\0', 'b'};
  static const char expanded[] =
      "value=hello node=ns=1;i=42 name=2:name number=17";

  memset(&request, 0, sizeof(request));
  memset(&logger, 0, sizeof(logger));
  logger.fn = receive;
  logger.user = state;
  state->check_message = 1;
  state->expected = "record";
  state->expected_length = 6;
  request.message = "record";
  request.length = 6;
  for (level = 100; level <= 600; level += 100) {
    for (category = 0; category < 10; ++category) {
      for (plugin = 0; plugin < 3; ++plugin) {
        request.level = (cpkt_opcua_log_level)level;
        request.category = (cpkt_opcua_log_category)category;
        request.plugin = plugin;
        before = state->sink.records;
        if (server != NULL) {
          CHECK(cpkt_opcua_server_native_config(server, cpkt_log_server_emit,
                                                &request,
                                                &status) == CPKT_OPCUA_OK);
        } else {
          CHECK(cpkt_opcua_client_native_config(client, cpkt_log_client_emit,
                                                &request,
                                                &status) == CPKT_OPCUA_OK);
        }
        CHECK(status == 0 && state->sink.records == before + 1);
        CHECK(state->sink.last_level == request.level);
        CHECK(state->sink.last_category == request.category);
        CHECK(strstr(state->sink.prefix, "\"format_status\":0") != NULL);
      }
    }
  }
  request.level = CPKT_OPCUA_LOG_INFO;
  request.category = CPKT_OPCUA_LOG_APPLICATION;
  request.plugin = 0;
  request.kind = 1;
  request.message = "hello";
  request.length = 5;
  state->expected = expanded;
  state->expected_length = sizeof(expanded) - 1;
  if (server != NULL) {
    CHECK(cpkt_opcua_server_native_config(server, cpkt_log_server_emit,
                                          &request, &status) == CPKT_OPCUA_OK);
  } else {
    CHECK(cpkt_opcua_client_native_config(client, cpkt_log_client_emit,
                                          &request, &status) == CPKT_OPCUA_OK);
  }
  request.kind = 0;
  request.message = embedded;
  request.length = sizeof(embedded);
  state->expected = embedded;
  state->expected_length = sizeof(embedded);
  if (server != NULL) {
    CHECK(cpkt_opcua_server_native_config(server, cpkt_log_server_emit,
                                          &request, &status) == CPKT_OPCUA_OK);
  } else {
    CHECK(cpkt_opcua_client_native_config(client, cpkt_log_client_emit,
                                          &request, &status) == CPKT_OPCUA_OK);
  }
  CHECK(strstr(state->sink.prefix, "a\\u0000b") != NULL);
  large = (char *)malloc(75001);
  CHECK(large != NULL);
  memset(large, 'z', 75000);
  large[75000] = '\0';
  request.message = large;
  request.length = 75000;
  state->expected = large;
  state->expected_length = 75000;
  if (server != NULL) {
    CHECK(cpkt_opcua_server_native_config(server, cpkt_log_server_emit,
                                          &request, &status) == CPKT_OPCUA_OK);
  } else {
    CHECK(cpkt_opcua_client_native_config(client, cpkt_log_client_emit,
                                          &request, &status) == CPKT_OPCUA_OK);
  }
  CHECK(state->sink.z_count == 75000 && state->sink.chunks > 1);
#ifdef CPKT_LOGGING_ALLOC_FAILURE_TEST
  state->expected = "OPC UA log message formatting failed";
  state->expected_length = strlen(state->expected);
  state->expect_alloc_failure = 1;
  before = state->sink.records;
  cpkt_log_fail_allocation(75001);
  if (server != NULL) {
    CHECK(cpkt_opcua_server_native_config(server, cpkt_log_server_emit,
                                          &request, &status) == CPKT_OPCUA_OK);
  } else {
    CHECK(cpkt_opcua_client_native_config(client, cpkt_log_client_emit,
                                          &request, &status) == CPKT_OPCUA_OK);
  }
  CHECK(cpkt_log_allocation_failed() && state->sink.records == before + 1);
  state->expect_alloc_failure = 0;
#endif
  free(large);
  state->expected = "";
  state->expected_length = 0;
  request.message = "";
  request.length = 0;
  if (server != NULL) {
    CHECK(cpkt_opcua_server_native_config(server, cpkt_log_server_emit,
                                          &request, &status) == CPKT_OPCUA_OK);
  } else {
    CHECK(cpkt_opcua_client_native_config(client, cpkt_log_client_emit,
                                          &request, &status) == CPKT_OPCUA_OK);
  }
  logger.min_level = CPKT_OPCUA_LOG_WARNING;
  if (server != NULL)
    CHECK(cpkt_opcua_server_set_logger(server, &logger) == CPKT_OPCUA_OK);
  else
    CHECK(cpkt_opcua_client_set_logger(client, &logger) == CPKT_OPCUA_OK);
  for (level = 100; level <= 600; level += 100) {
    request.level = (cpkt_opcua_log_level)level;
    request.plugin = 1;
    before = state->sink.records;
    if (server != NULL) {
      CHECK(cpkt_opcua_server_native_config(server, cpkt_log_server_emit,
                                            &request,
                                            &status) == CPKT_OPCUA_OK);
    } else {
      CHECK(cpkt_opcua_client_native_config(client, cpkt_log_client_emit,
                                            &request,
                                            &status) == CPKT_OPCUA_OK);
    }
    CHECK(state->sink.records == before + (level >= 400 ? 1 : 0));
  }
  logger.fn = NULL;
  if (server != NULL)
    CHECK(cpkt_opcua_server_set_logger(server, &logger) == CPKT_OPCUA_OK);
  else
    CHECK(cpkt_opcua_client_set_logger(client, &logger) == CPKT_OPCUA_OK);
  before = state->sink.records;
  if (server != NULL) {
    CHECK(cpkt_opcua_server_native_config(server, cpkt_log_server_emit,
                                          &request, &status) == CPKT_OPCUA_OK);
  } else {
    CHECK(cpkt_opcua_client_native_config(client, cpkt_log_client_emit,
                                          &request, &status) == CPKT_OPCUA_OK);
  }
  CHECK(state->sink.records == before);
  logger.fn = receive;
  logger.min_level = (cpkt_opcua_log_level)0;
  if (server != NULL)
    CHECK(cpkt_opcua_server_set_logger(server, &logger) == CPKT_OPCUA_OK);
  else
    CHECK(cpkt_opcua_client_set_logger(client, &logger) == CPKT_OPCUA_OK);
  state->check_message = 0;
}

int main(int argc, char **argv) {
  cpkt_opcua_server *server;
  cpkt_opcua_client *client;
  cpkt_opcua_log_config logger;
  cpkt_opcua_status status;
  struct callback_state first;
  struct callback_state second;
  struct cpkt_log_request request;
  size_t before;
  size_t second_before;
  FILE *file;
  const char *path;
  int legacy;
  int owned_clears;
  unsigned char *certificate;
  unsigned char *key;
  size_t certificate_length;
  size_t key_length;
  static const unsigned char json[] =
      "{ applicationDescription: { applicationUri: 'urn:cpkt:logging' } }";
  static const unsigned char invalid[] = "{ tcpEnabled: 17 }";
  static const unsigned char bad_key[] = "invalid key";

  CHECK(argc == 2 || argc == 3);
  legacy = argc == 3;
  owned_clears = 0;
  path = argv[1];
  memset(&first, 0, sizeof(first));
  memset(&second, 0, sizeof(second));
  cpkt_log_sink_init(&first.sink);
  cpkt_log_sink_init(&second.sink);
  memset(&logger, 0, sizeof(logger));
  logger.fn = receive;
  logger.user = &first;
  server = NULL;
  client = NULL;
  CHECK(cpkt_opcua_server_new_with_logger(NULL, 0, &logger) ==
        CPKT_OPCUA_ERR_ARG);
  CHECK(cpkt_opcua_client_new_with_logger(NULL, &logger) == CPKT_OPCUA_ERR_ARG);
  logger.min_level = (cpkt_opcua_log_level)301;
  CHECK(cpkt_opcua_server_new_with_logger(&server, 0, &logger) ==
        CPKT_OPCUA_ERR_ARG);
  CHECK(server == NULL);
  CHECK(cpkt_opcua_client_new_with_logger(&client, &logger) ==
        CPKT_OPCUA_ERR_ARG);
  CHECK(client == NULL);
  logger.min_level = (cpkt_opcua_log_level)0;
  if (legacy) {
    CHECK(cpkt_opcua_server_new(&server, 0) == CPKT_OPCUA_OK);
    CHECK(cpkt_opcua_server_native_config(server, cpkt_log_server_owned_logger,
                                          &owned_clears,
                                          &status) == CPKT_OPCUA_OK);
    CHECK(cpkt_opcua_server_set_logger(server, &logger) == CPKT_OPCUA_OK);
  } else {
    CHECK(cpkt_opcua_server_new_with_logger(&server, 0, &logger) ==
          CPKT_OPCUA_OK);
    CHECK(first.sink.records > 0);
  }
  CHECK(cpkt_opcua_server_set_logger(NULL, &logger) == CPKT_OPCUA_ERR_ARG);
  CHECK(cpkt_opcua_server_set_logger(server, NULL) == CPKT_OPCUA_ERR_ARG);
  /* Distinct destinations must coexist on live handles without a global hook.
   */
  logger.user = &second;
  CHECK(cpkt_opcua_client_new_with_logger(&client, &logger) == CPKT_OPCUA_OK);
  memset(&request, 0, sizeof(request));
  request.level = CPKT_OPCUA_LOG_INFO;
  request.category = CPKT_OPCUA_LOG_CLIENT;
  request.message = "isolated";
  request.length = 8;
  before = first.sink.records;
  second_before = second.sink.records;
  CHECK(cpkt_opcua_client_native_config(client, cpkt_log_client_emit, &request,
                                        &status) == CPKT_OPCUA_OK);
  CHECK(first.sink.records == before &&
        second.sink.records == second_before + 1);
  cpkt_opcua_client_free(client);
  client = NULL;
  second_before = second.sink.records;
  logger.fn = NULL;
  exercise(server, NULL, &first);
  CHECK(second.sink.records == second_before);
  logger.fn = receive;
  before = first.sink.records;
  CHECK(cpkt_opcua_server_startup(server, &status) == CPKT_OPCUA_OK);
  CHECK(first.sink.records > before);
  /* Changing user data directs event-loop and shutdown logs to the new sink. */
  logger.user = &second;
  CHECK(cpkt_opcua_server_set_logger(server, &logger) == CPKT_OPCUA_OK);
  before = first.sink.records;
  second_before = second.sink.records;
  cpkt_opcua_server_free(server);
  CHECK(owned_clears == (legacy ? 1 : 0));
  CHECK(first.sink.records == before && second.sink.records > second_before);
  server = NULL;
  logger.user = &first;
  if (legacy) {
    CHECK(cpkt_opcua_client_new(&client) == CPKT_OPCUA_OK);
    CHECK(cpkt_opcua_client_native_config(client, cpkt_log_client_owned_logger,
                                          &owned_clears,
                                          &status) == CPKT_OPCUA_OK);
    CHECK(cpkt_opcua_client_set_logger(client, &logger) == CPKT_OPCUA_OK);
  } else {
    CHECK(cpkt_opcua_client_new_with_logger(&client, &logger) == CPKT_OPCUA_OK);
  }
  CHECK(cpkt_opcua_client_set_logger(NULL, &logger) == CPKT_OPCUA_ERR_ARG);
  CHECK(cpkt_opcua_client_set_logger(client, NULL) == CPKT_OPCUA_ERR_ARG);
  exercise(NULL, client, &first);
#ifdef CPKT_LOGGING_ALLOC_FAILURE_TEST
  CHECK(cpkt_opcua_client_native_config(client, cpkt_log_client_hkdf_failure,
                                        &first.sink, &status) == CPKT_OPCUA_OK);
#endif
  /* Security configuration errors must also reach the installed hook. */
  CHECK(cpkt_opcua_client_native_config(client, cpkt_log_client_reject_password,
                                        NULL, &status) == CPKT_OPCUA_OK);
  before = first.sink.records;
  CHECK(cpkt_opcua_client_set_default_encryption(
            client, bad_key, sizeof(bad_key) - 1, bad_key, sizeof(bad_key) - 1,
            NULL, 0, NULL, 0, &status) == CPKT_OPCUA_ERR_UPSTREAM);
  CHECK(first.sink.records > before);
  cpkt_log_credentials(&certificate, &certificate_length, &key, &key_length);
  CHECK(cpkt_opcua_client_set_default_encryption(
            client, certificate, certificate_length, key, key_length, NULL, 0,
            NULL, 0, &status) == CPKT_OPCUA_OK);
  exercise(NULL, client, &first);
  before = first.sink.records;
  CHECK(cpkt_opcua_client_connect(client, "invalid endpoint", &status) ==
        CPKT_OPCUA_ERR_UPSTREAM);
  CHECK(status != 0 && first.sink.records > before);
  cpkt_opcua_client_free(client);
  CHECK(owned_clears == (legacy ? 2 : 0));
  client = NULL;
  CHECK(cpkt_opcua_server_new_from_json_with_logger(&server, json,
                                                    sizeof(json) - 1, &logger,
                                                    &status) == CPKT_OPCUA_OK);
  exercise(server, NULL, &first);
  CHECK(cpkt_opcua_server_native_config(server, cpkt_log_server_reject_password,
                                        NULL, &status) == CPKT_OPCUA_OK);
  before = first.sink.records;
  CHECK(cpkt_opcua_server_set_default_security(
            server, 0, bad_key, sizeof(bad_key) - 1, bad_key,
            sizeof(bad_key) - 1, NULL, 0, NULL, 0, NULL, 0,
            &status) == CPKT_OPCUA_ERR_UPSTREAM);
  CHECK(first.sink.records > before);
  CHECK(cpkt_opcua_server_set_default_security(
            server, 0, certificate, certificate_length, key, key_length, NULL,
            0, NULL, 0, NULL, 0, &status) == CPKT_OPCUA_OK);
  exercise(server, NULL, &first);
  CHECK(cpkt_opcua_server_set_default_security(
            server, 1, certificate, certificate_length, key, key_length, NULL,
            0, NULL, 0, NULL, 0, &status) == CPKT_OPCUA_OK);
  exercise(server, NULL, &first);
  cpkt_opcua_server_free(server);
  cpkt_log_credentials_free(certificate, key);
  server = NULL;
  before = first.sink.records;
  CHECK(cpkt_opcua_server_new_from_json_with_logger(
            &server, invalid, sizeof(invalid) - 1, &logger, &status) ==
        CPKT_OPCUA_ERR_UPSTREAM);
  CHECK(server == NULL && status != 0 && first.sink.records > before);
  file = fopen(path, "wb");
  CHECK(file != NULL);
  CHECK(fwrite(json, 1, sizeof(json) - 1, file) == sizeof(json) - 1);
  CHECK(fclose(file) == 0);
  CHECK(cpkt_opcua_server_new_from_json_file_with_logger(
            &server, path, &logger, &status) == CPKT_OPCUA_OK);
  CHECK(remove(path) == 0);
  exercise(server, NULL, &first);
  cpkt_opcua_server_free(server);
  cpkt_log_sink_clear(&first.sink);
  cpkt_log_sink_clear(&second.sink);
  puts("[test] OPC UA logging through libpslog passed");
  return 0;
}
