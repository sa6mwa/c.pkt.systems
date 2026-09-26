"""C89 backend for open62541's parsed public type graph (no schema parser).

The upstream CGenerator owns filtering, type ordering and declarations. This
backend changes representation and emits offsets for a single conversion engine.
"""
from pathlib import Path
import re
from nodeset_compiler.type_parser import BuiltinType, EnumerationType, OpaqueType, StructType


def emit(generator, output, client_header):
    types = [t for ns in generator.filtered_types.values() for t in ns.values()]
    index = {t.name: i for i, t in enumerate(types)}
    if len(index) != len(types):
        raise ValueError("C89 backend requires unique public type names")
    header = ['/* Generated from the upstream public schema. Do not edit. */',
              '/* Schema: Copyright OPC Foundation, MIT License 1.00; see THIRD_PARTY_NOTICES. */',
              '#ifndef CPKT_OPCUA_TYPES_H', '#define CPKT_OPCUA_TYPES_H',
              '#include <cpkt/opcua_types_base.h>', '#ifdef __cplusplus',
              'extern "C" {', '#endif',
              f'#define CPKT_OPCUA_TYPES_COUNT {len(types)}']
    metadata = ['/* Generated conversion metadata. Do not edit. */',
                'typedef char cpkt_type_count_matches[(CPKT_OPCUA_TYPES_COUNT == UA_TYPES_COUNT) ? 1 : -1];']
    kinds = {name: i for i, name in enumerate([
        'Boolean', 'SByte', 'Byte', 'Int16', 'UInt16', 'Int32', 'UInt32', 'Int64',
        'UInt64', 'Float', 'Double', 'String', 'DateTime', 'Guid', 'ByteString',
        'XmlElement', 'NodeId', 'ExpandedNodeId', 'StatusCode', 'QualifiedName',
        'LocalizedText', 'ExtensionObject', 'DataValue', 'Variant', 'DiagnosticInfo'])}
    for i, t in enumerate(types):
        if not isinstance(t, BuiltinType):
            description = t.description.strip().replace('*/', '* /')
            header.append(f'/** {t.name}. {description} */')
        if isinstance(t, BuiltinType):
            kind = kinds[t.name]  # fail early for a new unsupported builtin
        elif isinstance(t, EnumerationType):
            kind = kinds[t.strDataType.replace('UA_', '', 1)] if t.isOptionSet else 26
            if t.isOptionSet and t.strDataType == 'UA_UInt64':
                header.append(f'typedef cpkt_opcua_UInt64 cpkt_opcua_{t.name};')
                for key, value in t.elements.items():
                    number = int(value, 0)
                    header.append(f'#define cpkt_opcua_{t.name.upper()}_{key.upper()} '
                                  f'{{0x{number >> 32:08x}U, 0x{number & 0xffffffff:08x}U}}')
            else:
                declaration = generator.print_enum_typedef(t)
                declaration = re.sub(r'\nUA_STATIC_ASSERT\([^\n]+\);', '', declaration)
                header.append(declaration.replace('__UA_', 'UA_').replace('UA_', 'cpkt_opcua_'))
        elif isinstance(t, OpaqueType):
            kind = kinds[t.base_type]
            header.append(generator.print_datatype_typedef(t).replace('UA_', 'cpkt_opcua_'))
        elif isinstance(t, StructType):
            kind = 29 if t.is_union else 27
            declaration = generator.print_struct_typedef(t)
            declaration = re.sub(r'\nUA_STATIC_ASSERT\([^\n]+\);', '', declaration)
            declaration = declaration.replace('\n\ntypedef', '\n\n/** Selected payload of the schema union. */\ntypedef')
            header.append(declaration.replace('__UA_', 'UA_').replace('UA_', 'cpkt_opcua_'))
            metadata.append(f'static const cpkt_opcua_type_member members_{i}[] = {{')
            for m in t.members:
                name = m.name if m.name not in ('float', 'double', 'int', 'char') else '_' + m.name
                typename = m.member_type.name
                if isinstance(m.member_type, StructType) and not m.member_type.members:
                    typename = 'ExtensionObject'
                if typename not in index:
                    raise ValueError(f'{t.name}.{name}: missing public type {typename}')
                path = f'fields.{name}' if t.is_union else name
                length = f'{path}.{name}Size' if t.is_union else f'{name}Size'
                if t.is_union and m.is_array:
                    path += f'.{name}'
                offsets = [f'offsetof({prefix}{t.name}, {path})' for prefix in ('cpkt_opcua_', 'UA_')]
                offsets += [f'offsetof({prefix}{t.name}, {length})' if m.is_array else '0'
                            for prefix in ('cpkt_opcua_', 'UA_')]
                metadata.append('  {' + ', '.join(offsets + [str(index[typename]),
                                str(int(m.is_array)), str(int(bool(m.is_optional)))]) + '},')
            metadata.append('};')
        else:
            raise ValueError(f'Unsupported public schema class {type(t).__name__}')
        header.append(f'#define CPKT_OPCUA_TYPES_{t.name.upper()} {i}')
        metadata.append(f'typedef char type_index_{i}[(UA_TYPES_{t.name.upper()} == {i}) ? 1 : -1];')
        for action, params, args in [('init', 'p', 'p'), ('new', '', ''),
                                     ('copy', 's, d', 's, d'), ('clear', 'p', 'p'),
                                     ('delete', 'p', 'p'), ('equal', 'a, b', 'a, b')]:
            callargs = ', '.join('(' + arg.strip() + ')' for arg in args.split(',')) + ', ' if args else ''
            cast = f'(cpkt_opcua_{t.name} *)' if action == 'new' else ''
            expression = f'{cast}cpkt_opcua_type_{action}({callargs}cpkt_opcua_type_at({i}))'
            if cast:
                expression = '(' + expression + ')'
            header.append(f'#define cpkt_opcua_{t.name}_{action}({params}) {expression}')
    metadata.append('static const cpkt_opcua_Type cpkt_types[] = {')
    for i, t in enumerate(types):
        kind = (kinds[t.name] if isinstance(t, BuiltinType) else
                kinds[t.base_type] if isinstance(t, OpaqueType) else
                (kinds[t.strDataType.replace('UA_', '', 1)] if t.isOptionSet else 26)
                if isinstance(t, EnumerationType) else 29 if t.is_union else 27)
        members = f'members_{i}' if isinstance(t, StructType) else 'NULL'
        size = len(t.members) if isinstance(t, StructType) else 0
        metadata.append(f'  {{"{t.name}", sizeof(cpkt_opcua_{t.name}), &UA_TYPES[{i}], '
                        f'{kind}, {size}, {members}}},')
    metadata.append('};')
    services = []
    if client_header:
        text = Path(client_header).read_text()
        services = re.findall(r'UA_(\w+Response)\s+UA_EXPORT\s+UA_THREADSAFE\s+'
                              r'UA_Client_Service_(\w+)\(UA_Client \*client,\s*'
                              r'const UA_(\w+Request) req\)', text)
    declared = set(re.findall(r'UA_Client_Service_(\w+)\s*\(', text))
    parsed = {name for _, name, _ in services}
    if declared != parsed:
        raise ValueError(f'Adapt C89 emitter to changed public service signatures: {declared - parsed}')
    configuration = Path(client_header).with_name('config.h').read_text()
    query_enabled = re.search(r'^#define UA_ENABLE_QUERY\b', configuration, re.M) is not None
    for response, name, request in services:
        if name.startswith('query') and not query_enabled:
            continue
        if request not in index or response not in index:
            raise ValueError(f'Public service {name} has missing schema types')
        header.extend([f'/** Call upstream {name}; output is owned and must start empty. '
                       'Check responseHeader.serviceResult and per-operation status. */',
                       f'cpkt_opcua_StatusCode cpkt_opcua_client_service_{name}('
                       f'cpkt_opcua_client *client, const cpkt_opcua_{request} *request, '
                       f'cpkt_opcua_{response} *response);'])
        metadata.append(f'static void service_{name}(UA_Client *client, const void *request, void *response) {{\n'
                        f'  *(UA_{response} *)response = UA_Client_Service_{name}(client, *(const UA_{request} *)request);\n}}\n'
                        f'cpkt_opcua_StatusCode cpkt_opcua_client_service_{name}('
                        f'cpkt_opcua_client *client, const cpkt_opcua_{request} *request, '
                        f'cpkt_opcua_{response} *response) {{\n'
                        f'  return cpkt_typed_service(client, request, response, '
                        f'&cpkt_types[{index[request]}], &cpkt_types[{index[response]}], service_{name});\n}}')
    header.extend([
        '/** Read all public DataValue fields through the native server read API. */',
        'cpkt_opcua_StatusCode cpkt_opcua_server_read_typed(cpkt_opcua_server *server, const cpkt_opcua_ReadValueId *request, cpkt_opcua_TimestampsToReturn timestamps, cpkt_opcua_DataValue *response);',
        '/** Write any generated scalar, array or nested public value. */',
        'cpkt_opcua_StatusCode cpkt_opcua_server_write_typed(cpkt_opcua_server *server, const cpkt_opcua_WriteValue *request);',
        '#ifdef __cplusplus', '}', '#endif', '#endif', ''])
    root = Path(output)
    root.mkdir(parents=True, exist_ok=True)
    (root / 'cpkt' / 'opcua_types.h').parent.mkdir(parents=True, exist_ok=True)
    (root / 'cpkt' / 'opcua_types.h').write_text('\n'.join(header))
    (root / 'opcua_types_metadata.inc').write_text('\n'.join(metadata) + '\n')
