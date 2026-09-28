"""C89 public codec records and bindings, derived from upstream declarations.

Wire formats and allocation decisions stay in open62541. The conversion layer
only changes the in-memory representation of public schema and message records.
"""
import re
from pathlib import Path
from node_emitter import record_body
from plugin_emitter import translate, uncomment
from core_client_emitter import enum_declaration

OPTIONS = ('EncodeBinaryOptions', 'DecodeBinaryOptions', 'EncodeJsonOptions',
           'DecodeJsonOptions', 'EncodeXmlOptions', 'DecodeXmlOptions')
ENUMS = ('FieldEncoding', 'DataSetMessageType', 'NetworkMessageType')
RECORDS = ('DataSetMessageHeader', 'DataSetMessage_DeltaFrameField',
           'DataSetMessage', 'NetworkMessageGroupHeader',
           'NetworkMessageSecurityHeader', 'NetworkMessage',
           'DataSetMessage_EncodingMetaData', 'NetworkMessage_EncodingOptions')
COUNTS = {'keyFrameFields': 'fieldCount', 'deltaFrameFields': 'fieldCount',
          'promotedFields': 'promotedFieldsSize', 'dataSetMessages': 'messageCount',
          'fields': 'fieldsSize', 'metaData': 'metaDataSize'}


def fields(body, path=''):
    """Flatten public unions; retain their upstream discriminants explicitly."""
    out = []
    while body.strip():
        body = body.lstrip()
        union = re.match(r'union\s*\{([^{}]*)\}\s*(\w+)\s*;', body, re.S)
        if union:
            out += fields(union[1], path + union[2] + '.')
            body = body[union.end():]
            continue
        match = re.match(r'(UA_\w+|size_t)\s*(\*?)\s*(\w+)\s*(?:\[([^\]]+)\])?\s*;', body)
        if not match:
            raise ValueError('Adapt public message field: ' + body[:140])
        out.append((match[1][3:] if match[1].startswith('UA_') else match[1],
                    path + match[3], bool(match[2]), match[4]))
        body = body[match.end():]
    return out


def condition(record, field, receiver):
    if record == 'DataSetMessage' and field.startswith('data.'):
        kind = 'DATADELTAFRAME' if field.endswith('deltaFrameFields') else 'DATAKEYFRAME'
        return f'(int){receiver}->header.dataSetMessageType == (int)UA_DATASETMESSAGETYPE_{kind}'
    if record == 'NetworkMessage':
        if field == 'payload.dataSetMessages':
            return f'(int){receiver}->networkMessageType == (int)UA_NETWORKMESSAGE_DATASET'
        if field == 'promotedFields':
            return f'{receiver}->promotedFieldsEnabled'
        if field == 'publisherId':
            return f'{receiver}->publisherIdEnabled'
    return None


