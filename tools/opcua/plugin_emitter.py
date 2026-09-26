"""Generate callback records and trampolines from handwritten public plugins.

Upstream has no generator for these records. Declarations remain authoritative;
the small binding policy below identifies context and ownership boundaries, not
a second copy of OPC UA schema types. Unsupported signatures fail generation.
"""
from pathlib import Path
import re

CALLBACK_PATTERN = re.compile(r'(void|size_t|UA_\w+|const UA_\w+\s*\*)\s*'
                              r'\(\*(\w+)\)\s*\((.*?)\)\s*;', re.S)


def uncomment(text):
    return re.sub(r'/\*.*?\*/', '', text, flags=re.S)


def public_body(path, name, configuration):
    text = path.read_text()
    body = re.search(r'struct UA_' + name + r'\s*\{(.*?)\n\};', text, re.S)
    if not body:
        raise ValueError(f'Adapt C89 plugin backend: missing {name} declaration')
    active = [True]
    result = []
    for line in body.group(1).splitlines():
        directive = re.match(r'\s*#(\w+)(.*)', line)
        if directive:
            op, arg = directive.group(1), directive.group(2).strip()
            if op == 'ifdef':
                active.append(active[-1] and re.search(r'^#define ' + re.escape(arg) + r'\b', configuration, re.M) is not None)
            elif op == 'endif' and len(active) > 1:
                active.pop()
            else:
                raise ValueError(f'Adapt C89 plugin conditional: {line}')
        elif active[-1]:
            result.append(line)
    if len(active) != 1:
        raise ValueError('Unbalanced public plugin conditionals')
    return '\n'.join(result)


def translate(text):
    text = re.sub(r'\bbool\b', 'cpkt_opcua_Boolean', text)
    return re.sub(r'\bUA_Server\b', 'cpkt_opcua_server', text).replace('UA_', 'cpkt_opcua_')


def parameters(text):
    result = []
    for parameter in text.split(','):
        match = re.fullmatch(r'\s*(.*?)\b(\w+)\s*', parameter, re.S)
        if not match or not match[1].strip():
            raise ValueError(f'Adapt C89 plugin parameter: {parameter}')
        result.append((' '.join(match[1].split()), match[2]))
    return result


def callbacks(body):
    return CALLBACK_PATTERN.findall(uncomment(body))


def validate_fields(body, fields):
    residual = CALLBACK_PATTERN.sub('', uncomment(body))
    declarations = [' '.join(field.split()) for field in residual.split(';') if field.strip()]
    if declarations != fields:
        raise ValueError(f'Adapt C89 plugin data fields: {declarations}')


