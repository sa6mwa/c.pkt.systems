# Bundled dependencies

The SDK builds and ships pinned source releases for OpenSSL, zlib, curl,
nghttp2, libssh2, libxml2, libpng, libHaru, iODBC, Lua, miniaudio, whisper.cpp and
ggml, open62541, MQTT-C, MIT Kerberos, Cyrus SASL, OpenLDAP, PostgreSQL
libpq, and SQLite. CMocka is used for build tests. OPC UA logging tests use
MIT-licensed [libpslog 0.11.0](https://github.com/sa6mwa/libpslog/releases/tag/v0.11.0)'s
target-specific static release binaries, verified against the
SHA-256 pins in [CpktTestPslog.cmake](../cmake/CpktTestPslog.cmake). They are
acquired only with `CPKT_BUILD_TESTS=ON`, remain in the test cache/build tree,
and are not installed, linked into SDK libraries, or listed in SDK dependency
metadata. No host libpslog installation is required. Some components are
transitive dependencies of others; each independently shipped library has
its own SDK package metadata.

The authoritative versions and verified source archive hashes are the pins
in [CMakeLists.txt](../CMakeLists.txt) and
[CpktDependencies.cmake](../cmake/CpktDependencies.cmake). The generated SDK
dependency manifest records what a particular release actually contains.
Licenses and required notices are shipped under
`share/doc/c.pkt.systems/third_party/`, with the aggregate notice source at
[THIRD_PARTY_NOTICES.md](third_party/THIRD_PARTY_NOTICES.md).

iODBC 3.52.16 supplies the ODBC driver manager (`libiodbc`) and configuration
API (`libiodbcinst`), not a
database-specific ODBC driver. It is built from source under the upstream
three-clause BSD license. The full public ODBC headers and both static and
shared libraries are shipped; the 32-bit `SQLBIGINT` declaration is patched to
retain its 64-bit ABI while compiling under the supported strict-C89 toolchains.
Applications must supply an ODBC driver to connect to a database. The SDK does
not create or modify host ODBC configuration files.

Dependency upgrades follow the published bundle compatibility policy in
[AGENTS.md](../AGENTS.md). Release-specific selection and verification evidence
belongs in the release record, rather than a date-stamped audit in this source
tree. Host tools such as Clang, clangd, clang-format, Valgrind, QEMU, Podman,
and osxcross are development prerequisites, not SDK dependencies.

## Known open62541 build diagnostics

The pinned open62541/OpenSSL combination has these inspected diagnostics:

- GNU compilers report `-Wcast-qual` in OpenSSL 3.6.4's `EVP_EC_gen`
  macro. `EVP_PKEY_Q_keygen` receives the curve name through a `char *`
  parameter; the EC provider's `COPY_UTF8_PARAM` duplicates that input without
  modifying it. The cast is in the upstream convenience macro.
- Darwin arm64 reports `No native IEEE 754 format detected`. Upstream recognizes
  the IEEE representation, but its float-endianness detection lacks the Darwin
  compiler macros. It selects the upstream generic float encoder, with the
  corresponding performance cost. Native encoding behavior remains subject to
  macOS runtime verification.
- Darwin reports `-Wmissing-prototypes` for
  `UA_ConnectionManager_new_POSIX_Ethernet`. The existing bundle patch preserves
  this constructor as a NULL-returning stub on unsupported platforms; upstream
  declares its public prototype only on Linux. Ethernet transport remains
  unavailable on Darwin.
- The Darwin linker reports that `-s` is obsolete. Upstream adds that option to
  release links; `ld64.lld` ignores it.

These diagnostics remain visible. They do not change the logging callback
contract; native Darwin runtime checks run in the macOS workflow.

## Other inspected dependency diagnostics

- libssh2's compression build can report a `HAVE_UNISTD_H` redefinition:
  libssh2 defines it without a value, and zlib defines it as `1`. libssh2 checks
  whether it is defined; zlib uses its numeric value. Both configurations
  recognize the available header.
- Cyrus SASL 2.1.28 reports function-pointer casts for its heterogeneous,
  callback-ID-tagged table. Dispatch selects the specific callback type before
  invocation. Its bundled MD5 implementation also uses K&R function definitions,
  accepted by the configured C dialect with old-style-definition warnings.
- libtool reports ignored version metadata for convenience libraries and
  relocation/finish messages for staged SASL installs. The SDK's package and
  plugin relocation gates check installed paths and runtime discovery.
