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
- [x] Verify OpenLDAP's Cyrus SASL status path through a local LDAPI socket in
  both static and shared builds. Keep optional logger integrations in tests;
  never make libpslog a shipped SDK dependency.

Implemented destinations and the remaining audit are described in
[the logging-hook guide](docs/logging-hooks.md).

Current inventory:

| Component | Existing facility / follow-up |
| --- | --- |
| open62541 / `cpkt_opcua` | Implemented: first-class C89 callbacks before construction and for existing client/server handles, preserving security/event-loop references. The key-derivation stdout bypass is patched and covered by injected-failure tests. See the [logging contract](docs/opcua-c89-facade-spec.md#logging-plugin). |
| SQLite / `cpkt_sqlite` | Already exposes `cpkt_sqlite_global_config_log`, corresponding to SQLite's process-wide `SQLITE_CONFIG_LOG`. Respect SQLite's configuration timing and callback reentrancy rules. Check existing coverage before adding work. |
| PostgreSQL libpq / `cpkt_postgres` | Process default and connection-specific client diagnostic callbacks now cover client warnings/debug including pre-connection option parsing; existing notice callbacks and result errors remain distinct. Formatted protocol trace records have a callback as well as the existing file destination. |
| Lua / `cpkt_lua_runtime` | Runtime warning fragments and continuation flags reach a per-runtime callback; the direct generated Lua facade exposes `cpkt_lua_setwarnf`. |
| whisper.cpp/ggml / `cpkt_sus` | Recoverable GGML validation messages now use the existing level-bearing `cpkt_sus_log_set` path. |
| MIT Kerberos / `cpkt_gssapi` | Process-wide trace registration is applied to new internal Kerberos GSS contexts, including NegoEx. |
| OpenLDAP/Cyrus SASL | LDAP's Cyrus log and status paths use typed C89 `ber_set_log_print_fn` and the global BER print callback; direct Cyrus clients retain the level-bearing C89 callback. |
| iODBC | Native strict C89 tracing to a caller-selected file and ODBC diagnostics are documented and tested; no facade is needed. |
| Other shipped components | Finish the diagnostic-output audit for OpenSSL, zlib, curl, nghttp2, libssh2, libxml2, miniaudio, and MQTT-C, including transitive paths. No missing hook is presumed until output and controls are inspected. |

The public upstream SDK surface and C89 facade are both part of this audit.
Native callbacks requiring upstream headers do not replace a usable C89 logging
interface. Deliver records as emitted; materializing one formatted record is
acceptable, but do not buffer a sequence of logs or spool them through a file
behind a callback interface.

# Remaining OPC UA function and callback coverage

- [x] Inventory enabled public declarations, configuration fields and callback
  slots with the configured compiler. A maintained coverage contract rejects
  new or changed declarations, missing bindings and incomplete C89 records.
- [x] Bind all enabled usable public declarations and run the standard contract
  test with `--require-complete`. Implementation-only declarations remain
  explicitly classified; native escape hatches do not count as C89 bindings.
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
- [x] Expose every custom history gathering callback and stock gathering/database
  factory with actual native settings borrows, callable slots, backend ownership,
  polling, payload alias preservation and allocation-failure tests.
- [x] Use generated typed records at PubSub component configuration and custom
  security, event-loop and nodestore plugin boundaries, including custom native
  datatype registration and the corresponding ownership rules.
- [ ] Compare generated public type layouts and indices with the last released
  model during dependency upgrades, enforcing the existing compatibility policy.
