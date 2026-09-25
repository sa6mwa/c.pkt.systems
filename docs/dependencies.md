# Bundled dependencies

The SDK builds and ships pinned source releases for OpenSSL, zlib, curl,
nghttp2, libssh2, libxml2, libpng, libHaru, iODBC, Lua, miniaudio, whisper.cpp and
ggml, open62541, MQTT-C, MIT Kerberos, Cyrus SASL, OpenLDAP, PostgreSQL
libpq, and SQLite. CMocka is used for build tests. Some components are
transitive dependencies of others; each independently shipped library has
its own SDK package metadata.

The authoritative versions and verified source archive hashes are the pins
in [CMakeLists.txt](../CMakeLists.txt) and
[CpktDependencies.cmake](../cmake/CpktDependencies.cmake). The generated SDK
dependency manifest records what a particular release actually contains.
Licenses and required notices are shipped under
`share/doc/c.pkt.systems/third_party/`, with the aggregate notice source at
[THIRD_PARTY_NOTICES.md](third_party/THIRD_PARTY_NOTICES.md).

iODBC 3.52.16 supplies the ODBC driver manager and installer libraries, not a
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
