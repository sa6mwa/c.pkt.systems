"""Certificate and trust-list boundaries derived from public declarations."""
import re
from plugin_emitter import parameters, translate, uncomment
from core_client_emitter import enum_declaration

CERTIFICATES = {
    'verifyApplicationUri', 'getExpirationDate', 'getSubjectName', 'getThumbprint',
    'getKeySize', 'comparePublicKeys', 'checkKeyPair', 'checkCA', 'decryptPrivateKey',
}
TRUST = {'add', 'set', 'remove', 'contains', 'getSize'}


def declarations(headers):
    text = '\n'.join((headers / name).read_text() for name in (
        'plugin/certificategroup.h', 'plugin/create_certificate.h', 'util.h'))
    found = re.findall(r'((?:(?:UA_EXPORT|UA_StatusCode|UA_Boolean|UA_UInt32)\s+)+)'
                       r'(UA_(?:CertificateUtils_\w+|CreateCertificate|TrustListDataType_\w+))\s*\((.*?)\)\s*;',
                       uncomment(text), re.S)
    expected = {'UA_CertificateUtils_' + name for name in CERTIFICATES}
    expected |= {'UA_TrustListDataType_' + name for name in TRUST} | {'UA_CreateCertificate'}
    if {name for _, name, _ in found} != expected:
        raise ValueError('Adapt certificate/trust public declarations')
    return found


def emit_certificates(index, headers):
    declaration, constants = enum_declaration((headers / 'plugin/create_certificate.h').read_text(), 'CertificateFormat')
    header = [re.match(r'/\*.*?\*/', (headers / 'plugin/certificategroup.h').read_text(), re.S)[0],
              '/** Native certificate output encoding. */', declaration]
    metadata = [f'typedef char certificate_{name}[((int){name} == (int){name.replace("UA_", "CPKT_OPCUA_")}) ? 1 : -1];' for name in constants]
    for prefix, name, signature in declarations(headers):
        result = [word for word in prefix.split() if word != 'UA_EXPORT']
        expected = 'UA_Boolean' if name.endswith('_contains') else 'UA_UInt32' if name.endswith('_getSize') else 'UA_StatusCode'
        if result != [expected]:
            raise ValueError('Adapt certificate/trust return: ' + name)
        args = parameters(signature)
        public = translate(signature).replace('const cpkt_opcua_Logger *', 'const cpkt_opcua_log_config *')
        target = name.replace('UA_', 'cpkt_opcua_', 1)
        if name.startswith('UA_CertificateUtils_'):
            h, m = emit_inspection(index, name, args, public, target)
        elif name == 'UA_CreateCertificate':
            h, m = emit_creation(index, name, args, public, target)
        else:
            h, m = emit_trust(index, name, args, public, target, expected)
        header += h
        metadata += m
    return header, metadata


def emit_inspection(index, name, args, public, target):
    outputs = {
        'UA_CertificateUtils_getExpirationDate': ('expiryDateTime', 'DateTime'),
        'UA_CertificateUtils_getSubjectName': ('subjectName', 'String'),
        'UA_CertificateUtils_getThumbprint': ('thumbprint', 'String'),
        'UA_CertificateUtils_getKeySize': ('keySize', 'size_t'),
        'UA_CertificateUtils_decryptPrivateKey': ('outDerKey', 'ByteString'),
    }
    output = outputs.get(name)
    declarations_, setup, validation, call, commit = [], [], [], [], []
    for spelling, param in args:
        if output and param == output[0]:
            typename = output[1]
            if spelling != ('size_t *' if typename == 'size_t' else 'UA_' + typename + ' *'):
                raise ValueError('Adapt certificate output: ' + name)
            validation.append('!' + param)
            declarations_.append(f'  {"size_t" if typename == "size_t" else "UA_" + typename} n_{param};')
            if name.endswith('_getThumbprint'):
                setup.append(f'  n_{param} = cpkt_string_view({param});')
                validation.append(f'({param} && {param}->length && !{param}->data)')
            else:
                setup.append(f'  memset(&n_{param}, 0, sizeof(n_{param}));')
            call.append('&n_' + param)
            if typename == 'size_t':
                commit.append(f'  if(!status) *{param} = n_{param};')
            elif typename == 'DateTime':
                commit.append(f'  if(!status) status = cpkt_convert(&n_{param}, {param}, &cpkt_types[{index[typename]}], 0, 0);')
            elif name.endswith('_getThumbprint'):
                commit.append(f'  {param}->length = n_{param}.length; {param}->data = n_{param}.data;')
            else:
                commit.append(f'  if(!status || n_{param}.data) cpkt_string_take({param}, n_{param});')
        elif spelling in ('const UA_ByteString *', 'UA_ByteString *', 'const UA_String *'):
            declarations_.append(f'  UA_ByteString n_{param};')
            setup.append(f'  n_{param} = cpkt_string_view({param});')
            validation += ['!' + param, f'({param} && {param}->length && !{param}->data)']
            call.append('&n_' + param)
        elif spelling == 'const UA_ByteString':
            declarations_.append(f'  UA_ByteString n_{param};')
            setup.append(f'  n_{param} = cpkt_string_view(&{param});')
            validation.append(f'({param}.length && !{param}.data)')
            call.append('n_' + param)
        else:
            raise ValueError('Adapt certificate argument: ' + name + '.' + param)
    if output and output[0] not in {param for _, param in args}:
        raise ValueError('Missing certificate output: ' + name)
    h = [f'/** Invoke native {name}. Counted byte inputs borrow until return.',
         ' * Required pointers and nonempty byte buffers must be valid.',
         ' * Owned String/ByteString outputs start empty; clear them after use.',
         ' * Scalar outputs remain unchanged on native failure. Thumbprint requires',
         ' * the native 40-byte writable buffer and retains its address/ownership.',
         ' * Uses the bundled native certificate implementation and status codes. */',
         f'cpkt_opcua_StatusCode {target}({public});']
    m = [f'cpkt_opcua_StatusCode {target}({public}) {{', '  UA_StatusCode status;',
         *declarations_, *setup, f'  if({" || ".join(validation)}) return UA_STATUSCODE_BADINVALIDARGUMENT;',
         f'  status = {name}({", ".join(call)});', *commit, '  return status;', '}']
    return h, m


