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
