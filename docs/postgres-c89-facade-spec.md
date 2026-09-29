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

Event registrations use `cpkt_postgres_event_register()`. Each callback
receives `info->event`, the borrowed identity of its current registration,
including during REGISTER before `register()` returns. That identity can be
used immediately with the connection and result instance-data accessors.
Registration callbacks may register another callback. A failed REGISTER is
removed; a failed RESULT_CREATE remains eligible on a later explicit fire,
and only initialized registrations receive COPY and DESTROY. A result retains
its frozen registration list after its connection closes, and detached results
may be cleared independently. A callback identity remains borrowed until the
connection and all results that refer to it have been destroyed.
Server notice results carry native libpq events even though libpq does not
fire RESULT_CREATE for the borrowed notice. A receiver may copy the notice
with `CPKT_POSTGRES_COPY_RESULT_EVENTS`; the copy keeps its frozen event owner
after the callback and connection close. Fire RESULT_CREATE explicitly if the
application needs create callbacks on that copy, then free the copy normally.
During RESULT_COPY dispatch, the destination is a borrowed result identity.
An application callback may copy it again, including with notice hooks only;
the nested copy keeps the source's notice callback and context snapshot after
the connection, source, and outer copy have been released. Libpq initializes
the outer copy's event only after that callback returns, so a nested copy made
inside the callback does not receive RESULT_COPY for that still-uninitialized
native event. A later explicit RESULT_CREATE may initialize its copied event.
During RESULT_DESTROY, `info->result` remains borrowed through the callback.
A notice-hooks copy made there also retains its source callback and context
after the original result and connection are freed. The borrow ends when the
destroy callback returns; later uses of `info->result` are invalid.
The borrowed notice's event state ends when its receiver returns because
libpq does not send RESULT_DESTROY for an uninitialized notice. If an
application explicitly fires RESULT_CREATE on that borrowed result, libpq
initializes its event and sends RESULT_DESTROY when it clears the notice.
If event bookkeeping cannot be allocated, the facade skips that notice
receiver invocation so it cannot return an unsafe event-bearing copy.

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
module is a libpq-internal loadable component tied to PostgreSQL's major
version. The SDK ships it beside shared libpq so the built-in OAuth flow can
load it at runtime; it has no public shared-library ABI or link target.

## Live protocol compatibility gate

`make e2e-postgres` exercises the receiver shell against both PostgreSQL and
CockroachDB. The target starts both services from the rootless Podman Kube
manifest generated from `devenv.yaml.in`, provides their libpq connection
strings to the test runner, and stops the services when the run finishes. To run
`scripts/e2e-postgres.sh` directly against existing servers, set
`CPKT_POSTGRES_E2E_CONNINFO` and `CPKT_COCKROACH_E2E_CONNINFO` to standard libpq
connection strings. The gate covers receiver queries, parameters, asynchronous
send/receive, prepared statements, transaction state, and an exact 64-bit
`INT8` result. The integration executable reads connection strings directly
from those environment variables; the harness never prints them or passes them
as command-line arguments. Shared wire assertions cover result metadata,
NULL and binary values, errors and recovery, event instance data, single-row
and chunked delivery, nonblocking transport, escaping, prepared statements,
transactions, and 64-bit text results on both servers. PostgreSQL-specific
assertions cover
pipeline error recovery, fragmented COPY in/out, notice and notification
callbacks, cancellation/reset, and sparse large-object offsets above 4 GiB.
Those assertions run only against PostgreSQL because CockroachDB does not
promise those server-side features.
