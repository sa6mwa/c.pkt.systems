"""Generate complete public policy callback bridges and stock constructors.

Only public policy records are retained. Cryptographic contexts and channel
contexts belong to upstream; no private layouts or algorithms are reproduced.
"""
import re
from plugin_emitter import callbacks, public_body, parameters, translate, uncomment

ALGORITHMS = {
    'asymSignatureAlgorithm': 'SecurityPolicySignatureAlgorithm',
    'asymEncryptionAlgorithm': 'SecurityPolicyEncryptionAlgorithm',
    'symSignatureAlgorithm': 'SecurityPolicySignatureAlgorithm',
    'symEncryptionAlgorithm': 'SecurityPolicyEncryptionAlgorithm',
    'certSignatureAlgorithm': 'SecurityPolicySignatureAlgorithm',
}


def policy_methods(headers, cls):
    text = uncomment((headers / 'plugin/securitypolicy.h').read_text())
    body = public_body(headers / 'plugin/securitypolicy.h', cls, (headers / 'config.h').read_text())
    result = [('', r, n, s) for r, n, s in callbacks(body)]
    if cls == 'SecurityPolicy':
        for field, typename in ALGORITHMS.items():
            found = re.search(r'typedef\s+struct\s*\{([^{}]*)\}\s*UA_' + typename + r'\s*;', text, re.S)
            if not found:
                raise ValueError('Adapt public security algorithm: ' + typename)
            result += [(field + '.', r, n, s) for r, n, s in callbacks(found[1])]
    for path, returns, name, signature in result:
        args = parameters(signature)
        if returns not in ('void', 'size_t', 'UA_StatusCode') or args[0] not in [('const UA_' + cls + ' *', 'policy'), ('UA_' + cls + ' *', 'policy')]:
            raise ValueError('Adapt policy callback signature: ' + path + name)
        if name == 'clear' and (path or len(args) != 1 or returns != 'void'):
            raise ValueError('Adapt native policy clear')
    return body, result


