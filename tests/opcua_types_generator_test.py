#!/usr/bin/env python3
"""Fail-fast and deterministic-generation checks for the upstream emitter hook."""
from pathlib import Path
import importlib.util
import re
import shutil
import subprocess
import sys
import tempfile
from types import SimpleNamespace

repo, upstream, build = map(Path, sys.argv[1:])
sys.dont_write_bytecode = True
sys.path.insert(0, str(upstream))
sys.path.insert(0, str(repo / 'tools/opcua'))
from nodeset_compiler.type_parser import BuiltinType
spec = importlib.util.spec_from_file_location('cpkt_c89_emitter', repo / 'tools/opcua/c89_emitter.py')
emitter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(emitter)
import plugin_emitter
import server_emitter
import history_emitter
import node_emitter
import producer_emitter
import creation_emitter
import client_emitter
import async_client_emitter

with tempfile.TemporaryDirectory(prefix='opcua-generator-', dir=build) as temporary:
    work = Path(temporary)
    for graph, error in [
        ({'ns': {'Decimal': BuiltinType('Decimal')}}, KeyError),
        ({'a': {'Byte': BuiltinType('Byte')}, 'b': {'Byte': BuiltinType('Byte')}}, ValueError),
        ({'ns': {'Unknown': SimpleNamespace(name='Unknown', description='')}}, ValueError),
    ]:
        try:
            emitter.emit(SimpleNamespace(filtered_types=graph), work / 'invalid', None)
        except error:
            pass
        else:
            raise AssertionError('Emitter accepted an unsupported or ambiguous graph')
    assert not (work / 'invalid/cpkt/opcua_types.h').exists()
    assert plugin_emitter.translate('UA_ServerState state; bool flag; UA_Server *server;') == 'cpkt_opcua_ServerState state; cpkt_opcua_Boolean flag; cpkt_opcua_server *server;'
    try:
        plugin_emitter.validate_fields('void *context; UA_UInt32 newField;', ['void *context'])
    except ValueError:
        pass
    else:
        raise AssertionError('Plugin backend silently ignored a new data field')
    tool = repo / 'tools/opcua/generate.py'
    for name in ('one', 'two'):
        subprocess.run([sys.executable, tool, '--upstream', upstream, '--output', work / name], check=True)
    for filename in ('cpkt/opcua_types.h', 'cpkt/opcua_constants.h', 'cpkt/opcua_plugins.h',
                     'opcua_types_metadata.inc', 'opcua_plugins_metadata.inc'):
        first = (work / 'one' / filename).read_bytes()
        assert first == (work / 'two' / filename).read_bytes()
        assert str(repo).encode() not in first and str(work).encode() not in first
    index = {name: int(value) for name, value in re.findall(
        r'^#define CPKT_OPCUA_TYPES_(\w+) (\d+)$',
        (work / 'one/cpkt/opcua_types.h').read_text(), re.M)}
    # Use descriptor names to retain upstream mixed-case names.
    names = re.findall(r'^  \{"(\w+)", sizeof\(cpkt_opcua_\w+\)',
                       (work / 'one/opcua_types_metadata.inc').read_text(), re.M)
    index = {name: index[name.upper()] for name in names}
    native_headers = upstream.parent.parent / 'include/open62541'
    changed_headers = work / 'headers'
    shutil.copytree(native_headers, changed_headers)
    ac = changed_headers / 'plugin/accesscontrol.h'
    original_ac = ac.read_text()
    ac.chmod(0o644)
    ac.write_text(original_ac.replace('void *context;', 'void *context; UA_UInt32 unexpected;', 1))
    try:
        plugin_emitter.emit_plugins(index, changed_headers, work / 'one')
    except ValueError:
        pass
    else:
        raise AssertionError('New public plugin field was silently ignored')
    ac.write_text(original_ac.replace('(*activateSession)', '(*activateSessionChanged)', 1).replace('UA_AccessControl *ac,', 'UA_UnknownPlugin *ac,', 1))
    try:
        plugin_emitter.emit_plugins(index, changed_headers, work / 'one')
    except ValueError:
        pass
    else:
        raise AssertionError('Unsupported public callback context was accepted')
    history_header = changed_headers / 'plugin/historydata/history_data_backend.h'
    original_history = history_header.read_text()
    history_header.chmod(0o644)
    for broken in (
        original_history.replace('void *context;', 'void *context; UA_UInt32 unexpected;', 1),
        original_history.replace('const MatchStrategy strategy', 'const UA_UnknownType strategy', 1),
        original_history.replace('const UA_DataValue*', 'UA_DataValue**', 1),
    ):
        history_header.write_text(broken)
        try:
            history_emitter.emit_backend(index, changed_headers, (changed_headers / 'config.h').read_text())
        except ValueError:
            pass
        else:
            raise AssertionError('Unsupported history backend signature was accepted')
    history_header.write_text(original_history)
    history_public, history_private = history_emitter.emit_backend(index, changed_headers, (changed_headers / 'config.h').read_text())
    assert 'const cpkt_opcua_history_value*' in '\n'.join(history_public)
    assert re.match(r'/\*.*?\*/', (native_headers / 'types.h').read_text(), re.S)[0] in '\n'.join(history_public)
    assert any('return stored ? &stored->native : NULL;' in line for line in history_private)
    assert {name for _, name, _ in plugin_emitter.callbacks(plugin_emitter.public_body(history_header, 'HistoryDataBackend', (changed_headers / 'config.h').read_text()))} == {
        'deleteMembers', 'serverSetHistoryData', 'getHistoryData', 'getDateTimeMatch', 'getEnd', 'lastIndex', 'firstIndex',
        'resultSize', 'copyDataValues', 'getDataValue', 'boundSupported', 'timestampsToReturnSupported',
        'insertDataValue', 'replaceDataValue', 'updateDataValue', 'removeDataValue',
    }
    highlevel = changed_headers / 'client_highlevel.h'
    original_highlevel = highlevel.read_text()
    highlevel.chmod(0o644)
    client_public, client_private = client_emitter.emit_client(index, changed_headers)
    for _, client_name, _ in client_emitter.declarations(original_highlevel):
        assert 'cpkt_opcua_client_' + client_name + '_typed' in '\n'.join(client_public)
        assert 'UA_Client_' + client_name + '(' in '\n'.join(client_private)
    assert len(client_emitter.declarations(original_highlevel)) == 75
    for old, new in (
        ('UA_Variant **output', 'UA_Variant *output'),
        ('const UA_UInt32 *newArrayDimensions', 'const UA_UInt64 *newArrayDimensions'),
        ('const UA_DataType *valueType', 'const UA_DataType **valueType'),
        ('UA_HistoricalIteratorCallback)', 'UA_HistoricalIteratorCallbackChanged)'),
        ('UA_Boolean moreDataAvailable', 'UA_UInt64 moreDataAvailable'),
        ('UA_NodeIteratorCallback callback', 'UA_ServerCallback callback'),
    ):
        assert old in original_highlevel
        highlevel.write_text(original_highlevel.replace(old, new, 1))
        try:
            client_emitter.emit_client(index, changed_headers)
        except ValueError:
            pass
        else:
            raise AssertionError('Changed high-level client signature was accepted')
    highlevel.write_text(original_highlevel)
    asynchronous = changed_headers / 'client_highlevel_async.h'
    asynchronous.chmod(0o644)
    original_async = asynchronous.read_text()
    async_public, async_private = async_client_emitter.emit_async_client(index, changed_headers)
    assert len(async_client_emitter.declarations(original_async)) == 59
    assert len(async_client_emitter.callback_declarations(original_async)) == 31
    for _, native_name, _ in async_client_emitter.declarations(original_async):
        assert 'cpkt_opcua_client_' + native_name.split('UA_Client_', 1)[1] + '_typed' in '\n'.join(async_public)
        assert native_name + '(' in '\n'.join(async_private)
    for old, new in (
        ('UA_UInt32 requestId, UA_ReadResponse *rr', 'UA_UInt64 requestId, UA_ReadResponse *rr'),
        ('const ATTR_TYPE *attr', 'const ATTR_TYPE **attr'),
        ('UA_ClientAsyncCallCallback callback', 'UA_ClientAsyncOperationCallback callback'),
        ('size_t inputSize', 'UA_UInt32 inputSize'),
        ('UA_NodeId *outNewNodeId', 'UA_NodeId **outNewNodeId'),
    ):
        assert old in original_async
        asynchronous.write_text(original_async.replace(old, new, 1))
        try:
            async_client_emitter.emit_async_client(index, changed_headers)
        except ValueError:
            pass
        else:
            raise AssertionError('Changed asynchronous client signature was accepted')
    asynchronous.write_text(original_async)
    server = changed_headers / 'server.h'
    server.chmod(0o644)
    original_server = server.read_text()
    node_public, node_private = node_emitter.emit_nodes(index, changed_headers)
    for record in ('ValueSourceNotifications', 'NodeTypeLifecycle', 'GlobalNodeLifecycle'):
        assert ('cpkt_opcua_' + record) in '\n'.join(node_public)
    for old, new in (
        ('(*onRead)', '(*onReadChanged)'),
        ('const UA_NumericRange *range,', 'const UA_UnknownType *range,'),
        ('} UA_GlobalNodeLifecycle;', 'UA_UInt32 unexpected; } UA_GlobalNodeLifecycle;'),
        ('UA_NodeId *targetNodeId);', 'UA_NodeId **targetNodeId);'),
        ('UA_DataValue **value,', 'UA_DataValue *value,'),
        ('UA_DataValue **value,', 'UA_Variant **value,'),
    ):
        assert old in original_server
        server.write_text(original_server.replace(old, new, 1))
        try:
            node_emitter.emit_nodes(index, changed_headers)
        except ValueError:
            pass
        else:
            raise AssertionError('Changed node callback declaration was accepted')
    server.write_text(original_server)
    producer_public, producer_private = producer_emitter.emit_producers(index, changed_headers)
    assert 'cpkt_opcua_CallbackValueSource' in '\n'.join(producer_public)
    assert 'cpkt_opcua_MethodCallback' in '\n'.join(producer_public)
    for old, new in (
        ('} UA_CallbackValueSource;', 'UA_UInt32 unexpected; } UA_CallbackValueSource;'),
        ('(*UA_MethodCallback)', '(*UA_MethodCallbackChanged)'),
        ('UA_Boolean includeSourceTimeStamp,', 'UA_UInt64 includeSourceTimeStamp,'),
        ('const UA_DataValue *value);', 'UA_DataValue **value);'),
    ):
        assert old in original_server
        if old == 'const UA_DataValue *value);':
            end = original_server.index('} UA_CallbackValueSource;')
            start = original_server.rfind('typedef struct {', 0, end)
            scope = original_server[start:end]
            assert old in scope
            changed = original_server[:start] + scope.replace(old, new, 1) + original_server[end:]
        else:
            changed = original_server.replace(old, new, 1)
        server.write_text(changed)
        try:
            producer_emitter.emit_producers(index, changed_headers)
        except ValueError:
            pass
        else:
            raise AssertionError('Changed producer signature was accepted')
    server.write_text(original_server)
    creation_public, creation_private = creation_emitter.emit_creation(index, changed_headers)
    for creation_name in creation_emitter.NAMES:
        assert 'cpkt_opcua_server_' + creation_name + '_typed' in '\n'.join(creation_public)
        assert 'UA_Server_' + creation_name + '(' in '\n'.join(creation_private)
    for creation_name, old, new in (
        ('addCallbackValueSourceVariableNode', 'const UA_CallbackValueSource evs', 'const UA_CallbackValueSource *evs'),
        ('addMethodNodeEx', 'const UA_Argument *inputArguments', 'const UA_Argument **inputArguments'),
        ('addNode_begin', 'const UA_DataType *attributeType', 'const UA_DataType **attributeType'),
        ('addMethodNode_finish', 'UA_MethodCallback method', 'UA_ServerCallback method'),
    ):
        start = original_server.index('UA_Server_' + creation_name + '(')
        end = original_server.index(');', start) + 2
        scope = original_server[start:end]
        assert old in scope
        server.write_text(original_server[:start] + scope.replace(old, new, 1) + original_server[end:])
        try:
            creation_emitter.emit_creation(index, changed_headers)
        except ValueError:
            pass
        else:
            raise AssertionError('Changed creation signature was accepted')
    server.write_text(original_server)
    server.write_text(server.read_text().replace('UA_NodeId *out);', 'UA_NodeId **out);', 1))
    try:
        server_emitter.emit_server(index, changed_headers)
    except ValueError:
        pass
    else:
        raise AssertionError('Changed public server pointer signature was accepted')
    changed = work / 'changed/share/open62541'
    shutil.copytree(upstream, changed)
    generator = changed / 'generate_datatypes.py'
    generator.chmod(0o644)
    generator.write_text(generator.read_text().replace('args = parser.parse_args()', 'args = None'))
    result = subprocess.run([sys.executable, tool, '--upstream', changed, '--output', work / 'changed-output'],
                            capture_output=True, text=True)
    assert result.returncode and 'Upstream generator changed' in result.stderr
    assert not (work / 'changed-output/cpkt/opcua_types.h').exists()
print('Unsupported graphs and changed hooks fail early; generated interfaces are deterministic')
