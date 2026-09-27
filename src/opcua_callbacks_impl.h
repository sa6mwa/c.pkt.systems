/* Private callback bridge, included by opcua_types.c so conversion helpers and
 * bookkeeping stay private in both static and shared libraries. */
/** Forward native defaults through the shared schema conversion. */
cpkt_opcua_StatusCode cpkt_opcua_CreateSubscriptionRequest_default(
    cpkt_opcua_CreateSubscriptionRequest *request) {
  UA_CreateSubscriptionRequest native;
  if (!request)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memset(request, 0, sizeof(*request));
  native = UA_CreateSubscriptionRequest_default();
  return cpkt_convert(&native, request,
                      &cpkt_types[CPKT_OPCUA_TYPES_CREATESUBSCRIPTIONREQUEST],
                      0, 0);
}
/** Preserve upstream's shallow node-ID assignment in this default factory. */
cpkt_opcua_StatusCode cpkt_opcua_MonitoredItemCreateRequest_default(
    cpkt_opcua_NodeId node_id, cpkt_opcua_MonitoredItemCreateRequest *request) {
  UA_MonitoredItemCreateRequest native;
  UA_StatusCode status;
  if (!request)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memset(request, 0, sizeof(*request));
  native = UA_MonitoredItemCreateRequest_default(UA_NODEID_NULL);
  status = cpkt_convert(
      &native, request,
      &cpkt_types[CPKT_OPCUA_TYPES_MONITOREDITEMCREATEREQUEST], 0, 0);
  if (!status)
    request->itemToMonitor.nodeId = node_id;
  return status;
}
struct cpkt_typed_subscription {
  struct cpkt_typed_subscription *next;
  cpkt_opcua_client *owner;
  void *user;
  cpkt_opcua_subscription_status_fn status_fn;
  cpkt_opcua_subscription_delete_fn delete_fn;
  UA_UInt32 id;
  unsigned int references;
  int deleted;
};
struct cpkt_typed_monitor {
  struct cpkt_typed_monitor *next;
  cpkt_opcua_client *owner;
  cpkt_opcua_MonitoredItemCallbacks callbacks;
  UA_UInt32 sub_id, id;
  unsigned int references;
  int deleted;
};
static struct cpkt_typed_monitor *
cpkt_monitor_new(cpkt_opcua_client *client, UA_UInt32 subscription_id,
                 const cpkt_opcua_MonitoredItemCallbacks *callbacks) {
  struct cpkt_typed_monitor *mon =
      (struct cpkt_typed_monitor *)UA_calloc(1, sizeof(*mon));
  if (!mon)
    return NULL;
  mon->owner = client;
  mon->sub_id = subscription_id;
  mon->callbacks = *callbacks;
  mon->references = 2; /* Native lifetime plus pending creator. */
  mon->next = client->typed_monitors;
  client->typed_monitors = mon;
  return mon;
}
static struct cpkt_typed_subscription *
cpkt_find_subscription(cpkt_opcua_client *client, UA_UInt32 id) {
  struct cpkt_typed_subscription *sub;
  for (sub = client->typed_subscriptions; sub; sub = sub->next)
    if (sub->id == id && !sub->deleted)
      return sub;
  return NULL;
}
static void *cpkt_subscription_user(cpkt_opcua_client *client, UA_UInt32 id,
                                    void *native_context) {
  struct cpkt_typed_subscription *sub = cpkt_find_subscription(client, id);
  return sub && sub == native_context ? sub->user : native_context;
}
static void cpkt_subscription_unref(struct cpkt_typed_subscription *sub) {
  struct cpkt_typed_subscription **slot;
  if (--sub->references)
    return;
  for (slot = &sub->owner->typed_subscriptions; *slot; slot = &(*slot)->next)
    if (*slot == sub) {
      *slot = sub->next;
      break;
    }
  UA_free(sub);
}
static void cpkt_subscription_deleted(UA_Client *client, UA_UInt32 id,
                                      void *context) {
  struct cpkt_typed_subscription *sub =
      (struct cpkt_typed_subscription *)context;
  (void)client;
  sub->deleted = 1;
  if (sub->delete_fn)
    sub->delete_fn(sub->owner, id, sub->user);
  cpkt_subscription_unref(sub);
}
static void cpkt_subscription_status(UA_Client *client, UA_UInt32 id,
                                     void *context,
                                     UA_StatusChangeNotification *native) {
  struct cpkt_typed_subscription *sub =
      (struct cpkt_typed_subscription *)context;
  cpkt_opcua_StatusChangeNotification notification;
  UA_StatusCode status;
  (void)client;
  if (!sub->status_fn)
    return;
  ++sub->references;
  memset(&notification, 0, sizeof(notification));
  status = cpkt_convert(native, &notification,
                        &cpkt_types[CPKT_OPCUA_TYPES_STATUSCHANGENOTIFICATION],
                        0, 0);
  sub->status_fn(sub->owner, id, sub->user, status,
                 status ? NULL : &notification);
  cpkt_opcua_StatusChangeNotification_clear(&notification);
  cpkt_subscription_unref(sub);
}
static struct cpkt_typed_subscription *
cpkt_subscription_new(cpkt_opcua_client *client, void *user,
                      cpkt_opcua_subscription_status_fn status_fn,
                      cpkt_opcua_subscription_delete_fn delete_fn) {
  struct cpkt_typed_subscription *sub =
      (struct cpkt_typed_subscription *)UA_calloc(1, sizeof(*sub));
  if (!sub)
    return NULL;
  sub->owner = client;
  sub->user = user;
  sub->status_fn = status_fn;
  sub->delete_fn = delete_fn;
  sub->references = 2; /* Native lifetime plus pending creator/completion. */
  sub->next = client->typed_subscriptions;
  client->typed_subscriptions = sub;
  return sub;
}
static void
cpkt_subscription_finish(struct cpkt_typed_subscription *sub,
                         const UA_CreateSubscriptionResponse *response) {
  if (response && !response->responseHeader.serviceResult)
    sub->id = response->subscriptionId;
  else if (!sub->deleted) {
    sub->deleted = 1;
    cpkt_subscription_unref(sub);
  }
  cpkt_subscription_unref(sub);
}
/** Implements the full generated subscription model and native callbacks. */
cpkt_opcua_StatusCode cpkt_opcua_client_subscription_create_typed(
    cpkt_opcua_client *client,
    const cpkt_opcua_CreateSubscriptionRequest *request, void *context,
    cpkt_opcua_subscription_status_fn status_fn,
    cpkt_opcua_subscription_delete_fn delete_fn,
    cpkt_opcua_CreateSubscriptionResponse *response) {
  UA_CreateSubscriptionRequest native_request;
  UA_CreateSubscriptionResponse native_response;
  struct cpkt_typed_subscription *sub;
  UA_StatusCode status;
  if (!client || !request || !response)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memset(response, 0, sizeof(*response));
  UA_CreateSubscriptionRequest_init(&native_request);
  status = cpkt_convert(request, &native_request,
                        &cpkt_types[CPKT_OPCUA_TYPES_CREATESUBSCRIPTIONREQUEST],
                        1, 0);
  if (status) {
    UA_CreateSubscriptionRequest_clear(&native_request);
    return status;
  }
  sub = cpkt_subscription_new(client, context, status_fn, delete_fn);
  if (!sub) {
    UA_CreateSubscriptionRequest_clear(&native_request);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  native_response = UA_Client_Subscriptions_create(
      client->client, native_request, sub, cpkt_subscription_status,
      cpkt_subscription_deleted);
  status = cpkt_convert(
      &native_response, response,
      &cpkt_types[CPKT_OPCUA_TYPES_CREATESUBSCRIPTIONRESPONSE], 0, 0);
  /* If conversion fails, a successfully created native subscription still
   * exists. Return its scalar ID and upstream status so it can be removed. */
  if (status) {
    cpkt_opcua_CreateSubscriptionResponse_clear(response);
    response->subscriptionId = native_response.subscriptionId;
    response->responseHeader.serviceResult =
        native_response.responseHeader.serviceResult;
  }
  cpkt_subscription_finish(sub, &native_response);
  UA_CreateSubscriptionRequest_clear(&native_request);
  UA_CreateSubscriptionResponse_clear(&native_response);
  return status;
}
typedef struct {
  cpkt_async_base base;
  struct cpkt_typed_subscription *sub;
  cpkt_opcua_subscription_created_fn fn;
  UA_UInt32 subscription_id;
} cpkt_subscription_async;
static void cpkt_subscription_async_deliver(cpkt_async_base *base, UA_UInt32 id,
                                            UA_StatusCode status,
                                            const void *response) {
  cpkt_subscription_async *context = (cpkt_subscription_async *)base;
  context->fn(base->client, base->user, id, status, context->subscription_id,
              (const cpkt_opcua_CreateSubscriptionResponse *)response);
}
static void
cpkt_subscription_async_native(UA_Client *client, void *user, UA_UInt32 id,
                               UA_CreateSubscriptionResponse *response) {
  cpkt_subscription_async *context = (cpkt_subscription_async *)user;
  struct cpkt_typed_subscription *sub = context->sub;
  if (!response->responseHeader.serviceResult) {
    sub->id = response->subscriptionId;
    context->subscription_id = response->subscriptionId;
  }
  /* Keep creator ownership while user completion runs (it may delete the sub).
   */
  cpkt_async_response(client, context, id, response);
  cpkt_subscription_finish(sub, response);
}
static UA_StatusCode cpkt_subscription_async_invoke(UA_Client *client,
                                                    const void *request,
                                                    cpkt_async_base *base,
                                                    UA_UInt32 *id) {
  cpkt_subscription_async *context = (cpkt_subscription_async *)base;
  return UA_Client_Subscriptions_create_async(
      client, *(const UA_CreateSubscriptionRequest *)request, context->sub,
      cpkt_subscription_status, cpkt_subscription_deleted,
      cpkt_subscription_async_native, context, id);
}
/** Implements upstream's specialized async subscription creation. */
cpkt_opcua_StatusCode cpkt_opcua_client_subscription_create_typed_async(
    cpkt_opcua_client *client,
    const cpkt_opcua_CreateSubscriptionRequest *request, void *context,
    cpkt_opcua_subscription_status_fn status_fn,
    cpkt_opcua_subscription_delete_fn delete_fn,
    cpkt_opcua_subscription_created_fn complete_fn, void *user,
    cpkt_opcua_UInt32 *request_id) {
  cpkt_subscription_async *call;
  struct cpkt_typed_subscription *sub;
  UA_StatusCode status;
  if (request_id)
    *request_id = 0;
  if (!client || !request || !complete_fn)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  call = (cpkt_subscription_async *)UA_calloc(1, sizeof(*call));
  if (!call)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  sub = cpkt_subscription_new(client, context, status_fn, delete_fn);
  if (!sub) {
    UA_free(call);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  call->sub = sub;
  call->fn = complete_fn;
  call->base.client = client;
  call->base.user = user;
  call->base.type = &cpkt_types[CPKT_OPCUA_TYPES_CREATESUBSCRIPTIONRESPONSE];
  call->base.deliver = cpkt_subscription_async_deliver;
  call->base.invoke = cpkt_subscription_async_invoke;
  status = cpkt_typed_async(
      request, &cpkt_types[CPKT_OPCUA_TYPES_CREATESUBSCRIPTIONREQUEST],
      &call->base, request_id);
  if (status)
    cpkt_subscription_finish(sub, NULL);
  return status;
}
static struct cpkt_typed_monitor *
cpkt_find_monitor(cpkt_opcua_client *client, UA_UInt32 sub_id, UA_UInt32 id) {
  struct cpkt_typed_monitor *mon;
  for (mon = client->typed_monitors; mon; mon = mon->next)
    if (mon->sub_id == sub_id && mon->id == id && !mon->deleted)
      return mon;
  return NULL;
}
static void cpkt_monitor_unref(struct cpkt_typed_monitor *mon) {
  struct cpkt_typed_monitor **slot;
  if (--mon->references)
    return;
  for (slot = &mon->owner->typed_monitors; *slot; slot = &(*slot)->next)
    if (*slot == mon) {
      *slot = mon->next;
      break;
    }
  UA_free(mon);
}
static void cpkt_monitor_deleted(UA_Client *client, UA_UInt32 sub_id,
                                 void *sub_context, UA_UInt32 id,
                                 void *context) {
  struct cpkt_typed_monitor *mon = (struct cpkt_typed_monitor *)context;
  (void)client;
  mon->deleted = 1;
  if (mon->callbacks.deleted)
    mon->callbacks.deleted(
        mon->owner, sub_id,
        cpkt_subscription_user(mon->owner, sub_id, sub_context), id,
        mon->callbacks.context);
  cpkt_monitor_unref(mon);
}
static void cpkt_monitor_changed(UA_Client *client, UA_UInt32 sub_id,
                                 void *sub_context, UA_UInt32 id, void *context,
                                 UA_DataValue *native) {
  struct cpkt_typed_monitor *mon = (struct cpkt_typed_monitor *)context;
  cpkt_opcua_DataValue value;
  UA_StatusCode status;
  (void)client;
  if (!mon->callbacks.data_change)
    return;
  ++mon->references;
  memset(&value, 0, sizeof(value));
  status = cpkt_convert(native, &value, &cpkt_types[CPKT_OPCUA_TYPES_DATAVALUE],
                        0, 0);
  mon->callbacks.data_change(
      mon->owner, sub_id,
      cpkt_subscription_user(mon->owner, sub_id, sub_context), id,
      mon->callbacks.context, status, status ? NULL : &value);
  cpkt_opcua_DataValue_clear(&value);
  cpkt_monitor_unref(mon);
}
static void cpkt_monitor_event(UA_Client *client, UA_UInt32 sub_id,
                               void *sub_context, UA_UInt32 id, void *context,
                               const UA_KeyValueMap native) {
  struct cpkt_typed_monitor *mon = (struct cpkt_typed_monitor *)context;
  cpkt_opcua_KeyValueMap fields;
  UA_StatusCode status;
  void *data = NULL;
  (void)client;
  if (!mon->callbacks.event)
    return;
  ++mon->references;
  status = cpkt_array(native.map, native.mapSize, &data,
                      &cpkt_types[CPKT_OPCUA_TYPES_KEYVALUEPAIR], 0, 0);
  fields.map = (cpkt_opcua_KeyValuePair *)data;
  fields.mapSize = data ? native.mapSize : 0;
  mon->callbacks.event(mon->owner, sub_id,
                       cpkt_subscription_user(mon->owner, sub_id, sub_context),
                       id, mon->callbacks.context, status,
                       status ? NULL : &fields);
  cpkt_clear_array(data, fields.mapSize,
                   &cpkt_types[CPKT_OPCUA_TYPES_KEYVALUEPAIR]);
  cpkt_monitor_unref(mon);
}
typedef struct {
  size_t count;
  struct cpkt_typed_monitor **monitors;
  void **contexts;
  UA_Client_DataChangeNotificationCallback *data_callbacks;
  UA_Client_EventNotificationCallback *event_callbacks;
  UA_Client_DeleteMonitoredItemCallback *delete_callbacks;
  int events;
} cpkt_monitor_batch;
static void
cpkt_monitor_batch_finish(cpkt_monitor_batch *batch,
                          const UA_CreateMonitoredItemsResponse *response) {
  size_t i;
  for (i = 0; i < batch->count; ++i) {
    struct cpkt_typed_monitor *mon = batch->monitors[i];
    if (!mon)
      continue;
    if (response && !response->responseHeader.serviceResult &&
        i < response->resultsSize && !response->results[i].statusCode)
      mon->id = response->results[i].monitoredItemId;
    else if (!mon->deleted) {
      mon->deleted = 1;
      cpkt_monitor_unref(mon);
    }
    cpkt_monitor_unref(mon);
  }
  UA_free(batch->monitors);
  UA_free(batch->contexts);
  UA_free(batch->data_callbacks);
  UA_free(batch->event_callbacks);
  UA_free(batch->delete_callbacks);
  memset(batch, 0, sizeof(*batch));
}
static UA_StatusCode
cpkt_monitor_batch_new(cpkt_monitor_batch *batch, cpkt_opcua_client *client,
                       const cpkt_opcua_CreateMonitoredItemsRequest *request,
                       int events,
                       const cpkt_opcua_MonitoredItemCallbacks *registrations) {
  size_t i, count = request->itemsToCreateSize;
  memset(batch, 0, sizeof(*batch));
  batch->events = events;
  if (count > (size_t)-1 / sizeof(*batch->monitors) ||
      count > (size_t)-1 / sizeof(*batch->delete_callbacks) ||
      count > (size_t)-1 / sizeof(*batch->data_callbacks) ||
      count > (size_t)-1 / sizeof(*batch->event_callbacks))
    return UA_STATUSCODE_BADOUTOFMEMORY;
  if (!count)
    return 0;
  batch->monitors =
      (struct cpkt_typed_monitor **)UA_calloc(count, sizeof(*batch->monitors));
  batch->contexts = (void **)UA_calloc(count, sizeof(*batch->contexts));
  batch->delete_callbacks = (UA_Client_DeleteMonitoredItemCallback *)UA_calloc(
      count, sizeof(*batch->delete_callbacks));
  if (events)
    batch->event_callbacks = (UA_Client_EventNotificationCallback *)UA_calloc(
        count, sizeof(*batch->event_callbacks));
  else
    batch->data_callbacks =
        (UA_Client_DataChangeNotificationCallback *)UA_calloc(
            count, sizeof(*batch->data_callbacks));
  if (!batch->monitors || !batch->contexts || !batch->delete_callbacks ||
      (events ? !batch->event_callbacks : !batch->data_callbacks)) {
    cpkt_monitor_batch_finish(batch, NULL);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  for (i = 0; i < count; ++i) {
    struct cpkt_typed_monitor *mon =
        cpkt_monitor_new(client, request->subscriptionId, &registrations[i]);
    if (!mon) {
      cpkt_monitor_batch_finish(batch, NULL);
      return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    batch->monitors[i] = mon;
    batch->count = i + 1;
    batch->contexts[i] = mon;
    batch->delete_callbacks[i] = cpkt_monitor_deleted;
    if (events)
      batch->event_callbacks[i] = cpkt_monitor_event;
    else
      batch->data_callbacks[i] = cpkt_monitor_changed;
  }
  return 0;
}
static UA_StatusCode
cpkt_monitor_single_create(cpkt_opcua_client *client, UA_UInt32 subscription_id,
                           cpkt_opcua_TimestampsToReturn timestamps,
                           const cpkt_opcua_MonitoredItemCreateRequest *item,
                           int events,
                           const cpkt_opcua_MonitoredItemCallbacks *callbacks,
                           cpkt_opcua_MonitoredItemCreateResult *result) {
  UA_MonitoredItemCreateRequest native_item;
  UA_MonitoredItemCreateResult native_result;
  struct cpkt_typed_monitor *mon;
  UA_StatusCode status;
  if (!client || !client->client || !item || !result)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memset(result, 0, sizeof(*result));
  UA_MonitoredItemCreateRequest_init(&native_item);
  status = cpkt_convert(
      item, &native_item,
      &cpkt_types[CPKT_OPCUA_TYPES_MONITOREDITEMCREATEREQUEST], 1, 0);
  if (status) {
    UA_MonitoredItemCreateRequest_clear(&native_item);
    return status;
  }
  mon = cpkt_monitor_new(client, subscription_id, callbacks);
  if (!mon) {
    UA_MonitoredItemCreateRequest_clear(&native_item);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  if (events)
    native_result = UA_Client_MonitoredItems_createEvent(
        client->client, subscription_id, (UA_TimestampsToReturn)timestamps,
        native_item, mon, cpkt_monitor_event, cpkt_monitor_deleted);
  else
    native_result = UA_Client_MonitoredItems_createDataChange(
        client->client, subscription_id, (UA_TimestampsToReturn)timestamps,
        native_item, mon, cpkt_monitor_changed, cpkt_monitor_deleted);
  if (!native_result.statusCode)
    mon->id = native_result.monitoredItemId;
  else if (!mon->deleted) {
    mon->deleted = 1;
    cpkt_monitor_unref(mon);
  }
  status = cpkt_convert(&native_result, result,
                        &cpkt_types[CPKT_OPCUA_TYPES_MONITOREDITEMCREATERESULT],
                        0, 0);
  if (status)
    cpkt_opcua_MonitoredItemCreateResult_clear(result);
  cpkt_monitor_unref(mon);
  UA_MonitoredItemCreateRequest_clear(&native_item);
  UA_MonitoredItemCreateResult_clear(&native_result);
  return status;
}
/** Bind the exact native single-item data-change helper and its callbacks. */
cpkt_opcua_StatusCode cpkt_opcua_client_MonitoredItems_createDataChange_typed(
    cpkt_opcua_client *client, cpkt_opcua_UInt32 subscription_id,
    cpkt_opcua_TimestampsToReturn timestamps,
    const cpkt_opcua_MonitoredItemCreateRequest *item, void *context,
    cpkt_opcua_data_change_typed_fn callback,
    cpkt_opcua_monitored_delete_fn delete_callback,
    cpkt_opcua_MonitoredItemCreateResult *result) {
  cpkt_opcua_MonitoredItemCallbacks callbacks;
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.context = context;
  callbacks.data_change = callback;
  callbacks.deleted = delete_callback;
  return cpkt_monitor_single_create(client, subscription_id, timestamps, item,
                                    0, &callbacks, result);
}
/** Bind the exact native single-item event helper and its callbacks. */
cpkt_opcua_StatusCode cpkt_opcua_client_MonitoredItems_createEvent_typed(
    cpkt_opcua_client *client, cpkt_opcua_UInt32 subscription_id,
    cpkt_opcua_TimestampsToReturn timestamps,
    const cpkt_opcua_MonitoredItemCreateRequest *item, void *context,
    cpkt_opcua_event_typed_fn callback,
    cpkt_opcua_monitored_delete_fn delete_callback,
    cpkt_opcua_MonitoredItemCreateResult *result) {
  cpkt_opcua_MonitoredItemCallbacks callbacks;
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.context = context;
  callbacks.event = callback;
  callbacks.deleted = delete_callback;
  return cpkt_monitor_single_create(client, subscription_id, timestamps, item,
                                    1, &callbacks, result);
}
/** Implements complete native monitored-item creation and notifications. */
cpkt_opcua_StatusCode cpkt_opcua_client_monitored_items_create_typed(
    cpkt_opcua_client *client,
    const cpkt_opcua_CreateMonitoredItemsRequest *request, int event_monitoring,
    const cpkt_opcua_MonitoredItemCallbacks *registrations,
    cpkt_opcua_CreateMonitoredItemsResponse *response) {
  UA_CreateMonitoredItemsRequest native_request;
  UA_CreateMonitoredItemsResponse native_response;
  cpkt_monitor_batch batch;
  UA_StatusCode status;
  if (!client || !request || !response ||
      (request->itemsToCreateSize && !registrations))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memset(response, 0, sizeof(*response));
  UA_CreateMonitoredItemsRequest_init(&native_request);
  status = cpkt_convert(
      request, &native_request,
      &cpkt_types[CPKT_OPCUA_TYPES_CREATEMONITOREDITEMSREQUEST], 1, 0);
  if (!status)
    status = cpkt_monitor_batch_new(&batch, client, request, event_monitoring,
                                    registrations);
  if (status) {
    UA_CreateMonitoredItemsRequest_clear(&native_request);
    return status;
  }
  if (event_monitoring)
    native_response = UA_Client_MonitoredItems_createEvents(
        client->client, native_request, batch.contexts, batch.event_callbacks,
        batch.delete_callbacks);
  else
    native_response = UA_Client_MonitoredItems_createDataChanges(
        client->client, native_request, batch.contexts, batch.data_callbacks,
        batch.delete_callbacks);
  status = cpkt_convert(
      &native_response, response,
      &cpkt_types[CPKT_OPCUA_TYPES_CREATEMONITOREDITEMSRESPONSE], 0, 0);
  if (status)
    cpkt_opcua_CreateMonitoredItemsResponse_clear(response);
  cpkt_monitor_batch_finish(&batch, &native_response);
  UA_CreateMonitoredItemsRequest_clear(&native_request);
  UA_CreateMonitoredItemsResponse_clear(&native_response);
  return status;
}
typedef struct {
  cpkt_async_base base;
  cpkt_monitor_batch batch;
  cpkt_opcua_async_service_fn fn;
} cpkt_monitor_async;
static void cpkt_monitor_async_deliver(cpkt_async_base *base, UA_UInt32 id,
                                       UA_StatusCode status,
                                       const void *response) {
  cpkt_monitor_async *context = (cpkt_monitor_async *)base;
  context->fn(base->client, base->user, id, status, response, base->type);
}
static void
cpkt_monitor_async_native(UA_Client *client, void *user, UA_UInt32 id,
                          UA_CreateMonitoredItemsResponse *response) {
  cpkt_monitor_async *context = (cpkt_monitor_async *)user;
  size_t i;
  /* Publish notifications and user completion can access contexts before
   * creation has returned. Assign IDs before handing control to the user. */
  if (!response->responseHeader.serviceResult)
    for (i = 0; i < context->batch.count && i < response->resultsSize; ++i)
      if (!response->results[i].statusCode)
        context->batch.monitors[i]->id = response->results[i].monitoredItemId;
  cpkt_monitor_batch_finish(&context->batch, response);
  cpkt_async_response(client, context, id, response);
}
static UA_StatusCode cpkt_monitor_async_invoke(UA_Client *client,
                                               const void *request,
                                               cpkt_async_base *base,
                                               UA_UInt32 *id) {
  cpkt_monitor_async *context = (cpkt_monitor_async *)base;
  cpkt_monitor_batch *batch = &context->batch;
  if (batch->events)
    return UA_Client_MonitoredItems_createEvents_async(
        client, *(const UA_CreateMonitoredItemsRequest *)request,
        batch->contexts, batch->event_callbacks, batch->delete_callbacks,
        cpkt_monitor_async_native, context, id);
  return UA_Client_MonitoredItems_createDataChanges_async(
      client, *(const UA_CreateMonitoredItemsRequest *)request, batch->contexts,
      batch->data_callbacks, batch->delete_callbacks, cpkt_monitor_async_native,
      context, id);
}
/** Implements upstream's specialized asynchronous monitored-item creation. */
cpkt_opcua_StatusCode cpkt_opcua_client_monitored_items_create_typed_async(
    cpkt_opcua_client *client,
    const cpkt_opcua_CreateMonitoredItemsRequest *request, int event_monitoring,
    const cpkt_opcua_MonitoredItemCallbacks *registrations,
    cpkt_opcua_async_service_fn complete_fn, void *user,
    cpkt_opcua_UInt32 *request_id) {
  cpkt_monitor_async *call;
  cpkt_monitor_batch saved;
  UA_StatusCode status;
  if (request_id)
    *request_id = 0;
  if (!client || !request || !complete_fn ||
      (request->itemsToCreateSize && !registrations))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  call = (cpkt_monitor_async *)UA_calloc(1, sizeof(*call));
  if (!call)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  status = cpkt_monitor_batch_new(&call->batch, client, request,
                                  event_monitoring, registrations);
  if (status) {
    UA_free(call);
    return status;
  }
  saved = call->batch;
  call->fn = complete_fn;
  call->base.client = client;
  call->base.user = user;
  call->base.type = &cpkt_types[CPKT_OPCUA_TYPES_CREATEMONITOREDITEMSRESPONSE];
  call->base.deliver = cpkt_monitor_async_deliver;
  call->base.invoke = cpkt_monitor_async_invoke;
  status = cpkt_typed_async(
      request, &cpkt_types[CPKT_OPCUA_TYPES_CREATEMONITOREDITEMSREQUEST],
      &call->base, request_id);
  /* Submission failure freed the async context. The batch remains separately
   * owned until either native deletion callbacks or this cleanup release it. */
  if (status)
    cpkt_monitor_batch_finish(&saved, NULL);
  return status;
}
/** Implements context access without leaking subscription bridge pointers. */
cpkt_opcua_StatusCode cpkt_opcua_client_subscription_get_context(
    cpkt_opcua_client *client, cpkt_opcua_UInt32 id, void **context) {
  UA_StatusCode status;
  void *native;
  if (!client || !context)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  *context = NULL;
  status = UA_Client_Subscriptions_getContext(client->client, id, &native);
  if (!status)
    *context = cpkt_subscription_user(client, id, native);
  return status;
}
/** Implements context replacement while preserving subscription callbacks. */
cpkt_opcua_StatusCode cpkt_opcua_client_subscription_set_context(
    cpkt_opcua_client *client, cpkt_opcua_UInt32 id, void *context) {
  struct cpkt_typed_subscription *sub;
  if (!client)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  sub = cpkt_find_subscription(client, id);
  if (sub) {
    sub->user = context;
    return 0;
  }
  return UA_Client_Subscriptions_setContext(client->client, id, context);
}
/** Implements native subscription deletion. */
cpkt_opcua_StatusCode
cpkt_opcua_client_subscription_delete_single(cpkt_opcua_client *client,
                                             cpkt_opcua_UInt32 id) {
  if (!client)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  return UA_Client_Subscriptions_deleteSingle(client->client, id);
}
/** Implements context access without leaking monitored-item bridge pointers. */
cpkt_opcua_StatusCode cpkt_opcua_client_monitored_item_get_context(
    cpkt_opcua_client *client, cpkt_opcua_UInt32 sub_id, cpkt_opcua_UInt32 id,
    void **context) {
  struct cpkt_typed_monitor *mon;
  if (!client || !context)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  *context = NULL;
  mon = cpkt_find_monitor(client, sub_id, id);
  if (mon) {
    *context = mon->callbacks.context;
    return 0;
  }
  return UA_Client_MonitoredItem_getContext(client->client, sub_id, id,
                                            context);
}
/** Implements context replacement while preserving monitored-item callbacks. */
cpkt_opcua_StatusCode cpkt_opcua_client_monitored_item_set_context(
    cpkt_opcua_client *client, cpkt_opcua_UInt32 sub_id, cpkt_opcua_UInt32 id,
    void *context) {
  struct cpkt_typed_monitor *mon;
  if (!client)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  mon = cpkt_find_monitor(client, sub_id, id);
  if (mon) {
    mon->callbacks.context = context;
    return 0;
  }
  return UA_Client_MonitoredItem_setContext(client->client, sub_id, id,
                                            context);
}
/** Implements native monitored-item deletion. */
cpkt_opcua_StatusCode cpkt_opcua_client_monitored_item_delete_single(
    cpkt_opcua_client *client, cpkt_opcua_UInt32 sub_id, cpkt_opcua_UInt32 id) {
  if (!client)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  return UA_Client_MonitoredItems_deleteSingle(client->client, sub_id, id);
}

/* Timer userdata is retained across reentrant cancellation, but the native
 * EventLoop remains the only scheduler. All handle operations are serialized.
 */
struct cpkt_server_timer {
  struct cpkt_server_timer *next;
  cpkt_opcua_server *owner;
  cpkt_opcua_ServerCallback callback;
  void *data;
  UA_UInt64 id;
  unsigned int references;
  int repeated;
  int removed;
};
static void cpkt_server_timer_unref(struct cpkt_server_timer *timer) {
  struct cpkt_server_timer **slot;
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
static void cpkt_server_timer_invoke(UA_Server *native, void *context) {
  struct cpkt_server_timer *timer = (struct cpkt_server_timer *)context;
  (void)native;
  ++timer->references;
  timer->callback(timer->owner, timer->data);
  if (!timer->repeated && !timer->removed) {
    timer->removed = 1;
    cpkt_server_timer_unref(timer);
  }
  cpkt_server_timer_unref(timer);
}
static void cpkt_server_callbacks_clear(cpkt_opcua_server *server) {
  struct cpkt_server_timer *timer;
  while ((timer = server->typed_timers) != NULL) {
    server->typed_timers = timer->next;
    UA_Server_removeCallback(server->server, timer->id);
    UA_free(timer);
  }
}
static UA_StatusCode cpkt_server_timer_add(cpkt_opcua_server *server,
                                           cpkt_opcua_ServerCallback callback,
                                           void *data, UA_DateTime date,
                                           UA_Double interval, int repeated,
                                           cpkt_opcua_UInt64 *callback_id) {
  struct cpkt_server_timer *timer;
  UA_StatusCode status;
  if (callback_id)
    memset(callback_id, 0, sizeof(*callback_id));
  if (!server || !callback)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  timer = (struct cpkt_server_timer *)UA_calloc(1, sizeof(*timer));
  if (!timer)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  timer->owner = server;
  timer->callback = callback;
  timer->data = data;
  timer->references = 1;
  timer->repeated = repeated;
  if (repeated)
    status = UA_Server_addRepeatedCallback(
        server->server, cpkt_server_timer_invoke, timer, interval, &timer->id);
  else
    status = UA_Server_addTimedCallback(
        server->server, cpkt_server_timer_invoke, timer, date, &timer->id);
  if (status) {
    UA_free(timer);
    return status;
  }
  timer->next = server->typed_timers;
  server->typed_timers = timer;
  server->typed_callbacks_clear = cpkt_server_callbacks_clear;
  if (callback_id)
    cpkt_convert(&timer->id, callback_id, &cpkt_types[CPKT_OPCUA_TYPES_UINT64],
                 0, 0);
  return 0;
}
/** Implements native one-shot scheduling with C89 DateTime conversion. */
cpkt_opcua_StatusCode cpkt_opcua_server_addTimedCallback_typed(
    cpkt_opcua_server *server, cpkt_opcua_ServerCallback callback, void *data,
    cpkt_opcua_DateTime date, cpkt_opcua_UInt64 *callback_id) {
  UA_DateTime native_date;
  cpkt_convert(&date, &native_date, &cpkt_types[CPKT_OPCUA_TYPES_DATETIME], 1,
               0);
  return cpkt_server_timer_add(server, callback, data, native_date, 0, 0,
                               callback_id);
}
/** Implements native repeated scheduling without retaining a facade queue. */
cpkt_opcua_StatusCode cpkt_opcua_server_addRepeatedCallback_typed(
    cpkt_opcua_server *server, cpkt_opcua_ServerCallback callback, void *data,
    cpkt_opcua_Double interval_ms, cpkt_opcua_UInt64 *callback_id) {
  return cpkt_server_timer_add(server, callback, data, 0, interval_ms, 1,
                               callback_id);
}
/** Implements native repeated timer interval changes. */
cpkt_opcua_StatusCode cpkt_opcua_server_changeRepeatedCallbackInterval_typed(
    cpkt_opcua_server *server, cpkt_opcua_UInt64 callback_id,
    cpkt_opcua_Double interval_ms) {
  UA_UInt64 native_id;
  UA_StatusCode status;
  struct cpkt_server_timer *timer;
  if (!server)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  cpkt_convert(&callback_id, &native_id, &cpkt_types[CPKT_OPCUA_TYPES_UINT64],
               1, 0);
  status = UA_Server_changeRepeatedCallbackInterval(server->server, native_id,
                                                    interval_ms);
  if (!status) {
    for (timer = server->typed_timers; timer; timer = timer->next) {
      if (timer->id == native_id && !timer->removed) {
        timer->repeated = 1;
        break;
      }
    }
  }
  return status;
}
/** Implements reentrant cancellation while preserving native callback IDs. */
void cpkt_opcua_server_removeCallback_typed(cpkt_opcua_server *server,
                                            cpkt_opcua_UInt64 callback_id) {
  UA_UInt64 native_id;
  struct cpkt_server_timer *timer;
  if (!server)
    return;
  cpkt_convert(&callback_id, &native_id, &cpkt_types[CPKT_OPCUA_TYPES_UINT64],
               1, 0);
  UA_Server_removeCallback(server->server, native_id);
  for (timer = server->typed_timers; timer; timer = timer->next) {
    if (timer->id == native_id && !timer->removed) {
      timer->removed = 1;
      cpkt_server_timer_unref(timer);
      break;
    }
  }
}
struct cpkt_server_iterator {
  cpkt_opcua_NodeIteratorCallback callback;
  void *handle;
};
static UA_StatusCode cpkt_server_iterator_invoke(UA_NodeId child,
                                                 UA_Boolean inverse,
                                                 UA_NodeId reference,
                                                 void *context) {
  struct cpkt_server_iterator *iterator =
      (struct cpkt_server_iterator *)context;
  cpkt_opcua_NodeId c_child, c_reference;
  UA_StatusCode status;
  memset(&c_child, 0, sizeof(c_child));
  memset(&c_reference, 0, sizeof(c_reference));
  status = cpkt_convert(&child, &c_child, &cpkt_types[CPKT_OPCUA_TYPES_NODEID],
                        0, 0);
  if (!status)
    status = cpkt_convert(&reference, &c_reference,
                          &cpkt_types[CPKT_OPCUA_TYPES_NODEID], 0, 0);
  if (!status)
    status =
        iterator->callback(c_child, inverse, c_reference, iterator->handle);
  cpkt_opcua_NodeId_clear(&c_child);
  cpkt_opcua_NodeId_clear(&c_reference);
  return status;
}
/** Implements the synchronous upstream child-node iterator boundary. */
cpkt_opcua_StatusCode cpkt_opcua_server_forEachChildNodeCall_typed(
    cpkt_opcua_server *server, cpkt_opcua_NodeId parent,
    cpkt_opcua_NodeIteratorCallback callback, void *handle) {
  struct cpkt_server_iterator iterator;
  UA_NodeId native_parent;
  UA_StatusCode status;
  if (!server || !callback)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memset(&native_parent, 0, sizeof(native_parent));
  status = cpkt_convert(&parent, &native_parent,
                        &cpkt_types[CPKT_OPCUA_TYPES_NODEID], 1, 0);
  iterator.callback = callback;
  iterator.handle = handle;
  if (!status)
    status = UA_Server_forEachChildNodeCall(
        server->server, native_parent, cpkt_server_iterator_invoke, &iterator);
  UA_NodeId_clear(&native_parent);
  return status;
}

/* Local operations persist their outputs in the native AsyncManager. This
 * registry owns only callback dispatch/context identities, never a response
 * queue. Two references protect a completion that runs inside submission. */
struct cpkt_server_async {
  struct cpkt_server_async *next;
  cpkt_opcua_server *owner;
  void *context;
  unsigned int references;
  int completed;
  union {
    cpkt_opcua_ServerAsyncReadResultCallback read;
    cpkt_opcua_ServerAsyncWriteResultCallback write;
    cpkt_opcua_ServerAsyncMethodResultCallback call;
  } callback;
};
static void cpkt_server_async_unref(struct cpkt_server_async *operation) {
  struct cpkt_server_async **slot;
  if (--operation->references)
    return;
  for (slot = &operation->owner->typed_asyncs; *slot; slot = &(*slot)->next) {
    if (*slot == operation) {
      *slot = operation->next;
      break;
    }
  }
  UA_free(operation);
}
static struct cpkt_server_async *
cpkt_server_async_new(cpkt_opcua_server *server, void *context) {
  struct cpkt_server_async *operation;
  operation = (struct cpkt_server_async *)UA_calloc(1, sizeof(*operation));
  if (operation) {
    operation->owner = server;
    operation->context = context;
    operation->references = 2;
  }
  return operation;
}
static void cpkt_server_async_submitted(struct cpkt_server_async *operation,
                                        UA_StatusCode status) {
  struct cpkt_server_async **slot;
  if (operation->owner->typed_producers_refresh)
    operation->owner->typed_producers_refresh(operation->owner);
  if (!operation->completed) {
    if (status) {
      cpkt_server_async_unref(operation);
    } else {
      /* Append after native persistence, matching the order of accepted
       * operations even when a value-source callback submits another call. */
      for (slot = &operation->owner->typed_asyncs; *slot; slot = &(*slot)->next)
        ;
      *slot = operation;
    }
  }
  cpkt_server_async_unref(operation);
}
static void cpkt_server_read_completed(UA_Server *server, void *context,
                                       const UA_DataValue *result) {
  struct cpkt_server_async *operation = (struct cpkt_server_async *)context;
  cpkt_opcua_DataValue value;
  UA_StatusCode status;
  (void)server;
  memset(&value, 0, sizeof(value));
  status = cpkt_convert(result, &value, &cpkt_types[CPKT_OPCUA_TYPES_DATAVALUE],
                        0, 0);
  operation->completed = 1;
  operation->callback.read(operation->owner, operation->context, status,
                           status ? NULL : &value);
  cpkt_opcua_DataValue_clear(&value);
  cpkt_server_async_unref(operation);
}
static void cpkt_server_write_completed(UA_Server *server, void *context,
                                        UA_StatusCode result) {
  struct cpkt_server_async *operation = (struct cpkt_server_async *)context;
  (void)server;
  operation->completed = 1;
  operation->callback.write(operation->owner, operation->context, result);
  cpkt_server_async_unref(operation);
}
static void cpkt_server_call_completed(UA_Server *server, void *context,
                                       const UA_CallMethodResult *result) {
  struct cpkt_server_async *operation = (struct cpkt_server_async *)context;
  cpkt_opcua_CallMethodResult value;
  UA_StatusCode status;
  (void)server;
  memset(&value, 0, sizeof(value));
  status = cpkt_convert(result, &value,
                        &cpkt_types[CPKT_OPCUA_TYPES_CALLMETHODRESULT], 0, 0);
  operation->completed = 1;
  operation->callback.call(operation->owner, operation->context, status,
                           status ? NULL : &value);
  cpkt_opcua_CallMethodResult_clear(&value);
  cpkt_server_async_unref(operation);
}
/** Implements native local asynchronous read with complete C89 records. */
cpkt_opcua_StatusCode cpkt_opcua_server_read_async_typed(
    cpkt_opcua_server *server, const cpkt_opcua_ReadValueId *operation,
    cpkt_opcua_TimestampsToReturn timestamps,
    cpkt_opcua_ServerAsyncReadResultCallback callback, void *context,
    cpkt_opcua_UInt32 timeout_ms) {
  struct cpkt_server_async *bridge;
  UA_ReadValueId request;
  UA_StatusCode status;
  if (!server || !operation || !callback)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (server->destroying)
    return UA_STATUSCODE_BADSHUTDOWN;
  memset(&request, 0, sizeof(request));
  status = cpkt_convert(operation, &request,
                        &cpkt_types[CPKT_OPCUA_TYPES_READVALUEID], 1, 0);
  if (status) {
    UA_ReadValueId_clear(&request);
    return status;
  }
  bridge = cpkt_server_async_new(server, context);
  if (!bridge) {
    UA_ReadValueId_clear(&request);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  bridge->callback.read = callback;
  status = UA_Server_read_async(server->server, &request,
                                (UA_TimestampsToReturn)timestamps,
                                cpkt_server_read_completed, bridge, timeout_ms);
  UA_ReadValueId_clear(&request);
  cpkt_server_async_submitted(bridge, status);
  return status;
}
/** Implements native local asynchronous write with no facade result queue. */
cpkt_opcua_StatusCode cpkt_opcua_server_write_async_typed(
    cpkt_opcua_server *server, const cpkt_opcua_WriteValue *operation,
    cpkt_opcua_ServerAsyncWriteResultCallback callback, void *context,
    cpkt_opcua_UInt32 timeout_ms) {
  struct cpkt_server_async *bridge;
  UA_WriteValue request;
  UA_StatusCode status;
  if (!server || !operation || !callback)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (server->destroying)
    return UA_STATUSCODE_BADSHUTDOWN;
  memset(&request, 0, sizeof(request));
  status = cpkt_convert(operation, &request,
                        &cpkt_types[CPKT_OPCUA_TYPES_WRITEVALUE], 1, 0);
  if (status) {
    UA_WriteValue_clear(&request);
    return status;
  }
  bridge = cpkt_server_async_new(server, context);
  if (!bridge) {
    UA_WriteValue_clear(&request);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  bridge->callback.write = callback;
  status =
      UA_Server_write_async(server->server, &request,
                            cpkt_server_write_completed, bridge, timeout_ms);
  UA_WriteValue_clear(&request);
  cpkt_server_async_submitted(bridge, status);
  return status;
}
/** Implements native local asynchronous method calls with C89 Variants. */
cpkt_opcua_StatusCode cpkt_opcua_server_call_async_typed(
    cpkt_opcua_server *server, const cpkt_opcua_CallMethodRequest *operation,
    cpkt_opcua_ServerAsyncMethodResultCallback callback, void *context,
    cpkt_opcua_UInt32 timeout_ms) {
  struct cpkt_server_async *bridge;
  UA_CallMethodRequest request;
  UA_StatusCode status;
  if (!server || !operation || !callback)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (server->destroying)
    return UA_STATUSCODE_BADSHUTDOWN;
  memset(&request, 0, sizeof(request));
  status = cpkt_convert(operation, &request,
                        &cpkt_types[CPKT_OPCUA_TYPES_CALLMETHODREQUEST], 1, 0);
  if (status) {
    UA_CallMethodRequest_clear(&request);
    return status;
  }
  bridge = cpkt_server_async_new(server, context);
  if (!bridge) {
    UA_CallMethodRequest_clear(&request);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  bridge->callback.call = callback;
  status = UA_Server_call_async(server->server, &request,
                                cpkt_server_call_completed, bridge, timeout_ms);
  UA_CallMethodRequest_clear(&request);
  cpkt_server_async_submitted(bridge, status);
  return status;
}
/** Implements native cancellation using caller-visible context identity. */
void cpkt_opcua_server_cancelAsync_typed(
    cpkt_opcua_server *server, void *context, cpkt_opcua_StatusCode status,
    cpkt_opcua_Boolean synchronous_result_callback) {
  struct cpkt_server_async *operation, *last, *next;
  /* Native destruction already owns cancellation of every remaining entry. */
  if (!server || !server->server || server->destroying)
    return;
  /* Pin the existing registry before invoking callbacks. Completed entries
   * cannot disappear underneath a reentrant cancellation, and operations
   * submitted by a completion callback are outside this cancellation pass. */
  last = NULL;
  for (operation = server->typed_asyncs; operation;
       operation = operation->next) {
    ++operation->references;
    last = operation;
  }
  operation = server->typed_asyncs;
  UA_Server_cancelAsync(server->server, context, status,
                        synchronous_result_callback != 0);
  while (operation) {
    next = operation->next;
    if (!operation->completed && operation->context == context)
      UA_Server_cancelAsync(server->server, operation, status,
                            synchronous_result_callback != 0);
    if (operation == last) {
      cpkt_server_async_unref(operation);
      break;
    }
    cpkt_server_async_unref(operation);
    operation = next;
  }
}