def emit_policies(index, headers):
    public, private = [], []
    for cls, prefix in [('SecurityPolicy', 'sp'), ('PubSubSecurityPolicy', 'psp')]:
        body, methods = policy_methods(headers, cls)
        fields = ['policyUri'] + ([field + '.uri' for field in ALGORITHMS] if cls == 'SecurityPolicy' else [])
        blobs = fields + (['localCertificate'] if cls == 'SecurityPolicy' else [])
        nodes = ['certificateGroupId', 'certificateTypeId'] if cls == 'SecurityPolicy' else []
        scalars = [('nonceLength', '')] + ([('securityLevel', ''), ('policyType', '(UA_SecurityPolicyType)')] if cls == 'SecurityPolicy' else [])
        private += emit_metadata(index, cls, prefix, fields, blobs, nodes, scalars)
        # Declare callback assignment helpers before generated callbacks and stock sync.
        private += [f'static void cpkt_{prefix}_stock_assign(cpkt_opcua_{cls} *, cpkt_{prefix}_stock *);',
                    f'static void cpkt_{prefix}_native_assign(UA_{cls} *, const cpkt_opcua_{cls} *);',
                    f'static void cpkt_{prefix}_root_assign(cpkt_{prefix}_stock *, const cpkt_opcua_{cls} *);']
        stock_assign, native_assign, root_assign = [], [], []
        for path, returns, name, signature in methods:
            if name == 'clear':
                continue
            slot = path + name
            ident = path.replace('.', '_') + name
            stock_name = f'cpkt_{prefix}_stock_{ident}'
            native_name = f'cpkt_{prefix}_custom_{ident}'
            root_name = f'cpkt_{prefix}_root_{ident}'
            private += emit_stock_callback(index, cls, prefix, slot, stock_name, returns, signature)
            private += emit_native_callback(index, cls, prefix, slot, native_name, returns, signature, False)
            private += emit_native_callback(index, cls, prefix, slot, root_name, returns, signature, True)
            stock_assign += [f'  out->{slot} = store->saved.{slot} ? {stock_name} : NULL;']
            native_assign += [f'  native->{slot} = source->{slot} ? {native_name} : NULL;']
            root_assign += [f'  store->native.{slot} = source->{slot} ? {root_name} : NULL;']
        private += [f'static void cpkt_{prefix}_stock_assign(cpkt_opcua_{cls} *out, cpkt_{prefix}_stock *store) {{',
                    *stock_assign, f'  out->clear = cpkt_{prefix}_stock_clear;', '  out->policyContext = store;', '  out->logger = &store->logger.config;', '}',
                    f'static void cpkt_{prefix}_native_assign(UA_{cls} *native, const cpkt_opcua_{cls} *source) {{',
                    *native_assign, f'  native->clear = cpkt_{prefix}_custom_clear;', '}',
                    f'static void cpkt_{prefix}_root_assign(cpkt_{prefix}_stock *store, const cpkt_opcua_{cls} *source) {{',
                    *root_assign, '  store->native.clear = store->saved.clear;', '}']
        private += emit_install(index, cls, prefix)
        if cls == 'SecurityPolicy':
            public += ['/** Append a full security policy before server startup. Success consumes',
                            ' * the record and transfers clear ownership; failure preserves it.',
                            ' * Clear releases complete owned C89 metadata/context. Serialize access. */',
                            'cpkt_opcua_StatusCode cpkt_opcua_server_add_security_policy(cpkt_opcua_server *server, cpkt_opcua_SecurityPolicy *policy);',
                            '/** Append a client policy while disconnected; auth selects user-token',
                            ' * policies. Success consumes policy; failure preserves it. */',
                            'cpkt_opcua_StatusCode cpkt_opcua_client_add_security_policy(cpkt_opcua_client *client, cpkt_opcua_Boolean auth, cpkt_opcua_SecurityPolicy *policy);']
        else:
            public += ['/** Append a PubSub security policy before server startup. Success consumes',
                       ' * policy and transfers its complete clear ownership; failure retains it. */',
                       'cpkt_opcua_StatusCode cpkt_opcua_server_add_pubsub_security_policy(cpkt_opcua_server *server, cpkt_opcua_PubSubSecurityPolicy *policy);']
        public += [f'/** Retry authoritative native metadata conversion after a stock operation',
                   ' * reports conversion failure. Native effects are retained. Refresh before',
                   ' * editing metadata after such a failure. Inputs and operations serialize. */',
                   f'cpkt_opcua_StatusCode cpkt_opcua_{cls}_refresh(cpkt_opcua_{cls} *policy);']
        private += [f'cpkt_opcua_StatusCode cpkt_opcua_{cls}_refresh(cpkt_opcua_{cls} *policy) {{',
                    f'  cpkt_{prefix}_stock *store;',
                    f'  if(!policy || policy->clear != cpkt_{prefix}_stock_clear || !policy->policyContext) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                    f'  store = (cpkt_{prefix}_stock *)policy->policyContext;',
                    f'  return cpkt_{prefix}_refresh(store, policy);', '}']
    h, m = emit_constructors(index, headers)
    public += h
    private += m
    h, m = emit_filestore()
    public += h
    private += m
    h, m = emit_policy_utilities()
    public += h
    private += m
    return public, private


