#include "opcua_logging_peer.h"
#include <open62541/client.h>
#include <open62541/plugin/create_certificate.h>
#include <open62541/plugin/securitypolicy_default.h>
#include <open62541/server.h>
#include <open62541/util.h>
#include <pslog.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "logging peer check failed at %d: %s\n", __LINE__,       \
              #expr);                                                          \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

/* Consume chunks immediately, retaining only a bounded prefix for assertions.
 */
static int sink_write(void *user, const char *data, size_t length,
                      size_t *written) {
  struct cpkt_log_sink *sink = user;
  size_t copy = sizeof(sink->prefix) - 1 - sink->prefix_length;
  if (copy > length)
    copy = length;
  memcpy(sink->prefix + sink->prefix_length, data, copy);
  sink->prefix_length += copy;
  sink->prefix[sink->prefix_length] = '\0';
  for (size_t i = 0; i < length; ++i) {
    if (data[i] == '\n')
      ++sink->lines;
    if (data[i] == 'z')
      ++sink->z_count;
  }
  sink->bytes += length;
  ++sink->chunks;
  *written = length;
  return 0;
}

void cpkt_log_sink_init(struct cpkt_log_sink *sink) {
  pslog_config config;
  memset(sink, 0, sizeof(*sink));
  pslog_default_config(&config);
  config.mode = PSLOG_MODE_JSON;
  config.color = PSLOG_COLOR_NEVER;
  config.timestamps = 0;
  config.min_level = PSLOG_LEVEL_TRACE;
  memset(&config.output, 0, sizeof(config.output));
  config.output.write = sink_write;
  config.output.userdata = sink;
  sink->logger = pslog_new(&config);
  CHECK(sink->logger != NULL);
}

void cpkt_log_sink_clear(struct cpkt_log_sink *sink) {
  pslog_logger *logger = sink->logger;
  logger->destroy(logger);
  sink->logger = NULL;
}

void cpkt_log_sink_emit(struct cpkt_log_sink *sink,
                        const cpkt_opcua_log_record *record) {
  pslog_logger *logger = sink->logger;
  pslog_field fields[3];
  pslog_string_view message = {record->message, record->message_length};
  pslog_level level;
  switch (record->level) {
  case CPKT_OPCUA_LOG_TRACE:
    level = PSLOG_LEVEL_TRACE;
    break;
  case CPKT_OPCUA_LOG_DEBUG:
    level = PSLOG_LEVEL_DEBUG;
    break;
  case CPKT_OPCUA_LOG_INFO:
    level = PSLOG_LEVEL_INFO;
    break;
  case CPKT_OPCUA_LOG_WARNING:
    level = PSLOG_LEVEL_WARN;
    break;
  case CPKT_OPCUA_LOG_ERROR:
    level = PSLOG_LEVEL_ERROR;
    break;
  case CPKT_OPCUA_LOG_FATAL:
    level = PSLOG_LEVEL_FATAL;
    break;
  default:
    CHECK(0);
    return;
  }
  fields[0] = pslog_i64("opcua_level", record->level);
  fields[1] = pslog_i64("opcua_category", record->category);
  fields[2] = pslog_u64("format_status", record->format_status);
  sink->bytes = sink->lines = sink->chunks = sink->z_count = 0;
  sink->prefix_length = 0;
  sink->prefix[0] = '\0';
  /* Generic log_view preserves FATAL severity without calling fatal_view,
   * which is a separate application decision to terminate the process. */
  logger->log_view(logger, level, message, fields, 3);
  CHECK(sink->lines == 1);
  CHECK(sink->bytes > record->message_length);
  sink->last_level = record->level;
  sink->last_category = record->category;
  ++sink->records;
}

static void emit(const UA_Logger *logger, UA_LogLevel level,
                 UA_LogCategory category, const char *format, ...) {
  va_list args;
  va_start(args, format);
  logger->log(logger->context, level, category, format, args);
  va_end(args);
}

static void emit_request(const UA_Logger *logger,
                         const struct cpkt_log_request *request) {
  UA_String message = {request->length, (UA_Byte *)(void *)request->message};
  CHECK(logger != NULL && logger->log != NULL);
  if (request->kind == 1) {
    emit(logger, (UA_LogLevel)request->level, (UA_LogCategory)request->category,
         "value=%S node=%N name=%Q number=%d", message,
         UA_NODEID_NUMERIC(1, 42), UA_QUALIFIEDNAME(2, "name"), 17);
  } else {
    emit(logger, (UA_LogLevel)request->level, (UA_LogCategory)request->category,
         "%S", message);
  }
}

