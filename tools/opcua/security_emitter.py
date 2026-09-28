"""Public certificate-group callbacks and native factories.

The native backend owns certificate stores. Only public records cross the C89
boundary; no crypto/store internals or replacement verification engines exist.
"""
import re
from plugin_emitter import public_body, validate_fields, callbacks, parameters, translate

GROUP_METHODS = {
    'getTrustList': [('UA_TrustListDataType *', 'trustList')],
    'setTrustList': [('const UA_TrustListDataType *', 'trustList')],
    'addToTrustList': [('const UA_TrustListDataType *', 'trustList')],
    'removeFromTrustList': [('const UA_TrustListDataType *', 'trustList')],
    'getRejectedList': [('UA_ByteString **', 'rejectedList'), ('size_t *', 'rejectedListSize')],
    'getCertificateCrls': [('const UA_ByteString *', 'certificate'), ('const UA_Boolean', 'isTrusted'), ('UA_ByteString **', 'crls'), ('size_t *', 'crlsSize')],
    'verifyCertificate': [('const UA_ByteString *', 'certificate')],
    'clear': [],
}


def emit_security(index, headers):
    path = headers / 'plugin/certificategroup.h'
    body = public_body(path, 'CertificateGroup', (headers / 'config.h').read_text())
    validate_fields(body, ['UA_NodeId certificateGroupId', 'void *context', 'const UA_Logger *logging'])
    methods = callbacks(body)
    if {n for _, n, _ in methods} != set(GROUP_METHODS):
        raise ValueError('Adapt complete public certificate-group callbacks')
    for result, name, signature in methods:
        args = parameters(signature)
        normalized = [(' '.join(t.replace('**', ' **').split()), n) for t, n in args]
        if normalized != [('UA_CertificateGroup *', 'certGroup')] + GROUP_METHODS[name] or result != ('void' if name == 'clear' else 'UA_StatusCode'):
            raise ValueError('Adapt certificate-group callback: ' + name + str(normalized))
    public_body_ = translate(body).replace('const cpkt_opcua_Logger *', 'const cpkt_opcua_log_config *')
    header = [re.match(r'/\*.*?\*/', path.read_text(), re.S)[0],
              '/** Forward declaration of the full C89 certificate-group callback record. */\ntypedef struct cpkt_opcua_CertificateGroup cpkt_opcua_CertificateGroup;',
              '/** Complete public certificate-group record. Owned metadata/context moves',
              ' * on successful installation; record copies do not add ownership. Inputs',
              ' * borrow until callback return. Output trust lists and certificate arrays',
              ' * start empty and transfer independent owned records, even on failure.',
              ' * clear releases owned certificateGroupId/context and resets the record.',
              ' * Stock contexts are opaque; invoke clear exactly once after all borrowers.',
              ' * Logging configuration is copied; user data borrows until clear. */',
              'struct cpkt_opcua_CertificateGroup {' + public_body_ + '};',
              '/** Install a complete native certificate-group plugin before startup.',
              ' * Success consumes and empties group, transferring clear ownership.',
              ' * Failure preserves group. session selects session PKI instead of channel',
              ' * PKI. Clear releases the entire moved C89 record and its context. */',
              'cpkt_opcua_StatusCode cpkt_opcua_server_set_certificate_group(cpkt_opcua_server *server, cpkt_opcua_Boolean session, cpkt_opcua_CertificateGroup *group);',
              '/** Install client certificate verification while disconnected. Success',
              ' * consumes group; failure retains it. Native verification remains native. */',
              'cpkt_opcua_StatusCode cpkt_opcua_client_set_certificate_group(cpkt_opcua_client *client, cpkt_opcua_CertificateGroup *group);']
    metadata = []
    stock_assignments, custom_assignments = [], []
    for result, name, signature in methods:
        if name == 'clear':
            continue
        public_signature = translate(signature)
        stock = 'cpkt_cg_stock_' + name
        custom = 'cpkt_cg_custom_' + name
        # Stock native implementation -> complete C89 callable record.
        metadata += [f'static cpkt_opcua_StatusCode {stock}({public_signature}) {{',
                     '  cpkt_cg_stock *store;', '  UA_StatusCode status, native_status = 0;']
        if name in ('getTrustList', 'setTrustList', 'addToTrustList', 'removeFromTrustList'):
            metadata += ['  UA_TrustListDataType native;', '  cpkt_opcua_TrustListDataType staged;',
                         '  memset(&native, 0, sizeof(native)); memset(&staged, 0, sizeof(staged));',
                         '  if(!certGroup || !certGroup->context || !trustList) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                         '  store = (cpkt_cg_stock *)certGroup->context;',
                         '  status = cpkt_cg_stock_sync(store, certGroup);']
            if name == 'getTrustList':
                metadata += [f'  if(!status) {{ native_status = store->native.{name}(&store->native, &native);',
                             f'    status = cpkt_convert(&native, &staged, &cpkt_types[{index["TrustListDataType"]}], 0, 0);',
                             '    if(!status) { *trustList = staged; memset(&staged, 0, sizeof(staged)); } }']
            else:
                metadata += [f'  if(!status) status = cpkt_convert(trustList, &native, &cpkt_types[{index["TrustListDataType"]}], 1, 0);',
                             f'  if(!status) native_status = store->native.{name}(&store->native, &native);']
            metadata += ['  UA_TrustListDataType_clear(&native); cpkt_opcua_TrustListDataType_clear(&staged);']
        else:
            array = name in ('getRejectedList', 'getCertificateCrls')
            if name != 'getRejectedList':
                metadata += ['  UA_ByteString input;']
            if array:
                out, count = ('rejectedList', 'rejectedListSize') if name == 'getRejectedList' else ('crls', 'crlsSize')
                metadata += ['  UA_ByteString *native = NULL;', '  void *staged = NULL;', '  size_t size = 0;',
                             f'  if(!certGroup || !certGroup->context || !{out} || !{count}' + (' || !certificate' if name != 'getRejectedList' else '') + ') return UA_STATUSCODE_BADINVALIDARGUMENT;',
                             '  store = (cpkt_cg_stock *)certGroup->context;', '  status = cpkt_cg_stock_sync(store, certGroup);']
                if name != 'getRejectedList':
                    metadata += ['  input = cpkt_string_view(certificate);']
                call = '&store->native, ' + ('&input, isTrusted, ' if name == 'getCertificateCrls' else '') + '&native, &size'
                metadata += [f'  if(!status) {{ native_status = store->native.{name}({call});',
                             f'    status = cpkt_array(native, size, &staged, &cpkt_types[{index["ByteString"]}], 0, 0);',
                             f'    if(!status) {{ *{out} = (cpkt_opcua_ByteString *)staged; *{count} = size; staged = NULL; }} }}',
                             f'  if(staged) cpkt_opcua_array_delete(staged, size, &cpkt_types[{index["ByteString"]}]);',
                             f'  if(native) UA_Array_delete(native, size, cpkt_types[{index["ByteString"]}].native);']
            else:
                metadata += ['  if(!certGroup || !certGroup->context || !certificate) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                             '  store = (cpkt_cg_stock *)certGroup->context;', '  status = cpkt_cg_stock_sync(store, certGroup);',
                             '  input = cpkt_string_view(certificate);',
                             f'  if(!status) native_status = store->native.{name}(&store->native, &input);']
        metadata += ['  return status ? status : native_status;', '}']
        stock_assignments += [f'  group->{name} = store->native.{name} ? {stock} : NULL;']
        # Complete C89 callback implementation -> native callback invocation.
        metadata += [f'static UA_StatusCode {custom}({signature}) {{',
                     '  cpkt_cg_custom *store = (cpkt_cg_custom *)certGroup->context;',
                     '  UA_StatusCode status, native_status = 0;']
        if name in ('getTrustList', 'setTrustList', 'addToTrustList', 'removeFromTrustList'):
            metadata += ['  cpkt_opcua_TrustListDataType value;', '  UA_TrustListDataType staged;',
                         '  memset(&value, 0, sizeof(value)); memset(&staged, 0, sizeof(staged));', '  status = 0;']
            if name != 'getTrustList':
                metadata += [f'  status = cpkt_convert(trustList, &value, &cpkt_types[{index["TrustListDataType"]}], 0, 0);']
            metadata += [f'  if(!status) {{ native_status = store->plugin.{name}(&store->plugin, &value);']
            if name == 'getTrustList':
                metadata += [f'    status = cpkt_convert(&value, &staged, &cpkt_types[{index["TrustListDataType"]}], 1, 0);',
                             '    if(!status) { *trustList = staged; memset(&staged, 0, sizeof(staged)); }']
            metadata += ['  }', '  cpkt_opcua_TrustListDataType_clear(&value); UA_TrustListDataType_clear(&staged);']
        else:
            array = name in ('getRejectedList', 'getCertificateCrls')
            if name != 'getRejectedList':
                metadata += ['  cpkt_opcua_ByteString input;']
            if array:
                out, count = ('rejectedList', 'rejectedListSize') if name == 'getRejectedList' else ('crls', 'crlsSize')
                metadata += ['  cpkt_opcua_ByteString *value = NULL;', '  void *staged = NULL;', '  size_t size = 0;',
                             *(['  cpkt_string_take(&input, *certificate);'] if name != 'getRejectedList' else []),
                             f'  native_status = store->plugin.{name}(&store->plugin, ' + ('&input, isTrusted, ' if name == 'getCertificateCrls' else '') + '&value, &size);',
                             f'  status = cpkt_array(value, size, &staged, &cpkt_types[{index["ByteString"]}], 1, 0);',
                             f'  if(!status) {{ *{out} = (UA_ByteString *)staged; *{count} = size; staged = NULL; }}',
                             f'  if(staged) UA_Array_delete(staged, size, cpkt_types[{index["ByteString"]}].native);',
                             f'  cpkt_opcua_array_delete(value, size, &cpkt_types[{index["ByteString"]}]);']
            else:
                metadata += ['  cpkt_string_take(&input, *certificate);', f'  native_status = store->plugin.{name}(&store->plugin, &input);', '  status = 0;']
        metadata += ['  if(!status) status = cpkt_cg_sync_metadata(certGroup, &store->plugin, &store->logger);',
                     '  cpkt_cg_custom_assign(certGroup, &store->plugin);',
                     '  return status ? status : native_status;', '}']
        custom_assignments += [f'  native->{name} = group->{name} ? {custom} : NULL;']
    metadata += ['static void cpkt_cg_stock_assign(cpkt_opcua_CertificateGroup *group, cpkt_cg_stock *store) {',
                 *stock_assignments, '  group->clear = cpkt_cg_stock_clear;', '  group->context = store;',
                 '  group->logging = &store->logger.config;', '}',
                 'static void cpkt_cg_custom_assign(UA_CertificateGroup *native, const cpkt_opcua_CertificateGroup *group) {',
                 *custom_assignments, '  native->clear = cpkt_cg_custom_clear;', '}']
    metadata += emit_factories(index, headers, header)
    return header, metadata


