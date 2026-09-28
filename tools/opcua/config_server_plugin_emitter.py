"""Callable views of native server plugins; never clone their private contexts."""
from plugin_emitter import public_body, callbacks, parameters, translate
from node_emitter import record_body
from history_emitter import emit_stock_database


def emit_server_plugin_views(index, headers, configuration):
    h = ['/** Borrow complete callable native plugin views. Metadata is a snapshot;',
         ' * slots invoke the current configuration backend and preserve application',
         ' * contexts. Never clear metadata yourself or install these views as owned',
         ' * plugins. clear forwards backend cleanup only while config is quiescent.',
         ' * Serialize with configuration and parent handle operations. */']
    impl = []
    for cls, prefix, body in [('AccessControl', 'ac', public_body(headers / 'plugin/accesscontrol.h', 'AccessControl', configuration)),
                             ('GlobalNodeLifecycle', 'glc', record_body((headers / 'server.h').read_text(), 'GlobalNodeLifecycle'))]:
        assignments = []
        for returns, method, signature in callbacks(body):
            if method == 'clear':
                continue
            name = f'cpkt_cfg_view_{prefix}_{method}'
            args = parameters(signature)
            declarations, setup, call, cleanup, outputs = [], [], [], [], []
            for spelling, param in args:
                if param == 'server':
                    call.append('server ? server->server : NULL')
                elif param == 'ac':
                    call.append('native')
                elif spelling in ('void *', 'void **'):
                    call.append(param)
                else:
                    typename = spelling.replace('const ', '').replace('*', '').strip()
                    typename = 'Boolean' if typename == 'bool' else typename.removeprefix('UA_')
                    if typename not in index or spelling.count('*') > 1:
                        raise ValueError(f'Adapt native plugin view {cls}.{method}.{param}: {spelling}')
                    mutable = '*' in spelling and not spelling.startswith('const ')
                    declarations += [f'  UA_{typename} n_{param};']
                    setup += [f'  UA_{typename}_init(&n_{param});',
                              f'  if(!status{" && " + param if "*" in spelling else ""}) status = cpkt_convert({param if "*" in spelling else "&" + param}, &n_{param}, &cpkt_types[{index[typename]}], 1, 0);']
                    cleanup += [f'  UA_{typename}_clear(&n_{param});']
                    call.append(f'{param} ? &n_{param} : NULL' if '*' in spelling else f'n_{param}')
                    if mutable:
                        declarations += [f'  cpkt_opcua_{typename} staged_{param};']
                        setup += [f'  memset(&staged_{param}, 0, sizeof(staged_{param}));']
                        outputs += [f'  if(!status && {param}) status = cpkt_convert(&n_{param}, &staged_{param}, &cpkt_types[{index[typename]}], 0, 0);',
                                    f'  if(!status && {param}) {{ cpkt_opcua_{typename}_clear({param}); *{param} = staged_{param}; memset(&staged_{param}, 0, sizeof(staged_{param})); }}']
                        cleanup += [f'  cpkt_opcua_{typename}_clear(&staged_{param});']
            fallback = 'UA_STATUSCODE_BADINVALIDARGUMENT' if returns == 'UA_StatusCode' else '0'
            impl += [f'static {translate(returns)} {name}({translate(signature)}) {{',
                     '  cpkt_cfg_plugin_view *view;', f'  UA_{cls} *native;',
                     '  UA_StatusCode status = 0;',
                     *([] if returns == 'void' else [f'  {returns.replace("bool", "UA_Boolean")} result = 0;']), *declarations]
            if prefix == 'ac':
                impl += ['  view = ac ? (cpkt_cfg_plugin_view *)ac->context : NULL;']
            else:
                impl += ['  view = cpkt_cfg_global_view(server);']
            impl += [f'  native = view ? (UA_{cls} *)cpkt_cfg_view_native(view) : NULL;',
                     f'  if(!native || !native->{method}) return {"" if returns == "void" else fallback};',
                     f'  if(!server || !server->server || (view->server->owner && view->server->owner != server)) return {"" if returns == "void" else fallback};',
                     '  cpkt_cfg_view_enter(view);', *setup]
            impl += [f'  if(!status) {"" if returns == "void" else "result = "}native->{method}({", ".join(call)});', *outputs]
            if returns == 'void':
                impl += ['  if(status) cpkt_cfg_view_error(view, status);']
            impl += [*cleanup, '  cpkt_cfg_view_leave(view);']
            if returns != 'void':
                impl += ['  return status ? ' + ('status' if returns == 'UA_StatusCode' else '0') + ' : result;']
            impl += ['}']
            assignments += [f'  out->{method} = native->{method} ? {name} : NULL;']
        if prefix == 'ac':
            impl += ['static void cpkt_cfg_view_ac_clear(cpkt_opcua_AccessControl *record) {',
                     '  if(record && record->context) cpkt_cfg_view_clear_backend((cpkt_cfg_plugin_view *)record->context);', '}']
        impl += [f'static void cpkt_cfg_view_{prefix}_assign(cpkt_cfg_plugin_view *view, UA_{cls} *native) {{',
                 f'  cpkt_opcua_{cls} *out = &view->record.{prefix};', *assignments]
        if prefix == 'ac':
            impl += ['  out->context = view;', '  out->clear = native->clear ? cpkt_cfg_view_ac_clear : NULL;']
        impl += ['}']
    impl += ['static int cpkt_cfg_global_is_view(const cpkt_opcua_GlobalNodeLifecycle *plugin) {',
             '  return plugin && (plugin->constructor == cpkt_cfg_view_glc_constructor ||',
             '    plugin->destructor == cpkt_cfg_view_glc_destructor ||',
             '    plugin->createOptionalChild == cpkt_cfg_view_glc_createOptionalChild ||',
             '    plugin->generateChildNodeId == cpkt_cfg_view_glc_generateChildNodeId);', '}']
    impl += emit_stock_database(index, headers, configuration, configuration_view=True)[1]
    methods = callbacks(public_body(headers / 'plugin/historydatabase.h', 'HistoryDatabase', configuration))
    impl += ['static void cpkt_cfg_view_hdb_clear(cpkt_opcua_HistoryDatabase *record) {',
             '  if(record && record->context) cpkt_cfg_view_clear_backend((cpkt_cfg_plugin_view *)record->context);', '}',
             'static void cpkt_cfg_view_hdb_assign(cpkt_cfg_plugin_view *view, UA_HistoryDatabase *native) {',
             '  cpkt_opcua_HistoryDatabase *out = &view->record.hdb;',
             *[f'  out.{name} = native->{name} ? cpkt_cfg_view_hdb_{name} : NULL;'.replace('out.', 'out->') for _, name, _ in methods if name != 'clear'],
             '  out->context = view;', '  out->clear = native->clear ? cpkt_cfg_view_hdb_clear : NULL;', '}']
    for cls, prefix in [('AccessControl','ac'), ('HistoryDatabase','hdb'), ('GlobalNodeLifecycle','glc')]:
        h += ['/** Inspect native configuration through its C89 view. Owned settings outputs require their matching clear; plugin/handle views belong to configuration. Serialize with reconfiguration and dispatch. */\n' f'cpkt_opcua_StatusCode cpkt_opcua_ServerConfig_get{cls}(cpkt_opcua_ServerConfig *config, cpkt_opcua_{cls} **plugin);']
        impl += [f'cpkt_opcua_StatusCode cpkt_opcua_ServerConfig_get{cls}(cpkt_opcua_ServerConfig *config, cpkt_opcua_{cls} **plugin) {{',
                 '  cpkt_cfg_plugin_view *view;', '  UA_StatusCode status;',
                 '  if(!plugin) return UA_STATUSCODE_BADINVALIDARGUMENT;', '  *plugin = NULL;',
                 '  if(!config || !config->native) return UA_STATUSCODE_BADINVALIDARGUMENT;']
        if prefix == 'glc':
            impl += ['  if(!config->native->nodeLifecycle) return 0;']
        impl += [f'  status = cpkt_cfg_view_get(NULL, config, CPKT_CFG_{prefix.upper()}, 0, &view);',
                 f'  if(!status) *plugin = &view->record.{prefix};', '  return status;', '}']
    return h, impl
