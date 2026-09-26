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
        '     * outputs and clears every C89 slot after return, including on failure. */\n'
        '    cpkt_opcua_StatusCode\n    (*copyDataValues)')
    public_backend = public_backend.replace('cpkt_opcua_TRUE', 'nonzero')
    header = [re.match(r'/\*.*?\*/', text, re.S)[0],
              re.match(r'/\*.*?\*/', native_types, re.S)[0],
              '/** Inclusive numeric range dimension, borrowed during history callbacks. */', translate(dimensions[0]),
              '/** History callback range; dimensions are borrowed until callback return. */', translate(numeric_range[0]),
              '/** Native historical timestamp matching criteria. */', match_enum,
              '/** Native historical-value collection policy. POLL requires start_poll. */',
              translate(strategy[0]).replace('cpkt_opcua_HISTORIZING', 'CPKT_OPCUA_HISTORIZING'),
              '/** Persistent backend-owned value. Its native address survives other callbacks.',
              ' * Do not set/free it while any native history operation may borrow it.',
              ' * Synchronize callbacks and storage changes using the upstream server rules. */',
              'typedef struct cpkt_opcua_history_value cpkt_opcua_history_value;',
              '/** Deep-copy value into persistent storage. On failure *out is NULL. */',
              'cpkt_opcua_StatusCode cpkt_opcua_history_value_new(const cpkt_opcua_DataValue *value, cpkt_opcua_history_value **out);',
              '/** Replace a quiescent stored value. Failure preserves the previous value.',
              ' * The object address stays stable; existing borrowers must finish first. */',
              'cpkt_opcua_StatusCode cpkt_opcua_history_value_set(cpkt_opcua_history_value *stored, const cpkt_opcua_DataValue *value);',
              '/** Return an owned C89 copy into empty out. Clear with DataValue_clear.',
              ' * Failure leaves out empty; this does not expose a native borrowed pointer. */',
              'cpkt_opcua_StatusCode cpkt_opcua_history_value_get(const cpkt_opcua_history_value *stored, cpkt_opcua_DataValue *out);',
              '/** Release a quiescent stored value; NULL is safe. Normally called from',
              ' * backend deleteMembers after the gathering stops using that backend. */',
              'void cpkt_opcua_history_value_free(cpkt_opcua_history_value *stored);',
              '/** Full public history storage backend. Inputs borrow until return; output',
              ' * records/arrays own C89 allocations, cleared by the bridge after conversion.',
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
              ' * Each registration transfers context to deleteMembers only on success.',
              ' * Shared contexts require caller-managed references; userContext is borrowed.',
              ' * maxHistoryDataResponseSize must be nonzero. Polling starts explicitly. */',
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
                callargs.append('bridge->owner')
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
            ret = '  return stored ? &stored->native : NULL;'
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
                     *([f'  if(status) UA_LOG_ERROR(UA_Server_getConfig(server)->logging, UA_LOGCATEGORY_SERVER, "C89 history backend {name} conversion failed: %08lx", (unsigned long)status);'] if result != 'UA_StatusCode' else []),
                     *cleanup, ret, '}']
        assignments.append(f'  native->{name} = plugin->{name} ? cpkt_hb_{name} : NULL;')
    metadata += ['static void cpkt_hb_assign(UA_HistoryDataBackend *native, const cpkt_opcua_HistoryDataBackend *plugin) {',
                 '  memset(native, 0, sizeof(*native));', *assignments, '}']
    return header, metadata
