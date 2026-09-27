"""Bind client lifecycle/discovery/session operations from public declarations.

This is argument ownership policy, not another service implementation. The
native client owns connection state, session transfer, discovery and timers.
"""
import re
from plugin_emitter import parameters, uncomment
from client_emitter import public

DIRECT = {
    'connect', 'connectAsync', 'connectSecureChannel', 'connectSecureChannelAsync',
    'connectUsername', 'disconnect', 'disconnectAsync', 'disconnectSecureChannel',
    'disconnectSecureChannelAsync', 'activateCurrentSession',
    'activateCurrentSessionAsync', 'activateSession', 'activateSessionAsync',
    'getSessionAuthenticationToken', 'getState', 'run_iterate', 'runUntilInterrupt',
    'startListeningForReverseConnect', 'getEndpoints', 'findServers',
    'findServersOnNetwork', 'getNamespaceUri', 'getNamespaceIndex', 'addNamespace',
}
TIMERS = {'addTimedCallback', 'addRepeatedCallback',
          'changeRepeatedCallbackInterval', 'removeCallback'}
ARRAY_INPUTS = {
    'startListeningForReverseConnect': {'listenHostnames': 'listenHostnamesLength'},
    'findServers': {'serverUris': 'serverUrisSize', 'localeIds': 'localeIdsSize'},
    'findServersOnNetwork': {'serverCapabilityFilter': 'serverCapabilityFilterSize'},
}
ARRAY_OUTPUTS = {
    'getEndpoints': ('endpointDescriptions', 'endpointDescriptionsSize', 'EndpointDescription'),
    'findServers': ('registeredServers', 'registeredServersSize', 'ApplicationDescription'),
    'findServersOnNetwork': ('serverOnNetwork', 'serverOnNetworkSize', 'ServerOnNetwork'),
}
OUTPUTS = {
    'getSessionAuthenticationToken': {'authenticationToken': 'NodeId', 'serverNonce': 'ByteString'},
    'getNamespaceUri': {'nsUri': 'String'},
}


def documentation(name):
    lines = [f'/** Invoke native UA_Client_{name} with complete C89 arguments.',
             ' * Return native status or a C89 conversion failure; serialize client',
             ' * operations. Synchronous calls cannot recursively run its EventLoop.']
    if name in ARRAY_OUTPUTS:
        lines += [' * Inputs borrow until return. Required count/array outputs must be empty;',
                  ' * success transfers the complete array, including nested owned fields.',
                  ' * Release with cpkt_opcua_array_delete and the matching type descriptor.',
                  ' * Failure leaves count zero and array NULL, including conversion failure.']
    elif name in OUTPUTS:
        lines += [' * Required outputs must be empty; success transfers owned records.',
                  ' * Release with the corresponding type clear. Failure leaves them empty.']
    elif name == 'getState':
        lines += [' * Every output is optional; values preserve the complete native enums.',
                  ' * This function returns Good for a valid client; connection errors are',
                  ' * reported through connectStatus, including BadConnectionClosed.']
    elif name in ('addNamespace', 'getNamespaceIndex'):
        lines += [' * The counted URI borrows until return, including embedded NUL bytes.',
                  ' * Required outIndex is unchanged on failure. addNamespace returns the',
                  ' * existing index if that URI is already registered.']
    elif name.startswith('activateSession'):
        lines += [' * Authentication token and nonce borrow until return; native code copies',
                  ' * them before returning. The recipient must have no session, use the',
                  ' * original user identity, and have matching endpoint/token policies.',
                  ' * A SecureChannel-only async connect does not discover those policies;',
                  ' * configure them explicitly. No extra discovery operation is added.']
    elif name == 'startListeningForReverseConnect':
        lines += [' * Counted hostnames borrow until return; native code owns listen sockets.',
                  ' * Native iteration accepts the server connection and progresses sessions.']
    elif name == 'runUntilInterrupt':
        lines += [' * Run the native EventLoop until its interrupt handler stops it. Native',
                  ' * signal registration and cleanup remain unchanged; callbacks may submit',
                  ' * async work but must not destroy the client or recursively iterate it.']
    elif name == 'run_iterate':
        lines += [' * Native iteration dispatches connection, service, subscription and timer',
                  ' * callbacks. Its return status includes native connection errors.']
    if name in ('connect', 'connectAsync'):
        lines += [' * endpointUrl borrows until return; NULL reuses the configured endpoint.']
    elif name.startswith('connect'):
        lines += [' * URL and any credentials borrow until return; preserve native validation.']
    if name == 'connectUsername':
        lines += [' * This binding does not enable allowNonePolicyPassword or weaken the',
                  ' * configured authentication/security policy.']
    if name.endswith('Async'):
        lines += [' * Submission is asynchronous; native iteration progresses completion.']
    lines += [' */']
    return lines


