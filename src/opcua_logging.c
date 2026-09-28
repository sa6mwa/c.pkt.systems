/* Handwritten OPC UA facade: logging. */
#include "opcua_facade_internal.h"
#include <string.h>

static void cpkt_logger_release(struct cpkt_opcua_logger *logger) {
  void *context = logger->owned_context;
  void (*clear)(void *) = logger->owned_clear;
  logger->owned_context = NULL;
  logger->owned_clear = NULL;
  if (clear)
    clear(context);
}

/* Shared private logger bridge, defined once for all facade translation units.
 */
int cpkt_logger_valid(const cpkt_opcua_log_config *config) {
  return config == NULL || config->min_level == 0 ||
         (config->min_level >= CPKT_OPCUA_LOG_TRACE &&
          config->min_level <= CPKT_OPCUA_LOG_FATAL &&
          config->min_level % 100 == 0);
}

void cpkt_logger_log(void *context, UA_LogLevel level, UA_LogCategory category,
                     const char *format, va_list args) {
  struct cpkt_opcua_logger *logger;
  cpkt_opcua_log_record record;
  UA_String message;
  UA_StatusCode status;
  static const char failure[] = "OPC UA log message formatting failed";

  logger = (struct cpkt_opcua_logger *)context;
  if (logger->config.fn == NULL || (int)level < (int)logger->config.min_level) {
    return;
  }
  message = UA_STRING_NULL;
  status = UA_String_vformat(&message, format, args);
  record.level = (cpkt_opcua_log_level)level;
  record.category = (cpkt_opcua_log_category)category;
  record.format_status = (cpkt_opcua_status)status;
  if (status == UA_STATUSCODE_GOOD) {
    record.message = message.length == 0 ? "" : (const char *)message.data;
    record.message_length = message.length;
  } else {
    record.message = failure;
    record.message_length = sizeof(failure) - 1;
  }
  logger->config.fn(&record, logger->config.user);
  UA_String_clear(&message);
}

static void cpkt_logger_clear(UA_Logger *native) {
  struct cpkt_opcua_logger *logger;
  logger = (struct cpkt_opcua_logger *)native->context;
  /* An upstream plugin may own context as well as the logger allocation.
   * Restore its complete object before calling its original destructor. */
  cpkt_logger_release(logger);
  *native = logger->original;
  if (native->clear != NULL) {
    native->clear(native);
  }
}

void cpkt_logger_set(struct cpkt_opcua_logger *logger, UA_Logger *native,
                     const cpkt_opcua_log_config *config) {
  /* Config and legacy handles can replace one another on the same native
   * address. Unwrap prior facade destinations and release their owned context
   * without running the native address destructor until final teardown. */
  while (native->log == cpkt_logger_log && native->context != logger) {
    struct cpkt_opcua_logger *previous =
        (struct cpkt_opcua_logger *)native->context;
    *native = previous->original;
    cpkt_logger_release(previous);
    previous->configured = 0;
    memset(&previous->config, 0, sizeof(previous->config));
  }
  if (native->log != cpkt_logger_log || native->context != logger) {
    logger->original = *native;
  }
  cpkt_logger_release(logger);
  logger->config = *config;
  logger->configured = 1;
  native->log = cpkt_logger_log;
  native->context = logger;
  native->clear = cpkt_logger_clear;
  /* Keep the plugin address borrowed by event-loop/security plugins. */
}
