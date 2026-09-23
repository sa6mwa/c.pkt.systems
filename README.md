# c.pkt.systems

Pinned C dependency bundle for pkt.systems C projects.

The project builds release artifacts for:

- OpenSSL
- zlib
- libpng, with its complete public C API
- libHaru, with a complete strict C89 `cpkt_pdf` facade
- nghttp2
- libssh2
- curl
- libxml2
- Lua, with the complete strict-C89 `cpkt_lua` facade and the higher-level
  strict-C89 `cpkt_lua_runtime` embedding facade
- miniaudio, behind the strict C89 `cpkt_audio` facade
- whisper.cpp/ggml, behind the strict C89 `cpkt_sus` facade
- MQTT-C
- open62541
- cmocka, for test builds on Linux targets

Release tarballs always contain the complete installable SDK surface: headers,
static archives, shared libraries, CMake package metadata, and pkg-config
metadata. Partial static-only or shared-only dependency bundles are not
supported.

It does not build or package project-level pkt.systems libraries such as
`lonejson` or `libpslog`; those are released and consumed independently.

## Targets

The release matrix is:

- `x86_64-linux-gnu`
- `x86_64-linux-musl`
- `aarch64-linux-gnu`
- `aarch64-linux-musl`
- `armhf-linux-gnu`
- `armhf-linux-musl`
- `arm64-apple-darwin`

Every Linux configure, build, package smoke, and staged ABI check resolves its
complete compiler collection from the shared pinned Bootlin cache:

```sh
${CPKT_TOOLCHAIN_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/c.pkt.systems/toolchains}
```

The resolver downloads and SHA-256-verifies the collection when needed; host
GCC, Clang, and binutils are never Linux build fallbacks. Every Linux
compile uses its pinned Bootlin GCC collection; Darwin stays
local-osxcross-only and does not download an Apple SDK.

Pinned third-party source archives use a separate shared verified cache:

```sh
${CPKT_DEPENDENCY_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/c.pkt.systems/deps}
```

CMake hashes every cache hit, downloads a miss to a temporary file, verifies
the pinned SHA-256, and atomically publishes the archive. Extraction, build,
and install trees remain under this repository's `.cache/` and are disposable;
`make clean` and `make release` never remove the shared archive cache. Set
`-DCPKT_DEPENDENCY_CACHE=/path/to/deps` to use a different shared cache.

The Linux compiler collections are pinned to Bootlin stable `2026.08-1`
(GCC 15.3.0, binutils 2.45.1, Linux headers 5.10.269, glibc 2.44 or musl 1.2.6).
The repository resolver and bundled lifecycle skill use the same archive hashes.
AFL++ is built for the selected GCC collection; its plugin cache is not reused
across collection changes.

The current glibc SDKs require **glibc 2.43 or newer** for the complete shared
library dependency set on x86_64, aarch64, and armhf. The published 0.9.0 SDKs
required at most glibc 2.38. This is a deployment compatibility change despite
unchanged facade ABI majors and `libc.so.6`. The SDK does not bundle glibc.
Linking SDK static archives into an otherwise dynamically linked executable does
not remove its libc requirement. Fully static musl builds avoid a dynamic glibc
requirement; their OS and runtime behavior still needs application verification.

## Release Workflow

```sh
make prerelease
make release
```

`make prerelease` runs the complete release proof graph without first deleting
generated state: formatting, deterministic debug and clangd checks, native
Valgrind and AFL++ smoke checks, then the release matrix. It therefore produces
and verifies the same package set as the final gate. `make release` first runs
the release-version contract check, then removes repository-local generated
state and invokes that same proof graph. It builds and verifies local artifacts;
it does not publish a release, push commits, or create a release tag.

### Release diagnostics

The package matrix reports each target and phase, runs CTest verbosely to retain
individual test-case progress, and reports a failed command's exit status or a
signal received by the package script. Failures stop the matrix immediately.
An exit status such as 143 can mean SIGTERM or an explicit `exit(143)`; it cannot
identify who sent a signal. Shell traps likewise cannot recover sender identity.

