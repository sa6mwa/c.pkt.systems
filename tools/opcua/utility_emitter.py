"""Public common records and schema utilities derived from upstream headers.

Ownership policy identifies native borrowed bytes and owned schema outputs.
Parsing and printing remain upstream operations; no replacement parser exists.
"""
import re
from plugin_emitter import parameters, translate, uncomment
from core_client_emitter import enum_declaration

PARSERS = {'AttributeOperand', 'ReadValueId', 'RelativePath', 'SimpleAttributeOperand'}


def emit_utilities(index, headers):
    common = (headers / 'common.h').read_text()
    util = (headers / 'util.h').read_text()
    header, metadata = [], []
    pubsub = uncomment((headers / 'server_pubsub.h').read_text())
    result = re.search(r'typedef\s+struct\s*\{([^{}]*)\}\s*UA_DataSetFieldResult\s*;', pubsub, re.S)
    if not result or ' '.join(result[1].split()) != 'UA_StatusCode result; UA_ConfigurationVersionDataType configurationVersion;':
        raise ValueError('Adapt public DataSetFieldResult fields')
    header += ['/** Complete native dataset-field result; inspect result separately. */',
               'typedef struct {' + translate(result[1]) + '} cpkt_opcua_DataSetFieldResult;']
    metadata += ['static UA_StatusCode cpkt_result_DataSetFieldResult(const UA_DataSetFieldResult *source, cpkt_opcua_DataSetFieldResult *out) {',
                 '  out->result = source->result;',
                 f'  return cpkt_convert(&source->configurationVersion, &out->configurationVersion, &cpkt_types[{index["ConfigurationVersionDataType"]}], 0, 0);', '}']
    for typename in ('AttributeId', 'RuleHandling', 'ApplicationNotificationType',
                     'ShutdownReason', 'LifecycleState'):
        declaration, constants = enum_declaration(common, typename)
        header += ['/** Complete native public enum, generated from common.h. */', declaration]
        metadata += [f'typedef char utility_{name}[((int){name} == (int){name.replace("UA_", "CPKT_OPCUA_")}) ? 1 : -1];' for name in constants]
    declaration, constants = enum_declaration(pubsub, 'PubSubComponentType')
    header += ['/** Complete native PubSub component enum. */', declaration]
    metadata += [f'typedef char utility_{name}[((int){name} == (int){name.replace("UA_", "CPKT_OPCUA_")}) ? 1 : -1];' for name in constants]
    statistic_fields = {}
    for typename, text in (('UInt32Range', util), ('DurationRange', util),
                           ('SecureChannelStatistics', common), ('SessionStatistics', common)):
        match = re.search(r'typedef\s+struct\s*\{([^{}]*)\}\s*UA_' + typename + r'\s*;', uncomment(text), re.S)
        if not match:
            raise ValueError('Adapt public utility record: ' + typename)
        fields = []
        for field in match[1].split(';'):
            if not field.strip():
                continue
            parsed = re.fullmatch(r'\s*(size_t|UA_UInt32|UA_Duration)\s+(\w+)\s*', field)
            if not parsed:
                raise ValueError('Adapt utility record field: ' + typename + '.' + field)
            fields.append(parsed.groups())
        statistic_fields[typename] = fields
        header += ['/** Native public scalar fields; initialize the complete record to zero. */',
                   'typedef struct {' + translate(match[1]) + '} cpkt_opcua_' + typename + ';']
    server = uncomment((headers / 'server.h').read_text())
    match = re.search(r'typedef\s+struct\s*\{([^{}]*)\}\s*UA_ServerStatistics\s*;', server, re.S)
    if not match or ' '.join(match[1].split()) != 'UA_SecureChannelStatistics scs; UA_SessionStatistics ss;':
        raise ValueError('Adapt complete native server statistics')
    header += ['/** Complete native server counters; no pointers or owned allocations. */',
               'typedef struct {' + translate(match[1]) + '} cpkt_opcua_ServerStatistics;']
    metadata += ['static UA_StatusCode cpkt_result_ServerStatistics(const UA_ServerStatistics *native, cpkt_opcua_ServerStatistics *out) {']
    for typename, member in [('SecureChannelStatistics', 'scs'), ('SessionStatistics', 'ss')]:
        metadata += [f'  out->{member}.{field} = native->{member}.{field};' for spelling, field in statistic_fields[typename]]
    metadata += ['  return 0;', '}']
    header += ['/** Borrow the native static attribute name; never free the result. */',
               'const char *cpkt_opcua_AttributeId_name(cpkt_opcua_AttributeId attrId);']
    metadata += ['const char *cpkt_opcua_AttributeId_name(cpkt_opcua_AttributeId attrId) {',
                 '  return UA_AttributeId_name((UA_AttributeId)attrId);', '}']
    text = uncomment(util)
    names = {f'UA_{t}_{a}' for t in PARSERS for a in ('parse', 'print')}
    names.add('UA_RelativePath_parseWithServer')
    declarations = re.findall(r'((?:(?:UA_StatusCode|UA_EXPORT)\s+)+)(UA_\w+)\s*\((.*?)\)\s*;', text, re.S)
    found = {name for _, name, _ in declarations if name in names}
    if found != names:
        raise ValueError('Adapt public parse/print declarations: ' + str(names - found))
    for modifiers, name, signature in declarations:
        if name not in names:
            continue
        if set(modifiers.split()) != {'UA_EXPORT', 'UA_StatusCode'}:
            raise ValueError('Adapt public utility return: ' + name)
        kind, action = name[3:].rsplit('_', 1)
        args = parameters(signature)
        server = action == 'parseWithServer'
        if server:
            if args[0] != ('UA_Server *', 'server'):
                raise ValueError('Adapt server-relative parser')
            args = args[1:]
        if kind not in index or len(args) != 2:
            raise ValueError('Adapt utility schema: ' + name)
        typename = args[0][0]
        pointer = args[0][1]
        string = args[1][1]
        is_print = action == 'print'
        expected = [('const UA_' + kind + ' *', pointer), ('UA_String *', string)] if is_print else [('UA_' + kind + ' *', pointer), ('const UA_String', string)]
        if args != expected:
            raise ValueError('Adapt public utility parameters: ' + name)
        target = name.replace('UA_', 'cpkt_opcua_', 1)
        public = translate(signature)
        header += [f'/** Call native {name}; inputs borrow until return.',
                   ' * Parse outputs start empty and own converted native results, including',
                   ' * native partial results on failure. Clear them after use. Conversion',
                   ' * failure leaves the caller output unchanged. Print output follows native',
                   ' * allocation/preallocated-buffer rules and retains caller buffer identity.',
                   ' * No alternate parsing or formatting mechanism is introduced. */',
                   f'cpkt_opcua_StatusCode {target}({public});']
        metadata += [f'cpkt_opcua_StatusCode {target}({public}) {{',
                     f'  UA_{kind} value;', '  UA_String text;', '  UA_StatusCode status, conversion;',
                     *([] if is_print else [f'  cpkt_opcua_{kind} staged;']),
                     f'  if(!{pointer}' + (f' || !{string}' if is_print else f' || ({string}.length && !{string}.data)') + (' || !server || !server->server' if server else '') + ') return UA_STATUSCODE_BADINVALIDARGUMENT;',
                     '  memset(&value, 0, sizeof(value));']
        if is_print:
            metadata += [f'  text = cpkt_string_view({string});',
                         f'  status = cpkt_convert({pointer}, &value, &cpkt_types[{index[kind]}], 1, 0);',
                         f'  if(!status) {{ status = {name}(&value, &text); cpkt_string_take({string}, text); }}',
                         '  conversion = 0;']
        else:
            metadata += [f'  memset(&staged, 0, sizeof(staged));',
                         f'  text = cpkt_string_view(&{string});',
                         f'  status = {name}(' + ('server->server, ' if server else '') + '&value, text);',
                         f'  conversion = cpkt_convert(&value, &staged, &cpkt_types[{index[kind]}], 0, 0);',
                         f'  if(!conversion) *{pointer} = staged;',
                         f'  else cpkt_opcua_{kind}_clear(&staged);']
        metadata += [f'  UA_{kind}_clear(&value);', '  return conversion ? conversion : status;', '}']
    return header, metadata