def emit_creation(index, name, args, public, target):
    expected = [('const UA_Logger *', 'logger'), ('const UA_String *', 'subject'),
                ('size_t', 'subjectSize'), ('const UA_String *', 'subjectAltName'),
                ('size_t', 'subjectAltNameSize'), ('UA_CertificateFormat', 'certFormat'),
                ('UA_KeyValueMap *', 'params'), ('UA_ByteString *', 'outPrivateKey'),
                ('UA_ByteString *', 'outCertificate')]
    if args != expected:
        raise ValueError('Adapt certificate creation signature')
    h = [f'/** Invoke native {name}, including native key/curve/expiry parameters.',
         ' * Subject/name arrays and parameter map borrow until return.',
         ' * Outputs must be distinct empty ByteStrings; native allocations transfer',
         ' * directly, including any partial native output on failure. Clear both.',
         ' * Logger configuration is copied for this synchronous call only; user',
         ' * data borrows through return. NULL deliberately silences logging.',
         ' * Parameters remain caller-owned; the native implementation reads them. */',
         f'cpkt_opcua_StatusCode {target}({public});']
    m = [f'cpkt_opcua_StatusCode {target}({public}) {{',
         '  struct cpkt_opcua_logger bridge;', '  cpkt_opcua_log_config quiet;',
         '  UA_KeyValueMap map;', '  void *n_subject = NULL, *n_names = NULL, *n_pairs = NULL;',
         '  UA_ByteString key = UA_BYTESTRING_NULL, certificate = UA_BYTESTRING_NULL;',
         '  UA_StatusCode status;', '  memset(&bridge, 0, sizeof(bridge));',
         '  memset(&quiet, 0, sizeof(quiet));', '  memset(&map, 0, sizeof(map));',
         '  if(!outPrivateKey || !outCertificate || outPrivateKey == outCertificate || !cpkt_logger_valid(logger)) return UA_STATUSCODE_BADINVALIDARGUMENT;',
         '  cpkt_logger_set(&bridge, &bridge.native, logger ? logger : &quiet);',
         f'  status = cpkt_array(subject, subjectSize, &n_subject, &cpkt_types[{index["String"]}], 1, 0);',
         f'  if(!status) status = cpkt_array(subjectAltName, subjectAltNameSize, &n_names, &cpkt_types[{index["String"]}], 1, 0);',
         f'  if(!status && params) status = cpkt_array(params->map, params->mapSize, &n_pairs, &cpkt_types[{index["KeyValuePair"]}], 1, 0);',
         '  map.map = (UA_KeyValuePair *)n_pairs; map.mapSize = n_pairs && params ? params->mapSize : 0;',
         '  if(!status) {',
         '    status = UA_CreateCertificate(&bridge.native, (const UA_String *)n_subject, subjectSize, (const UA_String *)n_names, subjectAltNameSize, (UA_CertificateFormat)certFormat, params ? &map : NULL, &key, &certificate);',
         '    cpkt_string_take(outPrivateKey, key); cpkt_string_take(outCertificate, certificate); }',
         f'  if(n_subject) UA_Array_delete(n_subject, subjectSize, &UA_TYPES[UA_TYPES_STRING]);',
         f'  if(n_names) UA_Array_delete(n_names, subjectAltNameSize, &UA_TYPES[UA_TYPES_STRING]);',
         '  UA_KeyValueMap_clear(&map);',
         '  return status;', '}']
    return h, m


