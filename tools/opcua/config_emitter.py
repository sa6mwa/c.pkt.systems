"""Derive configuration values and stock helpers from public declarations.

The native configuration remains the actual backing. Schema values are converted
by the shared engine; plugins retain independent native lifetimes. Field and
callback classification fails closed when upstream adds an unhandled surface.
"""
import re
from plugin_emitter import public_body, uncomment, callbacks, CALLBACK_PATTERN, parameters, translate


SPECIAL = {
    'Client': {'logging', 'eventLoop', 'externalEventLoop', 'customDataTypes',
               'securityPolicies', 'securityPoliciesSize', 'authSecurityPolicies',
               'authSecurityPoliciesSize', 'certificateVerification',
               'globalNotificationCallback', 'lifecycleNotificationCallback', 'serviceNotificationCallback'},
    'Server': {'logging', 'eventLoop', 'externalEventLoop', 'customDataTypes',
               'securityPolicies', 'securityPoliciesSize', 'secureChannelPKI', 'sessionPKI',
               'accessControl', 'nodestore', 'nodeLifecycle', 'pubSubConfig', 'historyDatabase',
               'globalNotificationCallback', 'lifecycleNotificationCallback',
               'secureChannelNotificationCallback', 'sessionNotificationCallback',
               'serviceNotificationCallback', 'subscriptionNotificationCallback'},
}
CALLBACKS = {
    'Client': {'privateKeyPasswordCallback', 'stateCallback', 'inactivityCallback', 'subscriptionInactivityCallback'},
    'Server': {'notifyLifecycleState', 'asyncOperationCancelCallback', 'monitoredItemRegisterCallback', 'privateKeyPasswordCallback'},
}
SCALARS = {'Boolean', 'Byte', 'UInt16', 'UInt32', 'Double', 'Duration', 'MessageSecurityMode', 'RuleHandling'}
RECORDS = {'DurationRange': ('min', 'max'), 'UInt32Range': ('min', 'max'),
           'ConnectionConfig': ('protocolVersion', 'recvBufferSize', 'sendBufferSize', 'localMaxMessageSize',
                                'remoteMaxMessageSize', 'localMaxChunkCount', 'remoteMaxChunkCount')}


