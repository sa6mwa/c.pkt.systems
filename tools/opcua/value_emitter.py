"""Native public value factories, preserving borrowed and allocated storage."""
import re
from plugin_emitter import parameters, translate, uncomment

FACTORIES = {
    'STRING', 'GUID', 'NODEID', 'NODEID_NUMERIC', 'NODEID_STRING',
    'NODEID_STRING_ALLOC', 'NODEID_GUID', 'NODEID_BYTESTRING',
    'NODEID_BYTESTRING_ALLOC', 'EXPANDEDNODEID', 'EXPANDEDNODEID_NUMERIC',
    'EXPANDEDNODEID_STRING', 'EXPANDEDNODEID_STRING_ALLOC',
    'EXPANDEDNODEID_STRING_GUID', 'EXPANDEDNODEID_BYTESTRING',
    'EXPANDEDNODEID_BYTESTRING_ALLOC', 'EXPANDEDNODEID_NODEID',
    'QUALIFIEDNAME', 'QUALIFIEDNAME_ALLOC', 'LOCALIZEDTEXT',
    'LOCALIZEDTEXT_ALLOC', 'NUMERICRANGE',
}
RESULTS = {
    'String': ['  cpkt_opcua_String out;', '  cpkt_string_take(&out, native);', '  return out;'],
    'Guid': ['  cpkt_opcua_Guid out;', '  memcpy(&out, &native, sizeof(out));', '  return out;'],
    'NodeId': ['  return cpkt_nodeid_take(native);'],
    'ExpandedNodeId': ['  return cpkt_expanded_take(native);'],
    'QualifiedName': ['  cpkt_opcua_QualifiedName out;', '  out.namespaceIndex = native.namespaceIndex;',
                      '  cpkt_string_take(&out.name, native.name);', '  return out;'],
    'LocalizedText': ['  cpkt_opcua_LocalizedText out;', '  cpkt_string_take(&out.locale, native.locale);',
                      '  cpkt_string_take(&out.text, native.text);', '  return out;'],
    'NumericRange': ['  return cpkt_range_take(native);'],
}


def declarations(text):
    found = re.findall(r'UA_EXPORT\s+UA_(\w+)\s+UA_(\w+)\s*\((.*?)\)\s*;', uncomment(text), re.S)
    selected = [(r, n, s) for r, n, s in found if n in FACTORIES]
    if {n for _, n, _ in selected} != FACTORIES:
        raise ValueError('Adapt native public value factory declarations')
    return selected


