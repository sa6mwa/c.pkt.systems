/* Native architecture event loops. Public handles retain ABI conversions and
 * callback ownership only; all polling, timers, packets and signals are native.
 */
#ifndef CPKT_OPCUA_EVENTLOOP_IMPL_H
#define CPKT_OPCUA_EVENTLOOP_IMPL_H
#include <open62541/plugin/eventloop.h>

typedef struct cpkt_connection_registration cpkt_connection_registration;
typedef struct cpkt_connection_slot cpkt_connection_slot;
typedef struct cpkt_interrupt_registration cpkt_interrupt_registration;

struct cpkt_opcua_EventLoop {
  UA_EventLoop *native;
  UA_EventLoop **configuration_slot;
  struct cpkt_opcua_EventLoop *registry_next;
  UA_StatusCode (*native_start)(UA_EventLoop *);
  void (*native_stop)(UA_EventLoop *);
  UA_StatusCode (*native_free)(UA_EventLoop *);
  UA_StatusCode (*native_run)(UA_EventLoop *, UA_UInt32);
  UA_StatusCode (*native_register)(UA_EventLoop *, UA_EventSource *);
  UA_StatusCode (*native_deregister)(UA_EventLoop *, UA_EventSource *);
  cpkt_opcua_EventLoopPlugin plugin;
  void *context;
  int custom;
  struct cpkt_opcua_logger logger;
  cpkt_opcua_EventSource *sources;
  unsigned int depth;
  int freeing;
};
struct cpkt_opcua_EventSource {
  UA_EventSource *native;
  struct cpkt_opcua_EventSource *registry_next;
  UA_StatusCode (*native_free)(UA_EventSource *);
  UA_EventSourceType kind;
  cpkt_opcua_EventSourcePlugin plugin;
  void *context;
  int custom;
  cpkt_opcua_EventLoop *loop;
  cpkt_opcua_EventSource *next;
  unsigned int depth;
};
struct cpkt_opcua_ConnectionManager {
  cpkt_opcua_EventSource source;
  cpkt_connection_slot *connections;
  cpkt_opcua_ConnectionManagerPlugin plugin;
};
struct cpkt_opcua_InterruptManager {
  cpkt_opcua_EventSource source;
  cpkt_interrupt_registration *interrupts;
  cpkt_opcua_InterruptManagerPlugin plugin;
};
struct cpkt_connection_registration {
  cpkt_opcua_ConnectionManager *manager;
  cpkt_opcua_ConnectionManager_connectionCallback callback;
  UA_ConnectionManager_connectionCallback native_callback;
  void *application;
  void *initial_context;
  cpkt_connection_slot *reserved;
  unsigned char failure_tag;
  unsigned int references;
};
struct cpkt_connection_slot {
  cpkt_connection_slot *next;
  cpkt_connection_registration *registration;
  uintptr_t id;
  void *context;
  unsigned int references;
  int closed;
};
struct cpkt_interrupt_registration {
  cpkt_interrupt_registration *next;
  cpkt_opcua_InterruptManager *manager;
  cpkt_opcua_InterruptCallback callback;
  UA_InterruptCallback native_callback;
  void *context;
  uintptr_t id;
  unsigned int references;
};
typedef char
    cpkt_connection_id_width[(sizeof(uintptr_t) == sizeof(size_t)) ? 1 : -1];

static void cpkt_loop_publish(cpkt_opcua_EventLoop *);
static void cpkt_loop_source_publish(cpkt_opcua_EventSource *);
static cpkt_opcua_EventLoop *cpkt_loop_native_find(UA_EventLoop *);
static cpkt_opcua_EventSource *cpkt_loop_source_native_find(UA_EventSource *);
static cpkt_opcua_EventLoop *cpkt_loop_native_adopt(UA_EventLoop *);
static void cpkt_loop_forget_config(UA_EventLoop **slot) {
  cpkt_opcua_EventLoop *loop = cpkt_loop_native_find(*slot);
  if (loop && loop->configuration_slot == slot)
    loop->configuration_slot = NULL;
}
static int cpkt_loop_server_active(cpkt_opcua_server *server) {
  cpkt_opcua_EventLoop *loop =
      cpkt_loop_native_find(UA_Server_getConfig(server->server)->eventLoop);
  cpkt_opcua_EventSource *source;
  if (!loop)
    return 0;
  if (loop->depth || loop->freeing)
    return 1;
  for (source = loop->sources; source; source = source->next)
    if (source->depth)
      return 1;
  return 0;
}
static UA_UInt64 cpkt_loop_u64_native(const cpkt_opcua_UInt64 *value) {
  UA_UInt64 native;
  (void)cpkt_convert(value, &native, &cpkt_types[CPKT_OPCUA_TYPES_UINT64], 1,
                     0);
  return native;
}
static UA_Int64 cpkt_loop_i64_native(const cpkt_opcua_Int64 *value) {
  UA_Int64 native;
  (void)cpkt_convert(value, &native, &cpkt_types[CPKT_OPCUA_TYPES_INT64], 1, 0);
  return native;
}
static void cpkt_loop_u64_public(UA_UInt64 value, cpkt_opcua_UInt64 *output) {
  (void)cpkt_convert(&value, output, &cpkt_types[CPKT_OPCUA_TYPES_UINT64], 0,
                     0);
}
static UA_StatusCode
cpkt_loop_map_to_native(const cpkt_opcua_KeyValueMap *source,
                        UA_KeyValueMap *destination) {
  UA_StatusCode status;
  memset(destination, 0, sizeof(*destination));
  if (!source)
    return 0;
  status = cpkt_array(source->map, source->mapSize, (void **)&destination->map,
                      &cpkt_types[CPKT_OPCUA_TYPES_KEYVALUEPAIR], 1, 0);
  /* Even a failed nested conversion can own a partially built full array. */
  destination->mapSize = destination->map ? source->mapSize : 0;
  return status;
}
static UA_StatusCode
cpkt_loop_map_from_native(const UA_KeyValueMap *source,
                          cpkt_opcua_KeyValueMap *destination) {
  UA_StatusCode status;
  memset(destination, 0, sizeof(*destination));
  if (!source)
    return 0;
  status = cpkt_array(source->map, source->mapSize, (void **)&destination->map,
                      &cpkt_types[CPKT_OPCUA_TYPES_KEYVALUEPAIR], 0, 0);
  destination->mapSize = destination->map ? source->mapSize : 0;
  return status;
}
static UA_StatusCode cpkt_loop_map_set(UA_KeyValueMap *destination,
                                       const cpkt_opcua_KeyValueMap *source) {
  UA_KeyValueMap staged;
  UA_StatusCode status = cpkt_loop_map_to_native(source, &staged);
  if (!status) {
    UA_KeyValueMap_clear(destination);
    *destination = staged;
  } else {
    UA_KeyValueMap_clear(&staged);
  }
  return status;
}
static UA_StatusCode cpkt_loop_map_get(const UA_KeyValueMap *source,
                                       cpkt_opcua_KeyValueMap *destination) {
  UA_StatusCode status;
  if (!destination)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  status = cpkt_loop_map_from_native(source, destination);
  if (status)
    cpkt_opcua_KeyValueMap_clear(destination);
  return status;
}
static cpkt_opcua_EventSource *cpkt_loop_source_find(cpkt_opcua_EventLoop *loop,
                                                     UA_EventSource *native) {
  cpkt_opcua_EventSource *source = cpkt_loop_source_native_find(native);
  return source && source->loop == loop ? source : NULL;
}
static void cpkt_loop_source_detach(cpkt_opcua_EventSource *source) {
  cpkt_opcua_EventSource **slot;
  if (!source->loop)
    return;
  slot = &source->loop->sources;
  while (*slot && *slot != source)
    slot = &(*slot)->next;
  if (*slot)
    *slot = source->next;
  source->next = NULL;
  source->loop = NULL;
}
static void
cpkt_connection_registration_release(cpkt_connection_registration *entry) {
  if (!--entry->references) {
    UA_free(entry->reserved);
    UA_free(entry);
  }
}
static void cpkt_connection_slot_release(cpkt_connection_slot *slot) {
  if (!--slot->references) {
    cpkt_connection_registration_release(slot->registration);
    UA_free(slot);
  }
}
static void cpkt_connection_slot_remove(cpkt_opcua_ConnectionManager *manager,
                                        cpkt_connection_slot *entry) {
  cpkt_connection_slot **slot = &manager->connections;
  if (entry->closed)
    return;
  while (*slot && *slot != entry)
    slot = &(*slot)->next;
  if (*slot) {
    *slot = entry->next;
    entry->closed = 1;
    cpkt_connection_slot_release(entry);
  }
}
static void cpkt_loop_interrupt_remove(cpkt_opcua_InterruptManager *,
                                       cpkt_interrupt_registration *);
