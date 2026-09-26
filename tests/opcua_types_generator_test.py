#!/usr/bin/env python3
"""Fail-fast and deterministic-generation checks for the upstream emitter hook."""
from pathlib import Path
import importlib.util
import shutil
import subprocess
import sys
import tempfile
from types import SimpleNamespace

repo, upstream, build = map(Path, sys.argv[1:])
sys.dont_write_bytecode = True
sys.path.insert(0, str(upstream))
from nodeset_compiler.type_parser import BuiltinType
spec = importlib.util.spec_from_file_location('cpkt_c89_emitter', repo / 'tools/opcua/c89_emitter.py')
emitter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(emitter)

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
    tool = repo / 'tools/opcua/generate.py'
    for name in ('one', 'two'):
        subprocess.run([sys.executable, tool, '--upstream', upstream, '--output', work / name], check=True)
    for filename in ('cpkt/opcua_types.h', 'opcua_types_metadata.inc'):
        first = (work / 'one' / filename).read_bytes()
        assert first == (work / 'two' / filename).read_bytes()
        assert str(repo).encode() not in first and str(work).encode() not in first
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
