"""Bind configuration-owned native security plugins without cloning backends.

Views resolve the current public native record on each call, including after a
policy-array reallocation. Only C89 metadata snapshots and callback signatures
are generated here; stores, cryptography and private contexts stay native.
"""
import re
from plugin_emitter import public_body, callbacks, translate, parameters, uncomment
from policy_emitter import policy_methods, bridge_arguments
from security_emitter import GROUP_METHODS
from config_server_plugin_emitter import emit_server_plugin_views


def emit_config_plugins(index, headers, output):
    h, impl = [], []
    configuration = (headers / 'config.h').read_text()
    for cls, prefix, relative in [('SecurityPolicy', 'sp', 'plugin/securitypolicy.h'),
                                  ('PubSubSecurityPolicy', 'psp', 'plugin/securitypolicy.h'),
                                  ('CertificateGroup', 'cg', 'plugin/certificategroup.h')]:
        body = public_body(headers / relative, cls, configuration)
        methods = policy_methods(headers, cls)[1] if cls != 'CertificateGroup' else [('', ret, name, sig) for ret, name, sig in callbacks(body)]
        assignments = []
        for parent, returns, method, signature in methods:
            if method == 'clear':
                continue
            member = parent + method
            name = f'cpkt_cfg_view_{prefix}_{member.replace(".", "_")}'
            args = parameters(signature)
            receiver = args[0][1]
            field = 'context' if prefix == 'cg' else 'policyContext'
            fallback = 'UA_STATUSCODE_BADINVALIDARGUMENT' if returns == 'UA_StatusCode' else '0'
            lines = [f'static {translate(returns)} {name}({translate(signature)}) {{',
                     '  cpkt_cfg_plugin_view *view;', f'  UA_{cls} *native;',
                     '  UA_StatusCode status = 0;']
            if cls != 'CertificateGroup':
                declarations, setup, outputs, cleanup, call = bridge_arguments(index, signature, True)
                lines += ([] if returns == 'void' else [f'  {returns} result = 0;']) + declarations
                lines += [f'  if(!{receiver} || !{receiver}->{field}) return {"" if returns == "void" else fallback};',
                          f'  view = (cpkt_cfg_plugin_view *){receiver}->{field};',
                          f'  native = (UA_{cls} *)cpkt_cfg_view_native(view);',
                          f'  if(!native || !native->{member}) return {"" if returns == "void" else "UA_STATUSCODE_BADNOTSUPPORTED" if returns == "UA_StatusCode" else "0"};',
                          '  cpkt_cfg_view_enter(view);', *setup,
                          f'  if(!status) {{ {"" if returns == "void" else "result = "}native->{member}(native{", " if call else ""}{", ".join(call)});',
                          *outputs, '  }', *cleanup]
                if not args[0][0].startswith('const '):
                    lines += ['  if(!status) status = cpkt_cfg_view_refresh(view);']
                if returns == 'void':
                    lines += ['  if(status) cpkt_cfg_view_error(view, status);']
                lines += ['  cpkt_cfg_view_leave(view);']
                if returns != 'void':
                    lines += ['  return status ? ' + ('status' if returns == 'UA_StatusCode' else '0') + ' : result;']
            else:
                if [a for _, a in args[1:]] != [a for _, a in GROUP_METHODS[method]]:
                    raise ValueError('Adapt certificate configuration view: ' + method)
                lines += ['  UA_StatusCode result = 0;']
                setup, finish, call, cleanup = [], [], [], []
                if method in ('getTrustList', 'setTrustList', 'addToTrustList', 'removeFromTrustList'):
                    lines += ['  UA_TrustListDataType value;', '  cpkt_opcua_TrustListDataType staged;']
                    setup += ['  memset(&value, 0, sizeof(value)); memset(&staged, 0, sizeof(staged));',
                              '  if(!trustList) status = UA_STATUSCODE_BADINVALIDARGUMENT;']
                    if method != 'getTrustList':
                        setup += [f'  if(!status) status = cpkt_convert(trustList, &value, &cpkt_types[{index["TrustListDataType"]}], 1, 0);']
                    else:
                        finish += [f'  if(!status) status = cpkt_convert(&value, &staged, &cpkt_types[{index["TrustListDataType"]}], 0, 0);',
                                   '  if(!status) { *trustList = staged; memset(&staged, 0, sizeof(staged)); }']
                    cleanup += ['  UA_TrustListDataType_clear(&value); cpkt_opcua_TrustListDataType_clear(&staged);']
                    call += ['&value']
                else:
                    if method != 'getRejectedList':
                        lines += ['  UA_ByteString certificate_view;']
                        setup += ['  if(!certificate || !cpkt_string_valid(certificate)) status = UA_STATUSCODE_BADINVALIDARGUMENT;',
                                  '  memset(&certificate_view, 0, sizeof(certificate_view)); if(certificate) certificate_view = cpkt_string_view(certificate);']
                        call += ['&certificate_view']
                    if method == 'getCertificateCrls':
                        call += ['isTrusted']
                    if method in ('getRejectedList', 'getCertificateCrls'):
                        pointer, size = ('rejectedList', 'rejectedListSize') if method == 'getRejectedList' else ('crls', 'crlsSize')
                        lines += ['  UA_ByteString *values = NULL;', '  void *staged = NULL;', '  size_t count = 0;']
                        setup += [f'  if(!{pointer} || !{size}) status = UA_STATUSCODE_BADINVALIDARGUMENT;']
                        call += ['&values', '&count']
                        finish += [f'  if(!status) status = cpkt_array(values, count, &staged, &cpkt_types[{index["ByteString"]}], 0, 0);',
                                   f'  if(!status) {{ *{pointer} = (cpkt_opcua_ByteString *)staged; *{size} = count; staged = NULL; }}']
                        cleanup += [f'  if(values) UA_Array_delete(values, count, cpkt_types[{index["ByteString"]}].native);',
                                    f'  if(staged) cpkt_clear_array(staged, count, &cpkt_types[{index["ByteString"]}]);']
                lines += [f'  if(!{receiver} || !{receiver}->context) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                          f'  view = (cpkt_cfg_plugin_view *){receiver}->context;',
                          '  native = (UA_CertificateGroup *)cpkt_cfg_view_native(view);',
                          f'  if(!native || !native->{method}) return UA_STATUSCODE_BADNOTSUPPORTED;',
                          '  cpkt_cfg_view_enter(view);', *setup,
                          f'  if(!status) result = native->{method}(native, {", ".join(call)});',
                          *finish, *cleanup, '  cpkt_cfg_view_leave(view);', '  return status ? status : result;']
            lines += ['}']
            impl += lines
            assignments += [f'  out->{member} = native->{member} ? {name} : NULL;']
        impl += [f'static void cpkt_cfg_view_{prefix}_clear(cpkt_opcua_{cls} *record) {{',
                 f'  cpkt_cfg_plugin_view *view = record ? (cpkt_cfg_plugin_view *)record->{field} : NULL;',
                 '  if(view) cpkt_cfg_view_clear_backend(view);', '}',
                 f'static void cpkt_cfg_view_{prefix}_assign(cpkt_cfg_plugin_view *view, UA_{cls} *native) {{',
                 f'  cpkt_opcua_{cls} *out = &view->record.{prefix};', *assignments,
                 f'  out->{field} = view;', f'  out->clear = native->clear ? cpkt_cfg_view_{prefix}_clear : NULL;', '}']
    for kind in ('Client', 'Server'):
        config = 'cpkt_opcua_' + kind + 'Config'
        h += ['/** Configuration-owned plugin view. Never free/copy/install its record as',
              ' * an owned plugin. Fields are metadata snapshots; use configuration setters',
              ' * to change settings. Callback methods operate the ACTUAL native backend.',
              ' * clear forwards native backend cleanup while quiescent and detaches the',
              ' * cleared plugin from configuration ownership. View storage remains owned',
              ' * by the configuration. Replacement/clear/destruction invalidates borrowers.',
              ' * Array growth alone retains view identity. Operations serialize with config. */']
        selector = ', cpkt_opcua_Boolean session' if kind == 'Server' else ''
        h += [f'cpkt_opcua_StatusCode cpkt_opcua_{kind}Config_getCertificateGroup({config} *config{selector}, cpkt_opcua_CertificateGroup **group);',
              '/** Consume a complete owned group on Good; failure preserves it. */',
              f'cpkt_opcua_StatusCode cpkt_opcua_{kind}Config_setCertificateGroup({config} *config{selector}, cpkt_opcua_CertificateGroup *group);']
        auth = ', cpkt_opcua_Boolean auth' if kind == 'Client' else ''
        h += ['/** Inspect native configuration through its C89 view. Owned settings outputs require their matching clear; plugin/handle views belong to configuration. Serialize with reconfiguration and dispatch. */\n' f'cpkt_opcua_StatusCode cpkt_opcua_{kind}Config_getSecurityPolicy({config} *config{auth}, size_t index, cpkt_opcua_SecurityPolicy **policy);',
              '/** Inspect native configuration through its C89 view. Owned settings outputs require their matching clear; plugin/handle views belong to configuration. Serialize with reconfiguration and dispatch. */\n' f'cpkt_opcua_StatusCode cpkt_opcua_{kind}Config_getSecurityPoliciesSize(const {config} *config{auth}, size_t *size);',
              '/** Append/replace owned policies before startup/connection. Good consumes',
              ' * each complete source record; failure preserves all sources and the old',
              ' * configuration. Zero size clears the array. Native contexts are not cloned. */',
              f'cpkt_opcua_StatusCode cpkt_opcua_{kind}Config_addSecurityPolicy({config} *config{auth}, cpkt_opcua_SecurityPolicy *policy);',
              '/** Configure the actual native record while quiescent. Schema inputs borrow until return; plugin/context ownership follows this family contract. Failures preserve input ownership before native handoff. */\n' f'cpkt_opcua_StatusCode cpkt_opcua_{kind}Config_setSecurityPolicies({config} *config{auth}, cpkt_opcua_SecurityPolicy *policies, size_t size);']
    h += ['/** Inspect native configuration through its C89 view. Owned settings outputs require their matching clear; plugin/handle views belong to configuration. Serialize with reconfiguration and dispatch. */\ncpkt_opcua_StatusCode cpkt_opcua_ServerConfig_getPubSubSecurityPolicy(cpkt_opcua_ServerConfig *config, size_t index, cpkt_opcua_PubSubSecurityPolicy **policy);',
          '/** Inspect native configuration through its C89 view. Owned settings outputs require their matching clear; plugin/handle views belong to configuration. Serialize with reconfiguration and dispatch. */\ncpkt_opcua_StatusCode cpkt_opcua_ServerConfig_getPubSubSecurityPoliciesSize(const cpkt_opcua_ServerConfig *config, size_t *size);',
          '/** Configure the actual native record while quiescent. Schema inputs borrow until return; plugin/context ownership follows this family contract. Failures preserve input ownership before native handoff. */\ncpkt_opcua_StatusCode cpkt_opcua_ServerConfig_addPubSubSecurityPolicy(cpkt_opcua_ServerConfig *config, cpkt_opcua_PubSubSecurityPolicy *policy);',
          '/** Configure the actual native record while quiescent. Schema inputs borrow until return; plugin/context ownership follows this family contract. Failures preserve input ownership before native handoff. */\ncpkt_opcua_StatusCode cpkt_opcua_ServerConfig_setPubSubSecurityPolicies(cpkt_opcua_ServerConfig *config, cpkt_opcua_PubSubSecurityPolicy *policies, size_t size);']
    h += ['/** Install complete callback tables on a stopped/standalone configuration.',
          ' * The facade keeps one stable server identity through native construction;',
          ' * constructor callbacks receive that same eventual handle. No temporary',
          ' * native server or private backend clone is created. Plugin records are',
          ' * copied; context clear ownership transfers on Good, preserving the existing',
          ' * plugin on staging failure. Access-control token policies are copied.',
          ' * Callbacks borrow arguments and cannot destroy the configuration/handle. */',
          'cpkt_opcua_StatusCode cpkt_opcua_ServerConfig_setAccessControl(cpkt_opcua_ServerConfig *config, const cpkt_opcua_AccessControl *plugin);',
          '/** Configure the actual native record while quiescent. Schema inputs borrow until return; plugin/context ownership follows this family contract. Failures preserve input ownership before native handoff. */\ncpkt_opcua_StatusCode cpkt_opcua_ServerConfig_setHistoryDatabase(cpkt_opcua_ServerConfig *config, const cpkt_opcua_HistoryDatabase *plugin);',
          '/** Copy the global lifecycle table; contexts retain application ownership.',
          ' * NULL removes the table. Configure before native server construction if',
          ' * callbacks must cover namespace-zero initialization too. */',
          'cpkt_opcua_StatusCode cpkt_opcua_ServerConfig_setGlobalNodeLifecycle(cpkt_opcua_ServerConfig *config, const cpkt_opcua_GlobalNodeLifecycle *lifecycle);']
    h += ['/** Invoke native ClientConfig_copy into an empty owned destination.',
          ' * Ordinary values are independent copies. Plugins, event loop and custom',
          ' * descriptors retain NATIVE SHALLOW ownership: clearing either copy can',
          ' * invalidate the other. This does not create independent backend ownership.',
          ' * Keep source facade metadata alive while a copied backend can use it.',
          ' * Callback tables are copied; application context pointers remain original.',
          ' * Failure preserves both configurations. Serialize with the source handle. */',
          'cpkt_opcua_StatusCode cpkt_opcua_ClientConfig_copy(const cpkt_opcua_ClientConfig *source, cpkt_opcua_ClientConfig *destination);']
    h += ['/** Borrow the actual native nodestore. Configure a custom store before',
          ' * native server construction so it participates in namespace-zero creation.',
          ' * set transfers ownership on Good; a configured store cannot be freed',
          ' * independently. Replacement frees the old backend exactly as native config',
          ' * cleanup does. A live replacement must preserve the native model/runtime',
          ' * invariants; an empty replacement does not recreate namespace zero. NULL',
          ' * removes the store. Native-origin teardown also releases facade metadata. */',
          'cpkt_opcua_StatusCode cpkt_opcua_ServerConfig_getNodestore(cpkt_opcua_ServerConfig *config, cpkt_opcua_Nodestore **store);',
          '/** Configure the actual native record while quiescent. Schema inputs borrow until return; plugin/context ownership follows this family contract. Failures preserve input ownership before native handoff. */\ncpkt_opcua_StatusCode cpkt_opcua_ServerConfig_setNodestore(cpkt_opcua_ServerConfig *config, cpkt_opcua_Nodestore *store);']
    h += ['/** Configure the complete PubSub table before construction/startup.',
          ' * Success consumes each owned security-policy record, copying callbacks.',
          ' * The policy array itself remains caller-owned. Failure preserves sources.',
          ' * Getter borrows a configuration-owned table and callable policy views;',
          ' * never free its array or install its records as owned plugins. Callback',
          ' * slots require the associated native server handle. No backend is cloned. */',
          'cpkt_opcua_StatusCode cpkt_opcua_ServerConfig_setPubSubConfiguration(cpkt_opcua_ServerConfig *config, cpkt_opcua_PubSubConfiguration *pubsub);',
          '/** Inspect native configuration through its C89 view. Owned settings outputs require their matching clear; plugin/handle views belong to configuration. Serialize with reconfiguration and dispatch. */\ncpkt_opcua_StatusCode cpkt_opcua_ServerConfig_getPubSubConfiguration(cpkt_opcua_ServerConfig *config, cpkt_opcua_PubSubConfiguration **pubsub);']
    for kind, cls, prefix in [('Client', 'SecurityPolicy', 'sp'), ('Server', 'SecurityPolicy', 'sp'),
                              ('Server', 'PubSubSecurityPolicy', 'psp')]:
        config = 'cpkt_opcua_' + kind + 'Config'
        mutable = 'cpkt_cfg_' + kind.lower() + '_mutable'
        enter, leave = 'cpkt_cfg_' + kind.lower() + '_enter', 'cpkt_cfg_' + kind.lower() + '_leave'
        selector = ', cpkt_opcua_Boolean auth' if kind == 'Client' else ''
        field = 'pubSubConfig.securityPolicies' if prefix == 'psp' else 'securityPolicies'
        count = field + 'Size'
        if kind == 'Client':
            array = '(auth ? &config->native->authSecurityPolicies : &config->native->securityPolicies)'
            size = '(auth ? &config->native->authSecurityPoliciesSize : &config->native->securityPoliciesSize)'
            view_kind = '(auth ? CPKT_CFG_AUTH_SP : CPKT_CFG_CLIENT_SP)'
        else:
            array, size = '&config->native->' + field, '&config->native->' + count
            view_kind = 'CPKT_CFG_PSP' if prefix == 'psp' else 'CPKT_CFG_SERVER_SP'
        for action in ('add', 'set'):
            plural = cls.replace('Policy', 'Policies') if action == 'set' else cls
            params = f'{config} *config{selector}, cpkt_opcua_{cls} *source' + (', size_t size' if action == 'set' else '')
            call = f'cpkt_cfg_{prefix}_replace({array}, {size}, source, size)' if action == 'set' else f'cpkt_{prefix}_append({array}, {size}, source)'
            impl += [f'cpkt_opcua_StatusCode cpkt_opcua_{kind}Config_{action}{plural}({params}) {{',
                     '  UA_StatusCode status;',
                     '  if(!config || !config->native) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                     f'  if(!{mutable}(config)) return UA_STATUSCODE_BADINVALIDSTATE;',
                     f'  {enter}(config);', f'  status = {call};', f'  {leave}(config);', '  return status;', '}']
        impl += [f'cpkt_opcua_StatusCode cpkt_opcua_{kind}Config_get{cls}( {config} *config{selector}, size_t index, cpkt_opcua_{cls} **policy) {{',
                 '  cpkt_cfg_plugin_view *view;', '  UA_StatusCode status;',
                 '  if(!policy) return UA_STATUSCODE_BADINVALIDARGUMENT;', '  *policy = NULL;',
                 '  if(!config || !config->native) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                 f'  status = cpkt_cfg_view_get({"config, NULL" if kind == "Client" else "NULL, config"}, {view_kind}, index, &view);',
                 f'  if(!status) *policy = &view->record.{prefix};', '  return status;', '}',
                 f'cpkt_opcua_StatusCode cpkt_opcua_{kind}Config_get{cls.replace("Policy", "Policies")}Size(const {config} *config{selector}, size_t *size) {{',
                 '  if(!config || !config->native || !size) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                 f'  *size = *({size});', '  return 0;', '}']
    h += ['/** Invoke native file-based constructor with JSON5 content (not a filename).',
          ' * Files, defaults and native diagnostics retain upstream behavior.',
          ' * Output is NULL on failure; successful handle owns its configuration. */',
          'cpkt_opcua_StatusCode cpkt_opcua_server_newFromFile_typed(const cpkt_opcua_ByteString json_config, cpkt_opcua_server **server);',
          '#if defined(__linux__) || defined(_WIN32)',
          '/** Add the native filestore wrapper around a complete owned policy.',
          ' * Good consumes innerPolicy; failure preserves it. The native policy-array',
          ' * allocation/filesystem effects remain native. Configure before startup. */',
          'cpkt_opcua_StatusCode cpkt_opcua_ServerConfig_addSecurityPolicy_Filestore(cpkt_opcua_ServerConfig *config, cpkt_opcua_SecurityPolicy *innerPolicy, const cpkt_opcua_String storePath);',
          '#endif']
    for kind in ('Client', 'Server'):
        config = 'cpkt_opcua_' + kind + 'Config'
        lower = kind.lower()
        h += ['/** Install the persistent public custom descriptor graph before startup.',
              ' * Arrays and C89 descriptor inputs borrow during conversion. cleanup=true',
              ' * creates native-owned descriptor metadata; cleanup=false borrows source',
              ' * descriptor internals, which must outlive every native configuration use.',
              ' * Input C89 allocations remain caller-owned. Native cleanup flags retain',
              ' * their meaning; facade-owned conversion backing is reclaimed separately.',
              ' * Good replaces the graph; failure preserves the previous graph. NULL',
              ' * removes it. Getter borrows C89 metadata snapshots until replacement or',
              ' * destruction; never delete them. Values using borrowed snapshots must',
              ' * finish first. Each getter observes the current native graph, including',
              ' * native escape changes; earlier snapshots remain configuration-owned. */',
              f'cpkt_opcua_StatusCode cpkt_opcua_{kind}Config_setCustomDataTypes({config} *config, const cpkt_opcua_DataTypeArray *types);',
              '/** Inspect native configuration through its C89 view. Owned settings outputs require their matching clear; plugin/handle views belong to configuration. Serialize with reconfiguration and dispatch. */\n' f'cpkt_opcua_StatusCode cpkt_opcua_{kind}Config_getCustomDataTypes({config} *config, const cpkt_opcua_DataTypeArray **types);']
        impl += [f'cpkt_opcua_StatusCode cpkt_opcua_{kind}Config_setCustomDataTypes({config} *config, const cpkt_opcua_DataTypeArray *types) {{',
                 '  UA_DataTypeArray *staged;', '  cpkt_cfg_type_allocation *allocations;',
                 '  cpkt_cfg_type_snapshot *snapshot = NULL;',
                 '  const cpkt_opcua_DataTypeArray *view;',
                 '  UA_StatusCode status;',
                 '  if(!config || !config->native) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                 f'  if(!cpkt_cfg_{lower}_mutable(config)) return UA_STATUSCODE_BADINVALIDSTATE;',
                 '  status = cpkt_cfg_types_stage(types, &staged, &allocations);',
                 '  if(status) return status;',
                 '  status = cpkt_cfg_types_snapshot(staged, config, &snapshot, &view);',
                 '  if(status) { cpkt_dynamic_native_delete(staged); cpkt_cfg_type_metadata_clear(&allocations, &snapshot); return status; }',
                 f'  cpkt_cfg_{lower}_enter(config);',
                 '  cpkt_dynamic_native_delete(config->native->customDataTypes);',
                 '  cpkt_cfg_type_metadata_clear(&config->custom_allocations, &config->custom_snapshot);',
                 '  config->native->customDataTypes = staged;',
                 '  config->custom_allocations = allocations;',
                 '  config->custom_snapshot = snapshot;', f'  cpkt_cfg_{lower}_leave(config);',
                 '  return 0;', '}',
                 f'cpkt_opcua_StatusCode cpkt_opcua_{kind}Config_getCustomDataTypes({config} *config, const cpkt_opcua_DataTypeArray **types) {{',
                 '  if(!types) return UA_STATUSCODE_BADINVALIDARGUMENT;', '  *types = NULL;',
                 '  if(!config || !config->native) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                 '  return cpkt_cfg_types_snapshot(config->native->customDataTypes, config, &config->custom_snapshot, types);', '}']
    # Validate every handwritten stock credential field/callback/factory.
    access_default = uncomment((headers / 'plugin/accesscontrol_default.h').read_text())
    login_record = re.search(r'typedef\s+struct\s*\{([^{}]*)\}\s*UA_UsernamePasswordLogin\s*;', access_default, re.S)
    if not login_record or ' '.join(login_record[1].split()) != 'UA_String username; UA_ByteString password;':
        raise ValueError('Adapt complete stock credential record')
    login_callback = re.search(r'typedef\s+UA_StatusCode\s*\(\*UA_UsernamePasswordLoginCallback\)\s*\((.*?)\)\s*;', access_default, re.S)
    callback_args = [('const UA_String *', 'userName'), ('const UA_ByteString *', 'password'),
                     ('size_t', 'usernamePasswordLoginSize'), ('const UA_UsernamePasswordLogin *', 'usernamePasswordLogin'),
                     ('void **', 'sessionContext'), ('void *', 'loginContext')]
    if not login_callback or parameters(login_callback[1]) != callback_args:
        raise ValueError('Adapt complete stock login callback')
    factory_args = [('UA_ServerConfig *', 'config'), ('UA_Boolean', 'allowAnonymous'),
                    ('const UA_String *', 'userTokenPolicyUri'), ('size_t', 'usernamePasswordLoginSize'),
                    ('const UA_UsernamePasswordLogin *', 'usernamePasswordLogin')]
    for factory, expected in [('UA_AccessControl_default', factory_args),
                              ('UA_AccessControl_defaultWithLoginCallback', factory_args + [('UA_UsernamePasswordLoginCallback', 'loginCallback'), ('void *', 'loginContext')])]:
        found = re.search(r'\b' + factory + r'\s*\((.*?)\)\s*;', access_default, re.S)
        if not found or parameters(found[1]) != expected:
            raise ValueError('Adapt stock access-control factory: ' + factory)
    h += ['/** Invoke native asynchronous discovery registration/deregistration.',
          ' * Native consumes the client configuration, replaces its event loop, state',
          ' * callback and clientContext, and retains its internal client until teardown.',
          ' * Set *config=NULL on handoff, including errors after native client creation.',
          ' * A non-NULL *config on return remains caller-owned. After handoff do not',
          ' * retain/use/delete the input wrapper: callbacks borrow',
          ' * it and its internal client through that callback only. Never destroy the',
          ' * borrowed client or parent server from a callback. Native notification and',
          ' * logger callbacks are forwarded; their metadata survives native cleanup.',
          ' * A native failure before client creation leaves an empty reusable wrapper;',
          ' * an error after creation can still consume it, exactly as native does.',
          ' * Boundary/preflight failure, including an old owned event-loop free failure,',
          ' * preserves the input. Quiesce that loop and retry. Follow upstream server',
          ' * lifecycle requirements and discovery registration scheduling. */',
          'cpkt_opcua_StatusCode cpkt_opcua_server_registerDiscovery_typed(cpkt_opcua_server *server, cpkt_opcua_ClientConfig **config, const cpkt_opcua_String discoveryServerUrl, const cpkt_opcua_String semaphoreFilePath);',
          '/** Native asynchronous deregistration consumes the owned ClientConfig pointer on handoff, setting it NULL even on later failure. A non-NULL result remains caller-owned; callbacks survive child teardown. */\ncpkt_opcua_StatusCode cpkt_opcua_server_deregisterDiscovery_typed(cpkt_opcua_server *server, cpkt_opcua_ClientConfig **config, const cpkt_opcua_String discoveryServerUrl);']
    h += ['/** Stock username/password record; input strings borrow during installation.',
          ' * Native owns its copied credentials. Callback arguments borrow until return.',
          ' * Native authentication/sessionContext semantics are unchanged. */',
          'typedef struct { cpkt_opcua_String username; cpkt_opcua_ByteString password; } cpkt_opcua_UsernamePasswordLogin;',
          '/** Full native username/password login callback. Credentials borrow until return; sessionContext may be replaced, and loginContext remains caller-owned through plugin teardown. */\ntypedef cpkt_opcua_StatusCode (*cpkt_opcua_UsernamePasswordLoginCallback)(const cpkt_opcua_String *userName, const cpkt_opcua_ByteString *password, size_t usernamePasswordLoginSize, const cpkt_opcua_UsernamePasswordLogin *usernamePasswordLogin, void **sessionContext, void *loginContext);',
          '/** Install the actual native default access-control backend before startup.',
          ' * loginContext remains borrowed until backend replacement/clear/destruction.',
          ' * Callback cannot destroy its configuration or parent server. Native partial',
          ' * effects/statuses are preserved; installation failure can clear old access',
          ' * control as upstream does. Boundary staging failure preserves the backend. */',
          'cpkt_opcua_StatusCode cpkt_opcua_AccessControl_default(cpkt_opcua_ServerConfig *config, cpkt_opcua_Boolean allowAnonymous, const cpkt_opcua_String *userTokenPolicyUri, size_t usernamePasswordLoginSize, const cpkt_opcua_UsernamePasswordLogin *usernamePasswordLogin);',
          '/** Install native default access control with a full C89 username/password callback. Inputs borrow during installation; callback/context outlive the installed plugin. Authentication and authorization remain native. */\ncpkt_opcua_StatusCode cpkt_opcua_AccessControl_defaultWithLoginCallback(cpkt_opcua_ServerConfig *config, cpkt_opcua_Boolean allowAnonymous, const cpkt_opcua_String *userTokenPolicyUri, size_t usernamePasswordLoginSize, const cpkt_opcua_UsernamePasswordLogin *usernamePasswordLogin, cpkt_opcua_UsernamePasswordLoginCallback loginCallback, void *loginContext);']
    server_h, server_impl = emit_server_plugin_views(index, headers, configuration)
    h += server_h
    impl += server_impl
    (output / 'opcua_config_plugins_metadata.inc').write_text('\n'.join(impl) + '\n')
    return h
