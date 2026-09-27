"""Bind the complete public async client header without rebuilding services.

The only handwritten policy distinguishes encoded inputs, callback results,
and the upstream async node helpers' unused assigned-ID argument. Callback
signatures and operation argument types remain authoritative upstream data.
"""
import re
from plugin_emitter import parameters, uncomment
from client_emitter import public


def declarations(original):
    text = uncomment(original)
    text = re.sub(r'^\s*_UA_(?:BEGIN|END)_DECLS\s*$', '', text, flags=re.M)
    macro = re.search(r'#define UA_CLIENT_ASYNCWRITE\(NAME, ATTR_TYPE\)(.*?)(?=\n\n)', text, re.S)
    if not macro:
        raise ValueError('Adapt async write declaration macro')
    body = macro[1].replace('\\\n', ' ')
    if 'NAME(' not in body or 'const ATTR_TYPE *attr' not in body:
        raise ValueError('Adapt async write macro parameters')
    text = text[:macro.start()] + text[macro.end():]
    text = re.sub(r'UA_CLIENT_ASYNCWRITE\((\w+),\s*(\w+)\)',
                  lambda m: body.replace('NAME', m[1]).replace('ATTR_TYPE', m[2]), text)
    found = re.findall(r'((?:(?:UA_EXPORT|UA_THREADSAFE|UA_\w+)\s+)+)'
                       r'(__UA_Client_\w+|UA_Client_\w+)\s*\((.*?)\)\s*;', text, re.S)
    names = set(re.findall(r'\b(?:__UA_Client_|UA_Client_)\w+(?=\s*\()', text))
    if names != {name for _, name, _ in found}:
        raise ValueError('Adapt async client declaration parser')
    return found


def callback_declarations(original):
    found = re.findall(r'typedef\s+void\s*\(\*(UA_Client\w+Callback)\)\s*\((.*?)\)\s*;', uncomment(original), re.S)
    if set(re.findall(r'\(\*(UA_Client\w+Callback)\)', uncomment(original))) != {name for name, _ in found}:
        raise ValueError('Adapt async client callback parser')
    return found


