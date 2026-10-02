#!/usr/bin/env python3
"""Require the OPC UA facade's definitions to match its public declarations."""
from pathlib import Path
import re
import subprocess
import sys

nm, readelf, system_name, compiler, library, private_header, *headers = sys.argv[1:]
private = set(re.findall(r"\b(cpkt_\w+)\s*\(", Path(private_header).read_text()))
expected = set()
# Use the actual target compiler to resolve platform guards and includes.
# Reading raw headers would require Linux-only factories in Darwin libraries.
include_dirs = sorted({str(Path(header).resolve().parent.parent) for header in headers})
source = ''.join(f'#include "{Path(header).resolve()}"\n' for header in headers)
text = subprocess.check_output([compiler, '-E', '-P', '-x', 'c', '-std=c99',
                                *(f'-I{directory}' for directory in include_dirs), '-'],
                               input=source, text=True)
expected.update(re.findall(r'\b(cpkt_opcua_\w+)\s*\((?!\s*\*)', text))
expected.update(re.findall(r'\bextern\s+const\s+cpkt_opcua_\w+\s+\*?\s*(cpkt_opcua_\w+)\s*;', text))
# Apple nm has neither GNU's --defined-only nor ELF symbol visibility. Its
# detailed Mach-O output identifies private externals in static objects.
macho = system_name == 'Darwin'
flags = ['-g', '-m'] if macho else ['-g', '--defined-only']
output = subprocess.check_output([nm, *flags, library], text=True)
actual = set()
seen_private = set()
for line in output.splitlines():
    parts = line.split()
    if len(parts) < 2 or parts[-2].upper() == 'U' or '(undefined)' in line:
        continue
    if macho and ' external ' not in line:
        continue
    # Musl's ELF startup object defines these runtime entry points, as in
    # the other facade export policy gates. They are not bridge definitions.
    if parts[-1] in ('_init', '_fini'):
        continue
    symbol = parts[-1].lstrip('_')
    if symbol in private:
        if not library.endswith('.a'):
            raise SystemExit(f'Private helper escaped the shared-library boundary: {symbol}')
        if macho:
            if 'private external ' not in line:
                raise SystemExit(f'Private helper has public visibility: {symbol}')
            seen_private.add(symbol)
        continue
    if symbol.startswith('cpkt_opcua_'):
        actual.add(symbol)
    elif macho or parts[-2].upper() in ('T', 'D', 'B', 'R'):
        raise SystemExit(f'Unexpected private definition in OPC UA facade: {symbol}')
if actual != expected:
    raise SystemExit(f'OPC UA export mismatch: missing={sorted(expected - actual)}, '
                     f'extra={sorted(actual - expected)}')
# Archives need private inter-object linkage, but those symbols must remain
# hidden when a downstream executable links them. Check the object visibility.
if library.endswith('.a') and not macho:
    symbols = subprocess.check_output([readelf, '-W', '-s', library], text=True)
    for line in symbols.splitlines():
        parts = line.split()
        if len(parts) >= 8 and parts[-1] in private and parts[6] != 'UND':
            if parts[5] != 'HIDDEN':
                raise SystemExit(f'Private helper has public visibility: {parts[-1]}')
            seen_private.add(parts[-1])
if library.endswith('.a') and seen_private != private:
    raise SystemExit(f'Missing private archive helpers: {sorted(private - seen_private)}')
print(f'{len(actual)} public OPC UA definitions; private helpers stay hidden')
