#ifndef CPKT_OPCUA_CALLBACKS_H
#define CPKT_OPCUA_CALLBACKS_H
#include <cpkt/opcua_types.h>
#ifdef __cplusplus
extern "C" {
#endif

/** Same fields as upstream's event/configuration map. Entries are full
 * generated KeyValuePairs. Notification storage is borrowed until callback
 * return. */
struct cpkt_opcua_KeyValueMap {
  size_t mapSize;
  cpkt_opcua_KeyValuePair *map;
};
/** Receives the complete status notification. Nonzero conversion status means
 * notification is NULL. Context is exactly the caller's subscription context.
 */
typedef void (*cpkt_opcua_subscription_status_fn)(
    cpkt_opcua_client *client, cpkt_opcua_UInt32 subscription_id, void *context,
    cpkt_opcua_StatusCode conversion_status,
    const cpkt_opcua_StatusChangeNotification *notification);
/** Runs when upstream deletes local subscription state, including destruction.
 * The caller owns context and may release it here. */
typedef void (*cpkt_opcua_subscription_delete_fn)(
    cpkt_opcua_client *client, cpkt_opcua_UInt32 subscription_id,
    void *context);
/** Subscription creation completion. Native subscription_id is returned even
 * when response conversion fails, so a created subscription can be removed.
 * A nonzero conversion status yields NULL response. Service rejection yields
 * subscription_id zero. The caller owns both user and subscription context. */
typedef void (*cpkt_opcua_subscription_created_fn)(
    cpkt_opcua_client *client, void *user, cpkt_opcua_UInt32 request_id,
    cpkt_opcua_StatusCode conversion_status, cpkt_opcua_UInt32 subscription_id,
    const cpkt_opcua_CreateSubscriptionResponse *response);
/** Create a subscription with the complete upstream request/response model.
 * Output starts empty and owns nested data. Function return is conversion
 * status; inspect responseHeader.serviceResult for upstream status. Calls on
 * one facade handle must be serialized. Context is borrowed until upstream
 * deletion. */
cpkt_opcua_StatusCode cpkt_opcua_client_subscription_create_typed(
    cpkt_opcua_client *client,
    const cpkt_opcua_CreateSubscriptionRequest *request, void *context,
    cpkt_opcua_subscription_status_fn status_fn,
    cpkt_opcua_subscription_delete_fn delete_fn,
    cpkt_opcua_CreateSubscriptionResponse *response);
/** Submit subscription creation using upstream's specialized async operation.
 * The request is borrowed during submission only. Context must survive until
 * completion on failure, or subscription deletion on success. Completion and
 * status notifications borrow converted records; copy them to retain them. */
cpkt_opcua_StatusCode cpkt_opcua_client_subscription_create_typed_async(
    cpkt_opcua_client *client,
    const cpkt_opcua_CreateSubscriptionRequest *request, void *context,
    cpkt_opcua_subscription_status_fn status_fn,
    cpkt_opcua_subscription_delete_fn delete_fn,
    cpkt_opcua_subscription_created_fn complete_fn, void *user,
    cpkt_opcua_UInt32 *request_id);
/** Retrieve the original caller context, without exposing bridge bookkeeping.
 * Also works with subscriptions created by the convenience/native interface. */
cpkt_opcua_StatusCode
cpkt_opcua_client_subscription_get_context(cpkt_opcua_client *client,
                                           cpkt_opcua_UInt32 subscription_id,
                                           void **context);
/** Update caller context. Native callback registration remains intact. */
cpkt_opcua_StatusCode
cpkt_opcua_client_subscription_set_context(cpkt_opcua_client *client,
                                           cpkt_opcua_UInt32 subscription_id,
                                           void *context);
/** Delete one subscription through upstream, including its monitored items. */
cpkt_opcua_StatusCode
cpkt_opcua_client_subscription_delete_single(cpkt_opcua_client *client,
                                             cpkt_opcua_UInt32 subscription_id);
/** Receives every DataValue field and arbitrary generated nested value. Data is
 * borrowed until return. Nonzero conversion status yields NULL value. */
typedef void (*cpkt_opcua_data_change_typed_fn)(
    cpkt_opcua_client *client, cpkt_opcua_UInt32 subscription_id,
    void *subscription_context, cpkt_opcua_UInt32 monitored_item_id,
    void *context, cpkt_opcua_StatusCode conversion_status,
    const cpkt_opcua_DataValue *value);
/** Receives the full upstream event field map (names plus arbitrary Variants).
 * Nonzero conversion status yields NULL fields; success borrows fields until
 * return. No fixed event-field subset or scalar restriction. */
typedef void (*cpkt_opcua_event_typed_fn)(
    cpkt_opcua_client *client, cpkt_opcua_UInt32 subscription_id,
    void *subscription_context, cpkt_opcua_UInt32 monitored_item_id,
    void *context, cpkt_opcua_StatusCode conversion_status,
    const cpkt_opcua_KeyValueMap *fields);
/** Mirrors native deletion callbacks, including native creation failures where
 * item ID is zero. Caller context remains caller-owned. */
typedef void (*cpkt_opcua_monitored_delete_fn)(
    cpkt_opcua_client *client, cpkt_opcua_UInt32 subscription_id,
    void *subscription_context, cpkt_opcua_UInt32 monitored_item_id,
    void *context);
/** Per-item callback registration, borrowed during submission only. Context is
 * borrowed until the item's native deletion callback. Choose data_change for
 * data monitoring, event for event monitoring. */
typedef struct {
  void *context;
  cpkt_opcua_data_change_typed_fn data_change;
  cpkt_opcua_event_typed_fn event;
  cpkt_opcua_monitored_delete_fn deleted;
} cpkt_opcua_MonitoredItemCallbacks;
/** Create data or event monitored items with the complete request/response.
 * event_monitoring chooses upstream createEvents versus createDataChanges;
 * registrations has exactly itemsToCreateSize entries. Requests and
 * registration records are borrowed during the call only. Output owns its
 * nested results. Partial creation and notification-before-response follow
 * native semantics. Subscription may originate from any facade interface. If
 * response conversion fails, native creation may have succeeded: delete the
 * known subscription to release those items and receive deletion callbacks
 * before freeing contexts. */
cpkt_opcua_StatusCode cpkt_opcua_client_monitored_items_create_typed(
    cpkt_opcua_client *client,
    const cpkt_opcua_CreateMonitoredItemsRequest *request, int event_monitoring,
    const cpkt_opcua_MonitoredItemCallbacks *registrations,
    cpkt_opcua_CreateMonitoredItemsResponse *response);
/** Asynchronous counterpart: invokes upstream's specialized create operation,
 * preserving its local monitored-item state and notification ordering. */
cpkt_opcua_StatusCode cpkt_opcua_client_monitored_items_create_typed_async(
    cpkt_opcua_client *client,
    const cpkt_opcua_CreateMonitoredItemsRequest *request, int event_monitoring,
    const cpkt_opcua_MonitoredItemCallbacks *registrations,
    cpkt_opcua_async_service_fn complete_fn, void *user,
    cpkt_opcua_UInt32 *request_id);
/** Retrieve the original caller monitored-item context. */
cpkt_opcua_StatusCode cpkt_opcua_client_monitored_item_get_context(
    cpkt_opcua_client *client, cpkt_opcua_UInt32 subscription_id,
    cpkt_opcua_UInt32 monitored_item_id, void **context);
/** Update caller context while preserving the native callback bridge. */
cpkt_opcua_StatusCode cpkt_opcua_client_monitored_item_set_context(
    cpkt_opcua_client *client, cpkt_opcua_UInt32 subscription_id,
    cpkt_opcua_UInt32 monitored_item_id, void *context);
/** Delete one monitored item through the native client state machine. */
cpkt_opcua_StatusCode cpkt_opcua_client_monitored_item_delete_single(
    cpkt_opcua_client *client, cpkt_opcua_UInt32 subscription_id,
    cpkt_opcua_UInt32 monitored_item_id);

/** Native server timer callback. Context is borrowed until execution of a
 * one-shot timer, removal of a repeated timer, or server destruction. Calls on
 * a handle must be serialized. A callback may remove itself or other timers;
 * it must not destroy the server while a callback is running. */
typedef void (*cpkt_opcua_ServerCallback)(cpkt_opcua_server *server,
                                          void *data);
/** Schedule with the native EventLoop, without a facade queue. date uses the
 * upstream DateTime epoch/units. callback_id is optional; on failure it is
 * zero. A past date executes on the next native iteration. */
cpkt_opcua_StatusCode cpkt_opcua_server_addTimedCallback_typed(
    cpkt_opcua_server *server, cpkt_opcua_ServerCallback callback, void *data,
    cpkt_opcua_DateTime date, cpkt_opcua_UInt64 *callback_id);
/** Native repeated timer. interval_ms must be positive. callback_id is
 * optional. Context remains caller-owned and borrowed until removal or server
 * destruction. The native scheduler controls timing and missed executions. */
cpkt_opcua_StatusCode cpkt_opcua_server_addRepeatedCallback_typed(
    cpkt_opcua_server *server, cpkt_opcua_ServerCallback callback, void *data,
    cpkt_opcua_Double interval_ms, cpkt_opcua_UInt64 *callback_id);
/** Change the interval of a native callback ID, including a callback not
 * originally installed through the typed facade. Native validation applies.
 * A one-shot timer becomes repeated on success, also when changed inside its
 * callback; its context is then borrowed until removal or destruction. */
cpkt_opcua_StatusCode cpkt_opcua_server_changeRepeatedCallbackInterval_typed(
    cpkt_opcua_server *server, cpkt_opcua_UInt64 callback_id,
    cpkt_opcua_Double interval_ms);
/** Remove a native callback; an unknown ID is a no-op. NULL server is a no-op.
 * A callback already running retains its borrowed context until it returns. */
void cpkt_opcua_server_removeCallback_typed(cpkt_opcua_server *server,
                                            cpkt_opcua_UInt64 callback_id);
/** Synchronous native child-node visitor. Node identifiers and reference type
 * are borrowed only during each call; copy them to retain them. A nonzero
 * return stops traversal with that status, as in the upstream iterator. */
typedef cpkt_opcua_StatusCode (*cpkt_opcua_NodeIteratorCallback)(
    cpkt_opcua_NodeId child_id, cpkt_opcua_Boolean is_inverse,
    cpkt_opcua_NodeId reference_type_id, void *handle);
/** Iterate native references without collecting them into a facade array.
 * Context is borrowed until return. Conversion failures stop traversal and
 * return their status. Reference ordering and inverse flags are native. */
cpkt_opcua_StatusCode cpkt_opcua_server_forEachChildNodeCall_typed(
    cpkt_opcua_server *server, cpkt_opcua_NodeId parent,
    cpkt_opcua_NodeIteratorCallback callback, void *handle);

/** Native local read completion. Context is the original caller pointer.
 * A nonzero conversion_status yields NULL result; otherwise all DataValue
 * fields are borrowed until return. Native operation errors remain in result.
 * Copy the result to retain it. The callback may run during submission, an
 * EventLoop iteration, cancellation, or server destruction, exactly once for
 * an accepted operation. Do not destroy the server from a callback. */
typedef void (*cpkt_opcua_ServerAsyncReadResultCallback)(
    cpkt_opcua_server *server, void *context,
    cpkt_opcua_StatusCode conversion_status,
    const cpkt_opcua_DataValue *result);
/** Native local write completion; result is the native operation status. */
typedef void (*cpkt_opcua_ServerAsyncWriteResultCallback)(
    cpkt_opcua_server *server, void *context, cpkt_opcua_StatusCode result);
/** Native local method completion. The complete result is borrowed until
 * return. Nonzero conversion_status yields NULL result; native method errors
 * remain in result->statusCode. The caller retains ownership of context. */
typedef void (*cpkt_opcua_ServerAsyncMethodResultCallback)(
    cpkt_opcua_server *server, void *context,
    cpkt_opcua_StatusCode conversion_status,
    const cpkt_opcua_CallMethodResult *result);
/** Submit directly to UA_Server_read_async. Operation storage is borrowed
 * during submission only. Context must survive completion, which can run
 * before this function returns. Submission failure invokes no completion.
 * timeout_ms is native milliseconds; zero is infinite. Serialize calls on
 * one server handle, including EventLoop iterations. Submitting during server
 * destruction returns BadShutdown without invoking a callback. */
cpkt_opcua_StatusCode cpkt_opcua_server_read_async_typed(
    cpkt_opcua_server *server, const cpkt_opcua_ReadValueId *operation,
    cpkt_opcua_TimestampsToReturn timestamps,
    cpkt_opcua_ServerAsyncReadResultCallback callback, void *context,
    cpkt_opcua_UInt32 timeout_ms);
/** Local write with the same submission/context contract as read_async_typed.
 * Open62541 owns asynchronous bookkeeping; the facade adds no result queue. */
cpkt_opcua_StatusCode cpkt_opcua_server_write_async_typed(
    cpkt_opcua_server *server, const cpkt_opcua_WriteValue *operation,
    cpkt_opcua_ServerAsyncWriteResultCallback callback, void *context,
    cpkt_opcua_UInt32 timeout_ms);
/** Local method call with full Variant inputs and output arguments. Request
 * storage is borrowed during submission only; completion borrows its result. */
cpkt_opcua_StatusCode cpkt_opcua_server_call_async_typed(
    cpkt_opcua_server *server, const cpkt_opcua_CallMethodRequest *operation,
    cpkt_opcua_ServerAsyncMethodResultCallback callback, void *context,
    cpkt_opcua_UInt32 timeout_ms);
/** Cancel operations using the original caller context, not bridge pointers.
 * Cancels the matching operations registered when cancellation begins and
 * also forwards to native operations installed outside the typed facade.
 * synchronous_result_callback requests immediate native result callbacks;
 * otherwise drive the EventLoop until completion before releasing context.
 * Already-ready results retain their native status. NULL server is harmless.
 * Callbacks may submit new operations; those require their own cancellation. */
void cpkt_opcua_server_cancelAsync_typed(
    cpkt_opcua_server *server, void *context, cpkt_opcua_StatusCode status,
    cpkt_opcua_Boolean synchronous_result_callback);

#ifdef __cplusplus
}
#endif
#endif
