# Diagnostic and logging destinations

The SDK does not link a logging framework. Use the native or C89 callback
below and forward each borrowed record to the application's logger. Callback
contexts stay caller-owned. File and interactive output intentionally requested
by applications are separate from diagnostic records.

| Component | C89 or native destination | Scope and behavior |
| --- | --- | --- |
| open62541 / `cpkt_opcua` | `cpkt_opcua` logging plugin callbacks | Client/server construction, runtime, and teardown; native severity and category. See [OPC UA facade](opcua-c89-facade-spec.md#logging-plugin). |
| SQLite / `cpkt_sqlite` | `cpkt_sqlite_global_config_log` | Native `SQLITE_CONFIG_LOG` callback; configure before SQLite initializes. Errors remain queryable on connection/statement handles. |
| PostgreSQL libpq / `cpkt_postgres` | Process default or per-connection diagnostic sink; server notice receiver/processor; per-connection protocol trace sink | Client warnings/debug, server notices, and wire records are distinct. Register process default before connection creation to catch startup warnings. See [PostgreSQL facade](postgres-c89-facade-spec.md#diagnostic-and-trace-delivery). |
| Lua / `cpkt_lua_runtime` | `cpkt_lua_runtime_set_warning_callback` | One native warning fragment and continuation flag per callback. Direct Lua states can use `cpkt_lua_setwarnf`. |
| whisper.cpp and ggml / `cpkt_sus` | `cpkt_sus_log_set` | Both model and GGML validation messages use the installed sink, with native log levels. |
| MIT Kerberos / `cpkt_gssapi` | `cpkt_gss_set_trace_callback` | Opt-in trace for internally constructed GSS contexts; register before concurrent use. Explicit GSS statuses remain available. |
| Cyrus SASL / `cpkt_sasl` | SASL log callback in the C89 callback set | Native SASL severity is forwarded by the facade. |
| OpenLDAP / liblber | Native strict-C89 `ber_set_log_print_fn(sink)` | Global BER print sink also receives LDAP's Cyrus SASL status and client log records. Install before LDAP/Cyrus initialization. The typed setter returns the previous sink; passing null restores the default. BER print records have no severity argument; direct Cyrus SASL callbacks do. |
| iODBC | Native `SQL_ATTR_TRACEFILE`, `SQL_ATTR_TRACE`, and `SQLGetDiagRec` | File trace and structured ODBC diagnostics through strict C89 headers. The SDK has no iODBC facade. |

The OpenLDAP print callback receives a complete borrowed line. The typed setter
avoids the function-pointer-to-`void *` cast otherwise required by the upstream
`LBER_OPT_LOG_PRINT_FN` option. It replaces the
library's default print destination, and the caller must keep the global
callback installed for as long as LDAP operations may emit records. Its
signature carries text only; applications that require a severity can use a
separate source tag and classify the text themselves. Direct Cyrus clients can
use the level-bearing SASL callback. When no custom BER print sink is set,
OpenLDAP/Cyrus retain their native destinations.

For components with explicit error retrieval instead of unsolicited logging,
read the returned status and error object at the call site. File trace writers
remain useful for opt-in debugging. No callback API changes the underlying
library's treatment of a message as a warning, error, or trace event.
