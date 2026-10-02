/* Handwritten OPC UA facade: subscriptions. */
#include "opcua_facade_internal.h"
#include <open62541/nodeids.h>
#include <stdlib.h>
#include <string.h>

static void
cpkt_remove_monitor_context(cpkt_opcua_client *client,
                            struct cpkt_opcua_monitor_context *context) {
  struct cpkt_opcua_monitor_context **slot;

  if (client == NULL || context == NULL) {
    return;
  }
  slot = &client->monitors;
  while (*slot != NULL) {
    if (*slot == context) {
      *slot = context->next;
      return;
    }
    slot = &(*slot)->next;
  }
}

static void
cpkt_monitor_context_free(struct cpkt_opcua_monitor_context *context) {
  size_t i;

  if (context == NULL) {
    return;
  }
  if (context->event_field_names != NULL) {
    for (i = 0; i < context->event_field_count; ++i) {
      free(context->event_field_names[i]);
    }
    free(context->event_field_names);
  }
  free(context);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
void cpkt_opcua_monitor_options_default(cpkt_opcua_monitor_options *options) {
  if (options == NULL) {
    return;
  }
  options->sampling_interval_ms = 0.0;
  options->queue_size = 0;
  options->discard_oldest = 1;
  options->deadband_type = CPKT_OPCUA_DEADBAND_NONE;
  options->deadband_value = 0.0;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_create_subscription(
    cpkt_opcua_client *client, double publishing_interval_ms,
    cpkt_opcua_subscription_id *subscription_id_out,
    cpkt_opcua_status *status_out) {
  UA_CreateSubscriptionRequest request;
  UA_CreateSubscriptionResponse response;
  UA_StatusCode service_result;
  UA_UInt32 subscription_id;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (subscription_id_out != NULL) {
    *subscription_id_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      publishing_interval_ms < 0.0) {
    return CPKT_OPCUA_ERR_ARG;
  }
  request = UA_CreateSubscriptionRequest_default();
  if (publishing_interval_ms > 0.0) {
    request.requestedPublishingInterval = publishing_interval_ms;
  }
  response =
      UA_Client_Subscriptions_create(client->client, request, NULL, NULL, NULL);
  service_result = response.responseHeader.serviceResult;
  subscription_id = response.subscriptionId;
  if (status_out != NULL) {
    *status_out = cpkt_status(service_result);
  }
  UA_CreateSubscriptionResponse_clear(&response);
  if (service_result != UA_STATUSCODE_GOOD) {
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (subscription_id_out != NULL) {
    *subscription_id_out = (cpkt_opcua_subscription_id)subscription_id;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_delete_subscription(
    cpkt_opcua_client *client, cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_status *status_out) {
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      subscription_id > (unsigned long)UINT_MAX) {
    return CPKT_OPCUA_ERR_ARG;
  }
  status = UA_Client_Subscriptions_deleteSingle(client->client,
                                                (UA_UInt32)subscription_id);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_modify_subscription(
    cpkt_opcua_client *client, cpkt_opcua_subscription_id subscription_id,
    double publishing_interval_ms, cpkt_opcua_status *status_out) {
  UA_ModifySubscriptionRequest request;
  UA_ModifySubscriptionResponse response;
  UA_StatusCode service_result;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      subscription_id > (unsigned long)UINT_MAX ||
      publishing_interval_ms < 0.0) {
    return CPKT_OPCUA_ERR_ARG;
  }
  UA_ModifySubscriptionRequest_init(&request);
  request.subscriptionId = (UA_UInt32)subscription_id;
  request.requestedPublishingInterval = publishing_interval_ms;
  request.requestedLifetimeCount = 10000;
  request.requestedMaxKeepAliveCount = 10;
  request.maxNotificationsPerPublish = 0;
  request.priority = 0;
  response = UA_Client_Subscriptions_modify(client->client, request);
  service_result = response.responseHeader.serviceResult;
  if (status_out != NULL) {
    *status_out = cpkt_status(service_result);
  }
  UA_ModifySubscriptionResponse_clear(&response);
  return service_result == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK
                                              : CPKT_OPCUA_ERR_UPSTREAM;
}

static void cpkt_data_change_callback(UA_Client *native_client,
                                      UA_UInt32 subscription_id,
                                      void *subscription_context,
                                      UA_UInt32 monitored_item_id,
                                      void *monitored_item_context,
                                      UA_DataValue *data_value) {
  struct cpkt_opcua_monitor_context *context;
  cpkt_opcua_value value;
  cpkt_opcua_result result;
  char *string_buffer;
  size_t required;

  (void)native_client;
  (void)subscription_context;
  context = (struct cpkt_opcua_monitor_context *)monitored_item_context;
  if (context == NULL || context->fn == NULL) {
    return;
  }
  string_buffer = NULL;
  cpkt_opcua_value_clear(&value);
  if (data_value == NULL || !data_value->hasValue) {
    context->fn((cpkt_opcua_subscription_id)subscription_id,
                (cpkt_opcua_monitored_item_id)monitored_item_id, &value,
                UA_STATUSCODE_BADNODATAAVAILABLE, context->user);
    return;
  }
  result = cpkt_get_variant(&data_value->value, &value, NULL, 0, &required);
  if (result == CPKT_OPCUA_ERR_RANGE &&
      cpkt_value_type_needs_buffer(value.type) && required != 0) {
    string_buffer = (char *)malloc(required);
    if (string_buffer != NULL) {
      result = cpkt_get_variant(&data_value->value, &value, string_buffer,
                                required, &required);
    } else {
      result = CPKT_OPCUA_ERR_ALLOC;
    }
  }
  context->fn((cpkt_opcua_subscription_id)subscription_id,
              (cpkt_opcua_monitored_item_id)monitored_item_id, &value,
              result == CPKT_OPCUA_OK ? UA_STATUSCODE_GOOD
                                      : UA_STATUSCODE_BADTYPEMISMATCH,
              context->user);
  free(string_buffer);
}

static void cpkt_delete_monitored_item_callback(UA_Client *native_client,
                                                UA_UInt32 subscription_id,
                                                void *subscription_context,
                                                UA_UInt32 monitored_item_id,
                                                void *monitored_item_context) {
  struct cpkt_opcua_monitor_context *context;

  (void)native_client;
  (void)subscription_id;
  (void)subscription_context;
  (void)monitored_item_id;
  context = (struct cpkt_opcua_monitor_context *)monitored_item_context;
  if (context == NULL) {
    return;
  }
  cpkt_remove_monitor_context(context->owner, context);
  cpkt_monitor_context_free(context);
}

static int cpkt_event_field_matches(const UA_QualifiedName *key,
                                    const char *name) {
  const char *key_text;
  size_t key_length;
  size_t name_length;
  size_t offset;

  if (key == NULL || key->name.data == NULL || name == NULL) {
    return 0;
  }
  key_text = (const char *)key->name.data;
  key_length = key->name.length;
  name_length = strlen(name);
  if (key_length == name_length && memcmp(key_text, name, name_length) == 0) {
    return 1;
  }
  if (key_length > name_length &&
      key_text[key_length - name_length - 1] == '/') {
    offset = key_length - name_length;
    return memcmp(key_text + offset, name, name_length) == 0;
  }
  return 0;
}

static void cpkt_event_callback(UA_Client *native_client,
                                UA_UInt32 subscription_id,
                                void *subscription_context,
                                UA_UInt32 monitored_item_id,
                                void *monitored_item_context,
                                const UA_KeyValueMap event_fields) {
  struct cpkt_opcua_monitor_context *context;
  cpkt_opcua_event event;
  UA_KeyValuePair *pair;
  UA_ByteString *event_id;
  UA_String *source_name;
  UA_LocalizedText *message;
  UA_UInt16 *severity;
  size_t i;

  (void)native_client;
  (void)subscription_context;
  context = (struct cpkt_opcua_monitor_context *)monitored_item_context;
  if (context == NULL || context->event_fn == NULL) {
    return;
  }
  memset(&event, 0, sizeof(event));
  for (i = 0; i < event_fields.mapSize; ++i) {
    pair = &event_fields.map[i];
    if (cpkt_event_field_matches(&pair->key, "EventId") &&
        UA_Variant_hasScalarType(&pair->value,
                                 &UA_TYPES[UA_TYPES_BYTESTRING])) {
      event_id = (UA_ByteString *)pair->value.data;
      event.event_id = (const unsigned char *)event_id->data;
      event.event_id_length = event_id->length;
    } else if (cpkt_event_field_matches(&pair->key, "SourceName") &&
               UA_Variant_hasScalarType(&pair->value,
                                        &UA_TYPES[UA_TYPES_STRING])) {
      source_name = (UA_String *)pair->value.data;
      event.source_name = (const char *)source_name->data;
      event.source_name_length = source_name->length;
    } else if (cpkt_event_field_matches(&pair->key, "Message") &&
               UA_Variant_hasScalarType(&pair->value,
                                        &UA_TYPES[UA_TYPES_LOCALIZEDTEXT])) {
      message = (UA_LocalizedText *)pair->value.data;
      event.message = (const char *)message->text.data;
      event.message_length = message->text.length;
    } else if (cpkt_event_field_matches(&pair->key, "Severity") &&
               UA_Variant_hasScalarType(&pair->value,
                                        &UA_TYPES[UA_TYPES_UINT16])) {
      severity = (UA_UInt16 *)pair->value.data;
      event.severity = (unsigned long)*severity;
    }
  }
  context->event_fn((cpkt_opcua_subscription_id)subscription_id,
                    (cpkt_opcua_monitored_item_id)monitored_item_id, &event, 0,
                    context->user);
}

static void cpkt_event_fields_callback(UA_Client *native_client,
                                       UA_UInt32 subscription_id,
                                       void *subscription_context,
                                       UA_UInt32 monitored_item_id,
                                       void *monitored_item_context,
                                       const UA_KeyValueMap event_fields) {
  struct cpkt_opcua_monitor_context *context;
  cpkt_opcua_event_field *fields;
  UA_KeyValuePair *pair;
  cpkt_opcua_status callback_status;
  cpkt_opcua_status field_status;
  size_t i;
  size_t j;

  (void)native_client;
  (void)subscription_context;
  context = (struct cpkt_opcua_monitor_context *)monitored_item_context;
  if (context == NULL || context->event_fields_fn == NULL) {
    return;
  }
  fields = NULL;
  callback_status = 0;
  if (context->event_field_count != 0) {
    fields = (cpkt_opcua_event_field *)calloc(context->event_field_count,
                                              sizeof(*fields));
    if (fields == NULL) {
      context->event_fields_fn((cpkt_opcua_subscription_id)subscription_id,
                               (cpkt_opcua_monitored_item_id)monitored_item_id,
                               NULL, 0, UA_STATUSCODE_BADOUTOFMEMORY,
                               context->user);
      return;
    }
  }
  for (i = 0; i < context->event_field_count; ++i) {
    fields[i].name = context->event_field_names[i];
    fields[i].name_length = strlen(context->event_field_names[i]);
    field_status = cpkt_status(UA_STATUSCODE_BADNOTFOUND);
    for (j = 0; j < event_fields.mapSize; ++j) {
      pair = &event_fields.map[j];
      if (!cpkt_event_field_matches(&pair->key,
                                    context->event_field_names[i])) {
        continue;
      }
      field_status = cpkt_status(UA_STATUSCODE_GOOD);
      if (cpkt_get_variant_borrowed(&pair->value, &fields[i].value) !=
          CPKT_OPCUA_OK) {
        field_status = cpkt_status(UA_STATUSCODE_BADTYPEMISMATCH);
      }
      break;
    }
    fields[i].status = field_status;
    if (field_status != cpkt_status(UA_STATUSCODE_GOOD)) {
      callback_status = field_status;
    }
  }
  context->event_fields_fn((cpkt_opcua_subscription_id)subscription_id,
                           (cpkt_opcua_monitored_item_id)monitored_item_id,
                           fields, context->event_field_count, callback_status,
                           context->user);
  free(fields);
}

static void
cpkt_init_event_select_clause(UA_SimpleAttributeOperand *select_clause,
                              UA_QualifiedName *browse_name,
                              const char *field_name) {
  UA_SimpleAttributeOperand_init(select_clause);
  *browse_name = UA_QUALIFIEDNAME(0, (char *)field_name);
  select_clause->typeDefinitionId =
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEEVENTTYPE);
  select_clause->browsePathSize = 1;
  select_clause->browsePath = browse_name;
  select_clause->attributeId = UA_ATTRIBUTEID_VALUE;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_monitor_value(
    cpkt_opcua_client *client, cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_node_id node_id, double sampling_interval_ms,
    cpkt_opcua_data_change_fn fn, void *user,
    cpkt_opcua_monitored_item_id *monitored_item_id_out,
    cpkt_opcua_status *status_out) {
  cpkt_opcua_monitor_options options;

  cpkt_opcua_monitor_options_default(&options);
  options.sampling_interval_ms = sampling_interval_ms;
  return cpkt_opcua_client_monitor_value_ex(client, subscription_id, node_id,
                                            &options, fn, user,
                                            monitored_item_id_out, status_out);
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_monitor_value_ex(
    cpkt_opcua_client *client, cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_node_id node_id, const cpkt_opcua_monitor_options *options,
    cpkt_opcua_data_change_fn fn, void *user,
    cpkt_opcua_monitored_item_id *monitored_item_id_out,
    cpkt_opcua_status *status_out) {
  struct cpkt_opcua_monitor_context *context;
  UA_MonitoredItemCreateRequest request;
  UA_MonitoredItemCreateResult result;
  UA_DataChangeFilter filter;
  UA_NodeId native_node_id;
  UA_StatusCode item_status;
  UA_UInt32 monitored_item_id;
  double sampling_interval_ms;
  unsigned long queue_size;
  int discard_oldest;
  int deadband_type;
  double deadband_value;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (monitored_item_id_out != NULL) {
    *monitored_item_id_out = 0;
  }
  sampling_interval_ms = options != NULL ? options->sampling_interval_ms : 0.0;
  queue_size = options != NULL ? options->queue_size : 0;
  discard_oldest = options != NULL ? options->discard_oldest : 1;
  deadband_type =
      options != NULL ? options->deadband_type : CPKT_OPCUA_DEADBAND_NONE;
  deadband_value = options != NULL ? options->deadband_value : 0.0;
  if (client == NULL || client->client == NULL || fn == NULL ||
      subscription_id > (unsigned long)UINT_MAX || sampling_interval_ms < 0.0 ||
      queue_size > (unsigned long)UINT_MAX || !cpkt_valid_node_id(node_id) ||
      (deadband_type != CPKT_OPCUA_DEADBAND_NONE &&
       deadband_type != CPKT_OPCUA_DEADBAND_ABSOLUTE &&
       deadband_type != CPKT_OPCUA_DEADBAND_PERCENT) ||
      (deadband_type == CPKT_OPCUA_DEADBAND_NONE && deadband_value != 0.0) ||
      (deadband_type != CPKT_OPCUA_DEADBAND_NONE && deadband_value < 0.0) ||
      (deadband_type == CPKT_OPCUA_DEADBAND_PERCENT &&
       deadband_value > 100.0)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  context = (struct cpkt_opcua_monitor_context *)calloc(1, sizeof(*context));
  if (context == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->owner = client;
  context->subscription_id = subscription_id;
  context->fn = fn;
  context->user = user;

  native_node_id = cpkt_make_node_id(node_id);
  request = UA_MonitoredItemCreateRequest_default(native_node_id);
  if (sampling_interval_ms > 0.0) {
    request.requestedParameters.samplingInterval = sampling_interval_ms;
  }
  if (queue_size != 0) {
    request.requestedParameters.queueSize = (UA_UInt32)queue_size;
  }
  request.requestedParameters.discardOldest = discard_oldest ? true : false;
  if (deadband_type != CPKT_OPCUA_DEADBAND_NONE) {
    UA_DataChangeFilter_init(&filter);
    filter.trigger = UA_DATACHANGETRIGGER_STATUSVALUE;
    filter.deadbandType = (UA_UInt32)deadband_type;
    filter.deadbandValue = (UA_Double)deadband_value;
    request.requestedParameters.filter.encoding = UA_EXTENSIONOBJECT_DECODED;
    request.requestedParameters.filter.content.decoded.type =
        &UA_TYPES[UA_TYPES_DATACHANGEFILTER];
    request.requestedParameters.filter.content.decoded.data = &filter;
  }
  result = UA_Client_MonitoredItems_createDataChange(
      client->client, (UA_UInt32)subscription_id, UA_TIMESTAMPSTORETURN_BOTH,
      request, context, cpkt_data_change_callback,
      cpkt_delete_monitored_item_callback);
  item_status = result.statusCode;
  monitored_item_id = result.monitoredItemId;
  if (status_out != NULL) {
    *status_out = cpkt_status(item_status);
  }
  UA_MonitoredItemCreateResult_clear(&result);
  if (item_status != UA_STATUSCODE_GOOD) {
    cpkt_monitor_context_free(context);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  context->monitored_item_id = (cpkt_opcua_monitored_item_id)monitored_item_id;
  context->next = client->monitors;
  client->monitors = context;
  if (monitored_item_id_out != NULL) {
    *monitored_item_id_out = context->monitored_item_id;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_monitor_events(
    cpkt_opcua_client *client, cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_node_id node_id, double sampling_interval_ms,
    cpkt_opcua_event_fn fn, void *user,
    cpkt_opcua_monitored_item_id *monitored_item_id_out,
    cpkt_opcua_status *status_out) {
  struct cpkt_opcua_monitor_context *context;
  UA_MonitoredItemCreateRequest request;
  UA_MonitoredItemCreateResult result;
  UA_NodeId native_node_id;
  UA_EventFilter filter;
  UA_SimpleAttributeOperand select_clauses[4];
  UA_QualifiedName browse_names[4];
  UA_StatusCode item_status;
  UA_UInt32 monitored_item_id;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (monitored_item_id_out != NULL) {
    *monitored_item_id_out = 0;
  }
  if (client == NULL || client->client == NULL || fn == NULL ||
      subscription_id > (unsigned long)UINT_MAX || sampling_interval_ms < 0.0 ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  context = (struct cpkt_opcua_monitor_context *)calloc(1, sizeof(*context));
  if (context == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->owner = client;
  context->subscription_id = subscription_id;
  context->event_fn = fn;
  context->user = user;

  native_node_id = cpkt_make_node_id(node_id);
  request = UA_MonitoredItemCreateRequest_default(native_node_id);
  request.itemToMonitor.attributeId = UA_ATTRIBUTEID_EVENTNOTIFIER;
  if (sampling_interval_ms > 0.0) {
    request.requestedParameters.samplingInterval = sampling_interval_ms;
  }
  UA_EventFilter_init(&filter);
  cpkt_init_event_select_clause(&select_clauses[0], &browse_names[0],
                                "EventId");
  cpkt_init_event_select_clause(&select_clauses[1], &browse_names[1],
                                "SourceName");
  cpkt_init_event_select_clause(&select_clauses[2], &browse_names[2],
                                "Message");
  cpkt_init_event_select_clause(&select_clauses[3], &browse_names[3],
                                "Severity");
  filter.selectClauses = select_clauses;
  filter.selectClausesSize = 4;
  request.requestedParameters.filter.encoding = UA_EXTENSIONOBJECT_DECODED;
  request.requestedParameters.filter.content.decoded.type =
      &UA_TYPES[UA_TYPES_EVENTFILTER];
  request.requestedParameters.filter.content.decoded.data = &filter;

  result = UA_Client_MonitoredItems_createEvent(
      client->client, (UA_UInt32)subscription_id, UA_TIMESTAMPSTORETURN_BOTH,
      request, context, cpkt_event_callback,
      cpkt_delete_monitored_item_callback);
  item_status = result.statusCode;
  monitored_item_id = result.monitoredItemId;
  if (status_out != NULL) {
    *status_out = cpkt_status(item_status);
  }
  UA_MonitoredItemCreateResult_clear(&result);
  if (item_status != UA_STATUSCODE_GOOD) {
    cpkt_monitor_context_free(context);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  context->monitored_item_id = (cpkt_opcua_monitored_item_id)monitored_item_id;
  context->next = client->monitors;
  client->monitors = context;
  if (monitored_item_id_out != NULL) {
    *monitored_item_id_out = context->monitored_item_id;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_monitor_event_fields(
    cpkt_opcua_client *client, cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_node_id node_id, double sampling_interval_ms,
    const cpkt_opcua_string_view *field_names, size_t field_count,
    cpkt_opcua_event_fields_fn fn, void *user,
    cpkt_opcua_monitored_item_id *monitored_item_id_out,
    cpkt_opcua_status *status_out) {
  struct cpkt_opcua_monitor_context *context;
  UA_MonitoredItemCreateRequest request;
  UA_MonitoredItemCreateResult result;
  UA_NodeId native_node_id;
  UA_EventFilter filter;
  UA_SimpleAttributeOperand *select_clauses;
  UA_QualifiedName *browse_names;
  UA_StatusCode item_status;
  UA_UInt32 monitored_item_id;
  size_t i;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (monitored_item_id_out != NULL) {
    *monitored_item_id_out = 0;
  }
  if (client == NULL || client->client == NULL || fn == NULL ||
      field_names == NULL || field_count == 0 ||
      subscription_id > (unsigned long)UINT_MAX || sampling_interval_ms < 0.0 ||
      !cpkt_valid_node_id(node_id)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  context = (struct cpkt_opcua_monitor_context *)calloc(1, sizeof(*context));
  if (context == NULL) {
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->event_field_names =
      (char **)calloc(field_count, sizeof(*context->event_field_names));
  if (context->event_field_names == NULL) {
    cpkt_monitor_context_free(context);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->event_field_count = field_count;
  for (i = 0; i < field_count; ++i) {
    if (field_names[i].data == NULL || field_names[i].length == 0) {
      cpkt_monitor_context_free(context);
      return CPKT_OPCUA_ERR_ARG;
    }
    context->event_field_names[i] =
        (char *)calloc(field_names[i].length + 1, 1);
    if (context->event_field_names[i] == NULL) {
      cpkt_monitor_context_free(context);
      return CPKT_OPCUA_ERR_ALLOC;
    }
    memcpy(context->event_field_names[i], field_names[i].data,
           field_names[i].length);
  }
  select_clauses =
      (UA_SimpleAttributeOperand *)calloc(field_count, sizeof(*select_clauses));
  browse_names = (UA_QualifiedName *)calloc(field_count, sizeof(*browse_names));
  if (select_clauses == NULL || browse_names == NULL) {
    free(select_clauses);
    free(browse_names);
    cpkt_monitor_context_free(context);
    return CPKT_OPCUA_ERR_ALLOC;
  }
  context->owner = client;
  context->subscription_id = subscription_id;
  context->event_fields_fn = fn;
  context->user = user;

  native_node_id = cpkt_make_node_id(node_id);
  request = UA_MonitoredItemCreateRequest_default(native_node_id);
  request.itemToMonitor.attributeId = UA_ATTRIBUTEID_EVENTNOTIFIER;
  if (sampling_interval_ms > 0.0) {
    request.requestedParameters.samplingInterval = sampling_interval_ms;
  }
  UA_EventFilter_init(&filter);
  for (i = 0; i < field_count; ++i) {
    cpkt_init_event_select_clause(&select_clauses[i], &browse_names[i],
                                  context->event_field_names[i]);
  }
  filter.selectClauses = select_clauses;
  filter.selectClausesSize = field_count;
  request.requestedParameters.filter.encoding = UA_EXTENSIONOBJECT_DECODED;
  request.requestedParameters.filter.content.decoded.type =
      &UA_TYPES[UA_TYPES_EVENTFILTER];
  request.requestedParameters.filter.content.decoded.data = &filter;

  result = UA_Client_MonitoredItems_createEvent(
      client->client, (UA_UInt32)subscription_id, UA_TIMESTAMPSTORETURN_BOTH,
      request, context, cpkt_event_fields_callback,
      cpkt_delete_monitored_item_callback);
  item_status = result.statusCode;
  monitored_item_id = result.monitoredItemId;
  if (status_out != NULL) {
    *status_out = cpkt_status(item_status);
  }
  UA_MonitoredItemCreateResult_clear(&result);
  free(select_clauses);
  free(browse_names);
  if (item_status != UA_STATUSCODE_GOOD) {
    cpkt_monitor_context_free(context);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  context->monitored_item_id = (cpkt_opcua_monitored_item_id)monitored_item_id;
  context->next = client->monitors;
  client->monitors = context;
  if (monitored_item_id_out != NULL) {
    *monitored_item_id_out = context->monitored_item_id;
  }
  return CPKT_OPCUA_OK;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_set_monitoring_mode(
    cpkt_opcua_client *client, cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_monitored_item_id monitored_item_id, int monitoring_mode,
    cpkt_opcua_status *status_out) {
  UA_SetMonitoringModeRequest request;
  UA_SetMonitoringModeResponse response;
  UA_UInt32 native_monitored_item_id;
  UA_StatusCode item_status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      subscription_id > (unsigned long)UINT_MAX ||
      monitored_item_id > (unsigned long)UINT_MAX ||
      (monitoring_mode != CPKT_OPCUA_MONITORING_DISABLED &&
       monitoring_mode != CPKT_OPCUA_MONITORING_SAMPLING &&
       monitoring_mode != CPKT_OPCUA_MONITORING_REPORTING)) {
    return CPKT_OPCUA_ERR_ARG;
  }
  native_monitored_item_id = (UA_UInt32)monitored_item_id;
  UA_SetMonitoringModeRequest_init(&request);
  request.subscriptionId = (UA_UInt32)subscription_id;
  request.monitoringMode = (UA_MonitoringMode)monitoring_mode;
  request.monitoredItemIdsSize = 1;
  request.monitoredItemIds = &native_monitored_item_id;
  response =
      UA_Client_MonitoredItems_setMonitoringMode(client->client, request);
  if (status_out != NULL) {
    *status_out = cpkt_status(response.responseHeader.serviceResult);
  }
  if (response.responseHeader.serviceResult != UA_STATUSCODE_GOOD) {
    UA_SetMonitoringModeResponse_clear(&response);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  if (response.resultsSize != 1 || response.results == NULL) {
    UA_SetMonitoringModeResponse_clear(&response);
    return CPKT_OPCUA_ERR_UPSTREAM;
  }
  item_status = response.results[0];
  if (status_out != NULL) {
    *status_out = cpkt_status(item_status);
  }
  UA_SetMonitoringModeResponse_clear(&response);
  return item_status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK
                                           : CPKT_OPCUA_ERR_UPSTREAM;
}

/** Implements the public OPC UA facade function declared in <cpkt/opcua.h>. */
cpkt_opcua_result cpkt_opcua_client_delete_monitored_item(
    cpkt_opcua_client *client, cpkt_opcua_subscription_id subscription_id,
    cpkt_opcua_monitored_item_id monitored_item_id,
    cpkt_opcua_status *status_out) {
  UA_StatusCode status;

  if (status_out != NULL) {
    *status_out = 0;
  }
  if (client == NULL || client->client == NULL ||
      subscription_id > (unsigned long)UINT_MAX ||
      monitored_item_id > (unsigned long)UINT_MAX) {
    return CPKT_OPCUA_ERR_ARG;
  }
  status = UA_Client_MonitoredItems_deleteSingle(
      client->client, (UA_UInt32)subscription_id, (UA_UInt32)monitored_item_id);
  if (status_out != NULL) {
    *status_out = cpkt_status(status);
  }
  return status == UA_STATUSCODE_GOOD ? CPKT_OPCUA_OK : CPKT_OPCUA_ERR_UPSTREAM;
}
