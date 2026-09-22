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
state operation required to use it. A declaration that already uses C89
language and standard-library types is directly available through
`<cpkt/openssl.h>`; it does not need a redundant `cpkt_openssl_*` wrapper. A
declaration that uses native 64-bit storage, `long long`, or a record/callback
that embeds either must have an explicit typed adapter. A target may not
silently lose such an API. Typed values use the facade's explicit two-word
representation and adapter functions.

For the current x86_64 GNU OpenSSL 3.6.4 configuration, 6,468 public header
functions are present. Only the mechanically classified typed-adapter subset
(currently 74 direct signatures, plus record-dependent callers) needs new
`cpkt_openssl_*` entry points. The 6,500-ish OpenSSL dynamic-export count is
not a count of c.pkt facade exports and is not an instruction to duplicate all
of OpenSSL's already-C89 declarations.

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
exports, and records the exact typed-adapter subset. The generated facade
coverage check must fail for every typed or record-dependent public catalog
entry lacking an adapter before `cpkt_openssl` ships.
