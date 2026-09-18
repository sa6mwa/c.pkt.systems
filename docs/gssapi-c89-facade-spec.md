# GSSAPI C89 facade

`cpkt/gssapi.h` is the supported C89 interface to bundled MIT Kerberos
GSSAPI. It intentionally keeps provider headers, fixed-width integer typedefs,
and C++ linkage declarations outside the consumer boundary.

The facade supports mechanism discovery and OID conversion, name and
credential ownership, initiator and acceptor context setup, MIC and wrap
operations, and status rendering. Statuses, flags, QOP values, and lifetimes
are exact unsigned 32-bit values carried in `unsigned long`; the private
implementation verifies that contract at compile time.

All handles are opaque. Results returned through a `cpkt_gss_buffer` are
provider-owned and must be passed to `cpkt_gss_release_buffer`. OIDs and OID
sets returned by creation or conversion operations must use their corresponding
release operation. Standard name-type OIDs and OID-set members are borrowed.

PostgreSQL uses the bundled provider internally for GSS authentication. Its
separate `cpkt/postgres.h` boundary does not expose provider-specific GSS
objects.