def emit_configuration_callbacks(kind, body, index):
    name = kind + 'Config'
    lower = kind.lower()
    notification = f'cpkt_opcua_{kind}NotificationCallback'
    header = ['/** Complete native notification callback with an explicit C89 conversion',
              ' * status. Payload borrows until return; NULL marks conversion failure.',
              ' * Never destroy the handle or its configuration during dispatch. */',
              f'typedef void (*{notification})(cpkt_opcua_{lower} *handle, cpkt_opcua_ApplicationNotificationType type, cpkt_opcua_StatusCode conversionStatus, const cpkt_opcua_KeyValueMap *payload);',
              '/** Complete configuration callback slots. Arguments borrow until return.',
              ' * Notification maps are C89 schema copies with explicit conversionStatus;',
              ' * NULL payload marks conversion failure. Context pointers remain original.',
              ' * Password output starts empty and owns C89 bytes; the facade consumes it',
              ' * after callback return, converting native owned bytes on Good. No callback',
              ' * may destroy its configuration or handle. NULL slots disable notification. */',
              'typedef struct {']
    impl, assignments, checks = [], [], []
    for result, slot, signature in callbacks(body):
        args = parameters(signature)
        public = translate(signature).replace('cpkt_opcua_Client *', 'cpkt_opcua_client *')
        validation = {
            'privateKeyPasswordCallback': [f'UA_{name} *', 'UA_ByteString *'],
            'stateCallback': ['UA_Client *', 'UA_SecureChannelState', 'UA_SessionState', 'UA_StatusCode'],
            'inactivityCallback': ['UA_Client *'],
            'subscriptionInactivityCallback': ['UA_Client *', 'UA_UInt32', 'void *'],
            'notifyLifecycleState': ['UA_Server *', 'UA_LifecycleState'],
            'asyncOperationCancelCallback': ['UA_Server *', 'const void *'],
            'monitoredItemRegisterCallback': ['UA_Server *', 'const UA_NodeId *', 'void *',
                                             'const UA_NodeId *', 'void *', 'UA_UInt32', 'UA_Boolean'],
        }
        if [spelling for spelling, _ in args] != validation[slot]:
            raise ValueError('Adapt configuration callback signature: ' + name + '.' + slot)
        if slot == 'monitoredItemRegisterCallback':
            public += ', cpkt_opcua_StatusCode conversionStatus'
        header += [f'  {translate(result)} (*{slot})({public});']
        proxy = 'cpkt_cfg_' + lower + '_' + slot
        first = args[0][1]
        lookup = first if slot == 'privateKeyPasswordCallback' else f'UA_{kind}_getConfig({first})'
        impl += [f'static {result} {proxy}({signature}) {{',
                 f'  cpkt_opcua_{name} *config = cpkt_cfg_{lower}_find({lookup});']
        if slot == 'privateKeyPasswordCallback':
            impl += ['  cpkt_opcua_ByteString value;', '  UA_ByteString staged;', '  UA_StatusCode status;',
                     '  if(!config || !config->callbacks.privateKeyPasswordCallback || !password) return UA_STATUSCODE_BADINVALIDSTATE;',
                     '  memset(&value, 0, sizeof(value)); memset(&staged, 0, sizeof(staged));',
                     f'  cpkt_cfg_{lower}_enter(config);',
                     '  status = config->callbacks.privateKeyPasswordCallback(config, &value);',
                     f'  if(!status) status = cpkt_convert(&value, &staged, &cpkt_types[{index["ByteString"]}], 1, 0);',
                     '  cpkt_opcua_ByteString_clear(&value);',
                     f'  cpkt_cfg_{lower}_leave(config);',
                     '  if(!status) *password = staged; else UA_ByteString_clear(&staged);', '  return status;']
        elif slot == 'monitoredItemRegisterCallback':
            impl += ['  cpkt_opcua_NodeId session, node;', '  UA_StatusCode status = 0;',
                     f'  void (*callback)({public});',
                     '  if(!config || !config->owner || !config->callbacks.monitoredItemRegisterCallback) return;',
                     '  cpkt_cfg_bind_server_owner(config->owner, server);',
                     '  callback = config->callbacks.monitoredItemRegisterCallback;',
                     '  memset(&session, 0, sizeof(session)); memset(&node, 0, sizeof(node));',
                     '  cpkt_cfg_server_enter(config);',
                     f'  if(sessionId) status = cpkt_convert(sessionId, &session, &cpkt_types[{index["NodeId"]}], 0, 0);',
                     f'  if(!status && nodeId) status = cpkt_convert(nodeId, &node, &cpkt_types[{index["NodeId"]}], 0, 0);',
                     '  callback(config->owner, !status && sessionId ? &session : NULL, sessionContext,',
                     '      !status && nodeId ? &node : NULL, nodeContext, attibuteId, removed, status);',
                     '  cpkt_opcua_NodeId_clear(&node); cpkt_opcua_NodeId_clear(&session);',
                     '  cpkt_cfg_server_leave(config);']
        else:
            call = ['config->owner']
            for spelling, field in args[1:]:
                call += [f'({translate(spelling)}){field}' if spelling in ('UA_LifecycleState', 'UA_SecureChannelState', 'UA_SessionState') else field]
            impl += [f'  void (*callback)({public});',
                     f'  if(!config || !config->owner || !config->callbacks.{slot}) return;',
                     (f'  cpkt_cfg_bind_server_owner(config->owner, {first});' if kind == 'Server' else f'  if(!config->owner->{lower}) config->owner->{lower} = {first};'),
                     f'  callback = config->callbacks.{slot};', f'  cpkt_cfg_{lower}_enter(config);',
                     f'  callback({", ".join(call)});', f'  cpkt_cfg_{lower}_leave(config);']
        impl += ['}']
        assignment = f'config->native->{slot} = value->{slot} ? {proxy} : NULL;'
        check = f'config->native->{slot} && config->native->{slot} != {proxy}'
        if slot == 'asyncOperationCancelCallback':
            assignments += ['  if(config->native->asyncOperationCancelCallback == cpkt_producer_cancel && config->owner && config->owner->typed_nodes)',
                            f'    config->owner->typed_nodes->native_cancel = value->{slot} ? {proxy} : NULL;',
                            '  else ' + assignment]
            checks += [f'  if({check} && !(config->owner && config->owner->typed_nodes &&',
                       '      config->native->asyncOperationCancelCallback == cpkt_producer_cancel &&',
                       f'      (!config->owner->typed_nodes->native_cancel || config->owner->typed_nodes->native_cancel == {proxy}))) return UA_STATUSCODE_BADNOTSUPPORTED;']
        else:
            assignments += ['  ' + assignment]
            checks += [f'  if({check}) return UA_STATUSCODE_BADNOTSUPPORTED;']
    for slot in sorted(f for f in SPECIAL[kind] if f.endswith('NotificationCallback')):
        public = f'cpkt_opcua_{lower} *handle, cpkt_opcua_ApplicationNotificationType type, cpkt_opcua_StatusCode conversionStatus, const cpkt_opcua_KeyValueMap *payload'
        proxy = 'cpkt_cfg_' + lower + '_' + slot
        header += [f'  {notification} {slot};']
        impl += [f'static void {proxy}(UA_{kind} *handle, UA_ApplicationNotificationType type, const UA_KeyValueMap payload) {{',
                 f'  cpkt_opcua_{name} *config = cpkt_cfg_{lower}_find(UA_{kind}_getConfig(handle));',
                 '  cpkt_opcua_KeyValueMap value;', '  UA_StatusCode status;',
                 f'  void (*callback)({public});',
                 f'  if(!config || !config->owner || !config->callbacks.{slot}) return;',
                 ('  cpkt_cfg_bind_server_owner(config->owner, handle);' if kind == 'Server' else f'  if(!config->owner->{lower}) config->owner->{lower} = handle;'),
                 f'  callback = config->callbacks.{slot};', '  memset(&value, 0, sizeof(value));',
                 f'  cpkt_cfg_{lower}_enter(config);', '  status = cpkt_loop_map_from_native(&payload, &value);',
                 '  callback(config->owner, (cpkt_opcua_ApplicationNotificationType)type, status, status ? NULL : &value);',
                 '  cpkt_opcua_KeyValueMap_clear(&value);', f'  cpkt_cfg_{lower}_leave(config);', '}']
        if kind == 'Client' and slot == 'globalNotificationCallback':
            assignments += [f'  if(config->discovery_parent) config->discovery_global = value->{slot} ? {proxy} : NULL;',
                            f'  config->native->{slot} = config->discovery_parent ? cpkt_cfg_discovery_notify : value->{slot} ? {proxy} : NULL;']
            checks += [f'  if(cpkt_cfg_client_global(config) && cpkt_cfg_client_global(config) != {proxy}) return UA_STATUSCODE_BADNOTSUPPORTED;']
        else:
            assignments += [f'  config->native->{slot} = value->{slot} ? {proxy} : NULL;']
            checks += [f'  if(config->native->{slot} && config->native->{slot} != {proxy}) return UA_STATUSCODE_BADNOTSUPPORTED;']
    null_checks = []
    for _, slot, _ in callbacks(body):
        if slot == 'asyncOperationCancelCallback':
            null_checks += ['  if(!config->native->asyncOperationCancelCallback || (config->native->asyncOperationCancelCallback == cpkt_producer_cancel && config->owner && config->owner->typed_nodes && !config->owner->typed_nodes->native_cancel)) value->asyncOperationCancelCallback = NULL;']
        else:
            null_checks += [f'  if(!config->native->{slot}) value->{slot} = NULL;']
    null_checks += [f'  if(!{"cpkt_cfg_client_global(config)" if kind == "Client" and slot == "globalNotificationCallback" else "config->native->" + slot}) value->{slot} = NULL;' for slot in sorted(f for f in SPECIAL[kind] if f.endswith('NotificationCallback'))]
    header += [f'}} cpkt_opcua_{name}Callbacks;',
               '/** Copy callback slots before startup/connection. Mandatory producer cleanup',
               ' * remains installed. getCallbacks returns BadNotSupported for callbacks',
               ' * installed through native or different typed interfaces, never a cast. */',
               f'cpkt_opcua_StatusCode cpkt_opcua_{name}_setCallbacks(cpkt_opcua_{name} *config, const cpkt_opcua_{name}Callbacks *callbacks);',
               '/** Inspect native configuration through its C89 view. Owned settings outputs require their matching clear; plugin/handle views belong to configuration. Serialize with reconfiguration and dispatch. */\n' f'cpkt_opcua_StatusCode cpkt_opcua_{name}_getCallbacks(const cpkt_opcua_{name} *config, cpkt_opcua_{name}Callbacks *callbacks);']
    impl += [f'cpkt_opcua_StatusCode cpkt_opcua_{name}_setCallbacks(cpkt_opcua_{name} *config, const cpkt_opcua_{name}Callbacks *value) {{',
             '  if(!config || !config->native || !value) return UA_STATUSCODE_BADINVALIDARGUMENT;',
             f'  if(!cpkt_cfg_{lower}_mutable(config)) return UA_STATUSCODE_BADINVALIDSTATE;',
             '  config->callbacks = *value;', *assignments, '  return 0;', '}',
             f'cpkt_opcua_StatusCode cpkt_opcua_{name}_getCallbacks(const cpkt_opcua_{name} *config, cpkt_opcua_{name}Callbacks *value) {{',
             '  if(!config || !config->native || !value) return UA_STATUSCODE_BADINVALIDARGUMENT;',
             *checks, '  *value = config->callbacks;', *null_checks, '  return 0;', '}']
    return header, impl


