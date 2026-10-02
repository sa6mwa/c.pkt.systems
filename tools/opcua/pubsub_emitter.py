"""Derive full PubSub records and boundary conversions from public headers.

The upstream schema emitter remains responsible for schema fields. This layer
only describes handwritten record ownership, discriminated unions and callbacks.
It does not implement PubSub transport, scheduling, security or state machines.
"""
import re
from node_emitter import record_body
from core_client_emitter import enum_declaration
from plugin_emitter import parameters, translate, uncomment

ENUMS = ('PublisherIdType', 'PublishedDataSetType', 'DataSetFieldType',
         'PubSubEncodingType', 'SubscribedDataSetType', 'PubSubOffsetType')
RECORDS = ('PublishedDataItemsTemplateConfig', 'PublishedEventConfig',
           'PublishedEventTemplateConfig', 'PublishedDataSetConfig',
           'AddPublishedDataSetResult', 'DataSetVariableConfig', 'DataSetFieldConfig',
           'PubSubConnectionConfig', 'WriterGroupConfig', 'DataSetWriterConfig',
           'SubscribedDataSetConfig', 'DataSetReaderConfig', 'ReaderGroupConfig',
           'PubSubOffset', 'PubSubOffsetTable')
ACTIVE = {'PubSubConnectionConfig': 'CONNECTION', 'WriterGroupConfig': 'WRITERGROUP',
          'DataSetWriterConfig': 'DATASETWRITER', 'ReaderGroupConfig': 'READERGROUP',
          'DataSetReaderConfig': 'DATASETREADER'}
UNIONS = {
    'PublishedDataSetConfig': ('publishedDataSetType', {
        'itemsTemplate': 'UA_PUBSUB_DATASET_PUBLISHEDITEMS_TEMPLATE',
        'event': 'UA_PUBSUB_DATASET_PUBLISHEDEVENTS',
        'eventTemplate': 'UA_PUBSUB_DATASET_PUBLISHEDEVENTS_TEMPLATE'}),
    'DataSetFieldConfig': ('dataSetFieldType', {'variable': 'UA_PUBSUB_DATASETFIELD_VARIABLE'}),
    'SubscribedDataSetConfig': ('subscribedDataSetType', {'target': 'UA_PUBSUB_SDS_TARGET'}),
    'DataSetReaderConfig': ('subscribedDataSetType', {'target': 'UA_PUBSUB_SDS_TARGET'}),
}


def source(headers):
    text = uncomment((headers / 'server_pubsub.h').read_text())
    # The macro is public API and contains the native callback signature.
    match = re.search(r'^#define UA_PUBSUBCOMPONENT_COMMON\s+(.*?)(?=\n[^\s\\])', text, re.M | re.S)
    if not match:
        raise ValueError('Adapt public PubSub common fields')
    common = match[1].replace('\\\n', '\n').rstrip('\\ \n')
    return text, common


def fields(body):
    body = re.sub(r'UA_StatusCode\s*\(\*customStateMachine\)\s*\(.*?\)\s*;', '', body, flags=re.S)
    result = []
    while body.strip():
        body = body.lstrip()
        union = re.match(r'union\s*\{([^{}]*)\}\s*(\w+)\s*;', body, re.S)
        if union:
            result.append(('union', union[2], fields(union[1])))
            body = body[union.end():]
            continue
        match = re.match(r'(UA_\w+|size_t|void)\s*(\*?)\s*(\w+)\s*;', body)
        if not match:
            raise ValueError('Adapt public PubSub field: ' + body[:160])
        result.append((match[1], match[3], match[2]))
        body = body[match.end():]
    return result


def layouts(headers):
    text, common = source(headers)
    return {record: fields(record_body(text, record).replace('UA_PUBSUBCOMPONENT_COMMON', common))
            for record in RECORDS}