For an unexplained interruption on Linux, capture signals **before** reproducing
it with the host's `strace`:

```sh
strace --seccomp-bpf -f -tt -Y -e trace=kill,tgkill,tkill \
  -e signal=SIGTERM,SIGINT,SIGHUP,SIGQUIT \
  make release
```

The trace records signal recipients, sender PIDs and process names when available.
Correlate these with the package phase and PID in the terminal output. Intentional
test-child cleanup also appears in the trace; a SIGTERM entry alone is not a
release failure. Retain the terminal output and do not label an unexplained interruption
as a fixed flake merely because a subsequent run passes.

Scanners and verification helpers keep repository-local scratch under the
gitignored `build/`, never beside source files or at the repository root. Focused
checks may save logs there too. Stream a full clean release to the terminal:
`make release` deletes `build/`, so a log opened there before the clean would be
removed during the run.

### Versions and artifacts

An untagged Git checkout produces version `0.0.0`, suitable for a local rehearsal.
A lightweight `vX.Y.Z` tag pointing at HEAD supplies the release version;
annotated tags do not qualify. Outside this repository's Git worktree, source
archives use their injected `VERSION` file. Use `make print-release-version` to
check the selected version before packaging.

`make build` and `make test` cover the six Linux targets. `make package`,
`make release-matrix`, and the full release gates also require Darwin.
Cross-target tests and package consumers use `/usr/bin/qemu-aarch64` and
`/usr/bin/qemu-arm` by default. Set `CPKT_QEMU_AARCH64` and `CPKT_QEMU_ARM`
to executable paths when QEMU is installed elsewhere; use those values for
configure, test, and package verification.

The release matrix builds each dependency tree, runs the ABI/link smoke tests
where the target can execute locally, writes `dist/c.pkt.systems-<version>-<target>.tar.gz`,
writes `dist/c.pkt.systems-<version>.tar.gz` for source builds, writes
`dist/c.pkt.systems-<version>-CHECKSUMS`, and verifies the archive contents.
It also packages the Darwin
`dist/c.pkt.systems-<version>-arm64-apple-darwin-smoke-test.zip`.
The c.pkt.systems bundle release requires all listed Linux targets and
`arm64-apple-darwin`; a missing osxcross SDK is a release failure, never a skip.
Package verification also extracts each binary tarball and builds downstream
CMake and pkg-config consumers for every shipped dependency package, asserting
that static link requirements propagate through the shipped metadata. Linux
consumers are run when executable locally or through the configured emulator;
Darwin consumers are configure/link checked with the required local osxcross
toolchain. Source
archive verification extracts the source tarball, checks its `RELEASE_MANIFEST`,
verifies that non-git version resolution uses the injected `VERSION` file, and
builds/runs the facade-only local tests from the extracted tree.

To build only the source archive:

```sh
make package-source
make package-source-smoke
```

To verify existing archives:

```sh
make verify-release-archives
```

## SDK Layout

Each archive extracts to one directory:

```text
c.pkt.systems-<version>-<target>/
```

The installable SDK surface is under that root:

```text
include/
lib/
lib/cmake/OpenSSL/OpenSSLConfig.cmake
lib/cmake/OpenSSL/OpenSSLConfigVersion.cmake
lib/cmake/CpktOpenSSL/CpktOpenSSLConfig.cmake
lib/cmake/CpktPng/CpktPngConfig.cmake
lib/cmake/CpktHaru/CpktHaruConfig.cmake
lib/cmake/CpktPdf/CpktPdfConfig.cmake
lib/cmake/CpktNghttp2/CpktNghttp2Config.cmake
lib/cmake/CpktLibssh2/CpktLibssh2Config.cmake
lib/cmake/CpktMqttc/CpktMqttcConfig.cmake
lib/cmake/CpktSqlite/CpktSqliteConfig.cmake
lib/cmake/CpktSasl/CpktSaslConfig.cmake
lib/cmake/CpktGssapi/CpktGssapiConfig.cmake
lib/cmake/CpktPostgres/CpktPostgresConfig.cmake
lib/cmake/zlib/ZLIBConfig.cmake
lib/cmake/zlib/ZLIBConfigVersion.cmake
lib/cmake/nghttp2/nghttp2Config.cmake
lib/cmake/nghttp2/nghttp2ConfigVersion.cmake
lib/cmake/libssh2/libssh2-config.cmake
lib/cmake/libssh2/libssh2-config-version.cmake
lib/cmake/CURL/CURLConfig.cmake
lib/cmake/CURL/CURLConfigVersion.cmake
lib/cmake/libxml2/libxml2-config.cmake
lib/cmake/libxml2/libxml2-config-version.cmake
lib/cmake/Lua/LuaConfig.cmake
lib/cmake/Lua/LuaConfigVersion.cmake
lib/cmake/CpktLua/CpktLuaConfig.cmake
lib/cmake/CpktLua/CpktLuaConfigVersion.cmake
lib/cmake/CpktLuaRuntime/CpktLuaRuntimeConfig.cmake
lib/cmake/CpktLuaRuntime/CpktLuaRuntimeConfigVersion.cmake
lib/cmake/CpktAudio/CpktAudioConfig.cmake
lib/cmake/CpktAudio/CpktAudioConfigVersion.cmake
lib/cmake/CpktSus/CpktSusConfig.cmake
lib/cmake/CpktSus/CpktSusConfigVersion.cmake
lib/cmake/CpktOpcUa/CpktOpcUaConfig.cmake
lib/cmake/CpktOpcUa/CpktOpcUaConfigVersion.cmake
lib/cmake/mqtt-c/mqtt-cConfig.cmake
lib/cmake/mqtt-c/mqtt-cConfigVersion.cmake
lib/cmake/open62541/open62541Config.cmake
lib/cmake/open62541/open62541ConfigVersion.cmake
lib/pkgconfig/libcrypto.pc
lib/pkgconfig/libssl.pc
lib/pkgconfig/openssl.pc
lib/pkgconfig/cpkt-openssl.pc
lib/pkgconfig/cpkt-png.pc
lib/pkgconfig/cpkt-haru.pc
lib/pkgconfig/cpkt-pdf.pc
lib/pkgconfig/cpkt-nghttp2.pc
lib/pkgconfig/cpkt-libssh2.pc
lib/pkgconfig/cpkt-mqttc.pc
lib/pkgconfig/cpkt-lua.pc
lib/pkgconfig/cpkt-sqlite.pc
lib/pkgconfig/cpkt-sasl.pc
lib/pkgconfig/cpkt-gssapi.pc
lib/pkgconfig/cpkt-postgres.pc
lib/pkgconfig/zlib.pc
lib/pkgconfig/libnghttp2.pc
lib/pkgconfig/libssh2.pc
lib/pkgconfig/libcurl.pc
lib/pkgconfig/libxml-2.0.pc
lib/pkgconfig/lua.pc
lib/pkgconfig/lua5.5.pc
lib/pkgconfig/cpkt-lua-runtime.pc
lib/pkgconfig/cpkt-audio.pc
lib/pkgconfig/cpkt-sus.pc
lib/pkgconfig/cpkt-opcua.pc
lib/pkgconfig/mqtt-c.pc
lib/pkgconfig/open62541.pc
share/c.pkt.systems/manifest.txt
share/doc/c.pkt.systems/third_party/<dependency>/LICENSE
```

The CMake package directory names intentionally mirror upstream packages:
OpenSSL and CURL use capitalized package directories, zlib and libssh2 use
lowercase package directories with their upstream config filename casing, and
nghttp2 uses its lowercase package directory.