def emit_metadata(index, cls, prefix, fields, blobs, nodes, scalars):
    out = [f'typedef struct {{ UA_{cls} native, saved; cpkt_opcua_{cls} snapshot; const cpkt_opcua_{cls} *active; struct cpkt_opcua_logger logger; unsigned int owned; int pending, clears_metadata; }} cpkt_{prefix}_stock;',
           f'typedef struct {{ cpkt_opcua_{cls} plugin; cpkt_opcua_{cls} *borrowed; struct cpkt_opcua_logger logger; void *dispatch; void (*enter)(void *), (*leave)(void *); }} cpkt_{prefix}_custom;',
           f'static void cpkt_{prefix}_metadata_clear(cpkt_opcua_{cls} *value) {{']
    for f in blobs:
        out += [f'  cpkt_opcua_String_clear(&value->{f});']
    for f in nodes:
        out += [f'  cpkt_opcua_NodeId_clear(&value->{f});']
    out += ['}', f'static UA_StatusCode cpkt_{prefix}_metadata_read(const UA_{cls} *native, cpkt_opcua_{cls} *out) {{',
            f'  cpkt_opcua_{cls} staged;', '  UA_StatusCode status = 0;', '  UA_String view;', '  UA_NodeId node;', '  unsigned int changed = 0;', '  memset(&staged, 0, sizeof(staged));']
    for bit, f in enumerate(blobs):
        out += [f'  view = cpkt_string_view(&out->{f});',
                f'  if(!UA_String_equal(&view, &native->{f}) || (!view.length && (!!view.data != !!native->{f}.data))) {{',
                f'    if(!status) status = cpkt_convert(&native->{f}, &staged.{f}, &cpkt_types[{index["String"]}], 0, 0);',
                f'    changed |= {1 << bit}U;', '  }']
    for bit, f in enumerate(nodes, len(blobs)):
        out += [f'  node = cpkt_nodeid_view(&out->{f});',
                f'  if(!UA_NodeId_equal(&node, &native->{f})) {{',
                f'    if(!status) status = cpkt_convert(&native->{f}, &staged.{f}, &cpkt_types[{index["NodeId"]}], 0, 0);',
                f'    changed |= {1 << bit}U;', '  }']
    out += [f'  if(status) {{ cpkt_{prefix}_metadata_clear(&staged); return status; }}']
    for bit, f in enumerate(blobs):
        out += [f'  if(changed & {1 << bit}U) {{ cpkt_opcua_String_clear(&out->{f}); out->{f} = staged.{f}; }}']
    for bit, f in enumerate(nodes, len(blobs)):
        out += [f'  if(changed & {1 << bit}U) {{ cpkt_opcua_NodeId_clear(&out->{f}); out->{f} = staged.{f}; }}']
    for f, cast in scalars:
        out += [f'  out->{f} = ' + ('(cpkt_opcua_SecurityPolicyType)' if cast else '') + f'native->{f};']
    out += ['  return 0;', '}', f'static void cpkt_{prefix}_metadata_view(const UA_{cls} *native, cpkt_opcua_{cls} *out) {{']
    for f in blobs:
        out += [f'  cpkt_string_take(&out->{f}, native->{f});']
    for f in nodes:
        out += [f'  cpkt_hb_borrow_node(&native->{f}, &out->{f});']
    for f, cast in scalars:
        out += [f'  out->{f} = ' + ('(cpkt_opcua_SecurityPolicyType)' if cast else '') + f'native->{f};']
    out += ['}', f'static UA_StatusCode cpkt_{prefix}_metadata_write(UA_{cls} *native, const cpkt_opcua_{cls} *source, struct cpkt_opcua_logger *logger, unsigned int *owned) {{',
            f'  UA_{cls} staged;', '  UA_StatusCode status = 0;', '  UA_String view;', '  UA_NodeId node;', '  unsigned int changed = 0;',
            '  cpkt_opcua_log_config quiet;', '  memset(&staged, 0, sizeof(staged)); memset(&quiet, 0, sizeof(quiet));',
            '  if(!cpkt_logger_valid(source->logger)) return UA_STATUSCODE_BADINVALIDARGUMENT;']
    for bit, f in enumerate(blobs):
        out += [f'  if(source->{f}.length && !source->{f}.data) {{ status = UA_STATUSCODE_BADINVALIDARGUMENT; goto finish; }}',
                f'  view = cpkt_string_view(&source->{f});', f'  if(!UA_String_equal(&view, &native->{f}) || (!view.length && (!!view.data != !!native->{f}.data))) {{',
                f'    status = UA_String_copy(&view, &staged.{f});', '    if(status) goto finish;', f'    changed |= {1 << bit}U;', '  }']
    for bit, f in enumerate(nodes, len(blobs)):
        out += [f'  node = cpkt_nodeid_view(&source->{f});', f'  if(!UA_NodeId_equal(&node, &native->{f})) {{',
                f'    status = cpkt_convert(&source->{f}, &staged.{f}, &cpkt_types[{index["NodeId"]}], 1, 0);',
                '    if(status) goto finish;', f'    changed |= {1 << bit}U;', '  }']
    for bit, f in enumerate(blobs):
        # Stock URI fields are originally static; local certificates are owned.
        owned_condition = '*owned & ' + str(1 << bit) + 'U' if f in fields else '1'
        out += [f'  if(changed & {1 << bit}U) {{ if({owned_condition}) UA_String_clear(&native->{f}); native->{f} = staged.{f}; staged.{f} = UA_STRING_NULL; }}']
    for bit, f in enumerate(nodes, len(blobs)):
        out += [f'  if(changed & {1 << bit}U) {{ UA_NodeId_clear(&native->{f}); native->{f} = staged.{f}; UA_NodeId_init(&staged.{f}); }}']
    for f, cast in scalars:
        out += [f'  native->{f} = {cast}source->{f};']
    out += ['  *owned |= changed;', '  cpkt_logger_set(logger, &logger->native, source->logger ? source->logger : &quiet);', '  native->logger = &logger->native;', 'finish:']
    for f in blobs:
        out += [f'  UA_String_clear(&staged.{f});']
    for f in nodes:
        out += [f'  UA_NodeId_clear(&staged.{f});']
    out += ['  return status;', '}', f'static UA_StatusCode cpkt_{prefix}_refresh(cpkt_{prefix}_stock *store, cpkt_opcua_{cls} *value) {{',
            f'  UA_StatusCode status = cpkt_{prefix}_metadata_read(&store->native, value);',
            '  store->pending = status != 0;', f'  cpkt_{prefix}_metadata_view(&store->native, &store->snapshot);', '  return status;', '}',
            f'static void cpkt_{prefix}_stock_clear(cpkt_opcua_{cls} *value) {{',
            f'  cpkt_{prefix}_stock *store;', '  if(!value) return;', f'  store = (cpkt_{prefix}_stock *)value->policyContext;',
            '  if(store) {', f'    UA_{cls} saved = store->native;',
            '    if(store->saved.clear) store->saved.clear(&store->native);']
    for bit, f in enumerate(blobs):
        if f in fields:
            out += [f'    if(!store->clears_metadata && (store->owned & {1 << bit}U)) UA_String_clear(&saved.{f});']
        else:
            out += [f'    if(!store->saved.clear) UA_String_clear(&saved.{f});']
    for f in nodes:
        out += [f'    if(!store->clears_metadata) UA_NodeId_clear(&saved.{f});']
    out += ['    UA_free(store);', '  }', f'  cpkt_{prefix}_metadata_clear(value);', '  memset(value, 0, sizeof(*value));', '}',
            f'static void cpkt_{prefix}_custom_clear(UA_{cls} *native) {{',
            f'  cpkt_{prefix}_custom *store = (cpkt_{prefix}_custom *)native->policyContext;',
            '  if(store) { if(store->plugin.clear) store->plugin.clear(&store->plugin); UA_free(store); }']
    for f in blobs:
        out += [f'  UA_String_clear(&native->{f});']
    for f in nodes:
        out += [f'  UA_NodeId_clear(&native->{f});']
    out += ['  memset(native, 0, sizeof(*native));', '}']
    # PubSub has no node fields: avoid an unused local under -Werror.
    if not nodes:
        out = [line for line in out if line != '  UA_NodeId node;']
    return out


