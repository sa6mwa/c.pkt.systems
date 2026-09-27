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

## Default OPC UA history allocation safety

The bundled open62541 patch `0006-check-default-history-allocations.patch` checks
default history gathering/database constructor allocations, guards gathering
capacity multiplication, preserves the original store on realloc failure, and
propagates NodeId-copy failure. It also releases parsed numeric ranges after
both backend read paths, including failed callbacks; the native range tests run
under Valgrind to catch regressions. The C89 default-history binding maps empty
constructor records to BADOUTOFMEMORY without replacing the installed plugin.
Its registration is closed after polling first starts so native monitored-item
contexts are never moved by later gathering growth.

## OPC UA reentrant async result callbacks

`0007-safe-reentrant-async-result-callbacks.patch` prevents the native async
manager from retaining a next-operation pointer across local result callbacks.
A callback can synchronously cancel another pending or ready operation, freeing
that operation before an outer traversal resumes. Cancellation restarts from
live queue links; normal ready processing takes the current queue head each
time. This preserves native result dispatch and introduces no response queue,
public type layout, symbol, or ABI change. C89 local-operation regression tests
exercise nested cancellation under Valgrind.

The open62541 patch `0008-preserve-date-parser-overflow-guard-direction.patch`
retains the original sign while applying and reverting the DateTime parser's
one-second overflow protection. Without it, the Unix epoch and the immediately
following second parse two seconds late/early. Native-peer and C89 facade tests
cover both boundaries, a negative Unix second, calendar fractions, and dates
requiring more than 32 bits of Unix seconds. It changes no public API or ABI.

## OPC UA method argument ownership

`0009-release-method-argument-ownership.patch` releases owned argument NodeIds
created by the extended method factory while retaining borrowed IDs from browse
results. It propagates requested output-ID copy failures and clears partial
outputs through the existing native error path. Native method invocation now
also releases its borrowed OutputArguments node if output-array allocation
fails. The creation/constructor allocation sweep and string argument-ID tests
run under Valgrind and catch both leaks. Native creation/call algorithms and
public API/ABI identities remain unchanged.

## OPC UA runtime warning in local package checks

The strict C89 package consumer constructs a server from a minimal JSON
application description and exercises local APIs without starting a listener.
Its native default access control includes an X509 token policy while the
fixture has no encrypting security policy, so open62541 reports that mismatch
through its logger. The warning originates in `ua_accesscontrol_default.c`
when the token policy uses SecurityPolicy None. This is an intentionally
unconfigured local fixture, not missing encryption support:
`UA_ENABLE_ENCRYPTION_OPENSSL` is enabled in the shipped native configuration.
Applications choosing certificate authentication must configure its encrypting
security policy; the facade preserves the native diagnostic.

### OPC UA external-source notification slots

`0010-use-external-source-notification-slots.patch` corrects native external
value reads and writes to dispatch the notifications installed in the external
source union member. Upstream was accessing the internal source's differently
located slots, suppressing external hooks. Internal value behavior is unchanged;
this patch neither changes the public ABI nor adds a callback mechanism.
It also preserves the borrowed external pointer slot during native node copies;
the former copy overwrote that slot with an internal DataValue, corrupting the
external union representation and leaking the copied payload. Native nodestore
copy tests verify pointer identity and cleanup alongside the read/write tests.
C89 tests exercise both native hooks, ranged reads/writes, pointer selection
inside read notifications, and registration replacement during a callback.
