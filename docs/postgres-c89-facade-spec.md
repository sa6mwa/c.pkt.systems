# PostgreSQL C89 facade

`<cpkt/postgres.h>` is the supported PostgreSQL and CockroachDB client
interface in c.pkt.systems.  It is a C89 public header: it does not expose
libpq headers, C99 fixed-width integer types, C++ linkage syntax, or native
TLS/GSSAPI objects.

The primary connection form is an explicit-receiver shell:

```c
cpkt_postgres *pg;
cpkt_postgres_result *result;

pg = cpkt_postgres_new("host=127.0.0.1 dbname=app user=app");
if (pg == 0) {
  return 1;
}
result = pg->tx(pg, "select 1");
cpkt_postgres_result_free(result);
pg->close(pg);
```

`cpkt_postgres_new()` owns one connection.  `pg->close(pg)` closes that
connection and releases the receiver.  The direct `cpkt_postgres_*` calls are
also available for every supported connection, result, COPY, pipeline,
prepared statement, escaping, notification, large-object, and fast-path
operation.

`cpkt_postgres_i64` represents an exact signed 64-bit value as two 32-bit
words (`high`, `low`) without requiring `long long`.  Values are two's
complement bits; the facade performs the conversion for large-object offsets
and PostgreSQL microsecond timestamps.  PostgreSQL OIDs use
`cpkt_postgres_oid`; values outside PostgreSQL's 32-bit protocol range are
rejected before they reach libpq.

The bundled client includes TLS, GSSAPI/Kerberos, LDAP, Cyrus SASL, curl, and
the PostgreSQL OAuth client module.  It ships library artifacts only: no
server, command-line client, daemon, authentication helper, or generated
configuration tree is part of the runtime bundle.

For static consumers, use `find_package(CpktPostgres CONFIG REQUIRED)` and
link `cpkt::postgres`, or use `pkg-config --static --libs cpkt-postgres`.
Those interfaces carry libpq's complete static closure, including its OAuth,
frontend-common, frontend-port, LDAP, SASL, GSSAPI, TLS, resolver, and math
dependencies. Shared consumers link `cpkt::postgres_shared`. The OAuth client
module is a libpq-internal loadable component with no stable shared-library
identity, so c.pkt.systems retains it only in the static closure rather than
shipping it as a runtime library.

## Live protocol compatibility gate

`make e2e-postgres` exercises the receiver shell against both PostgreSQL and
CockroachDB. It requires `CPKT_POSTGRES_E2E_CONNINFO` and
`CPKT_COCKROACH_E2E_CONNINFO`, both standard libpq connection strings. The
gate covers receiver queries, parameters, asynchronous send/receive, prepared
statements, transaction state, and an exact 64-bit `INT8` result. Connection
strings are never printed by the harness.