def bridge_arguments(index, signature, to_native):
    args = parameters(signature)
    declarations, setup, outputs, cleanup, call = [], [], [], [], []
    for spelling, param in args[1:]:
        if spelling in ('void *', 'const void *', 'void **', 'size_t', 'UA_UInt32'):
            call.append(param)
        elif spelling in ('const UA_ByteString', 'const UA_ByteString *', 'UA_ByteString *', 'const UA_String *'):
            typ = 'UA_ByteString' if to_native else 'cpkt_opcua_ByteString'
            declarations += [f'  {typ} value_{param};']
            pointer = '*' in spelling
            if to_native:
                setup += [f'  memset(&value_{param}, 0, sizeof(value_{param}));']
                if pointer:
                    setup += [f'  if({param}) {{',
                              f'    if(!cpkt_string_valid({param})) status = UA_STATUSCODE_BADINVALIDARGUMENT;',
                              f'    value_{param} = cpkt_string_view({param});', '  }']
                else:
                    setup += [f'  if(!cpkt_string_valid(&{param})) status = UA_STATUSCODE_BADINVALIDARGUMENT;',
                              f'  value_{param} = cpkt_string_view(&{param});']
            else:
                setup += [f'  memset(&value_{param}, 0, sizeof(value_{param}));', f'  {"if(" + param + ") " if pointer else ""}cpkt_string_take(&value_{param}, {"*" if pointer else ""}{param});']
            call += [f'{param} ? &value_{param} : NULL' if pointer else 'value_' + param]
            if spelling == 'UA_ByteString *':
                outputs += [f'  if({param}) ' + (f'cpkt_string_take({param}, value_{param});' if to_native else f'*{param} = cpkt_string_view(&value_{param});')]
        elif spelling == 'const UA_KeyValueMap *':
            typ = 'UA_KeyValueMap' if to_native else 'cpkt_opcua_KeyValueMap'
            declarations += [f'  {typ} value_{param};', f'  void *pairs_{param} = NULL;']
            setup += [f'  memset(&value_{param}, 0, sizeof(value_{param}));',
                      f'  if(!status && {param}) status = cpkt_array({param}->map, {param}->mapSize, &pairs_{param}, &cpkt_types[{index["KeyValuePair"]}], {1 if to_native else 0}, 0);',
                      f'  value_{param}.map = ({"UA_KeyValuePair" if to_native else "cpkt_opcua_KeyValuePair"} *)pairs_{param}; value_{param}.mapSize = pairs_{param} && {param} ? {param}->mapSize : 0;']
            cleanup += [f'  {"UA" if to_native else "cpkt_opcua"}_KeyValueMap_clear(&value_{param});']
            call += [f'{param} ? &value_{param} : NULL']
        else:
            raise ValueError('Adapt public policy callback argument: ' + param + ':' + spelling)
    return declarations, setup, outputs, cleanup, call


