"""Derive history backend callbacks from authoritative public native headers."""
import re
from plugin_emitter import public_body, callbacks, parameters, validate_fields, translate, uncomment


def emit_backend(index, native_headers, configuration):
    path = native_headers / 'plugin/historydata/history_data_backend.h'
    text = path.read_text()
    body = public_body(path, 'HistoryDataBackend', configuration)
    validate_fields(body, ['void *context'])
    methods = callbacks(body)
    if {name for _, name, _ in methods} != set(re.findall(r'\(\*(\w+)\)', uncomment(body))):
        raise ValueError('Adapt history backend callback declarations')
    enum = re.search(r'typedef enum \{(.*?)\} MatchStrategy;', text, re.S)
    if not enum:
        raise ValueError('Adapt history match strategy')
    match_enum = re.sub(r'\bMATCH_', 'CPKT_OPCUA_MATCH_', enum[0]).replace('MatchStrategy', 'cpkt_opcua_MatchStrategy')
    gathering = (native_headers / 'plugin/historydata/history_data_gathering.h').read_text()
    strategy = re.search(r'typedef enum \{(.*?)\} UA_HistorizingUpdateStrategy;', gathering, re.S)
    settings = re.search(r'typedef struct \{(.*?)\} UA_HistorizingNodeIdSettings;', gathering, re.S)
    if not strategy or not settings:
        raise ValueError('Adapt historizing settings declarations')
    validate_fields(settings[1], ['UA_HistoryDataBackend historizingBackend', 'size_t maxHistoryDataResponseSize',
                                 'UA_HistorizingUpdateStrategy historizingUpdateStrategy', 'size_t pollingInterval', 'void *userContext'])
    native_types = (native_headers / 'types.h').read_text()
    dimensions = re.search(r'typedef struct \{([^{}]*)\} UA_NumericRangeDimension;', native_types, re.S)
    numeric_range = re.search(r'typedef struct\s*\{([^{}]*)\} UA_NumericRange;', native_types, re.S)
    if not dimensions or not numeric_range:
        raise ValueError('Adapt numeric range declarations')
    validate_fields(dimensions[1], ['UA_UInt32 min', 'UA_UInt32 max'])
    validate_fields(numeric_range[1], ['size_t dimensionsSize', 'UA_NumericRangeDimension *dimensions'])
    public_backend = translate(body).replace('const cpkt_opcua_DataValue*\n    (*getDataValue)',
        '/** Return backend-owned persistent storage for a valid index.\n'
        '     * The native pointer remains borrowed after return and across later calls.\n'
        '     * Keep distinct retained values in distinct objects; finish all borrowers\n'
        '     * before set/free. NULL is forwarded, but the native engine may dereference it. */\n'
        '    const cpkt_opcua_history_value*\n    (*getDataValue)').replace('MatchStrategy', 'cpkt_opcua_MatchStrategy')
    public_backend = public_backend.replace('cpkt_opcua_StatusCode\n    (*copyDataValues)',
        '/** Fill the empty owned C89 array, at most valueSize records.\n'
        '     * providedValues reports the initialized count. The bridge stages native\n'
        '     * outputs and clears every C89 slot after a custom callback returns.\n'
        '     * Calling a stock factory slot instead returns caller-owned outputs. */\n'
        '    cpkt_opcua_StatusCode\n    (*copyDataValues)')
    public_backend = public_backend.replace('cpkt_opcua_TRUE', 'nonzero')
    header = [re.match(r'/\*.*?\*/', text, re.S)[0],
              re.match(r'/\*.*?\*/', native_types, re.S)[0],
              '/** Inclusive numeric range dimension, borrowed during history callbacks. */', translate(dimensions[0]),
              '/** History callback range; dimensions are borrowed until callback return. */', translate(numeric_range[0]),
              '/** Native historical timestamp matching criteria. */', match_enum,
              '/** Native historical-value collection policy. POLL requires start_poll. */',
              translate(strategy[0]).replace('cpkt_opcua_HISTORIZING', 'CPKT_OPCUA_HISTORIZING'),
              '/** Opaque actual native DataValue storage. Owned values created below retain',
              ' * their address across callbacks; stock backend slots return const borrows.',
              ' * Do not set/free it while any native history operation may borrow it.',
              ' * Synchronize callbacks and storage changes using the upstream server rules. */',
              'typedef struct cpkt_opcua_history_value cpkt_opcua_history_value;',
              '/** Deep-copy value into persistent storage. On failure *out is NULL. */',
              'cpkt_opcua_StatusCode cpkt_opcua_history_value_new(const cpkt_opcua_DataValue *value, cpkt_opcua_history_value **out);',
              '/** Replace a quiescent stored value. Failure preserves the previous value.',
              ' * The object address stays stable; existing borrowers must finish first.',
              ' * Only owned objects from history_value_new may be set. */',
              'cpkt_opcua_StatusCode cpkt_opcua_history_value_set(cpkt_opcua_history_value *stored, const cpkt_opcua_DataValue *value);',
              '/** Return an owned C89 copy into empty out. Clear with DataValue_clear.',
              ' * Failure leaves out empty; this does not expose a native borrowed pointer. */',
              'cpkt_opcua_StatusCode cpkt_opcua_history_value_get(const cpkt_opcua_history_value *stored, cpkt_opcua_DataValue *out);',
              '/** Release a quiescent stored value; NULL is safe. Normally called from',
              ' * backend deleteMembers after the gathering stops using that backend.',
              ' * Only owned objects from history_value_new may be freed. */',
              'void cpkt_opcua_history_value_free(cpkt_opcua_history_value *stored);',
              '/** Full public history storage backend. Inputs borrow until return; output',
              ' * records/arrays own C89 allocations. The bridge clears custom callback',
              ' * outputs after conversion; callers clear stock factory slot outputs.',
              ' * getDataValue returns backend-owned persistent storage, never a temporary.',
              ' * Returning another value must not invalidate earlier borrowed values.',
              ' * NULL and all native callback semantics are preserved. Conversion failures',
              ' * return status, NULL or zero as appropriate and are logged for non-status',
              ' * hooks. copyDataValues must not exceed valueSize; providedValues is output.',
              ' * deleteMembers releases context and persistent values after native cleanup.',
              ' * Use getHistoryData OR all low-level read slots, as upstream requires. */',
              'typedef struct cpkt_opcua_HistoryDataBackend cpkt_opcua_HistoryDataBackend;',
              '/** Generated native history backend slots with C89 schema arguments. */',
              'struct cpkt_opcua_HistoryDataBackend {',
              public_backend, '};',
              '/** Per-node history settings. The backend record is copied on registration.',
              ' * Native gathering borrows backend contexts; userContext is borrowed.',
              ' * The server_register_history_backend convenience API instead transfers',
              ' * context on success and requires nonzero maxHistoryDataResponseSize.',
              ' * Native polling and settings lifetimes remain upstream-defined. */',
              translate(settings[0]).replace('cpkt_opcua_HISTORIZING', 'CPKT_OPCUA_HISTORIZING'),
              '/** Install upstream default gathering/database before server startup.',
              ' * initial capacity must be nonzero. Failure preserves the previous plugin.',
              ' * Replacement/destruction stops polling and clears all registered backends. */',
              'cpkt_opcua_StatusCode cpkt_opcua_server_set_default_history_database(cpkt_opcua_server *server, size_t initial_capacity);',
              '/** Register backend storage before startup in the installed default database.',
              ' * Success transfers backend context to deleteMembers; failure leaves it with',
              ' * the caller. Duplicate nodes are rejected. Does not alter node attributes.',
              ' * Register all nodes before starting any polling; later registration is',
              ' * rejected to keep native monitored-item contexts stable during store growth. */',
              'cpkt_opcua_StatusCode cpkt_opcua_server_register_history_backend(cpkt_opcua_server *server, const cpkt_opcua_NodeId *node, const cpkt_opcua_HistorizingNodeIdSettings *settings);',
              '/** Start native polling for a registered POLL node. */',
              'cpkt_opcua_StatusCode cpkt_opcua_server_history_start_poll(cpkt_opcua_server *server, const cpkt_opcua_NodeId *node);',
              '/** Stop native polling for a registered POLL node. */',
              'cpkt_opcua_StatusCode cpkt_opcua_server_history_stop_poll(cpkt_opcua_server *server, const cpkt_opcua_NodeId *node);']
    # Do not silently emit the original borrowed C89 DataValue return type.
    if 'const cpkt_opcua_DataValue*' in '\n'.join(header):
        raise ValueError('Adapt persistent history value callback return declaration')
    metadata, assignments = [], []
    for result, name, signature in methods:
        if name == 'deleteMembers':
            continue
        args = parameters(signature)
        declarations, conversion, cleanup, callargs, outputs, commits = [], [], [], [], [], []
        for spelling, param in args:
            bare = spelling.replace('const ', '').replace('*', '').strip()
            typename = bare[3:] if bare.startswith('UA_') else None
            if param == 'server':
                callargs.append('server ? bridge->owner : NULL')
            elif param == 'hdbContext':
                callargs.append('bridge->plugin.context')
            elif param == 'backend':
                callargs.append('&bridge->plugin')
            elif spelling in ('void *', 'size_t', 'size_t *'):
                if param == 'providedValues':
                    declarations.append('  size_t c_providedValues = 0;')
                    conversion.append('  if(!providedValues || (valueSize && !values)) status = UA_STATUSCODE_BADINVALIDARGUMENT;')
                    callargs.append('&c_providedValues')
                else:
                    callargs.append(param)
            elif bare == 'MatchStrategy':
                callargs.append('(cpkt_opcua_MatchStrategy)' + param)
            elif typename == 'NumericRange':
                declarations += ['  cpkt_opcua_NumericRange c_range;', '  size_t range_index;']
                conversion += ['  memset(&c_range, 0, sizeof(c_range));',
                    '  if(!status && range.dimensionsSize) {',
                    '    if(!range.dimensions || range.dimensionsSize > (size_t)-1 / sizeof(*c_range.dimensions)) status = UA_STATUSCODE_BADINVALIDARGUMENT;',
                    '    else {',
                    '      c_range.dimensions = (cpkt_opcua_NumericRangeDimension *)UA_calloc(range.dimensionsSize, sizeof(*c_range.dimensions));',
                    '      if(!c_range.dimensions) status = UA_STATUSCODE_BADOUTOFMEMORY;',
                    '      else { c_range.dimensionsSize = range.dimensionsSize;',
                    '        for(range_index = 0; range_index < range.dimensionsSize; ++range_index) {',
                    '          c_range.dimensions[range_index].min = range.dimensions[range_index].min;',
                    '          c_range.dimensions[range_index].max = range.dimensions[range_index].max;', '        }', '      }', '    }', '  }']
                cleanup.append('  UA_free(c_range.dimensions);')
                callargs.append('c_range')
            elif typename == 'NodeId' and spelling == 'const UA_NodeId *':
                declarations.append(f'  cpkt_opcua_NodeId c_{param};')
                conversion.append(f'  if({param}) cpkt_hb_borrow_node({param}, &c_{param});')
                callargs.append(f'{param} ? &c_{param} : NULL')
            elif param == 'values' and spelling == 'UA_DataValue *':
                declarations += ['  cpkt_opcua_DataValue *c_values = NULL;', '  UA_DataValue *staged_values = NULL;', '  size_t value_index;']
                conversion += [f'  if(!status) {{ c_values = (cpkt_opcua_DataValue *)cpkt_opcua_array_new(valueSize, &cpkt_types[{index["DataValue"]}]);',
                               '    if(!c_values) status = UA_STATUSCODE_BADOUTOFMEMORY; }']
                callargs.append('c_values')
                outputs += ['  if(!status && c_providedValues > valueSize) status = UA_STATUSCODE_BADINVALIDARGUMENT;',
                            f'  if(!status) status = cpkt_array(c_values, c_providedValues, (void **)&staged_values, &cpkt_types[{index["DataValue"]}], 1, 0);']
                commits += ['  if(!status) {', '    for(value_index = 0; value_index < c_providedValues; ++value_index) {',
                            '      UA_DataValue_clear(&values[value_index]); values[value_index] = staged_values[value_index];',
                            '      UA_DataValue_init(&staged_values[value_index]);', '    }', '    *providedValues = c_providedValues;', '  }']
                cleanup += [f'  cpkt_opcua_array_delete(c_values, valueSize, &cpkt_types[{index["DataValue"]}]);',
                            f'  if(staged_values) UA_Array_delete(staged_values, c_providedValues, cpkt_types[{index["DataValue"]}].native);']
            elif typename in index and spelling.count('*') <= 1:
                mutable = '*' in spelling and not spelling.startswith('const ')
                declarations.append(f'  cpkt_opcua_{typename} c_{param};')
                conversion.append(f'  memset(&c_{param}, 0, sizeof(c_{param}));')
                if not mutable:
                    conversion.append(f'  if(!status{f" && {param}" if "*" in spelling else ""}) status = cpkt_convert({param if "*" in spelling else "&" + param}, &c_{param}, &cpkt_types[{index[typename]}], 0, 0);')
                else:
                    declarations.append(f'  UA_{typename} staged_{param};')
                    conversion += [f'  UA_{typename}_init(&staged_{param});', f'  if(!{param}) status = UA_STATUSCODE_BADINVALIDARGUMENT;']
                    outputs.append(f'  if(!status) status = cpkt_convert(&c_{param}, &staged_{param}, &cpkt_types[{index[typename]}], 1, 0);')
                    commits.append(f'  if(!status) {{ UA_{typename}_clear({param}); *{param} = staged_{param}; UA_{typename}_init(&staged_{param}); }}')
                    cleanup.append(f'  UA_{typename}_clear(&staged_{param});')
                cleanup.append(f'  cpkt_opcua_type_clear(&c_{param}, &cpkt_types[{index[typename]}]);')
                callargs.append(f'{param} ? &c_{param} : NULL' if '*' in spelling else 'c_' + param)
            else:
                raise ValueError(f'Adapt history backend argument {name}.{param}: {spelling}')
        if result not in ('UA_StatusCode', 'size_t', 'UA_Boolean', 'const UA_DataValue*'):
            raise ValueError(f'Adapt history backend return: {result}')
        pointer_result = name == 'getDataValue'
        if pointer_result:
            declarations.append('  const cpkt_opcua_history_value *stored = NULL;')
            invoke = f'  if(!status) stored = bridge->plugin.{name}({", ".join(callargs)});'
            ret = '  return cpkt_history_native_const(stored);'
        else:
            declarations.append(f'  {result} answer = 0;')
            invoke = f'  if(!status) answer = bridge->plugin.{name}({", ".join(callargs)});'
            if result == 'UA_StatusCode':
                invoke += '\n  if(!status) status = answer;'
                ret = '  return status;'
            else:
                ret = '  return status ? 0 : answer;'
        context = 'backend->context' if name == 'getHistoryData' else 'hdbContext'
        metadata += [f'static {result} cpkt_hb_{name}({signature}) {{',
                     f'  cpkt_hb_bridge *bridge = (cpkt_hb_bridge *){context};',
                     '  UA_StatusCode status = 0;', *declarations, '  (void)server;', *conversion,
                     invoke, *outputs, *commits,
                     *([f'  if(status && server) UA_LOG_ERROR(UA_Server_getConfig(server)->logging, UA_LOGCATEGORY_SERVER, "C89 history backend {name} conversion failed: %08lx", (unsigned long)status);'] if result != 'UA_StatusCode' else []),
                     *cleanup, ret, '}']
        assignments.append(f'  native->{name} = plugin->{name} ? cpkt_hb_{name} : NULL;')
    metadata += ['static void cpkt_hb_assign(UA_HistoryDataBackend *native, const cpkt_opcua_HistoryDataBackend *plugin) {',
                 '  memset(native, 0, sizeof(*native));', *assignments, '}']
    stock_header, stock_metadata = emit_stock_backend(index, native_headers, methods)
    gathering_header, gathering_metadata = emit_gathering(index, native_headers, configuration)
    return header + stock_header + gathering_header, metadata + stock_metadata + gathering_metadata