def emit_values(native_headers):
    text = (native_headers / 'types.h').read_text()
    header = [re.match(r'/\*.*?\*/', text, re.S)[0]]
    metadata = []
    for name, typename, initializer in (
        ('STRING_NULL', 'String', '{0, NULL}'),
        ('BYTESTRING_NULL', 'ByteString', '{0, NULL}'),
        ('GUID_NULL', 'Guid', '{0, 0, 0, {0,0,0,0,0,0,0,0}}'),
        ('NODEID_NULL', 'NodeId', '{0, CPKT_OPCUA_NODEIDTYPE_NUMERIC, {0}}'),
        ('EXPANDEDNODEID_NULL', 'ExpandedNodeId', '{{0, CPKT_OPCUA_NODEIDTYPE_NUMERIC, {0}}, {0, NULL}, 0}'),
    ):
        if not re.search(r'extern\s+const\s+UA_' + typename + r'\s+UA_' + name + r'\s*;', text):
            raise ValueError('Adapt native null constant: ' + name)
        header += ['/** Native null value; const static storage, never clear/delete it. */',
                   f'extern const cpkt_opcua_{typename} cpkt_opcua_{name};']
        metadata += [f'const cpkt_opcua_{typename} cpkt_opcua_{name} = {initializer};']
    for result, name, signature in declarations(text):
        if result not in RESULTS:
            raise ValueError('Adapt native factory result: ' + name)
        args, native_args, locals_ = parameters(signature), [], []
        for spelling, arg in args:
            if spelling in ('char *', 'const char *', 'UA_UInt16', 'UA_UInt32'):
                native_args += [arg]
            elif spelling == 'UA_Guid':
                locals_ += [f'  UA_Guid native_{arg};']
                native_args += [f'native_{arg}']
            elif spelling == 'UA_NodeId':
                native_args += [f'cpkt_nodeid_view(&{arg})']
            else:
                raise ValueError('Adapt native factory argument: ' + name + '.' + arg)
        if name == 'NUMERICRANGE':
            native_args = [f'{args[0][1]} ? {args[0][1]} : ""']
        owned = name.endswith('_ALLOC') or name in ('NODEID', 'EXPANDEDNODEID', 'NUMERICRANGE')
        description = ('Owned result: clear with the corresponding type clear (NumericRange_clear',
                       'for ranges). Native parse/allocation failure may return an empty or partial',
                       'result; clear it even on failure. No extra facade allocation is added.') if owned else (
                       'Borrowed result: bytes and nested NodeIds retain their original address.',
                       'Keep input storage alive; do not clear borrowed results. Scalar numeric',
                       'IDs and GUIDs contain no owned allocation.')
        if name == 'NUMERICRANGE':
            description += ('NULL range text is treated as an empty string (empty result).',)
        elif result == 'Guid' or name.endswith('_NUMERIC') or name.endswith('_GUID'):
            description = ('Scalar result with no retained input storage or owned allocation.',
                           'Native parse/default behavior and every scalar bit are preserved.')
        header += [f'/** Invoke native UA_{name}; names and native factory behavior are preserved.',
                   *[' * ' + line for line in description], ' */',
                   f'cpkt_opcua_{result} cpkt_opcua_{name}({translate(signature)});']
        result_lines = RESULTS[result]
        declarations_ = [line for line in result_lines if line.startswith('  cpkt_opcua_')]
        statements = [line for line in result_lines if line not in declarations_]
        metadata += [f'cpkt_opcua_{result} cpkt_opcua_{name}({translate(signature)}) {{',
                     f'  UA_{result} native;', *locals_, *declarations_]
        for spelling, arg in args:
            if spelling == 'UA_Guid':
                metadata += [f'  memcpy(&native_{arg}, &{arg}, sizeof(native_{arg}));']
        metadata += [f'  native = UA_{name}({", ".join(native_args)});', *statements, '}']
    header += [
        '/** Borrow bytes exactly as STRING does; no copy or extra terminator. */',
        '#define cpkt_opcua_BYTESTRING(CHARS) cpkt_opcua_STRING(CHARS)',
        '/** Native allocated-string shorthand; clear the owned result. */',
        '#define cpkt_opcua_STRING_ALLOC(CHARS) cpkt_opcua_String_fromChars(CHARS)',
        '/** Native allocated-byte-string shorthand; clear the owned result. */',
        '#define cpkt_opcua_BYTESTRING_ALLOC(CHARS) cpkt_opcua_String_fromChars(CHARS)',
        '/** C89 static initializer for a literal/array; borrows its bytes. */',
        '#define cpkt_opcua_STRING_STATIC(CHARS) {sizeof(CHARS)-1, (cpkt_opcua_Byte*)(CHARS)}',
        '/** Native namespace-zero numeric NodeId shorthand. */',
        '#define cpkt_opcua_NS0ID(ID) cpkt_opcua_NODEID_NUMERIC(0, CPKT_OPCUA_NS0ID_##ID)',
        '/** Native namespace-zero ExpandedNodeId shorthand. */',
        '#define cpkt_opcua_NS0EXID(ID) cpkt_opcua_EXPANDEDNODEID_NUMERIC(0, CPKT_OPCUA_NS0ID_##ID)',
        '/** Native shallow NodeId promotion; nested bytes remain borrowed. */',
        '#define cpkt_opcua_NODEID2EXPANDEDNODEID(ID) cpkt_opcua_EXPANDEDNODEID_NODEID(ID)',
        '/** Parse the counted native range into an empty owned output. Failure',
        ' * preserves output; success replaces it without clearing a previous value.',
        ' * All dimensions and full UInt32 endpoints are retained. */',
        'cpkt_opcua_StatusCode cpkt_opcua_NumericRange_parse(cpkt_opcua_NumericRange *range, cpkt_opcua_String text);',
        '/** Release owned range dimensions, reset fields; NULL is harmless. */',
        'void cpkt_opcua_NumericRange_clear(cpkt_opcua_NumericRange *range);',
        '/** Native endpoint parser. Output strings borrow slices of input bytes;',
        ' * never clear them. Port/path and partial outputs retain native behavior',
        ' * on success/failure; path is optional. Input, hostname and port are required.',
        ' * Initialize outputs before calling. No copy, allocation or URL rewrite. */',
        'cpkt_opcua_StatusCode cpkt_opcua_parseEndpointUrl(const cpkt_opcua_String *url, cpkt_opcua_String *hostname, cpkt_opcua_UInt16 *port, cpkt_opcua_String *path);',
        '/** Native Ethernet endpoint parser; borrowed target and native VLAN/PCP',
        ' * validation/partial outputs. All arguments are required and initialized.',
        ' * Parsing does not imply Ethernet transport availability on the platform. */',
        'cpkt_opcua_StatusCode cpkt_opcua_parseEndpointUrlEthernet(const cpkt_opcua_String *url, cpkt_opcua_String *target, cpkt_opcua_UInt16 *vid, cpkt_opcua_Byte *pcp);',
        '/** Native decimal reader: return consumed bytes, retain native UInt32',
        ' * wraparound, no locale or signed-number parsing. Buffer/number required;',
        ' * invalid NULL returns zero without changing number. No allocation. */',
        'size_t cpkt_opcua_readNumber(const cpkt_opcua_Byte *buffer, size_t length, cpkt_opcua_UInt32 *number);',
        '/** Native base reader, including its digit/overflow rules. Buffer and',
        ' * number required; invalid NULL returns zero and preserves number. */',
        'size_t cpkt_opcua_readNumberWithBase(const cpkt_opcua_Byte *buffer, size_t length, cpkt_opcua_UInt32 *number, cpkt_opcua_Byte base);',
        '/** Invoke native constant-time byte comparison. Both buffers must contain',
        ' * length bytes; NULL is allowed only for zero length, otherwise false.',
        ' * The bridge does not substitute memcmp or short-circuit byte differences. */',
        'cpkt_opcua_Boolean cpkt_opcua_constantTimeEqual(const void *first, const void *second, size_t length);',
    ]
    return header, metadata