## Consuming With CMake

Point CMake at the extracted SDK root and link the package target you need:

```sh
cmake -S examples/cmake-consumer -B build/cpkt-example \
  -DCMAKE_PREFIX_PATH=/path/to/c.pkt.systems-<version>-<target>
cmake --build build/cpkt-example
```

The CMake packages export these standard targets:

```text
OpenSSL::Crypto
OpenSSL::SSL
ZLIB::ZLIB
nghttp2::nghttp2
Libssh2::libssh2
CURL::libcurl
LibXml2::LibXml2
cpkt::png
cpkt::haru
cpkt::pdf
cpkt::openssl
cpkt::sqlite
cpkt::sasl
cpkt::gssapi
cpkt::postgres
Lua::Lua
cpkt::lua
cpkt::lua_runtime
cpkt::audio
cpkt::sus
MQTT-C::mqttc
open62541::open62541
cpkt::opcua
```

Static transitive dependencies are part of the imported targets. Consumers
should not add private workaround libraries such as `-ldl`, `-pthread`,
`-latomic`, zlib, nghttp2, libssh2, OpenSSL, or Darwin frameworks by hand.
For PDF generation, use `find_package(CpktPdf CONFIG REQUIRED)` and link
`cpkt::pdf` (or `cpkt::pdf_shared`). The full upstream libpng surface is
available through `find_package(CpktPng CONFIG REQUIRED)` and `cpkt::png`.
The C89 facade API and its transitive dependencies are described in
[`docs/pdf-c89-facade.md`](docs/pdf-c89-facade.md).
The other facade contracts are documented in the matching files under `docs/`:
[`PostgreSQL`](docs/postgres-c89-facade-spec.md),
[`SQLite`](docs/sqlite-c89-facade-spec.md),
[`SASL`](docs/sasl-c89-facade-spec.md),
[`GSSAPI`](docs/gssapi-c89-facade-spec.md),
[`OpenSSL`](docs/openssl-c89-facade-surface.md), and
[`OPC UA`](docs/opcua-c89-facade-spec.md).

The bundled `open62541::open62541` target is built with OpenSSL-backed
security policy support, the upstream default reduced namespace zero, and static
OpenSSL plus POSIX system-library requirements carried through the imported
target. Full namespace-zero builds require the upstream UA-Nodeset submodule as
an additional third-party source and are not enabled in this bundle.
The bundled MQTT-C package is also available as `MQTT-C::mqttc` and
`cpkt::mqttc_shared`. open62541's MQTT transport embeds MQTT-C source internally
as upstream expects so it can provide open62541's EventLoop-backed MQTT-C PAL.
c.pkt.systems applies an MPL-2.0 patch that prefixes the embedded MQTT-C symbols
inside open62541, so consumers may also link the standalone MQTT-C package
without duplicate `mqtt_*` symbols from open62541.

The bundled libcurl uses each target platform's system trust store by default
when the consumer has not set an explicit CA bundle or CA path. Linux targets
use OpenSSL with libcurl's CA fallback enabled; Darwin targets use curl's
OpenSSL-backed Apple SecTrust integration.

Direct package-directory lookup is also supported for packages with bundled
dependencies:

```sh
cmake -S examples/cmake-consumer -B build/cpkt-example \
  -DCURL_DIR=/path/to/c.pkt.systems-<version>-<target>/lib/cmake/CURL
```

## Consuming With pkg-config

Use the extracted SDK's pkg-config directory as an isolated search root:

```sh
PKG_CONFIG_PATH= \
PKG_CONFIG_LIBDIR=/path/to/c.pkt.systems-<version>-<target>/lib/pkgconfig \
  pkg-config --static --cflags --libs libcurl
```

The pkg-config files are relocatable and encode private static link
requirements. The aggregate `openssl.pc` also works for normal non-static
OpenSSL consumers:

