#!/usr/bin/env python3
"""Require the OPC UA facade's definitions to match its public declarations."""
from pathlib import Path
import re
import subprocess
import sys

nm, readelf, library, private_header, *headers = sys.argv[1:]
private = set(re.findall(r"\b(cpkt_\w+)\s*\(", Path(private_header).read_text()))
expected = set()
for header in headers:
    text = Path(header).read_text()
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    text = re.sub(r'^\s*#.*$', '', text, flags=re.M)
    expected.update(re.findall(r'\b(cpkt_opcua_\w+)\s*\((?!\s*\*)', text))
    expected.update(re.findall(r'\bextern\s+const\s+cpkt_opcua_\w+\s+\*?\s*(cpkt_opcua_\w+)\s*;', text))
output = subprocess.check_output([nm, '-g', '--defined-only', library], text=True)
actual = set()
for line in output.splitlines():
    parts = line.split()
    if len(parts) < 2 or parts[-2].upper() == 'U':
        continue
    # Musl's ELF startup object defines these runtime entry points, as in
    # the other facade export policy gates. They are not bridge definitions.
    if parts[-1] in ('_init', '_fini'):
        continue
    symbol = parts[-1].lstrip('_')
    if symbol in private:
        if not library.endswith('.a'):
            raise SystemExit(f'Private helper escaped the shared-library boundary: {symbol}')
        continue
    if symbol.startswith('cpkt_opcua_'):
        actual.add(symbol)
    elif parts[-2].upper() in ('T', 'D', 'B', 'R'):
        raise SystemExit(f'Unexpected private definition in OPC UA facade: {symbol}')
if actual != expected:
    raise SystemExit(f'OPC UA export mismatch: missing={sorted(expected - actual)}, '
                     f'extra={sorted(actual - expected)}')
# Archives need private inter-object linkage, but those symbols must remain
# hidden when a downstream executable links them. Check the object visibility.
if library.endswith('.a'):
    symbols = subprocess.check_output([readelf, '-W', '-s', library], text=True)
    seen = set()
    for line in symbols.splitlines():
        parts = line.split()
        if len(parts) >= 8 and parts[-1] in private and parts[6] != 'UND':
            if parts[5] != 'HIDDEN':
                raise SystemExit(f'Private helper has public visibility: {parts[-1]}')
            seen.add(parts[-1])
    if seen != private:
        raise SystemExit(f'Missing private archive helpers: {sorted(private - seen)}')
print(f'{len(actual)} public OPC UA definitions; private helpers stay hidden')
