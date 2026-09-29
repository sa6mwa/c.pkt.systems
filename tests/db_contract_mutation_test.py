#!/usr/bin/env python3
"""Exercise the real DB contract path with declaration and binding drift."""

import copy
import pathlib
import shutil
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
import db_api_contract as contract  # noqa: E402

TARGET = "x86_64-linux-gnu"


def replace_once(path, before, after):
    original = path.read_text()
    if original.count(before) != 1:
        raise RuntimeError("mutation fixture not unique: " + before)
    path.write_text(original.replace(before, after))
    return original


def rejects(action, expected):
    try:
        action()
    except RuntimeError as error:
        if expected not in str(error):
            raise RuntimeError("wrong contract rejection: " + str(error))
        return
    raise RuntimeError("contract accepted mutation: " + expected)


def main():
    scratch = contract.inventory.output_dir(TARGET).parent
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=scratch, prefix="db-mutation-") as tmp:
        temporary = pathlib.Path(tmp)
        facade = temporary / "facade/include"
        shutil.copytree(ROOT / "include/cpkt", facade / "cpkt")
        sqlite_native = temporary / "sqlite/install/include"
        sqlite_native.mkdir(parents=True)
        source_native = ROOT / ".cache/deps" / TARGET / "sqlite/install/include"
        for name in ("sqlite3.h", "sqlite3session.h", "sqlite3rtree.h", "fts5.h",
                     "sqlite3ext.h"):
            shutil.copy2(source_native / name, sqlite_native / name)

        facade_cases = (
            ("postgres.h", "cpkt_postgres_event_fire_result_create(",
             "cpkt_postgres_event_fire_result_create_REMOVED(",
             "facade signature"),
            ("postgres.h", "    cpkt_postgres_event_id id, const cpkt_postgres_event_info *info,\n    void *context);",
             "    cpkt_postgres_event_id id, const cpkt_postgres_event_info *info,\n    int context);",
             "facade signature"),
            ("sqlite.h", "  int column;\n  unsigned long operation;\n  int usable;",
             "  unsigned long operation;\n  int column;\n  int usable;",
             "facade signature"),
            ("postgres.h", "CPKT_POSTGRES_EVENT_RESULT_DESTROY = 5",
             "CPKT_POSTGRES_EVENT_RESULT_DESTROY = 6", "facade signature"),
            ("sqlite_constants.h", "#define CPKT_SQLITE_FCNTL_FILE_POINTER 7",
             "#define CPKT_SQLITE_FCNTL_FILE_POINTER 700",
             "facade constant changed"),
        )
        for filename, before, after, message in facade_cases:
            path = facade / "cpkt" / filename
            original = replace_once(path, before, after)
            try:
                rejects(lambda: contract.check(TARGET, facade_include=facade),
                        message)
            finally:
                path.write_text(original)

        native_header = sqlite_native / "sqlite3.h"
        original = replace_once(
            native_header,
            "     unsigned char op;         /* Constraint operator */\n"
            "     unsigned char usable;     /* True if this constraint is usable */",
            "     unsigned char usable;     /* True if this constraint is usable */\n"
            "     unsigned char op;         /* Constraint operator */")
        try:
            rejects(lambda: contract.check(TARGET,
                    native_includes={"sqlite": sqlite_native}),
                    "native API drift")
        finally:
            native_header.write_text(original)

        extension_header = sqlite_native / "sqlite3ext.h"
        original = replace_once(extension_header,
                                "  int  (*step)(sqlite3_stmt*);",
                                "  void (*step)(sqlite3_stmt*);")
        try:
            rejects(lambda: contract.check(TARGET,
                    native_includes={"sqlite": sqlite_native}),
                    "native API drift")
        finally:
            extension_header.write_text(original)

        original = replace_once(
            extension_header,
            "  int  (*bind_null)(sqlite3_stmt*,int);\n"
            "  int  (*bind_parameter_count)(sqlite3_stmt*);",
            "  int  (*bind_parameter_count)(sqlite3_stmt*);\n"
            "  int  (*bind_null)(sqlite3_stmt*,int);")
        try:
            rejects(lambda: contract.check(TARGET,
                    native_includes={"sqlite": sqlite_native}),
                    "native API drift")
        finally:
            extension_header.write_text(original)

        original_read = contract.read
        def read_missing_extension_field(name):
            value = original_read(name)
            if name == "db_record_bindings.json":
                value = copy.deepcopy(value)
                del value["sqlite"]["sqlite3ext.h:sqlite3_api_routines"]["step"]
            return value
        contract.read = read_missing_extension_field
        try:
            rejects(lambda: contract.check(TARGET),
                    "missing native record field binding")
        finally:
            contract.read = original_read

        def read_missing_extension_macro(name):
            value = original_read(name)
            if name == "db_constant_bindings.json":
                value = copy.deepcopy(value)
                del value["sqlite"]["SQLITE_EXTENSION_INIT2(v)"]
            return value
        contract.read = read_missing_extension_macro
        try:
            rejects(lambda: contract.check(TARGET),
                    "missing native constant decision")
        finally:
            contract.read = original_read

        def read_missing_operation(name):
            value = original_read(name)
            if name == "db_operation_bindings.json":
                value = copy.deepcopy(value)
                del value["global_config"]["SQLITE_CONFIG_MUTEX"]
            return value
        contract.read = read_missing_operation
        try:
            rejects(lambda: contract.check(TARGET),
                    "missing typed global_config operation decision")
        finally:
            contract.read = original_read
        def read_missing_variable(name):
            value = original_read(name)
            if name == "db_variable_bindings.json":
                value = copy.deepcopy(value)
                del value["sqlite"]["sqlite3_temp_directory"]
            return value
        contract.read = read_missing_variable
        try:
            rejects(lambda: contract.check(TARGET),
                    "missing native variable decision")
        finally:
            contract.read = original_read

        original = replace_once(native_header,
                                "SQLITE_API SQLITE_EXTERN char *sqlite3_temp_directory;",
                                "SQLITE_API SQLITE_EXTERN const char *sqlite3_temp_directory;")
        try:
            rejects(lambda: contract.check(TARGET,
                    native_includes={"sqlite": sqlite_native}),
                    "native API drift")
        finally:
            native_header.write_text(original)
    print("thirteen DB function, callback, nested/extension field, enum, constant, typed-operation and variable mutations rejected")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