def emit_records(headers):
    text, common = source(headers)
    h = [re.match(r'/\*.*?\*/', (headers / 'server_pubsub.h').read_text(), re.S)[0]]
    for typename in ENUMS:
        declaration, constants = enum_declaration(text, typename)
        h += ['/** Complete public native PubSub enum. */', declaration]
    publisher = record_body(text, 'PublisherId')
    if fields(publisher) != [('UA_PublisherIdType', 'idType', ''), ('union', 'id', [('UA_Byte', 'byte', ''), ('UA_UInt16', 'uint16', ''), ('UA_UInt32', 'uint32', ''), ('UA_UInt64', 'uint64', ''), ('UA_String', 'string', '')])]:
        raise ValueError('Adapt public PublisherId union')
    h += ['/** Native tagged publisher ID. Only the selected id field is active.',
          ' * The String arm follows normal owned/borrowed String rules. */',
          'typedef struct {' + translate(publisher) + '} cpkt_opcua_PublisherId;']
    for record in RECORDS:
        body = record_body(text, record).replace('UA_PUBSUBCOMPONENT_COMMON', common)
        fields(body)  # Validate the complete declaration before emitting it.
        h += ['/** Complete native public PubSub record. Schema members own allocations',
              ' * in copied/getter results. Contexts, callback slots and securityPolicy',
              ' * remain borrowed; clearing the config never clears a borrowed policy.',
              ' * Installed callback user data and policies outlive the component.',
              ' * Calls on the server and changes to borrowed policies are serialized. */',
              'typedef struct {' + translate(body) + '} cpkt_opcua_' + record + ';']
    configuration = record_body(text, 'PubSubConfiguration')
    configuration = re.sub(r'^\s*#(?:ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL|endif)\s*$', '', configuration, flags=re.M)
    if '#' in configuration:
        raise ValueError('Adapt public PubSub configuration features')
    h += ['/** Complete native global PubSub configuration. Callback arguments borrow',
          ' * during dispatch. Configuration installation transfers policy record',
          ' * ownership only on success; the caller retains its array allocation. */',
          'typedef struct {' + translate(configuration) + '} cpkt_opcua_PubSubConfiguration;']
    return h


