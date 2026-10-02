"""Generate schema-only server boundaries from authoritative public declarations.

The binding policy selects operations with owned outputs. Borrowed native
outputs and callback/configuration records require separate lifetime bridges.
"""
import re
from plugin_emitter import parameters, uncomment, translate

BASIC_PUBSUB = {
    'disableAllPubSubComponents', 'enableAllPubSubComponents',
    'disableDataSetReader', 'disableDataSetWriter', 'disablePubSubConnection',
    'disableReaderGroup', 'disableWriterGroup', 'enableDataSetReader',
    'enableDataSetWriter', 'enablePubSubConnection', 'enableReaderGroup',
    'enableWriterGroup', 'removeDataSetField', 'removeDataSetReader',
    'removeDataSetWriter', 'removePubSubConnection', 'removePublishedDataSet',
    'removeReaderGroup', 'removeSubscribedDataSet', 'removeWriterGroup',
    'triggerWriterGroupPublish', 'processPubSubConnectionReceive',
    'getPublishedDataSetMetaData', 'getWriterGroupLastPublishTimestamp',
    'getPubSubComponentType', 'getPubSubComponentParent',
    'getPubSubComponentChildren', 'getDataSetReaderState',
    'getDataSetWriterState', 'getReaderGroupState', 'getWriterGroupState',
    'setReaderGroupEncryptionKeys', 'setWriterGroupEncryptionKeys',
    'setDataSetReaderTargetVariables',
}
ARRAY_INPUTS = {
    'browseSimplifiedBrowsePath': {'browsePath': 'browsePathSize'},
    'addCertificates': {'certificates': 'certificatesSize', 'crls': 'crlsSize'},
    'removeCertificates': {'certificates': 'certificatesSize'},
    'setDataSetReaderTargetVariables': {'targetVariables': 'targetVariablesSize'},
}
ARRAY_OUTPUTS = {
    'browseRecursive': ('results', 'resultsSize', 'ExpandedNodeId'),
    'getPubSubComponentChildren': ('outChildren', 'outChildrenSize', 'NodeId'),
}
PUBLIC_ENUMS = {'LifecycleState', 'PubSubState', 'PubSubComponentType'}
OPTIONAL_INPUTS = {
    'updateCertificate': {'privateKey'},
    'createSigningRequest': {'subjectName', 'regenerateKey', 'nonce'},
}


def selected(name):
    return (name in BASIC_PUBSUB or name in OPTIONAL_INPUTS or
            name in {'deleteMonitoredItem', 'removeReverseConnect',
                     'browseRecursive', 'browseSimplifiedBrowsePath',
                     'addCertificates', 'removeCertificates',
                     'getLifecycleState', 'getStatistics',
                     'setAdminSessionContext'} or
            re.fullmatch(r'(read|write)[A-Z][A-Za-z]+', name) is not None or
            name in {'addVariableNode', 'addVariableTypeNode', 'addObjectNode',
                     'addObjectTypeNode', 'addReferenceTypeNode', 'addDataTypeNode',
                     'addViewNode', 'browse', 'browseNext', 'call',
                     'translateBrowsePathToNodeIds', 'deleteNode', 'addReference',
                     'deleteReference', 'getNodeContext', 'setNodeContext',
                     'closeSession', 'setSessionAttribute', 'deleteSessionAttribute',
                     'getSessionAttributeCopy', 'setVariableNodeDynamic',
                     'getNamespaceByName', 'getNamespaceByIndex'})