def emit_plugins(index, native_headers, output):
    configuration = (native_headers / 'config.h').read_text()
    header = ['/* Generated public plugin declarations; do not edit. */',
              '/* Derived from open62541 public headers, Mozilla Public License 2.0. */',
              '#ifndef CPKT_OPCUA_PLUGINS_H', '#define CPKT_OPCUA_PLUGINS_H',
              '#include <cpkt/opcua_types.h>', '#ifdef __cplusplus', 'extern "C" {', '#endif']
    metadata = ['/* Generated private callback trampolines; do not edit. */']
    access_control_header = native_headers / 'plugin/accesscontrol.h'
    header.insert(1, re.match(r'/\*.*?\*/', access_control_header.read_text(), re.S).group(0))
    body = public_body(access_control_header, 'AccessControl', configuration)
    validate_fields(body, ['void *context', 'size_t userTokenPoliciesSize',
                           'UA_UserTokenPolicy *userTokenPolicies'])
    methods = callbacks(body)
    expected = set(re.findall(r'\(\*(\w+)\)', uncomment(body)))
    if expected != {name for _, name, _ in methods}:
        raise ValueError('Adapt C89 access-control callback declarations')
    header += ['/** Complete upstream access-control record. Input policy arrays are borrowed',
               ' * during installation only; callbacks borrow a facade-owned policy copy.',
               ' * clear releases caller context only; do not free or modify policy arrays.',
               ' * Scalar/session context outputs follow the native plugin contract. */',
               'typedef struct cpkt_opcua_AccessControl cpkt_opcua_AccessControl;',
               '/** Generated access-control callback slots, with C89 schema arguments. */',
               'struct cpkt_opcua_AccessControl {', translate(body), '};',
               '/** Replace access control before startup. Copies callbacks and token policies.',
               ' * Context ownership transfers to clear only on success. Previous native clear',
               ' * runs after successful staging. Conversion failures deny the operation.',
               ' * If closeSession conversion fails, it still receives sessionContext with a',
               ' * NULL sessionId so it can release session resources; the logger reports it.',
               ' * All mandatory native authentication/rights callbacks must be present. */',
               'cpkt_opcua_StatusCode cpkt_opcua_server_set_access_control_plugin(cpkt_opcua_server *server, const cpkt_opcua_AccessControl *plugin);']
    assignments = []
    for result, name, signature in methods:
        if name == 'clear':
            continue
        args = parameters(signature)
        declarations, conversion, cleanup, callargs = [], [], [], []
        if args[:2] != [('UA_Server *', 'server'), ('UA_AccessControl *', 'ac')]:
            raise ValueError(f'Adapt access-control context: {name}')
        for spelling, param in args:
            if param == 'server':
                callargs.append('bridge->owner')
            elif param == 'ac':
                callargs.append('&bridge->plugin')
            elif spelling in ('void *', 'void **'):
                callargs.append(param)
            else:
                bare = spelling.replace('const ', '').replace('*', '').strip()
                typename = 'Boolean' if bare == 'bool' else bare[3:] if bare.startswith('UA_') else bare
                if typename not in index or spelling.count('*') > 1:
                    raise ValueError(f'Adapt public callback {name}.{param}: {spelling}')
                if '*' in spelling and not spelling.startswith('const '):
                    raise ValueError(f'Adapt mutable access-control argument {name}.{param}')
                declarations.append(f'  cpkt_opcua_{typename} c_{param};')
                conversion.append(f'  memset(&c_{param}, 0, sizeof(c_{param}));')
                pointer = param if '*' in spelling else '&' + param
                condition = f'if(!status && {param}) ' if '*' in spelling else 'if(!status) '
                conversion.append(f'  {condition}status = cpkt_convert({pointer}, &c_{param}, &cpkt_types[{index[typename]}], 0, 0);')
                cleanup.append(f'  cpkt_opcua_type_clear(&c_{param}, &cpkt_types[{index[typename]}]);')
                callargs.append(f'{param} ? &c_{param} : NULL' if '*' in spelling else 'c_' + param)
        fallback = 'status' if result == 'UA_StatusCode' else '0'
        native_signature = re.sub(r'\bbool\b', 'UA_Boolean', signature)
        invocation = (f'  if(!status) bridge->plugin.{name}({", ".join(callargs)});'
                      if result == 'void' else
                      f'  result = status ? ({result})({fallback}) : bridge->plugin.{name}({", ".join(callargs)});')
        if name == 'closeSession':
            invocation += ('\n  else {\n'
                '    UA_LOG_ERROR(UA_Server_getConfig(server)->logging, UA_LOGCATEGORY_SERVER, "C89 closeSession conversion failed: %08lx", (unsigned long)status);\n'
                '    bridge->plugin.closeSession(bridge->owner, &bridge->plugin, NULL, sessionContext);\n  }')
        metadata += [f'static {result} cpkt_ac_{name}({native_signature}) {{',
                     '  cpkt_ac_bridge *bridge = (cpkt_ac_bridge *)ac->context;',
                     '  UA_StatusCode status = 0;',
                     *([f'  {result} result;'] if result != 'void' else []),
                     *declarations, '  (void)server;', *conversion,
                     invocation, *cleanup,
                     *(['  return result;'] if result != 'void' else []), '}']
        assignments.append(f'  native.{name} = plugin->{name} ? cpkt_ac_{name} : NULL;')
    metadata += ['static void cpkt_ac_assign(UA_AccessControl *destination, const cpkt_opcua_AccessControl *plugin) {',
                 '  UA_AccessControl native;', '  memset(&native, 0, sizeof(native));',
                 *assignments, '  *destination = native;', '}']
    history_header, history_metadata = emit_history(index, native_headers, configuration)
    header += history_header
    metadata += history_metadata
    from history_emitter import emit_backend
    backend_header, backend_metadata = emit_backend(index, native_headers, configuration)
    header += backend_header
    metadata += backend_metadata
    header += ['#ifdef __cplusplus', '}', '#endif', '#endif', '']
    (output / 'cpkt/opcua_plugins.h').write_text('\n'.join(header))
    (output / 'opcua_plugins_metadata.inc').write_text('\n'.join(metadata) + '\n')