```sh
PKG_CONFIG_PATH= \
PKG_CONFIG_LIBDIR=/path/to/c.pkt.systems-<version>-<target>/lib/pkgconfig \
  pkg-config --cflags --libs openssl
```

The bundled OPC UA library is available through `open62541.pc`:

```sh
PKG_CONFIG_PATH= \
PKG_CONFIG_LIBDIR=/path/to/c.pkt.systems-<version>-<target>/lib/pkgconfig \
  pkg-config --static --cflags --libs open62541
```

The standalone MQTT-C library is available through `mqtt-c.pc`:

```sh
PKG_CONFIG_PATH= \
PKG_CONFIG_LIBDIR=/path/to/c.pkt.systems-<version>-<target>/lib/pkgconfig \
  pkg-config --static --cflags --libs mqtt-c
```

The strict C89 audio and local speech facades are available through
`cpkt-audio.pc` and `cpkt-sus.pc`:

```sh
PKG_CONFIG_PATH= \
PKG_CONFIG_LIBDIR=/path/to/c.pkt.systems-<version>-<target>/lib/pkgconfig \
  pkg-config --static --cflags --libs cpkt-sus
```

The repository includes a representative pkg-config consumer:

```sh
CPKT_SDK_PREFIX=/path/to/c.pkt.systems-<version>-<target> \
  examples/pkg-config-consumer/build.sh build/cpkt-pkg-config-example
```

## Lua And C89 Consumers

`Lua::Lua` is the upstream Lua 5.5 C API and keeps upstream number handling.
Source files that include `lua.h` must compile as C99 or newer.

Strict C89 applications that need the complete Lua stack/value API should use:

```text
#include <cpkt/lua.h>
```

`cpkt_lua` covers all declared Lua, lauxlib, and lualib APIs and Lua's public
convenience macros. Its header never includes upstream Lua headers or exposes
`long long`; its two-word C89 integer value preserves the configured 64-bit
Lua integer ABI. Link it with `find_package(CpktLua CONFIG REQUIRED)` and
`cpkt::lua`, or `pkg-config --static --libs cpkt-lua`.
The generated API's naming, integer representation, callback lifetimes, and
stack behavior are described in [`docs/lua-c89-facade.md`](docs/lua-c89-facade.md).

`cpkt_lua_runtime` is intentionally a narrower embedding/runtime API, not a
replacement for the full `cpkt_lua` C API. Consumers of the runtime facade can:

- create runtimes, including runtimes with a memory cap,
- create runtimes with caller-provided allocation callbacks,
- open all standard libraries or a selected standard-library bitmask,
- enable traceback text and instruction-count limits,
- configure package search paths,
- set simple string, boolean, integer, and number globals,
- run Lua files or buffers and pass `argv` as Lua `arg`,
- require modules for side effects,
- register named C module loaders,
- register named Lua preload chunks,
- pass an opaque embedder context through to C module loaders.

The runtime facade does not expose a general stack/value API. Consumers that
need stack operations, returned Lua values, metatables, userdata manipulation,
or other full embedding details should use `cpkt_lua`; upstream `Lua::Lua`
remains available for C99-or-newer source.

Strict C89 applications that need audio decoding, URL-backed audio streams,
capture/playback, VOX/PTT segmentation, or local speech-to-text should use the
SDK facades:

```text
#include <cpkt/audio.h>
#include <cpkt/sus.h>
```

`cpkt_audio` is a C89 facade over miniaudio. It exposes receiver-style handles
for decoders, encoders, capture, playback, VOX, and PTT without exposing
miniaudio headers or backend types. Decoders produce float32 mono 16 kHz PCM for
speech workflows and support file, HTTP/HTTPS URL, and callback-reader inputs.
The first advertised encoder format is WAV. Capture and playback accept
`auto`, `process`, `coreaudio`, and `native` backend selections. On Linux,
static `auto` uses the process backend by default while explicit `native`
retains miniaudio runtime loading; shared Linux builds may try native runtime
loading first and fall back to process when the native open fails.

