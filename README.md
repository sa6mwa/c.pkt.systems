# c.pkt.systems

Pinned C dependency bundle for pkt.systems C projects.

The project builds release artifacts for:

- OpenSSL
- zlib
- nghttp2
- libssh2
- curl
- libxml2
- Lua
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

## Release Workflow

```sh
make prerelease
make release
```

`make prerelease` runs the complete release proof graph without first deleting
generated state: formatting, deterministic debug and clangd checks, native
Valgrind and AFL++ smoke checks, then the release matrix. It therefore produces
and verifies the same package set as the final gate. `make release` removes
generated state first and then invokes that exact same proof graph; it is the
final local release action.

The release matrix builds each dependency tree, runs the ABI/link smoke tests
where the target can execute locally, writes `dist/c.pkt.systems-<version>-<target>.tar.gz`,
writes `dist/c.pkt.systems-<version>.tar.gz` for source builds, writes
`dist/c.pkt.systems-<version>-CHECKSUMS`, and verifies the archive contents.
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
Lua::Lua
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

Strict C89 applications should use the SDK facade instead:

```text
#include <cpkt/lua_runtime.h>
```

The facade header does not include Lua headers or expose `lua_State`,
`lua_Integer`, `lua_Number`, Lua constants, `long long`, or inline functions.
Its implementation is compiled as C99 inside the SDK and links the bundled Lua
runtime.

The facade is intentionally an embedding/runtime API, not a second Lua C API.
Consumers can:

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

The facade does not expose a general stack/value API. Consumers that need stack
operations, returned Lua values, metatables, userdata manipulation, or other
full embedding details should use `Lua::Lua` directly from C99-or-newer source.

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
CMake and pkg-config examples. The installed strict Lua facade consumer is
compiled as C89, links through both CMake and pkg-config metadata, runs the
example program, and exercises the custom allocator API from the extracted SDK.

Facade hardening is available through debug-only, facade-only presets and Make
targets:

```sh
make debug
make clangd-surface
make valgrind
make fuzz-smoke
make fuzz
```

`valgrind` is the required native x86_64 Linux Memcheck gate for the repo-owned facade test
surface. AFL++ 5.02c is cached and built against the pinned Bootlin x86_64 GCC
plugin headers; `fuzz-smoke` runs bounded AFL++ jobs against the mock-backed Lua
runtime and public OPC UA facades. The OPC UA fuzzer reuses the normal debug
dependency install tree for linkage. These hardening builds live under
`build/valgrind`, `build/fuzz`, and `build/opcua-fuzz`; they are part of the
shared prerelease and release proof graph and never instrument release package
artifacts. Valgrind and AFL++ never run via a cross target, emulator, or QEMU.

`make fuzz-long` is an opt-in extended native fuzz run and requires
`CPKT_FUZZ_LONG_ENABLE=1`. External-provider checks are likewise separate from
the deterministic release gate: `CPKT_LIVE_CHECKS=1 make prerelease-live`.

`clang-format` and `clangd` are required host development tools and must be
installed with the host OS package manager. `make clangd-surface` configures
the native debug compile database, verifies that every public facade header
declaration and non-static facade implementation has adjacent Doxygen
documentation for LSP hover text, and checks that the shipped examples are
present in `compile_commands.json`. The same target also runs
`clangd --check` against the examples using that compile database. Cross-target
CTest and package configurations do not invoke host `clangd`; their compiler,
target-runner, and package verification gates remain authoritative.