def emit_stock_callback(index, cls, prefix, slot, name, returns, signature):
    readonly = parameters(signature)[0][0].startswith('const ')
    declarations, setup, outputs, cleanup, call = bridge_arguments(index, signature, True)
    call.insert(0, '&store->native')
    result_type = translate(returns)
    zero = '' if returns == 'void' else 'UA_STATUSCODE_BADINVALIDARGUMENT' if returns == 'UA_StatusCode' else '0'
    out = [f'static {result_type} {name}({translate(signature)}) {{',
           f'  cpkt_{prefix}_stock *store;', f'  const cpkt_opcua_{cls} *previous;',
           '  UA_StatusCode status;', *([] if returns == 'void' else [f'  {returns} result = 0;']), *declarations,
           f'  if(!policy || !policy->policyContext) return {zero};',
           f'  store = (cpkt_{prefix}_stock *)policy->policyContext;',
           ('  status = store->pending ? UA_STATUSCODE_BADINVALIDSTATE : 0;' if readonly else
            f'  status = store->pending ? cpkt_{prefix}_refresh(store, policy) : 0;'),
           f'  if(!status) status = cpkt_{prefix}_metadata_write(&store->native, policy, &store->logger, &store->owned);',
           *setup, '  previous = store->active;', '  if(!status) {',
           '    store->snapshot = *policy;', f'    cpkt_{prefix}_metadata_view(&store->native, &store->snapshot);',
           '    store->active = policy;', f'    cpkt_{prefix}_root_assign(store, policy);',
           f'    {"" if returns == "void" else "result = "}store->saved.{slot}({", ".join(call)});',
           '    store->active = previous;',
           *outputs,
           (f'    cpkt_{prefix}_metadata_view(&store->native, &store->snapshot);' if readonly else
            f'    status = cpkt_{prefix}_refresh(store, policy);'), '  }', *cleanup]
    if returns == 'void':
        out += ['  if(status) UA_LOG_ERROR(&store->logger.native, UA_LOGCATEGORY_SECURITYPOLICY, "C89 policy conversion failed: %08x", status);']
    else:
        out += ['  return status ? ' + ('status' if returns == 'UA_StatusCode' else '0') + ' : result;']
    out += ['}']
    return out


def emit_native_callback(index, cls, prefix, slot, name, returns, signature, root):
    readonly = parameters(signature)[0][0].startswith('const ')
    declarations, setup, outputs, cleanup, call = bridge_arguments(index, signature, False)
    receiver = 'public_policy'
    call.insert(0, receiver)
    if root:
        opening = [f'  cpkt_{prefix}_stock *store = (cpkt_{prefix}_stock *)((char *)policy - offsetof(cpkt_{prefix}_stock, native));',
                   f'  {"const " if readonly else ""}cpkt_opcua_{cls} *public_policy = store->active ? ' +
                   (f'store->active : &store->snapshot;' if readonly else f'(cpkt_opcua_{cls} *)store->active : &store->snapshot;')]
    else:
        opening = [f'  cpkt_{prefix}_custom *store = (cpkt_{prefix}_custom *)policy->policyContext;',
                   f'  cpkt_opcua_{cls} *public_policy = store->borrowed ? store->borrowed : &store->plugin;']
    out = [f'static {returns} {name}({signature}) {{', *opening, '  UA_StatusCode status = 0;',
           *([] if returns == 'void' else [f'  {returns} result = 0;']), *declarations,
           *([] if root else ['  if(store->enter) store->enter(store->dispatch);']),
           *setup, '  if(!status) {', f'    {"" if returns == "void" else "result = "}public_policy->{slot}({", ".join(call)});',
           *outputs, '  }', *cleanup]
    if not root and not readonly:
        out += ['  if(!status) { unsigned int owned = (unsigned int)-1;',
                f'    status = cpkt_{prefix}_metadata_write(policy, public_policy, &store->logger, &owned);',
                f'    if(!status) cpkt_{prefix}_native_assign(policy, public_policy);', '  }']
    if not root:
        out += ['  if(store->leave) store->leave(store->dispatch);']
    if returns == 'void':
        out += ['  if(status) UA_LOG_ERROR(&store->logger.native, UA_LOGCATEGORY_SECURITYPOLICY, "C89 policy callback conversion failed: %08x", status);']
    else:
        out += ['  return status ? ' + ('status' if returns == 'UA_StatusCode' else '0') + ' : result;']
    out += ['}']
    return out


