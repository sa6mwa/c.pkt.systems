#include "opcua_callbacks_test.h"
#include "opcua_types_peer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check(int ok, const char *expression, int line) {
  if (!ok) {
    fprintf(stderr, "callback line %d: %s\n", line, expression);
    abort();
  }
}
#define CHECK(expression) check(!!(expression), #expression, __LINE__)
typedef struct {
  unsigned int changes, events, deleted, completed, subscription_deleted;
  cpkt_opcua_UInt32 sub_id, item_id;
  void *expected_sub_context;
  cpkt_opcua_DataValue copy;
  cpkt_opcua_CreateMonitoredItemsResponse response;
} callback_state;
static void subscription_deleted(cpkt_opcua_client *client,
                                 cpkt_opcua_UInt32 id, void *user) {
  callback_state *state = (callback_state *)user;
  CHECK(client && id == state->sub_id);
  ++state->subscription_deleted;
}
static void monitored_deleted(cpkt_opcua_client *client, cpkt_opcua_UInt32 sub,
                              void *sub_context, cpkt_opcua_UInt32 id,
                              void *user) {
  callback_state *state = (callback_state *)user;
  CHECK(client && sub == state->sub_id &&
        sub_context == state->expected_sub_context);
  if (state->item_id)
    CHECK(id == state->item_id);
  else
    CHECK(id == 0);
  ++state->deleted;
}
static void changed(cpkt_opcua_client *client, cpkt_opcua_UInt32 sub,
                    void *sub_context, cpkt_opcua_UInt32 id, void *user,
                    cpkt_opcua_StatusCode status,
                    const cpkt_opcua_DataValue *value) {
  callback_state *state = (callback_state *)user;
  cpkt_opcua_Int64 *values;
  CHECK(client && sub == state->sub_id &&
        sub_context == state->expected_sub_context);
  CHECK(id && !status && value && value->hasValue && value->hasSourceTimestamp);
  CHECK(value->value.type == cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64));
  CHECK(value->value.arrayLength == 4);
  values = (cpkt_opcua_Int64 *)value->value.data;
  CHECK(values[0].high32 == 0x80000000U && values[0].low32 == 0);
  CHECK(values[1].high32 == 0x7fffffffU && values[1].low32 == 0xffffffffU);
  cpkt_opcua_DataValue_clear(&state->copy);
  CHECK(cpkt_opcua_DataValue_copy(value, &state->copy) == 0);
  ++state->changes;
}
static void event_received(cpkt_opcua_client *client, cpkt_opcua_UInt32 sub,
                           void *sub_context, cpkt_opcua_UInt32 id, void *user,
                           cpkt_opcua_StatusCode status,
                           const cpkt_opcua_KeyValueMap *fields) {
  callback_state *state = (callback_state *)user;
  cpkt_opcua_LocalizedText *message;
  CHECK(client && sub == state->sub_id &&
        sub_context == state->expected_sub_context);
  CHECK(id && !status && fields && fields->mapSize == 4);
  CHECK(fields->map[0].key.name.length != 0);
  CHECK(fields->map[0].value.type ==
        cpkt_opcua_type_at(CPKT_OPCUA_TYPES_LOCALIZEDTEXT));
  message = (cpkt_opcua_LocalizedText *)fields->map[0].value.data;
  CHECK(message->text.length == 11 &&
        memcmp(message->text.data, "typed event", 11) == 0);
  CHECK(fields->map[1].value.type ==
        cpkt_opcua_type_at(CPKT_OPCUA_TYPES_UINT16));
  CHECK(*(cpkt_opcua_UInt16 *)fields->map[1].value.data == 321);
  CHECK(fields->map[2].value.type ==
        cpkt_opcua_type_at(CPKT_OPCUA_TYPES_DATETIME));
  CHECK(((cpkt_opcua_DateTime *)fields->map[2].value.data)->high32 != 0);
  CHECK(fields->map[3].value.type ==
        cpkt_opcua_type_at(CPKT_OPCUA_TYPES_BYTESTRING));
  ++state->events;
}
static void items_created(cpkt_opcua_client *client, void *user,
                          cpkt_opcua_UInt32 id, cpkt_opcua_StatusCode status,
                          const void *response, const cpkt_opcua_Type *type) {
  callback_state *state = (callback_state *)user;
  const cpkt_opcua_CreateMonitoredItemsResponse *result =
      (const cpkt_opcua_CreateMonitoredItemsResponse *)response;
  CHECK(client && id && !status && result);
  CHECK(type ==
        cpkt_opcua_type_at(CPKT_OPCUA_TYPES_CREATEMONITOREDITEMSRESPONSE));
  CHECK(!result->responseHeader.serviceResult && result->resultsSize == 1 &&
        !result->results[0].statusCode);
  CHECK(cpkt_opcua_CreateMonitoredItemsResponse_copy(result,
                                                     &state->response) == 0);
  state->item_id = result->results[0].monitoredItemId;
  ++state->completed;
}
static void
items_deleted(cpkt_opcua_client *client, void *user, cpkt_opcua_UInt32 id,
              cpkt_opcua_StatusCode status,
              const cpkt_opcua_DeleteMonitoredItemsResponse *response) {
  callback_state *state = (callback_state *)user;
  CHECK(client && id && !status && response &&
        !response->responseHeader.serviceResult);
  CHECK(response->resultsSize == 1 && !response->results[0]);
  ++state->completed;
}
static void
subscription_created(cpkt_opcua_client *client, void *user,
                     cpkt_opcua_UInt32 id, cpkt_opcua_StatusCode status,
                     cpkt_opcua_UInt32 subscription_id,
                     const cpkt_opcua_CreateSubscriptionResponse *response) {
  callback_state *state = (callback_state *)user;
  const cpkt_opcua_CreateSubscriptionResponse *result =
      (const cpkt_opcua_CreateSubscriptionResponse *)response;
  CHECK(client && id && !status && result &&
        !result->responseHeader.serviceResult);
  CHECK(subscription_id && subscription_id == result->subscriptionId);
  state->sub_id = result->subscriptionId;
  ++state->completed;
}
void cpkt_types_test_callbacks(cpkt_opcua_client *client, void *peer) {
  callback_state state, failed, events, second;
  cpkt_opcua_CreateSubscriptionRequest subscription;
  cpkt_opcua_CreateSubscriptionResponse subscription_response;
  cpkt_opcua_CreateMonitoredItemsRequest request;
  cpkt_opcua_CreateMonitoredItemsResponse response;
  cpkt_opcua_MonitoredItemCreateRequest items[2];
  cpkt_opcua_MonitoredItemCallbacks registrations[2];
  cpkt_opcua_EventFilter filter;
  cpkt_opcua_SimpleAttributeOperand operands[4];
  cpkt_opcua_QualifiedName names[4];
  cpkt_opcua_ModifySubscriptionRequest modify;
  cpkt_opcua_ModifySubscriptionResponse modified;
  cpkt_opcua_DeleteMonitoredItemsRequest deletion;
  cpkt_opcua_UInt32 id;
  const char *field_names[4] = {"Message", "Severity", "Time", "EventId"};
  void *context;
  size_t i;
  memset(&state, 0, sizeof(state));
  memset(&failed, 0, sizeof(failed));
  memset(&events, 0, sizeof(events));
  memset(&second, 0, sizeof(second));
  cpkt_opcua_CreateSubscriptionRequest_init(&subscription);
  subscription.requestedPublishingInterval = 10;
  subscription.requestedLifetimeCount = 1000;
  subscription.requestedMaxKeepAliveCount = 10;
  subscription.publishingEnabled = 1;
  CHECK(cpkt_opcua_client_subscription_create_typed(
            client, &subscription, &state, NULL, subscription_deleted,
            &subscription_response) == 0);
  CHECK(subscription_response.responseHeader.serviceResult == 0 &&
        subscription_response.subscriptionId);
  state.sub_id = subscription_response.subscriptionId;
  cpkt_opcua_CreateSubscriptionResponse_clear(&subscription_response);
  CHECK(cpkt_opcua_client_subscription_get_context(client, state.sub_id,
                                                   &context) == 0 &&
        context == &state);
  CHECK(cpkt_opcua_client_subscription_set_context(client, state.sub_id,
                                                   &second) == 0);
  CHECK(cpkt_opcua_client_subscription_get_context(client, state.sub_id,
                                                   &context) == 0 &&
        context == &second);
  CHECK(cpkt_opcua_client_subscription_set_context(client, state.sub_id,
                                                   &state) == 0);
  state.expected_sub_context = &state;
  failed.sub_id = state.sub_id;
  failed.expected_sub_context = &state;
  events.sub_id = state.sub_id;
  events.expected_sub_context = &state;
  memset(registrations, 0, sizeof(registrations));
  registrations[0].context = &state;
  registrations[0].data_change = changed;
  registrations[0].deleted = monitored_deleted;
  registrations[1].context = &failed;
  registrations[1].deleted = monitored_deleted;
  cpkt_opcua_CreateMonitoredItemsRequest_init(&request);
  request.subscriptionId = state.sub_id;
  request.timestampsToReturn = cpkt_opcua_TIMESTAMPSTORETURN_BOTH;
  request.itemsToCreate = items;
  request.itemsToCreateSize = 2;
  for (i = 0; i < 2; ++i) {
    cpkt_opcua_MonitoredItemCreateRequest_init(&items[i]);
    items[i].itemToMonitor.nodeId.namespaceIndex = 1;
    items[i].itemToMonitor.nodeId.identifier.numeric =
        i == 0 ? 6001 : 0xffffffffU;
    items[i].itemToMonitor.attributeId = 13;
    items[i].monitoringMode = cpkt_opcua_MONITORINGMODE_REPORTING;
    items[i].requestedParameters.samplingInterval = 10;
    items[i].requestedParameters.queueSize = 1;
    items[i].requestedParameters.discardOldest = 1;
  }
  CHECK(cpkt_opcua_client_monitored_items_create_typed(
            client, &request, 0, registrations, &response) == 0);
  CHECK(response.responseHeader.serviceResult == 0 &&
        response.resultsSize == 2);
  CHECK(response.results[0].statusCode == 0 &&
        response.results[1].statusCode != 0);
  CHECK(failed.deleted == 1 && state.deleted == 0);
  state.item_id = response.results[0].monitoredItemId;
  cpkt_opcua_CreateMonitoredItemsResponse_clear(&response);
  CHECK(cpkt_opcua_client_monitored_item_get_context(
            client, state.sub_id, state.item_id, &context) == 0 &&
        context == &state);
  CHECK(cpkt_opcua_client_monitored_item_set_context(
            client, state.sub_id, state.item_id, &second) == 0);
  CHECK(cpkt_opcua_client_monitored_item_get_context(
            client, state.sub_id, state.item_id, &context) == 0 &&
        context == &second);
  CHECK(cpkt_opcua_client_monitored_item_set_context(
            client, state.sub_id, state.item_id, &state) == 0);
  for (i = 0; i < 200 && !state.changes; ++i)
    CHECK(cpkt_opcua_client_run_iterate(client, 10, NULL) == CPKT_OPCUA_OK);
  CHECK(state.changes && state.copy.hasValue && state.copy.value.data);
  cpkt_opcua_ModifySubscriptionRequest_init(&modify);
  modify.subscriptionId = state.sub_id;
  modify.requestedPublishingInterval = 15;
  modify.requestedLifetimeCount = 1000;
  modify.requestedMaxKeepAliveCount = 10;
  CHECK(cpkt_opcua_client_service_Subscriptions_modify(client, &modify,
                                                       &modified) == 0);
  CHECK(!modified.responseHeader.serviceResult &&
        modified.revisedPublishingInterval >= 15);
  cpkt_opcua_ModifySubscriptionResponse_clear(&modified);
  cpkt_opcua_EventFilter_init(&filter);
  filter.selectClausesSize = 4;
  filter.selectClauses = operands;
  for (i = 0; i < 4; ++i) {
    cpkt_opcua_SimpleAttributeOperand_init(&operands[i]);
    cpkt_opcua_QualifiedName_init(&names[i]);
    names[i].name.length = strlen(field_names[i]);
    names[i].name.data = (cpkt_opcua_Byte *)field_names[i];
    operands[i].typeDefinitionId.identifier.numeric = 2041;
    operands[i].attributeId = 13;
    operands[i].browsePathSize = 1;
    operands[i].browsePath = &names[i];
  }
  request.itemsToCreateSize = 1;
  items[0].itemToMonitor.nodeId.namespaceIndex = 0;
  items[0].itemToMonitor.nodeId.identifier.numeric = 2253;
  items[0].itemToMonitor.attributeId = 12;
  items[0].requestedParameters.filter.encoding =
      CPKT_OPCUA_EXTENSIONOBJECT_DECODED_NODELETE;
  items[0].requestedParameters.filter.content.decoded.type =
      cpkt_opcua_type_at(CPKT_OPCUA_TYPES_EVENTFILTER);
  items[0].requestedParameters.filter.content.decoded.data = &filter;
  registrations[0].context = &events;
  registrations[0].data_change = NULL;
  registrations[0].event = event_received;
  CHECK(cpkt_opcua_client_monitored_items_create_typed_async(
            client, &request, 1, registrations, items_created, &events, &id) ==
            0 &&
        id);
  /* Prove nested filter and callback registration records are no longer
   * borrowed. */
  memset(operands, 0, sizeof(operands));
  memset(registrations, 0, sizeof(registrations));
  for (i = 0; i < 200 && !events.completed; ++i)
    CHECK(cpkt_opcua_client_run_iterate(client, 10, NULL) == CPKT_OPCUA_OK);
  CHECK(events.completed == 1 && events.item_id);
  CHECK(cpkt_types_peer_event(peer) == 0);
  for (i = 0; i < 200 && !events.events; ++i)
    CHECK(cpkt_opcua_client_run_iterate(client, 10, NULL) == CPKT_OPCUA_OK);
  CHECK(events.events == 1);
  cpkt_opcua_DeleteMonitoredItemsRequest_init(&deletion);
  deletion.subscriptionId = state.sub_id;
  deletion.monitoredItemIdsSize = 1;
  deletion.monitoredItemIds = &state.item_id;
  CHECK(cpkt_opcua_client_service_MonitoredItems_delete_async(
            client, &deletion, items_deleted, &state, &id) == 0);
  for (i = 0; i < 200 && !state.completed; ++i)
    CHECK(cpkt_opcua_client_run_iterate(client, 10, NULL) == CPKT_OPCUA_OK);
  CHECK(state.completed == 1 && state.deleted == 1);
  CHECK(cpkt_opcua_client_monitored_item_delete_single(client, events.sub_id,
                                                       events.item_id) == 0);
  CHECK(events.deleted == 1);
  CHECK(cpkt_opcua_client_subscription_delete_single(client, state.sub_id) ==
        0);
  CHECK(state.subscription_deleted == 1);
  CHECK(cpkt_opcua_client_subscription_get_context(client, state.sub_id,
                                                   &context) != 0 &&
        context == NULL);
  CHECK(cpkt_opcua_client_subscription_create_typed_async(
            client, &subscription, &second, NULL, subscription_deleted,
            subscription_created, &second, &id) == 0);
  for (i = 0; i < 200 && !second.completed; ++i)
    CHECK(cpkt_opcua_client_run_iterate(client, 10, NULL) == CPKT_OPCUA_OK);
  CHECK(second.completed == 1 && second.sub_id);
  CHECK(cpkt_opcua_client_subscription_delete_single(client, second.sub_id) ==
        0);
  CHECK(second.subscription_deleted == 1);
  cpkt_opcua_DataValue_clear(&state.copy);
  cpkt_opcua_CreateMonitoredItemsResponse_clear(&events.response);
}