def emit_value_operations(index, headers):
    native_types = uncomment((headers / 'types.h').read_text())
    header, metadata = [], []
    metadata += ['static UA_StatusCode cpkt_utility_range(cpkt_opcua_NumericRange source, UA_NumericRange *out) {',
                 '  size_t i;', '  memset(out, 0, sizeof(*out));',
                 '  if(!source.dimensionsSize) return 0;',
                 '  if(!source.dimensions) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                 '  if(source.dimensionsSize > (size_t)-1 / sizeof(*out->dimensions)) return UA_STATUSCODE_BADOUTOFMEMORY;',
                 '  out->dimensions = (UA_NumericRangeDimension *)UA_calloc(source.dimensionsSize, sizeof(*out->dimensions));',
                 '  if(!out->dimensions) return UA_STATUSCODE_BADOUTOFMEMORY;',
                 '  out->dimensionsSize = source.dimensionsSize;',
                 '  for(i = 0; i < source.dimensionsSize; ++i) { out->dimensions[i].min = source.dimensions[i].min; out->dimensions[i].max = source.dimensions[i].max; }',
                 '  return 0;', '}']
    for typename in ('Variant', 'DataValue'):
        name = 'UA_' + typename + '_copyRange'
        match = re.search(name + r'\s*\((.*?)\)\s*;', native_types, re.S)
        expected = [('const UA_' + typename + ' *', 'src'), ('UA_' + typename + ' *', 'dst'), ('const UA_NumericRange', 'range')]
        if not match or parameters(match[1]) != expected:
            raise ValueError('Adapt public range-copy signature: ' + name)
        public = translate(match[1])
        target = name.replace('UA_', 'cpkt_opcua_', 1)
        header += [f'/** Invoke native {name}; destination must start empty and owns its result.',
                   ' * Inputs borrow until return. Native range errors are retained. Conversion',
                   ' * failure leaves dst unchanged; clear a successful result after use. */',
                   f'cpkt_opcua_StatusCode {target}({public});']
        metadata += [f'cpkt_opcua_StatusCode {target}({public}) {{',
                     f'  UA_{typename} source, destination;', '  UA_NumericRange native_range;',
                     f'  cpkt_opcua_{typename} staged;', '  UA_StatusCode status, native_status = 0;',
                     '  if(!src || !dst || src == dst) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                     '  memset(&source, 0, sizeof(source)); memset(&destination, 0, sizeof(destination));',
                     '  memset(&staged, 0, sizeof(staged));',
                     '  status = cpkt_utility_range(range, &native_range);',
                     f'  if(!status) status = cpkt_convert(src, &source, &cpkt_types[{index[typename]}], 1, 0);',
                     f'  if(!status) {{ native_status = {name}(&source, &destination, native_range);',
                     f'    status = cpkt_convert(&destination, &staged, &cpkt_types[{index[typename]}], 0, 0);',
                     '    if(!status) { *dst = staged; memset(&staged, 0, sizeof(staged)); } }',
                     f'  UA_{typename}_clear(&source); UA_{typename}_clear(&destination); cpkt_opcua_{typename}_clear(&staged);',
                     '  UA_free(native_range.dimensions);', '  return status ? status : native_status;', '}']
    for name in ('setRange', 'setRangeCopy'):
        match = re.search('UA_Variant_' + name + r'\s*\((.*?)\)\s*;', native_types, re.S)
        expected = [('UA_Variant *', 'v'), ('const void *' if name.endswith('Copy') else 'void *', 'array'),
                    ('size_t', 'arraySize'), ('const UA_NumericRange', 'range')]
        if not match or parameters(match[1]) != expected:
            raise ValueError('Adapt native range insertion: ' + name)
        header += [
            '/** Insert with the native multidimensional range rules. Destination',
            ' * array/dimension allocations and untouched element addresses are retained.',
            ' * Move transfers the actual C89 payload pointers and zeros pointer-owned',
            ' * input elements, just as native does. Copy allocates independent payloads.',
            ' * Native partial copy failures replace selected elements with their native',
            ' * partial results; no facade rollback is added. Boundary staging failures',
            ' * leave C89 inputs unchanged. Source/destination ownership must not overlap.',
            ' * NODELETE retains destination buffer ownership; overwritten nested owned',
            ' * members are still cleared, following the native operation. No custom',
            ' * range parser exists: native setRange validates and copyRange selects.',
            ' * Scalar string/Variant slicing remains unsupported by native setRange. */',
            'cpkt_opcua_StatusCode cpkt_opcua_Variant_' + name + '(' + translate(match[1]) + ');']
    header += ['/** Native pretty printing of any supported C89 schema value. Inputs borrow',
               ' * through return; output follows native String allocation/buffer rules. */',
               'cpkt_opcua_StatusCode cpkt_opcua_print(const void *p, const cpkt_opcua_Type *type, cpkt_opcua_String *output);',
               '/** Native numeric-type predicate for a supported public type descriptor. */',
               'cpkt_opcua_Boolean cpkt_opcua_DataType_isNumeric(const cpkt_opcua_Type *type);']
    metadata += ['cpkt_opcua_StatusCode cpkt_opcua_print(const void *p, const cpkt_opcua_Type *type, cpkt_opcua_String *output) {',
                 '  void *native;', '  UA_String text;', '  UA_StatusCode status;',
                 '  if(!p || !output || !cpkt_valid_type(type)) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                 '  native = UA_new(type->native); if(!native) return UA_STATUSCODE_BADOUTOFMEMORY;',
                 '  status = cpkt_convert(p, native, type, 1, 0);',
                 '  if(!status) { text = cpkt_string_view(output); status = UA_print(native, type->native, &text); cpkt_string_take(output, text); }',
                 '  UA_delete(native, type->native); return status;', '}',
                 'cpkt_opcua_Boolean cpkt_opcua_DataType_isNumeric(const cpkt_opcua_Type *type) {',
                 '  return cpkt_valid_type(type) ? UA_DataType_isNumeric(type->native) : 0;', '}']
    return header, metadata