def emit_install(index, cls, prefix):
    out = [f'static UA_StatusCode cpkt_{prefix}_append(UA_{cls} **array, size_t *size, cpkt_opcua_{cls} *source) {{',
           f'  cpkt_{prefix}_custom *store;', f'  UA_{cls} native, *grown;', '  UA_StatusCode status;', '  unsigned int owned = (unsigned int)-1;',
           f'  if(!source || !source->clear || source->clear == cpkt_cfg_view_{prefix}_clear || !cpkt_logger_valid(source->logger)) return UA_STATUSCODE_BADINVALIDARGUMENT;',
           '  if(*size >= (size_t)-1 / sizeof(native)) return UA_STATUSCODE_BADOUTOFMEMORY;',
           '  memset(&native, 0, sizeof(native));', f'  store = (cpkt_{prefix}_custom *)UA_calloc(1, sizeof(*store));',
           '  if(!store) return UA_STATUSCODE_BADOUTOFMEMORY;',
           f'  status = cpkt_{prefix}_metadata_write(&native, source, &store->logger, &owned);',
           '  if(!status) {', f'    grown = (UA_{cls} *)UA_realloc(*array, (*size + 1) * sizeof(native));',
           '    if(!grown) status = UA_STATUSCODE_BADOUTOFMEMORY;', '    else {',
           '      store->plugin = *source; native.policyContext = store;',
           f'      cpkt_{prefix}_native_assign(&native, source);',
           '      grown[*size] = native; *array = grown; ++*size;',
           '      memset(source, 0, sizeof(*source)); return 0;', '    }', '  }',
           '  native.policyContext = store;', f'  cpkt_{prefix}_custom_clear(&native);', '  return status;', '}']
    if cls == 'SecurityPolicy':
        out += ['cpkt_opcua_StatusCode cpkt_opcua_server_add_security_policy(cpkt_opcua_server *server, cpkt_opcua_SecurityPolicy *policy) {',
                '  UA_ServerConfig *config;', '  if(!server || !server->server || server->started || server->destroying) return UA_STATUSCODE_BADINVALIDSTATE;',
                '  config = UA_Server_getConfig(server->server);', '  return cpkt_sp_append(&config->securityPolicies, &config->securityPoliciesSize, policy);', '}',
                'cpkt_opcua_StatusCode cpkt_opcua_client_add_security_policy(cpkt_opcua_client *client, cpkt_opcua_Boolean auth, cpkt_opcua_SecurityPolicy *policy) {',
                '  UA_ClientConfig *config;', '  UA_SecureChannelState state;', '  if(!client || !client->client) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                '  UA_Client_getState(client->client, &state, NULL, NULL);', '  if(state != UA_SECURECHANNELSTATE_CLOSED) return UA_STATUSCODE_BADINVALIDSTATE;',
                '  config = UA_Client_getConfig(client->client);',
                '  return auth ? cpkt_sp_append(&config->authSecurityPolicies, &config->authSecurityPoliciesSize, policy) : cpkt_sp_append(&config->securityPolicies, &config->securityPoliciesSize, policy);', '}']
    else:
        out += ['cpkt_opcua_StatusCode cpkt_opcua_server_add_pubsub_security_policy(cpkt_opcua_server *server, cpkt_opcua_PubSubSecurityPolicy *policy) {',
                '  UA_ServerConfig *config;', '  if(!server || !server->server || server->started || server->destroying) return UA_STATUSCODE_BADINVALIDSTATE;',
                '  config = UA_Server_getConfig(server->server);', '  return cpkt_psp_append(&config->pubSubConfig.securityPolicies, &config->pubSubConfig.securityPoliciesSize, policy);', '}']
    return out


