/* Private client timer ownership bridge. Native EventLoop is the scheduler. */
/* Timer userdata is retained across reentrant cancellation, but the native
 * EventLoop remains the only scheduler. All handle operations are serialized.
 */
struct cpkt_client_timer {
  struct cpkt_client_timer *next;
  cpkt_opcua_client *owner;
  cpkt_opcua_ClientCallback callback;
  void *data;
  UA_UInt64 id;
  unsigned int references;
  int repeated;
  int removed;
};
static void cpkt_client_timer_unref(struct cpkt_client_timer *timer) {
  struct cpkt_client_timer **slot;
  if (--timer->references)
    return;
  for (slot = &timer->owner->typed_timers; *slot; slot = &(*slot)->next) {
    if (*slot == timer) {
      *slot = timer->next;
      break;
    }
  }
  UA_free(timer);
}
static void cpkt_client_timer_invoke(UA_Client *native, void *context) {
  struct cpkt_client_timer *timer = (struct cpkt_client_timer *)context;
  (void)native;
  ++timer->references;
  timer->callback(timer->owner, timer->data);
  if (!timer->repeated && !timer->removed) {
    timer->removed = 1;
    cpkt_client_timer_unref(timer);
  }
  cpkt_client_timer_unref(timer);
}
static void cpkt_client_callbacks_clear(cpkt_opcua_client *client,
                                        UA_EventLoop *external_loop) {
  struct cpkt_client_timer *timer;
  while ((timer = client->typed_timers) != NULL) {
    client->typed_timers = timer->next;
    /* Native deletion may run due callbacks while disconnecting. Preserve that
     * timing. An external loop survives native deletion; detach only our
     * remaining timers there before releasing their bridge state. */
    if (external_loop)
      external_loop->removeTimer(external_loop, timer->id);
    UA_free(timer);
  }
}
static UA_StatusCode cpkt_client_timer_add(cpkt_opcua_client *client,
                                           cpkt_opcua_ClientCallback callback,
                                           void *data, UA_DateTime date,
                                           UA_Double interval, int repeated,
                                           cpkt_opcua_UInt64 *callback_id) {
  struct cpkt_client_timer *timer;
  UA_StatusCode status;
  if (callback_id)
    memset(callback_id, 0, sizeof(*callback_id));
  if (!client || !client->client || !callback)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  timer = (struct cpkt_client_timer *)UA_calloc(1, sizeof(*timer));
  if (!timer)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  timer->owner = client;
  timer->callback = callback;
  timer->data = data;
  timer->references = 1;
  timer->repeated = repeated;
  if (repeated)
    status = UA_Client_addRepeatedCallback(
        client->client, cpkt_client_timer_invoke, timer, interval, &timer->id);
  else
    status = UA_Client_addTimedCallback(
        client->client, cpkt_client_timer_invoke, timer, date, &timer->id);
  if (status) {
    UA_free(timer);
    return status;
  }
  timer->next = client->typed_timers;
  client->typed_timers = timer;
  client->typed_callbacks_clear = cpkt_client_callbacks_clear;
  if (callback_id)
    cpkt_convert(&timer->id, callback_id, &cpkt_types[CPKT_OPCUA_TYPES_UINT64],
                 0, 0);
  return 0;
}
/** Implements native one-shot scheduling with C89 DateTime conversion. */
cpkt_opcua_StatusCode cpkt_opcua_client_addTimedCallback_typed(
    cpkt_opcua_client *client, cpkt_opcua_ClientCallback callback, void *data,
    cpkt_opcua_DateTime date, cpkt_opcua_UInt64 *callback_id) {
  UA_DateTime native_date;
  cpkt_convert(&date, &native_date, &cpkt_types[CPKT_OPCUA_TYPES_DATETIME], 1,
               0);
  return cpkt_client_timer_add(client, callback, data, native_date, 0, 0,
                               callback_id);
}
/** Implements native repeated scheduling without retaining a facade queue. */
cpkt_opcua_StatusCode cpkt_opcua_client_addRepeatedCallback_typed(
    cpkt_opcua_client *client, cpkt_opcua_ClientCallback callback, void *data,
    cpkt_opcua_Double interval_ms, cpkt_opcua_UInt64 *callback_id) {
  return cpkt_client_timer_add(client, callback, data, 0, interval_ms, 1,
                               callback_id);
}
/** Implements native repeated timer interval changes. */
cpkt_opcua_StatusCode cpkt_opcua_client_changeRepeatedCallbackInterval_typed(
    cpkt_opcua_client *client, cpkt_opcua_UInt64 callback_id,
    cpkt_opcua_Double interval_ms) {
  UA_UInt64 native_id;
  UA_StatusCode status;
  struct cpkt_client_timer *timer;
  if (!client || !client->client)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  cpkt_convert(&callback_id, &native_id, &cpkt_types[CPKT_OPCUA_TYPES_UINT64],
               1, 0);
  status = UA_Client_changeRepeatedCallbackInterval(client->client, native_id,
                                                    interval_ms);
  if (!status) {
    for (timer = client->typed_timers; timer; timer = timer->next) {
      if (timer->id == native_id && !timer->removed) {
        timer->repeated = 1;
        break;
      }
    }
  }
  return status;
}
/** Implements reentrant cancellation while preserving native callback IDs. */
void cpkt_opcua_client_removeCallback_typed(cpkt_opcua_client *client,
                                            cpkt_opcua_UInt64 callback_id) {
  UA_UInt64 native_id;
  struct cpkt_client_timer *timer;
  if (!client || !client->client)
    return;
  cpkt_convert(&callback_id, &native_id, &cpkt_types[CPKT_OPCUA_TYPES_UINT64],
               1, 0);
  UA_Client_removeCallback(client->client, native_id);
  for (timer = client->typed_timers; timer; timer = timer->next) {
    if (timer->id == native_id && !timer->removed) {
      timer->removed = 1;
      cpkt_client_timer_unref(timer);
      break;
    }
  }
}