static void cpkt_interrupt_release(cpkt_interrupt_registration *entry) {
  if (!--entry->references)
    UA_free(entry);
}
static void cpkt_loop_source_metadata_clear(cpkt_opcua_EventSource *source) {
  cpkt_connection_slot *connection, *next_connection;
  cpkt_interrupt_registration *interrupt, *next_interrupt;
  cpkt_opcua_ConnectionManager *cm;
  cpkt_opcua_InterruptManager *im;
  /* Called after native free, so the native pointer is already invalid. */
  if (source->kind == UA_EVENTSOURCETYPE_CONNECTIONMANAGER) {
    cm = (cpkt_opcua_ConnectionManager *)source;
    for (connection = cm->connections; connection;
         connection = next_connection) {
      next_connection = connection->next;
      connection->closed = 1;
      cpkt_connection_slot_release(connection);
    }
  } else if (source->kind == UA_EVENTSOURCETYPE_INTERRUPTMANAGER) {
    im = (cpkt_opcua_InterruptManager *)source;
    for (interrupt = im->interrupts; interrupt; interrupt = next_interrupt) {
      next_interrupt = interrupt->next;
      cpkt_interrupt_release(interrupt);
    }
  }
  UA_free(source);
}

cpkt_opcua_EventLoop *
cpkt_opcua_EventLoop_new_POSIX(const cpkt_opcua_log_config *config) {
  cpkt_opcua_EventLoop *loop;
  cpkt_opcua_log_config quiet;
  if (!cpkt_logger_valid(config))
    return NULL;
  loop = (cpkt_opcua_EventLoop *)UA_calloc(1, sizeof(*loop));
  if (!loop)
    return NULL;
  memset(&quiet, 0, sizeof(quiet));
  cpkt_logger_set(&loop->logger, &loop->logger.native,
                  config ? config : &quiet);
  loop->native = UA_EventLoop_new_POSIX(&loop->logger.native);
  if (!loop->native) {
    UA_free(loop);
    return NULL;
  }
  cpkt_loop_publish(loop);
  return loop;
}
cpkt_opcua_StatusCode
cpkt_opcua_EventLoop_setLogger(cpkt_opcua_EventLoop *loop,
                               const cpkt_opcua_log_config *config) {
  cpkt_opcua_log_config quiet;
  if (!loop || !loop->native || !cpkt_logger_valid(config))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memset(&quiet, 0, sizeof(quiet));
  cpkt_logger_set(&loop->logger, &loop->logger.native,
                  config ? config : &quiet);
  loop->native->logger = &loop->logger.native;
  return 0;
}
cpkt_opcua_StatusCode
cpkt_opcua_EventLoop_getParams(cpkt_opcua_EventLoop *loop,
                               cpkt_opcua_KeyValueMap *params) {
  if (!loop || !loop->native)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  return cpkt_loop_map_get(&loop->native->params, params);
}
cpkt_opcua_StatusCode
cpkt_opcua_EventLoop_setParams(cpkt_opcua_EventLoop *loop,
                               const cpkt_opcua_KeyValueMap *params) {
  if (!loop || !loop->native)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  return cpkt_loop_map_set(&loop->native->params, params);
}
cpkt_opcua_StatusCode
cpkt_opcua_EventLoop_getState(cpkt_opcua_EventLoop *loop,
                              cpkt_opcua_EventLoopState *state) {
  if (!loop || !loop->native || !state)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  *state = (cpkt_opcua_EventLoopState)loop->native->state;
  return 0;
}
cpkt_opcua_StatusCode cpkt_opcua_EventLoop_start(cpkt_opcua_EventLoop *loop) {
  if (!loop || !loop->native || !loop->native->start)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  return loop->native->start(loop->native);
}
cpkt_opcua_StatusCode cpkt_opcua_EventLoop_run(cpkt_opcua_EventLoop *loop,
                                               cpkt_opcua_UInt32 timeout) {
  if (!loop || !loop->native || !loop->native->run)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  return loop->native->run(loop->native, timeout);
}
cpkt_opcua_StatusCode cpkt_opcua_EventLoop_free(cpkt_opcua_EventLoop *loop) {
  if (loop && loop->configuration_slot)
    return UA_STATUSCODE_BADINVALIDSTATE;
  if (!loop || !loop->native || !loop->native->free)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  return loop->native->free(loop->native);
}
void cpkt_opcua_EventLoop_stop(cpkt_opcua_EventLoop *loop) {
  if (loop && loop->native && loop->native->stop)
    loop->native->stop(loop->native);
}
void cpkt_opcua_EventLoop_cancel(cpkt_opcua_EventLoop *loop) {
  if (loop && loop->native && loop->native->cancel)
    loop->native->cancel(loop->native);
}
void cpkt_opcua_EventLoop_lock(cpkt_opcua_EventLoop *loop) {
  if (loop && loop->native && loop->native->lock)
    loop->native->lock(loop->native);
}
void cpkt_opcua_EventLoop_unlock(cpkt_opcua_EventLoop *loop) {
  if (loop && loop->native && loop->native->unlock)
    loop->native->unlock(loop->native);
}
cpkt_opcua_DateTime
cpkt_opcua_EventLoop_dateTime_now(cpkt_opcua_EventLoop *loop) {
  UA_DateTime value = 0;
  cpkt_opcua_DateTime result;
  if (loop && loop->native && loop->native->dateTime_now)
    value = loop->native->dateTime_now(loop->native);
  cpkt_loop_u64_public((UA_UInt64)value, &result);
  return result;
}
cpkt_opcua_DateTime
cpkt_opcua_EventLoop_dateTime_nowMonotonic(cpkt_opcua_EventLoop *loop) {
  UA_DateTime value = 0;
  cpkt_opcua_DateTime result;
  if (loop && loop->native && loop->native->dateTime_nowMonotonic)
    value = loop->native->dateTime_nowMonotonic(loop->native);
  cpkt_loop_u64_public((UA_UInt64)value, &result);
  return result;
}
cpkt_opcua_Int64
cpkt_opcua_EventLoop_dateTime_localTimeUtcOffset(cpkt_opcua_EventLoop *loop) {
  UA_Int64 value = 0;
  cpkt_opcua_Int64 result;
  if (loop && loop->native && loop->native->dateTime_localTimeUtcOffset)
    value = loop->native->dateTime_localTimeUtcOffset(loop->native);
  cpkt_loop_u64_public((UA_UInt64)value, &result);
  return result;
}
cpkt_opcua_DateTime cpkt_opcua_EventLoop_nextTimer(cpkt_opcua_EventLoop *loop) {
  UA_DateTime value = 0;
  cpkt_opcua_DateTime result;
  if (loop && loop->native && loop->native->nextTimer)
    value = loop->native->nextTimer(loop->native);
  cpkt_loop_u64_public((UA_UInt64)value, &result);
  return result;
}
cpkt_opcua_StatusCode cpkt_opcua_EventLoop_addTimer(
    cpkt_opcua_EventLoop *loop, cpkt_opcua_Callback callback, void *application,
    void *context, cpkt_opcua_Double interval, cpkt_opcua_DateTime *base,
    cpkt_opcua_TimerPolicy policy, cpkt_opcua_UInt64 *id) {
  UA_DateTime native_base;
  UA_UInt64 native_id = 0;
  UA_StatusCode status;
  if (!loop || !loop->native || !loop->native->addTimer || !callback)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (base)
    native_base = cpkt_loop_i64_native(base);
  ++loop->depth;
  status =
      loop->native->addTimer(loop->native, callback, application, context,
                             interval, base ? &native_base : NULL,
                             (UA_TimerPolicy)policy, id ? &native_id : NULL);
  --loop->depth;
  if (base)
    cpkt_loop_u64_public((UA_UInt64)native_base, base);
  if (id && !status)
    cpkt_loop_u64_public(native_id, id);
  return status;
}
cpkt_opcua_StatusCode cpkt_opcua_EventLoop_modifyTimer(
    cpkt_opcua_EventLoop *loop, cpkt_opcua_UInt64 id,
    cpkt_opcua_Double interval, cpkt_opcua_DateTime *base,
    cpkt_opcua_TimerPolicy policy) {
  UA_DateTime native_base;
  UA_StatusCode status;
  if (!loop || !loop->native || !loop->native->modifyTimer)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (base)
    native_base = cpkt_loop_i64_native(base);
  status = loop->native->modifyTimer(loop->native, cpkt_loop_u64_native(&id),
                                     interval, base ? &native_base : NULL,
                                     (UA_TimerPolicy)policy);
  if (base)
    cpkt_loop_u64_public((UA_UInt64)native_base, base);
  return status;
}
void cpkt_opcua_EventLoop_removeTimer(cpkt_opcua_EventLoop *loop,
                                      cpkt_opcua_UInt64 id) {
  if (loop && loop->native && loop->native->removeTimer)
    loop->native->removeTimer(loop->native, cpkt_loop_u64_native(&id));
}
cpkt_opcua_DelayedCallback *
cpkt_opcua_DelayedCallback_new(cpkt_opcua_Callback callback, void *application,
                               void *context) {
  UA_DelayedCallback *native =
      (UA_DelayedCallback *)UA_calloc(1, sizeof(*native));
  if (!native)
    return NULL;
  native->callback = callback;
  native->application = application;
  native->context = context;
  return (cpkt_opcua_DelayedCallback *)native;
}
void cpkt_opcua_DelayedCallback_delete(cpkt_opcua_DelayedCallback *record) {
  UA_free(record);
}
cpkt_opcua_StatusCode
cpkt_opcua_DelayedCallback_getInfo(const cpkt_opcua_DelayedCallback *record,
                                   cpkt_opcua_DelayedCallbackInfo *info) {
  const UA_DelayedCallback *native = (const UA_DelayedCallback *)record;
  if (!native || !info)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  info->next = (cpkt_opcua_DelayedCallback *)native->next;
  info->callback = native->callback;
  info->application = native->application;
  info->context = native->context;
  return 0;
}
cpkt_opcua_StatusCode
cpkt_opcua_DelayedCallback_setInfo(cpkt_opcua_DelayedCallback *record,
                                   const cpkt_opcua_DelayedCallbackInfo *info) {
  UA_DelayedCallback *native = (UA_DelayedCallback *)record;
  if (!native || !info)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  native->callback = info->callback;
  native->application = info->application;
  native->context = info->context;
  return 0;
}
void cpkt_opcua_EventLoop_addDelayedCallback(
    cpkt_opcua_EventLoop *loop, cpkt_opcua_DelayedCallback *record) {
  if (loop && loop->native && loop->native->addDelayedCallback && record)
    loop->native->addDelayedCallback(loop->native,
                                     (UA_DelayedCallback *)record);
}
void cpkt_opcua_EventLoop_removeDelayedCallback(
    cpkt_opcua_EventLoop *loop, cpkt_opcua_DelayedCallback *record) {
  if (loop && loop->native && loop->native->removeDelayedCallback && record)
    loop->native->removeDelayedCallback(loop->native,
                                        (UA_DelayedCallback *)record);
}
cpkt_opcua_StatusCode
cpkt_opcua_EventLoop_registerEventSource(cpkt_opcua_EventLoop *loop,
                                         cpkt_opcua_EventSource *source) {
  if (!loop || !loop->native || !loop->native->registerEventSource || !source ||
      !source->native || source->loop || loop->freeing)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  return loop->native->registerEventSource(loop->native, source->native);
}
cpkt_opcua_StatusCode
cpkt_opcua_EventLoop_deregisterEventSource(cpkt_opcua_EventLoop *loop,
                                           cpkt_opcua_EventSource *source) {
  if (!loop || !loop->native || !loop->native->deregisterEventSource ||
      !source || !source->native || source->loop != loop || source->depth)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  return loop->native->deregisterEventSource(loop->native, source->native);
}
cpkt_opcua_EventSource *
cpkt_opcua_EventLoop_eventSources(cpkt_opcua_EventLoop *loop) {
  return loop && loop->native
             ? cpkt_loop_source_find(loop, loop->native->eventSources)
             : NULL;
}
cpkt_opcua_StatusCode
cpkt_opcua_EventSource_getInfo(cpkt_opcua_EventSource *source,
                               cpkt_opcua_EventSourceInfo *info) {
  if (!source || !source->native || !info)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memset(info, 0, sizeof(*info));
  info->eventSourceType =
      (cpkt_opcua_EventSourceType)source->native->eventSourceType;
  info->state = (cpkt_opcua_EventSourceState)source->native->state;
  cpkt_string_take(&info->name, source->native->name);
  info->eventLoop = source->loop;
  info->next = source->loop
                   ? cpkt_loop_source_find(source->loop, source->native->next)
                   : NULL;
  return 0;
}
cpkt_opcua_StatusCode
cpkt_opcua_EventSource_setName(cpkt_opcua_EventSource *source,
                               cpkt_opcua_String name) {
  UA_String view, owned;
  UA_StatusCode status;
  if (!source || !source->native || !cpkt_string_valid(&name))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  view = cpkt_string_view(&name);
  UA_String_init(&owned);
  status = UA_String_copy(&view, &owned);
  if (status)
    return status;
  UA_String_clear(&source->native->name);
  source->native->name = owned;
  return 0;
}
cpkt_opcua_StatusCode
cpkt_opcua_EventSource_getParams(cpkt_opcua_EventSource *source,
                                 cpkt_opcua_KeyValueMap *params) {
  if (!source || !source->native)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  return cpkt_loop_map_get(&source->native->params, params);
}
cpkt_opcua_StatusCode
cpkt_opcua_EventSource_setParams(cpkt_opcua_EventSource *source,
                                 const cpkt_opcua_KeyValueMap *params) {
  if (!source || !source->native)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  return cpkt_loop_map_set(&source->native->params, params);
}
cpkt_opcua_StatusCode
cpkt_opcua_EventSource_start(cpkt_opcua_EventSource *source) {
  UA_StatusCode status;
  cpkt_opcua_EventLoop *loop;
  if (!source || !source->native || !source->native->start || !source->loop)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  loop = source->loop;
  ++loop->depth;
  ++source->depth;
  status = source->native->start(source->native);
  --source->depth;
  --loop->depth;
  return status;
}
void cpkt_opcua_EventSource_stop(cpkt_opcua_EventSource *source) {
  cpkt_opcua_EventLoop *loop;
  if (!source || !source->native || !source->native->stop || !source->loop)
    return;
  loop = source->loop;
  ++loop->depth;
  ++source->depth;
  source->native->stop(source->native);
  --source->depth;
  --loop->depth;
}
cpkt_opcua_StatusCode
cpkt_opcua_EventSource_free(cpkt_opcua_EventSource *source) {
  if (!source || !source->native || !source->native->free || source->loop ||
      source->depth)
    return UA_STATUSCODE_BADINVALIDSTATE;
  return source->native->free(source->native);
}
static cpkt_opcua_ConnectionManager *cpkt_loop_connection_factory(
    cpkt_opcua_String name, UA_ConnectionManager *(*factory)(const UA_String)) {
  cpkt_opcua_ConnectionManager *manager;
  UA_ConnectionManager *native;
  if (!cpkt_string_valid(&name))
    return NULL;
  manager = (cpkt_opcua_ConnectionManager *)UA_calloc(1, sizeof(*manager));
  if (!manager)
    return NULL;
  native = factory(cpkt_string_view(&name));
  if (!native) {
    UA_free(manager);
    return NULL;
  }
  manager->source.native = &native->eventSource;
  manager->source.kind = native->eventSource.eventSourceType;
  cpkt_loop_source_publish(&manager->source);
  return manager;
}
cpkt_opcua_ConnectionManager *
cpkt_opcua_ConnectionManager_new_POSIX_TCP(cpkt_opcua_String name) {
  return cpkt_loop_connection_factory(name, UA_ConnectionManager_new_POSIX_TCP);
}
cpkt_opcua_ConnectionManager *
cpkt_opcua_ConnectionManager_new_POSIX_UDP(cpkt_opcua_String name) {
  return cpkt_loop_connection_factory(name, UA_ConnectionManager_new_POSIX_UDP);
}
#if defined(__linux__)
cpkt_opcua_ConnectionManager *
cpkt_opcua_ConnectionManager_new_POSIX_Ethernet(cpkt_opcua_String name) {
  return cpkt_loop_connection_factory(name,
                                      UA_ConnectionManager_new_POSIX_Ethernet);
}
#endif
cpkt_opcua_ConnectionManager *
cpkt_opcua_ConnectionManager_new_MQTT(cpkt_opcua_String name) {
  return cpkt_loop_connection_factory(name, UA_ConnectionManager_new_MQTT);
}
cpkt_opcua_InterruptManager *
cpkt_opcua_InterruptManager_new_POSIX(cpkt_opcua_String name) {
  cpkt_opcua_InterruptManager *manager;
  UA_InterruptManager *native;
  if (!cpkt_string_valid(&name))
    return NULL;
  manager = (cpkt_opcua_InterruptManager *)UA_calloc(1, sizeof(*manager));
  if (!manager)
    return NULL;
  native = UA_InterruptManager_new_POSIX(cpkt_string_view(&name));
  if (!native) {
    UA_free(manager);
    return NULL;
  }
  manager->source.native = &native->eventSource;
  manager->source.kind = native->eventSource.eventSourceType;
  cpkt_loop_source_publish(&manager->source);
  return manager;
}
cpkt_opcua_EventSource *cpkt_opcua_ConnectionManager_eventSource(
    cpkt_opcua_ConnectionManager *manager) {
  return manager ? &manager->source : NULL;
}
cpkt_opcua_EventSource *
cpkt_opcua_InterruptManager_eventSource(cpkt_opcua_InterruptManager *manager) {
  return manager ? &manager->source : NULL;
}
cpkt_opcua_ConnectionManager *
cpkt_opcua_EventSource_connectionManager(cpkt_opcua_EventSource *source) {
  return source && source->kind == UA_EVENTSOURCETYPE_CONNECTIONMANAGER
             ? (cpkt_opcua_ConnectionManager *)source
             : NULL;
}
cpkt_opcua_InterruptManager *
cpkt_opcua_EventSource_interruptManager(cpkt_opcua_EventSource *source) {
  return source && source->kind == UA_EVENTSOURCETYPE_INTERRUPTMANAGER
             ? (cpkt_opcua_InterruptManager *)source
             : NULL;
}
cpkt_opcua_StatusCode
cpkt_opcua_ConnectionManager_protocol(cpkt_opcua_ConnectionManager *manager,
                                      cpkt_opcua_String *protocol) {
  UA_ConnectionManager *native;
  if (!manager || !manager->source.native || !protocol)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  native = (UA_ConnectionManager *)manager->source.native;
  cpkt_string_take(protocol, native->protocol);
  return 0;
}
static UA_StatusCode cpkt_loop_connection_deliver(
    cpkt_connection_registration *registration, uintptr_t id, void **context,
    UA_ConnectionState state, const UA_KeyValueMap *native_params,
    const cpkt_opcua_KeyValueMap *public_params, UA_ByteString message,
    UA_StatusCode status) {
  cpkt_opcua_ConnectionManager *manager = registration->manager;
  UA_ConnectionManager *native = (UA_ConnectionManager *)manager->source.native;
  cpkt_opcua_KeyValueMap c_map;
  UA_KeyValueMap n_map;
  cpkt_opcua_ByteString public_message;
  cpkt_string_take(&public_message, message);
  if (registration->native_callback) {
    memset(&n_map, 0, sizeof(n_map));
    if (!status)
      status = cpkt_loop_map_to_native(public_params, &n_map);
    if (status) {
      if (manager->source.loop)
        UA_LOG_ERROR(manager->source.loop->native->logger,
                     UA_LOGCATEGORY_EVENTLOOP,
                     "C89 connection callback conversion failed: %08lx",
                     (unsigned long)status);
      /* A void native callback has no conversion-status channel. Close instead
       * of inventing successful metadata; final close still ends native users.
       */
      if (state == UA_CONNECTIONSTATE_CLOSING)
        registration->native_callback(native, id, registration->application,
                                      context, state, &UA_KEYVALUEMAP_NULL,
                                      message);
    } else {
      registration->native_callback(native, id, registration->application,
                                    context, state, &n_map, message);
    }
    UA_KeyValueMap_clear(&n_map);
  } else {
    memset(&c_map, 0, sizeof(c_map));
    if (!status)
      status = cpkt_loop_map_from_native(native_params, &c_map);
    registration->callback(manager, (size_t)id, registration->application,
                           context, (cpkt_opcua_ConnectionState)state, status,
                           status ? NULL : &c_map, public_message);
    cpkt_opcua_KeyValueMap_clear(&c_map);
  }
  return status;
}
static void cpkt_loop_connection_dispatch(
    cpkt_connection_registration *registration, uintptr_t id, void **context,
    UA_ConnectionState state, const UA_KeyValueMap *native_params,
    const cpkt_opcua_KeyValueMap *public_params, UA_ByteString message,
    UA_StatusCode input_status) {
  UA_ConnectionManager *native =
      (UA_ConnectionManager *)registration->manager->source.native;
  cpkt_opcua_ConnectionManager *manager = registration->manager;
  cpkt_opcua_EventLoop *loop = manager->source.loop;
  cpkt_connection_slot *entry, *parent;
  UA_StatusCode status;
  void *initial_context = registration->initial_context;
  int rejected = (*context == &registration->failure_tag);
  ++registration->references;
  ++manager->source.depth;
  if (loop)
    ++loop->depth;
  for (entry = manager->connections; entry; entry = entry->next)
    if (entry->id == id && entry->registration == registration)
      break;
  if (!entry && !rejected) {
    /* Accepted TCP children inherit the current user context of their listener,
     * while each native connection keeps its own mutable context slot. */
    for (parent = manager->connections; parent; parent = parent->next)
      if (parent == *context) {
        initial_context = parent->context;
        break;
      }
    entry = registration->reserved;
    registration->reserved = NULL;
    if (!entry)
      entry = (cpkt_connection_slot *)UA_calloc(1, sizeof(*entry));
    if (entry) {
      entry->registration = registration;
      ++registration->references;
      entry->id = id;
      entry->context = initial_context;
      entry->references = 1;
      entry->next = manager->connections;
      manager->connections = entry;
      *context = entry;
    }
  }
  if (!entry) {
    /* No hidden loss of an asynchronous registration. Report the boundary
     * failure and close the announced connection; CLOSING never retries close.
     */
    if (!rejected && state != UA_CONNECTIONSTATE_CLOSING) {
      *context = &registration->failure_tag;
      ++registration->references; /* Retain through asynchronous final close. */
    }
    (void)cpkt_loop_connection_deliver(registration, id, &initial_context,
                                       state, native_params, public_params,
                                       message, UA_STATUSCODE_BADOUTOFMEMORY);
    if (!rejected && state != UA_CONNECTIONSTATE_CLOSING)
      (void)native->closeConnection(native, id);
    else if (rejected && state == UA_CONNECTIONSTATE_CLOSING)
      cpkt_connection_registration_release(registration);
  } else {
    ++entry->references;
    status = cpkt_loop_connection_deliver(registration, id, &entry->context,
                                          state, native_params, public_params,
                                          message, input_status);
    if (status && registration->native_callback &&
        state != UA_CONNECTIONSTATE_CLOSING)
      (void)native->closeConnection(native, id);
    if (state == UA_CONNECTIONSTATE_CLOSING)
      cpkt_connection_slot_remove(manager, entry);
    cpkt_connection_slot_release(entry);
  }
  if (loop)
    --loop->depth;
  --manager->source.depth;
  cpkt_connection_registration_release(registration);
}
static void cpkt_loop_connection_invoke(UA_ConnectionManager *native,
                                        uintptr_t id, void *application,
                                        void **context,
                                        UA_ConnectionState state,
                                        const UA_KeyValueMap *params,
                                        UA_ByteString message) {
  (void)native;
  cpkt_loop_connection_dispatch((cpkt_connection_registration *)application, id,
                                context, state, params, NULL, message, 0);
}
static void cpkt_loop_custom_connection_invoke(
    cpkt_opcua_ConnectionManager *manager, size_t id, void *application,
    void **context, cpkt_opcua_ConnectionState state,
    cpkt_opcua_StatusCode status, const cpkt_opcua_KeyValueMap *params,
    cpkt_opcua_ByteString message) {
  (void)manager;
  cpkt_loop_connection_dispatch((cpkt_connection_registration *)application,
                                (uintptr_t)id, context,
                                (UA_ConnectionState)state, NULL, params,
                                cpkt_string_view(&message), status);
}
cpkt_opcua_StatusCode cpkt_opcua_ConnectionManager_openConnection(
    cpkt_opcua_ConnectionManager *manager, const cpkt_opcua_KeyValueMap *params,
    void *application, void *context,
    cpkt_opcua_ConnectionManager_connectionCallback callback) {
  UA_ConnectionManager *native;
  UA_KeyValueMap native_params;
  UA_StatusCode status;
  cpkt_connection_registration *entry;
  cpkt_opcua_EventLoop *loop;
  if (!manager || !manager->source.native || !manager->source.loop || !callback)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  native = (UA_ConnectionManager *)manager->source.native;
  if (!native->openConnection)
    return UA_STATUSCODE_BADNOTSUPPORTED;
  status = cpkt_loop_map_to_native(params, &native_params);
  if (status) {
    UA_KeyValueMap_clear(&native_params);
    return status;
  }
  entry = (cpkt_connection_registration *)UA_calloc(1, sizeof(*entry));
  if (!entry) {
    UA_KeyValueMap_clear(&native_params);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  entry->reserved =
      (cpkt_connection_slot *)UA_calloc(1, sizeof(*entry->reserved));
  if (!entry->reserved) {
    UA_free(entry);
    UA_KeyValueMap_clear(&native_params);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  entry->manager = manager;
  entry->callback = callback;
  entry->application = application;
  entry->initial_context = context;
  entry->references = 1; /* Native promises initial announcement inside open. */
  loop = manager->source.loop;
  ++loop->depth;
  ++manager->source.depth;
  status = native->openConnection(native, &native_params, entry, entry,
                                  cpkt_loop_connection_invoke);
  --manager->source.depth;
  --loop->depth;
  UA_KeyValueMap_clear(&native_params);
  cpkt_connection_registration_release(entry);
  return status;
}
cpkt_opcua_StatusCode cpkt_opcua_ConnectionManager_closeConnection(
    cpkt_opcua_ConnectionManager *manager, size_t id) {
  UA_ConnectionManager *native;
  UA_StatusCode status;
  cpkt_opcua_EventLoop *loop;
  if (!manager || !manager->source.native || !manager->source.loop)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  native = (UA_ConnectionManager *)manager->source.native;
  if (!native->closeConnection)
    return UA_STATUSCODE_BADNOTSUPPORTED;
  loop = manager->source.loop;
  ++loop->depth;
  ++manager->source.depth;
  status = native->closeConnection(native, (uintptr_t)id);
  --manager->source.depth;
  --loop->depth;
  return status;
}
cpkt_opcua_StatusCode cpkt_opcua_ConnectionManager_allocNetworkBuffer(
    cpkt_opcua_ConnectionManager *manager, size_t id,
    cpkt_opcua_ByteString *buffer, size_t size) {
  UA_ConnectionManager *native;
  UA_ByteString native_buffer;
  UA_StatusCode status;
  if (!manager || !manager->source.native || !manager->source.loop || !buffer)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  native = (UA_ConnectionManager *)manager->source.native;
  if (!native->allocNetworkBuffer)
    return UA_STATUSCODE_BADNOTSUPPORTED;
  UA_ByteString_init(&native_buffer);
  status =
      native->allocNetworkBuffer(native, (uintptr_t)id, &native_buffer, size);
  cpkt_string_take(buffer, native_buffer);
  return status;
}
void cpkt_opcua_ConnectionManager_freeNetworkBuffer(
    cpkt_opcua_ConnectionManager *manager, size_t id,
    cpkt_opcua_ByteString *buffer) {
  UA_ConnectionManager *native;
  UA_ByteString native_buffer;
  if (!manager || !manager->source.native || !manager->source.loop || !buffer)
    return;
  native = (UA_ConnectionManager *)manager->source.native;
  if (!native->freeNetworkBuffer)
    return;
  native_buffer = cpkt_string_view(buffer);
  native->freeNetworkBuffer(native, (uintptr_t)id, &native_buffer);
  cpkt_string_take(buffer, native_buffer);
}
cpkt_opcua_StatusCode cpkt_opcua_ConnectionManager_sendWithConnection(
    cpkt_opcua_ConnectionManager *manager, size_t id,
    const cpkt_opcua_KeyValueMap *params, cpkt_opcua_ByteString *buffer) {
  UA_ConnectionManager *native;
  UA_KeyValueMap native_params;
  UA_ByteString native_buffer;
  UA_StatusCode status;
  cpkt_opcua_EventLoop *loop;
  if (!manager || !manager->source.native || !manager->source.loop || !buffer ||
      !cpkt_string_valid(buffer))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  native = (UA_ConnectionManager *)manager->source.native;
  if (!native->sendWithConnection)
    return UA_STATUSCODE_BADNOTSUPPORTED;
  status = cpkt_loop_map_to_native(params, &native_params);
  if (status) {
    UA_KeyValueMap_clear(&native_params);
    return status;
  }
  native_buffer = cpkt_string_view(buffer);
  loop = manager->source.loop;
  ++loop->depth;
  ++manager->source.depth;
  status = native->sendWithConnection(native, (uintptr_t)id, &native_params,
                                      &native_buffer);
  --manager->source.depth;
  --loop->depth;
  UA_KeyValueMap_clear(&native_params);
  cpkt_string_take(buffer, native_buffer);
  return status;
}
static void cpkt_loop_interrupt_invoke(UA_InterruptManager *native,
                                       uintptr_t handle, void *context,
                                       const UA_KeyValueMap *infos) {
  cpkt_interrupt_registration *entry = (cpkt_interrupt_registration *)context;
  cpkt_opcua_InterruptManager *manager = entry->manager;
  cpkt_opcua_EventLoop *loop = manager->source.loop;
  cpkt_opcua_KeyValueMap public_infos;
  UA_StatusCode status;
  (void)native;
  ++entry->references;
  ++manager->source.depth;
  if (loop)
    ++loop->depth;
  status = cpkt_loop_map_from_native(infos, &public_infos);
  entry->callback(manager, (size_t)handle, entry->context, status,
                  status ? NULL : &public_infos);
  cpkt_opcua_KeyValueMap_clear(&public_infos);
  if (loop)
    --loop->depth;
  --manager->source.depth;
  cpkt_interrupt_release(entry);
}
cpkt_opcua_StatusCode cpkt_opcua_InterruptManager_registerInterrupt(
    cpkt_opcua_InterruptManager *manager, size_t handle,
    const cpkt_opcua_KeyValueMap *params, cpkt_opcua_InterruptCallback callback,
    void *context) {
  UA_InterruptManager *native;
  UA_KeyValueMap native_params;
  UA_StatusCode status;
  cpkt_interrupt_registration *entry;
  cpkt_opcua_EventLoop *loop;
  if (!manager || !manager->source.native || !manager->source.loop || !callback)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  native = (UA_InterruptManager *)manager->source.native;
  if (!native->registerInterrupt)
    return UA_STATUSCODE_BADNOTSUPPORTED;
  status = cpkt_loop_map_to_native(params, &native_params);
  if (status) {
    UA_KeyValueMap_clear(&native_params);
    return status;
  }
  entry = (cpkt_interrupt_registration *)UA_calloc(1, sizeof(*entry));
  if (!entry) {
    UA_KeyValueMap_clear(&native_params);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  entry->manager = manager;
  entry->callback = callback;
  entry->context = context;
  entry->id = (uintptr_t)handle;
  entry->references = 2;
  entry->next = manager->interrupts;
  manager->interrupts = entry;
  loop = manager->source.loop;
  ++loop->depth;
  ++manager->source.depth;
  status = native->registerInterrupt(native, (uintptr_t)handle, &native_params,
                                     cpkt_loop_interrupt_invoke, entry);
  --manager->source.depth;
  --loop->depth;
  UA_KeyValueMap_clear(&native_params);
  if (status)
    cpkt_loop_interrupt_remove(manager, entry);
  cpkt_interrupt_release(entry);
  return status;
}
void cpkt_opcua_InterruptManager_deregisterInterrupt(
    cpkt_opcua_InterruptManager *manager, size_t handle) {
  UA_InterruptManager *native;
  cpkt_interrupt_registration **slot, *entry;
  if (!manager || !manager->source.native || !manager->source.loop)
    return;
  native = (UA_InterruptManager *)manager->source.native;
  if (!native->deregisterInterrupt)
    return;
  native->deregisterInterrupt(native, (uintptr_t)handle);
  slot = &manager->interrupts;
  while (*slot && (*slot)->id != (uintptr_t)handle)
    slot = &(*slot)->next;
  if (*slot) {
    entry = *slot;
    *slot = entry->next;
    cpkt_interrupt_release(entry);
  }
}

/* Native callers can register their own public EventSource. Wrap the public
 * root only; the architecture-private backend and its ownership remain native.
 */
static cpkt_opcua_EventSource *
cpkt_loop_source_adopt(cpkt_opcua_EventLoop *loop, UA_EventSource *native) {
  cpkt_opcua_EventSource *source;
  size_t size;
  if (!loop || !native)
    return NULL;
  source = cpkt_loop_source_native_find(native);
  if (!source) {
    if (native->eventSourceType == UA_EVENTSOURCETYPE_CONNECTIONMANAGER)
      size = sizeof(cpkt_opcua_ConnectionManager);
    else if (native->eventSourceType == UA_EVENTSOURCETYPE_INTERRUPTMANAGER)
      size = sizeof(cpkt_opcua_InterruptManager);
    else
      return NULL;
    source = (cpkt_opcua_EventSource *)UA_calloc(1, size);
    if (!source)
      return NULL;
    source->native = native;
    source->kind = native->eventSourceType;
    cpkt_loop_source_publish(source);
  }
  if (source->loop && source->loop != loop)
    return NULL;
  if (!source->loop) {
    source->loop = loop;
    source->next = loop->sources;
    loop->sources = source;
  }
  return source;
}
void *cpkt_opcua_EventLoop_context(cpkt_opcua_EventLoop *loop) {
  return loop ? loop->context : NULL;
}
cpkt_opcua_StatusCode
cpkt_opcua_EventLoop_setState(cpkt_opcua_EventLoop *loop,
                              cpkt_opcua_EventLoopState state) {
  if (!loop || !loop->native || !loop->custom)
    return UA_STATUSCODE_BADINVALIDSTATE;
  /* The native interface declares state const for consumers; implementations
   * write it in their own allocated backing, as the POSIX backend does. */
  *(UA_EventLoopState *)(uintptr_t)&loop->native->state =
      (UA_EventLoopState)state;
  return 0;
}
cpkt_opcua_StatusCode
cpkt_opcua_EventLoop_setEventSources(cpkt_opcua_EventLoop *loop,
                                     cpkt_opcua_EventSource *head) {
  if (!loop || !loop->native || !loop->custom || (head && head->loop != loop))
    return UA_STATUSCODE_BADINVALIDSTATE;
  loop->native->eventSources = head ? head->native : NULL;
  return 0;
}
cpkt_opcua_StatusCode
cpkt_opcua_EventSource_setInfo(cpkt_opcua_EventSource *source,
                               const cpkt_opcua_EventSourceInfo *info) {
  UA_String name, view;
  UA_StatusCode status;
  cpkt_opcua_EventLoop *loop;
  if (!source || !source->native || !info || !cpkt_string_valid(&info->name))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  loop = source->loop ? source->loop : info->eventLoop;
  if ((!source->custom && (!loop || !loop->custom)) ||
      (info->eventLoop && info->eventLoop != loop) ||
      (info->next && info->next->loop != loop) ||
      (UA_EventSourceType)info->eventSourceType != source->kind)
    return UA_STATUSCODE_BADINVALIDSTATE;
  view = cpkt_string_view(&info->name);
  UA_String_init(&name);
  status = UA_String_copy(&view, &name);
  if (status)
    return status;
  UA_String_clear(&source->native->name);
  source->native->name = name;
  source->native->state = (UA_EventSourceState)info->state;
  source->native->eventLoop = info->eventLoop ? info->eventLoop->native : NULL;
  source->native->next = info->next ? info->next->native : NULL;
  return 0;
}

typedef struct {
  UA_ConnectionManager native;
  cpkt_opcua_ConnectionManager *owner;
} cpkt_loop_custom_cm;
typedef struct {
  UA_InterruptManager native;
  cpkt_opcua_InterruptManager *owner;
} cpkt_loop_custom_im;

static cpkt_opcua_EventSource *cpkt_loop_custom_source(UA_EventSource *native) {
  if (native->eventSourceType == UA_EVENTSOURCETYPE_CONNECTIONMANAGER)
    return &((cpkt_loop_custom_cm *)native)->owner->source;
  return &((cpkt_loop_custom_im *)native)->owner->source;
}
static UA_StatusCode cpkt_loop_custom_source_start(UA_EventSource *native) {
  cpkt_opcua_EventSource *source = cpkt_loop_custom_source(native);
  UA_StatusCode status;
  ++source->depth;
  if (source->loop)
    ++source->loop->depth;
  status = source->plugin.start(source);
  if (source->loop)
    --source->loop->depth;
  --source->depth;
  return status;
}
static void cpkt_loop_custom_source_stop(UA_EventSource *native) {
  cpkt_opcua_EventSource *source = cpkt_loop_custom_source(native);
  ++source->depth;
  if (source->loop)
    ++source->loop->depth;
  source->plugin.stop(source);
  if (source->loop)
    --source->loop->depth;
  --source->depth;
}
static UA_StatusCode cpkt_loop_custom_source_free(UA_EventSource *native) {
  cpkt_opcua_EventSource *source = cpkt_loop_custom_source(native);
  UA_StatusCode status;
  ++source->depth;
  status = source->plugin.free(source);
  --source->depth;
  if (!status) {
    if (native->eventSourceType == UA_EVENTSOURCETYPE_CONNECTIONMANAGER)
      UA_String_clear(&((UA_ConnectionManager *)native)->protocol);
    UA_String_clear(&native->name);
    UA_KeyValueMap_clear(&native->params);
    UA_free(native);
  }
  return status;
}
void *cpkt_opcua_EventSource_context(cpkt_opcua_EventSource *source) {
  return source ? source->context : NULL;
}
static UA_StatusCode
cpkt_loop_custom_cm_open(UA_ConnectionManager *native,
                         const UA_KeyValueMap *params, void *application,
                         void *context,
                         UA_ConnectionManager_connectionCallback callback) {
  cpkt_opcua_ConnectionManager *manager =
      ((cpkt_loop_custom_cm *)native)->owner;
  cpkt_connection_registration *entry;
  cpkt_opcua_KeyValueMap public_params;
  UA_StatusCode status;
  if (!callback)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  status = cpkt_loop_map_from_native(params, &public_params);
  if (status) {
    cpkt_opcua_KeyValueMap_clear(&public_params);
    return status;
  }
  entry = (cpkt_connection_registration *)UA_calloc(1, sizeof(*entry));
  if (!entry) {
    cpkt_opcua_KeyValueMap_clear(&public_params);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  entry->reserved =
      (cpkt_connection_slot *)UA_calloc(1, sizeof(*entry->reserved));
  if (!entry->reserved) {
    UA_free(entry);
    cpkt_opcua_KeyValueMap_clear(&public_params);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  entry->manager = manager;
  entry->native_callback = callback;
  entry->application = application;
  entry->initial_context = context;
  entry->references = 1;
  ++manager->source.depth;
  if (manager->source.loop)
    ++manager->source.loop->depth;
  status = manager->plugin.openConnection(manager, &public_params, entry, entry,
                                          cpkt_loop_custom_connection_invoke);
  if (manager->source.loop)
    --manager->source.loop->depth;
  --manager->source.depth;
  cpkt_opcua_KeyValueMap_clear(&public_params);
  cpkt_connection_registration_release(entry);
  return status;
}
static UA_StatusCode cpkt_loop_custom_cm_close(UA_ConnectionManager *native,
                                               uintptr_t id) {
  cpkt_opcua_ConnectionManager *manager =
      ((cpkt_loop_custom_cm *)native)->owner;
  UA_StatusCode status;
  ++manager->source.depth;
  if (manager->source.loop)
    ++manager->source.loop->depth;
  status = manager->plugin.closeConnection(manager, (size_t)id);
  if (manager->source.loop)
    --manager->source.loop->depth;
  --manager->source.depth;
  return status;
}
static UA_StatusCode cpkt_loop_custom_cm_alloc(UA_ConnectionManager *native,
                                               uintptr_t id,
                                               UA_ByteString *buffer,
                                               size_t size) {
  cpkt_opcua_ConnectionManager *manager =
      ((cpkt_loop_custom_cm *)native)->owner;
  cpkt_opcua_ByteString public_buffer;
  UA_StatusCode status;
  memset(&public_buffer, 0, sizeof(public_buffer));
  ++manager->source.depth;
  if (manager->source.loop)
    ++manager->source.loop->depth;
  status = manager->plugin.allocNetworkBuffer(manager, (size_t)id,
                                              &public_buffer, size);
  if (manager->source.loop)
    --manager->source.loop->depth;
  --manager->source.depth;
  *buffer = cpkt_string_view(&public_buffer);
  return status;
}
static void cpkt_loop_custom_cm_buffer_free(UA_ConnectionManager *native,
                                            uintptr_t id,
                                            UA_ByteString *buffer) {
  cpkt_opcua_ConnectionManager *manager =
      ((cpkt_loop_custom_cm *)native)->owner;
  cpkt_opcua_ByteString public_buffer;
  cpkt_string_take(&public_buffer, *buffer);
  ++manager->source.depth;
  if (manager->source.loop)
    ++manager->source.loop->depth;
  manager->plugin.freeNetworkBuffer(manager, (size_t)id, &public_buffer);
  if (manager->source.loop)
    --manager->source.loop->depth;
  --manager->source.depth;
  *buffer = cpkt_string_view(&public_buffer);
}
static UA_StatusCode cpkt_loop_custom_cm_send(UA_ConnectionManager *native,
                                              uintptr_t id,
                                              const UA_KeyValueMap *params,
                                              UA_ByteString *buffer) {
  cpkt_opcua_ConnectionManager *manager =
      ((cpkt_loop_custom_cm *)native)->owner;
  cpkt_opcua_KeyValueMap public_params;
  cpkt_opcua_ByteString public_buffer;
  UA_StatusCode status;
  status = cpkt_loop_map_from_native(params, &public_params);
  if (status) {
    cpkt_opcua_KeyValueMap_clear(&public_params);
    /* Native submission consumes its network buffer even on boundary failure.
     */
    cpkt_loop_custom_cm_buffer_free(native, id, buffer);
    return status;
  }
  cpkt_string_take(&public_buffer, *buffer);
  ++manager->source.depth;
  if (manager->source.loop)
    ++manager->source.loop->depth;
  status = manager->plugin.sendWithConnection(manager, (size_t)id,
                                              &public_params, &public_buffer);
  if (manager->source.loop)
    --manager->source.loop->depth;
  --manager->source.depth;
  *buffer = cpkt_string_view(&public_buffer);
  cpkt_opcua_KeyValueMap_clear(&public_params);
  return status;
}
static void cpkt_loop_interrupt_remove(cpkt_opcua_InterruptManager *manager,
                                       cpkt_interrupt_registration *entry) {
  cpkt_interrupt_registration **slot = &manager->interrupts;
  while (*slot && *slot != entry)
    slot = &(*slot)->next;
  if (*slot) {
    *slot = entry->next;
    cpkt_interrupt_release(entry);
  }
}
static void cpkt_loop_custom_interrupt_invoke(
    cpkt_opcua_InterruptManager *manager, size_t handle, void *context,
    cpkt_opcua_StatusCode status, const cpkt_opcua_KeyValueMap *infos) {
  cpkt_interrupt_registration *entry = (cpkt_interrupt_registration *)context;
  UA_KeyValueMap native_infos;
  cpkt_opcua_EventLoop *loop = manager->source.loop;
  ++entry->references;
  ++manager->source.depth;
  if (loop)
    ++loop->depth;
  memset(&native_infos, 0, sizeof(native_infos));
  if (!status)
    status = cpkt_loop_map_to_native(infos, &native_infos);
  if (status) {
    if (loop)
      UA_LOG_ERROR(loop->native->logger, UA_LOGCATEGORY_EVENTLOOP,
                   "C89 interrupt callback conversion failed: %08lx",
                   (unsigned long)status);
  } else {
    entry->native_callback((UA_InterruptManager *)manager->source.native,
                           (uintptr_t)handle, entry->context, &native_infos);
  }
  UA_KeyValueMap_clear(&native_infos);
  if (loop)
    --loop->depth;
  --manager->source.depth;
  cpkt_interrupt_release(entry);
}
static UA_StatusCode cpkt_loop_custom_im_register(UA_InterruptManager *native,
                                                  uintptr_t handle,
                                                  const UA_KeyValueMap *params,
                                                  UA_InterruptCallback callback,
                                                  void *context) {
  cpkt_opcua_InterruptManager *manager = ((cpkt_loop_custom_im *)native)->owner;
  cpkt_interrupt_registration *entry;
  cpkt_opcua_KeyValueMap public_params;
  UA_StatusCode status;
  if (!callback)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  status = cpkt_loop_map_from_native(params, &public_params);
  if (status) {
    cpkt_opcua_KeyValueMap_clear(&public_params);
    return status;
  }
  entry = (cpkt_interrupt_registration *)UA_calloc(1, sizeof(*entry));
  if (!entry) {
    cpkt_opcua_KeyValueMap_clear(&public_params);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  entry->manager = manager;
  entry->native_callback = callback;
  entry->context = context;
  entry->id = handle;
  entry->references = 2; /* Prospective registration and synchronous frame. */
  entry->next = manager->interrupts;
  manager->interrupts = entry;
  ++manager->source.depth;
  if (manager->source.loop)
    ++manager->source.loop->depth;
  status = manager->plugin.registerInterrupt(
      manager, (size_t)handle, &public_params,
      cpkt_loop_custom_interrupt_invoke, entry);
  if (manager->source.loop)
    --manager->source.loop->depth;
  --manager->source.depth;
  cpkt_opcua_KeyValueMap_clear(&public_params);
  if (status)
    cpkt_loop_interrupt_remove(manager, entry);
  cpkt_interrupt_release(entry);
  return status;
}
static void cpkt_loop_custom_im_deregister(UA_InterruptManager *native,
                                           uintptr_t handle) {
  cpkt_opcua_InterruptManager *manager = ((cpkt_loop_custom_im *)native)->owner;
  cpkt_interrupt_registration *entry;
  ++manager->source.depth;
  if (manager->source.loop)
    ++manager->source.loop->depth;
  manager->plugin.deregisterInterrupt(manager, (size_t)handle);
  if (manager->source.loop)
    --manager->source.loop->depth;
  --manager->source.depth;
  for (entry = manager->interrupts; entry; entry = entry->next)
    if (entry->id == handle && entry->native_callback) {
      cpkt_loop_interrupt_remove(manager, entry);
      break;
    }
}
static UA_StatusCode cpkt_loop_custom_source_init(
    cpkt_opcua_EventSource *source, UA_EventSource *native,
    const cpkt_opcua_EventSourcePlugin *plugin, void *context,
    cpkt_opcua_String name, const cpkt_opcua_KeyValueMap *params,
    UA_EventSourceType kind) {
  UA_StatusCode status;
  UA_String view = cpkt_string_view(&name);
  source->native = native;
  source->kind = kind;
  source->custom = 1;
  source->plugin = *plugin;
  source->context = context;
  native->eventSourceType = kind;
  native->state = UA_EVENTSOURCESTATE_FRESH;
  native->start = cpkt_loop_custom_source_start;
  native->stop = cpkt_loop_custom_source_stop;
  native->free = cpkt_loop_custom_source_free;
  status = UA_String_copy(&view, &native->name);
  if (!status)
    status = cpkt_loop_map_to_native(params, &native->params);
  if (status) {
    UA_String_clear(&native->name);
    UA_KeyValueMap_clear(&native->params);
  }
  return status;
}
cpkt_opcua_ConnectionManager *cpkt_opcua_ConnectionManager_fromPlugin(
    const cpkt_opcua_ConnectionManagerPlugin *plugin, void *context,
    cpkt_opcua_String name, cpkt_opcua_String protocol,
    const cpkt_opcua_KeyValueMap *params) {
  cpkt_opcua_ConnectionManager *manager;
  cpkt_loop_custom_cm *proxy;
  UA_String view;
  UA_StatusCode status;
  if (!plugin || !plugin->eventSource.start || !plugin->eventSource.stop ||
      !plugin->eventSource.free || !plugin->openConnection ||
      !plugin->closeConnection || !plugin->sendWithConnection ||
      !plugin->allocNetworkBuffer || !plugin->freeNetworkBuffer ||
      !cpkt_string_valid(&name) || !cpkt_string_valid(&protocol))
    return NULL;
  manager = (cpkt_opcua_ConnectionManager *)UA_calloc(1, sizeof(*manager));
  proxy = (cpkt_loop_custom_cm *)UA_calloc(1, sizeof(*proxy));
  if (!manager || !proxy) {
    UA_free(manager);
    UA_free(proxy);
    return NULL;
  }
  proxy->owner = manager;
  manager->plugin = *plugin;
  status = cpkt_loop_custom_source_init(
      &manager->source, &proxy->native.eventSource, &plugin->eventSource,
      context, name, params, UA_EVENTSOURCETYPE_CONNECTIONMANAGER);
  if (!status) {
    view = cpkt_string_view(&protocol);
    status = UA_String_copy(&view, &proxy->native.protocol);
    if (status) {
      UA_String_clear(&proxy->native.eventSource.name);
      UA_KeyValueMap_clear(&proxy->native.eventSource.params);
    }
  }
  if (status) {
    UA_free(manager);
    UA_free(proxy);
    return NULL;
  }
  proxy->native.openConnection = cpkt_loop_custom_cm_open;
  proxy->native.closeConnection = cpkt_loop_custom_cm_close;
  proxy->native.sendWithConnection = cpkt_loop_custom_cm_send;
  proxy->native.allocNetworkBuffer = cpkt_loop_custom_cm_alloc;
  proxy->native.freeNetworkBuffer = cpkt_loop_custom_cm_buffer_free;
  cpkt_loop_source_publish(&manager->source);
  return manager;
}
cpkt_opcua_InterruptManager *cpkt_opcua_InterruptManager_fromPlugin(
    const cpkt_opcua_InterruptManagerPlugin *plugin, void *context,
    cpkt_opcua_String name, const cpkt_opcua_KeyValueMap *params) {
  cpkt_opcua_InterruptManager *manager;
  cpkt_loop_custom_im *proxy;
  UA_StatusCode status;
  if (!plugin || !plugin->eventSource.start || !plugin->eventSource.stop ||
      !plugin->eventSource.free || !plugin->registerInterrupt ||
      !plugin->deregisterInterrupt || !cpkt_string_valid(&name))
    return NULL;
  manager = (cpkt_opcua_InterruptManager *)UA_calloc(1, sizeof(*manager));
  proxy = (cpkt_loop_custom_im *)UA_calloc(1, sizeof(*proxy));
  if (!manager || !proxy) {
    UA_free(manager);
    UA_free(proxy);
    return NULL;
  }
  proxy->owner = manager;
  manager->plugin = *plugin;
  status = cpkt_loop_custom_source_init(
      &manager->source, &proxy->native.eventSource, &plugin->eventSource,
      context, name, params, UA_EVENTSOURCETYPE_INTERRUPTMANAGER);
  if (status) {
    UA_free(manager);
    UA_free(proxy);
    return NULL;
  }
  proxy->native.registerInterrupt = cpkt_loop_custom_im_register;
  proxy->native.deregisterInterrupt = cpkt_loop_custom_im_deregister;
  cpkt_loop_source_publish(&manager->source);
  return manager;
}
#include "opcua_eventloop_lifecycle_impl.h"

#include "opcua_eventloop_metadata.inc"
#endif