cpkt_opcua_status cpkt_log_server_emit(void *native, void *user) {
  UA_ServerConfig *config = native;
  const struct cpkt_log_request *request = user;
  const UA_Logger *logger = config->logging;
  CHECK(config->eventLoop->logger == logger);
  CHECK(config->secureChannelPKI.logging == logger);
  CHECK(config->sessionPKI.logging == logger);
  if (request->plugin == 1)
    logger = config->eventLoop->logger;
  if (request->plugin == 2)
    logger = config->secureChannelPKI.logging;
  emit_request(logger, request);
  return 0;
}

cpkt_opcua_status cpkt_log_client_emit(void *native, void *user) {
  UA_ClientConfig *config = native;
  const struct cpkt_log_request *request = user;
  const UA_Logger *logger = config->logging;
  CHECK(config->eventLoop->logger == logger);
  CHECK(config->certificateVerification.logging == logger);
  if (request->plugin == 1)
    logger = config->eventLoop->logger;
  if (request->plugin == 2)
    logger = config->certificateVerification.logging;
  emit_request(logger, request);
  return 0;
}

struct owned_logger_context {
  int *clear_count;
};
static void owned_logger_clear(UA_Logger *logger) {
  struct owned_logger_context *context = logger->context;
  CHECK(context != NULL && context->clear_count != NULL);
  ++*context->clear_count;
  UA_free(context);
  UA_free(logger);
}
static void owned_logger_log(void *context, UA_LogLevel level,
                             UA_LogCategory category, const char *format,
                             va_list args) {
  (void)context;
  (void)level;
  (void)category;
  (void)format;
  (void)args;
}
static void install_owned_logger(UA_Logger *logger, void *user) {
  struct owned_logger_context *context = UA_malloc(sizeof(*context));
  CHECK(context != NULL);
  context->clear_count = user;
  logger->log = owned_logger_log;
  logger->context = context;
  logger->clear = owned_logger_clear;
}
cpkt_opcua_status cpkt_log_server_owned_logger(void *native, void *user) {
  install_owned_logger(((UA_ServerConfig *)native)->logging, user);
  return 0;
}
cpkt_opcua_status cpkt_log_client_owned_logger(void *native, void *user) {
  install_owned_logger(((UA_ClientConfig *)native)->logging, user);
  return 0;
}

static UA_StatusCode server_password(UA_ServerConfig *config,
                                     UA_ByteString *password) {
  (void)config;
  (void)password;
  return UA_STATUSCODE_BADSECURITYCHECKSFAILED;
}
static UA_StatusCode client_password(UA_ClientConfig *config,
                                     UA_ByteString *password) {
  (void)config;
  (void)password;
  return UA_STATUSCODE_BADSECURITYCHECKSFAILED;
}
cpkt_opcua_status cpkt_log_server_reject_password(void *native, void *user) {
  (void)user;
  ((UA_ServerConfig *)native)->privateKeyPasswordCallback = server_password;
  return 0;
}
cpkt_opcua_status cpkt_log_client_reject_password(void *native, void *user) {
  (void)user;
  ((UA_ClientConfig *)native)->privateKeyPasswordCallback = client_password;
  return 0;
}
void cpkt_log_credentials(unsigned char **certificate,
                          size_t *certificate_length, unsigned char **key,
                          size_t *key_length) {
  UA_Logger quiet = {NULL, NULL, NULL};
  UA_String subject[] = {UA_STRING("CN=cpkt logging test")};
  UA_String names[] = {UA_STRING("DNS:localhost"),
                       UA_STRING("URI:urn:cpkt:logging")};
  UA_UInt16 bits = 2048;
  UA_KeyValueMap params = UA_KEYVALUEMAP_NULL;
  UA_ByteString private_key = UA_BYTESTRING_NULL;
  UA_ByteString cert = UA_BYTESTRING_NULL;
  CHECK(UA_KeyValueMap_setScalar(&params, UA_QUALIFIEDNAME(0, "key-size-bits"),
                                 &bits, &UA_TYPES[UA_TYPES_UINT16]) ==
        UA_STATUSCODE_GOOD);
  CHECK(UA_CreateCertificate(&quiet, subject, 1, names, 2,
                             UA_CERTIFICATEFORMAT_DER, &params, &private_key,
                             &cert) == UA_STATUSCODE_GOOD);
  UA_KeyValueMap_clear(&params);
  *certificate = cert.data;
  *certificate_length = cert.length;
  *key = private_key.data;
  *key_length = private_key.length;
}
void cpkt_log_credentials_free(unsigned char *certificate, unsigned char *key) {
  UA_free(certificate);
  UA_free(key);
}

#ifdef CPKT_LOGGING_ALLOC_FAILURE_TEST
#include <openssl/err.h>
#include <openssl/kdf.h>

static int fail_hkdf;
static int hkdf_failed;
int __real_EVP_KDF_derive(EVP_KDF_CTX *context, unsigned char *key,
                          size_t length, const OSSL_PARAM params[]);
int __wrap_EVP_KDF_derive(EVP_KDF_CTX *context, unsigned char *key,
                          size_t length, const OSSL_PARAM params[]);
