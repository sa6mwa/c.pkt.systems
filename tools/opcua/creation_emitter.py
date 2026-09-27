"""Generate node creation calls from the public server declarations.

Schema conversion is shared with every typed boundary. This policy identifies
callback registration and array/dynamic-type parameters; native creation and
its callbacks remain native, including the begin/finish interfaces.
"""
import re
from plugin_emitter import parameters, translate, uncomment

NAMES = ('addCallbackValueSourceVariableNode', 'addMethodNode', 'addMethodNodeEx',
         'addMethodNode_finish', 'addNode_begin', 'addNode_finish')


def emit_creation(index, native_headers):
    text = uncomment((native_headers / 'server.h').read_text())
    header, metadata = [], []
    for name in NAMES:
        match = re.search(r'((?:(?:UA_StatusCode|UA_EXPORT|UA_THREADSAFE)\s+)+)UA_Server_' +
                          name + r'\s*\((.*?)\)\s*;', text, re.S)
        if not match:
            raise ValueError('Adapt public creation declaration: ' + name)
        if set(match[1].split()) != {'UA_StatusCode', 'UA_EXPORT', 'UA_THREADSAFE'}:
            raise ValueError('Adapt creation modifiers: ' + name)
        signature = match[2]
        args = parameters(signature)
        if args[0] != ('UA_Server *', 'server'):
            raise ValueError('Adapt creation receiver: ' + name)
        public_signature = translate(signature).replace('cpkt_opcua_DataType *', 'cpkt_opcua_Type *')
        public_name = 'cpkt_opcua_server_' + name + '_typed'
        declarations, initialize, conversions, cleanup, outputs, invocation = [], [], [], [], [], []
        validate = ['!server', '!server->server', 'server->destroying']
        scope = name in NAMES[:4]
        new_node = name in NAMES[:3] or name == 'addNode_begin'
        identity = 'native_outNewNodeId' if new_node else 'native_nodeId'
        if scope:
            declarations += ['  cpkt_creation creation;']
            initialize += ['  memset(&creation, 0, sizeof(creation));']
        dynamic = name == 'addNode_begin'
        if dynamic:
            declarations += ['  void *native_attr = NULL;']
            validate += ['!attr', '!cpkt_valid_type(attributeType)']
            conversions += ['  if(!status) {',
                            '    native_attr = UA_new(attributeType->native);',
                            '    if(!native_attr) status = UA_STATUSCODE_BADOUTOFMEMORY;',
                            '    else status = cpkt_convert(attr, native_attr, attributeType, 1, 0);', '}']
            cleanup += ['  if(native_attr) UA_delete(native_attr, attributeType->native);']
        for spelling, param in args:
            if param == 'server':
                invocation.append('server->server')
                continue
            if spelling == 'void *' and param == 'nodeContext':
                invocation.append(param)
                continue
            if spelling == 'const void *' and param == 'attr' and dynamic:
                invocation.append('native_attr')
                continue
            if spelling == 'const UA_DataType *' and param == 'attributeType' and dynamic:
                invocation.append('attributeType->native')
                continue
            if spelling == 'size_t' and param in ('inputArgumentsSize', 'outputArgumentsSize'):
                invocation.append(param)
                continue
            if spelling == 'const UA_Argument *' and param in ('inputArguments', 'outputArguments'):
                count = param + 'Size'
                if ('size_t', count) not in args:
                    raise ValueError('Adapt creation array count: ' + name)
                declarations += [f'  void *native_{param} = NULL;']
                conversions += [f'  if(!status) status = cpkt_array({param}, {count}, &native_{param}, &cpkt_types[{index["Argument"]}], 1, 0);']
                cleanup += [f'  if(native_{param}) UA_Array_delete(native_{param}, {count}, cpkt_types[{index["Argument"]}].native);']
                invocation.append(f'(const UA_Argument *)native_{param}')
                continue
            if spelling == 'const UA_CallbackValueSource' and param == 'evs' and scope:
                declarations += ['  UA_CallbackValueSource native_evs;']
                initialize += ['  native_evs.read = evs.read ? cpkt_producer_native_read : NULL;',
                               '  native_evs.write = evs.write ? cpkt_producer_native_write : NULL;']
                invocation.append('native_evs')
                continue
            if spelling == 'UA_MethodCallback' and param == 'method' and scope:
                invocation.append('method ? cpkt_producer_native_method : NULL')
                continue
            bare = spelling.replace('const ', '').replace('*', '').strip()
            typename = bare[3:] if bare.startswith('UA_') else None
            if typename not in index or spelling.count('*') > 1:
                raise ValueError(f'Adapt creation argument: {name}.{param}: {spelling}')
            pointer = '*' in spelling
            output = pointer and not spelling.startswith('const ')
            if output and typename != 'NodeId':
                raise ValueError('Adapt creation output: ' + name + '.' + param)
            declarations += [f'  UA_{typename} native_{param};']
            initialize += [f'  memset(&native_{param}, 0, sizeof(native_{param}));']
            cleanup += [f'  UA_clear(&native_{param}, cpkt_types[{index[typename]}].native);']
            if output:
                initialize += [f'  if({param}) memset({param}, 0, sizeof(*{param}));']
                outputs += [f'    if(!conversion_status && {param}) conversion_status = cpkt_convert(&native_{param}, {param}, &cpkt_types[{index[typename]}], 0, 0);']
            else:
                if pointer:
                    validate += ['!' + param]
                conversions += [f'  if(!status) status = cpkt_convert({param if pointer else "&" + param}, &native_{param}, &cpkt_types[{index[typename]}], 1, 0);']
            invocation.append('&native_' + param if pointer else 'native_' + param)
        if dynamic and ('const UA_DataType *', 'attributeType') not in args:
            raise ValueError('Adapt dynamic creation type descriptor')
        if scope and (('const UA_CallbackValueSource', 'evs') not in args and
                      ('UA_MethodCallback', 'method') not in args):
            raise ValueError('Adapt creation callback: ' + name)
        header += [f'/** Invoke native UA_Server_{name}, including its constructor ordering.',
                   ' * Inputs are borrowed during the call; callback records/pointers are copied,',
                   ' * and original node contexts stay caller-owned. Assigned IDs work during',
                   ' * constructors; callbacks may replace themselves or create other nodes.',
                   ' * Output NodeIds must start empty and own allocations. Native failure may',
                   ' * still return an assigned ID/node; this preserves upstream rollback rules.',
                   ' * Output conversion failure after successful creation deletes that node.',
                   ' * Clear returned IDs even on native failure. Serialize calls on the server. */',
                   f'cpkt_opcua_StatusCode {public_name}({public_signature});']
        metadata += [f'cpkt_opcua_StatusCode {public_name}({public_signature}) {{',
                     '  UA_StatusCode status = 0, native_status = 0, conversion_status = 0;',
                     *declarations,
                     f'  if({" || ".join(validate)}) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                     *initialize, *conversions]
        if scope:
            metadata += [f'  if(!status) status = cpkt_creation_begin(server, &{identity}, {int(name == "addMethodNode_finish")}, &creation);']
            if name == 'addCallbackValueSourceVariableNode':
                metadata += ['  if(!status) creation.entry->source = evs;']
            else:
                metadata += ['  if(!status) creation.entry->method = method;']
        metadata += ['  if(!status) {',
                     f'    native_status = UA_Server_{name}({", ".join(invocation)});',
                     *outputs]
        if outputs:
            metadata += ['    if(conversion_status) {']
            for spelling, param in args:
                if spelling == 'UA_NodeId *':
                    metadata += [f'      if({param}) cpkt_opcua_NodeId_clear({param});']
            if new_node:
                metadata += [f'      if(!native_status) cpkt_creation_rollback(server, &{identity}, {int(name in ("addMethodNode", "addMethodNodeEx"))});']
            metadata += ['    }']
        metadata += ['    status = conversion_status ? conversion_status : native_status;', '  }']
        if scope:
            metadata += [f'  cpkt_creation_end(&creation, &{identity});']
        metadata += ['  if(server->typed_producers_refresh) server->typed_producers_refresh(server);',
                     *cleanup, '  return status;', '}']
    return header, metadata
