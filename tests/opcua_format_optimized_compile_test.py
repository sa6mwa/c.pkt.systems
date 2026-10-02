#!/usr/bin/env python3
"""Catch optimized native formatter diagnostics during the debug gate."""
import json
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

build = Path(sys.argv[1]).resolve()
source = build / "generated/opcua/opcua_native_format.c"
commands = json.loads((build / "compile_commands.json").read_text())
entry = next(item for item in commands if Path(item["file"]).resolve() == source)
arguments = entry.get("arguments") or shlex.split(entry["command"])
# Retain the actual toolchain, sysroot, headers and warning policy. Put the
# object and any compiler bookkeeping in an isolated build-directory workspace.
filtered = []
index = 0
while index < len(arguments):
    arg = arguments[index]
    if arg in ("-o", "-MF", "-MT", "-MQ"):
        index += 2
        continue
    if arg not in ("-MD", "-MMD", "-MP"):
        filtered.append(arg)
    index += 1
with tempfile.TemporaryDirectory(prefix="opcua-format-opt-", dir=build) as work:
    command = filtered + ["-O3", "-o", str(Path(work) / "formatter.o")]
    subprocess.run(command, cwd=entry["directory"], check=True)
print("Optimized native formatter compiles with the configured warning policy")
