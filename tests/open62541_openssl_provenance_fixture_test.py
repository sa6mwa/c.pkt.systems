#!/usr/bin/env python3
"""Check actual Ninja/Makefiles link plans and reject wrong OpenSSL provenance."""
from pathlib import Path
import subprocess
import sys
import tempfile

if not __debug__:
    raise SystemExit("Provenance tests require Python assertions enabled")

repo = Path(sys.argv[1]).resolve()
build_root = Path(sys.argv[2]).resolve()
generator = sys.argv[3]
with tempfile.TemporaryDirectory(prefix="openssl-provenance-", dir=build_root) as work:
    root = Path(work)
    source = root / "source"
    source.mkdir()
    (source / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.21)
project(openssl_provenance C)
set(OpenSSL_DIR "${PREFIX}/lib/cmake/OpenSSL" CACHE PATH "")
set(OPENSSL_USE_STATIC_LIBS "${STATIC}" CACHE BOOL "")
foreach(component ssl crypto)
  add_library(${component}_fixture SHARED probe.c)
  set_target_properties(${component}_fixture PROPERTIES OUTPUT_NAME ${component}
    LIBRARY_OUTPUT_DIRECTORY "${PREFIX}/lib")
endforeach()
add_library(open62541 SHARED probe.c)
target_link_libraries(open62541 PRIVATE "${SSL_LIBRARY}" "${CRYPTO_LIBRARY}")
''')
    (source / "probe.c").write_text("int probe(void) { return 0; }\n")
    prefix = root / "bundled"
    ssl = prefix / "lib/libssl.so"
    crypto = prefix / "lib/libcrypto.so"

    def configure(linkage, ssl_library=ssl, crypto_library=crypto, *options):
        result = subprocess.run([
            "cmake", "-S", str(source), "-B", str(root / f"build-{linkage}"),
            "-G", generator, f"-DPREFIX={prefix}",
            f"-DSTATIC={'ON' if linkage == 'static' else 'OFF'}",
            f"-DSSL_LIBRARY={ssl_library}", f"-DCRYPTO_LIBRARY={crypto_library}",
            *options], capture_output=True, text=True)
        if result.returncode:
            raise SystemExit(result.stdout + result.stderr)

    def check(expected_error=None):
        result = subprocess.run([
            "cmake", f"-DCPKT_OPEN62541_BUILD_ROOT={root}",
            f"-DCPKT_OPENSSL_PREFIX={prefix}", "-DCPKT_SHARED_LIBRARY_SUFFIX=.so",
            "-P", str(repo / "tests/open62541_openssl_provenance_test.cmake")],
            capture_output=True, text=True)
        output = result.stdout + result.stderr
        if expected_error is None:
            if result.returncode:
                raise SystemExit(output)
        else:
            assert result.returncode and expected_error in output, output

    configure("static")
    # Existing shared objects with SONAMEs match the real imported libraries.
    # Missing dummy paths can make CMake emit -lssl instead of an absolute link.
    subprocess.run(["cmake", "--build", str(root / "build-static"),
                    "--target", "ssl_fixture", "crypto_fixture"], check=True,
                   stdout=subprocess.DEVNULL)
    configure("shared")
    check()
    configure("shared", root / "host/libssl.so")
    check("shared build does not link bundled")
    configure("shared", ssl, prefix / "lib/libcrypto.a")
    check("shared build does not link bundled")
    configure("shared", ssl, crypto, "-DOPENSSL_USE_STATIC_LIBS=ON")
    check("shared build did not select shared OpenSSL")
    configure("shared", ssl, crypto, "-DOPENSSL_USE_STATIC_LIBS=OFF",
              f"-DOpenSSL_DIR={root}/host/lib/cmake/OpenSSL")
    check("shared uses OpenSSL config")
    configure("shared", ssl, crypto, f"-DOpenSSL_DIR={prefix}/lib/cmake/OpenSSL")
    configure("static", ssl, crypto, "-DOPENSSL_USE_STATIC_LIBS=OFF")
    check("static build did not select static OpenSSL")
    configure("static", ssl, crypto, "-DOPENSSL_USE_STATIC_LIBS=ON")
    check()
    # The generator's link plan is required; cache entries alone cannot prove it.
    plan = (root / "build-shared/build.ninja" if generator == "Ninja" else
            root / "build-shared/CMakeFiles/open62541.dir/link.txt")
    plan.unlink()
    check("shared build plan is missing")
print(f"{generator}: bundled shared links pass; wrong paths/linkage and missing plans fail")
