# PostgreSQL dependency C89 facade contract

MIT Kerberos, Cyrus SASL, OpenLDAP, and LBER are separately bundled public
dependency roots. Each must therefore ship both its upstream raw package and
an independently versioned C89 facade package, in static and shared forms.

The facade headers must not include or expose upstream headers, opaque native
pointers, native callback records, platform socket types, or non-C89 scalar
types. Each native callback record is represented by a C89-owned record and
trampolines that preserve callback context, ownership, and destruction.

The required scope is every client/public entry point and public callback,
record, constant, and lifetime rule in the pinned installed headers: Kerberos
and GSSAPI, SASL client/server/property/plugin interfaces, LDAP and BER
client/controls/URL/DN interfaces. Provider-only APIs may be unavailable at
runtime when their upstream feature is not built, but remain represented in
the facade and return the native feature-unavailable result.

Each facade must have C89 compile tests, behavioral ownership/callback tests,
shared and static downstream CMake consumers, pkg-config consumers, package
archive checks, and target-matrix verification. No PostgreSQL facade test may
be used as a substitute for these direct public-package tests.
