# Dependency roadmap and implementation queue

This is the ordered TODO for dependency and facade work in c.pkt.systems. It
is not a pin or compatibility commitment: each item still needs its exact
upstream audit, ABI review, and release-matrix evidence before shipment.

## Current implementation queue

- [ ] **Repair the Kerberos SDK header closure.** Stage and ship the generated
  `com_err.h` required by the shipped `krb5.h`; add extracted-SDK coverage that
  compiles a strict-C89 Kerberos consumer. Build only the `krb5` component and
  its required closure to prove the component build boundary.
- [ ] **Add `cpkt_openssl`.** Provide the complete public OpenSSL C89 facade
  over the bundled crypto and TLS libraries, with separate CMake and
  pkg-config packages.
- [ ] **Add `cpkt_nghttp2`.** Provide the complete public nghttp2 C89 facade,
  including callback, session, frame, and error interfaces.
- [ ] **Add `cpkt_libssh2`.** Provide the complete public libssh2 C89 facade,
  including session, authentication, channel, SFTP, SCP, and public-key APIs.
- [ ] **Add `cpkt_mqttc`.** Provide the complete public MQTT-C C89 facade,
  including client lifecycle, packet handling, callbacks, and transport
  integration.
- [ ] **Sweep `cpkt_postgres` for complete libpq coverage.** Assert every
  supported libpq public operation is represented by the C89 receiver-shell
  facade and verify that Postgres-only facade work builds only its dependency
  closure.
- [ ] **Sweep `cpkt_sqlite` for complete SQLite coverage.** Assert every
  enabled public SQLite API is represented by the C89 facade and verify that
  SQLite-only facade work builds only SQLite and its platform closure.
- [ ] **Add `cpkt_lua` last.** `cpkt_lua_runtime` is already the priority
  embedding-policy facade. This later item is a complete C89 facade for the
  full public Lua C API, not a replacement for `cpkt_lua_runtime`.

The direct C89 downstream interfaces for curl, zlib, miniaudio, OpenLDAP,
Cyrus SASL, and the core MIT Kerberos/GSSAPI APIs remain available without new
facades. SQLite has no bundled third-party library dependency; its runtime
closure is limited to platform facilities.

## Distribution policy

c.pkt.systems is MIT licensed. We will bundle only dependencies under a
permissive license or public-domain dedication that permits distribution with
the SDK. Required copyright, license, and notice text will be installed in
each binary SDK and recorded in its dependency manifest.

LGPL dependencies are out of scope. We will not bundle them dynamically, ship
their static archives, or rely on relinking exceptions. GPL-only executables
are also out of scope.

Every bundled dependency must be built and package-verified for every shipped
target:

| Operating system | Architectures / libc |
| --- | --- |
| Linux | `x86_64`, `aarch64`, and `armhf`; each with glibc and musl |
| macOS | `arm64` |

## Public API rule: C89 facades

An upstream API that is not fully usable by a strict-C89 downstream consumer
must have a small, independently versioned C89 facade, built as both a shared
and static library and accompanied by CMake and pkg-config metadata.
Upstream interfaces that are already strict-C89 compatible may remain direct
SDK interfaces.

The facade must:

- expose C89-compatible headers and types only; it must not require C99/C++
  language features or leak upstream structs, headers, exceptions, allocators,
  or C++ standard-library types;
- use opaque handles, explicit ownership, fixed-width compatibility types
  supplied by the facade where needed, and explicit conversions for upstream
  features such as `long long`;
- provide actionable errors, bounded and documented resource behavior, and
  actual streaming where a dependency exposes a streaming operation;
- retain a stable c.pkt.systems ABI while allowing internal upstream upgrades;
  and
- have observable unit/integration coverage, shared and static downstream
  consumer tests, and target-appropriate end-to-end tests.

The implementation work includes testing the facade rather than exposing a
thin renamed upstream API. c.pkt.systems facade implementation sources are
C89-only; a dependency with a C++ implementation, such as `tdslite`, requires
a separately justified C89 implementation boundary and does not introduce C++
sources or a C++ compiler requirement into this project.

## Planned database and messaging clients

The order below reflects current priority. PostgreSQL support is first.

