#!/usr/bin/env python3
"""Create a typed, target-specific inventory of OpenSSL's public functions.

This is a development generator, not a build compiler.  It uses a compiler AST
only to preserve declarations from the pinned OpenSSL headers; the resulting
inventory is checked into no release artifact and must be consumed by the C89
facade generator before cpkt_openssl can ship.
"""

import argparse
import json
import pathlib
import re
import subprocess
import sys
from typing import Any, Dict, Iterable, List, Set


COMPATIBILITY_EXPORTS = {
    "ERR_load_CRYPTO_strings",
    "OCSP_crlID_new",
    "OPENSSL_fork_child",
    "OPENSSL_fork_parent",
    "OPENSSL_fork_prepare",
}

# These names remain in OpenSSL's dynamic ABI/export manifest for historical
# consumers, but are not declared by the configured installed public headers.
# They are either OpenSSL-internal helpers or obsolete PEM CMS compatibility
# entry points.  They are not a public API contract for a new cpkt facade.
ABI_ONLY_EXPORTS = {
    "DSO_METHOD_openssl",
    "DSO_bind_func",
    "DSO_convert_filename",
    "DSO_ctrl",
    "DSO_dsobyaddr",
    "DSO_flags",
    "DSO_free",
    "DSO_get_filename",
    "DSO_global_lookup",
    "DSO_load",
    "DSO_merge",
    "DSO_new",
    "DSO_pathbyaddr",
    "DSO_set_filename",
    "DSO_up_ref",
    "OPENSSL_DIR_end",
    "OPENSSL_DIR_read",
    "PEM_read_CMS",
    "PEM_read_bio_CMS",
    "PEM_write_CMS",
    "PEM_write_bio_CMS",
    "asn1_d2i_read_bio",
    "conf_ssl_get",
    "conf_ssl_get_cmd",
    "conf_ssl_name_find",
    "err_free_strings_int",
}


def read_num_files(paths: Iterable[pathlib.Path]) -> Set[str]:
    names: Set[str] = set()
    for path in paths:
        for line in path.read_text(encoding="utf-8").splitlines():
            fields = line.split()
            if len(fields) >= 4 and re.search(r"(^|,)EXIST::FUNCTION", fields[3]):
                names.add(fields[0])
    return names


def read_dynamic_functions(symbol_format: str, symbol_tool: str,
                           libraries: Iterable[pathlib.Path]) -> Set[str]:
    command: List[str]
    if symbol_format == "elf":
        command = [symbol_tool, "--dyn-syms", "--wide"]
    elif symbol_format == "macho":
        command = [symbol_tool, "-gU"]
    else:
        raise ValueError("unsupported symbol format: " + symbol_format)
    command.extend(str(path) for path in libraries)
    result = subprocess.run(command, check=True, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    names: Set[str] = set()
    for line in result.stdout.splitlines():
        fields = line.split()
        if symbol_format == "elf":
            if (len(fields) >= 8 and fields[3] == "FUNC" and
                    fields[4] in {"GLOBAL", "WEAK"} and fields[6] != "UND"):
                names.add(fields[7].split("@", 1)[0])
        elif len(fields) >= 3 and fields[-2] in {"T", "t"}:
            names.add(fields[-1])
    return names


def build_umbrella(include_dir: pathlib.Path, output_dir: pathlib.Path) -> pathlib.Path:
    headers = sorted((include_dir / "openssl").glob("*.h"))
    if not headers:
        raise ValueError("no OpenSSL headers found below " + str(include_dir))
    output_dir.mkdir(parents=True, exist_ok=True)
    source = output_dir / "openssl-api-inventory.c"
    source.write_text(
        "".join("#include <openssl/{}>\n".format(path.name) for path in headers)
        + "int main(void) { return 0; }\n",
        encoding="utf-8")
    return source


def walk_ast(node: Any) -> Iterable[Dict[str, Any]]:
    if not isinstance(node, dict):
        return
    yield node
    for child in node.get("inner", []):
        yield from walk_ast(child)


def function_declarations(ast: Dict[str, Any], expected: Set[str]) -> Dict[str, Dict[str, Any]]:
    declarations: Dict[str, Dict[str, Any]] = {}
    for node in walk_ast(ast):
        name = node.get("name")
        if node.get("kind") != "FunctionDecl" or name not in expected:
            continue
        type_info = node.get("type", {})
        qual_type = type_info.get("qualType")
        if not qual_type:
            continue
        parameters = []
        for child in node.get("inner", []):
            if child.get("kind") != "ParmVarDecl":
                continue
            parameters.append({
                "name": child.get("name", ""),
                "type": child.get("type", {}).get("qualType", ""),
            })
        candidate = {
            "type": qual_type,
            "parameters": parameters,
        }
        previous = declarations.get(name)
        if previous is None or len(candidate["parameters"]) > len(previous["parameters"]):
            declarations[name] = candidate
    return declarations


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--include-dir", required=True, type=pathlib.Path)
    parser.add_argument("--num", required=True, action="append", type=pathlib.Path)
    parser.add_argument("--library", required=True, action="append", type=pathlib.Path)
    parser.add_argument("--symbol-format", required=True, choices=("elf", "macho"))
    parser.add_argument("--symbol-tool", required=True)
    parser.add_argument("--clang", required=True)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--work-dir", required=True, type=pathlib.Path)
    parser.add_argument("--target")
    parser.add_argument("--sysroot", type=pathlib.Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    for path in [args.include_dir, *args.num, *args.library]:
        if not path.exists():
            raise ValueError("required input is missing: " + str(path))

    nominal = read_num_files(args.num)
    dynamic = read_dynamic_functions(args.symbol_format, args.symbol_tool, args.library)
    unexpected = dynamic - nominal - COMPATIBILITY_EXPORTS
    if unexpected:
        raise ValueError("unclassified dynamic functions: " + ", ".join(sorted(unexpected)))
    exported_public_abi = nominal & dynamic

    umbrella = build_umbrella(args.include_dir, args.work_dir)
    command = [args.clang, "-x", "c", "-std=c89", "-Wno-long-long",
               "-I", str(args.include_dir), "-Xclang", "-ast-dump=json",
               "-fsyntax-only", str(umbrella)]
    if args.target:
        command[1:1] = ["--target=" + args.target]
    if args.sysroot:
        command[1:1] = ["--sysroot=" + str(args.sysroot)]
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    if result.returncode != 0:
        raise ValueError("OpenSSL declaration AST failed:\n" + result.stderr)
    declarations = function_declarations(json.loads(result.stdout), exported_public_abi)
    unresolved = set(exported_public_abi - set(declarations))
    unclassified_unresolved = sorted(unresolved - ABI_ONLY_EXPORTS)

    inventory = {
        "schema": 1,
        "nominal_function_count": len(nominal),
        "dynamic_function_count": len(dynamic),
        "exported_public_abi_function_count": len(exported_public_abi),
        "declared_public_function_count": len(declarations),
        "feature_disabled_functions": sorted(nominal - dynamic),
        "compatibility_exports": sorted(dynamic & COMPATIBILITY_EXPORTS),
        "functions": {name: declarations[name] for name in sorted(declarations)},
        "abi_only_exports": sorted(unresolved),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(inventory, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8")
    if unclassified_unresolved:
        raise ValueError("unclassified ABI exports without a public header declaration: " +
                         ", ".join(unclassified_unresolved))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print("generate_openssl_api_inventory.py: " + str(error), file=sys.stderr)
        raise SystemExit(1)