def declarations(original):
    text = uncomment(original)
    found = re.findall(r'((?:(?:UA_EXPORT|UA_THREADSAFE|UA_\w+|void)\s+)+)'
                       r'UA_Client_(\w+)\s*\((.*?)\)\s*;', text, re.S)
    selected = [(p, n, s) for p, n, s in found if n in DIRECT | TIMERS]
    if {n for _, n, _ in selected} != DIRECT | TIMERS:
        raise ValueError('Adapt core client public declaration parser')
    return selected


def enum_declaration(original, typename):
    match = re.search(r'typedef\s+enum\s*\{([^{}]*)\}\s*UA_' + typename + r'\s*;', uncomment(original), re.S)
    if not match:
        raise ValueError('Adapt core client state enum: ' + typename)
    body = re.sub(r',\s*$', '', match[1])  # C89 forbids a trailing enum comma.
    if re.sub(r'\bUA_\w+\b|\d+|\s|[=,]', '', body):
        raise ValueError('Adapt core client enum values: ' + typename)
    return f'typedef enum {{{body.replace("UA_", "CPKT_OPCUA_")}}} cpkt_opcua_{typename};', re.findall(r'\bUA_\w+\b', body)


def emit_core_client(index, native_headers):
    original = (native_headers / 'client.h').read_text()
    common = (native_headers / 'common.h').read_text()
    header = [re.match(r'/\*.*?\*/', original, re.S)[0]]
    metadata = []
    for typename in ('SecureChannelState', 'SessionState'):
        declaration, constants = enum_declaration(common, typename)
        header += ['/** Complete native state enum; values are generated from common.h. */', declaration]
        metadata += [f'typedef char core_{value}[((int){value.replace("UA_", "CPKT_OPCUA_")} == (int){value}) ? 1 : -1];' for value in constants]
    callback = re.search(r'typedef\s+void\s*\(\*UA_ClientCallback\)\s*\((.*?)\)\s*;', uncomment(original), re.S)
    if not callback or parameters(callback[1]) != [('UA_Client *', 'client'), ('void *', 'data')]:
        raise ValueError('Adapt native client timer callback')
    header += ['/** Native timer dispatch with the original client and context. Context borrows',
               ' * until a one-shot fires, removal, or client destruction. A callback may',
               ' * remove/change timers or submit async operations. Do not destroy the client',
               ' * from a callback or synchronously run its EventLoop recursively. */',
               'typedef void (*cpkt_opcua_ClientCallback)(cpkt_opcua_client *client, void *data);']
    for prefix, name, signature in declarations(original):
        result = [p for p in prefix.split() if p not in ('UA_EXPORT', 'UA_THREADSAFE')]
        if result != ['void' if name in ('getState', 'removeCallback') else 'UA_StatusCode']:
            raise ValueError('Adapt core client return: ' + name)
        args = parameters(signature)
        if args[0] != ('UA_Client *', 'client'):
            raise ValueError('Adapt core client receiver')
        public_signature = public(signature)
        if name in TIMERS:
            expected = {
                'addTimedCallback': [('UA_Client *', 'client'), ('UA_ClientCallback', 'callback'), ('void *', 'data'), ('UA_DateTime', 'date'), ('UA_UInt64 *', 'callbackId')],
                'addRepeatedCallback': [('UA_Client *', 'client'), ('UA_ClientCallback', 'callback'), ('void *', 'data'), ('UA_Double', 'interval_ms'), ('UA_UInt64 *', 'callbackId')],
                'changeRepeatedCallbackInterval': [('UA_Client *', 'client'), ('UA_UInt64', 'callbackId'), ('UA_Double', 'interval_ms')],
                'removeCallback': [('UA_Client *', 'client'), ('UA_UInt64', 'callbackId')],
            }[name]
            if args != expected:
                raise ValueError('Adapt core client timer: ' + name)
            timer_docs = [f'/** Invoke native UA_Client_{name}; native EventLoop scheduling only.',
                          ' * DateTime and IDs retain all 64 bits; serialize client operations.']
            if name.startswith('add'):
                timer_docs += [' * Context borrows until dispatch (one-shot), removal or destruction.',
                               ' * callbackId is optional and zeroed on failure; NULL client/callback',
                               ' * is invalid. Interval validation and scheduling remain native.']
            elif name == 'changeRepeatedCallbackInterval':
                timer_docs += [' * Native status reports interval/ID validation; NULL client is invalid.',
                               ' * A one-shot changed to repeated retains context until removal or',
                               ' * destruction. Changing the current timer in its callback is supported.']
            else:
                timer_docs += [' * Unknown-ID removal and NULL client are no-ops. Removal within the',
                               ' * callback is supported; the current dispatch retains its context',
                               ' * until return. Other timers may also be removed from a callback.']
            header += [*timer_docs, ' */',
                       f'{"void" if name == "removeCallback" else "cpkt_opcua_StatusCode"} cpkt_opcua_client_{name}_typed({public_signature});']
            continue
        locals_, init, convert, clear, invoke, output, fail = [], [], [], [], [], [], []
        validation = ['!client', '!client->client']
        array_inputs = ARRAY_INPUTS.get(name, {})
        array_output = ARRAY_OUTPUTS.get(name)
        schema_outputs = OUTPUTS.get(name, {})
        for spelling, param in args:
            if param == 'client': invoke += ['client->client']; continue
            if array_output and param in array_output[:2]:
                array, count, typename = array_output
                validation += ['!' + param]
                if param == count:
                    if spelling.replace(' ', '') != 'size_t*': raise ValueError('Adapt discovery output count')
                    init += [f'  *{count} = 0;']; invoke += ['&native_count']; continue
                if spelling.replace(' ', '') != 'UA_' + typename + '**': raise ValueError('Adapt discovery output array')
                init += [f'  *{array} = NULL;']
                locals_ += [f'  UA_{typename} *native_{array} = NULL;', '  size_t native_count = 0;']
                invoke += ['&native_' + array]
                output += [f'    if(!native_status) status = cpkt_array(native_{array}, native_count, (void **){array}, &cpkt_types[{index[typename]}], 0, 0);', f'    if(!status && !native_status) *{count} = native_count;']
                fail += [f'  if(status) {{ cpkt_opcua_array_delete(*{array}, native_count, &cpkt_types[{index[typename]}]); *{array} = NULL; }}']
                clear += [f'  if(native_{array}) UA_Array_delete(native_{array}, native_count, cpkt_types[{index[typename]}].native);']
                continue
            if param in array_inputs:
                count = array_inputs[param]
                if spelling.replace('const ', '').replace(' ', '') != 'UA_String*': raise ValueError('Adapt core string-array input')
                if ('size_t', count) not in args: raise ValueError('Adapt core input count')
                locals_ += [f'  void *native_{param} = NULL;']
                convert += [f'  if(!status) status = cpkt_array({param}, {count}, &native_{param}, &cpkt_types[{index["String"]}], 1, 0);']
                clear += [f'  if(native_{param}) UA_Array_delete(native_{param}, {count}, &UA_TYPES[UA_TYPES_STRING]);']
                invoke += ['(UA_String *)native_' + param]; continue
            if spelling in ('size_t', 'const char *', 'UA_UInt32', 'UA_UInt16'):
                invoke += [param]; continue
            if name in ('getNamespaceIndex', 'addNamespace') and param == 'outIndex':
                if spelling != 'UA_UInt16 *': raise ValueError('Adapt namespace output index')
                validation += ['!' + param]; invoke += [param]; continue
            if name == 'getState':
                types = {'channelState': 'SecureChannelState', 'sessionState': 'SessionState', 'connectStatus': 'StatusCode'}
                typename = types.get(param)
                if not typename or spelling != 'UA_' + typename + ' *': raise ValueError('Adapt core state output')
                locals_ += [f'  UA_{typename} native_{param} = (UA_{typename})0;']
                invoke += [f'{param} ? &native_{param} : NULL']
                output += [f'    if({param}) *{param} = (cpkt_opcua_{typename})native_{param};']; continue
            typename = spelling.replace('const ', '').replace('*', '').strip()[3:]
            if not spelling.replace('const ', '').startswith('UA_') or typename not in index or spelling.count('*') > 1:
                raise ValueError('Adapt core schema argument: ' + name + '.' + param)
            pointer = '*' in spelling
            locals_ += [f'  UA_{typename} native_{param};']
            init += [f'  memset(&native_{param}, 0, sizeof(native_{param}));']
            clear += [f'  UA_clear(&native_{param}, cpkt_types[{index[typename]}].native);']
            if param in schema_outputs:
                if not pointer or schema_outputs[param] != typename: raise ValueError('Adapt core schema output')
                validation += ['!' + param]
                init += [f'  memset({param}, 0, sizeof(*{param}));']
                output += [f'    if(!status && !native_status) status = cpkt_convert(&native_{param}, {param}, &cpkt_types[{index[typename]}], 0, 0);']
                fail += [f'  if(status) cpkt_opcua_type_clear({param}, &cpkt_types[{index[typename]}]);']
            else:
                if pointer: validation += ['!' + param]
                convert += [f'  if(!status) status = cpkt_convert({param if pointer else "&" + param}, &native_{param}, &cpkt_types[{index[typename]}], 1, 0);']
            invoke += ['&native_' + param if pointer else 'native_' + param]
        header += [*documentation(name),
                   f'cpkt_opcua_StatusCode cpkt_opcua_client_{name}_typed({public_signature});']
        metadata += [f'cpkt_opcua_StatusCode cpkt_opcua_client_{name}_typed({public_signature}) {{',
                     '  UA_StatusCode status = 0, native_status = 0;', *locals_,
                     f'  if({" || ".join(validation)}) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                     *init, *convert, '  if(!status) {',
                     f'    {"" if name == "getState" else "native_status = "}UA_Client_{name}({", ".join(invoke)});',
                     *output, '  }', *fail, *clear, '  return status ? status : native_status;', '}']
    return header, metadata
