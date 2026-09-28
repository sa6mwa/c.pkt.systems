#include "opcua_callbacks_test.h"
#include "opcua_types_peer.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check(int condition, const char *expression, int line) {
  if (!condition) {
    fprintf(stderr, "format/logging line %d: %s\n", line, expression);
    abort();
  }
}
#define CHECK(e) check(!!(e), #e, __LINE__)
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
void cpkt_types_fail_after(size_t count);
int cpkt_types_fail_stop(void);
#endif
static cpkt_opcua_String text(const char *value) {
  cpkt_opcua_String out;
  out.length = strlen(value);
  out.data = (cpkt_opcua_Byte *)value;
  return out;
}
static cpkt_opcua_StatusCode vformat(cpkt_opcua_String *out, const char *format,
                                     ...) {
  cpkt_opcua_StatusCode status;
  va_list args;
  va_start(args, format);
  status = cpkt_opcua_String_vformat(out, format, args);
  va_end(args);
  return status;
}
static void equal(const cpkt_opcua_String *out, const char *expected) {
  CHECK(out->length == strlen(expected));
  CHECK(!out->length || memcmp(out->data, expected, out->length) == 0);
}
static void native_equal(unsigned int kind, const cpkt_opcua_String *out) {
  unsigned char *bytes = NULL;
  size_t length = 0;
  CHECK(cpkt_types_peer_format(kind, &bytes, &length) == 0);
  CHECK(out->length == length &&
        (!length || memcmp(out->data, bytes, length) == 0));
  cpkt_types_native_free(bytes);
}
static void formatting(void) {
  cpkt_opcua_String out, word = text("schema");
  cpkt_opcua_NodeId node;
  cpkt_opcua_QualifiedName name;
  cpkt_opcua_FormatArg args[4];
  unsigned char buffer[64];
  memset(&out, 0, sizeof(out));
  memset(&node, 0, sizeof(node));
  memset(&name, 0, sizeof(name));
  node.namespaceIndex = 2;
  node.identifierType = CPKT_OPCUA_NODEIDTYPE_STRING;
  node.identifier.string = text("node");
  name.namespaceIndex = 3;
  name.name = text("qualified");
  CHECK(cpkt_opcua_String_format(&out, "[%S|%N|%Q]", word, node, name) == 0);
  native_equal(0, &out);
  cpkt_opcua_String_clear(&out);
  CHECK(vformat(&out, "%*.*s %hhd %hu %ld %lu %x %c %% %p", -8, 3, "abcdef", -7,
                (unsigned int)65000, -1234L, 3456UL, (unsigned int)0xabc, 'Z',
                (void *)NULL) == 0);
  native_equal(1, &out);
  cpkt_opcua_String_clear(&out);
  memset(args, 0, sizeof(args));
  args[0].kind = CPKT_OPCUA_FORMAT_INT64;
  args[0].value.int64Value.high32 = 0x80000000U;
  args[1].kind = CPKT_OPCUA_FORMAT_UINT64;
  args[1].value.uint64Value.high32 = args[1].value.uint64Value.low32 =
      0xffffffffU;
  args[2].kind = CPKT_OPCUA_FORMAT_INT;
  args[2].value.intValue = INT_MIN;
  CHECK(cpkt_opcua_String_format_args(&out, "%lld %llu %*s", 3, args) ==
        CPKT_OPCUA_STATUSCODE_BADTYPEMISMATCH);
  CHECK(out.data == NULL && out.length == 0);
  CHECK(cpkt_opcua_String_format_args(&out, "%lld %llu %d", 3, args) == 0);
  native_equal(2, &out);
  cpkt_opcua_String_clear(&out);
  args[0].kind = CPKT_OPCUA_FORMAT_STRING;
  args[0].value.stringValue = word;
  args[1].kind = CPKT_OPCUA_FORMAT_NODEID;
  args[1].value.nodeIdValue = node;
  args[2].kind = CPKT_OPCUA_FORMAT_QUALIFIEDNAME;
  args[2].value.qualifiedNameValue = name;
  CHECK(cpkt_opcua_String_format_args(&out, "[%S|%N|%Q]", 3, args) == 0);
  native_equal(0, &out);
  cpkt_opcua_String_clear(&out);
  args[0].value.stringValue.length = 1;
  args[0].value.stringValue.data = NULL;
  CHECK(cpkt_opcua_String_format_args(&out, "%S", 1, args) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(out.data == NULL);
  args[0].kind = CPKT_OPCUA_FORMAT_INT;
  args[0].value.intValue = INT_MIN;
  args[1].kind = CPKT_OPCUA_FORMAT_STRING;
  args[1].value.stringValue.length = 1;
  args[1].value.stringValue.data = NULL;
  CHECK(cpkt_opcua_String_format_args(&out, "%*S", 2, args) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(out.data == NULL && out.length == 0);
  out.data = buffer;
  out.length = sizeof(buffer);
  CHECK(cpkt_opcua_String_format(&out, "%s", "preallocated") == 0);
  CHECK(out.data == buffer);
  equal(&out, "preallocated");
  out.length = 4;
  CHECK(cpkt_opcua_String_format(&out, "abcdef") ==
        CPKT_OPCUA_STATUSCODE_BADENCODINGLIMITSEXCEEDED);
  CHECK(out.data == buffer && out.length == 4 && memcmp(buffer, "abc", 4) == 0);
  out.length = sizeof(buffer);
  CHECK(cpkt_opcua_String_format(&out, "") == 0 && out.data == buffer &&
        out.length == 0);
  memset(&out, 0, sizeof(out));
  CHECK(cpkt_opcua_String_format(&out, "") == 0 &&
        out.data == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL);
  cpkt_opcua_String_clear(&out);
  CHECK(cpkt_opcua_String_format(NULL, "x") ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_String_format(&out, NULL) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  cpkt_types_fail_after(0);
  CHECK(cpkt_opcua_String_format(&out, "allocation") ==
        CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY);
  CHECK(cpkt_types_fail_stop());
  CHECK(out.data == NULL && out.length == 0);
#endif
}
static cpkt_opcua_ClientConfig *logging_config;
static cpkt_opcua_String logged;
static unsigned int messages, clears;
static void logger(void *context, cpkt_opcua_log_level level,
                   cpkt_opcua_log_category category, const char *format,
                   va_list args) {
  CHECK(context == &messages && (int)level >= 100 && (int)category >= 0);
  if (logging_config)
    CHECK(cpkt_opcua_ClientConfig_delete(logging_config) != 0);
  cpkt_opcua_String_clear(&logged);
  CHECK(cpkt_opcua_String_vformat(&logged, format, args) == 0);
  ++messages;
}
static void logger_clear(cpkt_opcua_Logger *record) {
  CHECK(record->context == &messages);
  if (logging_config)
    CHECK(cpkt_opcua_ClientConfig_delete(logging_config) != 0);
  ++clears;
}
static void logging(void) {
  cpkt_opcua_Logger record, view;
  cpkt_opcua_Logger *owned;
  cpkt_opcua_String word = text("schema");
  cpkt_opcua_ClientConfig *config = cpkt_opcua_ClientConfig_new();
  CHECK(config != NULL);
  record.log = logger;
  record.context = &messages;
  record.clear = logger_clear;
  messages = clears = 0;
  CHECK(cpkt_opcua_ClientConfig_setLoggerPlugin(config, &record) == 0);
  CHECK(record.log == NULL && record.context == NULL && record.clear == NULL);
  CHECK(cpkt_opcua_ClientConfig_getLoggerPlugin(config, &view) == 0 &&
        view.log != NULL && view.clear == NULL);
  logging_config = config;
  cpkt_opcua_LOG_WARNING(&view, CPKT_OPCUA_LOG_APPLICATION, "custom %S %ld",
                         word, -42L);
  CHECK(messages == 1);
  equal(&logged, "custom schema -42");
  CHECK(cpkt_opcua_ClientConfig_setLoggerPlugin(config, &view) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_ClientConfig_setLogger(config, NULL) == 0 && clears == 1);
  logging_config = NULL;
  CHECK(cpkt_opcua_ClientConfig_delete(config) == 0 && clears == 1);
  cpkt_opcua_String_clear(&logged);
  owned = cpkt_opcua_Log_Stdout_new(CPKT_OPCUA_LOG_FATAL);
  CHECK(owned != NULL && owned->clear != NULL && owned->log != NULL);
  owned->clear(owned);
  record = cpkt_opcua_Log_Stdout_withLevel(CPKT_OPCUA_LOG_FATAL);
  CHECK(record.log != NULL && record.clear == NULL &&
        cpkt_opcua_Log_Stdout == &cpkt_opcua_Log_Stdout_);
#if defined(__linux__) || defined(__unix__)
  owned = cpkt_opcua_Log_Syslog_new(CPKT_OPCUA_LOG_FATAL);
  CHECK(owned != NULL && owned->clear != NULL);
  owned->clear(owned);
  record = cpkt_opcua_Log_Syslog();
  CHECK(record.log != NULL && record.clear == NULL);
  record = cpkt_opcua_Log_Syslog_withLevel(CPKT_OPCUA_LOG_FATAL);
  CHECK(record.log != NULL && record.clear == NULL);
#endif
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  cpkt_types_fail_after(0);
  CHECK(cpkt_opcua_Log_Stdout_new(CPKT_OPCUA_LOG_FATAL) == NULL);
  CHECK(cpkt_types_fail_stop());
#endif
}
static cpkt_opcua_client *logging_client;
static unsigned int feedback_messages, feedback_clears;
static void feedback_logger(void *context, cpkt_opcua_log_level level,
                            cpkt_opcua_log_category category,
                            const char *format, va_list args) {
  (void)level;
  (void)category;
  (void)format;
  (void)args;
  CHECK(context == &feedback_messages);
  ++feedback_messages;
  CHECK(feedback_messages < 10);
  cpkt_opcua_client_free(logging_client);
}
static void feedback_clear(cpkt_opcua_Logger *record) {
  CHECK(record->context == &feedback_messages);
  ++feedback_clears;
}
static cpkt_opcua_status native_log(void *native, void *context) {
  (void)context;
  cpkt_types_peer_log(native);
  return 0;
}
static void native_logging_ownership(void) {
  cpkt_opcua_ClientConfig *config;
  cpkt_opcua_Logger record;
  cpkt_opcua_log_config rendered;
  cpkt_opcua_log_config quiet;
  memset(&quiet, 0, sizeof(quiet));
  CHECK(cpkt_opcua_client_new(&logging_client) == CPKT_OPCUA_OK);
  CHECK(cpkt_opcua_client_set_logger(logging_client, &quiet) == CPKT_OPCUA_OK);
  CHECK(cpkt_opcua_client_get_config_typed(logging_client, &config) == 0);
  feedback_messages = feedback_clears = 0;
  record.log = feedback_logger;
  record.clear = feedback_clear;
  record.context = &feedback_messages;
  CHECK(cpkt_opcua_ClientConfig_setLoggerPlugin(config, &record) == 0);
  CHECK(cpkt_opcua_ClientConfig_getLogger(config, &rendered) ==
        CPKT_OPCUA_STATUSCODE_BADNOTSUPPORTED);
  /* Native messages pin the client. Destruction feedback is also pinned and
   * cannot recursively emit an unbounded chain of destruction errors. */
  CHECK(cpkt_opcua_client_native(logging_client, native_log, NULL) ==
        CPKT_OPCUA_OK);
  CHECK(feedback_messages == 2);
  CHECK(cpkt_opcua_client_set_logger(logging_client, &quiet) == CPKT_OPCUA_OK &&
        feedback_clears == 1);
  record.log = feedback_logger;
  record.clear = feedback_clear;
  record.context = &feedback_messages;
  CHECK(cpkt_opcua_ClientConfig_setLoggerPlugin(config, &record) == 0);
  CHECK(cpkt_opcua_ClientConfig_setLogger(config, NULL) == 0 &&
        feedback_clears == 2);
  cpkt_opcua_client_free(logging_client);
  logging_client = NULL;
  CHECK(feedback_clears == 2);
}
void cpkt_types_test_formatting(void) {
  formatting();
  logging();
  native_logging_ownership();
}
