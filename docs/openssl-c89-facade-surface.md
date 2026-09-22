# OpenSSL C89 facade surface contract

OpenSSL 3.6.4 is not a direct C89 dependency. Its installed headers expose
64-bit scalar parameters, results, and legacy state records; strict C89 has no
portable scalar representation for those values. `cpkt_openssl` must therefore
adapt the complete configured OpenSSL public API, not a convenient TLS subset.

`<cpkt/openssl.h>` is the exhaustive C89-compatible import surface for every
staged top-level OpenSSL public header. It contains OpenSSL's unavoidable
legacy declaration diagnostic locally; it does not relax diagnostics in
consumer source. The typed adapters for native 64-bit values are a separate,
required layer of the facade.

The authoritative function inventory is target-specific and is derived from
OpenSSL's pinned `util/libcrypto.num` and `util/libssl.num` files. An entry
marked `EXIST::FUNCTION` belongs to the nominal public ABI. The target's
defined dynamic functions identify which entries are present after optional
algorithms and transports have been configured. The facade surface is the
subset of those functions declared by the configured installed public headers.

The public C89 facade must cover every such declared function, including any
deprecated API the configured headers still expose, and every corresponding
public type, constant, macro, callback, object lifetime operation, and legacy
state operation required to use it. A target may not silently lose a function
because its signature uses a 64-bit native scalar or a non-C89 declaration.
Such values use the facade's explicit two-word representation and adapter
functions.

The OpenSSL export manifests are not themselves an export allowlist for a
`cpkt_openssl` shared object. The facade needs its own source-controlled,
target-specific defined-export allowlist and separate import check. The only
currently accepted dynamic functions outside OpenSSL's public manifest are
the legacy compatibility or fork hooks `ERR_load_CRYPTO_strings`,
`OCSP_crlID_new`, `OPENSSL_fork_prepare`, `OPENSSL_fork_parent`, and
`OPENSSL_fork_child`; none is declared by the installed public headers and
none belongs to the facade surface. The source-controlled generator also
enumerates the OpenSSL ABI-only DSO, directory, ASN.1, SSL configuration, and
obsolete PEM CMS compatibility exports that the installed public headers omit.

`openssl_api_catalog` is a required target-correct gate. It reports the exact
exported ABI inventory for the selected build, rejects unclassified dynamic
exports, and must be extended so a generated facade coverage check fails for
every declared public catalog entry lacking an adapter before `cpkt_openssl`
ships.
