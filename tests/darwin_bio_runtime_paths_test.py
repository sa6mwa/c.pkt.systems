#!/usr/bin/env python3
"""Verify the real Darwin BIO test link plan before building dependencies."""

from pathlib import Path
import shlex
import subprocess
import sys

binary = Path(sys.argv[1])
make_program = sys.argv[2]
target = 'cpkt_openssl_bio_lifecycle_shared_test'
if (binary / 'build.ninja').is_file():
    commands = subprocess.run([make_program, '-C', str(binary), '-t', 'commands',
                               target], check=True, capture_output=True, text=True)
    links = []
    for line in commands.stdout.splitlines():
        tokens = shlex.split(line)
        if '-o' in tokens and Path(tokens[tokens.index('-o') + 1]).name == target:
            links.append(tokens)
    if len(links) != 1:
        raise SystemExit('Darwin BIO test link command missing or ambiguous')
    tokens = links[0]
else:
    tokens = shlex.split((binary / 'CMakeFiles' / (target + '.dir') /
                         'link.txt').read_text())
for directory in set(sys.argv[3:]):
    if '-Wl,-rpath,' + directory not in tokens:
        raise SystemExit('Darwin BIO shared test is missing build runtime path: '
                         + directory)
print('Darwin shared BIO link plan includes bundled OpenSSL runtime paths')