def emit_codecs(index, headers, output):
    text = uncomment((headers / 'types.h').read_text())
    ps = uncomment((headers / 'pubsub.h').read_text())
    h = ['/* Derived from open62541 public types.h and pubsub.h (MPL-2.0). */',
         '/** Borrowed linked list of public type descriptors for codec options.',
         ' * Each descriptor and array outlives the synchronous codec call.',
         ' * Decoded values borrow the exact selected descriptor, which must',
         ' * also outlive those values. cleanup governs configuration ownership,',
         ' * not codec calls. Equivalent descriptors retain separate owners. */',
         'typedef struct cpkt_opcua_DataTypeArray {',
         '  struct cpkt_opcua_DataTypeArray *next;', '  size_t typesSize;',
         '  const cpkt_opcua_Type *const *types;', '  cpkt_opcua_Boolean cleanup;',
         '} cpkt_opcua_DataTypeArray;']
    for name in OPTIONS:
        body = record_body(text, name)
        expected = {
            'EncodeBinaryOptions': 'UA_NamespaceMapping *namespaceMapping;',
            'DecodeBinaryOptions': 'const UA_DataTypeArray *customTypes; UA_NamespaceMapping *namespaceMapping; void *callocContext; void * (*calloc)(void *callocContext, size_t nelem, size_t elsize); size_t decodedLength;',
            'EncodeJsonOptions': 'UA_NamespaceMapping *namespaceMapping; const UA_String *serverUris; size_t serverUrisSize; UA_Boolean useReversible; UA_Boolean prettyPrint; UA_Boolean unquotedKeys; UA_Boolean stringNodeIds;',
            'DecodeJsonOptions': 'UA_NamespaceMapping *namespaceMapping; const UA_String *serverUris; size_t serverUrisSize; const UA_DataTypeArray *customTypes; size_t *decodedLength;',
            'EncodeXmlOptions': 'UA_NamespaceMapping *namespaceMapping; const UA_String *serverUris; size_t serverUrisSize;',
            'DecodeXmlOptions': 'UA_Boolean unwrapped; UA_NamespaceMapping *namespaceMapping; const UA_String *serverUris; size_t serverUrisSize; const UA_DataTypeArray *customTypes;'
        }[name]
        canonical = lambda value: re.sub(r'\s+', '', value)
        if canonical(body) != canonical(expected):
            raise ValueError('Adapt complete public codec options: ' + name)
        h += ['/** Complete upstream codec options. Initialize the whole record to zero.',
              ' * All mappings, URIs and custom descriptors are borrowed during the call.',
              ' * A binary calloc override also allocates the C89 result graph; native',
              ' * intermediate graphs remain in the same arena. No arena allocation',
              ' * is freed, including on failure. Release the arena yourself; do not',
              ' * call ordinary clear/delete on an arena-backed result. */',
              'typedef struct {' + translate(body) + '} cpkt_opcua_' + name + ';']
    for name in ENUMS:
        declaration, _ = enum_declaration(ps, name)
        h += ['/** Complete native PubSub message enum and discriminants. */', declaration]
    for constant in ('NETWORKMESSAGE_MAX_NONCE_LENGTH', 'NETWORKMESSAGE_MAXMESSAGECOUNT'):
        value = re.search(r'^#define UA_' + constant + r'\s+(\d+)\s*$', ps, re.M)
        if not value:
            raise ValueError('Adapt public message limit ' + constant)
        h += [f'#define CPKT_OPCUA_{constant} {value[1]}']
    layouts = {}
    for name in RECORDS:
        body = record_body(ps, name)
        layouts[name] = fields(body)
        h += ['/** Complete public message record. Only the native discriminated union',
              ' * arm is active. A headers-only NetworkMessage may have a NULL payload',
              ' * with a nonzero messageCount. Normal copies/results own nested values;',
              ' * clear/delete must not be used on arena-backed decode results. */',
              'typedef struct {' + translate(body).replace('cpkt_opcua_NETWORKMESSAGE_', 'CPKT_OPCUA_NETWORKMESSAGE_') + '} cpkt_opcua_' + name + ';',
              '/** Clear owned nested message fields and reset the record; never clear borrowed or arena-backed messages. */',
              f'void cpkt_opcua_{name}_clear(cpkt_opcua_{name} *value);',
              '/** Copy the selected native message arm into an empty destination. The result owns its nested schema values and must be cleared. */',
              f'cpkt_opcua_StatusCode cpkt_opcua_{name}_copy(const cpkt_opcua_{name} *source, cpkt_opcua_{name} *destination);']
    m = ['/* Generated C89 message conversion and codecs. */']
    for name in RECORDS:
        for native in (True, False):
            direction = 'native' if native else 'public'
            src, dst = ('cpkt_opcua_', 'UA_') if native else ('UA_', 'cpkt_opcua_')
            m += [f'static UA_StatusCode cpkt_message_{name}_{direction}(const {src}{name} *src, {dst}{name} *dst, const cpkt_conversion_allocator *allocator);']
    for name in RECORDS:
        layout = layouts[name]
        hcounts = {v for t, n, ptr, fixed in layout if ptr for v in [COUNTS[n.split('.')[-1]]]}
        for native in (True, False):
            direction = 'native' if native else 'public'
            src, dst = ('cpkt_opcua_', 'UA_') if native else ('UA_', 'cpkt_opcua_')
            m += [f'static UA_StatusCode cpkt_message_{name}_{direction}(const {src}{name} *src, {dst}{name} *dst, const cpkt_conversion_allocator *allocator) {{',
                  '  UA_StatusCode status = 0;', '  (void)allocator;']
            if name == 'NetworkMessage':
                m += ['  if(src->messageCount > UA_NETWORKMESSAGE_MAXMESSAGECOUNT) return UA_STATUSCODE_BADINVALIDARGUMENT;']
            if name == 'NetworkMessageSecurityHeader':
                m += ['  if(src->messageNonceSize > UA_NETWORKMESSAGE_MAX_NONCE_LENGTH) return UA_STATUSCODE_BADINVALIDARGUMENT;']
            for typ, field, ptr, fixed in layout:
                if field in hcounts:
                    # Preserve counts even on headers-only messages. Partial array
                    # allocation is zeroed, so clear safely handles conversion errors.
                    m += [f'  dst->{field} = src->{field};']
                    continue
                cond = condition(name, field, 'src')
                indent = '    ' if cond else '  '
                if cond:
                    m += ['  if(' + cond + ') {']
                if fixed:
                    m += [indent + f'memcpy(dst->{field}, src->{field}, sizeof(dst->{field}));']
                elif ptr:
                    count = COUNTS[field.split('.')[-1]]
                    # Header-only network messages deliberately have no payload.
                    m += [indent + f'if(!status && src->{field}) {{']
                    if typ in index:
                        m += [indent + f'  status = cpkt_array_ex(src->{field}, src->{count}, (void **)&dst->{field}, &cpkt_types[{index[typ]}], {int(native)}, 0, allocator);']
                    elif typ in RECORDS:
                        m += [indent + '  size_t i;',
                              indent + f'  if(src->{field} == UA_EMPTY_ARRAY_SENTINEL && src->{count}) status = UA_STATUSCODE_BADINVALIDARGUMENT;',
                              indent + f'  else if(!src->{count}) dst->{field} = ({dst}{typ} *)UA_EMPTY_ARRAY_SENTINEL;',
                              indent + '  else {',
                              indent + f'    dst->{field} = ({dst}{typ} *)cpkt_conversion_calloc(allocator, src->{count}, sizeof(*dst->{field}));',
                              indent + f'    if(!dst->{field}) status = UA_STATUSCODE_BADOUTOFMEMORY;',
                              indent + f'    for(i = 0; !status && i < src->{count}; ++i) status = cpkt_message_{typ}_{direction}(&src->{field}[i], &dst->{field}[i], allocator);',
                              indent + '  }']
                    else:
                        raise ValueError('Adapt message array ' + typ)
                    m += [indent + '}']
                    if not (name == 'NetworkMessage' and field == 'payload.dataSetMessages'):
                        m += [indent + f'else if(!status && src->{count}) status = UA_STATUSCODE_BADINVALIDARGUMENT;']
                elif typ in index:
                    m += [indent + f'if(!status) status = cpkt_convert_ex(&src->{field}, &dst->{field}, &cpkt_types[{index[typ]}], {int(native)}, 0, allocator);']
                elif typ == 'PublisherId':
                    # Publisher String must use the decode arena too.
                    m += [indent + f'dst->{field}.idType = ({dst}PublisherIdType)src->{field}.idType;',
                          indent + f'switch(src->{field}.idType) {{']
                    for member, schema in [('byte', 'Byte'), ('uint16', 'UInt16'), ('uint32', 'UInt32'), ('uint64', 'UInt64'), ('string', 'String')]:
                        m += [indent + f'case UA_PUBLISHERIDTYPE_{schema.upper()}:',
                              indent + f'  if(!status) status = cpkt_convert_ex(&src->{field}.id.{member}, &dst->{field}.id.{member}, &cpkt_types[{index[schema]}], {int(native)}, 0, allocator);', indent + '  break;']
                    m += [indent + 'default: status = UA_STATUSCODE_BADINVALIDARGUMENT; break;', indent + '}']
                elif typ in RECORDS:
                    m += [indent + f'if(!status) status = cpkt_message_{typ}_{direction}(&src->{field}, &dst->{field}, allocator);']
                elif typ in ENUMS:
                    m += [indent + f'dst->{field} = ({dst}{typ})src->{field};']
                elif typ == 'size_t':
                    m += [indent + f'dst->{field} = src->{field};']
                else:
                    raise ValueError('Adapt message field ' + name + '.' + field + ':' + typ)
                if cond:
                    m += ['  }']
            m += ['  return status;', '}']
        for native in (True, False):
            prefix = 'UA_' if native else 'cpkt_opcua_'
            fn = f'cpkt_message_{name}_clear_native' if native else f'cpkt_opcua_{name}_clear'
            m += [f'{"static " if native else ""}void {fn}({prefix}{name} *value) {{', '  if(!value) return;']
            deferred = []
            for typ, field, ptr, fixed in layout:
                if fixed:
                    continue
                cond = condition(name, field, 'value')
                indent = '    ' if cond else '  '
                if cond:
                    m += ['  if(' + cond + ') {']
                if ptr:
                    count = COUNTS[field.split('.')[-1]]
                    if typ in index:
                        fn2 = 'UA_Array_delete' if native else 'cpkt_opcua_array_delete'
                        typearg = f'cpkt_types[{index[typ]}].native' if native else f'&cpkt_types[{index[typ]}]'
                        m += [indent + f'if(value->{field}) {fn2}(value->{field}, value->{count}, {typearg});']
                    else:
                        fn2 = f'cpkt_message_{typ}_clear_native' if native else f'cpkt_opcua_{typ}_clear'
                        m += [indent + f'if(value->{field} && value->{field} != UA_EMPTY_ARRAY_SENTINEL) {{ size_t i; for(i = 0; i < value->{count}; ++i) {fn2}(&value->{field}[i]); UA_free(value->{field}); }}']
                elif typ in index:
                    # Scalars/counts/flags remain intact until every owned arm
                    # has been cleared. Only owning schema records need cleanup.
                    m += [indent + f'if(!cpkt_types[{index[typ]}].native->pointerFree) {prefix}{typ}_clear(&value->{field});']
                elif typ == 'PublisherId':
                    m += [indent + f'{prefix}PublisherId_clear(&value->{field});']
                elif typ in RECORDS:
                    fn2 = f'cpkt_message_{typ}_clear_native' if native else f'cpkt_opcua_{typ}_clear'
                    # Headers hold union discriminants; clear embedded records
                    # after sibling arrays have used their original controls.
                    deferred += [indent + f'{fn2}(&value->{field});']
                if cond:
                    m += ['  }']
            m += [*deferred, '  memset(value, 0, sizeof(*value));', '}']
        m += [f'cpkt_opcua_StatusCode cpkt_opcua_{name}_copy(const cpkt_opcua_{name} *src, cpkt_opcua_{name} *dst) {{',
              f'  UA_{name} native; UA_StatusCode status;',
              '  if(!src || !dst || src == dst) return UA_STATUSCODE_BADINVALIDARGUMENT;',
              '  memset(&native, 0, sizeof(native)); memset(dst, 0, sizeof(*dst));',
              f'  status = cpkt_message_{name}_native(src, &native, NULL);',
              f'  if(!status) status = cpkt_message_{name}_public(&native, dst, NULL);',
              f'  cpkt_message_{name}_clear_native(&native);',
              f'  if(status) cpkt_opcua_{name}_clear(dst);', '  return status;', '}']
    for encoding in ('Binary', 'Json', 'Xml'):
        const = '' if encoding == 'Binary' else 'const '
        for operation in ('calcSize', 'encode', 'decode'):
            ret = 'size_t' if operation == 'calcSize' else 'cpkt_opcua_StatusCode'
            if operation == 'decode':
                args = f'const cpkt_opcua_ByteString *source, void *destination, const cpkt_opcua_Type *type, {const}cpkt_opcua_Decode{encoding}Options *options'
            else:
                args = 'const void *source, const cpkt_opcua_Type *type, ' + ('cpkt_opcua_ByteString *output, ' if operation == 'encode' else '') + f'{const}cpkt_opcua_Encode{encoding}Options *options'
            h += ['/** Native codec with complete options; values use the public C89 descriptor.',
                  ' * Encode output uses the caller buffer or native allocation when empty.',
                  ' * Decode destinations start empty. Arena ownership follows the options. */',
                  f'{ret} cpkt_opcua_{operation}{encoding}({args});']
    for operation in ('encodeBinary', 'calcSizeBinary', 'decodeBinary', 'decodeBinaryHeaders', 'encodeJson', 'calcSizeJson', 'decodeJson'):
        decode = operation.startswith('decode')
        args = ('const cpkt_opcua_ByteString *source, cpkt_opcua_NetworkMessage *destination' if decode else 'const cpkt_opcua_NetworkMessage *source')
        if operation.startswith('encode'):
            args += ', cpkt_opcua_ByteString *output'
        args += ', const cpkt_opcua_NetworkMessage_EncodingOptions *encodingOptions'
        if operation.endswith('Json'):
            args += ', const cpkt_opcua_' + ('Decode' if decode else 'Encode') + 'JsonOptions *options'
        elif decode:
            args += ', const cpkt_opcua_DecodeBinaryOptions *options'
        if operation == 'decodeBinaryHeaders':
            args += ', size_t *payloadOffset'
        ret = 'size_t' if operation.startswith('calcSize') else 'cpkt_opcua_StatusCode'
        h += ['/** Native raw PubSub codec. Preserve native header/payload and buffer ownership.',
              ' * Destinations start empty. All metadata and options are borrowed for the call.',
              ' * Arena results follow DecodeBinaryOptions; never ordinary-clear them. */',
              f'{ret} cpkt_opcua_NetworkMessage_{operation}({args});']
    emit_bindings(output)
    (Path(output) / 'opcua_message_metadata.inc').write_text('\n'.join(m) + '\n')
    return h