def emit_conversion(index, headers, record, layout, to_native):
    dstprefix, srcprefix = ('UA_', 'cpkt_opcua_') if to_native else ('cpkt_opcua_', 'UA_')
    out = [f'static UA_StatusCode cpkt_ps_{record}_{"native" if to_native else "public"}(const {srcprefix}{record} *src, {dstprefix}{record} *dst, cpkt_pubsub_entry *entry) {{',
           '  UA_StatusCode status = 0;', '  (void)entry;']
    if record in ACTIVE:
        if to_native:
            out += ['  dst->customStateMachine = src->customStateMachine ? cpkt_pubsub_state : NULL;']
        else:
            out += ['  if(src->customStateMachine) {',
                    '    if(src->customStateMachine != cpkt_pubsub_state || !entry) return UA_STATUSCODE_BADNOTSUPPORTED;',
                    '    dst->customStateMachine = entry->state;', '  }']
    def emit(items, path='', indent='  '):
        lines = []
        names = {name for typ, name, ptr in items}
        for spelling, name, pointer in items:
            field = path + name
            if spelling == 'union':
                discriminant, cases = UNIONS[record]
                lines += [indent + f'switch(src->{discriminant}) {{']
                for typ, member, ptr in pointer:
                    lines += [indent + f'case {cases[member].replace("UA_", "CPKT_OPCUA_") if to_native else cases[member]}:']
                    lines += emit([(typ, member, ptr)], field + '.', indent + '  ')
                    lines += [indent + '  break;']
                lines += [indent + 'default: break;', indent + '}']
                continue
            if spelling == 'void' and pointer:
                lines += [indent + f'dst->{field} = src->{field};']
                continue
            if spelling == 'UA_PubSubSecurityPolicy' and pointer:
                if to_native:
                    lines += [indent + f'if(!status && src->{field}) {{ if(!entry) status = UA_STATUSCODE_BADINVALIDARGUMENT; else status = cpkt_pubsub_policy(entry, src->{field}, &dst->{field}); }}']
                else:
                    lines += [indent + f'if(src->{field}) {{ if(!entry || src->{field} != entry->policy) status = UA_STATUSCODE_BADNOTSUPPORTED; else dst->{field} = entry->borrowed_policy; }}']
                continue
            if pointer:
                count = name + 'Size'
                if count not in names:
                    raise ValueError('Adapt public PubSub array count: ' + record + '.' + field)
                if spelling.startswith('UA_') and spelling[3:] in index:
                    lines += [indent + f'if(!status) status = cpkt_array(src->{field}, src->{path + count}, (void **)&dst->{field}, &cpkt_types[{index[spelling[3:]]}], {int(to_native)}, 0);',
                              indent + f'dst->{path + count} = dst->{field} ? src->{path + count} : 0;']
                elif spelling[3:] in RECORDS:
                    typ = spelling[3:]
                    lines += [indent + '{ size_t i;', indent + f'  if(!status && src->{path + count}) {{',
                              indent + f'    if(!src->{field} || src->{path + count} > (size_t)-1 / sizeof(*dst->{field})) status = UA_STATUSCODE_BADINVALIDARGUMENT;',
                              indent + f'    else {{ dst->{field} = ({dstprefix}{typ} *)UA_calloc(src->{path + count}, sizeof(*dst->{field}));',
                              indent + f'      if(!dst->{field}) status = UA_STATUSCODE_BADOUTOFMEMORY;',
                              indent + f'      else {{ dst->{path + count} = src->{path + count}; for(i = 0; !status && i < src->{path + count}; ++i) status = cpkt_ps_{typ}_{"native" if to_native else "public"}(&src->{field}[i], &dst->{field}[i], entry); }} }}',
                              indent + '  }', indent + '}']
                else:
                    raise ValueError('Adapt public PubSub array type: ' + spelling)
                continue
            if name.endswith('Size') and name[:-4] in names:
                continue
            if spelling == 'size_t':
                lines += [indent + f'dst->{field} = src->{field};']
            elif spelling[3:] in index:
                lines += [indent + f'if(!status) status = cpkt_convert(&src->{field}, &dst->{field}, &cpkt_types[{index[spelling[3:]]}], {int(to_native)}, 0);']
            elif spelling[3:] in ENUMS:
                lines += [indent + f'dst->{field} = ({dstprefix}{spelling[3:]})src->{field};']
            elif spelling == 'UA_KeyValueMap':
                lines += [indent + f'if(!status) status = cpkt_array(src->{field}.map, src->{field}.mapSize, (void **)&dst->{field}.map, &cpkt_types[{index["KeyValuePair"]}], {int(to_native)}, 0);',
                          indent + f'dst->{field}.mapSize = dst->{field}.map ? src->{field}.mapSize : 0;']
            elif spelling == 'UA_PublisherId':
                lines += [indent + f'if(!status) status = cpkt_ps_publisher_{"native" if to_native else "public"}(&src->{field}, &dst->{field});']
            elif spelling[3:] in RECORDS:
                lines += [indent + f'if(!status) status = cpkt_ps_{spelling[3:]}_{"native" if to_native else "public"}(&src->{field}, &dst->{field}, entry);']
            else:
                raise ValueError('Adapt public PubSub value type: ' + record + ':' + spelling)
        return lines
    out += emit(layout)
    out += ['  return status;', '}']
    return out


def emit_clear(index, record, layout, native):
    prefix = 'UA_' if native else 'cpkt_opcua_'
    name = 'cpkt_ps_' + record + '_clear_native' if native else 'cpkt_opcua_' + record + '_clear'
    out = [f'{"static " if native else ""}void {name}({prefix}{record} *value) {{', '  if(!value) return;']
    def emit(items, path='', indent='  '):
        lines = []
        names = {name for typ, name, ptr in items}
        for typ, name, pointer in items:
            field = path + name
            if typ == 'union':
                discriminant, cases = UNIONS[record]
                lines += [indent + f'switch(value->{discriminant}) {{']
                for subtyp, member, ptr in pointer:
                    lines += [indent + f'case {cases[member] if native else cases[member].replace("UA_", "CPKT_OPCUA_")}:']
                    lines += emit([(subtyp, member, ptr)], field + '.', indent + '  ')
                    lines += [indent + '  break;']
                lines += [indent + 'default: break;', indent + '}']
            elif pointer and typ in ('void', 'UA_PubSubSecurityPolicy'):
                continue
            elif pointer and typ[3:] in index:
                count = path + name + 'Size'
                function = 'UA_Array_delete' if native else 'cpkt_opcua_array_delete'
                typearg = f'cpkt_types[{index[typ[3:]]}].native' if native else f'&cpkt_types[{index[typ[3:]]}]'
                lines += [indent + f'if(value->{field}) {function}(value->{field}, value->{count}, {typearg});']
            elif pointer and typ[3:] in RECORDS:
                count = path + name + 'Size'
                function = f'cpkt_ps_{typ[3:]}_clear_native' if native else f'cpkt_opcua_{typ[3:]}_clear'
                lines += [indent + '{ size_t i;', indent + f'  if(value->{field}) {{ for(i = 0; i < value->{count}; ++i) {function}(&value->{field}[i]); UA_free(value->{field}); }}', indent + '}']
            elif typ.startswith('UA_') and typ[3:] in index:
                lines += [indent + f'{prefix}{typ[3:]}_clear(&value->{field});']
            elif typ == 'UA_KeyValueMap':
                lines += [indent + f'{prefix}KeyValueMap_clear(&value->{field});']
            elif typ == 'UA_PublisherId':
                lines += [indent + f'{prefix}PublisherId_clear(&value->{field});']
            elif typ[3:] in RECORDS:
                function = f'cpkt_ps_{typ[3:]}_clear_native' if native else f'cpkt_opcua_{typ[3:]}_clear'
                lines += [indent + f'{function}(&value->{field});']
            elif typ not in ('size_t', 'void') and typ[3:] not in ENUMS:
                raise ValueError('Adapt public PubSub clear field: ' + record + '.' + field)
        return lines
    out += emit(layout)
    out += ['  memset(value, 0, sizeof(*value));', '}']
    return out


