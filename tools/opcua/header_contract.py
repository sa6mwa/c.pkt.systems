#!/usr/bin/env python3
"""Check public C89 header tokens without rejecting documentation or literals.

Compiler consumers remain the language/ABI gate. This additional package
contract rejects upstream type exposure and non-C89 integer declarations.
"""
import re
import sys
from pathlib import Path

FRAGMENTS = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*')
LITERALS = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
INCLUDES = re.compile(r'^\s*#\s*include\s*[<"](open62541/[^>"\n]*|std(?:int|bool)\.h)[>"]', re.M)


def violations(text):
    text = re.sub(r'\\\r?\n', '', text)
    text = FRAGMENTS.sub(lambda m: ' ' if m[0].startswith(('/',)) else m[0], text)
    failures = [f'upstream/non-C89 include: {m[1]}' for m in INCLUDES.finditer(text)]
    text = LITERALS.sub(' ', text)
    tokens = re.findall(r'[A-Za-z_]\w*|\S', text)
    for token in tokens:
        if (token.startswith('UA_') or token in ('inline', '_Bool') or
                re.fullmatch(r'u?int(?:8|16|32|64)_t', token)):
            failures.append(f'forbidden C89 facade token: {token}')
    if any(a == b == 'long' for a, b in zip(tokens, tokens[1:])):
        failures.append('forbidden C89 facade declaration: long long')
    return sorted(set(failures))


def main():
    if len(sys.argv) < 2:
        raise SystemExit('usage: header_contract.py <header> [<header> ...]')
    failed = False
    for name in sys.argv[1:]:
        for failure in violations(Path(name).read_text()):
            print(f'{name}: {failure}', file=sys.stderr)
            failed = True
    if failed:
        raise SystemExit(1)
    print(f'C89 facade header contract passed ({len(sys.argv) - 1} headers)')


if __name__ == '__main__':
    main()
