# PostgreSQL dependency C89 SDK contract

MIT Kerberos, Cyrus SASL, OpenLDAP, and LBER are separately bundled public
dependency roots. Each ships its upstream headers, static and shared libraries,
and independent CMake and pkg-config metadata. As specified in the
[dependency roadmap](roadmap.md#public-api-rule-c89-facades), public upstream
interfaces that already compile as strict C89 remain direct SDK interfaces;
they do not require a separately renamed facade.

The separately shipped `cpkt_gssapi` and `cpkt_sasl` interfaces cover the
operations described in their [GSSAPI](gssapi-c89-facade-spec.md) and
[SASL](sasl-c89-facade-spec.md) documentation. They do not replace the complete
upstream Kerberos, SASL, LDAP, or BER packages. Their facade headers must not
include or expose upstream headers, opaque native pointers, native callback
records, platform socket types, or non-C89 scalar
types. Each native callback record is represented by a C89-owned record and
trampolines that preserve callback context, ownership, and destruction.

The direct packages retain the public headers from the pinned upstream
sources. Their feature availability follows the bundled build configuration.
The PostgreSQL facade keeps these dependency types outside its own public
boundary, regardless of whether an application also uses a dependency directly.

Each shipped facade must have C89 compile tests, behavioral ownership/callback
tests, shared and static downstream CMake consumers, pkg-config consumers, package
archive checks, and target-matrix verification. Direct upstream packages require
their own C89 header and static/shared consumer checks, including LDAP and BER
linkage. No PostgreSQL facade test may be used as a substitute for these direct
public-package tests.