def emit_pubsub(index, headers):
    text, common = source(headers)
    records = layouts(headers)
    needed = conversion_records(text, records)
    h, m = [], []
    for record in RECORDS:
        h += [f'/** Release owned schema fields; borrowed contexts/policies remain caller-owned. */',
              f'void cpkt_opcua_{record}_clear(cpkt_opcua_{record} *value);',
              '/** Independent full public record copy. dst must start empty. Failure retains',
              ' * dst; success preserves callback/context/policy pointer identity. */',
              f'cpkt_opcua_StatusCode cpkt_opcua_{record}_copy(const cpkt_opcua_{record} *src, cpkt_opcua_{record} *dst);']
    for record in RECORDS:
        if record in needed[True]:
            m += [f'static UA_StatusCode cpkt_ps_{record}_native(const cpkt_opcua_{record} *, UA_{record} *, cpkt_pubsub_entry *);']
        if record in needed[False]:
            m += [f'static UA_StatusCode cpkt_ps_{record}_public(const UA_{record} *, cpkt_opcua_{record} *, cpkt_pubsub_entry *);']
        m += [f'static void cpkt_ps_{record}_clear_native(UA_{record} *);']
    for record, layout in records.items():
        for direction in (True, False):
            if record in needed[direction]:
                m += emit_conversion(index, headers, record, layout, direction)
        m += emit_clear(index, record, layout, True)
        m += emit_clear(index, record, layout, False)
        # A public copy must preserve borrowed slots exactly; schema ownership
        # conversion alone cannot erase/customize callbacks or policy pointers.
        m += emit_copy(index, record, layout)
    mh, mm = emit_publisher(index)
    h += mh
    m += mm
    mh, mm = emit_operations(index, headers)
    h += mh
    m += mm
    h += ['/** Set complete global PubSub configuration before adding components/startup.',
          ' * Callback records are copied. Success consumes each security policy and',
          ' * zeroes its input record; the array allocation stays caller-owned. Failure',
          ' * preserves every input policy. Old native policies clear after staging.',
          ' * Original application/server contexts are retained. Serialize server calls. */',
          'cpkt_opcua_StatusCode cpkt_opcua_server_set_pubsub_configuration(cpkt_opcua_server *server, cpkt_opcua_PubSubConfiguration *config);',
          '/** Replace global callbacks/flags without changing installed policies.',
          ' * securityPoliciesSize must be zero. Replacement within a callback is',
          ' * supported. Arguments borrow until return and are copied; user contexts',
          ' * and borrowed callback arguments follow their native lifetime rules. */',
          'cpkt_opcua_StatusCode cpkt_opcua_server_set_pubsub_callbacks(cpkt_opcua_server *server, const cpkt_opcua_PubSubConfiguration *config);',
          '/** Borrow an installed C89 policy. Never clear/free it; the server owns it.',
          ' * Valid until replacing global policies or server destruction. Edits while',
          ' * quiescent are applied before the next native facade operation. */',
          'cpkt_opcua_StatusCode cpkt_opcua_server_get_pubsub_security_policy(cpkt_opcua_server *server, size_t index, cpkt_opcua_PubSubSecurityPolicy **out);']
    return h, m


