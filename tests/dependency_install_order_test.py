#!/usr/bin/env python3
"""Prove common-prefix installs are ordered while variant builds overlap."""
from pathlib import Path
import subprocess
import sys
import tempfile

if not __debug__:
    raise SystemExit("Install-order tests require Python assertions enabled")

repo = Path(sys.argv[1]).resolve()
build_root = Path(sys.argv[2]).resolve()
generator = sys.argv[3] if len(sys.argv) > 3 else "Ninja"
with tempfile.TemporaryDirectory(prefix="install-order-", dir=build_root) as work:
    root = Path(work)
    (root / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.21)
project(install_order NONE)
include("${CPKT_REPO}/cmake/CpktDependencies.cmake")
foreach(kind static shared)
  ExternalProject_Add(${kind} SOURCE_DIR "${CMAKE_SOURCE_DIR}"
    DOWNLOAD_COMMAND "" UPDATE_COMMAND "" CONFIGURE_COMMAND ""
    BUILD_COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_BINARY_DIR} -DMODE=build-${kind} -P ${CMAKE_SOURCE_DIR}/phase.cmake
    INSTALL_COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_BINARY_DIR} -DMODE=install-${kind} -P ${CMAKE_SOURCE_DIR}/phase.cmake)
endforeach()
if(ENFORCE_ORDER)
  cpkt_order_shared_install(shared static)
endif()
''')
    (root / "phase.cmake").write_text('''if(MODE STREQUAL "build-shared")
  foreach(attempt RANGE 1 100)
    if(EXISTS "${ROOT}/static-active")
      file(WRITE "${ROOT}/shared-ready" "ready")
      return()
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 0.05)
  endforeach()
  message(FATAL_ERROR "Shared build did not overlap the static install")
elseif(MODE STREQUAL "install-static")
  file(WRITE "${ROOT}/static-active" "active")
  foreach(attempt RANGE 1 100)
    if(EXISTS "${ROOT}/shared-ready")
      execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 0.2)
      file(WRITE "${ROOT}/config" "static")
      file(REMOVE "${ROOT}/static-active")
      return()
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 0.05)
  endforeach()
  message(FATAL_ERROR "Variant builds were serialized unnecessarily")
elseif(MODE STREQUAL "install-shared")
  if(EXISTS "${ROOT}/static-active")
    message(FATAL_ERROR "Concurrent common-prefix installs")
  endif()
  file(WRITE "${ROOT}/config" "shared")
endif()
''')
    for ordered in (False, True):
        build = root / ("ordered" if ordered else "unordered")
        subprocess.run(["cmake", "-S", str(root), "-B", str(build), "-G", generator,
                        f"-DCPKT_REPO={repo}", f"-DENFORCE_ORDER={'ON' if ordered else 'OFF'}"], check=True)
        # Two workers reproduce the race without scaling the production build.
        result = subprocess.run(["cmake", "--build", str(build), "--parallel", "2"],
                                capture_output=True, text=True)
        if ordered:
            if result.returncode:
                raise SystemExit(result.stdout + result.stderr)
            assert (build / "config").read_text() == "shared"
        else:
            assert result.returncode and "Concurrent common-prefix installs" in result.stdout + result.stderr
print("Unordered installs conflict; ordered installs retain parallel builds and final shared metadata")