def emit_factories(index, headers, public):
    text = (headers / 'plugin/certificategroup_default.h').read_text()
    found = re.findall(r'((?:(?:UA_EXPORT|void|UA_StatusCode)\s+)+)(UA_CertificateGroup_\w+)\s*\((.*?)\)\s*;', text, re.S)
    if {n for _, n, _ in found} != {'UA_CertificateGroup_AcceptAll', 'UA_CertificateGroup_Memorystore', 'UA_CertificateGroup_Filestore'}:
        raise ValueError('Adapt certificate-group factories')
    result = []
    for _, name, signature in found:
        target = name.replace('UA_', 'cpkt_opcua_', 1)
        public_signature = translate(signature).replace('const cpkt_opcua_Logger *', 'const cpkt_opcua_log_config *')
        guard = name.endswith('_Filestore')
        if guard:
            public.append('#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32) || defined(__OpenBSD__)')
            result.append('#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32) || defined(__OpenBSD__)')
        public += [f'/** Invoke native {name}; initialize the group to zero before first use.',
                   ' * Inputs borrow during construction. Logger config is copied and its',
                   ' * user data borrows until clear. Native stores own their normal backend.',
                   ' * Existing group clear runs when native construction begins. Native',
                   ' * construction failure leaves group empty. Clear each owned result once.',
                   ' * Public NodeId metadata is independently owned C89 storage. */',
                   f'cpkt_opcua_StatusCode {target}({public_signature});']
        accept = name.endswith('_AcceptAll')
        memory = name.endswith('_Memorystore')
        result += [f'cpkt_opcua_StatusCode {target}({public_signature}) {{',
                   '  cpkt_cg_stock *store;', '  UA_NodeId id;', '  UA_StatusCode status;',
                   *([] if accept else ['  UA_KeyValueMap map;', '  void *pairs = NULL;', '  cpkt_opcua_log_config quiet;']),
                   *(['  UA_TrustListDataType trust;'] if memory else []),
                   *(['  UA_String path;'] if guard else []),
                   '  if(!certGroup' + ('' if accept else ' || !certificateGroupId || !cpkt_logger_valid(logger)') + ') return UA_STATUSCODE_BADINVALIDARGUMENT;',
                   '  UA_NodeId_init(&id);',
                   *([] if accept else ['  memset(&map, 0, sizeof(map)); memset(&quiet, 0, sizeof(quiet));']),
                   *(['  UA_TrustListDataType_init(&trust);'] if memory else []),
                   f'  status = cpkt_convert({"&certGroup->certificateGroupId" if accept else "certificateGroupId"}, &id, &cpkt_types[{index["NodeId"]}], 1, 0);',
                   *([f'  if(!status && trustList) status = cpkt_convert(trustList, &trust, &cpkt_types[{index["TrustListDataType"]}], 1, 0);'] if memory else []),
                   *([] if accept else [f'  if(!status && params) status = cpkt_array(params->map, params->mapSize, &pairs, &cpkt_types[{index["KeyValuePair"]}], 1, 0);',
                                          '  map.map = (UA_KeyValuePair *)pairs; map.mapSize = pairs && params ? params->mapSize : 0;']),
                   '  store = !status ? (cpkt_cg_stock *)UA_calloc(1, sizeof(*store)) : NULL;',
                   '  if(!status && !store) status = UA_STATUSCODE_BADOUTOFMEMORY;',
                   '  if(!status) {',
                   *(['    if(certGroup->logging) cpkt_logger_set(&store->logger, &store->logger.native, certGroup->logging);'] if accept else ['    cpkt_logger_set(&store->logger, &store->logger.native, logger ? logger : &quiet);']),
                   '    if(certGroup->clear) certGroup->clear(certGroup); else cpkt_opcua_NodeId_clear(&certGroup->certificateGroupId);', '    memset(certGroup, 0, sizeof(*certGroup));']
        if accept:
            result += ['    store->native.certificateGroupId = id;', '    UA_CertificateGroup_AcceptAll(&store->native);',
                       '    store->native.logging = &store->logger.native;',
                       # AcceptAll copies its incoming NodeId; the separate id remains owned here.
                       '    UA_NodeId_init(&id);']
        elif memory:
            result += [f'    status = {name}(&store->native, &id, trustList ? &trust : NULL, &store->logger.native, params ? &map : NULL);']
        else:
            result += ['    path = cpkt_string_view(&storePath);',
                       f'    status = {name}(&store->native, &id, path, &store->logger.native, params ? &map : NULL);']
        result += [f'    if(!status) status = cpkt_convert(&store->native.certificateGroupId, &certGroup->certificateGroupId, &cpkt_types[{index["NodeId"]}], 0, 0);',
                   '    if(!status) cpkt_cg_stock_assign(certGroup, store);',
                   '    else { if(store->native.clear) store->native.clear(&store->native); UA_NodeId_clear(&store->native.certificateGroupId); cpkt_opcua_NodeId_clear(&certGroup->certificateGroupId); UA_free(store); }',
                   '  }', '  UA_NodeId_clear(&id);',
                   *([] if accept else ['  UA_KeyValueMap_clear(&map);']),
                   *(['  UA_TrustListDataType_clear(&trust);'] if memory else []),
                   '  return status;', '}']
        if guard:
            public.append('#endif')
            result.append('#endif')
    return result