def emit_stock_backend(index, native_headers, methods):
    """Generate both stock factories and every callable native callback slot."""
    path = native_headers / 'plugin/historydata/history_data_backend_memory.h'
    text = path.read_text()
    found = re.findall(r'(UA_HistoryDataBackend|void)\s+UA_EXPORT\s+(UA_HistoryDataBackend_Memory\w*)\s*\((.*?)\)\s*;', uncomment(text), re.S)
    expected = {
        'UA_HistoryDataBackend_Memory': ('UA_HistoryDataBackend', [('size_t', 'initialNodeIdStoreSize'), ('size_t', 'initialDataStoreSize')]),
        'UA_HistoryDataBackend_Memory_Circular': ('UA_HistoryDataBackend', [('size_t', 'initialNodeIdStoreSize'), ('size_t', 'initialDataStoreSize')]),
        'UA_HistoryDataBackend_Memory_clear': ('void', [('UA_HistoryDataBackend *', 'backend')]),
    }
    if {name for _, name, _ in found} != set(expected):
        raise ValueError('Adapt stock history backend factories')
    for result, name, signature in found:
        if (result, parameters(signature)) != expected[name]:
            raise ValueError('Adapt stock history factory signature: ' + name)
    header = [re.match(r'/\*.*?\*/', text, re.S)[0],
        '/** Native growable memory backend; zero capacities retain native defaults.',
        ' * Empty record means constructor/conversion allocation failure. The record',
        ' * owns its context; copying it creates aliases, not additional ownership.',
        ' * All native callback slots remain callable with C89 values. Input node',
        ' * identifiers and continuation bytes borrow until return. Output records',
        ' * and arrays must start empty and own allocations, including partial error',
        ' * outputs. Clear those outputs even on failure. providedValues is optional.',
        ' * Borrowed getDataValue handles denote the actual native value, without',
        ' * per-value allocation or caching. Do not set/free them or cast away const.',
        ' * Validity follows native replacement/removal/destruction, not facade copies.',
        ' * Serialize operations and finish borrowers before mutating their storage.',
        ' * getHistoryData is NULL on the growable native backend. */',
        'cpkt_opcua_HistoryDataBackend cpkt_opcua_HistoryDataBackend_Memory(size_t initialNodeIdStoreSize, size_t initialDataStoreSize);',
        '/** Native circular memory backend. Retains native capacities, overwrite',
        ' * order and timestamp/read behavior; both high/low-level callback slots',
        ' * follow their native availability. Same ownership as Memory. */',
        'cpkt_opcua_HistoryDataBackend cpkt_opcua_HistoryDataBackend_Memory_Circular(size_t initialNodeIdStoreSize, size_t initialDataStoreSize);',
        '/** Invoke native Memory_clear, release the facade context and reset the',
        ' * record. Only stock Memory/Memory_Circular records may be passed. NULL',
        ' * and empty records are safe. All aliases and borrowed values expire.',
        ' * deleteMembers also releases context, but leaves the record invalid as',
        ' * the native callback does; call exactly one destructor per context. */',
        'void cpkt_opcua_HistoryDataBackend_Memory_clear(cpkt_opcua_HistoryDataBackend *backend);']
    metadata = ['static int cpkt_stock_hb_record(const cpkt_opcua_HistoryDataBackend *, cpkt_opcua_server *, void *, cpkt_stock_hb_dispatch *, UA_HistoryDataBackend *);']
    assignments, comparisons = [], []
    for result, name, signature in methods:
        if name == 'deleteMembers':
            continue
        args = parameters(signature)
        c_signature = translate(signature).replace('MatchStrategy', 'cpkt_opcua_MatchStrategy')
        c_result = 'const cpkt_opcua_history_value *' if name == 'getDataValue' else translate(result)
        context = 'backend ? backend->context : NULL' if name == 'getHistoryData' else 'hdbContext'
        declarations, setup, conversion, outputs, cleanup, callargs = [], [], [], [], [], []
        for spelling, param in args:
            bare = spelling.replace('const ', '').replace('*', '').strip()
            typename = bare[3:] if bare.startswith('UA_') else None
            if param == 'server':
                callargs.append('server ? server->server : NULL')
            elif param == 'hdbContext':
                callargs.append('bridge->native.context')
            elif param == 'backend':
                declarations += ['  cpkt_stock_hb_dispatch dispatch;', '  int dispatching = 0;', '  UA_HistoryDataBackend n_backend;']
                conversion += ['  if(!status) dispatching = cpkt_stock_hb_record(backend, server, sessionContext, &dispatch, &n_backend);']
                callargs.append('&n_backend')
            elif param == 'providedValues':
                declarations += ['  size_t n_provided = 0;']
                callargs.append('&n_provided')
            elif param == 'values':
                declarations += ['  UA_DataValue *n_values = NULL;', '  size_t value_index, c_provided = 0;', '  UA_StatusCode output_status = 0;']
                conversion += ['  if(!status && valueSize && !values) status = UA_STATUSCODE_BADINVALIDARGUMENT;',
                    f'  if(!status) {{ n_values = (UA_DataValue *)UA_Array_new(valueSize, cpkt_types[{index["DataValue"]}].native);',
                    '    if(!n_values) status = UA_STATUSCODE_BADOUTOFMEMORY; }']
                outputs += ['  if(invoked) {', '    if(n_provided > valueSize) output_status = UA_STATUSCODE_BADINTERNALERROR;',
                    '    for(value_index = 0; !output_status && value_index < n_provided; ++value_index) {',
                    f'      memset(&values[value_index], 0, sizeof(values[value_index])); output_status = cpkt_convert(&n_values[value_index], &values[value_index], &cpkt_types[{index["DataValue"]}], 0, 0);',
                    '      c_provided = value_index + 1;', '    }',
                    '    if(output_status) {', f'      for(value_index = 0; value_index < c_provided; ++value_index) cpkt_opcua_type_clear(&values[value_index], &cpkt_types[{index["DataValue"]}]);',
                    '      n_provided = 0; if(!status) status = output_status;', '    }',
                    '    if(providedValues) *providedValues = n_provided;', '  }']
                cleanup += [f'  if(n_values) UA_Array_delete(n_values, valueSize, cpkt_types[{index["DataValue"]}].native);']
                callargs.append('n_values')
            elif spelling in ('void *', 'size_t', 'size_t *'):
                callargs.append('(dispatching ? (void *)&dispatch : sessionContext)' if name == 'getHistoryData' and param == 'sessionContext' else param)
            elif bare == 'MatchStrategy':
                callargs.append('(MatchStrategy)' + param)
            elif typename == 'NumericRange':
                declarations += ['  UA_NumericRange n_range;', '  size_t range_index;']
                setup += ['  memset(&n_range, 0, sizeof(n_range));']
                conversion += ['  if(!status && range.dimensionsSize) {',
                    '    if(!range.dimensions || range.dimensions == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL) status = UA_STATUSCODE_BADINVALIDARGUMENT;',
                    '    else if(range.dimensionsSize > (size_t)-1 / sizeof(*n_range.dimensions)) status = UA_STATUSCODE_BADOUTOFMEMORY;',
                    '    else { n_range.dimensions = (UA_NumericRangeDimension *)UA_calloc(range.dimensionsSize, sizeof(*n_range.dimensions));',
                    '      if(!n_range.dimensions) status = UA_STATUSCODE_BADOUTOFMEMORY;',
                    '      else { n_range.dimensionsSize = range.dimensionsSize;',
                    '        for(range_index = 0; range_index < range.dimensionsSize; ++range_index) {',
                    '          n_range.dimensions[range_index].min = range.dimensions[range_index].min;',
                    '          n_range.dimensions[range_index].max = range.dimensions[range_index].max;', '        }', '      }', '    }', '  }']
                cleanup += ['  UA_free(n_range.dimensions);']
                callargs.append('n_range')
            elif typename == 'NodeId' and spelling == 'const UA_NodeId *':
                declarations += [f'  UA_NodeId n_{param};']
                conversion += [f'  if(!cpkt_stock_node_valid({param}){f" || !{param}" if param == "nodeId" else ""}) status = UA_STATUSCODE_BADINVALIDARGUMENT;',
                               f'  n_{param} = cpkt_nodeid_view({param});']
                callargs.append(f'{param} ? &n_{param} : NULL')
            elif typename in ('String', 'ByteString') and spelling.startswith('const '):
                declarations += [f'  UA_{typename} n_{param};']
                conversion += [f'  if(!{param} || ({param}->length && (!{param}->data || {param}->data == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL))) status = UA_STATUSCODE_BADINVALIDARGUMENT;',
                    f'  if({param}) n_{param} = cpkt_string_view({param});']
                callargs.append('&n_' + param)
            elif typename in ('String', 'ByteString') and spelling.count('*') == 1:
                declarations += [f'  UA_{typename} n_{param};']
                setup += [f'  UA_{typename}_init(&n_{param});']
                conversion += [f'  if(!{param}) status = UA_STATUSCODE_BADINVALIDARGUMENT;']
                outputs += [f'  if(invoked) {{ cpkt_string_take({param}, n_{param}); UA_{typename}_init(&n_{param}); }}']
                cleanup += [f'  UA_{typename}_clear(&n_{param});']
                callargs.append('&n_' + param)
            elif typename in index and spelling.count('*') <= 1:
                mutable = '*' in spelling and not spelling.startswith('const ')
                declarations += [f'  UA_{typename} n_{param};']
                setup += [f'  UA_{typename}_init(&n_{param});']
                if mutable:
                    declarations += [f'  cpkt_opcua_{typename} c_{param};', f'  UA_StatusCode status_{param} = 0;']
                    setup += [f'  memset(&c_{param}, 0, sizeof(c_{param}));']
                    conversion += [f'  if(!{param}) status = UA_STATUSCODE_BADINVALIDARGUMENT;']
                    outputs += [f'  if(invoked) {{ status_{param} = cpkt_convert(&n_{param}, &c_{param}, &cpkt_types[{index[typename]}], 0, 0);',
                        f'    if(!status_{param}) {{ *{param} = c_{param}; memset(&c_{param}, 0, sizeof(c_{param})); }}',
                        f'    else if(!status) status = status_{param}; }}']
                    cleanup += [f'  cpkt_opcua_type_clear(&c_{param}, &cpkt_types[{index[typename]}]);']
                else:
                    if '*' in spelling:
                        conversion += [f'  if(!{param}) status = UA_STATUSCODE_BADINVALIDARGUMENT;']
                    pointer = param if '*' in spelling else '&' + param
                    conversion += [f'  if(!status) status = cpkt_convert({pointer}, &n_{param}, &cpkt_types[{index[typename]}], 1, 0);']
                cleanup += [f'  UA_{typename}_clear(&n_{param});']
                callargs.append('&n_' + param if '*' in spelling else 'n_' + param)
            else:
                raise ValueError(f'Adapt stock backend argument {name}.{param}: {spelling}')
        if result not in ('UA_StatusCode', 'size_t', 'UA_Boolean', 'const UA_DataValue*'):
            raise ValueError('Adapt stock backend result: ' + name)
        invoke = f'bridge->native.{name}({", ".join(callargs)})'
        ret = '  return status;' if result == 'UA_StatusCode' else (
            '  return status ? NULL : (const cpkt_opcua_history_value *)(const void *)answer;' if name == 'getDataValue' else '  return status ? 0 : answer;')
        metadata += [f'static {c_result} cpkt_stock_hb_{name}({c_signature}) {{',
            f'  cpkt_stock_hb *bridge = (cpkt_stock_hb *)({context});',
            '  UA_StatusCode status = 0;', f'  {result} answer = 0;',
            *(['  int invoked = 0;'] if outputs else []), *declarations, *setup,
            f'  if(!bridge || !bridge->native.{name} || (server && !server->server)) status = UA_STATUSCODE_BADINVALIDARGUMENT;',
            *conversion, f'  if(!status) {{ answer = {invoke};' + (' invoked = 1;' if outputs else '') + ' }',
            *(['  if(!status) status = answer;'] if result == 'UA_StatusCode' else []),
            *outputs, *cleanup,
            *([f'  if(status && server && server->server) UA_LOG_ERROR(UA_Server_getConfig(server->server)->logging, UA_LOGCATEGORY_SERVER, "C89 stock history {name} conversion failed: %08lx", (unsigned long)status);'] if result != 'UA_StatusCode' else []),
            ret, '}']
        assignments += [f'  out.{name} = native.{name} ? cpkt_stock_hb_{name} : NULL;']
        comparisons += [f'backend->{name} == (bridge->native.{name} ? cpkt_stock_hb_{name} : NULL)']
    dispatch_assignments = []
    for result, name, signature in methods:
        if name in ('deleteMembers', 'getHistoryData'):
            continue
        callargs = []
        for _, param in parameters(signature):
            callargs.append('&dispatch->forwarded' if param == 'hdbContext' else
                            'dispatch->session_context' if param == 'sessionContext' else param)
        metadata += [f'static {result} cpkt_stock_dispatch_{name}({signature}) {{',
                     '  cpkt_stock_hb_dispatch *dispatch = (cpkt_stock_hb_dispatch *)sessionContext;',
                     '  (void)hdbContext;',
                     f'  return cpkt_hb_{name}({", ".join(callargs)});', '}']
        dispatch_assignments += [f'  native->{name} = backend->{name} ? cpkt_stock_dispatch_{name} : NULL;']
    metadata += [
        'static void cpkt_stock_hb_delete(cpkt_opcua_HistoryDataBackend *backend) {',
        '  cpkt_stock_hb *bridge = (cpkt_stock_hb *)backend->context;',
        '  bridge->native.deleteMembers(&bridge->native); UA_free(bridge);', '}',
        'static int cpkt_stock_hb_record(const cpkt_opcua_HistoryDataBackend *backend, cpkt_opcua_server *server, void *session_context, cpkt_stock_hb_dispatch *dispatch, UA_HistoryDataBackend *native) {',
        '  cpkt_stock_hb *bridge = (cpkt_stock_hb *)backend->context;',
        '  if(backend->deleteMembers == cpkt_stock_hb_delete && ' + ' && '.join(comparisons) + ') { *native = bridge->native; return 0; }',
        '  memset(dispatch, 0, sizeof(*dispatch)); dispatch->forwarded.owner = server; dispatch->forwarded.plugin = *backend;',
        '  dispatch->session_context = session_context; *native = bridge->native;',
        *dispatch_assignments, '  return 1;', '}',
        'static cpkt_opcua_HistoryDataBackend cpkt_stock_hb_new(size_t nodes, size_t values, int circular) {',
        '  cpkt_opcua_HistoryDataBackend out;', '  cpkt_stock_hb *bridge;',
        '  UA_HistoryDataBackend native = circular ? UA_HistoryDataBackend_Memory_Circular(nodes, values) : UA_HistoryDataBackend_Memory(nodes, values);',
        '  memset(&out, 0, sizeof(out)); if(!native.context) return out;',
        '  bridge = (cpkt_stock_hb *)UA_calloc(1, sizeof(*bridge));',
        '  if(!bridge) { UA_HistoryDataBackend_Memory_clear(&native); return out; }',
        '  bridge->native = native; out.context = bridge; out.deleteMembers = cpkt_stock_hb_delete;',
        *assignments, '  return out;', '}',
        'cpkt_opcua_HistoryDataBackend cpkt_opcua_HistoryDataBackend_Memory(size_t nodes, size_t values) { return cpkt_stock_hb_new(nodes, values, 0); }',
        'cpkt_opcua_HistoryDataBackend cpkt_opcua_HistoryDataBackend_Memory_Circular(size_t nodes, size_t values) { return cpkt_stock_hb_new(nodes, values, 1); }',
        'void cpkt_opcua_HistoryDataBackend_Memory_clear(cpkt_opcua_HistoryDataBackend *backend) {',
        '  cpkt_stock_hb *bridge;', '  if(!backend) return;',
        '  if(backend->context) { bridge = (cpkt_stock_hb *)backend->context; UA_HistoryDataBackend_Memory_clear(&bridge->native); UA_free(bridge); }',
        '  memset(backend, 0, sizeof(*backend));', '}']
    return header, metadata