def emit_server(index, native_headers):
    server_text = uncomment((native_headers / 'server.h').read_text())
    names = {name for name in re.findall(r'UA_Server_(\w+)\s*\(', server_text)
             if selected(name)} | BASIC_PUBSUB
    text = server_text + '\n' + uncomment((native_headers / 'server_pubsub.h').read_text())
    text = re.sub(r'^\s*#.*$', '', text, flags=re.M)
    declarations = re.findall(r'((?:(?:UA_EXPORT|UA_THREADSAFE|UA_\w+|void)\s+)+)'
                              r'UA_Server_(\w+)\s*\((.*?)\)\s*;', text, re.S)
    selected_names = names
    parsed_names = {name for _, name, _ in declarations if name in names}
    if selected_names != parsed_names:
        raise ValueError(f'Adapt C89 server declaration parser: {selected_names - parsed_names}')
    header, metadata = [], []
    for prefixes, name, signature in declarations:
        if name not in names:
            continue
        returns = [p for p in prefixes.split() if p not in ('UA_EXPORT', 'UA_THREADSAFE')]
        if len(returns) != 1:
            raise ValueError(f'Adapt server return declaration: {name}')
        result = returns[0]
        result_type = result[3:]
        if result != 'void' and result_type not in index and result_type not in {'DataSetFieldResult', 'ServerStatistics'} | PUBLIC_ENUMS:
            raise ValueError(f'Adapt server result type: {name}.{result}')
        args = parameters(signature)
        args = [(spelling.replace('size_t*', 'size_t *'), param) for spelling, param in args]
        if args[0] != ('UA_Server *', 'server'):
            raise ValueError(f'Adapt server receiver: {name}')
        locals_, initialization, conversions, outputs, cleanup, native_args = [], [], [], [], [], []
        validation = ['!server', '!server->server']
        array_inputs = ARRAY_INPUTS.get(name, {})
        array_output = ARRAY_OUTPUTS.get(name)
        input_arrays = set()
        if result not in ('UA_StatusCode', 'void'):
            signature += f', UA_{result_type} *response'
            validation.append('!response')
            locals_.append(f'  UA_{result_type} native_response;')
            initialization.append('  memset(&native_response, 0, sizeof(native_response));')
            initialization.append('  memset(response, 0, sizeof(*response));')
            if result_type in PUBLIC_ENUMS:
                outputs.append(f'  if(!status) *response = (cpkt_opcua_{result_type})native_response;')
            elif result_type in {'DataSetFieldResult', 'ServerStatistics'}:
                outputs.append(f'  if(!status) status = cpkt_result_{result_type}(&native_response, response);')
            else:
                outputs.append(f'  if(!status) status = cpkt_convert(&native_response, response, &cpkt_types[{index[result_type]}], 0, 0);')
                cleanup.append(f'  UA_clear(&native_response, cpkt_types[{index[result_type]}].native);')
        for spelling, param in args:
            if param == 'server':
                native_args.append('server->server')
                continue
            if array_output and param in array_output[:2]:
                array, count, typename = array_output
                validation.append('!' + param)
                if param == count:
                    if spelling != 'size_t *':
                        raise ValueError('Adapt server array count: ' + name)
                    initialization.append(f'  *{count} = 0;')
                    native_args.append('&native_count')
                    continue
                if spelling.replace(' ', '') != 'UA_' + typename + '**':
                    raise ValueError('Adapt server array output: ' + name)
                locals_ += [f'  UA_{typename} *native_{array} = NULL;', '  size_t native_count = 0;']
                initialization.append(f'  *{array} = NULL;')
                native_args.append('&native_' + array)
                outputs += [f'  if(!status) status = cpkt_array(native_{array}, native_count, (void **){array}, &cpkt_types[{index[typename]}], 0, 0);',
                            f'  if(!status) *{count} = native_count;',
                            f'  else {{ cpkt_opcua_array_delete(*{array}, native_count, &cpkt_types[{index[typename]}]); *{array} = NULL; }}']
                cleanup.append(f'  if(native_{array}) UA_Array_delete(native_{array}, native_count, cpkt_types[{index[typename]}].native);')
                continue
            if param in array_inputs:
                typename = spelling.replace('const ', '').replace('*', '').strip()[3:]
                count = array_inputs[param]
                if typename not in index or spelling.count('*') != 1 or ('size_t', count) not in args:
                    raise ValueError('Adapt server array input: ' + name + '.' + param)
                locals_.append(f'  void *native_{param} = NULL;')
                conversions.append(f'  if(!status) status = cpkt_array({param}, {count}, &native_{param}, &cpkt_types[{index[typename]}], 1, 0);')
                cleanup.append(f'  if(native_{param}) UA_Array_delete(native_{param}, {count}, cpkt_types[{index[typename]}].native);')
                native_args.append(f'(UA_{typename} *)native_{param}')
                input_arrays.add(param)
                continue
            if spelling in ('void *', 'void **', 'size_t', 'const size_t', 'size_t *'):
                native_args.append(param)
                if spelling.endswith('**') or spelling == 'size_t *':
                    validation.append('!' + param)
                continue
            bare = spelling.replace('const ', '').replace('*', '').strip()
            typename = bare[3:] if bare.startswith('UA_') else None
            if typename in PUBLIC_ENUMS:
                if spelling != 'UA_' + typename + ' *':
                    raise ValueError('Adapt server enum argument: ' + name)
                validation.append('!' + param)
                locals_.append(f'  UA_{typename} native_{param};')
                initialization.append(f'  native_{param} = (UA_{typename})0;')
                outputs.append(f'  if(!status) *{param} = (cpkt_opcua_{typename})native_{param};')
                native_args.append('&native_' + param)
                continue
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
                optional = pointer and param in OPTIONAL_INPUTS.get(name, set())
                if pointer and not optional:
                    validation.append('!' + param)
                conversions.append(f'  if(!status{" && " + param if optional else ""}) status = cpkt_convert({param if pointer else "&" + param}, &native_{param}, {type_ref}, 1, 0);')
            native_args.append(f'{param} ? &native_{param} : NULL' if pointer and param in OPTIONAL_INPUTS.get(name, set()) else '&native_' + param if pointer else 'native_' + param)
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
                     '  UA_StatusCode status = 0;', '  int prepared = 0;', *locals_,
                     f'  if({" || ".join(validation)}) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                     *initialization, *conversions,
                     '  if(!status && server->typed_pubsub_prepare) { status = server->typed_pubsub_prepare(server); prepared = !status; }',
                     f'  if(!status) ' + ('' if result == 'void' else ('status' if result == 'UA_StatusCode' else 'native_response') + ' = ') + invocation + ';']
        if name == 'removeReverseConnect':
            metadata += ['  if(!status) cpkt_reverse_removed(server, native_handle);']
        if name == 'deleteMonitoredItem':
            metadata += ['  if(!status) cpkt_local_monitor_remove(server, monitoredItemId);']
        if name.startswith('add') and name.endswith('Node'):
            metadata += ['  if(!status) {', *outputs,
                         '    if(status) UA_Server_deleteNode(server->server, native_outNewNodeId, UA_TRUE);', '  }']
        else:
            metadata += outputs
        # Empty failed outputs, including partially converted nested allocations.
        for spelling, param in args[1:] + ([('UA_' + result_type + ' *', 'response')] if result not in ('UA_StatusCode', 'void') else []):
            if param in input_arrays or (array_output and param in array_output[:2]):
                continue
            if '*' in spelling and not spelling.startswith('const ') and spelling not in ('void *', 'void **', 'size_t *'):
                typename = spelling.replace('*', '').strip()[3:]
                if typename in {'DataSetFieldResult', 'ServerStatistics'}:
                    metadata.append(f'  if(status && {param}) memset({param}, 0, sizeof(*{param}));')
                elif typename in PUBLIC_ENUMS:
                    continue
                else:
                    metadata.append(f'  if(status && {param}) cpkt_opcua_type_clear({param}, &cpkt_types[{index[typename]}]);')
        metadata += [*cleanup, '  if(prepared) server->typed_pubsub_finish(server);', '  return status;', '}']
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