def emit_configuration(index, headers, output):
    configuration = (headers / 'config.h').read_text()
    h, impl = [], []
    # ConnectionConfig is handwritten upstream and consists only of UInt32.
    text = uncomment((headers / 'util.h').read_text())
    match = re.search(r'typedef\s+struct\s*\{([^{}]*)\}\s*UA_ConnectionConfig\s*;', text, re.S)
    if not match or re.findall(r'UA_UInt32\s+(\w+)\s*;', match[1]) != list(RECORDS['ConnectionConfig']):
        raise ValueError('Adapt complete ConnectionConfig fields')
    h += ['/** Complete native connection limits; all fields are UInt32. */',
          'typedef struct {' + translate(match[1]) + '} cpkt_opcua_ConnectionConfig;']
    h += ['/** Native deprecated spelling is an alias of clear; same ownership/status. */',
          '#define cpkt_opcua_ServerConfig_clean cpkt_opcua_ServerConfig_clear']
    for kind, filename in [('Client', 'client.h'), ('Server', 'server.h')]:
        name = kind + 'Config'
        body = public_body(headers / filename, name, configuration)
        if {slot for _, slot, _ in callbacks(body)} != CALLBACKS[kind]:
            raise ValueError('Adapt complete configuration callbacks: ' + name)
        fields = []
        for declaration in CALLBACK_PATTERN.sub('', uncomment(body)).split(';'):
            if not declaration.strip():
                continue
            m = re.fullmatch(r'\s*(void\s*\*|size_t|UA_\w+\s*\*?)\s*(\w+)\s*', declaration)
            if not m:
                raise ValueError('Adapt public configuration field: ' + name + ':' + declaration)
            fields.append((' '.join(m[1].replace('*', ' *').split()), m[2]))
        field_map = dict((field, spelling) for spelling, field in fields)
        notification = re.search(r'typedef\s+void\s*\(\*UA_' + kind + r'NotificationCallback\)\s*\((.*?)\)\s*;', uncomment((headers / filename).read_text()), re.S)
        if not notification or [t for t, n in parameters(notification[1])] != ['UA_' + kind + ' *', 'UA_ApplicationNotificationType', 'const UA_KeyValueMap']:
            raise ValueError('Adapt configuration notification signature: ' + name)
        h += [
            '/** The actual native configuration with opaque backend storage. Standalone',
            ' * new/clear/delete own it. get_config_typed borrows a live handle configuration;',
            ' * never clear/delete that borrowed view. Serialize its use with the handle.',
            ' * Configure before startup/connection. Native defaults, policy selection and',
            ' * filesystem effects remain native. Configuration copies do not clone plugins. */',
            f'typedef struct cpkt_opcua_{name} cpkt_opcua_{name};',
            '/** Complete ordinary configuration values. Plugin, callback, event-loop and',
            ' * custom-type lifetimes are exposed separately. getSettings returns owned',
            ' * schema copies into an empty output; clear them once. setSettings stages',
            ' * all schema conversions',
            ' * before replacing native values. Context pointers retain caller ownership. */',
            'typedef struct {']
        selected = []
        consumed = set(SPECIAL[kind])
        for spelling, field in fields:
            if field in consumed:
                continue
            if spelling == 'size_t' and any(f + 'Size' == field for _, f in fields):
                continue
            typename = spelling.replace('UA_', '').replace(' *', '')
            array = spelling.endswith(' *') and field + 'Size' in field_map
            if array:
                if typename not in index:
                    raise ValueError('Adapt configuration array: ' + name + '.' + field)
                h += [f'  size_t {field}Size;', f'  cpkt_opcua_{typename} *{field};']
            elif spelling == 'void *':
                h += [f'  void *{field};']
            elif spelling == 'size_t':
                h += [f'  size_t {field};']
            elif typename in index or typename in SCALARS or typename in RECORDS:
                h += [f'  cpkt_opcua_{typename} {field};']
            else:
                raise ValueError('Adapt configuration field: ' + name + '.' + field + ':' + spelling)
            selected.append((spelling, typename, field, array))
        h += [f'}} cpkt_opcua_{name}Settings;',
              '/** Clear owned nested fields and reset the record. Do not clear borrowed or arena-backed results; backing record storage remains caller-owned. */\n' f'void cpkt_opcua_{name}Settings_clear(cpkt_opcua_{name}Settings *settings);',
              '/** Inspect native configuration through its C89 view. Owned settings outputs require their matching clear; plugin/handle views belong to configuration. Serialize with reconfiguration and dispatch. */\n' f'cpkt_opcua_StatusCode cpkt_opcua_{name}_getSettings(const cpkt_opcua_{name} *config, cpkt_opcua_{name}Settings *settings);',
              '/** Configure the actual native record while quiescent. Schema inputs borrow until return; plugin/context ownership follows this family contract. Failures preserve input ownership before native handoff. */\n' f'cpkt_opcua_StatusCode cpkt_opcua_{name}_setSettings(cpkt_opcua_{name} *config, const cpkt_opcua_{name}Settings *settings);',
              '/** Allocate an owned empty native identity backing; NULL reports allocation failure. Release after every borrowing tagged pointer has ended. */\n' f'cpkt_opcua_{name} *cpkt_opcua_{name}_new(void);',
              '/** Clear owned nested fields and reset the record. Do not clear borrowed or arena-backed results; backing record storage remains caller-owned. */\n' f'cpkt_opcua_StatusCode cpkt_opcua_{name}_clear(cpkt_opcua_{name} *config);',
              '/** Clear and delete a detached owned configuration. Adopted/borrowed configs and active callback frames reject deletion; native owned plugins are cleared once. */\n' f'cpkt_opcua_StatusCode cpkt_opcua_{name}_delete(cpkt_opcua_{name} *config);',
              '/** Borrow configuration identity until handle destruction. Output is NULL on failure. */',
              f'cpkt_opcua_StatusCode cpkt_opcua_{kind.lower()}_get_config_typed(cpkt_opcua_{kind.lower()} *handle, cpkt_opcua_{name} **config);',
              '/** Invoke native newWithConfig. Client success moves plugin ownership; failure',
              ' * preserves its configuration. Server consumes/clears configuration even on',
              ' * native failure, as upstream does. Success transfers this wrapper to the',
              ' * new handle; it becomes borrowed and must not be deleted independently.',
              ' * Client failure preserves the owned wrapper; server failure leaves it empty. */',
              f'cpkt_opcua_StatusCode cpkt_opcua_{kind.lower()}_newWithConfig_typed(cpkt_opcua_{name} *config, cpkt_opcua_{kind.lower()} **handle);']
        h += [
            '/** Copy rendered logging configuration in place, retaining the native logger',
            ' * address used by dependent plugins. NULL deliberately silences logging.',
            ' * User data borrows until replacement/destruction. Native stock logger',
            ' * inspection returns BadNotSupported if its callback cannot be represented. */',
            f'cpkt_opcua_StatusCode cpkt_opcua_{name}_setLogger(cpkt_opcua_{name} *config, const cpkt_opcua_log_config *logger);',
            '/** Inspect native configuration through its C89 view. Owned settings outputs require their matching clear; plugin/handle views belong to configuration. Serialize with reconfiguration and dispatch. */\n' f'cpkt_opcua_StatusCode cpkt_opcua_{name}_getLogger(const cpkt_opcua_{name} *config, cpkt_opcua_log_config *logger);',
            '/** Borrow the actual native event loop and its external ownership flag.',
            ' * Set before startup/connection. Internal ownership transfers on success;',
            ' * external loops remain caller-owned and must outlive every attached handle.',
            ' * Replaced internal loops must be fresh/stopped; free failure preserves the',
            ' * old assignment and ownership. NULL removes the loop. Same-loop changes',
            ' * only change ownership; they never free/recreate the native backend. */',
            f'cpkt_opcua_StatusCode cpkt_opcua_{name}_getEventLoop(cpkt_opcua_{name} *config, cpkt_opcua_EventLoop **loop, cpkt_opcua_Boolean *external);',
            '/** Configure the actual native record while quiescent. Schema inputs borrow until return; plugin/context ownership follows this family contract. Failures preserve input ownership before native handoff. */\n' f'cpkt_opcua_StatusCode cpkt_opcua_{name}_setEventLoop(cpkt_opcua_{name} *config, cpkt_opcua_EventLoop *loop, cpkt_opcua_Boolean external);',
        ]
        callback_header, callback_impl = emit_configuration_callbacks(kind, body, index)
        h += callback_header
        impl += callback_impl
        impl += [f'cpkt_opcua_StatusCode cpkt_opcua_{name}_setLogger(cpkt_opcua_{name} *config, const cpkt_opcua_log_config *logger) {{',
                 '  cpkt_opcua_log_config quiet;',
                 '  if(!config || !config->native || !cpkt_logger_valid(logger)) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                 f'  if(!cpkt_cfg_{kind.lower()}_mutable(config)) return UA_STATUSCODE_BADINVALIDSTATE;',
                 '  memset(&quiet, 0, sizeof(quiet));',
                 '  if(!config->native->logging) config->native->logging = &config->logger.native;',
                 f'  cpkt_cfg_{kind.lower()}_enter(config);',
                 '  cpkt_logger_set(&config->logger, config->native->logging, logger ? logger : &quiet);',
                 f'  cpkt_cfg_{kind.lower()}_leave(config);', '  return 0;', '}',
                 f'cpkt_opcua_StatusCode cpkt_opcua_{name}_getLogger(const cpkt_opcua_{name} *config, cpkt_opcua_log_config *logger) {{',
                 '  const UA_Logger *native;',
                 '  if(!config || !config->native || !logger) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                 '  native = config->native->logging;',
                 '  if(!native) { memset(logger, 0, sizeof(*logger)); return 0; }',
                 '  if(native->log != cpkt_logger_log || !native->context) return UA_STATUSCODE_BADNOTSUPPORTED;',
                 '  if(((const struct cpkt_opcua_logger *)native->context)->owned_context) return UA_STATUSCODE_BADNOTSUPPORTED;',
                 '  *logger = ((const struct cpkt_opcua_logger *)native->context)->config;', '  return 0;', '}',
                 f'cpkt_opcua_StatusCode cpkt_opcua_{name}_getEventLoop(cpkt_opcua_{name} *config, cpkt_opcua_EventLoop **loop, cpkt_opcua_Boolean *external) {{',
                 '  if(!loop) return UA_STATUSCODE_BADINVALIDARGUMENT;', '  *loop = NULL;',
                 '  if(!config || !config->native || !external) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                 '  *external = config->native->externalEventLoop;',
                 '  if(!config->native->eventLoop) return 0;',
                 '  *loop = cpkt_loop_native_adopt(config->native->eventLoop);',
                 '  if(!*loop) return UA_STATUSCODE_BADOUTOFMEMORY;',
                 '  if(!config->native->externalEventLoop) (*loop)->configuration_slot = &config->native->eventLoop;',
                 '  return 0;', '}',
                 f'cpkt_opcua_StatusCode cpkt_opcua_{name}_setEventLoop(cpkt_opcua_{name} *config, cpkt_opcua_EventLoop *loop, cpkt_opcua_Boolean external) {{',
                 '  UA_StatusCode status;',
                 '  if(!config || !config->native) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                 f'  if(!cpkt_cfg_{kind.lower()}_mutable(config)) return UA_STATUSCODE_BADINVALIDSTATE;',
                 '  status = cpkt_cfg_loop_assign(&config->native->eventLoop, &config->native->externalEventLoop, loop, external);',
                 *(['  if(config->owner && config->owner->typed_nodes_refresh_eventloop) config->owner->typed_nodes_refresh_eventloop(config->owner);'] if kind == 'Server' else []),
                 '  return status;', '}']
        impl += [f'void cpkt_opcua_{name}Settings_clear(cpkt_opcua_{name}Settings *settings) {{',
                 '  if(!settings) return;']
        for spelling, typename, field, array in selected:
            if array:
                impl += [f'  cpkt_clear_array(settings->{field}, settings->{field}Size, &cpkt_types[{index[typename]}]);']
            elif typename in index:
                impl += [f'  cpkt_opcua_type_clear(&settings->{field}, &cpkt_types[{index[typename]}]);']
        impl += ['  memset(settings, 0, sizeof(*settings));', '}']
        native_cleanup = []
        for spelling, typename, field, array in selected:
            if array:
                native_cleanup += [f'  if(native->{field}) UA_Array_delete(native->{field}, native->{field}Size, cpkt_types[{index[typename]}].native);',
                                   f'  native->{field} = NULL; native->{field}Size = 0;']
            elif typename in index:
                native_cleanup += [f'  UA_clear(&native->{field}, cpkt_types[{index[typename]}].native);']
        impl += [f'static void cpkt_cfg_{kind.lower()}_values_clear(UA_{name} *native) {{', *native_cleanup, '}']
        for direction in ('read', 'write'):
            source, dest = ('native', 'settings') if direction == 'read' else ('settings', 'native')
            signature = f'const UA_{name} *native, cpkt_opcua_{name}Settings *settings' if direction == 'read' else f'const cpkt_opcua_{name}Settings *settings, UA_{name} *native'
            impl += [f'static UA_StatusCode cpkt_cfg_{kind.lower()}_values_{direction}({signature}) {{',
                     '  UA_StatusCode status = 0;']
            for spelling, typename, field, array in selected:
                if array:
                    impl += [f'  if(!status) status = cpkt_array({source}->{field}, {source}->{field}Size, (void **)&{dest}->{field}, &cpkt_types[{index[typename]}], {int(direction == "write")}, 0);',
                             f'  if(!status) {dest}->{field}Size = {source}->{field}Size;']
                elif typename in index:
                    impl += [f'  if(!status) status = cpkt_convert(&{source}->{field}, &{dest}->{field}, &cpkt_types[{index[typename]}], {int(direction == "write")}, 0);']
                elif typename in RECORDS:
                    impl += [f'  {dest}->{field}.{member} = {source}->{field}.{member};' for member in RECORDS[typename]]
                elif typename in ('MessageSecurityMode', 'RuleHandling'):
                    cast = ('UA_' if direction == 'write' else 'cpkt_opcua_') + typename
                    impl += [f'  {dest}->{field} = ({cast}){source}->{field};']
                else:
                    impl += [f'  {dest}->{field} = {source}->{field};']
            impl += ['  return status;', '}']
        impl += [f'cpkt_opcua_StatusCode cpkt_opcua_{name}_getSettings(const cpkt_opcua_{name} *config, cpkt_opcua_{name}Settings *settings) {{',
                 f'  cpkt_opcua_{name}Settings staged;', '  UA_StatusCode status;',
                 '  if(!config || !config->native || !settings) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                 '  memset(&staged, 0, sizeof(staged));',
                 f'  status = cpkt_cfg_{kind.lower()}_values_read(config->native, &staged);',
                 f'  if(status) cpkt_opcua_{name}Settings_clear(&staged); else *settings = staged;',
                 '  return status;', '}',
                 f'cpkt_opcua_StatusCode cpkt_opcua_{name}_setSettings(cpkt_opcua_{name} *config, const cpkt_opcua_{name}Settings *settings) {{',
                 f'  UA_{name} staged;', '  UA_StatusCode status;',
                 '  if(!config || !config->native || !settings) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                 f'  if(!cpkt_cfg_{kind.lower()}_mutable(config)) return UA_STATUSCODE_BADINVALIDSTATE;',
                 '  memset(&staged, 0, sizeof(staged));',
                 f'  status = cpkt_cfg_{kind.lower()}_values_write(settings, &staged);',
                 f'  if(status) {{ cpkt_cfg_{kind.lower()}_values_clear(&staged); return status; }}',
                 f'  cpkt_cfg_{kind.lower()}_values_clear(config->native);']
        for spelling, typename, field, array in selected:
            impl += [f'  config->native->{field} = staged.{field};']
            if array:
                impl += [f'  config->native->{field}Size = staged.{field}Size;']
        impl += ['  return 0;', '}']
    # Stock helper policy is derived from declarations; no duplicate defaults.
    for filename in ('client.h', 'client_config_default.h', 'server_config_default.h', 'server_config_file_based.h'):
        text = uncomment((headers / filename).read_text())
        declarations = re.findall(r'(?:UA_EXPORT\s+UA_StatusCode|UA_StatusCode\s+UA_EXPORT|UA_INLINABLE\(\s*UA_StatusCode)\s+(UA_(?:Client|Server)Config_\w+)\s*\((.*?)\)', text, re.S)
        for function, signature in declarations:
            if function.endswith('_copy'):
                continue  # Native shallow ownership semantics need the dedicated copy path.
            kind = 'Client' if function.startswith('UA_Client') else 'Server'
            name = kind + 'Config'
            args = parameters(signature)
            if args[0][0] != f'UA_{name} *':
                raise ValueError('Adapt configuration helper receiver: ' + function)
            if function.endswith('_addSecurityPolicy_Filestore'):
                continue  # Consumes a full policy; handled by policy ownership plumbing.
            public = translate(signature)
            target = function.replace('UA_', 'cpkt_opcua_', 1)
            guard = '#if defined(__linux__) || defined(_WIN32)' if 'Filestore' in function else None
            if guard:
                h += [guard]; impl += [guard]
            h += [f'/** Invoke native {function}. Inputs borrow until return; native partial',
                  ' * changes/status and file effects are retained. Use before startup/connection.',
                  ' * Password callbacks may run synchronously. No alternative defaults are used. */',
                  f'cpkt_opcua_StatusCode {target}({public});']
            locals_, setup, cleanup, call = [], [], [], [args[0][1] + '->native']
            receiver = args[0][1]
            arg_names = {a for _, a in args}
            for spelling, field in args[1:]:
                array_count = field + 'Size' if field + 'Size' in arg_names else None
                if spelling in ('UA_ByteString', 'const UA_ByteString', 'const UA_String'):
                    locals_ += [f'  UA_ByteString value_{field};']
                    setup += [f'  if(!status && !cpkt_string_valid(&{field})) status = UA_STATUSCODE_BADINVALIDARGUMENT;',
                              f'  value_{field} = cpkt_string_view(&{field});']
                    call += ['value_' + field]
                elif spelling == 'const UA_ByteString *' and array_count:
                    locals_ += [f'  UA_ByteString *value_{field} = NULL;']
                    setup += [f'  if(!status) status = cpkt_array({field}, {array_count}, (void **)&value_{field}, &cpkt_types[{index["ByteString"]}], 1, 0);']
                    cleanup += [f'  if(value_{field}) UA_Array_delete(value_{field}, {array_count}, cpkt_types[{index["ByteString"]}].native);']
                    call += ['value_' + field]
                elif spelling == 'const UA_ByteString *':
                    locals_ += [f'  UA_ByteString value_{field};']
                    setup += [f'  memset(&value_{field}, 0, sizeof(value_{field}));',
                              f'  if({field}) {{ if(!cpkt_string_valid({field})) status = UA_STATUSCODE_BADINVALIDARGUMENT; else value_{field} = cpkt_string_view({field}); }}']
                    call += [f'{field} ? &value_{field} : NULL']
                elif spelling == 'UA_MessageSecurityMode':
                    call += [f'(UA_MessageSecurityMode){field}']
                elif spelling in ('UA_UInt16', 'UA_UInt32', 'size_t', 'const char *'):
                    call += [field]
                else:
                    raise ValueError('Adapt configuration helper parameter: ' + function + ':' + spelling)
            impl += [f'cpkt_opcua_StatusCode {target}({public}) {{', '  UA_StatusCode status = 0;', *locals_,
                     f'  if(!{receiver} || !{receiver}->native) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                     f'  if(!cpkt_cfg_{kind.lower()}_mutable({receiver})) return UA_STATUSCODE_BADINVALIDSTATE;',
                     *setup, f'  if(!status) {{ cpkt_cfg_{kind.lower()}_enter({receiver}); status = {function}({", ".join(call)}); cpkt_cfg_{kind.lower()}_leave({receiver}); }}',
                     *([f'  if({receiver}->owner && {receiver}->owner->typed_nodes_refresh_eventloop) {receiver}->owner->typed_nodes_refresh_eventloop({receiver}->owner);'] if kind == 'Server' else []),
                     *cleanup, '  return status;', '}']
            if guard:
                h += ['#endif']; impl += ['#endif']
    (output / 'opcua_config_metadata.inc').write_text('\n'.join(impl) + '\n')
    from config_plugin_emitter import emit_config_plugins
    h += emit_config_plugins(index, headers, output)
    return h