def emit_trust(index, name, args, public, target, result):
    action = name.rsplit('_', 1)[1]
    signatures = {
        'add': [('const UA_TrustListDataType *', 'src'), ('UA_TrustListDataType *', 'dst')],
        'set': [('const UA_TrustListDataType *', 'src'), ('UA_TrustListDataType *', 'dst')],
        'remove': [('const UA_TrustListDataType *', 'src'), ('UA_TrustListDataType *', 'dst')],
        'contains': [('const UA_TrustListDataType *', 'trustList'), ('const UA_ByteString *', 'certificate'), ('UA_TrustListMasks', 'mask')],
        'getSize': [('const UA_TrustListDataType *', 'trustList')],
    }
    if args != signatures[action]:
        raise ValueError('Adapt trust-list signature: ' + name)
    h = [f'/** Invoke native {name} with C89 trust-list storage.',
         ' * Mutation preserves native selection, duplicate and partial-result rules.',
         ' * Conversion failure before the call or while staging output retains dst.',
         ' * Successful output conversion commits native partial results even when',
         ' * native status reports failure. Source/destination root alias is preserved.',
         ' * Read-only queries allocate nothing and use bounded native byte views.',
         ' * Release owned destination records with TrustListDataType_clear. */',
         f'{translate(result)} {target}({public});']
    if action in ('add', 'set', 'remove'):
        kind = index['TrustListDataType']
        m = [f'cpkt_opcua_StatusCode {target}({public}) {{',
             '  UA_TrustListDataType source, destination;', '  cpkt_opcua_TrustListDataType staged;',
             '  UA_StatusCode status, native_status = 0;', '  UA_TrustListDataType_init(&source); UA_TrustListDataType_init(&destination);',
             '  cpkt_opcua_TrustListDataType_init(&staged);',
             f'  if(!dst{" || !src" if action == "set" else ""}) return UA_STATUSCODE_BADINVALIDARGUMENT;',
             *([f'  if(!src) return {name}(NULL, &destination);'] if action != 'set' else []),
             f'  status = cpkt_convert(dst, &destination, &cpkt_types[{kind}], 1, 0);',
             f'  if(!status && src != dst) status = cpkt_convert(src, &source, &cpkt_types[{kind}], 1, 0);',
             f'  if(!status) {{ native_status = {name}(src == dst ? &destination : &source, &destination);',
             f'    status = cpkt_convert(&destination, &staged, &cpkt_types[{kind}], 0, 0);',
             '    if(!status) { cpkt_opcua_TrustListDataType_clear(dst); *dst = staged; cpkt_opcua_TrustListDataType_init(&staged); } }',
             '  UA_TrustListDataType_clear(&source); UA_TrustListDataType_clear(&destination);',
             '  cpkt_opcua_TrustListDataType_clear(&staged);', '  return status ? status : native_status;', '}']
        return h, m
    fields = [('trustedCertificates', 'TRUSTEDCERTIFICATES'), ('trustedCrls', 'TRUSTEDCRLS'),
              ('issuerCertificates', 'ISSUERCERTIFICATES'), ('issuerCrls', 'ISSUERCRLS')]
    m = [f'{translate(result)} {target}({public}) {{', '  UA_TrustListDataType view;',
         '  UA_ByteString bytes[16];', '  size_t offset, count, i;',
         *(['  UA_ByteString needle;', '  if(!trustList || !certificate) return 0;', '  needle = cpkt_string_view(certificate);'] if action == 'contains' else ['  cpkt_opcua_UInt32 total = 0;', '  if(!trustList) return 0;'])]
    for field, mask in fields:
        m += [f'  for(offset = 0; offset < trustList->{field}Size; offset += count) {{',
              *([f'    if(!(mask & cpkt_opcua_TRUSTLISTMASKS_{mask})) break;'] if action == 'contains' else []),
              f'    count = trustList->{field}Size - offset; if(count > 16) count = 16;',
              f'    for(i = 0; i < count; ++i) bytes[i] = cpkt_string_view(&trustList->{field}[offset + i]);',
              '    UA_TrustListDataType_init(&view);', f'    view.{field} = bytes; view.{field}Size = count;',
              f'    {"if(UA_TrustListDataType_contains(&view, &needle, (UA_TrustListMasks)mask)) return 1;" if action == "contains" else "total += UA_TrustListDataType_getSize(&view);"}', '  }']
    m += ['  return 0;' if action == 'contains' else '  return total;', '}']
    return h, m