def emit_event_filter(index, headers):
    text = uncomment((headers / 'util.h').read_text())
    fields = re.search(r'typedef\s+struct\s*\{([^{}]*)\}\s*UA_EventFilterParserOptions\s*;', text, re.S)
    signature = re.search(r'UA_EventFilter_parse\s*\((.*?)\)\s*;', text, re.S)
    expected = [('UA_EventFilter *', 'filter'), ('UA_ByteString', 'content'), ('UA_EventFilterParserOptions *', 'options')]
    if not fields or ' '.join(fields[1].split()) != 'const UA_Logger *logger;' or not signature or parameters(signature[1]) != expected:
        raise ValueError('Adapt native event-filter parser declarations')
    header = ['/** Complete native parser options with the rendered C89 logger hook.',
              ' * Logger config and its user data borrow for this synchronous call. */',
              'typedef struct { const cpkt_opcua_log_config *logger; } cpkt_opcua_EventFilterParserOptions;',
              '/** Parse an event filter through the native JSON/event-filter parser.',
              ' * Output starts empty and owns its result. Native partial output is',
              ' * committed when conversion succeeds; conversion failure retains filter.',
              ' * NULL options preserves the native parser logging default. */',
              'cpkt_opcua_StatusCode cpkt_opcua_EventFilter_parse(cpkt_opcua_EventFilter *filter, cpkt_opcua_ByteString content, cpkt_opcua_EventFilterParserOptions *options);']
    metadata = ['cpkt_opcua_StatusCode cpkt_opcua_EventFilter_parse(cpkt_opcua_EventFilter *filter, cpkt_opcua_ByteString content, cpkt_opcua_EventFilterParserOptions *options) {',
                '  UA_EventFilter native;', '  cpkt_opcua_EventFilter staged;', '  UA_EventFilterParserOptions settings;',
                '  struct cpkt_opcua_logger bridge;', '  UA_ByteString text;', '  UA_StatusCode status, converted;',
                '  if(!filter || (content.length && !content.data) || (options && !cpkt_logger_valid(options->logger))) return UA_STATUSCODE_BADINVALIDARGUMENT;',
                '  memset(&native, 0, sizeof(native)); memset(&staged, 0, sizeof(staged));',
                '  memset(&settings, 0, sizeof(settings)); memset(&bridge, 0, sizeof(bridge));',
                '  if(options && options->logger) { cpkt_logger_set(&bridge, &bridge.native, options->logger); settings.logger = &bridge.native; }',
                '  text = cpkt_string_view(&content);',
                '  status = UA_EventFilter_parse(&native, text, options ? &settings : NULL);',
                f'  converted = cpkt_convert(&native, &staged, &cpkt_types[{index["EventFilter"]}], 0, 0);',
                '  if(!converted) { *filter = staged; memset(&staged, 0, sizeof(staged)); }',
                '  UA_EventFilter_clear(&native); cpkt_opcua_EventFilter_clear(&staged);',
                '  return converted ? converted : status;', '}']
    return header, metadata


