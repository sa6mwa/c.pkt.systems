#!/usr/bin/env python3
"""Apply the maintained emitter hook to a disposable copy of upstream tools."""
import argparse
from pathlib import Path
import re
import shutil
import subprocess
import sys

p = argparse.ArgumentParser()
p.add_argument('--upstream', required=True)
p.add_argument('--output', required=True)
p.add_argument("--extra-bsd", action="append", default=[])
p.add_argument("--fixture-table", action="store_true")
a = p.parse_args()
upstream = Path(a.upstream)
output = Path(a.output)
output.mkdir(parents=True, exist_ok=True)
scratch = output / 'upstream'
if scratch.exists():
    shutil.rmtree(scratch)
shutil.copytree(upstream, scratch)
script = scratch / 'generate_datatypes.py'
text = script.read_text()
anchor = 'args = parser.parse_args()'
if text.count(anchor) != 1 or not text.endswith('generator.write_definitions()\n'):
    raise SystemExit('Upstream generator changed: adapt the C89 backend hook')
text = text.replace(anchor, 'parser.add_argument("--cpkt-output", required=True)\nparser.add_argument("--cpkt-client-header", required=True)\n' + anchor)
text += '\nfrom c89_emitter import emit\nemit(generator, args.cpkt_output, args.cpkt_client_header)\n'
script.chmod(0o644)
script.write_text(text)
shutil.copyfile(Path(__file__).with_name('c89_emitter.py'), scratch / 'c89_emitter.py')
shutil.copyfile(Path(__file__).with_name('plugin_emitter.py'), scratch / 'plugin_emitter.py')
shutil.copyfile(Path(__file__).with_name('server_emitter.py'), scratch / 'server_emitter.py')
shutil.copyfile(Path(__file__).with_name('history_emitter.py'), scratch / 'history_emitter.py')
shutil.copyfile(Path(__file__).with_name('node_emitter.py'), scratch / 'node_emitter.py')
shutil.copyfile(Path(__file__).with_name('producer_emitter.py'), scratch / 'producer_emitter.py')
shutil.copyfile(Path(__file__).with_name('creation_emitter.py'), scratch / 'creation_emitter.py')
subprocess.run([sys.executable, str(scratch / 'generate_nodeid_header.py'),
                str(scratch / 'schema/NodeIds.csv'), str(output / 'nodeids'), 'NS0'], check=True)
command = [sys.executable, str(script), '--type-bsd',
                str(scratch / 'schema/Opc.Ua.Types.bsd'), '--type-csv',
                str(scratch / 'schema/NodeIds.csv'), '--cpkt-output', str(output),
                '--cpkt-client-header', str(upstream.parent.parent / 'include/open62541/client.h'),
                str(output / 'types')]
for bsd in a.extra_bsd:
    command += ['--type-bsd', bsd]
subprocess.run(command, check=True)
if a.fixture_table:
    native = output / 'native/open62541'
    native.mkdir(parents=True, exist_ok=True)
    for suffix in ('h', 'c'):
        source = output / ('types_generated.' + suffix)
        (native / source.name).write_text(re.sub(r'\bUA_TYPES\b', 'UA_CPKT_TEST_TYPES', source.read_text()))
    metadata = output / 'opcua_types_metadata.inc'
    metadata.write_text(re.sub(r'\bUA_TYPES\b', 'UA_CPKT_TEST_TYPES', metadata.read_text()))
