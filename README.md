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
- `arm64-apple-darwin`, when osxcross is available

## Release Workflow

```sh
make release
```

This builds each dependency tree, runs the ABI/link smoke tests where the target
can execute locally, writes `dist/c.pkt.systems-<version>-<target>.tar.gz`,
writes `dist/c.pkt.systems-<version>-CHECKSUMS`, and verifies the archive
contents. Package verification also extracts each tarball and builds downstream
CMake and pkg-config consumers for every shipped dependency package, asserting
that static link requirements propagate through the shipped metadata. Linux
consumers are run when executable locally or through the configured emulator;
Darwin consumers are configure/link checked when osxcross is available.

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
```

Static transitive dependencies are part of the imported targets. Consumers
should not add private workaround libraries such as `-ldl`, `-pthread`,
`-latomic`, zlib, nghttp2, libssh2, OpenSSL, or Darwin frameworks by hand.

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
make asan
make tsan
make msan
make fuzz-smoke
```

`asan` also enables UBSan. These sanitizer and fuzz gates compile only
repo-owned facade code and the mock Lua sink; they do not build, instrument, or
test bundled upstream dependencies. `fuzz-smoke` builds the libFuzzer target and
runs a bounded smoke pass against that mock-backed facade target. These
instrumentation builds live under `build/asan`, `build/tsan`, `build/msan`, and
`build/fuzz`; release package targets do not enable sanitizer or fuzzing
instrumentation.

`make clangd-surface` configures the debug compile database, verifies that the
public strict Lua facade declarations have adjacent Doxygen comments for LSP
hover text, and checks that the shipped examples are present in
`compile_commands.json`. When `clangd` is installed, the same target also runs
`clangd --check` against the examples using that compile database.