def emit_security_records(headers):
    """Retain every public policy field and callback signature, without backend types."""
    text = uncomment((headers / 'plugin/securitypolicy.h').read_text())
    declaration, constants = enum_declaration(text, 'SecurityPolicyType')
    header = ['/** Complete native security-policy kind values. */', declaration,
              '/** Forward declaration of the full C89 security-policy callback record. */\ntypedef struct cpkt_opcua_SecurityPolicy cpkt_opcua_SecurityPolicy;',
              '/** Forward declaration of the full C89 PubSub security-policy callback record. */\ntypedef struct cpkt_opcua_PubSubSecurityPolicy cpkt_opcua_PubSubSecurityPolicy;']
    metadata = [f'typedef char security_kind_{value}[((int){value} == (int){value.replace("UA_", "CPKT_OPCUA_")}) ? 1 : -1];' for value in constants]
    for typename in ('SecurityPolicySignatureAlgorithm', 'SecurityPolicyEncryptionAlgorithm',
                     'SecurityPolicy', 'PubSubSecurityPolicy'):
        if typename.endswith('Algorithm'):
            match = re.search(r'typedef\s+struct\s*\{([^{}]*)\}\s*UA_' + typename + r'\s*;', text, re.S)
        else:
            match = re.search(r'struct\s+UA_' + typename + r'\s*\{([^{}]*)\}\s*;', text, re.S)
        if not match:
            raise ValueError('Adapt security-policy public record: ' + typename)
        body = match[1]
        if '#' in body or re.search(r'\b(?:long|bool|uint64_t|int64_t)\b', body):
            raise ValueError('Adapt conditional or non-C89 policy record: ' + typename)
        body = translate(body).replace('const cpkt_opcua_Logger *', 'const cpkt_opcua_log_config *')
        header += ['/** Complete public policy fields and callback arguments. Backend/channel',
                   ' * contexts remain opaque. Byte buffers obey each native operation\'s',
                   ' * borrowed/preallocated/owned contract; callback records do not copy keys.',
                   ' * Logger config uses the rendered C89 hook, preserving one event per call. */']
        if typename.endswith('Algorithm'):
            header += ['typedef struct {' + body + '} cpkt_opcua_' + typename + ';']
        else:
            header += ['struct cpkt_opcua_' + typename + ' {' + body + '};']
    return header, metadata


