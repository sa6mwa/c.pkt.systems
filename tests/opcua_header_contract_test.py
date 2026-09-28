#!/usr/bin/env python3
"""Public header package checks distinguish actual code from documentation."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
repo = Path(sys.argv[1])
checker = repo / 'tools/opcua/header_contract.py'
spec = importlib.util.spec_from_file_location('headers', checker)
headers = importlib.util.module_from_spec(spec)
spec.loader.exec_module(headers)
valid = r"""
/** Native UA_Server_run takes long long internally. */
// inline uint64_t and <open62541/types.h> are documentation.
typedef struct { long longValue; unsigned long ulongValue; } cpkt_pair;
#define CPKT_LABEL "UA_Server inline long long /* a literal */ // text"
#define CPKT_QUOTE "\" UA_NodeId"
#define CPKT_CHARACTER '\''
#include <stddef.h>
"""
assert headers.violations(valid) == []
for invalid in ('#include <open62541/types.h>', '#include "stdint.h"',
                '#include <stdbool.h>', 'UA_Server *root;',
                '#define CPKT_ALIAS UA_Client', 'static inline int f(void);',
                'uint64_t value;', 'int64_t value;', '_Bool enabled;',
                'unsigned long\tlong value;', 'long/**/long value;',
                'UA_\\\nNodeId value;'):
    assert headers.violations(invalid), invalid
with tempfile.TemporaryDirectory(dir=repo / 'build') as temporary:
    header = Path(temporary) / 'api.h'
    header.write_text(valid)
    result = subprocess.run([sys.executable, str(checker), str(header)],
                            text=True, capture_output=True)
    assert result.returncode == 0, result.stderr
    header.write_text(valid + '\nlong long illegal;')
    result = subprocess.run([sys.executable, str(checker), str(header)],
                            text=True, capture_output=True)
    assert result.returncode == 1 and 'long long' in result.stderr
print('Public header code rejects native/non-C89 tokens and accepts documented native semantics')