int __wrap_EVP_KDF_derive(EVP_KDF_CTX *context, unsigned char *key,
                          size_t length, const OSSL_PARAM params[]) {
  if (fail_hkdf) {
    fail_hkdf = 0;
    hkdf_failed = 1;
    ERR_raise_data(ERR_LIB_USER, 1, "cpkt injected HKDF failure");
    return 0;
  }
  return __real_EVP_KDF_derive(context, key, length, params);
}

cpkt_opcua_status cpkt_log_client_hkdf_failure(void *native, void *user) {
  UA_ClientConfig *config = native;
  struct cpkt_log_sink *sink = user;
  UA_Logger quiet = {NULL, NULL, NULL};
  UA_String subject[] = {UA_STRING("CN=cpkt logging ECC test")};
  UA_String names[] = {UA_STRING("DNS:localhost"),
                       UA_STRING("URI:urn:cpkt:logging")};
  UA_String type = UA_STRING("EC");
  UA_KeyValueMap params = UA_KEYVALUEMAP_NULL;
  UA_ByteString key = UA_BYTESTRING_NULL;
  UA_ByteString certificate = UA_BYTESTRING_NULL;
  for (int policy_kind = 0; policy_kind < 2; ++policy_kind) {
    UA_SecurityPolicy policy;
    UA_ByteString nonce = UA_BYTESTRING_NULL;
    UA_ByteString output = UA_BYTESTRING_NULL;
    void *channel = NULL;
    size_t before;
    UA_String curve =
        policy_kind == 0 ? UA_STRING("prime256v1") : UA_STRING("ed25519");
    CHECK(UA_KeyValueMap_setScalar(&params, UA_QUALIFIEDNAME(0, "key-type"),
                                   &type, &UA_TYPES[UA_TYPES_STRING]) ==
          UA_STATUSCODE_GOOD);
    CHECK(UA_KeyValueMap_setScalar(&params, UA_QUALIFIEDNAME(0, "ecc-curve"),
                                   &curve, &UA_TYPES[UA_TYPES_STRING]) ==
          UA_STATUSCODE_GOOD);
    CHECK(UA_CreateCertificate(&quiet, subject, 1, names, 2,
                               UA_CERTIFICATEFORMAT_DER, &params, &key,
                               &certificate) == UA_STATUSCODE_GOOD);
    UA_KeyValueMap_clear(&params);
    memset(&policy, 0, sizeof(policy));
    if (policy_kind == 0) {
      CHECK(UA_SecurityPolicy_EccNistP256AesGcm(
                &policy, UA_APPLICATIONTYPE_CLIENT, certificate, key,
                config->logging) == UA_STATUSCODE_GOOD);
    } else {
      CHECK(UA_SecurityPolicy_EccCurve25519(
                &policy, UA_APPLICATIONTYPE_CLIENT, certificate, key,
                config->logging) == UA_STATUSCODE_GOOD);
    }
    CHECK(policy.newChannelContext(&policy, &certificate, &channel) ==
          UA_STATUSCODE_GOOD);
    CHECK(UA_ByteString_allocBuffer(&nonce, policy.nonceLength) ==
          UA_STATUSCODE_GOOD);
    memcpy(nonce.data, "eph", 3);
    CHECK(policy.generateNonce(&policy, channel, &nonce) == UA_STATUSCODE_GOOD);
    CHECK(UA_ByteString_allocBuffer(&output, 32) == UA_STATUSCODE_GOOD);
    /* Upstream also inspects the output's initial bytes as a mode marker. */
    memset(output.data, 0, output.length);
    /* Prove the setup reaches a working KDF before injecting its failure. */
    CHECK(policy.generateKey(&policy, channel, &nonce, &nonce, &output) ==
          UA_STATUSCODE_GOOD);
    memset(output.data, 0, output.length);
    before = sink->records;
    fail_hkdf = 1;
    hkdf_failed = 0;
    ERR_clear_error();
    CHECK(policy.generateKey(&policy, channel, &nonce, &nonce, &output) !=
          UA_STATUSCODE_GOOD);
    CHECK(hkdf_failed && sink->records == before + 1);
    CHECK(sink->last_level == CPKT_OPCUA_LOG_ERROR);
    CHECK(sink->last_category == CPKT_OPCUA_LOG_SECURITY);
    CHECK(strstr(sink->prefix, "OpenSSL key derivation failed") != NULL);
    CHECK(strstr(sink->prefix, "cpkt injected HKDF failure") != NULL);
    CHECK(ERR_peek_error() == 0);
    policy.deleteChannelContext(&policy, channel);
    policy.clear(&policy);
    UA_ByteString_clear(&nonce);
    UA_ByteString_clear(&output);
    UA_ByteString_clear(&key);
    UA_ByteString_clear(&certificate);
  }
  return 0;
}
#endif
