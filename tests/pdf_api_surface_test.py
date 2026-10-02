#!/usr/bin/env python3
"""Verify that the PDF facade publishes every pinned libHaru API function."""

import pathlib
import re
import subprocess
import sys


def facade_name(upstream_name):
    name = upstream_name[5:]
    name = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", name)
    name = re.sub(r"([A-Z])([A-Z][a-z])", r"\1_\2", name)
    return "cpkt_pdf_" + name.lower().replace("__", "_")


upstream_header, facade_header, shared_library, nm, allowlist = map(pathlib.Path, sys.argv[1:])
upstream_exports = set(re.findall(
    r"HPDF_EXPORT\([^)]*\)\s*(HPDF_\w+)\s*\(",
    upstream_header.read_text(), re.S
))
expected = {facade_name(name) for name in upstream_exports}
assert len(upstream_exports) == len(expected) == 290

declared = set(re.findall(r"\b(cpkt_pdf_[a-zA-Z0-9_]+)\s*\(",
                          facade_header.read_text()))
assert declared == expected, (
    "header missing=%s extra=%s" % (sorted(expected - declared), sorted(declared - expected))
)

symbols = subprocess.run(
    [str(nm), "-D", "--defined-only", str(shared_library)],
    check=True, capture_output=True, text=True,
).stdout
exported = {
    line.split()[-1].split("@")[0]
    for line in symbols.splitlines()
    if line.strip()
} - {"_init", "_fini"}
allowed = {
    line.strip() for line in allowlist.read_text().splitlines()
    if line.strip() and not line.lstrip().startswith("#")
}
assert allowed == expected, (
    "allowlist missing=%s extra=%s" %
    (sorted(expected - allowed), sorted(allowed - expected))
)
assert exported == expected, (
    "shared library missing=%s extra=%s" %
    (sorted(expected - exported), sorted(exported - expected))
)
print("PDF facade publishes all %d libHaru API functions" % len(expected))
