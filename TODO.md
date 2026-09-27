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
- [x] Generate every synchronous high-level client entry point with complete
  C89 arguments, owned outputs, native method/status semantics and history page
  callbacks; verify continuation release and conversion failures with a native
  network peer.
- [x] Generate the complete async client header: 59 operations and 31 callback
  types, preserving native IDs, header updates, callback/NULL semantics,
  cancellation, timeout and session-close cleanup, with exact 64-bit,
  reentrant, independent native parity and allocation-failure tests.
- [x] Expose native single-item data/event monitoring and subscription/item
  defaults, preserving complete notifications, reentrant deletion and the
  native default factory's shallow node ownership with native parity tests.
- [x] Expose 24 native core client connection, discovery, session and namespace
  operations, complete state enums, and four native client timer operations;
  verify synchronous/asynchronous session transfer, reverse connections,
  complete discovery records, reentrant timers and external-loop cleanup.
- [x] Expose native server timers and child-node iteration with C89 callback
  arguments, complete 64-bit IDs and reentrant cancellation/cleanup tests.
- [x] Generate full AccessControl and HistoryDatabase callback records, including
  staged replacement, nested conversions and allocation-failure coverage.
- [x] Generate the full HistoryDataBackend callback record, persistent borrowed
  DataValue storage, and native default gathering/database installation with
  allocation-failure and cleanup tests.
- [x] Expose native public value predicates, Variant/ExtensionObject ownership
  setters, status and byte/string helpers, array resize/move/copy append, and
  checked native ordering with upstream parity and ownership/OOM tests.
- [x] Expose the complete public configuration key/value map operations with
  borrowed lookup identity, native shallow ownership, atomic deep merge and
  allocation-failure/native parity coverage.
- [x] Expose complete identifier parse/print/extended URI forms, namespace
  mappings, exact 64-bit time/calendar/Unix conversions and native RNG helpers
  with independent native parity, ownership, preallocated-buffer and OOM tests.
- [x] Expose all native public value constructors, null identifier/string/GUID
  constants and C89 shorthand macros, numeric-range parsing, endpoint parsing,
  number readers and constant-time comparison. Preserve borrowed addresses,
  native owned/partial allocation results and parser outputs with native parity
  and allocation-failure tests.
- [x] Expose typed server-local async read/write/call submissions and context
  cancellation with synchronous completion, native pending results, timeout,
  shutdown, and conversion/allocation error coverage. Producer-side async
  completion is now exposed through the producer bindings below.
- [x] Expose full value-source and method callback signatures, setters/getters,
  native async result identities, cancellation hooks, timestamp synchronization,
  partial results and persistent native borrowing with C89/native/OOM tests.
- [x] Expose complete native callback-source/method creation and generic node
  begin/finish interfaces with assigned-ID dispatch during constructors,
  reentrant replacement, native failure parity and output rollback tests.
- [x] Generate full value notifications and node/global lifecycle callbacks,
  preserving original mutable contexts, reentrant replacement, destruction and
  owned child IDs with native-engine parity and allocation/logging coverage.
- [x] Expose external value sources through stable borrowed native pointer slots,
  preserving selection changes during read notifications, shared storage,
  native writes, and caller-owned lifetime with native pointer/OOM tests.
- [x] Expose stock memory/circular history backend factories and every callable
  callback slot, preserving native borrowed values and callback overrides.
- [ ] Use generated typed records at remaining PubSub component configuration,
  custom history gathering and stock gathering/database factories, and
  custom security/event-loop/nodestore plugin boundaries. Preserve the upstream
  behavior and ownership rules; no C99-only native callback substitutes for
  a promised C89 binding. Custom native datatype registration is still separate.
- [ ] Compare generated public type layouts and indices with the last released
  model during dependency upgrades, enforcing the existing compatibility policy.
