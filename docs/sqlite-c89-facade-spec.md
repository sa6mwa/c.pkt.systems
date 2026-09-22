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

`db->close(db)` is the convenience destructor: it defers native teardown until
facade-owned statements, blobs, and backups have been finalized or closed, so
callback state never outlives its facade owner. Use `cpkt_sqlite_close_strict()`
when the caller needs strict close behavior: it returns `CPKT_SQLITE_BUSY` and
leaves the receiver usable while any such child remains open.

`cpkt_sqlite_i64` holds exact signed 64-bit two's-complement bits in two
32-bit words, avoiding a public `long long` requirement. Use
`cpkt_sqlite_i64_make(high, low)` for binding and compare `high` and `low`
after `column_i64` or `last_insert_rowid`.

FTS5 extension registration is also facade-owned. `cpkt_sqlite_fts5_api_open()`
returns a receiver scoped to its database connection. It registers locale-aware
custom tokenizers and auxiliary functions without exposing native FTS types.
An auxiliary callback receives a callback-local `cpkt_sqlite_fts5_context`
receiver for phrase, occurrence, token, column, row-count, query-phrase, and
auxiliary-data operations. Do not retain that context, its phrase iterator, or
its text/token views after the callback returns. Closing the database releases
registered tokenizer and auxiliary bindings and invokes their supplied
destructors exactly once.

Static consumers use `find_package(CpktSqlite CONFIG REQUIRED)` and
`cpkt::sqlite`, or `pkg-config --static --libs cpkt-sqlite`. Shared consumers
link `cpkt::sqlite_shared`.

The V0 delivery contract is a complete adapter for the published SQLite 3.53.4
public surface, not only the receiver convenience layer shown above. Its
required feature families and coverage rule are recorded in
`docs/sqlite-c89-facade-surface.md`.
