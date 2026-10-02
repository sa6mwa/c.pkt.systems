/* Complete C89 logging callbacks and stock native backends. */
#ifndef CPKT_OPCUA_LOGGER_IMPL_H
#define CPKT_OPCUA_LOGGER_IMPL_H
#include <open62541/plugin/log_stdout.h>
#if defined(__linux__) || defined(__unix__)
#include <open62541/plugin/log_syslog.h>
#endif

typedef struct {
  cpkt_opcua_Logger record;
  UA_Logger *native;
} cpkt_owned_logger;
typedef struct {
  cpkt_opcua_Logger record;
  void *config;
  void (*enter)(void *), (*leave)(void *);
} cpkt_logger_plugin;

static void cpkt_log_native(const UA_Logger *native, UA_LogLevel level,
                            UA_LogCategory category, const char *format, ...) {
  va_list args;
  if (!native || !native->log)
    return;
  va_start(args, format);
  native->log(native->context, level, category, format, args);
  va_end(args);
}
static void cpkt_log_public(const cpkt_opcua_Logger *logger,
                            cpkt_opcua_log_level level,
                            cpkt_opcua_log_category category,
                            const char *format, ...) {
  va_list args;
  if (!logger || !logger->log)
    return;
  va_start(args, format);
  logger->log(logger->context, level, category, format, args);
  va_end(args);
}
static void cpkt_log_backend(const UA_Logger *native,
                             cpkt_opcua_log_level level,
                             cpkt_opcua_log_category category,
                             const char *format, va_list args) {
  cpkt_opcua_String rendered;
  UA_String view;
  UA_StatusCode status;
  if (!native || !native->log || !format || (int)category < 0 ||
      (int)category >= UA_LOGCATEGORIES)
    return;
  memset(&rendered, 0, sizeof(rendered));
  status = cpkt_opcua_String_vformat(&rendered, format, args);
  if (!status) {
    view = cpkt_string_view(&rendered);
    cpkt_log_native(native, (UA_LogLevel)level, (UA_LogCategory)category, "%S",
                    view);
  } else {
    cpkt_log_native(native, (UA_LogLevel)level, (UA_LogCategory)category,
                    "C89 log message formatting failed: %08lx",
                    (unsigned long)status);
  }
  cpkt_opcua_String_clear(&rendered);
}
static void cpkt_log_stdout(void *context, cpkt_opcua_log_level level,
                            cpkt_opcua_log_category category,
                            const char *format, va_list args) {
  UA_Logger native = UA_Log_Stdout_withLevel(UA_LOGLEVEL_TRACE);
  native.context = context;
  cpkt_log_backend(&native, level, category, format, args);
}
#if defined(__linux__) || defined(__unix__)
static void cpkt_log_syslog(void *context, cpkt_opcua_log_level level,
                            cpkt_opcua_log_category category,
                            const char *format, va_list args) {
  UA_Logger native = UA_Log_Syslog();
  native.context = context;
  cpkt_log_backend(&native, level, category, format, args);
}
#endif
static void cpkt_log_owned(void *context, cpkt_opcua_log_level level,
                           cpkt_opcua_log_category category, const char *format,
                           va_list args) {
  cpkt_owned_logger *logger = (cpkt_owned_logger *)context;
  if (logger)
    cpkt_log_backend(logger->native, level, category, format, args);
}
static void cpkt_log_owned_clear(cpkt_opcua_Logger *record) {
  cpkt_owned_logger *logger;
  if (!record || !record->context)
    return;
  logger = (cpkt_owned_logger *)record->context;
  if (logger->native && logger->native->clear)
    logger->native->clear(logger->native);
  UA_free(logger);
}
static cpkt_opcua_Logger *cpkt_log_new(UA_Logger *native) {
  cpkt_owned_logger *logger;
  if (!native)
    return NULL;
  logger = (cpkt_owned_logger *)UA_calloc(1, sizeof(*logger));
  if (!logger) {
    if (native->clear)
      native->clear(native);
    return NULL;
  }
  logger->native = native;
  logger->record.context = logger;
  logger->record.log = cpkt_log_owned;
  logger->record.clear = cpkt_log_owned_clear;
  return &logger->record;
}
const cpkt_opcua_Logger cpkt_opcua_Log_Stdout_ = {cpkt_log_stdout, NULL, NULL};
const cpkt_opcua_Logger *cpkt_opcua_Log_Stdout = &cpkt_opcua_Log_Stdout_;
cpkt_opcua_Logger
cpkt_opcua_Log_Stdout_withLevel(cpkt_opcua_log_level minimum) {
  UA_Logger native = UA_Log_Stdout_withLevel((UA_LogLevel)minimum);
  cpkt_opcua_Logger out;
  out.log = cpkt_log_stdout;
  out.context = native.context;
  out.clear = NULL;
  return out;
}
cpkt_opcua_Logger *cpkt_opcua_Log_Stdout_new(cpkt_opcua_log_level minimum) {
  return cpkt_log_new(UA_Log_Stdout_new((UA_LogLevel)minimum));
}
#if defined(__linux__) || defined(__unix__)
cpkt_opcua_Logger cpkt_opcua_Log_Syslog(void) {
  UA_Logger native = UA_Log_Syslog();
  cpkt_opcua_Logger out;
  out.log = cpkt_log_syslog;
  out.context = native.context;
  out.clear = NULL;
  return out;
}
cpkt_opcua_Logger
cpkt_opcua_Log_Syslog_withLevel(cpkt_opcua_log_level minimum) {
  UA_Logger native = UA_Log_Syslog_withLevel((UA_LogLevel)minimum);
  cpkt_opcua_Logger out;
  out.log = cpkt_log_syslog;
  out.context = native.context;
  out.clear = NULL;
  return out;
}
cpkt_opcua_Logger *cpkt_opcua_Log_Syslog_new(cpkt_opcua_log_level minimum) {
  return cpkt_log_new(UA_Log_Syslog_new((UA_LogLevel)minimum));
}
#endif

