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
The native iODBC headers are strict C89 on supported targets. Applications
can select a trace file with `SQLSetConnectAttr(dbc, SQL_ATTR_TRACEFILE, path,
SQL_NTS)` and enable or disable file tracing through `SQL_ATTR_TRACE` with
`SQL_OPT_TRACE_ON` or `SQL_OPT_TRACE_OFF`. Read operation diagnostics with
`SQLGetDiagRec` (or the matching ODBC diagnostic APIs). The trace file is an
iODBC manager facility and is caller-configured; there is no `cpkt_iodbc`
wrapper or SDK logger dependency. The static and shared C89 consumer tests
verify file output and diagnostic records.
The bundled source patch initializes the native error output to zero for
driver-manager diagnostics. Static and shared consumers verify that output
through `SQLGetDiagRec`, `SQLGetDiagRecA`, and `SQLGetDiagRecW`, including with
file tracing enabled; the same cases run under Valgrind.

OpenLDAP's bundled liblber adds `ber_set_log_print_fn` as a typed C89 entry
point to its existing global print hook. Its previous callback is returned for
later restoration, and a null argument restores the native default. LDAP's
Cyrus SASL client status lines and Cyrus diagnostics use that same destination
when a custom sink is installed before LDAP/Cyrus initialization; otherwise
Cyrus diagnostics retain their native syslog destination. The static and shared
local-socket integration tests assert callback delivery.

Dependency upgrades follow the published bundle compatibility policy in
[AGENTS.md](../AGENTS.md). Release-specific selection and verification evidence
belongs in the release record, rather than a date-stamped audit in this source
tree. Host tools such as Clang, clangd, clang-format, Valgrind, QEMU, Podman,
and osxcross are development prerequisites, not SDK dependencies.

## Known open62541 build diagnostics

The pinned open62541/OpenSSL combination has these inspected diagnostics:

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

- OpenSSL's metadata generator reports omitted `CMAKECONFIGDIR`,
  `PKGCONFIGDIR`, and `libdir` fields when creating its build-only metadata,
  and an omitted `COMMENT` when creating install metadata. Its upstream
  Makefile passes the actual install paths to the install metadata command.
  The SDK supplies its own consumer metadata; the provenance gate checks the
  selected bundled shared libraries in either Ninja or Unix Makefiles link
  plans, with negative fixtures for incorrect paths and linkage.
- zlib's installed header uses its `Z_HAVE_*` feature names, preserving the
  detected features without redefining a consumer's private `HAVE_*` macros.
  Its legacy configure probes guard absent macros. The header regression
  compiles consumers with absent, empty, zero and one definitions under
  `-Wundef -Werror`, including libssh2's empty-definition convention.
- Cyrus SASL 2.1.28 reports function-pointer casts for its heterogeneous,
  callback-ID-tagged table. Dispatch selects the specific callback type before
  invocation. Its bundled MD5 implementation also uses K&R function definitions,
  accepted by the configured C dialect with old-style-definition warnings.
  Its config reader also increments an unused line counter; this does not
  affect parsing or error returns.
- Cyrus SASL's cross-configure is pinned to `--with-lib-subdir=lib`. Its
  autodetection otherwise inspects the build host's `/usr/lib64` and can emit
  invalid OpenSSL `lib64` paths for Darwin, even though the SDK installs in
  `lib`.
- On 32-bit musl, `libssh2_poll` previously narrowed a 64-bit `timeval`
  difference into 32-bit `long`; a large clock jump could wrap the remaining
  timeout. The bundled source patch bounds elapsed time before narrowing, and
  the QEMU regression checks large forward and backward jumps plus a short
  remaining interval.
- ARM ggml reports `-Wpointer-to-int-cast` in its failed-affinity diagnostic:
  the log passes a mask-array address as an integer. The affinity mask itself
  is applied separately through `CPU_SET`; this diagnostic does not change
  affinity behavior.
- Darwin libxml2's `xmlLittleEndian` and PostgreSQL's `libpq_gettext_impl`
  report set-but-unused globals in their upstream builds. The linker also
  warns about an upstream `-single_module` probe. These do not affect exported
  interfaces. The direct SQLite dylib link carries the configured deployment
  minimum, and package verification checks the recorded minimum for every
  bundled Darwin dylib and module.
- libtool reports ignored version metadata for convenience libraries and
  relocation/finish messages for staged SASL installs. The SDK's package and
  plugin relocation gates check installed paths and runtime discovery.

## Kerberos error formatting and inspected Darwin diagnostics

The bundled GSS trace bridge shares one private declaration between its
definition and the SPNEGO caller. Both static and shared GSSAPI builds make
`-Wmissing-prototypes` fatal. Upstream's Darwin warning profile previously
reported the bridge's missing declaration without failing the build; the
Linux override did not enable that diagnostic. A successful build therefore
did not establish that this project-owned patch was warning-clean.

