# SQLite C89 facade

`<cpkt/sqlite.h>` is the supported embedded SQLite interface in
c.pkt.systems.  It is a C89 public header and deliberately does not expose
`sqlite3.h`, SQLite handles, or non-C89 integer types.

The primary form is the receiver shell:

```c
cpkt_sqlite *db;

db = cpkt_sqlite_new("app.db");
if (db == 0) return 1;
if (db->tx(db, "create table if not exists item (id integer primary key)", 0, 0)
    != CPKT_SQLITE_OK) {
  db->close(db);
  return 1;
}
db->close(db);
```

`cpkt_sqlite_open()` preserves SQLite's `open_v2` flags and optional VFS name.
The caller owns the returned receiver, including a receiver returned for a
failed SQLite open where SQLite supplied a database handle: inspect `error`
and `error_code`, then call `close`.

For process-wide automatic extensions, create an
`cpkt_sqlite_auto_extension` receiver and register it before opening a
connection. The initialization callback receives the eventual connection
receiver rather than a transient native handle, so registrations made from the
callback have the same lifetime as the returned connection. Cancel returns the
number of registrations removed, matching the underlying API.

`tx` executes one or more SQL statements and invokes its optional row callback
without materializing result sets. `prepare` creates an explicit statement
receiver. Text and blob bind methods copy their input before returning; column
text and blob pointers are SQLite-owned and remain valid only until the next
step/reset/finalize operation on that statement.

The legacy profiler may run alongside a v2 trace callback. Installing or
clearing the legacy profiler preserves the v2 trace registration and returns
the previous legacy profiler context. Registering a new trace callback cancels
the previous trace and legacy profiler, matching SQLite's trace behavior.

On serialized connections, callback setters that keep a callback/context pair
in facade state hold the connection mutex while updating the pair and the
native registration. The facade uses a private mutex for its own bookkeeping;
SQLite's static APPLICATION mutexes remain available to callers.

Unlock-notify batches contain contexts registered with the same public
callback. Each registration receives a callback invocation with that callback's
contexts, and the facade retains a replaced binding until SQLite finishes any
concurrent delivery.

Extended changeset apply operations collect rebase output only when
`rebase_out` is provided. Passing `NULL` avoids the input-proportional native
rebase allocation, including for streaming apply operations.

`db->close(db)` is the convenience destructor: it defers native teardown until
facade-owned statements, blobs, and backups have been finalized or closed, so
callback state never outlives its facade owner. Use `cpkt_sqlite_close_strict()`
when the caller needs strict close behavior: it returns `CPKT_SQLITE_BUSY` and
leaves the receiver usable while any such child remains open.

`cpkt_sqlite_i64` holds exact signed 64-bit two's-complement bits in two
32-bit words, avoiding a public `long long` requirement. Use
`cpkt_sqlite_i64_make(high, low)` for binding and compare `high` and `low`
after `column_i64` or `last_insert_rowid`.

SQLite's `%ll` formatting conversions require a native 64-bit scalar in a
variadic call. In C89, use the typed `cpkt_sqlite_format_i64()` and
`cpkt_sqlite_format_u64()` functions for one signed or unsigned conversion;
the corresponding `format_into`, `log`, and `string_append_format` typed
functions cover the same case. Their format string may include literal text
and `%%`, but exactly one conversion requiring an argument, with matching
signedness, is allowed. Combine multiple typed values through a
`cpkt_sqlite_string` builder. The ordinary variadic formatting functions
accept only C89-native argument types.

For file controls `SQLITE_FCNTL_SIZE_HINT`, `SQLITE_FCNTL_MMAP_SIZE`, and
`SQLITE_FCNTL_SIZE_LIMIT`, pass a `cpkt_sqlite_i64 *` to
`cpkt_sqlite_file_control()`. The same payload type is passed to facade VFS
file-control callbacks. Other file-control payloads keep their SQLite-defined
types.

FTS5 extension registration is also facade-owned. `cpkt_sqlite_fts5_api_open()`
returns a receiver scoped to its database connection. It registers locale-aware
custom tokenizers and auxiliary functions without exposing native FTS types.
An auxiliary callback receives a callback-local `cpkt_sqlite_fts5_context`
receiver for phrase, occurrence, token, column, row-count, query-phrase, and
auxiliary-data operations. Do not retain that context, its phrase iterator, or
its text/token views after the callback returns. Closing the database releases
registered tokenizer and auxiliary bindings and invokes their supplied
destructors exactly once.
The query-phrase callback receives its own callback-local context; its
`user_data` receiver returns the pointer passed to `query_phrase`.

Static consumers use `find_package(CpktSqlite CONFIG REQUIRED)` and
`cpkt::sqlite`, or `pkg-config --static --libs cpkt-sqlite`. Shared consumers
link `cpkt::sqlite_shared`.

The V0 delivery contract is a complete adapter for the published SQLite 3.53.4
public surface, not only the receiver convenience layer shown above. Its
required feature families and coverage rule are recorded in
`docs/sqlite-c89-facade-surface.md`.