static void cpkt_log_plugin(const cpkt_opcua_log_record *message,
                            void *context) {
  cpkt_logger_plugin *plugin = (cpkt_logger_plugin *)context;
  cpkt_opcua_String text;
  text.length = message->message_length;
  text.data = (cpkt_opcua_Byte *)message->message;
  plugin->enter(plugin->config);
  cpkt_log_public(&plugin->record, message->level, message->category, "%S",
                  text);
  plugin->leave(plugin->config);
}
static void cpkt_log_plugin_clear(void *context) {
  cpkt_logger_plugin *plugin = (cpkt_logger_plugin *)context;
  plugin->enter(plugin->config);
  if (plugin->record.clear)
    plugin->record.clear(&plugin->record);
  plugin->leave(plugin->config);
  UA_free(plugin);
}
static void cpkt_log_client_view(void *context, cpkt_opcua_log_level level,
                                 cpkt_opcua_log_category category,
                                 const char *format, va_list args) {
  cpkt_opcua_ClientConfig *config = (cpkt_opcua_ClientConfig *)context;
  if (!config || !config->native)
    return;
  cpkt_cfg_client_enter(config);
  cpkt_log_backend(config->native->logging, level, category, format, args);
  cpkt_cfg_client_leave(config);
}
static void cpkt_log_server_view(void *context, cpkt_opcua_log_level level,
                                 cpkt_opcua_log_category category,
                                 const char *format, va_list args) {
  cpkt_opcua_ServerConfig *config = (cpkt_opcua_ServerConfig *)context;
  if (!config || !config->native)
    return;
  cpkt_cfg_server_enter(config);
  cpkt_log_backend(config->native->logging, level, category, format, args);
  cpkt_cfg_server_leave(config);
}
static UA_StatusCode cpkt_log_install(struct cpkt_opcua_logger *bridge,
                                      UA_Logger **slot,
                                      cpkt_opcua_Logger *record, void *owner,
                                      void (*enter)(void *),
                                      void (*leave)(void *)) {
  cpkt_logger_plugin *plugin = NULL;
  cpkt_opcua_log_config config;
  memset(&config, 0, sizeof(config));
  if (record) {
    if (record->log == cpkt_log_client_view ||
        record->log == cpkt_log_server_view)
      return UA_STATUSCODE_BADINVALIDARGUMENT;
    plugin = (cpkt_logger_plugin *)UA_calloc(1, sizeof(*plugin));
    if (!plugin)
      return UA_STATUSCODE_BADOUTOFMEMORY;
    plugin->record = *record;
    plugin->config = owner;
    plugin->enter = enter;
    plugin->leave = leave;
    config.fn = cpkt_log_plugin;
    config.user = plugin;
  }
  if (!*slot)
    *slot = &bridge->native;
  cpkt_logger_set(bridge, *slot, &config);
  if (plugin) {
    bridge->owned_context = plugin;
    bridge->owned_clear = cpkt_log_plugin_clear;
    memset(record, 0, sizeof(*record));
  }
  return 0;
}
static void cpkt_log_client_enter(void *config) {
  cpkt_cfg_client_enter((cpkt_opcua_ClientConfig *)config);
}
static void cpkt_log_client_leave(void *config) {
  cpkt_cfg_client_leave((cpkt_opcua_ClientConfig *)config);
}
static void cpkt_log_server_enter(void *config) {
  cpkt_cfg_server_enter((cpkt_opcua_ServerConfig *)config);
}
static void cpkt_log_server_leave(void *config) {
  cpkt_cfg_server_leave((cpkt_opcua_ServerConfig *)config);
}
cpkt_opcua_StatusCode
cpkt_opcua_ClientConfig_setLoggerPlugin(cpkt_opcua_ClientConfig *config,
                                        cpkt_opcua_Logger *record) {
  UA_StatusCode status;
  if (!config || !config->native)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!cpkt_cfg_client_mutable(config))
    return UA_STATUSCODE_BADINVALIDSTATE;
  cpkt_cfg_client_enter(config);
  status =
      cpkt_log_install(&config->logger, &config->native->logging, record,
                       config, cpkt_log_client_enter, cpkt_log_client_leave);
  cpkt_cfg_client_leave(config);
  return status;
}
cpkt_opcua_StatusCode
cpkt_opcua_ServerConfig_setLoggerPlugin(cpkt_opcua_ServerConfig *config,
                                        cpkt_opcua_Logger *record) {
  UA_StatusCode status;
  if (!config || !config->native)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!cpkt_cfg_server_mutable(config))
    return UA_STATUSCODE_BADINVALIDSTATE;
  cpkt_cfg_server_enter(config);
  status =
      cpkt_log_install(&config->logger, &config->native->logging, record,
                       config, cpkt_log_server_enter, cpkt_log_server_leave);
  cpkt_cfg_server_leave(config);
  return status;
}
cpkt_opcua_StatusCode
cpkt_opcua_ClientConfig_getLoggerPlugin(cpkt_opcua_ClientConfig *config,
                                        cpkt_opcua_Logger *record) {
  if (!config || !config->native || !record)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  record->log = config->native->logging && config->native->logging->log
                    ? cpkt_log_client_view
                    : NULL;
  record->context = config;
  record->clear = NULL;
  return 0;
}
cpkt_opcua_StatusCode
cpkt_opcua_ServerConfig_getLoggerPlugin(cpkt_opcua_ServerConfig *config,
                                        cpkt_opcua_Logger *record) {
  if (!config || !config->native || !record)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  record->log = config->native->logging && config->native->logging->log
                    ? cpkt_log_server_view
                    : NULL;
  record->context = config;
  record->clear = NULL;
  return 0;
}

#define CPKT_DEFINE_LOG_HELPER(name, minimum)                                  \
  void cpkt_opcua_LOG_##name(const cpkt_opcua_Logger *logger,                  \
                             cpkt_opcua_log_category category,                 \
                             const char *format, ...) {                        \
    va_list args;                                                              \
    if (UA_LOGLEVEL > minimum || !logger || !logger->log || !format)           \
      return;                                                                  \
    va_start(args, format);                                                    \
    logger->log(logger->context, (cpkt_opcua_log_level)minimum, category,      \
                format, args);                                                 \
    va_end(args);                                                              \
  }
CPKT_DEFINE_LOG_HELPER(TRACE, 100)
CPKT_DEFINE_LOG_HELPER(DEBUG, 200)
CPKT_DEFINE_LOG_HELPER(INFO, 300)
CPKT_DEFINE_LOG_HELPER(WARNING, 400)
CPKT_DEFINE_LOG_HELPER(ERROR, 500)
CPKT_DEFINE_LOG_HELPER(FATAL, 600)
#undef CPKT_DEFINE_LOG_HELPER
#endif