def emit_gathering(index, native_headers, configuration):
    path = native_headers / 'plugin/historydata/history_data_gathering.h'
    body = public_body(path, 'HistoryDataGathering', configuration)
    validate_fields(body, ['void *context'])
    methods = callbacks(body)
    expected = {'deleteMembers', 'registerNodeId', 'stopPoll', 'startPoll',
                'updateNodeIdSetting', 'getHistorizingSetting', 'setValue'}
    if {name for _, name, _ in methods} != expected:
        raise ValueError('Adapt history gathering callbacks')
    returns = {'deleteMembers': 'void', 'registerNodeId': 'UA_StatusCode',
               'stopPoll': 'UA_StatusCode', 'startPoll': 'UA_StatusCode',
               'updateNodeIdSetting': 'UA_Boolean',
               'getHistorizingSetting': 'const UA_HistorizingNodeIdSettings*',
               'setValue': 'void'}
    for result, name, signature in methods:
        args = parameters(signature)
        if result != returns[name] or (name == 'deleteMembers' and args != [('UA_HistoryDataGathering *', 'gathering')]) or (
                name != 'deleteMembers' and args[:2] != [('UA_Server *', 'server'), ('void *', 'hdgContext')]):
            raise ValueError('Adapt history gathering signature: ' + name)
    public = translate(body).replace('const cpkt_opcua_HistorizingNodeIdSettings*',
                                    'const cpkt_opcua_history_settings*')
    header = [re.match(r'/\*.*?\*/', path.read_text(), re.S)[0],
        '/** Actual native public settings storage. Owned new/set records preserve',
        ' * their address; gathering getters return const native borrows. No lookup',
        ' * snapshot/cache is maintained. Backend contexts and userContext are borrowed.',
        ' * Finish native borrowers before set/free; never free a stock const borrow. */',
        'typedef struct cpkt_opcua_history_settings cpkt_opcua_history_settings;',
        '/** Create owned native settings and callback conversion metadata. Copies',
        ' * scalar fields and callback slots, borrowing backend context/userContext.',
        ' * Failure leaves *out NULL. No backend ownership transfers. */',
        'cpkt_opcua_StatusCode cpkt_opcua_history_settings_new(const cpkt_opcua_HistorizingNodeIdSettings *setting, cpkt_opcua_history_settings **out);',
        '/** Replace quiescent owned settings. Failure preserves the previous record.',
        ' * Native settings/backend copies must finish borrowing before replacement. */',
        'cpkt_opcua_StatusCode cpkt_opcua_history_settings_set(cpkt_opcua_history_settings *stored, const cpkt_opcua_HistorizingNodeIdSettings *setting);',
        '/** Read owned or stock-borrowed native settings into a C89 field copy.',
        ' * No allocation; backend callbacks/context and userContext remain aliases.',
        ' * The copy has no additional ownership. Failure leaves out empty. */',
        'cpkt_opcua_StatusCode cpkt_opcua_history_settings_get(const cpkt_opcua_history_settings *stored, cpkt_opcua_HistorizingNodeIdSettings *out);',
        '/** Free quiescent owned settings and conversion metadata only. Backend',
        ' * contexts/userContext remain caller-owned; NULL is safe. */',
        'void cpkt_opcua_history_settings_free(cpkt_opcua_history_settings *stored);',
        '/** Full native gathering record. Inputs borrow through the callback; native',
        ' * registration copies settings but borrows their backend context. A custom',
        ' * getHistorizingSetting returns persistent owned settings created above.',
        ' * Return distinct objects for distinct retained records, not a temporary.',
        ' * Native callbacks may continue borrowing after getter return. deleteMembers',
        ' * releases the gathering context and owned settings, after polling is stopped. */',
        'typedef struct cpkt_opcua_HistoryDataGathering cpkt_opcua_HistoryDataGathering;',
        '/** Generated native gathering callbacks, including persistent settings. */',
        'struct cpkt_opcua_HistoryDataGathering {', public, '};']
    metadata = []
    for result, name, signature in methods:
        if name == 'deleteMembers':
            continue
        declarations, setup, conversion, cleanup, callargs = [], [], [], [], []
        for spelling, param in parameters(signature):
            bare = spelling.replace('const ', '').replace('*', '').strip()
            typename = bare[3:] if bare.startswith('UA_') else None
            if param == 'server':
                callargs.append('bridge->owner')
            elif param == 'hdgContext':
                callargs.append('bridge->plugin.context')
            elif spelling == 'void *':
                callargs.append(param)
            elif typename == 'HistorizingNodeIdSettings' and spelling == 'const UA_HistorizingNodeIdSettings':
                declarations += ['  cpkt_opcua_HistorizingNodeIdSettings c_setting;']
                conversion += ['  if(!status) status = cpkt_history_settings_load(&setting, &c_setting);']
                callargs.append('c_setting')
            elif typename == 'NodeId' and spelling == 'const UA_NodeId *':
                declarations += [f'  cpkt_opcua_NodeId c_{param};']
                conversion += [f'  if({param}) cpkt_hb_borrow_node({param}, &c_{param});']
                callargs.append(f'{param} ? &c_{param} : NULL')
            elif typename in index and spelling.count('*') <= 1:
                declarations += [f'  cpkt_opcua_{typename} c_{param};']
                setup += [f'  memset(&c_{param}, 0, sizeof(c_{param}));']
                pointer = param if '*' in spelling else '&' + param
                conversion += [f'  if(!status{f" && {param}" if "*" in spelling else ""}) status = cpkt_convert({pointer}, &c_{param}, &cpkt_types[{index[typename]}], 0, 0);']
                cleanup += [f'  cpkt_opcua_type_clear(&c_{param}, &cpkt_types[{index[typename]}]);']
                callargs.append(f'{param} ? &c_{param} : NULL' if '*' in spelling else 'c_' + param)
            else:
                raise ValueError(f'Adapt gathering argument {name}.{param}: {spelling}')
        if result not in ('void', 'UA_StatusCode', 'UA_Boolean', 'const UA_HistorizingNodeIdSettings*'):
            raise ValueError('Adapt gathering result: ' + name)
        if name == 'getHistorizingSetting':
            declarations += ['  const cpkt_opcua_history_settings *answer = NULL;']
            invoke = f'  if(!status) answer = bridge->plugin.{name}({", ".join(callargs)});'
            ret = '  return status ? NULL : cpkt_history_settings_borrow(answer, bridge->owner);'
        elif result == 'void':
            invoke = f'  if(!status) bridge->plugin.{name}({", ".join(callargs)});'
            ret = ''
        else:
            declarations += [f'  {result} answer = 0;']
            invoke = f'  if(!status) answer = bridge->plugin.{name}({", ".join(callargs)});'
            ret = '  return status ? status : answer;' if result == 'UA_StatusCode' else '  return status ? 0 : answer;'
        metadata += [f'static {result} cpkt_hg_{name}({signature}) {{',
                     '  cpkt_hg_bridge *bridge = (cpkt_hg_bridge *)hdgContext;',
                     '  UA_StatusCode status = 0;', *declarations, '  (void)server;', *setup, *conversion,
                     invoke, *cleanup,
                     *([f'  if(status && server) UA_LOG_ERROR(UA_Server_getConfig(server)->logging, UA_LOGCATEGORY_SERVER, "C89 gathering {name} conversion failed: %08lx", (unsigned long)status);'] if result != 'UA_StatusCode' else []),
                     *([ret] if ret else []), '}']
    metadata += ['static void cpkt_hg_delete(UA_HistoryDataGathering *native) {',
                 '  cpkt_hg_bridge *bridge = (cpkt_hg_bridge *)native->context;',
                 '  if(bridge->plugin.deleteMembers) bridge->plugin.deleteMembers(&bridge->plugin);',
                 '  UA_free(bridge);', '}',
                 'static void cpkt_hg_assign(UA_HistoryDataGathering *native, const cpkt_opcua_HistoryDataGathering *plugin) {',
                 '  memset(native, 0, sizeof(*native));',
                 '  native->deleteMembers = cpkt_hg_delete;',
                 *[f'  native->{name} = plugin->{name} ? cpkt_hg_{name} : NULL;' for _, name, _ in methods if name != 'deleteMembers'], '}']
    stock_header, stock_metadata = emit_stock_gathering(index, native_headers, methods)
    database_header, database_metadata = emit_stock_database(index, native_headers, configuration)
    return header + stock_header + database_header, metadata + stock_metadata + database_metadata