def emit_async_client(index, native_headers):
    original = (native_headers / 'client_highlevel_async.h').read_text()
    header = [re.match(r'/\*.*?\*/', original, re.S)[0]]
    metadata, callbacks = [], {}
    for native_name, signature in callback_declarations(original):
        args = parameters(signature)
        if args[:3] != [('UA_Client *', 'client'), ('void *', 'userdata'), ('UA_UInt32', 'requestId')]:
            raise ValueError('Adapt async callback context/ID: ' + native_name)
        has_status = len(args) == 5
        if has_status and args[3] != ('UA_StatusCode', 'status'):
            raise ValueError('Adapt async callback native status: ' + native_name)
        if len(args) not in (4, 5):
            raise ValueError('Adapt async callback arity: ' + native_name)
        spelling, result = args[-1]
        typename = spelling.replace('*', '').strip().replace('UA_', '', 1)
        if spelling != 'void *' and (not spelling.startswith('UA_') or spelling.count('*') != 1 or typename not in index):
            raise ValueError('Adapt async callback result: ' + native_name)
        callbacks[native_name] = (typename, has_status)
        c_name = public(native_name)
        public_args = [(public(t), n) for t, n in args[:-1]] + [('cpkt_opcua_StatusCode', 'conversionStatus'), (public(spelling), result)]
        c_signature = ', '.join(t + ' ' + n for t, n in public_args)
        header += ['/** Complete native completion arguments with an additional conversion status.',
                   ' * Client, context and request ID are the original submitted values. Native',
                   ' * status/NULL results are preserved. A conversion error gives NULL result;',
                   ' * otherwise result borrows until return. Copy data to retain it. Do not',
                   ' * destroy the client from this callback. Callback timing remains native. */',
                   f'typedef void (*{c_name})({c_signature});']
        if native_name == 'UA_ClientAsyncOperationCallback':
            continue  # Public callback vocabulary, not accepted by a public operation.
        context = 'cpkt_async_' + native_name[3:]
        metadata += [f'typedef struct {{ cpkt_opcua_client *client; void *user; {c_name} callback; const cpkt_opcua_Type *type; }} {context};',
                     f'static void {context}_complete({signature}) {{',
                     f'  {context} *state = ({context} *)userdata;',
                     '  UA_StatusCode conversion = 0;', '  void *converted = NULL;',
                     '  (void)client;', f'  if({result}) {{',
                     '    converted = cpkt_opcua_type_new(state->type);',
                     f'    conversion = converted ? cpkt_convert({result}, converted, state->type, 0, 0) : UA_STATUSCODE_BADOUTOFMEMORY;',
                     '    if(conversion) { cpkt_opcua_type_delete(converted, state->type); converted = NULL; }',
                     '  }',
                     f'  state->callback(state->client, state->user, requestId, ' + ('status, ' if has_status else '') + f'conversion, ({public(spelling)})converted);',
                     '  cpkt_opcua_type_delete(converted, state->type);', '  UA_free(state);', '}']
    for prefix, native_name, signature in declarations(original):
        if [p for p in prefix.split() if p not in ('UA_EXPORT', 'UA_THREADSAFE')] != ['UA_StatusCode']:
            raise ValueError('Adapt async client return: ' + native_name)
        args = parameters(signature)
        if args[0] != ('UA_Client *', 'client'):
            raise ValueError('Adapt async receiver')
        name = native_name.split('UA_Client_', 1)[1]
        c_signature = public(signature).replace('cpkt_opcua_DataType *', 'cpkt_opcua_Type *')
        cb = [(t, n) for t, n in args if t in callbacks]
        if not cb:
            if name not in ('cancelByRequestHandle', 'cancelByRequestId', 'renewSecureChannel'):
                raise ValueError('Adapt async utility: ' + name)
            header += ['/** Invoke native cancellation/renewal. Optional cancelCount preserves the',
                       ' * native output-on-failure behavior. IDs, cancellation and completion',
                       ' * scheduling are controlled by upstream. Serialize client operations. */',
                       f'cpkt_opcua_StatusCode cpkt_opcua_client_{name}_typed({c_signature});']
            metadata += [f'cpkt_opcua_StatusCode cpkt_opcua_client_{name}_typed({c_signature}) {{',
                         '  if(!client || !client->client) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                         f'  return {native_name}(client->client' + (', ' + ', '.join(n for _, n in args[1:]) if len(args) > 1 else '') + ');', '}']
            # UInt32 pointers have the same declared underlying unsigned-int
            # type on every supported target, enforced by the conversion TU.
            continue
        if len(cb) != 1:
            raise ValueError('Adapt async callback slots')
        cb_type, cb_param = cb[0]
        result_type, callback_required = callbacks[cb_type]
        generic = name == 'AsyncService'
        if (result_type == 'void') != generic:
            raise ValueError('Adapt dynamic async callback: ' + name)
        context = 'cpkt_async_' + cb_type[3:]
        locals_, init, convert, clear, invoke = [], [], [], [], []
        validation = ['!client', '!client->client'] + (['!' + cb_param] if callback_required else [])
        if generic:
            validation += ['!request', '!cpkt_valid_type(requestType)', '!cpkt_valid_type(responseType)']
            locals_ += ['  void *native_request = NULL;']
            convert += ['  if(!status) { native_request = UA_new(requestType->native);',
                        '    status = native_request ? cpkt_convert(request, native_request, requestType, 1, 0) : UA_STATUSCODE_BADOUTOFMEMORY; }']
            clear += ['  if(native_request) UA_delete(native_request, requestType->native);']
        id_param = 'reqId' if any(n == 'reqId' for _, n in args) else 'requestId'
        for spelling, param in args:
            if param == 'client': invoke += ['client->client']; continue
            if param == cb_param: invoke += [context + '_complete' if callback_required else f'{cb_param} ? {context}_complete : NULL']; continue
            if param == 'userdata':
                if spelling != 'void *': raise ValueError('Adapt async userdata')
                invoke += ['state' if callback_required else f'{cb_param} ? state : userdata']; continue
            if param == id_param:
                if spelling != 'UA_UInt32 *': raise ValueError('Adapt async request ID')
                invoke += [param]; continue
            if generic and param in ('request', 'requestType', 'responseType'):
                if spelling != {'request': 'const void *', 'requestType': 'const UA_DataType *', 'responseType': 'const UA_DataType *'}[param]:
                    raise ValueError('Adapt generic async type boundary')
                invoke += ['native_request' if param == 'request' else param + '->native']; continue
            if name == 'call_async' and param in ('inputSize', 'input'):
                if spelling != {'inputSize': 'size_t', 'input': 'const UA_Variant *'}[param]:
                    raise ValueError('Adapt async method arguments')
                if param == 'inputSize': invoke += [param]; continue
                locals_ += ['  void *native_input = NULL;']
                convert += [f'  if(!status) status = cpkt_array(input, inputSize, &native_input, &cpkt_types[{index["Variant"]}], 1, 0);']
                clear += [f'  if(native_input) UA_Array_delete(native_input, inputSize, cpkt_types[{index["Variant"]}].native);']
                invoke += ['(const UA_Variant *)native_input']; continue
            typename = spelling.replace('const ', '').replace('*', '').strip()[3:]
            if not spelling.replace('const ', '').startswith('UA_') or typename not in index or spelling.count('*') > 1:
                raise ValueError('Adapt async input: ' + name + '.' + param)
            pointer = '*' in spelling
            if pointer and param != 'outNewNodeId': validation += ['!' + param]
            locals_ += [f'  UA_{typename} native_{param};']
            init += [f'  memset(&native_{param}, 0, sizeof(native_{param}));']
            clear += [f'  UA_clear(&native_{param}, cpkt_types[{index[typename]}].native);']
            if param != 'outNewNodeId':
                convert += [f'  if(!status) status = cpkt_convert({param if pointer else "&" + param}, &native_{param}, &cpkt_types[{index[typename]}], 1, 0);']
            elif spelling != 'UA_NodeId *': raise ValueError('Adapt async unused node output')
            invoke += [f'{param} ? &native_{param} : NULL' if param == 'outNewNodeId' else '&native_' + param if pointer else 'native_' + param]
        header += [f'/** Submit native {native_name} with complete C89 inputs.',
                   ' * Inputs borrow during submission only; native code encodes the request.',
                   ' * Context borrows until one native completion, including timeout/shutdown.',
                   ' * Submission failure frees bridge state and produces no callback. IDs and',
                   ' * native status are returned unchanged. Request ID output is optional and',
                   ' * starts zero before an attempt. Reentrant submissions remain native.',
                   *([' * callback is required for native typed attribute reads.'] if callback_required else [' * NULL callback is accepted natively and creates no facade callback state.']),
                   *([' * Mutable requests retain native timestamp/handle/timeout-hint updates;',
                      ' * nested inputs and the restored authentication token remain caller-owned.'] if name.startswith('sendAsync') else []),
                   *([' * Upstream does not use outNewNodeId; it remains unchanged. Assigned IDs',
                      ' * are delivered in the AddNodesResponse callback. No output is invented.'] if name.startswith('add') else []),
                   *([' * Pass genuine native service request/response descriptors with matching',
                      ' * encoding types; this preserves the generalized upstream typing contract.'] if generic else []),
                   ' * Serialize client operations; callback results borrow until return. */',
                   f'cpkt_opcua_StatusCode cpkt_opcua_client_{name}_typed({c_signature});']
        invocation = [f'  if(!status) {{ status = {native_name}({", ".join(invoke)});']
        if name.startswith('sendAsync'):
            # Native sendRequest updates these scalar header fields in place.
            # The authentication token is restored and nested inputs stay borrowed.
            invocation += ['    request->requestHeader.requestHandle = native_request.requestHeader.requestHandle;',
                           '    request->requestHeader.timeoutHint = native_request.requestHeader.timeoutHint;',
                           f'    (void)cpkt_convert(&native_request.requestHeader.timestamp, &request->requestHeader.timestamp, &cpkt_types[{index["DateTime"]}], 0, 0);']
        invocation += ['  }']
        metadata += [f'cpkt_opcua_StatusCode cpkt_opcua_client_{name}_typed({c_signature}) {{',
                     '  UA_StatusCode status = 0;', f'  {context} *state;', *locals_,
                     f'  if({id_param}) *{id_param} = 0;',
                     f'  if({" || ".join(validation)}) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                     *([f'  if(!requestType->members_size || requestType->members[0].c_offset || requestType->members[0].is_array || requestType->members[0].type_index != {index["RequestHeader"]} || !responseType->members_size || responseType->members[0].c_offset || responseType->members[0].is_array || responseType->members[0].type_index != {index["ResponseHeader"]}) return UA_STATUSCODE_BADTYPEMISMATCH;'] if generic else []), *init,
                     '  state = NULL;', f'  if({cb_param}) {{',
                     f'    state = ({context} *)UA_calloc(1, sizeof(*state));',
                     '    if(!state) return UA_STATUSCODE_BADOUTOFMEMORY;',
                     f'  state->client = client; state->user = userdata; state->callback = {cb_param};',
                     '  state->type = ' + ('responseType' if generic else f'&cpkt_types[{index[result_type]}]') + ';', '  }',
                     *convert, *invocation, *clear,
                     '  if(status) UA_free(state);', '  return status;', '}']
    return header, metadata