def conversion_records(text, records):
    """Only generate reachable native conversions; public copy/clear is full."""
    needed = {True: set(), False: set()}
    parsed = re.findall(r'((?:(?:UA_EXPORT|UA_THREADSAFE|UA_\w+)\s+)+)UA_Server_(\w+)\s*\((.*?)\)\s*;', text, re.S)
    for modifiers, name, signature in parsed:
        if name not in OPERATIONS:
            continue
        for spelling, parameter in parameters(signature):
            typ = spelling.replace('const ', '').replace('*', '').strip()[3:]
            if typ in records:
                needed[spelling.startswith('const ')].add(typ)
        result = [r for r in modifiers.split() if r not in ('UA_EXPORT', 'UA_THREADSAFE')][0][3:]
        if result in records:
            needed[False].add(result)
    def nested(items):
        result = set()
        for spelling, name, ptr in items:
            if spelling == 'union':
                result.update(nested(ptr))
            elif spelling[3:] in records:
                result.add(spelling[3:])
        return result
    for selected in needed.values():
        while True:
            additions = set().union(*(nested(records[name]) for name in selected)) - selected
            if not additions:
                break
            selected.update(additions)
    return needed


def emit_copy(index, record, layout):
    out = [f'cpkt_opcua_StatusCode cpkt_opcua_{record}_copy(const cpkt_opcua_{record} *src, cpkt_opcua_{record} *dst) {{',
           f'  cpkt_opcua_{record} staged;', '  UA_StatusCode status = 0;',
           '  if(!src || !dst || src == dst) return UA_STATUSCODE_BADINVALIDARGUMENT;',
           '  memset(&staged, 0, sizeof(staged));']
    if record in ACTIVE:
        out += ['  staged.customStateMachine = src->customStateMachine;']
    def emit(items, path='', indent='  '):
        lines = []
        for typ, name, pointer in items:
            field = path + name
            if typ == 'union':
                discriminant, cases = UNIONS[record]
                lines += [indent + f'switch(src->{discriminant}) {{']
                for subtyp, member, ptr in pointer:
                    lines += [indent + f'case {cases[member].replace("UA_", "CPKT_OPCUA_")}:'] + emit([(subtyp, member, ptr)], field + '.', indent + '  ') + [indent + '  break;']
                lines += [indent + 'default: break;', indent + '}']
            elif pointer and typ in ('void', 'UA_PubSubSecurityPolicy'):
                lines += [indent + f'staged.{field} = src->{field};']
            elif pointer:
                count = path + name + 'Size'
                if typ[3:] in index:
                    lines += [indent + f'if(!status) status = cpkt_opcua_array_copy(src->{field}, src->{count}, (void **)&staged.{field}, &cpkt_types[{index[typ[3:]]}]);',
                              indent + f'staged.{count} = staged.{field} ? src->{count} : 0;']
                else:
                    lines += [indent + '{ size_t i;', indent + f'  if(!status && src->{count}) {{',
                              indent + f'    if(!src->{field} || src->{count} > (size_t)-1 / sizeof(*staged.{field})) status = UA_STATUSCODE_BADINVALIDARGUMENT;',
                              indent + f'    else {{ staged.{field} = (cpkt_opcua_{typ[3:]} *)UA_calloc(src->{count}, sizeof(*staged.{field}));',
                              indent + f'      if(!staged.{field}) status = UA_STATUSCODE_BADOUTOFMEMORY;',
                              indent + f'      else {{ staged.{count} = src->{count}; for(i = 0; !status && i < src->{count}; ++i) status = cpkt_opcua_{typ[3:]}_copy(&src->{field}[i], &staged.{field}[i]); }} }}',
                              indent + '  }', indent + '}']
            elif name.endswith('Size') and any(n == name[:-4] and p == '*' for t,n,p in items):
                continue
            elif typ[3:] in index:
                lines += [indent + f'if(!status) status = cpkt_opcua_type_copy(&src->{field}, &staged.{field}, &cpkt_types[{index[typ[3:]]}]);']
            elif typ == 'UA_KeyValueMap':
                lines += [indent + f'if(!status) status = cpkt_opcua_KeyValueMap_copy(&src->{field}, &staged.{field});']
            elif typ == 'UA_PublisherId' or typ[3:] in RECORDS:
                lines += [indent + f'if(!status) status = cpkt_opcua_{typ[3:]}_copy(&src->{field}, &staged.{field});']
            else:
                lines += [indent + f'staged.{field} = src->{field};']
        return lines
    out += emit(layout) + [f'  if(status) cpkt_opcua_{record}_clear(&staged); else *dst = staged;', '  return status;', '}']
    return out