def emit_history(index, native_headers, configuration):
    body = public_body(native_headers / 'plugin/historydatabase.h', 'HistoryDatabase', configuration)
    validate_fields(body, ['void *context'])
    methods = callbacks(body)
    if set(re.findall(r'\(\*(\w+)\)', uncomment(body))) != {name for _, name, _ in methods}:
        raise ValueError('Adapt C89 history-database callback declarations')
    header = ['/** Full native history plugin. Callback inputs are borrowed until return.',
              ' * Mutable response/result records own their C89 allocations and are converted',
              ' * back after the callback. Use array_new/new for output storage. historyData',
              ' * pointers alias the corresponding response ExtensionObject payloads, as in',
              ' * upstream. Do not retain or free the pointer array itself. Context transfers',
              ' * to clear only after successful installation. clear releases caller context. */',
              'typedef struct cpkt_opcua_HistoryDatabase cpkt_opcua_HistoryDatabase;',
              '/** Generated callback slots for all native history operations. */',
              'struct cpkt_opcua_HistoryDatabase {', translate(body), '};',
              '/** Install a full history database before startup. Copies callback slots and',
              ' * borrows context until replacement or destruction. NULL callback slots retain',
              ' * their native unsupported-operation semantics. Conversion failures propagate',
              ' * through response/result status; void notification failures go to the logger. */',
              'cpkt_opcua_StatusCode cpkt_opcua_server_set_history_database_plugin(cpkt_opcua_server *server, const cpkt_opcua_HistoryDatabase *plugin);']
    metadata, assignments = [], []
    for result, name, signature in methods:
        if name == 'clear':
            continue
        if result != 'void':
            raise ValueError(f'Adapt history callback return: {name}')
        args = parameters(signature)
        if args[:2] != [('UA_Server *', 'server'), ('void *', 'hdbContext')]:
            raise ValueError(f'Adapt history context: {name}')
        declarations, conversion, cleanup, out, callargs = [], [], [], [], []
        error_target = None
        has_history_data = False
        for spelling, param in args:
            if param == 'server':
                callargs.append('bridge->owner')
            elif param == 'hdbContext':
                callargs.append('bridge->plugin.context')
            elif spelling == 'void *' or spelling == 'size_t':
                callargs.append(param)
            elif param == 'historyData':
                match = re.fullmatch(r'UA_(\w+) \* const \* const', spelling)
                if not match or match[1] not in index:
                    raise ValueError(f'Adapt history result pointer array: {spelling}')
                typename = match[1]
                has_history_data = True
                declarations += [f'  cpkt_opcua_{typename} **c_historyData = NULL;', '  size_t history_index;']
                conversion += [
                    '  if(!status && nodesToReadSize > (size_t)-1 / sizeof(*c_historyData)) status = UA_STATUSCODE_BADOUTOFMEMORY;',
                    '  if(!status && nodesToReadSize) {',
                    f'    c_historyData = (cpkt_opcua_{typename} **)UA_calloc(nodesToReadSize, sizeof(*c_historyData));',
                    '    if(!c_historyData) status = UA_STATUSCODE_BADOUTOFMEMORY;', '  }',
                    '  if(!status && c_response.resultsSize != nodesToReadSize) status = UA_STATUSCODE_BADINVALIDARGUMENT;',
                    '  for(history_index = 0; !status && history_index < nodesToReadSize; ++history_index) {',
                    '    const cpkt_opcua_ExtensionObject *eo = &c_response.results[history_index].historyData;',
                    '    const UA_ExtensionObject *native_eo = &response->results[history_index].historyData;',
                    f'    if(eo->encoding < CPKT_OPCUA_EXTENSIONOBJECT_DECODED || eo->content.decoded.type != &cpkt_types[{index[typename]}] ||',
                    '        !historyData || historyData[history_index] != native_eo->content.decoded.data)',
                    '      status = UA_STATUSCODE_BADTYPEMISMATCH;',
                    f'    else c_historyData[history_index] = (cpkt_opcua_{typename} *)eo->content.decoded.data;', '  }']
                cleanup.append('  UA_free(c_historyData);')
                callargs.append('c_historyData')
            else:
                bare = spelling.replace('const ', '').replace('*', '').strip()
                typename = bare[3:] if bare.startswith('UA_') else None
                if typename not in index or spelling.count('*') > 1:
                    raise ValueError(f'Adapt history argument {name}.{param}: {spelling}')
                if param == 'nodesToRead':
                    if spelling != 'const UA_HistoryReadValueId *':
                        raise ValueError('Adapt history input array')
                    declarations.append('  void *c_nodesToRead = NULL;')
                    conversion.append(f'  if(!status) status = cpkt_array(nodesToRead, nodesToReadSize, &c_nodesToRead, &cpkt_types[{index[typename]}], 0, 0);')
                    cleanup.append(f'  cpkt_clear_array(c_nodesToRead, c_nodesToRead ? nodesToReadSize : 0, &cpkt_types[{index[typename]}]);')
                    callargs.append('(const cpkt_opcua_HistoryReadValueId *)c_nodesToRead')
                    continue
                declarations.append(f'  cpkt_opcua_{typename} c_{param};')
                conversion += [f'  memset(&c_{param}, 0, sizeof(c_{param}));',
                    f'  if(!status{f" && {param}" if "*" in spelling else ""}) status = cpkt_convert({param if "*" in spelling else "&" + param}, &c_{param}, &cpkt_types[{index[typename]}], 0, 0);']
                cleanup.append(f'  cpkt_opcua_type_clear(&c_{param}, &cpkt_types[{index[typename]}]);')
                callargs.append(f'{param} ? &c_{param} : NULL' if '*' in spelling else 'c_' + param)
                if '*' in spelling and not spelling.startswith('const '):
                    declarations.append(f'  UA_{typename} staged_{param};')
                    conversion.append(f'  UA_{typename}_init(&staged_{param});')
                    out += [f'  if(!status && {param}) status = cpkt_convert(&c_{param}, &staged_{param}, &cpkt_types[{index[typename]}], 1, 0);']
                    out += [f'  if(!status && {param}) {{ UA_{typename}_clear({param}); *{param} = staged_{param}; UA_{typename}_init(&staged_{param}); }}']
                    cleanup.append(f'  UA_{typename}_clear(&staged_{param});')
                    if param in ('response', 'result'):
                        error_target = param
        if name.startswith('read') and not has_history_data:
            raise ValueError(f'Adapt history pointer views: {name}')
        error = (f'  if(status && {error_target}) {error_target}->' +
                 ('responseHeader.serviceResult' if error_target == 'response' else 'statusCode') + ' = status;'
                 if error_target else
                 f'  if(status) UA_LOG_ERROR(UA_Server_getConfig(server)->logging, UA_LOGCATEGORY_SERVER, "C89 history {name} conversion failed: %08lx", (unsigned long)status);')
        metadata += [f'static void cpkt_hdb_{name}({signature}) {{',
                     '  cpkt_hdb_bridge *bridge = (cpkt_hdb_bridge *)hdbContext;',
                     '  UA_StatusCode status = 0;', *declarations, '  (void)server;',
                     *conversion,
                     f'  if(!status) bridge->plugin.{name}({", ".join(callargs)});',
                     *out, error, *cleanup, '}']
        assignments.append(f'  native.{name} = plugin->{name} ? cpkt_hdb_{name} : NULL;')
    metadata += ['static void cpkt_hdb_assign(UA_HistoryDatabase *destination, const cpkt_opcua_HistoryDatabase *plugin) {',
                 '  UA_HistoryDatabase native;', '  memset(&native, 0, sizeof(native));',
                 *assignments, '  *destination = native;', '}']
    return header, metadata
