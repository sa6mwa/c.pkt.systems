"""Generate the entire synchronous public high-level client header.

Ownership policy distinguishes the handful of mutable-input and counted-array
parameters. Calls remain the corresponding upstream entry points. Schema
conversion is shared with the rest of the facade; no client service is rebuilt.
"""
import re
from plugin_emitter import parameters, translate, uncomment


def public(text):
    return re.sub(r'\bcpkt_opcua_Client\b', 'cpkt_opcua_client', translate(text))


def declarations(text):
    text = uncomment(text)
    text = re.sub(r'^\s*_UA_(?:BEGIN|END)_DECLS\s*$', '', text, flags=re.M)
    found = re.findall(r'((?:(?:UA_EXPORT|UA_THREADSAFE|UA_\w+)\s+)+)'
                       r'UA_Client_(\w+)\s*\((.*?)\)\s*;', text, re.S)
    names = set(re.findall(r'UA_Client_(\w+)\s*\(', text))
    if names != {name for _, name, _ in found}:
        raise ValueError('Adapt synchronous client declaration parser')
    return found


def emit_client(index, native_headers):
    original = (native_headers / 'client_highlevel.h').read_text()
    callback = re.search(r'typedef\s+UA_Boolean\s*\(\*UA_HistoricalIteratorCallback\)\s*\((.*?)\)\s*;', uncomment(original), re.S)
    if not callback or parameters(callback[1]) != [
            ('UA_Client *', 'client'), ('const UA_NodeId *', 'nodeId'),
            ('UA_Boolean', 'moreDataAvailable'),
            ('const UA_ExtensionObject *', 'data'), ('void *', 'callbackContext')]:
        raise ValueError('Adapt historical iterator callback')
    header = [re.match(r'/\*.*?\*/', original, re.S)[0],
              '/** Receive one upstream history page. All arguments borrow until return;',
              ' * context is the original caller pointer. Return zero to stop; upstream',
              ' * releases continuation points. Copy records to retain them. Do not destroy',
              ' * the client here. Conversion failure skips dispatch and returns its status',
              ' * from the enclosing history call, after upstream continuation cleanup. */',
              public(callback[0])]
    metadata = history_bridge(index)
    for prefix, name, signature in declarations(original):
        results = [p for p in prefix.split() if p not in ('UA_EXPORT', 'UA_THREADSAFE')]
        if len(results) != 1 or results[0][3:] not in index:
            raise ValueError('Adapt synchronous client return: ' + name)
        result = results[0]
        args = [(re.sub(r'\s*\*', ' *', spelling), param) for spelling, param in parameters(signature)]
        if args[0] != ('UA_Client *', 'client'):
            raise ValueError('Adapt synchronous client receiver: ' + name)
        history = name.startswith('HistoryRead_')
        iterator = name == 'forEachChildNodeCall'
        array_input = {'call': ('input', 'inputSize', 'Variant'),
                       'writeArrayDimensionsAttribute': ('newArrayDimensions', 'newArrayDimensionsSize', 'UInt32')}.get(name)
        array_output = {'call': ('output', 'outputSize', 'Variant'),
                        'readArrayDimensionsAttribute': ('outArrayDimensions', 'outArrayDimensionsSize', 'UInt32')}.get(name)
        input_mutable = {'writeAccessLevelExAttribute': {'newAccessLevelEx'},
                         'HistoryUpdate_insert': {'value'}, 'HistoryUpdate_replace': {'value'},
                         'HistoryUpdate_update': {'value'}, 'NamespaceGetIndex': {'namespaceUri'}}.get(name, set())
        optional_inputs = {'browse': {'view', 'nodesToBrowse'}}.get(name, set())
        dynamic = name == 'writeValueAttribute_scalar'
        public_signature = public(signature).replace('cpkt_opcua_DataType *', 'cpkt_opcua_Type *')
        locals_, init, convert, outputs, clear, invoke, failures = [], [], [], [], [], [], []
        validation = ['!client', '!client->client']
        if history or iterator:
            validation += ['!callback']
            locals_ += ['  cpkt_client_history iterator;' if history else '  cpkt_client_iterator iterator;']
            init += ['  memset(&iterator, 0, sizeof(iterator));', '  iterator.callback = callback;']
            init += ['  iterator.client = client;', '  iterator.context = callbackContext;'] if history else ['  iterator.context = handle;']
        if dynamic:
            validation += ['!newValue', '!cpkt_valid_type(valueType)']
            locals_ += ['  void *native_newValue = NULL;']
            convert += ['  if(!status) {', '    native_newValue = UA_new(valueType->native);',
                        '    if(!native_newValue) status = UA_STATUSCODE_BADOUTOFMEMORY;',
                        '    else status = cpkt_convert(newValue, native_newValue, valueType, 1, 0);', '  }']
            clear += ['  if(native_newValue) UA_delete(native_newValue, valueType->native);']
        if array_output:
            param, count, typename = array_output
            required = name != 'call'
            if required:
                validation += ['!' + param, '!' + count]
            selected = '1' if required else f'{param} && {count}'
            locals_ += [f'  UA_{typename} *native_{param} = NULL;', f'  size_t native_{count} = 0;',
                        '  void *converted_array = NULL;']
            init += [f'  if({selected}) {{ *{param} = NULL; *{count} = 0; }}']
            # Native call() accepts every non-Bad result, including GoodClamped.
            outputs += [f'    if(!UA_StatusCode_isBad(native_status) && ({selected})) {{',
                        f'      status = cpkt_array(native_{param}, native_{count}, &converted_array, &cpkt_types[{index[typename]}], 0, 0);',
                        f'      if(!status) {{ *{param} = (cpkt_opcua_{typename} *)converted_array; *{count} = native_{count}; converted_array = NULL; }}',
                        '    }']
            clear += [f'  UA_Array_delete(native_{param}, native_{count}, cpkt_types[{index[typename]}].native);',
                      f'  cpkt_opcua_array_delete(converted_array, native_{count}, &cpkt_types[{index[typename]}]);']
        if result != 'UA_StatusCode':
            typename = result[3:]
            public_signature += f', cpkt_opcua_{typename} *response'
            validation += ['!response']
            locals_ += [f'  {result} native_response;']
            init += ['  memset(response, 0, sizeof(*response));', '  memset(&native_response, 0, sizeof(native_response));']
            outputs += [f'    status = cpkt_convert(&native_response, response, &cpkt_types[{index[typename]}], 0, 0);']
            failures += [f'  if(status) cpkt_opcua_type_clear(response, &cpkt_types[{index[typename]}]);']
            clear += [f'  UA_clear(&native_response, cpkt_types[{index[typename]}].native);']
        for spelling, param in args:
            if param == 'client':
                invoke += ['client->client']; continue
            if param == 'callback' and (history or iterator):
                expected = 'const UA_HistoricalIteratorCallback' if history else 'UA_NodeIteratorCallback'
                if spelling != expected:
                    raise ValueError('Adapt synchronous client callback: ' + name)
                invoke += ['cpkt_client_history_invoke' if history else 'cpkt_client_iterator_invoke']; continue
            if (param == 'callbackContext' and history) or (param == 'handle' and iterator):
                if spelling != 'void *': raise ValueError('Adapt iterator context')
                invoke += ['&iterator']; continue
            if dynamic and param in ('newValue', 'valueType'):
                if spelling != {'newValue': 'const void *', 'valueType': 'const UA_DataType *'}[param]:
                    raise ValueError('Adapt scalar client write')
                invoke += ['native_newValue' if param == 'newValue' else 'valueType->native']; continue
            if array_input and param in array_input[:2]:
                array, count, typename = array_input
                if spelling != (f'const UA_{typename} *' if param == array else 'size_t'):
                    raise ValueError('Adapt counted client input: ' + name)
                if param == count:
                    invoke += [count]; continue
                locals_ += [f'  void *native_{array} = NULL;']
                convert += [f'  if(!status) status = cpkt_array({array}, {count}, &native_{array}, &cpkt_types[{index[typename]}], 1, 0);']
                clear += [f'  if(native_{array}) UA_Array_delete(native_{array}, {count}, cpkt_types[{index[typename]}].native);']
                invoke += [f'(const UA_{typename} *)native_{array}']; continue
            if array_output and param in array_output[:2]:
                array, count, typename = array_output
                if spelling != (f'UA_{typename} * *' if param == array else 'size_t *'):
                    raise ValueError('Adapt counted client output: ' + name + '.' + param + ': ' + spelling)
                selected = '1' if name != 'call' else f'{array} && {count}'
                invoke += [f'({selected}) ? &native_{param} : NULL']; continue
            typename = spelling.replace('const ', '').replace('*', '').strip()[3:]
            if not spelling.replace('const ', '').startswith('UA_') or typename not in index or spelling.count('*') > 1:
                raise ValueError('Adapt synchronous client argument: ' + name + '.' + param + ': ' + spelling)
            pointer = '*' in spelling
            output = pointer and not spelling.startswith('const ') and param not in input_mutable
            ref = f'&cpkt_types[{index[typename]}]'
            locals_ += [f'  UA_{typename} native_{param};']
            init += [f'  memset(&native_{param}, 0, sizeof(native_{param}));']
            clear += [f'  UA_clear(&native_{param}, cpkt_types[{index[typename]}].native);']
            if output:
                if param != 'outNewNodeId': validation += ['!' + param]
                if name == 'NamespaceGetIndex':
                    outputs += [f'    if(!native_status) status = cpkt_convert(&native_{param}, {param}, {ref}, 0, 0);']
                else:
                    init += [f'  if({param}) memset({param}, 0, sizeof(*{param}));']
                    # Native reads accept Good with lower information bits;
                    # their top-16 comparison rejects other Good subcodes too.
                    success = 'UA_StatusCode_isEqualTop(native_status, UA_STATUSCODE_GOOD)' if name.startswith('read') else '!native_status'
                    outputs += [f'    if({success} && {param}) status = cpkt_convert(&native_{param}, {param}, {ref}, 0, 0);']
                    failures += [f'  if(status && {param}) cpkt_opcua_type_clear({param}, {ref});']
                invoke += [f'{param} ? &native_{param} : NULL']
            else:
                if pointer and param not in optional_inputs: validation += ['!' + param]
                source = param if pointer else '&' + param
                condition = f'!status && {param}' if pointer and param in optional_inputs else '!status'
                convert += [f'  if({condition}) status = cpkt_convert({source}, &native_{param}, {ref}, 1, 0);']
                invoke += [f'{param} ? &native_{param} : NULL' if pointer and param in optional_inputs else '&native_' + param if pointer else 'native_' + param]
        extra = []
        if name == 'call':
            extra += [' * Output array/count are optional as a pair; if either is NULL both are',
                      ' * discarded by native code and the other is unchanged. Non-Bad native',
                      ' * statuses can return outputs. Clear owned arrays with array_delete.']
        elif array_output:
            extra += [' * Array/count outputs are required and start empty. Clear the owned',
                      ' * array with array_delete using the matching public UInt32 descriptor.']
        elif name == 'NamespaceGetIndex':
            extra += [' * namespaceUri is borrowed input despite its upstream mutable spelling.',
                      ' * namespaceIndex is unchanged on failure, as upstream specifies.']
        elif name.startswith('read') and name.endswith('Attribute'):
            extra += [' * Native reads return values for plain Good with lower information bits.',
                      ' * Other top-16 codes, including GoodClamped and Uncertain/Bad, return',
                      ' * their native status without a value. This differs from native call().']
        elif name.startswith('add') and name.endswith('Node'):
            extra += [' * outNewNodeId is optional. Conversion failure after native creation does',
                      ' * not undo remote node creation; inspect the requested ID or browse the',
                      ' * server. No remote rollback or additional service is invented.']
        elif history:
            extra += [' * Each native page is converted independently; no history collection is',
                      ' * accumulated by the facade. Pagination and cleanup remain upstream.']
        elif iterator:
            extra += [' * Native iteration controls ordering and aggregates callback status bits;',
                      ' * it does not stop on a callback error. Callback values borrow until return.']
        header += [f'/** Invoke native UA_Client_{name} with complete C89 types.',
                   ' * Inputs borrow until return; outputs must start empty and are owned unless',
                   ' * stated otherwise. Return status is native or conversion failure. Record',
                   ' * results retain their native status fields. Follow native client serialization.',
                   *extra, ' */', f'cpkt_opcua_StatusCode cpkt_opcua_client_{name}_typed({public_signature});']
        metadata += [f'cpkt_opcua_StatusCode cpkt_opcua_client_{name}_typed({public_signature}) {{',
                     '  UA_StatusCode status = 0, native_status = 0;', *locals_,
                     f'  if({" || ".join(validation)}) return UA_STATUSCODE_BADINVALIDARGUMENT;', *init]
        if name == 'writeArrayDimensionsAttribute':
            metadata += ['  if(!newArrayDimensions) return UA_STATUSCODE_BADTYPEMISMATCH;']
        metadata += [*convert, '  if(!status) {',
                     f'    {"native_status" if result == "UA_StatusCode" else "native_response"} = UA_Client_{name}({", ".join(invoke)});',
                     *outputs, '  }']
        if history: metadata += ['  if(!status && iterator.status) status = iterator.status;']
        metadata += [*failures, *clear, '  return status ? status : native_status;', '}']
    return header, metadata