def emit_publisher(index):
    h = ['/** Owned PublisherId copy. dst starts empty; failure preserves it. */',
         'cpkt_opcua_StatusCode cpkt_opcua_PublisherId_copy(const cpkt_opcua_PublisherId *src, cpkt_opcua_PublisherId *dst);',
         '/** Clear an owned ID; never clear borrowed string storage. */',
         'void cpkt_opcua_PublisherId_clear(cpkt_opcua_PublisherId *value);',
         '/** Native tagged conversion; p starts empty and owns the result. */',
         'cpkt_opcua_StatusCode cpkt_opcua_PublisherId_fromVariant(cpkt_opcua_PublisherId *p, const cpkt_opcua_Variant *src);',
         '/** Native shallow scalar view, borrowing p and its selected payload.',
         ' * dst starts empty. The native DATA flag is preserved despite borrowing:',
         ' * do not clear this view while attached to p; detach or deep-copy first. */',
         'void cpkt_opcua_PublisherId_toVariant(const cpkt_opcua_PublisherId *p, cpkt_opcua_Variant *dst);']
    m = ['void cpkt_opcua_PublisherId_clear(cpkt_opcua_PublisherId *value) {',
         '  if(!value) return;', '  if(value->idType == CPKT_OPCUA_PUBLISHERIDTYPE_STRING) cpkt_opcua_String_clear(&value->id.string);',
         '  memset(value, 0, sizeof(*value));', '}',
         'cpkt_opcua_StatusCode cpkt_opcua_PublisherId_copy(const cpkt_opcua_PublisherId *src, cpkt_opcua_PublisherId *dst) {',
         '  cpkt_opcua_PublisherId staged;', '  UA_StatusCode status = 0;',
         '  if(!src || !dst || src == dst) return UA_STATUSCODE_BADINVALIDARGUMENT;', '  staged = *src;',
         '  if(src->idType == CPKT_OPCUA_PUBLISHERIDTYPE_STRING) { staged.id.string = cpkt_opcua_String_fromChars(NULL); status = cpkt_opcua_String_copy(&src->id.string, &staged.id.string); }',
         '  if(!status) *dst = staged; else cpkt_opcua_PublisherId_clear(&staged);', '  return status;', '}',
         'cpkt_opcua_StatusCode cpkt_opcua_PublisherId_fromVariant(cpkt_opcua_PublisherId *p, const cpkt_opcua_Variant *src) {',
         '  UA_Variant native;', '  UA_PublisherId id;', '  cpkt_opcua_PublisherId staged;', '  UA_StatusCode status;',
         '  if(!p || !src) return UA_STATUSCODE_BADINVALIDARGUMENT;', '  UA_Variant_init(&native); memset(&id, 0, sizeof(id)); memset(&staged, 0, sizeof(staged));',
         f'  status = cpkt_convert(src, &native, &cpkt_types[{index["Variant"]}], 1, 0);',
         '  if(!status) status = UA_PublisherId_fromVariant(&id, &native);',
         '  if(!status) status = cpkt_ps_publisher_public(&id, &staged);',
         '  if(!status) *p = staged; else cpkt_opcua_PublisherId_clear(&staged);',
         '  UA_PublisherId_clear(&id); UA_Variant_clear(&native); return status;', '}',
         'void cpkt_opcua_PublisherId_toVariant(const cpkt_opcua_PublisherId *p, cpkt_opcua_Variant *dst) {',
         '  const void *data = NULL;', '  size_t index;', '  if(!p || !dst) return;',
         '  switch(p->idType) {']
    for arm, typ in [('byte','Byte'),('uint16','UInt16'),('uint32','UInt32'),('uint64','UInt64'),('string','String')]:
        m += [f'  case CPKT_OPCUA_PUBLISHERIDTYPE_{arm.upper()}: data = &p->id.{arm}; index = {index[typ]}; break;']
    m += ['  default: return;', '  }', '  memset(dst, 0, sizeof(*dst));', '  dst->data = (void *)data; dst->type = &cpkt_types[index]; dst->storageType = CPKT_OPCUA_VARIANT_DATA;', '}']
    return h, m