`cpkt_sus` is a C89 facade over a CPU-only whisper.cpp/ggml build. It ships as a
separate ABI-0 facade from `cpkt_audio`, while depending on `cpkt_audio` for
decoder and VOX integration. The public cache resolver uses curated GGML model
entries with pinned checksums. A NULL or empty cached-model name defaults to
`tiny`; callers can override the cache location with
`cpkt_sus_cache_config.cache_dir`, and the `cpktxscribe` shell exposes the same
setting as `--cache-dir DIR`.

Use `find_package(CpktAudio CONFIG REQUIRED)` and link `cpkt::audio`, or use
`pkg-config --static --libs cpkt-audio`. Use
`find_package(CpktSus CONFIG REQUIRED)` and link `cpkt::sus`, or use
`pkg-config --static --libs cpkt-sus`. The repository includes strict C89
examples under `examples/audio-*` and `examples/sus-*`, plus the `cpktxscribe`
CLI for cached-model transcription.

The upstream open62541 API is shipped as its native C99/C++98-compatible header
surface under `include/open62541/`. Strict C89 applications should use the SDK
facade instead:

```text
#include <cpkt/opcua.h>
```

The facade header does not include open62541 headers or expose `UA_Client`,
`UA_Server`, `UA_StatusCode`, `UA_NodeId`, `UA_Variant`, fixed-width C99 integer
types, `long long`, or inline functions. Its implementation is compiled as C99
inside the SDK and links the bundled open62541 library.

The OPC UA facade provides opaque client and server handles, C89-safe node-id
and scalar value wrappers, explicit server startup/iterate/shutdown control,
client connect/disconnect/iterate operations, scalar variable add/read/write
helpers for boolean, integer, double, and string values, object nodes, child
browse callbacks, scalar method registration and calls, client subscriptions,
monitored value callbacks, status-name helpers, and native callback escape hatches
for C99 translation units that need direct access to the underlying open62541
`UA_Client *` or `UA_Server *`.

Use `find_package(CpktOpcUa CONFIG REQUIRED)` and link `cpkt::opcua`, or use
`pkg-config --static --libs cpkt-opcua`.

The repository includes `examples/opcua-c89`, which builds as strict C89
against the facade. It creates server nodes, reads and writes a scalar value,
registers a scalar method callback, walks object children through browse
callbacks, and builds through both CMake and pkg-config package metadata.

The standalone MQTT-C headers are shipped under `include/` with the upstream MIT
license. They are not a C89 facade; use them from source modes compatible with
upstream MQTT-C. The pinned MQTT-C commit matches the source embedded into the
open62541 build.

Use `find_package(CpktLuaRuntime CONFIG REQUIRED)` and link
`cpkt::lua_runtime`, or use `pkg-config --static --libs cpkt-lua-runtime`.
The repository includes `examples/lua-runtime-c89`, which builds the strict
C89 host source separately from the C99 Lua module-opener source through both
CMake and pkg-config package metadata. The example also instantiates a custom
runtime allocator and asserts that the callbacks are used.

All facade-owned allocations, including the Lua state allocation hook, flow
through the runtime allocator. `cpkt_lua_runtime_new_with_limit()` uses the
default heap allocator with a byte cap. `cpkt_lua_runtime_new_with_allocator()`
lets embedders provide `alloc` and `free` callbacks, an optional `realloc`
callback, and the same byte cap. When `realloc` is omitted, the facade grows
blocks by allocating, copying the old byte count, and freeing through the same
allocator.

The facade owns instruction-limit enforcement. When the debug library is opened
explicitly or through `CPKT_LUA_RUNTIME_OPEN_LIBS`, `debug.sethook` is replaced
with a facade error function so script code cannot clear or replace the
instruction-limit hook. When the coroutine library is opened, the facade also
installs the limit hook on coroutines created through `coroutine.create` and
`coroutine.wrap`.

