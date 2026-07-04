#!/usr/bin/env bash
set -euo pipefail

SOURCE_DIR="${1:?usage: verify-clangd-surface.sh <source-dir> <build-dir>}"
BUILD_DIR="${2:?usage: verify-clangd-surface.sh <source-dir> <build-dir>}"
COMPILE_COMMANDS="${BUILD_DIR}/compile_commands.json"

if [[ ! -f "${COMPILE_COMMANDS}" ]]; then
  printf 'compile database not found: %s\n' "${COMPILE_COMMANDS}" >&2
  exit 1
fi

require_compile_command() {
  local source_file
  source_file="$1"
  if ! grep -F "\"${SOURCE_DIR}/${source_file}\"" "${COMPILE_COMMANDS}" >/dev/null; then
    printf 'compile database does not contain %s\n' "${source_file}" >&2
    exit 1
  fi
}

python3 - "$SOURCE_DIR" <<'PY'
import pathlib
import re
import sys

source_dir = pathlib.Path(sys.argv[1])


def previous_nonblank_is_comment(lines, index):
    i = index - 1
    while i >= 0 and not lines[i].strip():
        i -= 1
    return i >= 0 and lines[i].strip().endswith("*/")


def declaration_requires_comment(text):
    stripped = text.lstrip()
    if stripped.startswith(("typedef struct cpkt_", "typedef enum cpkt_")):
        return True
    if stripped.startswith("typedef ") and "cpkt_" in stripped:
        return True
    compact = " ".join(stripped.split())
    return re.match(r"^[A-Za-z_][A-Za-z0-9_\s\*]*\bcpkt_[A-Za-z0-9_]+\s*\(", compact) is not None


def function_name_from_definition(text):
    compact = " ".join(text.split())
    if compact.startswith("static "):
        return None
    match = re.match(r"^[A-Za-z_][A-Za-z0-9_\s\*]*\b(cpkt_[A-Za-z0-9_]+)\s*\(", compact)
    if match:
        return match.group(1)
    return None


def verify_header(path):
    lines = path.read_text(encoding="utf-8").splitlines()
    depth = 0
    start = None
    chunks = []
    for i, line in enumerate(lines):
        if depth == 0 and start is None and line.strip() and not line.lstrip().startswith("#"):
            start = i
        depth += line.count("{") - line.count("}")
        if start is not None and depth == 0 and ";" in line:
            chunks.append((start, "\n".join(lines[start : i + 1])))
            start = None
    failures = []
    for start_index, text in chunks:
        if declaration_requires_comment(text) and not previous_nonblank_is_comment(lines, start_index):
            failures.append((start_index + 1, text.splitlines()[0].strip()))
    return failures


def verify_source(path):
    lines = path.read_text(encoding="utf-8").splitlines()
    failures = []
    depth = 0
    start = None
    for i, line in enumerate(lines):
        stripped = line.strip()
        if depth == 0 and start is None:
            if stripped.startswith("static "):
                start = None
            elif re.match(r"^[A-Za-z_][A-Za-z0-9_\s\*]*\bcpkt_[A-Za-z0-9_]+\s*\(", stripped):
                start = i
            elif re.match(r"^cpkt_[A-Za-z0-9_]+$", stripped):
                start = i
        if start is not None and "{" in line:
            text = "\n".join(lines[start : i + 1])
            name = function_name_from_definition(text)
            if name and not previous_nonblank_is_comment(lines, start):
                failures.append((start + 1, name))
            start = None
        depth += line.count("{") - line.count("}")
        if depth == 0 and start is not None and ";" in line:
            start = None
    return failures


public_headers = sorted((source_dir / "include" / "cpkt").glob("*.h"))
facade_sources = sorted((source_dir / "src").glob("*.c"))

if not public_headers:
    print("no public facade headers found under include/cpkt", file=sys.stderr)
    sys.exit(1)
if not facade_sources:
    print("no facade source translations found under src", file=sys.stderr)
    sys.exit(1)

all_failures = []
for header in public_headers:
    for line, symbol in verify_header(header):
        all_failures.append((header, line, symbol))
for source in facade_sources:
    for line, symbol in verify_source(source):
        all_failures.append((source, line, symbol))

if all_failures:
    for path, line, symbol in all_failures:
        print(
            f"{path}:{line}: public facade symbol is missing an adjacent Doxygen comment: {symbol}",
            file=sys.stderr,
        )
    sys.exit(1)
PY

require_compile_command "examples/abi_smoke.c"
require_compile_command "examples/audio-sus-c89/main.c"
require_compile_command "examples/audio-vox-intro-c89/main.c"
require_compile_command "examples/sus-vox-intro-c89/main.c"
require_compile_command "examples/lua-runtime-c89/main.c"
require_compile_command "examples/lua-runtime-c89/host_module.c"
require_compile_command "examples/opcua-c89/main.c"

if ! command -v clangd >/dev/null 2>&1; then
  printf 'SKIP clangd --check: clangd is not installed\n'
  exit 0
fi

clangd --check="${SOURCE_DIR}/examples/abi_smoke.c" --compile-commands-dir="${BUILD_DIR}" >/dev/null
clangd --check="${SOURCE_DIR}/examples/audio-sus-c89/main.c" --compile-commands-dir="${BUILD_DIR}" >/dev/null
clangd --check="${SOURCE_DIR}/examples/audio-vox-intro-c89/main.c" --compile-commands-dir="${BUILD_DIR}" >/dev/null
clangd --check="${SOURCE_DIR}/examples/sus-vox-intro-c89/main.c" --compile-commands-dir="${BUILD_DIR}" >/dev/null
clangd --check="${SOURCE_DIR}/examples/lua-runtime-c89/main.c" --compile-commands-dir="${BUILD_DIR}" >/dev/null
clangd --check="${SOURCE_DIR}/examples/lua-runtime-c89/host_module.c" --compile-commands-dir="${BUILD_DIR}" >/dev/null
clangd --check="${SOURCE_DIR}/examples/opcua-c89/main.c" --compile-commands-dir="${BUILD_DIR}" >/dev/null