def emit_stock_gathering(index, native_headers, methods):
    path = native_headers / 'plugin/historydata/history_data_gathering_default.h'
    text = path.read_text()
    names = re.findall(r'\b(\w+)\s+(?:UA_EXPORT\s+)?(UA_HistoryDataGathering_\w+|gathering_default_\w+)\s*\((.*?)\)\s*;', uncomment(text), re.S)
    expected = {
        'UA_HistoryDataGathering_Default': [('size_t', 'initialNodeIdStoreSize')],
        'UA_HistoryDataGathering_Circular': [('size_t', 'initialNodeIdStoreSize')],
        'gathering_default_pauseRecording': [('UA_HistoryDataGathering *', 'gathering'), ('const UA_NodeId *', 'nodeId'), ('UA_Boolean', 'pause')],
    }
    if {name for _, name, _ in names} != set(expected):
        raise ValueError('Adapt stock gathering factories')
    for result, name, signature in names:
        native_result = 'void' if name == 'gathering_default_pauseRecording' else 'UA_HistoryDataGathering'
        if result != native_result or parameters(signature) != expected[name]:
            raise ValueError('Adapt stock gathering signature: ' + name)
    header = [re.match(r'/\*.*?\*/', text, re.S)[0],
        '/** Native default gathering, with callable C89 callback slots. Owns its',
        ' * gathering context and conversion metadata; all backend contexts remain',
        ' * caller-owned. Zero capacity retains native grow-on-registration behavior.',
        ' * Stop all polling before destruction. Registration/growth may invalidate',
        ' * native borrowed settings and polling contexts; finish borrows first and',
        ' * register all nodes before starting polling when growth can occur.',
        ' * Start polling with the facade server containing that node; registration',
        ' * may use NULL when native permits it. NULL lookup leaves polling intact.',
        ' * Copies alias one context. Call exactly one deleteMembers per context.',
        ' * It releases context without resetting the record, as native does.',
        ' * Empty record indicates allocation failure. Output settings are actual',
        ' * const native borrows; never free/set them or cast away const. */',
        'cpkt_opcua_HistoryDataGathering cpkt_opcua_HistoryDataGathering_Default(size_t initialNodeIdStoreSize);',
        '/** Native fixed-capacity gathering. The capacity is a node limit, with',
        ' * native failure when exceeded; no facade growth policy is added.',
        ' * Same context/backend ownership and callback rules as Default. */',
        'cpkt_opcua_HistoryDataGathering cpkt_opcua_HistoryDataGathering_Circular(size_t initialNodeIdStoreSize);',
        '/** Pause/resume native value-set recording for an existing node. Stock',
        ' * Default/Circular records only. Retains native polling behavior; this',
        ' * does not stop polling or discard existing values. */',
        'void cpkt_opcua_gathering_default_pauseRecording(cpkt_opcua_HistoryDataGathering *gathering, const cpkt_opcua_NodeId *nodeId, cpkt_opcua_Boolean pause);']
    metadata = []
    for result, name, signature in methods:
        if name == 'deleteMembers':
            continue
        c_result = 'const cpkt_opcua_history_settings *' if name == 'getHistorizingSetting' else translate(result)
        c_signature = translate(signature)
        declarations, setup, conversion, cleanup, callargs = [], [], [], [], []
        for spelling, param in parameters(signature):
            bare = spelling.replace('const ', '').replace('*', '').strip()
            typename = bare[3:] if bare.startswith('UA_') else None
            if param == 'server':
                callargs.append('server ? server->server : NULL')
            elif param == 'hdgContext':
                callargs.append('bridge->native.context')
            elif spelling == 'void *':
                callargs.append(param)
            elif typename == 'HistorizingNodeIdSettings':
                continue
            elif typename == 'NodeId' and spelling == 'const UA_NodeId *':
                if name in ('registerNodeId', 'updateNodeIdSetting'):
                    continue
                declarations += [f'  UA_NodeId n_{param};']
                conversion += [f'  if(!cpkt_stock_node_valid({param}){f" || !{param}" if param == "nodeId" else ""}) status = UA_STATUSCODE_BADINVALIDARGUMENT;',
                               f'  n_{param} = cpkt_nodeid_view({param});']
                callargs.append(f'{param} ? &n_{param} : NULL')
            elif typename in index and spelling.count('*') <= 1:
                declarations += [f'  UA_{typename} n_{param};']
                setup += [f'  UA_{typename}_init(&n_{param});']
                pointer = param if '*' in spelling else '&' + param
                conversion += [f'  if(!status{f" && {param}" if "*" in spelling else ""}) status = cpkt_convert({pointer}, &n_{param}, &cpkt_types[{index[typename]}], 1, 0);']
                cleanup += [f'  UA_{typename}_clear(&n_{param});']
                callargs.append('&n_' + param if '*' in spelling else 'n_' + param)
            else:
                raise ValueError(f'Adapt stock gathering argument {name}.{param}: {spelling}')
        if name in ('registerNodeId', 'updateNodeIdSetting'):
            declarations += ['  UA_Boolean changed = false;']
            invoke = f'  if(!status) status = cpkt_stock_hg_setting(bridge, server, nodeId, &setting, {int(name == "updateNodeIdSetting")}, &changed);'
            ret = '  return status;' if result == 'UA_StatusCode' else '  return status ? 0 : changed;'
        elif name == 'getHistorizingSetting':
            declarations += ['  const UA_HistorizingNodeIdSettings *answer = NULL;']
            invoke = f'  if(!status) answer = bridge->native.{name}({", ".join(callargs)});'
            invoke += '\n  (void)cpkt_history_settings_borrow((const cpkt_opcua_history_settings *)(const void *)answer, server);'
            ret = '  return status ? NULL : (const cpkt_opcua_history_settings *)(const void *)answer;'
        elif result == 'void':
            declarations += ['  const UA_HistorizingNodeIdSettings *stored;']
            invoke = '  if(!status) { stored = bridge->native.getHistorizingSetting(server ? server->server : NULL, bridge->native.context, &n_nodeId);\n'
            invoke += '    (void)cpkt_history_settings_borrow((const cpkt_opcua_history_settings *)(const void *)stored, server);\n'
            invoke += f'    bridge->native.{name}({", ".join(callargs)}); }}'
            ret = ''
        elif name == 'startPoll':
            declarations += ['  const UA_HistorizingNodeIdSettings *stored;',
                             '  cpkt_hb_bridge *binding = NULL;',
                             '  cpkt_opcua_server *previous_owner = NULL;']
            invoke = '  if(!status) { stored = bridge->native.getHistorizingSetting(server ? server->server : NULL, bridge->native.context, &n_nodeId);\n'
            invoke += '    if(stored && stored->historizingBackend.deleteMembers == cpkt_history_settings_backend_delete) {\n'
            invoke += '      binding = (cpkt_hb_bridge *)stored->historizingBackend.context; previous_owner = binding->owner;\n'
            invoke += '      binding->owner = server; }\n'
            invoke += f'    status = bridge->native.{name}({", ".join(callargs)});\n'
            invoke += '    if(status && binding) binding->owner = previous_owner; }'
            ret = '  return status;'
        elif result == 'UA_StatusCode':
            invoke = f'  if(!status) status = bridge->native.{name}({", ".join(callargs)});'
            ret = '  return status;'
        else:
            raise ValueError('Adapt stock gathering method: ' + name)
        metadata += [f'static {c_result} cpkt_stock_hg_{name}({c_signature}) {{',
                     '  cpkt_stock_hg *bridge = (cpkt_stock_hg *)hdgContext;',
                     '  UA_StatusCode status = 0;', *declarations, *setup,
                     f'  if(!bridge || !bridge->native.{name} || (server && !server->server)) status = UA_STATUSCODE_BADINVALIDARGUMENT;',
                     *conversion, invoke, *cleanup,
                     *([f'  if(status && server && server->server) UA_LOG_ERROR(UA_Server_getConfig(server->server)->logging, UA_LOGCATEGORY_SERVER, "C89 stock gathering {name} conversion failed: %08lx", (unsigned long)status);'] if result != 'UA_StatusCode' else []),
                     *([ret] if ret else []), '}']
    metadata += ['static void cpkt_stock_hg_delete(cpkt_opcua_HistoryDataGathering *gathering) {',
                 '  cpkt_stock_hg *bridge = (cpkt_stock_hg *)gathering->context;',
                 '  cpkt_hb_bridge *binding, *next;',
                 '  bridge->native.deleteMembers(&bridge->native);',
                 '  for(binding = bridge->bindings; binding; binding = next) { next = binding->next; UA_free(binding); }',
                 '  UA_free(bridge);', '}',
                 'static cpkt_opcua_HistoryDataGathering cpkt_stock_hg_new(size_t capacity, int circular) {',
                 '  cpkt_opcua_HistoryDataGathering out;', '  cpkt_stock_hg *bridge;',
                 '  UA_HistoryDataGathering native = circular ? UA_HistoryDataGathering_Circular(capacity) : UA_HistoryDataGathering_Default(capacity);',
                 '  memset(&out, 0, sizeof(out)); if(!native.context) return out;',
                 '  bridge = (cpkt_stock_hg *)UA_calloc(1, sizeof(*bridge));',
                 '  if(!bridge) { native.deleteMembers(&native); return out; }',
                 '  bridge->native = native; out.context = bridge; out.deleteMembers = cpkt_stock_hg_delete;',
                 *[f'  out.{name} = native.{name} ? cpkt_stock_hg_{name} : NULL;' for _, name, _ in methods if name != 'deleteMembers'],
                 '  return out;', '}',
                 'cpkt_opcua_HistoryDataGathering cpkt_opcua_HistoryDataGathering_Default(size_t capacity) { return cpkt_stock_hg_new(capacity, 0); }',
                 'cpkt_opcua_HistoryDataGathering cpkt_opcua_HistoryDataGathering_Circular(size_t capacity) { return cpkt_stock_hg_new(capacity, 1); }',
                 'void cpkt_opcua_gathering_default_pauseRecording(cpkt_opcua_HistoryDataGathering *gathering, const cpkt_opcua_NodeId *nodeId, cpkt_opcua_Boolean pause) {',
                 '  cpkt_stock_hg *bridge;', '  UA_NodeId node;',
                 '  if(!gathering || !gathering->context || !nodeId || !cpkt_stock_node_valid(nodeId)) return;',
                 '  bridge = (cpkt_stock_hg *)gathering->context; node = cpkt_nodeid_view(nodeId);',
                 '  gathering_default_pauseRecording(&bridge->native, &node, (UA_Boolean)pause);', '}']
    return header, metadata