## Verification Coverage

Release verification checks every produced tarball from an extracted install
tree. It asserts archive layout, checksum coverage, metadata path placement,
metadata relocatability, absence of old non-upstream CMake package directories,
privacy/path hygiene, static transitive propagation through CMake and
pkg-config, direct `Libssh2_DIR` and `CURL_DIR` package use, and representative
CMake and pkg-config examples. The installed `cpkt_lua` and `cpkt_lua_runtime`
consumers compile as C89 and link through both CMake and pkg-config metadata;
the runtime example also exercises the custom allocator API from the extracted
SDK.

Native debug and hardening checks are available through these Make targets:

```sh
make debug
make clangd-surface
make valgrind
make fuzz-smoke
make fuzz
```

`make test-all` combines `debug`, `clangd-surface`, `valgrind`, and `fuzz-smoke`.
The debug suite includes the real Lua runtime tests, mock-backed Lua tests, and
C89 embedding examples; there is no separate Lua-test command.

`valgrind` is the required native x86_64 Linux Memcheck gate for
`cpkt_lua_runtime_mock_test`. It does not cover every facade under Memcheck.
AFL++ 5.02c is cached and built against the pinned Bootlin x86_64 GCC
plugin headers; `fuzz-smoke` runs bounded AFL++ jobs against the mock-backed Lua
runtime and public OPC UA facades. The OPC UA fuzzer reuses the normal debug
dependency install tree for linkage. These hardening builds live under
`build/valgrind`, `build/fuzz`, and `build/opcua-fuzz`; they are part of the
shared prerelease and release proof graph and never instrument release package
artifacts. Valgrind and AFL++ never run via a cross target, emulator, or QEMU.

`make fuzz-long` is an opt-in extended native fuzz run and requires
`CPKT_FUZZ_LONG_ENABLE=1`. External-provider checks are likewise separate from
the deterministic release gate. To run both speech e2e workflows using the
native debug preset:

```sh
CPKT_LIVE_CHECKS=1 make E2E_SUS_PRESET=debug prerelease-live
```

Dependency updates must follow the [bundle ABI policy](AGENTS.md), including
embedded libraries and downstream consumers. OpenSSL remains on version 3.
The [September 2026 audit](docs/dependency-audit-2026-09.md) records selected
versions, security context, verification results, and supported compatibility
scope. Direct whisper.cpp/ggml API/ABI compatibility for external consumers is
out of scope; supported downstream speech use goes through `cpkt_sus`.

`clang-format` and `clangd` are host development tools supplied by the
[latest stable host LLVM installation](skills/pkt-systems-cmake-lifecycle/references/toolchains.md#host-llvm-and-clang),
which also supplies Clang for osxcross. c.pkt.systems does not download, cache,
or ship LLVM/Clang. `make clangd-surface` configures
the native debug compile database, verifies that every public facade header
declaration and non-static facade implementation has adjacent Doxygen
documentation for LSP hover text, and checks that the shipped examples are
present in `compile_commands.json`. The same target also runs
`clangd --check` against the examples using that compile database. Cross-target
CTest and package configurations do not invoke host `clangd`; their compiler,
target-runner, and package verification gates remain authoritative.

### Running with the selected Bootlin runtime

Native Linux development executables select the pinned Bootlin loader and libc
directly: tests, helpers, cpktxscribe, and all in-tree examples. This applies in
both debug and release builds; these executables are not shipped in the SDK.
CTest, e2e scripts, and named example targets execute them normally, for example:

```sh
./build/debug/tools/cpktxscribe --help
```

Shipped libraries and installed example sources keep normal runtime metadata.
Temporary SDK verification consumers follow the same link-time policy and run
directly, including CMake, pkg-config, and installed-example verification builds.
No runtime launcher is needed. Host subprocesses retain their own runtime.
Collection-runtime checks do not establish older-host deployment support.
