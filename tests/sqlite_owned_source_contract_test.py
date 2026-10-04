#!/usr/bin/env python3
"""Exercise SQLite's real recipe registration and owned glue warning policy."""

from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

repo = Path(sys.argv[1]).resolve()
configured = Path(sys.argv[2]).resolve()
recipe = (repo / "cmake/CpktDependencies.cmake").read_text()


def cache_value(build, key):
    match = re.search(r"^" + re.escape(key) + r":[^=]+=(.*)$",
                      (build / "CMakeCache.txt").read_text(), re.M)
    if not match:
        raise RuntimeError("configured {} missing in {}".format(key, build))
    return match.group(1)


def registration(name):
    # Extract the actual registration, including its INPUT_FILES. The fixture
    # deliberately has no duplicate list that could pass after recipe drift.
    match = re.search(r"  cpkt_prepare_dependency_component\(\s*NAME " + name +
                      r"\s+.*?RECIPE_FUNCTIONS .*?\)", recipe, re.S)
    if not match:
        raise RuntimeError("real " + name + " registration missing")
    return match.group(0)


def run(command, **kwargs):
    result = subprocess.run(command, capture_output=True, text=True, **kwargs)
    if result.returncode:
        raise RuntimeError("{} failed:\n{}{}".format(
            " ".join(map(str, command)), result.stdout, result.stderr))
    return result.stdout + result.stderr


def compile_command(build, target):
    generator = cache_value(build, "CMAKE_GENERATOR")
    make_program = cache_value(build, "CMAKE_MAKE_PROGRAM")
    if generator == "Ninja":
        commands = run([make_program, "-C", str(build), "-t", "commands", target])
    elif generator == "Unix Makefiles":
        makefile = build / "CMakeFiles" / (target + ".dir") / "build.make"
        commands = makefile.read_text()
    else:
        raise RuntimeError("unsupported configured generator: " + generator)
    for line in commands.splitlines():
        for segment in line.strip().split(" && "):
            if "sqlite_native_amalgamation.c" in segment and " -c " in segment:
                arguments = shlex.split(segment)
                if len(arguments) > 4 and Path(arguments[1]).name == 'cpkt_build_guard.py':
                    if Path(arguments[2]).resolve() != repo or arguments[3] != 'db':
                        raise RuntimeError('unexpected production launcher context')
                    arguments = arguments[4:]
                if "-c" in arguments and "-o" in arguments:
                    return arguments
    raise RuntimeError("SQLite compile recipe missing in " + str(build))