def history_bridge(index):
    return [
        'typedef struct { cpkt_opcua_client *client; cpkt_opcua_HistoricalIteratorCallback callback; void *context; UA_StatusCode status; } cpkt_client_history;',
        'static UA_Boolean cpkt_client_history_invoke(UA_Client *client, const UA_NodeId *node, UA_Boolean more, const UA_ExtensionObject *data, void *context) {',
        '  cpkt_client_history *iterator = (cpkt_client_history *)context;',
        '  cpkt_opcua_NodeId c_node;', '  cpkt_opcua_ExtensionObject c_data;',
        '  UA_Boolean next = 0;', '  (void)client;',
        '  cpkt_opcua_NodeId_init(&c_node);', '  cpkt_opcua_ExtensionObject_init(&c_data);',
        f'  iterator->status = cpkt_convert(node, &c_node, &cpkt_types[{index["NodeId"]}], 0, 0);',
        f'  if(!iterator->status) iterator->status = cpkt_convert(data, &c_data, &cpkt_types[{index["ExtensionObject"]}], 0, 0);',
        '  if(!iterator->status) next = iterator->callback(iterator->client, &c_node, more, &c_data, iterator->context) != 0;',
        '  cpkt_opcua_NodeId_clear(&c_node);', '  cpkt_opcua_ExtensionObject_clear(&c_data);', '  return next;', '}',
        'typedef struct { cpkt_opcua_NodeIteratorCallback callback; void *context; } cpkt_client_iterator;',
        'static UA_StatusCode cpkt_client_iterator_invoke(UA_NodeId child, UA_Boolean inverse, UA_NodeId reference, void *context) {',
        '  cpkt_client_iterator *iterator = (cpkt_client_iterator *)context;',
        '  cpkt_opcua_NodeId c_child, c_reference;', '  UA_StatusCode status;',
        '  cpkt_opcua_NodeId_init(&c_child);', '  cpkt_opcua_NodeId_init(&c_reference);',
        f'  status = cpkt_convert(&child, &c_child, &cpkt_types[{index["NodeId"]}], 0, 0);',
        f'  if(!status) status = cpkt_convert(&reference, &c_reference, &cpkt_types[{index["NodeId"]}], 0, 0);',
        '  if(!status) status = iterator->callback(c_child, inverse, c_reference, iterator->context);',
        '  cpkt_opcua_NodeId_clear(&c_child);', '  cpkt_opcua_NodeId_clear(&c_reference);', '  return status;', '}']
