# Logging hooks across the SDK

- [ ] Audit every shipped dependency and public facade for diagnostic output
  to stdout, stderr, files, or platform sinks. Cover initialization, runtime,
  failure paths, reconfiguration, and destruction; distinguish diagnostic logs
  from explicit application output and interactive input prompts.
- [ ] For each emitting component, record whether an upstream callback already
  covers that output and whether C89 consumers can reach it through the shipped
  interface. Expose missing hooks through the facade first. Patch upstream only
  when no suitable public hook exists. Do not invent logging for dependencies
  that do not emit diagnostics.
- [ ] Add regression coverage for each actual gap: destination replacement,
  levels/categories, formatting, callback lifetimes, and suppression of the
  default destination. Keep optional logger integrations in tests; never make
  libpslog a shipped SDK dependency.

Known starting points (not a completed inventory):

| Component | Existing facility / follow-up |
| --- | --- |
| open62541 / `cpkt_opcua` | Implemented: first-class C89 callbacks before construction and for existing client/server handles, preserving security/event-loop references. The key-derivation stdout bypass is patched and covered by injected-failure tests. See the [logging contract](docs/opcua-c89-facade-spec.md#logging-plugin). |
| SQLite / `cpkt_sqlite` | Already exposes `cpkt_sqlite_global_config_log`, corresponding to SQLite's process-wide `SQLITE_CONFIG_LOG`. Respect SQLite's configuration timing and callback reentrancy rules. Check existing coverage before adding work. |
| PostgreSQL libpq / `cpkt_postgres` | Already exposes notice receiver and processor callbacks. Connection/result errors also have explicit retrieval APIs. Audit optional protocol tracing separately: `PQtrace` / `cpkt_postgres_trace` currently accept `FILE *`, not a record callback. Establish whether an upstream hook exists before proposing changes. |
| Remaining shipped components | Audit OpenSSL, zlib, curl, nghttp2, libssh2, libxml2, libpng, libHaru, iODBC (manager and configuration library), Lua, miniaudio, whisper.cpp/ggml, MQTT-C, MIT Kerberos, Cyrus SASL, and OpenLDAP, including embedded/transitive diagnostic paths. No missing hook is presumed until output and its available controls have been inspected. |

The public upstream SDK surface and C89 facade are both part of this audit.
Native callbacks requiring upstream headers do not replace a usable C89 logging
interface. Deliver records as emitted; materializing one formatted record is
acceptable, but do not buffer a sequence of logs or spool them through a file
behind a callback interface.

# Remaining OPC UA function and callback coverage

- [x] Inventory enabled public declarations, configuration fields and callback
  slots with the configured compiler. A maintained coverage contract rejects
  new or changed declarations, missing bindings and incomplete C89 records.
- [ ] Finish the contract's explicit pending classifications/bindings and switch
  the standard contract test to `--require-complete`. Existing convenience or
  aggregate bindings need semantic review; pending counts are not a claim that
  every entry lacks implementation. Native escape hatches do not count.
- [x] Generate synchronous/asynchronous client services and full subscription,
  monitored-item and event notifications with native request/context lifetimes.
- [x] Generate schema-only server operations and native attribute defaults.
- [x] Expose native server timers and child-node iteration with C89 callback
  arguments, complete 64-bit IDs and reentrant cancellation/cleanup tests.
- [x] Generate full AccessControl and HistoryDatabase callback records, including
  staged replacement, nested conversions and allocation-failure coverage.
- [x] Generate the full HistoryDataBackend callback record, persistent borrowed
  DataValue storage, and native default gathering/database installation with
  allocation-failure and cleanup tests.
- [x] Expose typed server-local async read/write/call submissions and context
  cancellation with synchronous completion, native pending results, timeout,
  shutdown, and conversion/allocation error coverage. Producer-side async
  completion tokens remain part of the value-source/method callback work.
- [ ] Use generated typed records at remaining PubSub component configuration,
  custom history gathering and stock memory/circular backend factories, server method/value-source/lifecycle
  callbacks, producer-side async completion tokens, and custom
  security/event-loop/nodestore plugin boundaries. Preserve the upstream
  behavior and ownership rules; no C99-only native callback substitutes for
  a promised C89 binding. Custom native datatype registration is still separate.
- [ ] Compare generated public type layouts and indices with the last released
  model during dependency upgrades, enforcing the existing compatibility policy.