def emit_bindings(output):
    m = ['/* Generated native codec entry points. */']
    for encoding in ('Binary', 'Json', 'Xml'):
        const = '' if encoding == 'Binary' else 'const '
        for operation in ('calcSize', 'encode', 'decode'):
            decode = operation == 'decode'
            ret = 'size_t' if operation == 'calcSize' else 'cpkt_opcua_StatusCode'
            kind = ('Decode' if decode else 'Encode') + encoding
            if decode:
                args = f'const cpkt_opcua_ByteString *source, void *destination, const cpkt_opcua_Type *type, {const}cpkt_opcua_{kind}Options *options'
            else:
                args = 'const void *source, const cpkt_opcua_Type *type, ' + ('cpkt_opcua_ByteString *output, ' if operation == 'encode' else '') + f'{const}cpkt_opcua_{kind}Options *options'
            m += [f'{ret} cpkt_opcua_{operation}{encoding}({args}) {{',
                  '  cpkt_codec_views views;', f'  UA_{kind}Options nativeOptions;',
                  '  void *native;', '  UA_StatusCode status;', '  UA_StatusCode converted;',
                  *(['  size_t result = 0;'] if operation == 'calcSize' else []),
                  *(['  UA_ByteString bytes;'] if operation != 'calcSize' else []),
                  '  cpkt_conversion_allocator allocator;',
                  '  if(!source || !cpkt_valid_type(type)' + (' || !destination || !cpkt_string_valid(source)' if decode else ' || !output || !cpkt_string_valid(output)' if operation == 'encode' else '') + ') return ' + ('0;' if operation == 'calcSize' else 'UA_STATUSCODE_BADINVALIDARGUMENT;'),
                  '  memset(&views, 0, sizeof(views)); memset(&allocator, 0, sizeof(allocator));',
                  f'  status = cpkt_codec_{kind}_view(options, &nativeOptions, &views);',
                  '  native = UA_calloc(1, type->native->memSize);',
                  '  if(!native && !status) status = UA_STATUSCODE_BADOUTOFMEMORY;']
            if decode:
                m += ['  memset(destination, 0, type->size);', '  bytes = cpkt_string_view(source);']
                if encoding == 'Binary':
                    m += ['  allocator.context = nativeOptions.callocContext; allocator.calloc = nativeOptions.calloc;']
                m += ['  if(!status) status = cpkt_codec_views_enter(&views);',
                      f'  if(!status) status = UA_decode{encoding}(&bytes, native, type->native, options ? &nativeOptions : NULL);',
                      '  if(native) {',
                      '    converted = cpkt_convert_ex(native, destination, type, 0, 0, allocator.calloc ? &allocator : NULL);',
                      '    if(converted) { if(!allocator.calloc) cpkt_opcua_type_clear(destination, type); status = converted; }', '  }']
                if encoding == 'Binary':
                    m += ['  if(options) options->decodedLength = nativeOptions.decodedLength;']
            else:
                m += ['  if(!status) status = cpkt_convert(source, native, type, 1, 0);', '  converted = 0; (void)converted;']
                if operation == 'calcSize':
                    m += [f'  if(!status) result = UA_calcSize{encoding}(native, type->native, options ? &nativeOptions : NULL);']
                else:
                    m += ['  bytes = cpkt_string_view(output);',
                          f'  if(!status) {{ status = UA_encode{encoding}(native, type->native, &bytes, options ? &nativeOptions : NULL); cpkt_string_take(output, bytes); }}']
            m += ['  if(native) { if(!allocator.calloc) UA_clear(native, type->native); UA_free(native); }',
                  '  cpkt_codec_views_clear(&views);', '  return ' + ('result;' if operation == 'calcSize' else 'status;'), '}']
    for operation in ('encodeBinary', 'calcSizeBinary', 'decodeBinary', 'decodeBinaryHeaders', 'encodeJson', 'calcSizeJson', 'decodeJson'):
        decode = operation.startswith('decode')
        calc = operation.startswith('calcSize')
        json = operation.endswith('Json')
        kind = ('Decode' if decode else 'Encode') + ('Json' if json else 'Binary')
        args = ('const cpkt_opcua_ByteString *source, cpkt_opcua_NetworkMessage *destination' if decode else 'const cpkt_opcua_NetworkMessage *source')
        if operation.startswith('encode'):
            args += ', cpkt_opcua_ByteString *output'
        args += ', const cpkt_opcua_NetworkMessage_EncodingOptions *encodingOptions'
        has_options = json or decode
        if has_options:
            args += f', const cpkt_opcua_{kind}Options *options'
        if operation == 'decodeBinaryHeaders':
            args += ', size_t *payloadOffset'
        ret = 'size_t' if calc else 'cpkt_opcua_StatusCode'
        m += [f'{ret} cpkt_opcua_NetworkMessage_{operation}({args}) {{',
              '  UA_NetworkMessage native;', '  UA_NetworkMessage_EncodingOptions metadata;',
              '  cpkt_codec_views views;', '  cpkt_conversion_allocator allocator;',
              *([f'  UA_{kind}Options nativeOptions;'] if has_options else []),
              '  UA_StatusCode status;', *(['  UA_StatusCode converted;'] if decode else []),
              *(['  size_t result = 0;'] if calc else ['  UA_ByteString bytes;']),
              '  if(!source' + (' || !destination || !cpkt_string_valid(source)' if decode else ' || !output || !cpkt_string_valid(output)' if not calc else '') + (' || !payloadOffset' if operation == 'decodeBinaryHeaders' else '') + ') return ' + ('0;' if calc else 'UA_STATUSCODE_BADINVALIDARGUMENT;'),
              '  memset(&native, 0, sizeof(native)); memset(&metadata, 0, sizeof(metadata));',
              '  memset(&views, 0, sizeof(views)); memset(&allocator, 0, sizeof(allocator));',
              '  status = encodingOptions ? cpkt_message_NetworkMessage_EncodingOptions_native(encodingOptions, &metadata, NULL) : 0;']
        if has_options:
            m += [f'  if(!status) status = cpkt_codec_{kind}_view(options, &nativeOptions, &views);',
                  '  else memset(&nativeOptions, 0, sizeof(nativeOptions));']
        if decode:
            m += ['  memset(destination, 0, sizeof(*destination));', '  bytes = cpkt_string_view(source);']
            if not json:
                m += ['  allocator.context = nativeOptions.callocContext; allocator.calloc = nativeOptions.calloc;']
            m += ['  if(!status) status = cpkt_codec_views_enter(&views);',
                  f'  if(!status) status = UA_NetworkMessage_{operation}(&bytes, &native, encodingOptions ? &metadata : NULL, options ? &nativeOptions : NULL' + (', payloadOffset' if operation == 'decodeBinaryHeaders' else '') + ');',
                  '  converted = cpkt_message_NetworkMessage_public(&native, destination, allocator.calloc ? &allocator : NULL);',
                  '  if(converted) { if(!allocator.calloc) cpkt_opcua_NetworkMessage_clear(destination); status = converted; }']
        else:
            m += ['  if(!status) status = cpkt_message_NetworkMessage_native(source, &native, NULL);']
            options_arg = ', options ? &nativeOptions : NULL' if json else ''
            if calc:
                m += [f'  if(!status) result = UA_NetworkMessage_{operation}(&native, encodingOptions ? &metadata : NULL{options_arg});']
            else:
                m += ['  bytes = cpkt_string_view(output);',
                      f'  if(!status) {{ status = UA_NetworkMessage_{operation}(&native, &bytes, encodingOptions ? &metadata : NULL{options_arg}); cpkt_string_take(output, bytes); }}']
        m += ['  if(!allocator.calloc) cpkt_message_NetworkMessage_clear_native(&native);',
              '  cpkt_message_NetworkMessage_EncodingOptions_clear_native(&metadata);',
              '  cpkt_codec_views_clear(&views);', '  return ' + ('result;' if calc else 'status;'), '}']
    (Path(output) / 'opcua_codec_metadata.inc').write_text('\n'.join(m) + '\n')
