#!/usr/bin/env python3
"""Verify reviewed PostgreSQL/SQLite bindings against one configured target.

The native and facade baselines are deliberately separate. A provider change
needs an explicit binding edit; a public facade deletion or type/layout change
fails even when the native declarations have not changed.
"""

import argparse
import ast
import json
import pathlib
import re
import subprocess

import db_api_inventory as inventory

ROOT = pathlib.Path(__file__).resolve().parent.parent
CONTRACTS = ROOT / "tests/contracts"


def read(name):
    return json.loads((CONTRACTS / name).read_text())


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def string_macro(value):
    chunks = re.findall(r'"(?:\\.|[^"\\])*"', value)
    return "".join(ast.literal_eval(chunk) for chunk in chunks)


def compile_probe(target, provider, lines, native=False,
                  facade_include=None, native_include=None):
    path = inventory.output_dir(target).parent / "contract" / target
    path.mkdir(parents=True, exist_ok=True)
    file = path / (provider + ("-native" if native else "-facade") + ".c")
    file.write_text("\n".join(lines) + "\n")
    command = [inventory.compiler(target), "-std=gnu99" if native else "-std=c89",
               "-pedantic-errors" if not native else "-Werror", "-Werror",
               "-I", str(facade_include or ROOT / "include")]
    if native:
        command += ["-I", str(native_include or ROOT / ".cache/deps" / target /
                               inventory.DEPS[provider] / "install/include")]
        if provider == "postgres":
            command += ["-I", str(ROOT / ".cache/deps-build" / target /
                           "postgresql/src/src/include")]
        command += list(inventory.DEFINES[provider])
    command += ["-c", str(file), "-o", str(file.with_suffix(".o"))]
    result = subprocess.run(command, text=True, cwd=ROOT,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    require(result.returncode == 0,
            "{} {} C89/constant probe failed:\n{}".format(
                target, provider, result.stderr))


def check(target, facade_include=None, native_includes=None):
    native_snap = read("db_native_api.json")[target]
    facade_snap = read("db_facade_api.json")[target]
    functions = read("db_function_bindings.json")
    variables = read("db_variable_bindings.json")
    records = read("db_record_bindings.json")
    typedefs = read("db_typedef_bindings.json")
    enums = read("db_enum_bindings.json")
    constants = read("db_constant_bindings.json")
    operations = read("db_operation_bindings.json")
    catalogs = {}
    for provider in ("postgres", "sqlite"):
        native = inventory.inspect(target, provider,
                                   include_override=(native_includes or {}).get(provider))
        facade = inventory.inspect(target, provider, facade=True,
                                   include_override=facade_include)
        require(native == native_snap[provider],
                "{} {} native API drift: inspect snapshot and binding".format(
                    target, provider))
        require(facade == facade_snap[provider],
                "{} {} facade signature, field, enum, or layout drift".format(
                    target, provider))
        require(set(functions[provider]) == set(native["functions"]),
                provider + " missing native function decision")
        require(set(variables[provider]) == set(native["variables"]),
                provider + " missing native variable decision")
        require(set(records[provider]) == set(native["records"]),
                provider + " missing native record decision")
        require(set(typedefs[provider]) == set(native["typedefs"]),
                provider + " missing native typedef/callback decision")
        require(set(constants[provider]) == set(native["macros"]),
                provider + " missing native constant decision")
        require(set(enums[provider]) == set(native["enums"]),
                provider + " missing native enum decision")
        catalog = set((ROOT / "cmake/exports" /
                       ("cpkt_" + provider + ".txt")).read_text().split())
        require(catalog == set(facade["functions"]),
                provider + " export catalog differs from public functions")
        catalogs[provider] = catalog
        accessible = set(catalog)
        for record, fields in facade["records"].items():
            prefix = record.split(":", 1)[1]
            accessible.update(prefix + "." + field for field in fields)
        for name, decision in functions[provider].items():
            require(("facade" in decision) != ("exclude" in decision),
                    "ambiguous function decision " + name)
            for binding in decision.get("facade", []):
                require(binding in accessible,
                        "missing callable facade binding {} -> {}".format(
                            name, binding))
        for name, decision in variables[provider].items():
            require(("facade" in decision) != ("exclude" in decision),
                    "ambiguous variable decision " + name)
            for binding in decision.get("facade", []):
                require(binding in accessible,
                        "missing callable variable binding {} -> {}".format(
                            name, binding))
        for record, fields in native["records"].items():
            require(set(records[provider][record]) == set(fields),
                    "missing native record field binding " + record)
            for field, decision in records[provider][record].items():
                require(sum(key in decision for key in ("facade", "fixed", "exclude")) == 1,
                        "ambiguous field binding {}.{}".format(record, field))
                if "facade" in decision:
                    require(decision["facade"] in accessible,
                            "missing facade record field {}.{}".format(record, field))
        for name, decision in typedefs[provider].items():
            require(sum(key in decision for key in ("facade", "scalar", "exclude")) == 1,
                    "ambiguous typedef binding " + name)
            if "facade" in decision:
                require(decision["facade"] in facade["typedefs"],
                        "missing facade typedef/callback " + name)
        for name, binding in enums[provider].items():
            require(binding in facade["enums"] and
                    native["enums"][name] == facade["enums"][binding],
                    "enum binding/value differs: " + name)
        for name, decision in constants[provider].items():
            require(native["macros"][name] == decision["native_value"],
                    "native constant changed: " + name)
            require(sum(key in decision for key in ("facade", "equivalent", "exclude")) == 1,
                    "ambiguous constant decision: " + name)
        macro_text = inventory.run(
            [inventory.compiler(target), "-dM", "-E", "-x", "c", "-I",
             str(facade_include or ROOT / "include"), "-"],
            "#include <cpkt/{}.h>\n".format(provider))
        facade_macros = dict(re.findall(r"^#define[ \t]+(\w+)[ \t]+([^\r\n]+)$",
                                        macro_text, re.MULTILINE))
        for name, decision in constants[provider].items():
            if "facade" not in decision:
                continue
            facade_name = decision["facade"]
            if facade_name in facade["enums"]:
                continue
            require(facade_name in facade_macros,
                    "missing facade constant: " + facade_name)
            if "facade_value" in decision:
                actual_value = facade_macros[facade_name]
                expected_value = decision["facade_value"]
                if expected_value.startswith('"'):
                    actual_value = string_macro(actual_value)
                    expected_value = string_macro(expected_value)
                else:
                    actual_value = re.sub(r"\s+", "", actual_value)
                    expected_value = re.sub(r"\s+", "", expected_value)
                require(actual_value == expected_value,
                        "facade constant changed: " + facade_name)
            if native["macros"][name].startswith('"'):
                require(string_macro(facade_macros[facade_name]) ==
                        string_macro(native["macros"][name]),
                        "string constant binding changed: " + name)
        if provider == "sqlite":
            prefixes = {
                "global_config": "SQLITE_CONFIG_",
                "database_config": "SQLITE_DBCONFIG_",
                "virtual_table_config": "SQLITE_VTAB_",
                "file_control": "SQLITE_FCNTL_",
            }
            for group, prefix in prefixes.items():
                expected = {name for name in native["macros"]
                            if name.startswith(prefix)}
                require(set(operations[group]) == expected,
                        "missing typed {} operation decision".format(group))
                for name, decision in operations[group].items():
                    require(("facade" in decision) != ("exclude" in decision),
                            "ambiguous operation decision: " + name)
                    if "facade" in decision:
                        require(decision["facade"] in accessible,
                                "missing typed operation binding: " + name)

        typed = ["#include <cpkt/{}.h>".format(provider),
                 "void cpkt_db_contract_probe(void) {"]
        for name in sorted(catalog):
            typed.append("  (void)&{};".format(name))
        for record, fields in sorted(facade["records"].items()):
            type_name = record.split(":", 1)[1]
            for field in fields:
                typed.append("  (void)sizeof((({} *)0)->{});".format(
                    type_name, field))
        for name in sorted(facade["typedefs"]):
            typed.append("  (void)sizeof({} *);".format(name))
        typed.append("}")
        compile_probe(target, provider, typed, facade_include=facade_include)

        native_lines = ["#include <cpkt/{}.h>".format(provider)]
        if provider == "postgres":
            native_lines += ["#include <limits.h>",
                             "#include <libpq-fe.h>",
                             "#include <libpq-events.h>",
                             "#include <postgres_ext.h>",
                             "#include <libpq/libpq-fs.h>"]
        else:
            native_lines += ["#include <sqlite3.h>",
                             "#include <sqlite3session.h>",
                             "#include <sqlite3rtree.h>",
                             "#include <fts5.h>",
                             "#include <sqlite3ext.h>"]
        number = 0
        for name, decision in constants[provider].items():
            if "facade" not in decision:
                continue
            facade_name = decision["facade"]
            if not re.fullmatch(r"[A-Za-z_]\w*", facade_name):
                continue
            if native["macros"][name].startswith('"'):
                native_lines.append("void *cpkt_db_string_{} = (void *)&{};".format(
                    number, facade_name))
                number += 1
                continue
            native_lines.append("typedef char cpkt_db_constant_{}[(({}) == ({})) ? 1 : -1];".format(
                number, name, facade_name))
            number += 1
        native_lines.append("void cpkt_db_native_probe(void) {}")
        compile_probe(target, provider, native_lines, native=True,
                      facade_include=facade_include,
                      native_include=(native_includes or {}).get(provider))
        print("{} {}: {} native functions, {} records, {} typedefs, {} enums, {} constants; {} facade functions".format(
            target, provider, len(native["functions"]), len(native["records"]),
            len(native["typedefs"]), len(native["enums"]),
            len(native["macros"]), len(facade["functions"])))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", choices=inventory.TARGETS, required=True)
    args = parser.parse_args()
    check(args.target)


if __name__ == "__main__":
    main()
