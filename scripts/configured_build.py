"""Resolve the configured SDK build used by source-tree contract tools.

CTest supplies its binary directory. Standalone commands use the normal
per-target preset; a missing cache is an error, never a host-tool fallback.
"""

import os
from pathlib import Path


def binary_dir(root, target):
    selected = os.environ.get("CPKT_CONFIGURED_BINARY_DIR")
    preset = "debug" if target == "x86_64-linux-gnu" else target + "-release"
    directory = Path(selected).resolve() if selected else root / "build" / preset
    if not (directory / "CMakeCache.txt").is_file():
        raise RuntimeError("configured CMake cache missing: " + str(directory))
    return directory


def cache_value(root, target, name):
    cache = (binary_dir(root, target) / "CMakeCache.txt").read_text()
    for line in cache.splitlines():
        if line.startswith(name + ":") and "=" in line:
            return line.split("=", 1)[1]
    raise RuntimeError("configured {} missing for {}".format(name, target))


def scratch_dir(root, target, name):
    return binary_dir(root, target) / name
