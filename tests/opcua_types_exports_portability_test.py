#!/usr/bin/env python3
"""Export gates must reject leaks using both ELF and Apple nm interfaces."""
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

repo = Path(sys.argv[1]).resolve()
binary = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else repo / 'build'
configured_compiler = sys.argv[3] if len(sys.argv) > 3 else None
if configured_compiler is None:
    cache = (binary / 'CMakeCache.txt').read_text()
    match = re.search(r'^CMAKE_C_COMPILER:(?:FILEPATH|STRING)=(.+)$',
                      cache, re.MULTILINE)
    if not match:
        raise RuntimeError('configured C compiler missing: ' + str(binary))
    configured_compiler = match.group(1)
binary.mkdir(parents=True, exist_ok=True)
checker = repo / 'tests/opcua_types_exports_test.py'
with tempfile.TemporaryDirectory(prefix='opcua-export-tools-', dir=binary) as temporary:
    work = Path(temporary)
    private = work / 'private.h'
    private.write_text('void cpkt_bridge_helper(void);\n')
    public = work / 'public.h'
    public.write_text('void cpkt_opcua_hello(void);\n'
                      'extern const cpkt_opcua_DataType *cpkt_opcua_table;\n'
                      '#ifdef __linux__\nvoid cpkt_opcua_linux_only(void);\n#endif\n')
    tool = work / 'symbols'
    tool.write_text('#!' + sys.executable + '\n' + '''
import json
from pathlib import Path
import sys
fixture = json.loads(Path(__file__).with_suffix('.json').read_text())
args = sys.argv[1:]
if args[:-1] == ['-W', '-s']:
    assert fixture['system'] == 'Linux', 'Mach-O must not use readelf'
    print(fixture['visibility'])
else:
    flags = ['-g', '-m'] if fixture['system'] == 'Darwin' else ['-g', '--defined-only']
    assert args[:-1] == flags, args
    print(fixture['symbols'])
''')
    tool.chmod(0o755)
    compiler = work / 'compiler'
    compiler.write_text('#!' + sys.executable + '\n' + '''
import json
from pathlib import Path
import subprocess
import sys
fixture = json.loads(Path(__file__).with_name('symbols.json').read_text())
flags = ['-U__linux__', '-D__APPLE__'] if fixture['system'] == 'Darwin' else ['-D__linux__']
raise SystemExit(subprocess.run([fixture['compiler'], *flags, *sys.argv[1:]]).returncode)
''')
    compiler.chmod(0o755)

    for system in ('Linux', 'Darwin'):
        macho = system == 'Darwin'
        public_symbols = ('object.o:\n'
                          '                 (undefined) external _UA_Server_new\n'
                          '0000000000000000 (__TEXT,__text) external _cpkt_opcua_hello\n'
                          '0000000000000020 (__DATA,__const) external _cpkt_opcua_table\n') if macho else (
                          'object.o:\n00000000 T cpkt_opcua_hello\n'
                          '00000020 D cpkt_opcua_table\n00000040 T cpkt_opcua_linux_only\n'
                          '                 U UA_Server_new\n')
        helper = ('0000000000000030 (__TEXT,__text) private external _cpkt_bridge_helper\n'
                  if macho else '00000030 T cpkt_bridge_helper\n')
        hidden = '1: 00000030 4 FUNC GLOBAL HIDDEN 1 cpkt_bridge_helper\n'

        def check(suffix, symbols, visibility=hidden, error=None):
            tool.with_suffix('.json').write_text(json.dumps({
                'system': system, 'symbols': symbols, 'visibility': visibility,
                'compiler': configured_compiler}))
            library = work / ('facade' + suffix)
            result = subprocess.run([sys.executable, str(checker), str(tool), str(tool),
                                     system, str(compiler), str(library), str(private), str(public)],
                                    text=True, capture_output=True)
            output = result.stdout + result.stderr
            if error is None:
                count = 2 if macho else 3
                assert result.returncode == 0 and f'{count} public OPC UA definitions' in output, output
            else:
                assert result.returncode != 0 and error in output, output

        shared = '.dylib' if macho else '.so'
        check(shared, public_symbols)
        check('.a', public_symbols + helper)
        check(shared, public_symbols + helper,
              error='Private helper escaped the shared-library boundary')
        check('.a', public_symbols, visibility='', error='Missing private archive helpers')
        check('.a', public_symbols + helper.replace('private external ', 'external '),
              visibility=hidden.replace('HIDDEN', 'DEFAULT'),
              error='Private helper has public visibility')
        for symbol in ('UA_private', 'cpkt_opcua_unexpected'):
            extra = (f'0000000000000040 (__TEXT,__text) external _{symbol}\n' if macho
                     else f'00000040 T {symbol}\n')
            check(shared, public_symbols + extra,
                  error='Unexpected private definition' if symbol == 'UA_private' else 'export mismatch')
        check(shared, public_symbols.replace('cpkt_opcua_hello', 'cpkt_opcua_wrong'),
              error='export mismatch')
print('ELF and Mach-O export gates reject private leaks, missing helpers and API mismatches')
