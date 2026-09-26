"""Generate schema-only server boundaries from authoritative public declarations.

The binding policy selects operations with owned outputs. Borrowed native
outputs and callback/configuration records require separate lifetime bridges.
"""
import re
from plugin_emitter import parameters, uncomment, translate


def selected(name):
    return (re.fullmatch(r'(read|write)[A-Z][A-Za-z]+', name) is not None or
            name in {'addVariableNode', 'addVariableTypeNode', 'addObjectNode',
                     'addObjectTypeNode', 'addReferenceTypeNode', 'addDataTypeNode',
                     'addViewNode', 'browse', 'browseNext', 'call',
                     'translateBrowsePathToNodeIds', 'deleteNode', 'addReference',
                     'deleteReference', 'getNodeContext', 'setNodeContext',
                     'closeSession', 'setSessionAttribute', 'deleteSessionAttribute',
                     'getSessionAttributeCopy', 'setVariableNodeDynamic',
                     'getNamespaceByName', 'getNamespaceByIndex'})


def emit_server(index, native_headers):
    text = uncomment((native_headers / 'server.h').read_text())
    text = re.sub(r'^\s*#.*$', '', text, flags=re.M)
    declarations = re.findall(r'((?:(?:UA_EXPORT|UA_THREADSAFE|UA_\w+)\s+)+)'
                              r'UA_Server_(\w+)\s*\((.*?)\)\s*;', text, re.S)
    selected_names = {name for name in re.findall(r'UA_Server_(\w+)\s*\(', text)
                      if selected(name)}
    parsed_names = {name for _, name, _ in declarations if selected(name)}
    if selected_names != parsed_names:
        raise ValueError(f'Adapt C89 server declaration parser: {selected_names - parsed_names}')
    header, metadata = [], []
    for prefixes, name, signature in declarations:
        if not selected(name):
            continue
        returns = [p for p in prefixes.split() if p not in ('UA_EXPORT', 'UA_THREADSAFE')]
        if len(returns) != 1:
            raise ValueError(f'Adapt server return declaration: {name}')
        result = returns[0]
        result_type = result[3:]
        if result_type not in index:
            raise ValueError(f'Adapt server result type: {name}.{result}')
        args = parameters(signature)
        args = [(spelling.replace('size_t*', 'size_t *'), param) for spelling, param in args]
        if args[0] != ('UA_Server *', 'server'):
            raise ValueError(f'Adapt server receiver: {name}')
        locals_, initialization, conversions, outputs, cleanup, native_args = [], [], [], [], [], []
        validation = ['!server']
        if result != 'UA_StatusCode':
            signature += f', UA_{result_type} *response'
            validation.append('!response')
            locals_.append(f'  UA_{result_type} native_response;')
            initialization.append('  memset(&native_response, 0, sizeof(native_response));')
            initialization.append('  memset(response, 0, sizeof(*response));')
            outputs.append(f'  if(!status) status = cpkt_convert(&native_response, response, &cpkt_types[{index[result_type]}], 0, 0);')
            cleanup.append(f'  UA_clear(&native_response, cpkt_types[{index[result_type]}].native);')
        for spelling, param in args:
            if param == 'server':
                native_args.append('server->server')
                continue
            if spelling in ('void *', 'void **', 'size_t', 'const size_t', 'size_t *'):
                native_args.append(param)
                if spelling.endswith('**') or spelling == 'size_t *':
                    validation.append('!' + param)
                continue
            bare = spelling.replace('const ', '').replace('*', '').strip()
            typename = bare[3:] if bare.startswith('UA_') else None
            if typename not in index or spelling.count('*') > 1:
                raise ValueError(f'Adapt server argument: {name}.{param}: {spelling}')
            type_ref = f'&cpkt_types[{index[typename]}]'
            locals_.append(f'  UA_{typename} native_{param};')
            initialization.append(f'  memset(&native_{param}, 0, sizeof(native_{param}));')
            cleanup.append(f'  UA_clear(&native_{param}, cpkt_types[{index[typename]}].native);')
            pointer = '*' in spelling
            is_output = pointer and not spelling.startswith('const ')
            if is_output:
                initialization.append(f'  if({param}) memset({param}, 0, sizeof(*{param}));')
                if param != 'outNewNodeId':
                    validation.append('!' + param)
                outputs.append(f'  if(!status && {param}) status = cpkt_convert(&native_{param}, {param}, {type_ref}, 0, 0);')
            else:
                if pointer:
                    validation.append('!' + param)
                conversions.append(f'  if(!status) status = cpkt_convert({param if pointer else "&" + param}, &native_{param}, {type_ref}, 1, 0);')
            native_args.append('&native_' + param if pointer else 'native_' + param)
        public_name = f'cpkt_opcua_server_{name}_typed'
        public_signature = translate(signature)
        header += [f'/** Call native UA_Server_{name} with complete C89 schema records.',
                   ' * Inputs are borrowed until return; outputs must start empty and are owned.',
                   ' * Return status reports native or conversion failure. For record results,',
                   ' * inspect response status fields separately. Output conversion failure after',
                   ' * node creation deletes the new node. Calls follow native serialization. */',
                   f'cpkt_opcua_StatusCode {public_name}({public_signature});']
        invocation = f'UA_Server_{name}({", ".join(native_args)})'
        metadata += [f'cpkt_opcua_StatusCode {public_name}({public_signature}) {{',
                     '  UA_StatusCode status = 0;', *locals_,
                     f'  if({" || ".join(validation)}) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                     *initialization, *conversions,
                     f'  if(!status) {"status" if result == "UA_StatusCode" else "native_response"} = {invocation};']
        if name.startswith('add') and name.endswith('Node'):
            metadata += ['  if(!status) {', *outputs,
                         '    if(status) UA_Server_deleteNode(server->server, native_outNewNodeId, UA_TRUE);', '  }']
        else:
            metadata += outputs
        # Empty failed outputs, including partially converted nested allocations.
        for spelling, param in args[1:] + ([('UA_' + result_type + ' *', 'response')] if result != 'UA_StatusCode' else []):
            if '*' in spelling and not spelling.startswith('const ') and spelling not in ('void *', 'void **', 'size_t *'):
                typename = spelling.replace('*', '').strip()[3:]
                metadata.append(f'  if(status && {param}) cpkt_opcua_type_clear({param}, &cpkt_types[{index[typename]}]);')
        metadata += [*cleanup, '  return status;', '}']
    util = (native_headers / 'util.h').read_text()
    for typename in re.findall(r'UA_EXPORT extern const UA_(\w+Attributes) UA_\w+_default;', util):
        if typename not in index:
            raise ValueError(f'Missing public attribute type: {typename}')
        public_name = f'cpkt_opcua_{typename}_default'
        header += [f'/** Owned C89 copy of upstream {typename} defaults; destination must start empty. */',
                   f'cpkt_opcua_StatusCode {public_name}(cpkt_opcua_{typename} *value);']
        metadata += [f'cpkt_opcua_StatusCode {public_name}(cpkt_opcua_{typename} *value) {{',
                     '  UA_StatusCode status;',
                     '  if(!value) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                     '  memset(value, 0, sizeof(*value));',
                     f'  status = cpkt_convert(&UA_{typename}_default, value, &cpkt_types[{index[typename]}], 0, 0);',
                     f'  if(status) cpkt_opcua_type_clear(value, &cpkt_types[{index[typename]}]);',
                     '  return status;', '}']
    return header, metadata