def emit_constructors(index, headers):
    text = uncomment((headers / 'plugin/securitypolicy_default.h').read_text())
    found = re.findall(r'UA_EXPORT\s+UA_StatusCode\s+(UA_(?:SecurityPolicy|PubSubSecurityPolicy)_\w+)\s*\((.*?)\)\s*;', text, re.S)
    header, metadata = [], []
    for name, signature in found:
        if name.endswith('TPM') or name.endswith('_Filestore'):
            continue  # Filestore inner-policy ownership is implemented separately.
        cls = 'PubSubSecurityPolicy' if 'PubSub' in name else 'SecurityPolicy'
        prefix = 'psp' if cls == 'PubSubSecurityPolicy' else 'sp'
        args = parameters(signature)
        if args[0] != ('UA_' + cls + ' *', 'policy'):
            raise ValueError('Adapt stock policy constructor receiver: ' + name)
        public = translate(signature).replace('const cpkt_opcua_Logger *', 'const cpkt_opcua_log_config *')
        target = name.replace('UA_', 'cpkt_opcua_', 1)
        locals_, setup, call = [], [], ['&store->native']
        for spelling, param in args[1:]:
            if spelling == 'const UA_ByteString':
                locals_ += [f'  UA_ByteString value_{param};']
                setup += [f'  value_{param} = cpkt_string_view(&{param});']
                call += ['value_' + param]
            elif spelling == 'const UA_Logger *':
                call += ['&store->logger.native']
            elif spelling == 'const UA_ApplicationType':
                call += ['(UA_ApplicationType)' + param]
            else:
                raise ValueError('Adapt stock policy constructor argument: ' + name + ':' + spelling)
        header += [f'/** Invoke native {name} on a zero-initialized policy. Existing policies',
                   ' * must be cleared first. Byte inputs borrow during construction; context',
                   ' * and channel lifetimes remain native. Logger config is copied; user data',
                   ' * borrows until clear. Owned C89 metadata must remain valid until clear.',
                   ' * Record copies add no ownership. Clear the complete owned result once.',
                   ' * PubSub AES-CTR stock factories are unavailable with the bundled OpenSSL',
                   ' * backend: they return BadNotSupported and leave the output empty. */',
                   f'cpkt_opcua_StatusCode {target}({public});']
        metadata += [f'cpkt_opcua_StatusCode {target}({public}) {{',
                     f'  cpkt_{prefix}_stock *store;', '  cpkt_opcua_log_config quiet;', '  UA_StatusCode status;', *locals_,
                     '  if(!policy || policy->policyContext || policy->clear || !cpkt_logger_valid(logger)) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                     *[f'  if({param}.length && !{param}.data) return UA_STATUSCODE_BADINVALIDARGUMENT;' for spelling, param in args[1:] if spelling == 'const UA_ByteString'],
                     f'  store = (cpkt_{prefix}_stock *)UA_calloc(1, sizeof(*store));', '  if(!store) return UA_STATUSCODE_BADOUTOFMEMORY;',
                     '  memset(&quiet, 0, sizeof(quiet));', '  cpkt_logger_set(&store->logger, &store->logger.native, logger ? logger : &quiet);',
                     *setup, f'  status = {name}({", ".join(call)});',
                     '  store->saved = store->native;',
                     f'  if(!status) status = cpkt_{prefix}_metadata_read(&store->native, policy);',
                     f'  if(!status) {{ cpkt_{prefix}_stock_assign(policy, store); store->snapshot = *policy;',
                     f'    cpkt_{prefix}_metadata_view(&store->native, &store->snapshot); cpkt_{prefix}_root_assign(store, policy); }}',
                     f'  else {{ policy->policyContext = store; cpkt_{prefix}_stock_clear(policy); }}', '  return status;', '}']
    return header, metadata


def emit_policy_utilities():
    header = ['/** Native policy URI classification; the record and its URI borrow during',
              ' * this allocation-free call. NULL retains native classification defaults. */',
              'cpkt_opcua_Boolean cpkt_opcua_SecurityPolicy_isEnhancedSecurity(const cpkt_opcua_SecurityPolicy *policy);',
              '/** Native legacy-sequence-number classification; NULL follows native rules. */',
              'cpkt_opcua_Boolean cpkt_opcua_SecurityPolicy_useLegacySequenceNumbers(const cpkt_opcua_SecurityPolicy *policy);',
              '/** Hash a certificate with the bundled native backend and policy URI. Input',
              ' * bytes borrow through return; hash starts empty and owns native output. */',
              'cpkt_opcua_StatusCode cpkt_opcua_SecurityPolicy_hashCertificate(const cpkt_opcua_SecurityPolicy *policy, const cpkt_opcua_ByteString *certificate, cpkt_opcua_ByteString *hash);']
    metadata = []
    for name in ('isEnhancedSecurity', 'useLegacySequenceNumbers'):
        metadata += [f'cpkt_opcua_Boolean cpkt_opcua_SecurityPolicy_{name}(const cpkt_opcua_SecurityPolicy *policy) {{',
                     '  UA_SecurityPolicy native;', '  memset(&native, 0, sizeof(native));',
                     '  if(policy) native.policyUri = cpkt_string_view(&policy->policyUri);',
                     f'  return UA_SecurityPolicy_{name}(policy ? &native : NULL);', '}']
    metadata += ['cpkt_opcua_StatusCode cpkt_opcua_SecurityPolicy_hashCertificate(const cpkt_opcua_SecurityPolicy *policy, const cpkt_opcua_ByteString *certificate, cpkt_opcua_ByteString *hash) {',
                 '  UA_SecurityPolicy native;', '  UA_ByteString input, output = UA_BYTESTRING_NULL;', '  UA_StatusCode status;',
                 '  if(!policy || !certificate || !hash || (certificate->length && !certificate->data)) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                 '  memset(&native, 0, sizeof(native)); native.policyUri = cpkt_string_view(&policy->policyUri);', '  input = cpkt_string_view(certificate);',
                 '  status = UA_SecurityPolicy_hashCertificate(&native, &input, &output);', '  if(!status || output.data) cpkt_string_take(hash, output);', '  return status;', '}']
    return header, metadata


