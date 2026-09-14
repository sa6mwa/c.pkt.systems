#!/usr/bin/env python3
"""Exercise package failures and signals in isolated process groups."""

import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time


if not __debug__:
    raise SystemExit("Package diagnostic tests require Python assertions enabled")

source = Path(sys.argv[1]).resolve()
build = source / "build"
build.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix="package diagnostics-", dir=build) as tmp:
    root = Path(tmp)
    (root / "scripts").mkdir()
    for name in ("package.sh", "osxcross_available.sh"):
        shutil.copyfile(source / "scripts" / name, root / "scripts" / name)
    tools = root / "bin"
    tools.mkdir()
    stub = r'''#!/usr/bin/env bash
set -euo pipefail
if [[ ${0##*/} == ctest ]]; then
  phase=test
elif [[ $1 == --build && ${3:-} == package-* ]]; then
  phase=package
elif [[ $1 == --build ]]; then
  phase=build
else
  phase=configure
fi
printf '%s\n' "$phase" >> "$DIAG_CALLS"
if [[ $phase == "$DIAG_PHASE" ]]; then
  case "$DIAG_MODE" in
    fail) printf 'injected command failure\n' >&2; exit "$DIAG_STATUS" ;;
    child) kill -TERM "$$"; exit 99 ;;
    parent) kill -TERM "$PPID"; exit 0 ;;
    group) : > "$DIAG_READY"; exec sleep 30 ;;
  esac
fi
'''
    for name in ("cmake", "ctest"):
        path = tools / name
        path.write_text(stub)
        path.chmod(0o755)
    cross = root / "osxcross"
    (cross / "bin").mkdir(parents=True)
    (cross / "SDK/MacOSX-test.sdk/usr/include").mkdir(parents=True)
    for name in ("clang", "clang++", "ar", "ranlib", "ld",
                 "install_name_tool", "otool"):
        path = cross / "bin" / ("arm64-apple-darwin25-" + name)
        path.write_text("#!/bin/sh\nexit 0\n")
        path.chmod(0o755)
    (root / "Makefile").write_text("all:\n\tbash scripts/package.sh\n")
    cases = []

    def run(name, phase="", mode="fail", status=23,
            signum=signal.SIGTERM, via_make=False):
        env = os.environ.copy()
        for key in ("MAKEFLAGS", "MFLAGS", "MAKELEVEL"):
            env.pop(key, None)
        ready = root / (name + ".ready")
        calls = root / (name + ".calls")
        env.update(CMAKE=str(tools / "cmake"), CTEST=str(tools / "ctest"),
                   OSXCROSS_ROOT=str(cross),
                   CPKT_OSXCROSS_HOST="arm64-apple-darwin25",
                   DIAG_PHASE=phase, DIAG_MODE=mode, DIAG_STATUS=str(status),
                   DIAG_READY=str(ready), DIAG_CALLS=str(calls))
        command = ["make", "--no-print-directory"] if via_make else [
            "bash", "scripts/package.sh"]
        process = subprocess.Popen(command, cwd=root, env=env,
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True,
                                   start_new_session=True)
        try:
            if mode == "group":
                deadline = time.monotonic() + 5
                while not ready.exists():
                    if process.poll() is not None or time.monotonic() > deadline:
                        raise AssertionError(name + ": fixture never became ready")
                    time.sleep(0.01)
                assert os.getpgid(process.pid) == process.pid
                os.killpg(process.pid, signum)
            output, _ = process.communicate(timeout=10)
        except BaseException:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.communicate(timeout=5)
            raise
        actual_calls = calls.read_text().splitlines()
        context = name + ":\n" + output
        if not phase:
            assert process.returncode == 0, context
            assert actual_calls == ["configure", "build", "test", "package"] * 6 + [
                "configure", "build", "package"], context
        else:
            assert actual_calls == ["configure", "build", "test", "package"][
                :["configure", "build", "test", "package"].index(phase) + 1], context
            prefix = "[package] " + (
                "INTERRUPTED" if mode in ("group", "parent") else "FAILED")
            diagnostics = [line for line in output.splitlines()
                           if line.startswith(prefix)]
            assert len(diagnostics) == 1, context
            diagnostic = diagnostics[0]
            assert "target=x86_64-linux-gnu-release" in diagnostic, context
            assert "phase=" + phase in diagnostic, context
            if mode in ("group", "parent"):
                expected = signal.Signals(signum).name
                assert "received " + expected in diagnostic, context
                assert "sender" in diagnostic, context
                assert process.returncode != 0, context
                if not via_make:
                    assert process.returncode == 128 + signum, context
            else:
                expected_status = 143 if mode == "child" else status
                assert process.returncode == expected_status, context
                assert "status=" + str(expected_status) in diagnostic, context
                assert "command=" in diagnostic, context
                assert "received SIGTERM" not in output, context
                if expected_status == 143:
                    assert "SIGTERM" in output and "explicit exit" in output, context
        cases.append(name)

    for phase in ("configure", "build", "test", "package"):
        run("failure-" + phase, phase)
    run("explicit-143", "test", status=143)
    run("child-term", "test", mode="child")
    run("parent-term", "test", mode="parent")
    for signum in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP):
        run(signal.Signals(signum).name, "test", mode="group", signum=signum)
    run("make-group-term", "test", mode="group", via_make=True)
    run("success")
    print("[test] package failure diagnostics: %d cases passed" % len(cases))
