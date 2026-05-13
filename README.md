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
static archives, and shared libraries. Partial static-only or shared-only
dependency bundles are not supported.

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
contents. Linux package verification also extracts each tarball and builds a
consumer against the packaged `include/` and `lib/` prefix.

To verify existing archives:

```sh
make verify-release-archives
```
