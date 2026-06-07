# c.pkt.systems

Pinned C dependency bundle for pkt.systems C projects.

The project builds release artifacts for:

- OpenSSL
- zlib
- nghttp2
- libssh2
- curl
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
lib/pkgconfig/libcrypto.pc
lib/pkgconfig/libssl.pc
lib/pkgconfig/openssl.pc
lib/pkgconfig/zlib.pc
lib/pkgconfig/libnghttp2.pc
lib/pkgconfig/libssh2.pc
lib/pkgconfig/libcurl.pc
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

## Verification Coverage

Release verification checks every produced tarball from an extracted install
tree. It asserts archive layout, checksum coverage, metadata path placement,
metadata relocatability, absence of old non-upstream CMake package directories,
privacy/path hygiene, static transitive propagation through CMake and
pkg-config, direct `Libssh2_DIR` and `CURL_DIR` package use, and representative
CMake and pkg-config examples.
