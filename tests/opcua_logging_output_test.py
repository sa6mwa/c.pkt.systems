#!/usr/bin/env python3
"""Custom OPC UA logging must not also emit to stdout or stderr."""
import subprocess
import sys

result = subprocess.run(sys.argv[1:], text=True, stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE, timeout=25)
if result.returncode or result.stdout != "[test] OPC UA logging through libpslog passed\n" or result.stderr:
    raise SystemExit("Unexpected default log output or test failure:\n" + result.stdout + result.stderr)
print("[test] custom OPC UA logger exclusively owns diagnostic output")
