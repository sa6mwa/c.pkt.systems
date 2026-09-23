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

python3 - "$SOURCE_DIR" "$BUILD_DIR" <<'PY'
import pathlib
import re
import sys

source_dir = pathlib.Path(sys.argv[1])
build_dir = pathlib.Path(sys.argv[2])


def previous_nonblank_is_doxygen_comment(lines, index):
    i = index - 1
    while i >= 0 and not lines[i].strip():
        i -= 1
    if i < 0:
        return False

    stripped = lines[i].lstrip()
    if stripped.startswith(("///", "//!")):
        return True
    if not stripped.endswith("*/"):
        return False

    while i >= 0:
        stripped = lines[i].lstrip()
        if stripped.startswith(("/**", "/*!")):
            return True
        if stripped.startswith("/*"):
            return False
        i -= 1
    return False


def first_declaration_line(lines, start, end):
    i = start
    in_block_comment = False
    while i <= end:
        stripped = lines[i].lstrip()
        if in_block_comment:
            if "*/" in stripped:
                in_block_comment = False
            i += 1
            continue
        if not stripped or stripped.startswith("//"):
            i += 1
            continue
        if stripped.startswith("/*"):
            if "*/" not in stripped:
                in_block_comment = True
            i += 1
            continue
        return i
    return start


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
            declaration_start = first_declaration_line(lines, start, i)
            chunks.append((declaration_start, "\n".join(lines[declaration_start : i + 1])))
            start = None
    # A public header can document a coherent API family with a Doxygen group
    # rather than repeating an empty sentence on every function declaration.
    # SQLite's broad surface uses group contracts for ownership and callback
    # lifetime, with declaration-local comments where a contract differs.
    # Existing smaller facade headers may continue using local documentation.
    if "@defgroup cpkt_sqlite" in "\n".join(lines):
        return []
    failures = []
    for start_index, text in chunks:
        if declaration_requires_comment(text) and not previous_nonblank_is_doxygen_comment(lines, start_index):
            failures.append((start_index + 1, text.splitlines()[0].strip()))
    return failures


def verify_source(path, documented_symbols):
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
            # SQLite's broad installed header uses a Doxygen group contract;
            # its implementation intentionally does not duplicate it.
            # Every other facade source keeps a declaration-local Doxygen
            # comment, which also protects this verifier's negative fixture.
            if name and (name not in documented_symbols or
                         path.name != "sqlite.c") and not previous_nonblank_is_doxygen_comment(
                             lines, start):
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
documented_symbols = set()
for header in public_headers:
    for line, symbol in verify_header(header):
        all_failures.append((header, line, symbol))
    documented_symbols.update(
        re.findall(r"\b(cpkt_[A-Za-z0-9_]+)\s*\(",
                   header.read_text(encoding="utf-8"))
    )
for source in facade_sources:
    for line, symbol in verify_source(source, documented_symbols):
        all_failures.append((source, line, symbol))

# The complete Lua C89 header is generated in the build tree, so the installed
# header scan above cannot see its function comments.
lua_header = build_dir / "generated/lua/include/cpkt/lua.h"
if not lua_header.is_file():
    print(f"generated Lua facade header not found: {lua_header}", file=sys.stderr)
    sys.exit(1)
lua_lines = lua_header.read_text(encoding="utf-8").splitlines()
lua_declarations = 0
for index, line in enumerate(lua_lines):
    if line.startswith("CPKT_LUA_API "):
        lua_declarations += 1
        if not previous_nonblank_is_doxygen_comment(lua_lines, index):
            all_failures.append((lua_header, index + 1, line.strip()))
if lua_declarations < 156:
    print(f"generated Lua facade has only {lua_declarations} public declarations", file=sys.stderr)
    sys.exit(1)

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
  printf 'clangd is required for make clangd-surface; install it with the host OS package manager\n' >&2
  exit 1
fi

clangd --check="${SOURCE_DIR}/examples/abi_smoke.c" --compile-commands-dir="${BUILD_DIR}" >/dev/null
clangd --check="${SOURCE_DIR}/examples/audio-sus-c89/main.c" --compile-commands-dir="${BUILD_DIR}" >/dev/null
clangd --check="${SOURCE_DIR}/examples/audio-vox-intro-c89/main.c" --compile-commands-dir="${BUILD_DIR}" >/dev/null
clangd --check="${SOURCE_DIR}/examples/sus-vox-intro-c89/main.c" --compile-commands-dir="${BUILD_DIR}" >/dev/null
clangd --check="${SOURCE_DIR}/examples/lua-runtime-c89/main.c" --compile-commands-dir="${BUILD_DIR}" >/dev/null
clangd --check="${SOURCE_DIR}/examples/lua-runtime-c89/host_module.c" --compile-commands-dir="${BUILD_DIR}" >/dev/null
clangd --check="${SOURCE_DIR}/examples/opcua-c89/main.c" --compile-commands-dir="${BUILD_DIR}" >/dev/null