The `krb5_trace_prototypes` compiler regression checks the actual patch
additions with strict warnings and rejects missing or conflicting declarations.
It uses minimal native type declarations to isolate this compile contract;
the complete GSSAPI builds exercise the real generated Kerberos headers.
Packaging runs this fixture with every configured target compiler, including
Darwin, before starting any dependency build. The native macOS workflow runs
it in its initial portable fixtures as well. The packaging failure regression
verifies that a Darwin fixture failure prevents every matrix build.

MIT Kerberos 1.22.2's `krb5_vprepend_error_message` incorrectly forwarded its
`va_list` to the variadic `krb5_wrap_error_message`. The bundled
`krb5_error_va_list.patch` forwards it to `krb5_vwrap_error_message` instead.
The regression checks exact integer, mixed string/long/double, and nested
prepend results; the unpatched library fails the integer case.

The remaining inspected Darwin Kerberos diagnostics are:

- Mutex return locals and the pre-CFX trailer local are read by assertions;
  `NDEBUG` removes those reads. Mutex operations still execute, and the
  pre-CFX path constructs an empty crypto trailer as designed.
- The private AES table header replaces the SDK's function-like `ALIGN` macro
  with an empty declaration decoration. It is confined to the AES table
  translation unit, which does not call the SDK alignment macro afterward.
- `default_com_err_proc` lacks a format-checking attribute; its implementation
  forwards the `va_list` to `vfprintf` correctly. This diagnostic affects
  compile-time checking, not formatting.
- `ld64.lld` ignores upstream's `-dylib_file` mapping for indirect dependencies.
  The build links the in-tree libraries directly. Package verification checks
  installed dependency paths, relocation and consumer links.
- The generic configure probe warns about absent `gethostbyname_r` and
  `getservbyname_r`. The built Darwin support library imports native
  `getaddrinfo`; its old address-cache implementation is disabled. The remaining
  legacy krb524 service lookup uses `getservbyname`, whose
  [Apple implementation](https://github.com/apple-oss-distributions/Libinfo/blob/main/lookup.subproj/libinfo.c#L1756)
  retains the result in
  [thread-specific storage](https://github.com/apple-oss-distributions/Libinfo/blob/main/lookup.subproj/thread_data.c#L91).
  These source paths do not substantiate the probe's thread-safety warning.
  Native macOS runtime verification remains required before release.

## Inspected configure notices

- Kerberos's obsolete `sys_errlist` probe fails on modern libc; the available
  `strerror` path supplies error descriptions instead.
- Cyrus SASL has no local SASLDB database backend in this bundle. GSSAPI/GS2
  mechanisms are built and tested. Missing Sphinx and Perl documentation modules
  affect regeneration of upstream documentation, not SDK libraries or the
  shipped facade documentation.
- OpenLDAP disables `slapd`. Its configure script warns about ignoring default
  server backend and systemd options; the bundle builds client libraries with
  OpenSSL and Cyrus SASL support.
- iODBC's unshipped `iodbc-config` helper ignores `--datarootdir`; SDK metadata
  is generated separately. Autoconf's unprefixed `mt` probe is a host manifest
  tool check; compiler, archiver, strip and linker tools use the configured
  target paths.
- Cross-compilation notices describe configure tests that cannot execute on the
  build host. Linux runtime and consumer checks use the configured target
  toolchains and emulators. PostgreSQL's upstream server OAuth tests require
  its Python-enabled server configuration; the bundle builds client libraries
  and runs its own OAuth loader, cleanup and facade regressions.
- The stale-SDK-root regression intentionally configures a minimal project
  with an unused lifecycle marker and prints CMake's unused-variable notice.
  This is fixture output, not a warning from the SDK's own configuration.

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

## Native OPC UA memory history allocation safety

The bundled `0011-check-memory-history-backend-allocations.patch` checks stock
memory and circular-backend constructor, node-store, value-store, value-copy
and continuation allocations. Failed growth retains existing storage; failed
replacement retains the previous value. Upsert propagates allocation failure
instead of retrying insertion after a failed replacement. Successful storage,
sorting and circular replacement continue through the upstream implementation.
Methods without a status return use their zero/false/NULL failure result.

The patch also reads/writes counted continuation bytes with `memcpy`, avoiding
alignment-dependent `size_t` access, rejects a circular continuation offset
beyond the available records, and releases temporary continuation buffers on
failed reads. Native regression tests inject each allocation failure, retry
writes and node registration, check previous values and borrowed addresses,
exercise unaligned continuation input, and clear both partial and complete
outputs. These tests run alongside C89 facade tests in static/shared and
allocation-failure suites; they do not classify the remaining C89 history
factory bindings as complete.

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