def emit_stock_database(index, native_headers, configuration):
    path = native_headers / 'plugin/historydata/history_database_default.h'
    text = path.read_text()
    declaration = re.search(r'UA_HistoryDatabase\s+UA_EXPORT\s+UA_HistoryDatabase_default\s*\((.*?)\)\s*;', uncomment(text), re.S)
    if not declaration or parameters(declaration[1]) != [('UA_HistoryDataGathering', 'gathering')]:
        raise ValueError('Adapt default history database factory')
    methods = callbacks(public_body(native_headers / 'plugin/historydatabase.h', 'HistoryDatabase', configuration))
    header = [re.match(r'/\*.*?\*/', text, re.S)[0],
        '/** Native default history database with callable C89 slots. Success takes',
        ' * ownership of gathering context; failure leaves it with the caller.',
        ' * Copies alias one context: call exactly one clear after stopping polling.',
        ' * clear releases gathering/context without resetting the record, as native.',
        ' * Backend ownership remains the gathering implementation\'s native policy.',
        ' * Unsupported native slots remain NULL. Callback inputs borrow until return.',
        ' * Initialize mutable outputs before use and clear them even on failure.',
        ' * For reads, prepare response.results with decoded empty HistoryData payloads',
        ' * and historyData pointers that alias those payloads, as native requires.',
        ' * Retained payload roots keep their addresses; re-read response.results after',
        ' * return because conversion may replace its array. Conversion failure can',
        ' * follow a completed native update; it does not roll back backend mutation. */',
        'cpkt_opcua_HistoryDatabase cpkt_opcua_HistoryDatabase_default(cpkt_opcua_HistoryDataGathering gathering);']
    metadata = []
    for result, name, signature in methods:
        if name == 'clear':
            continue
        if result != 'void':
            raise ValueError('Adapt stock history database return: ' + name)
        declarations, setup, conversion, outputs, cleanup, callargs = [], [], [], [], [], []
        error_target = None
        history_type = None
        mutable = []
        for spelling, param in parameters(signature):
            bare = spelling.replace('const ', '').replace('*', '').strip()
            typename = bare[3:] if bare.startswith('UA_') else None
            if param == 'server':
                callargs.append('server ? server->server : NULL')
            elif param == 'hdbContext':
                callargs.append('bridge->native.context')
            elif spelling in ('void *', 'size_t'):
                callargs.append(param)
            elif param == 'historyData':
                match = re.fullmatch(r'UA_(\w+) \* const \* const', spelling)
                if not match or match[1] not in index:
                    raise ValueError('Adapt stock history pointer array: ' + spelling)
                history_type = match[1]
                declarations += [f'  UA_{history_type} **n_historyData = NULL;', '  size_t history_index;']
                conversion += ['  if(!status && (n_response.resultsSize != nodesToReadSize || (nodesToReadSize && !historyData))) status = UA_STATUSCODE_BADINVALIDARGUMENT;',
                    '  if(!status && nodesToReadSize > (size_t)-1 / sizeof(*n_historyData)) status = UA_STATUSCODE_BADOUTOFMEMORY;',
                    f'  if(!status && nodesToReadSize) {{ n_historyData = (UA_{history_type} **)UA_calloc(nodesToReadSize, sizeof(*n_historyData));',
                    '    if(!n_historyData) status = UA_STATUSCODE_BADOUTOFMEMORY; }',
                    '  for(history_index = 0; !status && history_index < nodesToReadSize; ++history_index) {',
                    '    const cpkt_opcua_ExtensionObject *eo = &response->results[history_index].historyData;',
                    f'    if(eo->encoding < CPKT_OPCUA_EXTENSIONOBJECT_DECODED || eo->content.decoded.type != &cpkt_types[{index[history_type]}] ||',
                    '        !historyData[history_index] || historyData[history_index] != eo->content.decoded.data) status = UA_STATUSCODE_BADTYPEMISMATCH;',
                    f'    else n_historyData[history_index] = (UA_{history_type} *)n_response.results[history_index].historyData.content.decoded.data;', '  }']
                cleanup += ['  UA_free(n_historyData);']
                callargs.append('n_historyData')
            elif param == 'nodesToRead':
                if spelling != 'const UA_HistoryReadValueId *':
                    raise ValueError('Adapt stock history input array')
                declarations += ['  void *n_nodesToRead = NULL;']
                conversion += [f'  if(!status) status = cpkt_array(nodesToRead, nodesToReadSize, &n_nodesToRead, &cpkt_types[{index[typename]}], 1, 0);']
                cleanup += [f'  if(n_nodesToRead) UA_Array_delete(n_nodesToRead, nodesToReadSize, cpkt_types[{index[typename]}].native);']
                callargs.append('(const UA_HistoryReadValueId *)n_nodesToRead')
            elif typename == 'NodeId' and spelling == 'const UA_NodeId *':
                declarations += [f'  UA_NodeId n_{param};']
                conversion += [f'  if(!cpkt_stock_node_valid({param})) status = UA_STATUSCODE_BADINVALIDARGUMENT;',
                               f'  n_{param} = cpkt_nodeid_view({param});']
                callargs.append(f'{param} ? &n_{param} : NULL')
            elif typename in index and spelling.count('*') <= 1:
                declarations += [f'  UA_{typename} n_{param};']
                setup += [f'  UA_{typename}_init(&n_{param});']
                pointer = param if '*' in spelling else '&' + param
                conversion += [f'  if(!status{f" && {param}" if "*" in spelling else ""}) status = cpkt_convert({pointer}, &n_{param}, &cpkt_types[{index[typename]}], 1, 0);']
                if '*' in spelling and not spelling.startswith('const '):
                    conversion += [f'  if(!{param}) status = UA_STATUSCODE_BADINVALIDARGUMENT;']
                    declarations += [f'  cpkt_opcua_{typename} staged_{param};']
                    setup += [f'  memset(&staged_{param}, 0, sizeof(staged_{param}));']
                    outputs += [f'  if(!status) status = cpkt_convert(&n_{param}, &staged_{param}, &cpkt_types[{index[typename]}], 0, 0);']
                    mutable.append((param, typename))
                    cleanup += [f'  cpkt_opcua_type_clear(&staged_{param}, &cpkt_types[{index[typename]}]);']
                    if param in ('response', 'result'):
                        error_target = param
                cleanup += [f'  UA_{typename}_clear(&n_{param});']
                callargs.append('&n_' + param if '*' in spelling else 'n_' + param)
            else:
                raise ValueError(f'Adapt stock database argument {name}.{param}: {spelling}')
        if history_type:
            outputs += ['  if(!status && staged_response.resultsSize == nodesToReadSize) {',
                '    for(history_index = 0; history_index < nodesToReadSize; ++history_index) {',
                '      cpkt_opcua_ExtensionObject *old = &response->results[history_index].historyData;',
                '      cpkt_opcua_ExtensionObject *staged = &staged_response.results[history_index].historyData;',
                '      const UA_ExtensionObject *n = &n_response.results[history_index].historyData;',
                f'      if(n->encoding >= UA_EXTENSIONOBJECT_DECODED && n->content.decoded.data == n_historyData[history_index] && staged->encoding >= CPKT_OPCUA_EXTENSIONOBJECT_DECODED && staged->content.decoded.type == &cpkt_types[{index[history_type]}]) {{',
                f'        cpkt_opcua_{history_type}_clear(historyData[history_index]);',
                f'        *historyData[history_index] = *(cpkt_opcua_{history_type} *)staged->content.decoded.data;',
                f'        cpkt_opcua_{history_type}_init((cpkt_opcua_{history_type} *)staged->content.decoded.data);',
                f'        cpkt_opcua_{history_type}_delete((cpkt_opcua_{history_type} *)staged->content.decoded.data);',
                '        staged->content.decoded.data = historyData[history_index]; staged->encoding = old->encoding;',
                '        old->encoding = CPKT_OPCUA_EXTENSIONOBJECT_DECODED_NODELETE;', '      }', '    }', '  }']
        for param, typename in mutable:
            outputs += [f'  if(!status) {{ cpkt_opcua_{typename}_clear({param}); *{param} = staged_{param}; memset(&staged_{param}, 0, sizeof(staged_{param})); }}']
        error = (f'  if(status && {error_target}) {error_target}->' +
                 ('responseHeader.serviceResult' if error_target == 'response' else 'statusCode') + ' = status;'
                 if error_target else
                 f'  if(status && server && server->server) UA_LOG_ERROR(UA_Server_getConfig(server->server)->logging, UA_LOGCATEGORY_SERVER, "C89 stock database {name} conversion failed: %08lx", (unsigned long)status);')
        metadata += [f'static void cpkt_stock_hdb_{name}({translate(signature)}) {{',
                     '  cpkt_stock_hdb *bridge = (cpkt_stock_hdb *)hdbContext;',
                     '  UA_StatusCode status = 0;', *declarations, *setup,
                     f'  if(!bridge || !bridge->native.{name} || (server && !server->server){" || !server" if name not in ("setValue", "setEvent") else ""}) status = UA_STATUSCODE_BADINVALIDARGUMENT;',
                     *conversion, f'  if(!status) {{ bridge->gathering->owner = server; bridge->native.{name}({", ".join(callargs)}); }}',
                     *outputs, error, *cleanup, '}']
    metadata += ['static void cpkt_stock_hdb_clear(cpkt_opcua_HistoryDatabase *database) {',
                 '  cpkt_stock_hdb *bridge = (cpkt_stock_hdb *)database->context;',
                 '  bridge->native.clear(&bridge->native); UA_free(bridge);', '}',
                 'cpkt_opcua_HistoryDatabase cpkt_opcua_HistoryDatabase_default(cpkt_opcua_HistoryDataGathering gathering) {',
                 '  cpkt_opcua_HistoryDatabase out;', '  cpkt_stock_hdb *bridge;', '  UA_HistoryDataGathering native_gathering;',
                 '  memset(&out, 0, sizeof(out));',
                 '  bridge = (cpkt_stock_hdb *)UA_calloc(1, sizeof(*bridge)); if(!bridge) return out;',
                 '  bridge->gathering = (cpkt_hg_bridge *)UA_calloc(1, sizeof(*bridge->gathering));',
                 '  if(!bridge->gathering) { UA_free(bridge); return out; }',
                 '  bridge->gathering->plugin = gathering; cpkt_hg_assign(&native_gathering, &gathering); native_gathering.context = bridge->gathering;',
                 '  bridge->native = UA_HistoryDatabase_default(native_gathering);',
                 '  if(!bridge->native.context) { UA_free(bridge->gathering); UA_free(bridge); return out; }',
                 '  out.context = bridge; out.clear = cpkt_stock_hdb_clear;',
                 *[f'  out.{name} = bridge->native.{name} ? cpkt_stock_hdb_{name} : NULL;' for _, name, _ in methods if name != 'clear'],
                 '  return out;', '}']
    return header, metadata