def emit_filestore():
    guard = '#if defined(__linux__) || defined(_WIN32)'
    header = [guard,
              '/** Wrap the complete native policy with native certificate/key file storage.',
              ' * policy must be zero-initialized and distinct from innerPolicy. Success',
              ' * consumes innerPolicy; failure preserves it. Owned metadata is independent',
              ' * of the inner record. Clear the result once after all channels have ended.',
              ' * storePath borrows until return. Native filesystem effects remain native. */',
              'cpkt_opcua_StatusCode cpkt_opcua_SecurityPolicy_Filestore(cpkt_opcua_SecurityPolicy *policy, cpkt_opcua_SecurityPolicy *innerPolicy, const cpkt_opcua_String storePath);',
              '#endif']
    metadata = [guard,
                'cpkt_opcua_StatusCode cpkt_opcua_SecurityPolicy_Filestore(cpkt_opcua_SecurityPolicy *policy, cpkt_opcua_SecurityPolicy *innerPolicy, const cpkt_opcua_String storePath) {',
                '  cpkt_sp_stock *store;',
                '  cpkt_sp_custom *inner_store;',
                '  cpkt_opcua_SecurityPolicy staged;',
                '  UA_SecurityPolicy *inner = NULL;',
                '  size_t count = 0;',
                '  UA_StatusCode status;',
                '  UA_String path;',
                '  cpkt_opcua_log_config quiet;',
                '  if(!policy || !innerPolicy || policy == innerPolicy || policy->policyContext || policy->clear || (storePath.length && !storePath.data) || !cpkt_logger_valid(innerPolicy->logger)) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                '  store = (cpkt_sp_stock *)UA_calloc(1, sizeof(*store));',
                '  if(!store) return UA_STATUSCODE_BADOUTOFMEMORY;',
                '  memset(&quiet, 0, sizeof(quiet));',
                '  cpkt_logger_set(&store->logger, &store->logger.native, innerPolicy->logger ? innerPolicy->logger : &quiet);',
                '  staged = *innerPolicy;',
                '  status = cpkt_sp_append(&inner, &count, &staged);',
                '  if(status) { UA_free(store); return status; }',
                '  inner_store = (cpkt_sp_custom *)inner->policyContext;',
                '  path = cpkt_string_view(&storePath);',
                '  status = UA_SecurityPolicy_Filestore(&store->native, inner, path);',
                '  if(status) {',
                '    memset(&inner_store->plugin, 0, sizeof(inner_store->plugin));',
                '    cpkt_sp_custom_clear(inner); UA_free(inner); UA_free(store);',
                '    return status;',
                '  }',
                '  store->saved = store->native;',
                '  store->owned = (unsigned int)-1; store->clears_metadata = 1;',
                '  status = cpkt_sp_metadata_read(&store->native, policy);',
                '  if(status) {',
                '    memset(&inner_store->plugin, 0, sizeof(inner_store->plugin));',
                '    policy->policyContext = store; cpkt_sp_stock_clear(policy);',
                '    return status;',
                '  }',
                '  memset(innerPolicy, 0, sizeof(*innerPolicy));',
                '  cpkt_sp_stock_assign(policy, store); store->snapshot = *policy;',
                '  cpkt_sp_metadata_view(&store->native, &store->snapshot);',
                '  cpkt_sp_root_assign(store, policy);',
                '  return 0;', '}', '#endif']
    return header, metadata