def emit_operations(headers):
    """Boundaries requiring actual native storage instead of copied C89 fields."""
    declarations = {
        'UA_Server_run': ('server.h', [('UA_Server *', 'server'), ('const volatile UA_Boolean *', 'running')]),
        'UA_Server_runUntilInterrupt': ('server.h', [('UA_Server *', 'server')]),
        'UA_Server_getSessionAttribute': ('server.h', [('UA_Server *', 'server'), ('const UA_NodeId *', 'sessionId'), ('const UA_QualifiedName', 'key'), ('UA_Variant *', 'outValue')]),
        'UA_Server_getSessionAttribute_scalar': ('server.h', [('UA_Server *', 'server'), ('const UA_NodeId *', 'sessionId'), ('const UA_QualifiedName', 'key'), ('const UA_DataType *', 'type'), ('void *', 'outValue')]),
        'UA_Server_writeObjectProperty_scalar': ('server.h', [('UA_Server *', 'server'), ('const UA_NodeId', 'objectId'), ('const UA_QualifiedName', 'propertyName'), ('const void *', 'value'), ('const UA_DataType *', 'type')]),
        'UA_Client_getConnectionAttribute': ('client.h', [('UA_Client *', 'client'), ('const UA_QualifiedName', 'key'), ('UA_Variant *', 'outValue')]),
        'UA_Client_getConnectionAttributeCopy': ('client.h', [('UA_Client *', 'client'), ('const UA_QualifiedName', 'key'), ('UA_Variant *', 'outValue')]),
        'UA_Client_getConnectionAttribute_scalar': ('client.h', [('UA_Client *', 'client'), ('const UA_QualifiedName', 'key'), ('const UA_DataType *', 'type'), ('void *', 'outValue')]),
        'UA_Client_findDataType': ('client.h', [('UA_Client *', 'client'), ('const UA_NodeId *', 'typeId')]),
    }
    for name, (filename, expected) in declarations.items():
        source = uncomment((headers / filename).read_text())
        match = re.search(r'\b' + name + r'\s*\((.*?)\)\s*;', source, re.S)
        if not match or parameters(match[1]) != expected:
            raise ValueError('Adapt public native operation: ' + name + ': ' + (str(parameters(match[1])) if match else 'missing'))
    return [
        '/** Actual native volatile Boolean storage for UA_Server_run. This avoids',
        ' * aliasing a C89 byte as C99 bool. Initialize before run; callbacks may set',
        ' * it to false to stop. Keep alive until run returns. Volatile provides the',
        ' * same native polling semantics; serialize other access as upstream requires. */',
        'typedef struct cpkt_opcua_RunFlag cpkt_opcua_RunFlag;',
        '/** Access actual native volatile Boolean run-flag storage. Keep it alive until the native run call returns; release only after the loop has stopped. */\ncpkt_opcua_RunFlag *cpkt_opcua_RunFlag_new(cpkt_opcua_Boolean running);',
        '/** Access actual native volatile Boolean run-flag storage. Keep it alive until the native run call returns; release only after the loop has stopped. */\nvoid cpkt_opcua_RunFlag_set(cpkt_opcua_RunFlag *flag, cpkt_opcua_Boolean running);',
        '/** Access actual native volatile Boolean run-flag storage. Keep it alive until the native run call returns; release only after the loop has stopped. */\ncpkt_opcua_Boolean cpkt_opcua_RunFlag_get(const cpkt_opcua_RunFlag *flag);',
        '/** Access actual native volatile Boolean run-flag storage. Keep it alive until the native run call returns; release only after the loop has stopped. */\nvoid cpkt_opcua_RunFlag_delete(cpkt_opcua_RunFlag *flag);',
        '/** Call the native run loop, including native shutdownDelay/interrupt handling.',
        ' * The server remains owned by the caller after return. Do not destroy it',
        ' * during callbacks; callback configuration and schema conversions stay live. */',
        'cpkt_opcua_StatusCode cpkt_opcua_server_run_typed(cpkt_opcua_server *server, const cpkt_opcua_RunFlag *running);',
        '/** Run the native server until its platform interrupt manager requests shutdown. Scheduling and stop status remain native; invoke outside callbacks. */\ncpkt_opcua_StatusCode cpkt_opcua_server_runUntilInterrupt_typed(cpkt_opcua_server *server);',
        '/** Native UA_Server_delete without implicit shutdown. Success destroys the',
        ' * handle. Failure retains it. Finish native shutdown before calling. The',
        ' * legacy server_free convenience function requests shutdown before deletion. */',
        'cpkt_opcua_StatusCode cpkt_opcua_server_delete_typed(cpkt_opcua_server *server);',
        '/** Owned native Variant root holding the upstream borrowed value. Non-copy',
        ' * attribute getters preserve DATA_NODELETE and actual backend pointers.',
        ' * The root does not extend backend payload lifetime: do not snapshot after',
        ' * connection/session/config changes invalidate that native borrowed value.',
        ' * Serialize reads and mutations, including snapshot. delete frees only the',
        ' * root. snapshot explicitly creates an owned C89 value; clear it after use. */',
        'typedef struct cpkt_opcua_VariantView cpkt_opcua_VariantView;',
        '/** Allocate an owned root for an actual native borrowed Variant. NULL reports allocation failure; payload ownership remains native. */',
        'cpkt_opcua_VariantView *cpkt_opcua_VariantView_new(void);',
        '/** Release only the owned root. Borrowed payload is never freed or inspected, including after its native owner expires. */',
        'void cpkt_opcua_VariantView_delete(cpkt_opcua_VariantView *view);',
        '/** Explicitly copy a currently valid native borrowed value into an empty owned C89 Variant; clear the output after use. */',
        'cpkt_opcua_StatusCode cpkt_opcua_VariantView_snapshot(const cpkt_opcua_VariantView *view, cpkt_opcua_Variant *value);',
        '/** Fill the view root using the native non-copy getter. Payload borrows until connection/configuration mutation; serialize access and snapshot before invalidation. */',
        'cpkt_opcua_StatusCode cpkt_opcua_client_getConnectionAttribute_typed(cpkt_opcua_client *client, cpkt_opcua_QualifiedName key, cpkt_opcua_VariantView *value);',
        '/** Fill the view root using the native non-copy session getter. Payload borrows until session/configuration mutation; serialize access and snapshot before invalidation. */',
        'cpkt_opcua_StatusCode cpkt_opcua_server_getSessionAttribute_typed(cpkt_opcua_server *server, const cpkt_opcua_NodeId *sessionId, cpkt_opcua_QualifiedName key, cpkt_opcua_VariantView *value);',
        '/** Call the native copying getter; output starts empty and owns a C89 copy. */',
        'cpkt_opcua_StatusCode cpkt_opcua_client_getConnectionAttributeCopy_typed(cpkt_opcua_client *client, cpkt_opcua_QualifiedName key, cpkt_opcua_Variant *value);',
        '/** Native scalar type check and borrowed scalar retrieval, followed by an',
        ' * explicit ABI conversion to an owned C89 scalar. Output starts empty.',
        ' * This conversion does not change/copy the native getter\'s payload. Clear',
        ' * the C89 result with type_clear when done. Failed calls retain output. */',
        'cpkt_opcua_StatusCode cpkt_opcua_client_getConnectionAttribute_scalar_typed(cpkt_opcua_client *client, cpkt_opcua_QualifiedName key, const cpkt_opcua_Type *type, void *value);',
        '/** Perform the native scalar type check and getter, then convert to an owned C89 scalar. Empty output required; clear with type_clear after use. Failure retains output. */',
        'cpkt_opcua_StatusCode cpkt_opcua_server_getSessionAttribute_scalar_typed(cpkt_opcua_server *server, const cpkt_opcua_NodeId *sessionId, cpkt_opcua_QualifiedName key, const cpkt_opcua_Type *type, void *value);',
        '/** Write one complete C89 scalar through the native property API. Borrow',
        ' * input only until return. Native property lookup/statuses remain native. */',
        'cpkt_opcua_StatusCode cpkt_opcua_server_writeObjectProperty_scalar_typed(cpkt_opcua_server *server, cpkt_opcua_NodeId objectId, cpkt_opcua_QualifiedName propertyName, const void *value, const cpkt_opcua_Type *type);',
        '/** Borrow the generated/custom descriptor selected by the actual client.',
        ' * It remains valid until configuration replacement or client deletion. */',
        'const cpkt_opcua_Type *cpkt_opcua_client_findDataType_typed(cpkt_opcua_client *client, const cpkt_opcua_NodeId *typeId);',
        '/** Actual native connection defaults; output has no owned pointers. */',
        'cpkt_opcua_StatusCode cpkt_opcua_ConnectionConfig_default(cpkt_opcua_ConnectionConfig *value);',
        '/** Native static None URI; never clear or modify this borrowed record. */',
        'extern const cpkt_opcua_String cpkt_opcua_SECURITY_POLICY_NONE_URI;',
    ]
