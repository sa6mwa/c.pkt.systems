"""Generate usable public node callbacks from native handwritten declarations."""
import re
from plugin_emitter import callbacks, parameters, translate, uncomment, validate_fields


def record_body(text, name):
    text = uncomment(text)
    end = re.search(r'}\s*UA_' + re.escape(name) + r'\s*;', text)
    if not end:
        raise ValueError(f'Missing public node record: {name}')
    depth = 1
    start = end.start() - 1
    while start >= 0:
        depth += (text[start] == '}') - (text[start] == '{')
        if not depth:
            if not re.search(r'typedef\s+struct\s*$', text[:start]):
                raise ValueError(f'Adapt public node record: {name}')
            return text[start + 1:end.start()]
        start -= 1
    raise ValueError(f'Unbalanced public node record: {name}')


def emit_nodes(index, native_headers):
    text = (native_headers / 'server.h').read_text()
    external = re.search(r'UA_Server_setVariableNode_externalValueSource\s*\((.*?)\)\s*;', uncomment(text), re.S)
    if not external or parameters(external[1]) != [
            ('UA_Server *', 'server'), ('const UA_NodeId', 'nodeId'),
            ('UA_DataValue **', 'value'),
            ('const UA_ValueSourceNotifications *', 'notifications')]:
        raise ValueError('Adapt public external value source declaration')
    header = [re.match(r'/\*.*?\*/', text, re.S)[0]]
    metadata = []
    slots = {'ValueSourceNotifications': ('onRead', 'onWrite'),
             'NodeTypeLifecycle': ('constructor', 'destructor'),
             'GlobalNodeLifecycle': ('constructor', 'destructor', 'createOptionalChild', 'generateChildNodeId')}
    for record, expected in slots.items():
        body = record_body(text, record)
        validate_fields(body, [])
        methods = callbacks(body)
        if tuple(name for _, name, _ in methods) != expected:
            raise ValueError(f'Adapt node callback slots: {record}')
        header += ['/** Complete native callback record. NodeIds and ranges are borrowed until',
                   ' * return; never clear or retain them. Notification values are borrowed C89',
                   ' * copies. Original session, node and type contexts are preserved. Mutable',
                   ' * context pointers are forwarded directly. Conversion failures return a',
                   ' * status/false or are logged for void notifications. Node destructors use',
                   ' * allocation-free NodeId views and always receive the original contexts. */',
                   'typedef struct {', translate(body), '} cpkt_opcua_' + record + ';']
        assignments = []
        for result, name, signature in methods:
            if result not in ('void', 'UA_StatusCode', 'UA_Boolean'):
                raise ValueError(f'Adapt node callback return: {record}.{name}')
            args = parameters(signature)
            if args[0] != ('UA_Server *', 'server'):
                raise ValueError(f'Adapt node callback receiver: {record}.{name}')
            local, convert, cleanup, outputs, call = [], [], [], [], []
            for spelling, param in args:
                if param == 'server':
                    call.append('owner->owner')
                elif spelling in ('void *', 'void **'):
                    call.append(param)
                elif spelling == 'const UA_NodeId *':
                    local.append(f'  cpkt_opcua_NodeId c_{param};')
                    convert.append(f'  if({param}) cpkt_hb_borrow_node({param}, &c_{param});')
                    call.append(f'{param} ? &c_{param} : NULL')
                elif spelling == 'UA_NodeId *':
                    local += [f'  cpkt_opcua_NodeId c_{param};', f'  UA_NodeId staged_{param};']
                    convert += [f'  cpkt_opcua_NodeId_init(&c_{param});', f'  UA_NodeId_init(&staged_{param});',
                                f'  if(!status && {param}) status = cpkt_convert({param}, &c_{param}, &cpkt_types[{index["NodeId"]}], 0, 0);']
                    call.append(f'{param} ? &c_{param} : NULL')
                    # The native target starts empty. Callback output must be owned.
                    outputs += [f'  if(!status && {param}) status = cpkt_convert(&c_{param}, &staged_{param}, &cpkt_types[{index["NodeId"]}], 1, 0);',
                                f'  if(!status && {param}) {{ UA_NodeId_clear({param}); *{param} = staged_{param}; UA_NodeId_init(&staged_{param}); }}']
                    cleanup += [f'  cpkt_opcua_NodeId_clear(&c_{param});', f'  UA_NodeId_clear(&staged_{param});']
                elif spelling == 'const UA_NumericRange *':
                    local.append(f'  cpkt_opcua_NumericRange c_{param};')
                    convert += [f'  memset(&c_{param}, 0, sizeof(c_{param}));',
                                f'  if(!status && {param}) status = cpkt_nodes_range({param}, &c_{param});']
                    cleanup.append(f'  UA_free(c_{param}.dimensions);')
                    call.append(f'{param} ? &c_{param} : NULL')
                elif spelling == 'const UA_DataValue *':
                    local.append(f'  cpkt_opcua_DataValue c_{param};')
                    convert += [f'  cpkt_opcua_DataValue_init(&c_{param});',
                                f'  if(!status && {param}) status = cpkt_convert({param}, &c_{param}, &cpkt_types[{index["DataValue"]}], 0, 0);']
                    cleanup.append(f'  cpkt_opcua_DataValue_clear(&c_{param});')
                    call.append(f'{param} ? &c_{param} : NULL')
                else:
                    raise ValueError(f'Adapt node callback argument: {record}.{name}.{param}: {spelling}')
            global_record = record == 'GlobalNodeLifecycle'
            lookup = 'NULL' if global_record else 'typeNodeId' if record == 'NodeTypeLifecycle' else 'nodeid' if name == 'onRead' else 'nodeId'
            group = 'lifecycle' if record == 'NodeTypeLifecycle' else 'notifications'
            lookup_line = '  cpkt_nodes_owner *owner = cpkt_nodes_find_owner(server);' if global_record else f'  cpkt_nodes_entry *entry = cpkt_nodes_acquire(server, {lookup});\n  cpkt_nodes_owner *owner = entry ? entry->owner : NULL;'
            target = 'owner->global' if global_record else f'entry->{group}'
            invocation = f'plugin.{name}({", ".join(call)})'
            default = '' if result == 'void' else ' UA_STATUSCODE_BADINTERNALERROR' if result == 'UA_StatusCode' else ' 0'
            metadata += [f'static {result} cpkt_nodes_{record}_{name}({signature}) {{', lookup_line,
                         f'  cpkt_opcua_{record} plugin;', '  UA_StatusCode status = 0;',
                         *([f'  {result} result = 0;'] if result != 'void' else []), *local,
                         f'  if(!owner) return{default};',
                         '  ++owner->owner->typed_config_depth;', f'  plugin = {target};', *convert]
            if result == 'void':
                metadata.append(f'  if(!status && plugin.{name}) {invocation};')
            else:
                metadata.append(f'  if(!status && plugin.{name}) result = {invocation};')
                if result == 'UA_StatusCode':
                    metadata.append('  if(!status) status = result;')
            error_condition = 'status && status != result' if result == 'UA_StatusCode' else 'status'
            metadata += outputs + cleanup + [
                f'  if({error_condition}) UA_LOG_ERROR(UA_Server_getConfig(server)->logging, UA_LOGCATEGORY_SERVER, "C89 {record}.{name} conversion failed: %08lx", (unsigned long)status);']
            metadata.append('  --owner->owner->typed_config_depth;')
            if not global_record:
                metadata.append('  cpkt_nodes_release(entry);')
            if result != 'void':
                metadata.append('  return status ? status : result;' if result == 'UA_StatusCode' else '  return status ? 0 : result;')
            metadata.append('}')
            assignments.append(f'  native->{name} = plugin->{name} ? cpkt_nodes_{record}_{name} : NULL;')
        metadata += [f'static void cpkt_nodes_assign_{record}(UA_{record} *native, const cpkt_opcua_{record} *plugin) {{',
                     '  memset(native, 0, sizeof(*native));', *assignments, '}']
    header += ['/** Upstream public spelling retained for its notification record alias. */',
        'typedef cpkt_opcua_ValueSourceNotifications cpkt_opcua_ValueCallback;']
    header += [
        '/** Stable native DataValue pointer slot. Borrows persistent history_value storage.',
        ' * The holder and selected storage must outlive every node using them. Serialize',
        ' * selection/storage changes with all native users, including other servers that',
        ' * share this holder. No reference counting or automatic lifetime extension. */',
        'typedef struct cpkt_opcua_external_value cpkt_opcua_external_value;',
        '/** Create a pointer slot selecting stored without copying its DataValue.',
        ' * On failure *out is NULL. stored must be non-NULL and remains caller-owned. */',
        'cpkt_opcua_StatusCode cpkt_opcua_external_value_new(cpkt_opcua_history_value *stored, cpkt_opcua_external_value **out);',
        '/** Select different persistent storage without allocating/copying a value.',
        ' * NULL is rejected, preserving selection. Call only with native users quiescent',
        ' * or inside the serialized onRead hook; the native read reloads this slot. */',
        'cpkt_opcua_StatusCode cpkt_opcua_external_value_set(cpkt_opcua_external_value *value, cpkt_opcua_history_value *stored);',
        '/** Borrow the selected storage; NULL holder returns NULL. Native writes mutate',
        ' * this storage, observable through history_value_get. No ownership transfer. */',
        'cpkt_opcua_history_value *cpkt_opcua_external_value_get(const cpkt_opcua_external_value *value);',
        '/** Free a quiescent, detached holder; NULL is safe. Does not free storage.',
        ' * Detach EVERY node or delete its server before freeing holder or storage. */',
        'void cpkt_opcua_external_value_free(cpkt_opcua_external_value *value);',
        '/** Native external source installation. Borrows the holder pointer slot, not',
        ' * a snapshot; nodes reload its selection when reading and write into selected',
        ' * native storage. Notifications are copied, preserve original node contexts,',
        ' * and receive callback-lifetime C89 copies. NULL notifications disables them.',
        ' * Failure preserves the previous source/notification record. Native NULL source',
        ' * rejection is preserved. Detach via another source or delete all borrowing nodes',
        ' * before free. Serialize server calls and changes to shared holders/storage. */',
        'cpkt_opcua_StatusCode cpkt_opcua_server_setVariableNode_externalValueSource_typed(cpkt_opcua_server *server, cpkt_opcua_NodeId nodeId, cpkt_opcua_external_value *value, const cpkt_opcua_ValueSourceNotifications *notifications);',
        '/** Replace internal value source and optional notifications using native behavior.',
        ' * NULL value follows native source-switching behavior; NULL notifications disables',
        ' * notifications. Input values/slots are borrowed during installation and copied.',
        ' * Contexts remain caller-owned. Failure preserves the previous notification record.',
        ' * Calls on a server are serialized; replacement from a notification is supported.',
        ' * Do not destroy the server from its callback. Dispatch metadata lives until replacement',
        ' * or server destruction. Conversion failures in notifications are logged and skip dispatch. */',
        'cpkt_opcua_StatusCode cpkt_opcua_server_setVariableNode_internalValueSource_typed(cpkt_opcua_server *server, cpkt_opcua_NodeId nodeId, const cpkt_opcua_DataValue *value, const cpkt_opcua_ValueSourceNotifications *notifications);',
        '/** Install copied node-type lifecycle slots with original mutable node contexts.',
        ' * Callback NodeIds are borrowed views; constructor/destructor use no conversion allocation.',
        ' * NULL slots preserve native behavior. Replacement is safe inside a callback.',
        ' * Contexts remain caller-owned; failure preserves the previous callback record. */',
        'cpkt_opcua_StatusCode cpkt_opcua_server_setNodeTypeLifecycle_typed(cpkt_opcua_server *server, cpkt_opcua_NodeId nodeId, cpkt_opcua_NodeTypeLifecycle lifecycle);',
        '/** Set copied global lifecycle slots before startup; NULL disables the native pointer.',
        ' * Original node/session contexts are preserved. A generated child NodeId must own its',
        ' * payload; the bridge converts it and clears it after return. Replacement is allowed',
        ' * in callbacks before startup. Caller contexts remain owned by the caller. */',
        'cpkt_opcua_StatusCode cpkt_opcua_server_set_global_node_lifecycle(cpkt_opcua_server *server, const cpkt_opcua_GlobalNodeLifecycle *lifecycle);']
    return header, metadata
