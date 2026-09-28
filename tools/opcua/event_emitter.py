"""Derive event descriptions and local notification signatures from server.h."""
import re
from node_emitter import record_body
from plugin_emitter import uncomment, translate


def emit_events(headers):
    text = uncomment((headers / 'server.h').read_text())
    body = record_body(text, 'EventDescription')
    expected = ['sourceNode', 'eventType', 'severity', 'message', 'eventFields',
                'eventInstance', 'sessionId', 'subscriptionId', 'monitoredItemId']
    if re.findall(r'\b(\w+)\s*;', body) != expected:
        raise ValueError('Adapt the complete public EventDescription')
    h = ['/** Complete synchronous event input. Pointer members borrow until return.',
         ' * Filtering by subscription requires sessionId; monitoredItem requires',
         ' * subscriptionId. Default field resolution remains native. */',
         'typedef struct {' + translate(body) + '} cpkt_opcua_EventDescription;']
    for kind in ('DataChange', 'Event'):
        match = re.search(r'typedef\s+void\s*\(\*UA_Server_' + kind + r'NotificationCallback\)\s*\((.*?)\)\s*;', text, re.S)
        if not match:
            raise ValueError('Adapt public local notification callback ' + kind)
        signature = translate(match[1])
        # Conversion can fail in a void native callback. Report it explicitly,
        # consistently with existing client notification bindings.
        last = 'const cpkt_opcua_DataValue *value' if kind == 'DataChange' else 'const cpkt_opcua_KeyValueMap eventFields'
        replacement = 'cpkt_opcua_StatusCode conversionStatus, ' + ('const cpkt_opcua_DataValue *value' if kind == 'DataChange' else 'const cpkt_opcua_KeyValueMap *eventFields')
        if last not in signature:
            raise ValueError('Adapt notification output ' + kind)
        signature = signature.replace(last, replacement)
        h += ['/** Native asynchronous local notification with original user/node contexts.',
              ' * conversionStatus reports facade conversion failures; the value/map is NULL',
              ' * on failure. Otherwise it borrows until return. Do not clear or retain it.',
              ' * Deleting this item inside its callback is supported. Serialize server calls.',
              ' * NULL callback disables user notification. Never destroy the server in a callback.',
              ' * No extra queue, sampling, or event filtering is introduced. */',
              f'typedef void (*cpkt_opcua_Server_{kind}NotificationCallback)({signature});']
    for name in ('createEvent', 'createEventEx', 'createDataChangeMonitoredItem',
                 'createEventMonitoredItem', 'createEventMonitoredItemEx'):
        match = re.search(r'(UA_StatusCode|UA_MonitoredItemCreateResult)\s+UA_EXPORT\s+UA_THREADSAFE\s+UA_Server_' + name + r'\s*\((.*?)\)\s*;', text, re.S)
        if not match:
            raise ValueError('Adapt public event operation ' + name)
        signature = translate(match[2])
        if match[1] == 'UA_MonitoredItemCreateResult':
            signature += ', cpkt_opcua_MonitoredItemCreateResult *response'
        h += ['/** Invoke the full native event/local monitoring operation.',
              ' * Inputs borrow during submission. Result outputs start empty and own data.',
              ' * Monitor callbacks/context outlive the item; delivery follows the native',
              ' * EventLoop. Inspect response->statusCode separately from boundary status.',
              ' * outEventId is optional; event field defaults and targeting remain native. */',
              f'cpkt_opcua_StatusCode cpkt_opcua_server_{name}_typed({signature});']
    match = re.search(r'typedef\s+void\s*\(\*UA_Server_ReverseConnectStateCallback\)\s*\((.*?)\)\s*;', text, re.S)
    if not match:
        raise ValueError('Adapt public reverse-connect callback')
    h += ['/** Native connection-state callback; full 64-bit handle and original context.',
          ' * Context borrows through the final CLOSED notification. Do not destroy the',
          ' * server in the callback. Native retry and close scheduling are preserved. */',
          'typedef void (*cpkt_opcua_Server_ReverseConnectStateCallback)(' + translate(match[1]) + ');']
    for name in ('addReverseConnect',):
        match = re.search(r'UA_StatusCode\s+UA_EXPORT\s+UA_Server_' + name + r'\s*\((.*?)\)\s*;', text, re.S)
        if not match:
            raise ValueError('Adapt public reverse-connect operation')
        h += ['/** Register with the native reverse-connect manager. URL borrows until return.',
              ' * handle is optional. A failed initial connection can still leave a native',
              ' * registration and output handle active for retries; explicitly remove it.',
              ' * State callbacks may run before this function returns. */',
              'cpkt_opcua_StatusCode cpkt_opcua_server_' + name + '_typed(' + translate(match[1]) + ');']
    match = re.search(r'typedef\s+void\s*\(\*UA_Server_registerServerCallback\)\s*\((.*?)\)\s*;', text, re.S)
    if not match:
        raise ValueError('Adapt public discovery callback')
    signature = translate(match[1]).replace('const cpkt_opcua_RegisteredServer *registeredServer', 'cpkt_opcua_StatusCode conversionStatus, const cpkt_opcua_RegisteredServer *registeredServer')
    h += ['/** Full native registered-server notification. The record borrows until return.',
          ' * On conversion failure conversionStatus is nonzero and the record is NULL.',
          ' * Replacing this hook inside its own callback is supported. */',
          'typedef void (*cpkt_opcua_Server_registerServerCallback)(' + signature + ');',
          '/** Replace discovery notification slots. NULL disables notification. Context',
          ' * remains caller-owned through the current callback and future dispatch. */',
          'cpkt_opcua_StatusCode cpkt_opcua_server_setRegisterServerCallback_typed(cpkt_opcua_server *server, cpkt_opcua_Server_registerServerCallback callback, void *data);']
    return h