| Priority | Component | Capability and boundary | License conclusion | Status / implementation gate |
| ---: | --- | --- | --- | --- |
| 1 | [libpq](https://www.postgresql.org/docs/current/libpq.html) | PostgreSQL C client; validate against PostgreSQL and CockroachDB in the facade e2e suite. | PostgreSQL License, a permissive BSD/MIT-like license. | First implementation slice. Build with the selected TLS/authentication configuration and prove shared/static facade consumers. |
| 2 | [SQLite](https://www.sqlite.org/about.html) | Embedded SQL database. | Public domain for the delivered SQLite library. | Implementation active. Pin the official amalgamation, enable the audited public feature set, and expose its complete public capability through the C89 facade. SQLite 3 database files remain readable and writable by later SQLite 3 releases; the test suite must cover current file-format and WAL behavior. |
| 3 | [librdkafka](https://github.com/confluentinc/librdkafka) | Kafka producer, consumer, and administration client. | BSD-2-Clause. | Third implementation slice. Start with a minimal feature set; explicitly select TLS, SASL, and compression dependencies and audit their licenses and static link closure. |
| 4 | [rabbitmq-c](https://github.com/alanxz/rabbitmq-c) | RabbitMQ/AMQP 0-9-1 C client. | MIT. | Fourth implementation slice. Use the bundled OpenSSL only through the facade and test broker e2e behavior. |
| 5 | [tdslite](https://github.com/tdslite/tdslite) | Direct Microsoft SQL Server TDS client. It is not a Sybase client commitment. | MIT. | Investigation candidate, not a committed dependency. It is header-only C++11 with a pluggable network layer; prove TLS, SQL Server authentication modes, server-feature coverage, and all-target transport integration before adoption. It must sit behind a C89 facade. |

CockroachDB uses the PostgreSQL wire protocol, so it belongs in the `libpq`
verification scope rather than requiring a separate client library. The
CockroachDB compatibility claim must be tested against the supported server
versions and authentication modes selected for the facade.

## Planned processing and interchange libraries

| Order | Component | Capability and boundary | License conclusion | Status / implementation gate |
| ---: | --- | --- | --- | --- |
| 1 | [zstd](https://github.com/facebook/zstd) | Zstandard compression/decompression. | BSD-3-Clause option from its `BSD-3-Clause OR GPL-2.0-only` license. | Add as the compression primitive after the initial database client slices, with only the permissively licensed library sources and required notices. |
| 2 | [libarchive](https://www.libarchive.org/) | Streaming archive read/write, including tar. | New BSD license. | Preferred archive dependency. Select and audit an intentionally narrow filter/format set; do not silently acquire unreviewed optional codec dependencies. |
| 3 | [libyaml](https://github.com/yaml/libyaml) | YAML parse and emit. | MIT. | Add with event/document API behavior isolated behind the C89 facade. |
| 4 | [protobuf-c](https://github.com/protobuf-c/protobuf-c) | Protocol Buffers encoding/decoding and C code generator. | BSD-2-Clause. | Add only with clear generator/runtime version coupling and generated-code compatibility tests. |

## Deliberately not planned

| Candidate | Decision | Reason |
| --- | --- | --- |
| FreeTDS | Excluded | LGPL. This also means no bundled SQL Server/Sybase client through FreeTDS. |
| MariaDB Connector/C | Excluded | LGPL-2.1-or-later. This removes the proposed MariaDB/MySQL client route under the current distribution policy. |
| Microsoft ODBC Driver for SQL Server (`msodbcsql`) | Excluded | Proprietary EULA and prebuilt driver; it cannot be reproducibly built as part of this SDK. |
| GNU gzip | Excluded | GPL-only executable. A separate gzip library is unnecessary: the bundled zlib can create and read gzip streams. |
| libtar | Excluded | Archived upstream with inconsistent license metadata in downstream distributions. `libarchive` is the maintained, broader, permissively licensed tar solution. |

## Existing compression support

[zlib](https://zlib.net/) is already bundled under the zlib license. Its gzip
API and `deflateInit2()`/`inflateInit2()` modes support gzip streams, so the
roadmap intentionally contains no separate gzip dependency.

## Release gates for each adopted component

Before a component moves from this roadmap into a shipped bundle:

1. Pin the stable release archive, source URL, and SHA-256 for every target.
2. Re-audit the selected source, license expression, notices, optional
   dependencies, ABI identity, public API, static link closure, and runtime
   requirements.
3. Build the dependency and its C89 facade as shared and static artifacts for
   all seven shipped target variants.
4. Run facade unit tests, representative live service integration tests, and
   shared/static extracted-SDK consumer tests for each runnable target.
5. Validate CMake and pkg-config metadata, dependency manifests, required
   license texts/notices, archive relocatability, and runtime loader metadata.
6. Compare the supported facade ABI with the latest released bundle; any
   public ABI break follows the shared-library ABI-version policy.

## Source evidence

- [PostgreSQL License](https://www.postgresql.org/about/licence/)
- [CockroachDB PostgreSQL compatibility](https://www.cockroachlabs.com/docs/stable/developer-basics.html)
- [SQLite public-domain statement](https://www.sqlite.org/copyright.html)
- [librdkafka license and features](https://github.com/confluentinc/librdkafka)
- [rabbitmq-c license](https://github.com/alanxz/rabbitmq-c/blob/master/LICENSE)
- [tdslite license and design](https://github.com/tdslite/tdslite)
- [zstd license](https://github.com/facebook/zstd/blob/dev/LICENSE)
- [libarchive licensing](https://www.libarchive.org/)
- [LibYAML license](https://github.com/yaml/libyaml/blob/master/License)
- [protobuf-c license](https://github.com/protobuf-c/protobuf-c/blob/master/LICENSE)
- [FreeTDS licensing](https://www.freetds.org/)
- [MariaDB Connector/C licensing](https://mariadb.com/docs/connectors/mariadb-connector-c/mariadb-connector-c-guide)
- [Microsoft ODBC driver FAQ](https://learn.microsoft.com/en-us/sql/connect/odbc/linux-mac/frequently-asked-questions-faq-for-odbc-linux)
- [zlib gzip support](https://www.zlib.net/zlib_faq.html)
