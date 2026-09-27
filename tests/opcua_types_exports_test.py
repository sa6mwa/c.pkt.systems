#!/usr/bin/env python3
"""Require the OPC UA facade's definitions to match its public declarations."""
from pathlib import Path
import re
import subprocess
import sys

nm, library, *headers = sys.argv[1:]
expected = set()
for header in headers:
    text = Path(header).read_text()
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    text = re.sub(r'^\s*#.*$', '', text, flags=re.M)
    expected.update(re.findall(r'\b(cpkt_opcua_\w+)\s*\((?!\s*\*)', text))
    expected.update(re.findall(r'\bextern\s+const\s+cpkt_opcua_\w+\s+(cpkt_opcua_\w+)\s*;', text))
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
    if symbol.startswith('cpkt_opcua_'):
        actual.add(symbol)
    elif parts[-2].upper() in ('T', 'D', 'B', 'R'):
        raise SystemExit(f'Unexpected private definition in OPC UA facade: {symbol}')
if actual != expected:
    raise SystemExit(f'OPC UA export mismatch: missing={sorted(expected - actual)}, '
                     f'extra={sorted(actual - expected)}')
print(f'{len(actual)} public OPC UA definitions; no private bridge definitions')