OPERATIONS = {
    'addPubSubConnection', 'getPubSubConnectionConfig', 'updatePubSubConnectionConfig',
    'addWriterGroup', 'getWriterGroupConfig', 'updateWriterGroupConfig',
    'addReaderGroup', 'getReaderGroupConfig', 'updateReaderGroupConfig',
    'addDataSetWriter', 'getDataSetWriterConfig', 'updateDataSetWriterConfig',
    'addDataSetReader', 'getDataSetReaderConfig', 'updateDataSetReaderConfig',
    'addPublishedDataSet', 'getPublishedDataSetConfig', 'addDataSetField',
    'getDataSetFieldConfig', 'addSubscribedDataSet',
    'computeWriterGroupOffsetTable', 'computeDataSetReaderOffsetTable',
}


def emit_operations(index, headers):
    text, common = source(headers)
    parsed = re.findall(r'((?:(?:UA_EXPORT|UA_THREADSAFE|UA_\w+)\s+)+)UA_Server_(\w+)\s*\((.*?)\)\s*;', text, re.S)
    if {name for modifiers, name, signature in parsed if name in OPERATIONS} != OPERATIONS:
        raise ValueError('Adapt public PubSub operation parser')
    h, m = [], []
    for modifiers, name, signature in parsed:
        if name not in OPERATIONS:
            continue
        result = [r for r in modifiers.split() if r not in ('UA_EXPORT', 'UA_THREADSAFE')]
        if len(result) != 1:
            raise ValueError('Adapt public PubSub result: ' + name)
        result = result[0]
        args = parameters(signature)
        locals_, init, convert, outputs, cleanup, invoke = [], [], [], [], [], []
        validation = ['!server', '!server->server', 'server->destroying']
        active_record = None
        outid = None
        inputid = None
        for spelling, param in args:
            if param == 'server':
                invoke += ['server->server']; continue
            typ = spelling.replace('const ', '').replace('*', '').strip()[3:]
            pointer = '*' in spelling
            output = pointer and not spelling.startswith('const ')
            if typ in RECORDS:
                locals_ += [f'  UA_{typ} native_{param};']
                init += [f'  memset(&native_{param}, 0, sizeof(native_{param}));']
                cleanup += [f'  cpkt_ps_{typ}_clear_native(&native_{param});']
                validation += ['!' + param]
                if output:
                    outputs += [f'  if(!status) status = cpkt_ps_{typ}_public(&native_{param}, &staged_{param}, cpkt_pubsub_find(server, &native_{inputid})' + ');' if inputid else f'  if(!status) status = cpkt_ps_{typ}_public(&native_{param}, &staged_{param}, NULL);']
                    locals_ += [f'  cpkt_opcua_{typ} staged_{param};']
                    init += [f'  memset(&staged_{param}, 0, sizeof(staged_{param}));']
                    outputs += [f'  if(!status) {{ *{param} = staged_{param}; memset(&staged_{param}, 0, sizeof(staged_{param})); }}']
                    cleanup += [f'  cpkt_opcua_{typ}_clear(&staged_{param});']
                else:
                    convert += [f'  if(!status) status = cpkt_ps_{typ}_native({param}, &native_{param}, entry);']
                    if typ in ACTIVE:
                        active_record = (typ, param)
                invoke += ['&native_' + param]
                continue
            if typ not in index or spelling.count('*') > 1:
                raise ValueError('Adapt public PubSub parameter: ' + name + ':' + spelling)
            locals_ += [f'  UA_{typ} native_{param};']
            init += [f'  memset(&native_{param}, 0, sizeof(native_{param}));']
            cleanup += [f'  UA_{typ}_clear(&native_{param});']
            if output:
                outid = param
                locals_ += [f'  cpkt_opcua_{typ} staged_{param};']
                init += [f'  memset(&staged_{param}, 0, sizeof(staged_{param}));']
                outputs += [f'  if(!status && {param}) status = cpkt_convert(&native_{param}, &staged_{param}, &cpkt_types[{index[typ]}], 0, 0);',
                            f'  if(!status && {param}) {{ *{param} = staged_{param}; memset(&staged_{param}, 0, sizeof(staged_{param})); }}']
                cleanup += [f'  cpkt_opcua_{typ}_clear(&staged_{param});']
            else:
                inputid = param
                if pointer: validation += ['!' + param]
                convert += [f'  if(!status) status = cpkt_convert({param if pointer else "&" + param}, &native_{param}, &cpkt_types[{index[typ]}], 1, 0);']
            invoke += ['&native_' + param if pointer else 'native_' + param]
        public = translate(signature)
        if result != 'UA_StatusCode':
            typ = result[3:]
            if typ not in RECORDS and typ != 'DataSetFieldResult':
                raise ValueError('Adapt public PubSub result record')
            public += f', cpkt_opcua_{typ} *response'
            validation += ['!response']
            locals_ += [f'  UA_{typ} native_response;', f'  cpkt_opcua_{typ} staged_response;']
            init += ['  memset(&native_response, 0, sizeof(native_response));', '  memset(&staged_response, 0, sizeof(staged_response));']
            if typ == 'DataSetFieldResult':
                outputs += ['  if(!status) status = cpkt_result_DataSetFieldResult(&native_response, &staged_response);']
            else:
                outputs += [f'  if(!status) status = cpkt_ps_{typ}_public(&native_response, &staged_response, NULL);']
                cleanup += [f'  cpkt_ps_{typ}_clear_native(&native_response);', f'  cpkt_opcua_{typ}_clear(&staged_response);']
            outputs += ['  if(!status) { *response = staged_response; memset(&staged_response, 0, sizeof(staged_response)); }']
        target = 'cpkt_opcua_server_' + name + '_typed'
        h += [f'/** Invoke native UA_Server_{name} with full public C89 configs.',
              ' * Inputs borrow through return; output configs/IDs/results start empty and',
              ' * own their schema fields. Contexts and policies remain borrowed. Native',
              ' * result-record status fields are separate from conversion status. Native',
              ' * scheduling and state changes are preserved; serialize server calls.',
              ' * Conversion failure after creation may leave the native component present;',
              ' * lifecycle callbacks observe its ID and permit explicit removal. */',
              f'cpkt_opcua_StatusCode {target}({public});']
        m += [f'cpkt_opcua_StatusCode {target}({public}) {{',
              ('  UA_StatusCode status = 0, native_status = 0;' if result == 'UA_StatusCode' else '  UA_StatusCode status = 0;'),
              '  int prepared = 0;',
              *(['  cpkt_pubsub_entry *entry = NULL;'] if any('entry)' in line for line in convert) else []), *locals_,
              f'  if({" || ".join(validation)}) return UA_STATUSCODE_BADINVALIDARGUMENT;', *init]
        # Convert NodeIds before staging scoped custom-state dispatch.
        schema_conversion = [line for line in convert if 'cpkt_convert(' in line]
        config_conversion = [line for line in convert if 'cpkt_convert(' not in line]
        m += schema_conversion
        if active_record:
            typ, param = active_record
            updating = name.startswith('update')
            m += [f'  if(!status) status = cpkt_pubsub_stage(server, UA_PUBSUBCOMPONENT_{ACTIVE[typ]}, {"&native_" + inputid if updating else "NULL"}, {param}->customStateMachine, &entry);']
        m += config_conversion
        m += ['  if(!status && server->typed_pubsub_prepare) { status = server->typed_pubsub_prepare(server); prepared = !status; }']
        m += ['  if(!status) {', f'    {"native_status" if result == "UA_StatusCode" else "native_response"} = UA_Server_{name}({", ".join(invoke)});', '  }']
        if active_record:
            m += [f'  cpkt_pubsub_finish(entry, !status && !native_status, {"&native_" + outid if outid else "NULL"});']
        if result == 'UA_StatusCode':
            m += ['  if(!status) status = native_status;']
        m += outputs + cleanup + ['  if(prepared) server->typed_pubsub_finish(server);', '  return status;', '}']
    return h, m