with tempfile.TemporaryDirectory(prefix="sqlite-owned-input-", dir=configured) as name:
    root = Path(name)
    compiler = cache_value(configured, "CMAKE_C_COMPILER")
    for generator, program in (("Ninja", "ninja"),
                               ("Unix Makefiles", "make")):
        source = root / ("source-" + program)
        binary = root / ("binary-" + program)
        (source / "cmake").mkdir(parents=True)
        (source / "src").mkdir()
        for path in ("CpktDependencies.cmake", "CpktDependencyContract.cmake",
                     "CpktDependencyArchiveCache.cmake", "patch_zlib_single_pass.cmake"):
            shutil.copy2(repo / "cmake" / path, source / "cmake" / path)
        helper = source / "src/sqlite_native_amalgamation.c"
        shutil.copy2(repo / "src/sqlite_native_amalgamation.c", helper)
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.21)\n"
            "project(sqlite_owned_contract NONE)\n"
            "set(CPKT_TARGET_ID x86_64-linux-gnu)\n"
            "set(CPKT_BUILD_DEPENDENCIES ON)\n"
            "set(CPKT_DEPENDENCY_BUILD_ROOT_LIFECYCLE_OWNED ON)\n"
            "set(CPKT_EXTERNAL_ROOT_LIFECYCLE_OWNED ON)\n"
            "set(CPKT_DEPENDENCY_BUILD_ROOT \"${CMAKE_SOURCE_DIR}/.cache/deps-build/x86_64-linux-gnu\")\n"
            "set(CPKT_EXTERNAL_ROOT \"${CMAKE_SOURCE_DIR}/.cache/deps/x86_64-linux-gnu\")\n"
            "set(CPKT_DEPENDENCY_CONTRACT_ROOT \"${CMAKE_SOURCE_DIR}/.cache/dependency-contracts\")\n"
            "include(cmake/CpktDependencyContract.cmake)\n"
            + registration("zlib") + "\n" + registration("sqlite") + "\n"
            "add_custom_target(contract_probe COMMAND ${CMAKE_COMMAND} -E true)\n"
            "add_custom_target(recipe_probe COMMAND \"${CPKT_TEST_COMPILER}\" -c \"${CMAKE_SOURCE_DIR}/src/sqlite_native_amalgamation.c\" -o \"${CMAKE_BINARY_DIR}/fixture.o\")\n")
        make_program = (cache_value(configured, "CMAKE_MAKE_PROGRAM")
                        if cache_value(configured, "CMAKE_GENERATOR") == generator
                        else shutil.which(program))
        if not make_program:
            raise RuntimeError("required build program missing: " + program)
        run(["cmake", "-S", str(source), "-B", str(binary), "-G", generator,
             "-DCMAKE_MAKE_PROGRAM=" + make_program,
             "-DCPKT_TEST_COMPILER=" + compiler])
        probe = compile_command(binary, "recipe_probe")
        if probe[0] != compiler or probe[probe.index("-c") + 1] != str(helper):
            raise RuntimeError(generator + " compile recipe extraction failed")
        contract = source / ".cache/dependency-contracts/x86_64-linux-gnu"
        before = (contract / "sqlite.txt").read_text()
        assert "src/sqlite_native_amalgamation.c" in before
        zlib_before = (contract / "zlib.txt").read_text()
        markers = []
        for component in ("sqlite", "zlib"):
            marker = source / ".cache/deps-build/x86_64-linux-gnu" / component / "fixture-marker"
            marker.parent.mkdir(parents=True, exist_ok=True)
            marker.write_text(component)
            markers.append(marker)
        run(["cmake", "--build", str(binary), "--target", "contract_probe"])
        assert all(marker.exists() for marker in markers)
        helper.write_text(helper.read_text() + "\n/* contract fixture edit */\n")
        output = run(["cmake", "--build", str(binary), "--target", "contract_probe"])
        assert "Refreshed dependency sqlite: effective inputs changed" in output, output
        assert not markers[0].exists() and markers[1].exists()
        assert (contract / "sqlite.txt").read_text() != before
        assert (contract / "zlib.txt").read_text() == zlib_before
        print(generator + ": actual SQLite registration auto-invalidated only SQLite")

    # Take the compile command from the configured production ExternalProject
    # rule, retaining its pinned compiler, target flags and staged include.
    producer = repo / 'build' / cache_value(configured,'CPKT_TARGET_ID') / 'db/producer'
    command = compile_command(producer, "cpkt_sqlite_project")
    if command[0] != compiler:
        raise RuntimeError("SQLite compilation did not select the configured compiler")
    source_index = command.index("-c") + 1
    object_index = command.index("-o") + 1
    warning = root / "warning.c"
    warning.write_text((repo / "src/sqlite_native_amalgamation.c").read_text()
                       + "\nstatic int cpkt_fixture_warning = 1;\n"
                         "int cpkt_fixture_extra(int unused_parameter) { return 1; }\n")
    command[source_index] = str(warning)
    command[object_index] = str(root / "warning.o")
    rejected = subprocess.run(command, capture_output=True, text=True)
    if (rejected.returncode == 0 or "cpkt_fixture_warning" not in rejected.stderr
            or "unused_parameter" not in rejected.stderr or "error:" not in rejected.stderr):
        raise RuntimeError("owned SQLite warning escaped production policy:\n" + rejected.stderr)
print("configured production compiler rejects owned -Wall/-Wextra warnings")
