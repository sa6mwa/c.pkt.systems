# OPC UA C89 Facade Specification

This document records the production contract for the `cpkt` C89 facade over
the bundled open62541 OPC UA stack. The facade is not a protocol
reimplementation. It is a C89-compatible public boundary that owns stable
handles, values, configuration records, callbacks, lifetime rules, and tests
while delegating OPC UA semantics to open62541.

## Goals

- Ship open62541 v1.5.8 as part of the `c.pkt.systems` SDK, with license,
  dependency provenance, docs, examples, static libraries, shared libraries, and
  source distribution artifacts covered by package verification.
- Provide a C89-compatible `include/cpkt/opcua.h` facade that lets C89 consumers
  build practical OPC UA clients and servers without including open62541 public
  headers or using C99-only upstream types in public signatures.
- Generate the complete standard public schema model in C89 using upstream's
  parser and C generator, with transparent conversions at typed API boundaries.
  Keep native extension hooks for hand-written plugin interfaces still awaiting
  C89 bindings.
- Prove facade behavior with integration tests that cross the facade/native
  boundary in both directions.

## Non-Goals

- Do not fork open62541 into a new implementation.
- Do not maintain a second schema parser or hand-written copies of hundreds of
  schema types. Adapt the upstream generator; centralize representation rules
  and conversion, including nested structures, arrays and 64-bit values.
- Do not expose open62541 headers from `include/cpkt/opcua.h`. Native callbacks
  receive borrowed `void *` upstream handles; users who opt in can cast them in
  implementation files that include open62541 headers.
- Do not hide blocking I/O, filesystem reads, certificate loading, trust-store
  mutation, or endpoint registration behind generic constructors.

## Constraints

- `include/cpkt/opcua.h` must compile as C89 and C++. It must not require
  `<stdbool.h>`, `<stdint.h>`, C99 inline semantics, compound literals,
  variadic macros, or upstream open62541 typedefs.
- Public integer typedefs carry upstream ids and status codes as C89-safe
  storage types. Range checks are mandatory when converting to upstream types.
- Public OPC UA `UInt64` and `DateTime` values must not expose `uint64_t`,
  `int64_t`, or `long long`. They are represented as explicit high/low 32-bit
  words so C89 consumers work on targets where the upstream C99 stack supports
  64-bit protocol values on 32-bit hardware. The implementation must compile
  assert that upstream `UA_UInt64` and `UA_DateTime` are 64 bits and must widen
  each public word to the upstream 64-bit type before shifting.
- Public handles remain opaque: `cpkt_opcua_client`, `cpkt_opcua_server`, and
  future opaque value/config handles.
- Borrowed input memory is valid only for the duration of the call. Public APIs
  that return variable-length data use caller-provided buffers, explicit
  required-size outputs, or `cpkt`-owned handles with matching free functions.
- Native escape hatches are explicit and documented. Native pointers are
  borrowed and valid only for the callback duration.
- Any downstream patch to open62541 or bundled MQTT code must be recorded under
  the existing vendor patch flow with SPDX headers where applicable.
- MPL-2.0 license text and upstream notices must be included in binary and
  source artifacts. Source archives must include the source needed to rebuild
  from the tarball and must use the injected archive `VERSION` fallback when
  the tree is not itself a Git worktree.

## Current Facade Surface

The existing facade already covers a useful core workflow:

- Anonymous and username/password client connection.
- Client endpoint URL and server application discovery.
- Server construction on a TCP port, explicit endpoint host/port configuration,
  startup, iteration, shutdown, and endpoint URL formatting.
- Server application identity configuration before startup.
- Null, numeric, string, GUID, and byte-string `NodeId` values.
- `NodeId` compare, parse, and print helpers for null, numeric, string, GUID,
  and byte-string ids.
- Scalar values: empty, boolean, integer, unsigned 64-bit integer, date/time,
  double, string, and byte string.
- C89 `DataValue` reads plus borrowed native `UA_Variant` and `UA_DataValue`
  callbacks for client/server value reads that need unsupported upstream
  payloads.
- Server-side object, variable, variable-under-parent, and scalar method nodes.
- Client-side object and writable scalar variable creation.
- Client/server delete-node and add/delete-reference wrappers.
- Scalar read/write from both client and server handles.
- Namespace registration and namespace URI/index lookup.
- Direct client/server reads for node id, node class, browse name, display name, and
  description; client/server reads for data type, value rank, access level, and
  historizing/executable metadata; client/server writes for display name,
  description, access level, historizing, and executable metadata.
- Browse-children callbacks with default and extended browse options, browse
  continuation points, browse-next, and hierarchical browse-path translation.
- Synchronous scalar method calls with one or more scalar outputs, plus method
  input/output argument metadata reads.
- Client value subscriptions, subscription modification, monitored items with
  queue/discard/deadband options, and monitoring-mode changes.
- Server prepared-event helpers for create/set-field/trigger workflows, plus
  client event monitored items for fixed event summaries and selected event
  fields decoded through the C89 value layer.
- Typed security configuration from explicit certificate/key/trust-list buffers.
  File-based certificate loading and custom security plugins remain available
  through native config callbacks.
- C89 username/password access-control callbacks for common login decisions,
  with full access-control plugins still available through native config
  callbacks.
- Native callbacks for client and server escape hatches.
- Explicit advanced native pass-through entry points for PubSub/MQTT, history,
  file/json server configuration, and security plugin configuration, plus
  selected asynchronous client read, write, browse, method-call, and node-add
  operations.
- File/json server configuration constructors from explicit JSON bytes and
  explicit file paths, with no implicit config-file discovery.
- PubSub/MQTT convenience wrappers for common MQTT connection, published data
  set, published variable, writer group, data set writer, reader group, data
  set reader, and PubSub configuration byte-string save/load workflows.
- Client raw-history reads for common historical `DataValue` workflows, decoded
  through the C89 value layer with native history services still available
  through explicit pass-through.

The generated model below extends the practical C89 surface. Some hand-written
configuration and plugin interfaces still use native callbacks; type completeness
does not imply that every such function is already wrapped.

## Upstream Surface Inventory

| Area | open62541 headers | Facade decision | Reason |
| --- | --- | --- | --- |
| Core handles and event loops | `client.h`, `server.h` | First-class wrappers | Small, stable, handle-oriented, already partly covered. |
| Client connection and discovery | `client.h`, `client_config_default.h` | First-class common wrappers plus native config callback | Endpoint discovery is common; full config is large and security-sensitive. |
| Read/write attributes | `client_highlevel.h`, `server.h` | First-class generic attribute wrappers | The upstream high-level API maps cleanly to C89 ids, values, and status codes. |
| Node management | `client_highlevel.h`, `server.h` | First-class wrappers for common node classes; native pass-through for full attributes | Object/variable/method/view/reference helpers are useful; generated attribute structs are large. |
| Browse and translate | `client.h`, `client_highlevel.h`, `server.h` | First-class wrappers | Browse options and continuation points are central client workflows. |
| Methods | `client_highlevel.h`, `server.h` | First-class multi-input and multi-output wrappers | Current one-output scalar wrapper is too narrow. |
| Subscriptions | `client_subscriptions.h` | First-class wrappers | Data-change, event, modify, delete, and monitoring-mode APIs are core client workflows. |
| Value and data model | `types.h`, `types_generated.h` | First-class C89 value layer with native variant escape hatch | Scalars, arrays, strings, byte strings, GUIDs, time, localized text, qualified names, status, data values, and node ids must be usable from C89. |
| Generated request/response services | `client.h`, `types_generated.h` | Generated C89 types and 14 typed public client service bindings | Reuse upstream parsing and declarations; convert recursively without changing the upstream ABI. |
| Security and certificates | `server_config_default.h`, `client_config_default.h`, plugin security headers | First-class buffer configuration wrappers plus native config callback | Common secure setup needs DX; file loading and custom policies/stores are application-specific and can be handled explicitly in native callbacks. |
| Access control | `plugin/accesscontrol*.h` | First-class username/password callback adapter for common login decisions; native pass-through for full plugin | Callback ABI can be C89; full plugin model stays upstream-owned. |
| History | `client.h`, `plugin/historydatabase.h`, `plugin/historydata/*` | Native-first server/backend setup plus common raw-history client reads | History backends are application-specific; raw value reads map cleanly to the C89 `DataValue` layer. |
| Events and alarms/conditions | `server.h`, `client_subscriptions.h` | First-class event creation/trigger and event monitored items; alarms/conditions native-first | Events are common; alarms/conditions are broad generated models. |
| PubSub and MQTT | `pubsub.h`, `server_pubsub.h` | Native-first plus common MQTT connection, publisher, subscriber, and config byte-string wrappers | PubSub is extensive and config-heavy. open62541 owns the MQTT integration; the facade should avoid duplicating generated config structures. |
| Async services | `client_highlevel_async.h`, `server.h` async operations | Native-first, with selected callbacks only when a concrete workflow needs them | Async callbacks can be C89, but full async service mirroring is large. |
| File/json server config | `server_config_file_based.h` | Explicit JSON bytes/file constructors plus native config callback | Useful, but file I/O must stay explicit at the application boundary. |
| Logging | `plugin/log.h` | First-class C89 callback with every upstream level/category, constructor configuration, and destination replacement | Capture initialization, runtime, security, event-loop, and destruction messages without depending on a logging library. |
| Event loop plugins | `plugin/eventloop.h` | Native config callback | Upstream event-loop customization remains available through native configuration. Its logs use the same configured logger. |

## Generated C89 public model

Include `<cpkt/opcua_types.h>` for all 388 types in the bundled standard public
schema graph. This includes request/response records, nested arrays, attributes,
PubSub configuration records, enums and opaque aliases. Types and field names
follow upstream, replacing `UA_` with `cpkt_opcua_`; existing lowercase
convenience records remain supported. No upstream private transport types are
exposed. Generated type completeness is checked against the native count and
every native type index at compile time.

`tools/opcua/generate.py` copies the installed upstream tools into the build tree
and adds a small emitter hook after `CGenerator.write_definitions`.
`c89_emitter.py` uses that generator's already parsed and filtered type graph,
its struct/enum declaration routines and the public client service declarations.
There is no second schema parser. Native dependency builds, their public type
layouts and their ABI are unchanged. A changed upstream hook, unsupported
builtin/class, missing member type, or mismatched native table fails generation
or compilation before tests/package production. The extra-schema/table options
are for regression fixtures, not an installed custom-type registration API.

The 25 builtin representations form the small maintained foundation. Boolean
is a C89 byte with zero/nonzero truth; short/int carry exact 16/32-bit values
on the supported target matrix. UInt64, Int64 and DateTime use two unsigned
32-bit words, most significant first. Signed values use two's-complement bits;
for example INT64_MIN is `{0x80000000U, 0U}`. The bridge widens before shifting
and uses bit-preserving copies for signed native values. Native byte order,
word alignment and C89/native nested struct layouts need not match. All ordinary
schema member offsets in both representations are generated with `offsetof`.

The public descriptor describes a type, not an opaque payload. Payloads are
ordinary structs. Each type has generated `init/new/copy/clear/delete/equal`
helpers and an index for `cpkt_opcua_type_at`. The shared recursive bridge
handles optional pointers, union selections, arrays and nested records;
Variant dimensions, null versus empty arrays, recursive diagnostics, all
DataValue flags/picoseconds, and encoded/decoded ExtensionObjects are preserved.
Unknown custom native descriptors fail explicitly instead of being reinterpreted
as a standard type. Recursive conversion is bounded to 128 levels.

Initialize destination values empty, clear before reuse, and use the facade's
clear/delete helpers to release owned results. Copies own all their allocations,
even when the input Variant or ExtensionObject is borrowed. Clearing caller-built
NODELETE values preserves borrowed payloads/dimensions. Caller-built borrowed
strings, arrays and struct members must not be cleared as owned values. Failed
copy/decode/service conversions leave the output empty, including allocation
failure. `type_equal` returns false if conversion/allocation fails.

The 14 `cpkt_opcua_client_service_*` bindings mirror the public synchronous
service calls enabled in this bundle: read/write, historyRead/historyUpdate,
call, addNodes/addReferences/deleteNodes/deleteReferences, browse/browseNext,
translateBrowsePathsToNodeIds, registerNodes/unregisterNodes. They return a
conversion status. A successful conversion can still contain an upstream service
error: inspect `responseHeader.serviceResult` and per-operation statuses.
`cpkt_opcua_server_read_typed` and `server_write_typed` expose complete native
DataValue operations; server write returns the upstream operation status. The
binary encode/decode helpers delegate to the upstream codec and materialize a
message just as that codec does. They make no streaming claim.

Verification covers every empty and populated standard type against independently
constructed native values and identical wire bytes, nested 64-bit array read/write
against a native server, browse/error responses, borrowed ownership, partial
allocation failures, strict C89/C++98 compilation, exact public exports and
Valgrind. Separate generated schema fixtures exercise optional fields, union
branches/arrays, invalid selections and 64-bit option-set constants. The installed
C89 example exercises typed server read/write too.

The standard graph and its indices are part of the new public facade surface.
Dependency upgrade review must compare generated layouts, indices and semantics
against the last released bundle alongside the existing ABI checks. Generated
model coverage does not yet make custom plugin configuration, generic async
services, subscriptions or event-loop vtables fully typed C89 APIs; track those
function/callback boundaries separately.

## Facade API Tiers

### Logging plugin

`cpkt_opcua_log_config` carries a callback, borrowed user data, and a minimum
level. Zero means TRACE; a NULL callback explicitly silences output. Passing a
NULL configuration to a logger-aware constructor retains upstream's default
stdout logger. Existing constructors preserve that default as well.

Use `cpkt_opcua_server_new_with_logger`,
`cpkt_opcua_server_new_from_json_with_logger`,
`cpkt_opcua_server_new_from_json_file_with_logger`, or
`cpkt_opcua_client_new_with_logger` to capture configuration and initialization
logs. `cpkt_opcua_server_set_logger` and `cpkt_opcua_client_set_logger` replace
the callback/filter in place, preserving the plugin address borrowed by
upstream event-loop and security plugins. Configuration is copied. User data
and cleanup belonging to an existing native logger are preserved until upstream
clears that plugin; the original context is restored for its cleanup callback.
The chosen facade destination is retained if a failed security setup clears the
upstream server configuration, so another configuration attempt uses the hook.
Callback user data must remain valid through destruction and failed construction;
it is never freed by the facade. Configure while no other thread operates on that handle.
Native extensions must not replace or free the configured logging plugin.

Callbacks run synchronously on the emitting thread and receive one borrowed
record with the original level/category and an explicitly sized, NUL-terminated
message. Formatting uses `UA_String_vformat`, including upstream `%S`, `%N`,
and `%Q` conversions, without a fixed-size truncation buffer. Empty messages
and embedded NUL bytes are preserved. Each formatted message is freed after
the callback returns; no message sequence is buffered or spooled. A formatting
or allocation failure produces a diagnostic record with nonzero
`format_status`. The facade does not add timestamps, prefixes, or newlines.

Callbacks must not reenter, reconfigure, or destroy the emitting handle. They
may overlap under concurrent upstream use, so applications must synchronize
their own destination state. FATAL identifies severity; an application decides
whether to terminate. The SDK is built with upstream `UA_LOGLEVEL=100`, allowing
every level; a callback cannot recover events compiled out by another upstream
build. Explicit application output and upstream interactive key-password
prompts are not log-plugin events.

The bundle patches one upstream bypass: ECC/XDHE OpenSSL key-derivation errors
are dispatched through the owning security policy's logger instead of dumping
the error queue directly to stdout. Queue entries are delivered individually;
existing upstream function signatures and ABI identities are preserved. Disabled
packet/parser debug dump facilities are not enabled by this bundle.

Static/shared C89 consumers test every level/category and event-loop/security
plugin reference, constructor/JSON failure logs, filters, destination changes,
shutdown, special formatting, and messages larger than upstream stdout's fixed
buffer, plus injected message-allocation failure in Linux static consumers.
Test-only libpslog integration forwards records directly to `log_view`
and consumes its output chunks with bounded state. It does not call libpslog's
terminating `fatal_view` method. No libpslog dependency is added to SDK headers,
libraries, CMake/pkg-config metadata, or installed files.

### Tier 0: Baseline Already Present

Tier 0 remains supported and must not regress. It covers the current API listed
above and is the minimum smoke-test surface for package consumers.

### Tier 1: Current Practical C89 Facade

Tier 1 is the released first-class C89 OPC UA surface.

- General C89 value layer:
  - scalar numeric widths needed by OPC UA without using C99 names in public
    signatures;
  - keep boolean, integer, double, string, and byte-string scalar values as the
    first stable value slice;
  - arrays for each supported scalar/value kind;
  - GUIDs, date/time, localized text, qualified names, status codes, data
    values, and variant handles;
  - parse/print helpers for node ids, GUIDs, qualified names, and localized
    text;
  - generated typed Variant/DataValue and ExtensionObject payloads for every
    standard public schema type; native hooks remain for custom native types.
- Expanded node ids:
  - keep null, numeric, string, GUID, and byte-string constructors and
    compare/parse/print helpers as the first stable node-id slice;
  - expanded node ids where a namespace URI or server index is needed;
  - compare, parse, and print helpers.
- Generic attribute access:
  - keep the direct client/server read helpers for `NodeClass`, `BrowseName`,
    `DisplayName`, `Description`, `DataType`, `ValueRank`, `AccessLevel`, and
    `Executable`, plus client `UserAccessLevel`/`UserExecutable` and
    display name, description, access-level, historizing, and executable
    writes, as the first stable attribute slice;
  - client and server read/write for `Value`, `NodeId`, `NodeClass`,
    `BrowseName`, `DisplayName`, `Description`, `WriteMask`, `IsAbstract`,
    `Symmetric`, `InverseName`, `ContainsNoLoops`, `EventNotifier`, `DataType`,
    `ValueRank`, `ArrayDimensions`, `AccessLevel`, `AccessLevelEx`,
    `MinimumSamplingInterval`, `Historizing`, `Executable`, and
    `UserExecutable` where upstream supports them;
  - array range and index-range reads/writes through value handles.
- Node-management wrappers:
  - keep object/variable/method creation plus delete-node and add/delete-reference
    wrappers as the first stable node-management slice;
  - add/delete nodes and references from client and server;
  - object, variable, method, view, reference type, object type, variable type,
    and data type nodes for common attributes;
  - begin/finish or native callback hooks for complex node construction.
- Browse and path translation:
  - keep browse options for direction, reference type, node class mask, result
    mask, max references, continuation points, browse-next, and hierarchical
    browse-path translation as the first stable browse slice;
  - browse options for reference type, direction, node class mask, result mask,
    max references, and continuation point handling;
  - translate browse paths to node ids.
- Methods:
  - keep multi-output scalar server callbacks, client calls, and method
    argument metadata reads as the first stable method-completion slice;
  - multiple input and output values;
  - method metadata arguments;
  - server method callbacks with borrowed input values and copied output values;
  - async method completion only through an explicitly named async API.
- Subscriptions:
  - keep create/modify/delete subscriptions, scalar data-change monitored
    items, and monitoring-mode changes as the first stable subscription slice;
  - create/modify/delete subscriptions;
  - data-change monitored items with sampling/filter/queue/discard options;
  - event monitored items with event field callbacks;
  - monitoring mode changes;
  - callback failure and cleanup semantics.
- Client discovery and namespace helpers:
  - keep endpoint URL and server application count/read helpers as the first
    stable discovery slice;
  - get endpoints;
  - find servers;
  - namespace URI/index lookup and namespace registration.
- Server configuration helpers:
  - keep application URI, product URI, and application name setters as the
    first stable server configuration slice;
  - application URI, product URI, application name, hostname, endpoint port, and
    endpoint URL;
  - namespace registration;
  - certificate/private-key/trust-list setup from explicit buffers;
  - certificate/private-key/trust-list setup from explicit paths through native
    config callbacks so the caller owns blocking filesystem behavior;
  - username/password and anonymous policy helpers: `allow_anonymous` controls
    anonymous access independently of the credential callback; the callback
    handles username/password sessions only, and tokens with both fields empty
    are rejected before it is invoked;
  - native config callback before startup.
- Event helpers:
  - create event, set event field, trigger event;
  - client event monitored items with field extraction through the value layer.

### Tier 2: Advanced Pass-Through With Convenience Entry Points

Tier 2 keeps hand-written advanced interfaces reachable. Schema-defined public
structures are generated in C89; the remaining work is their function and callback
boundaries, not maintaining hand-written copies of the structures.

- PubSub/MQTT:
  - convenience wrappers for common MQTT broker, topic, publisher, subscriber,
    dataset writer, and dataset reader setup;
  - load/save PubSub configuration as byte strings;
  - native server callback for full `UA_Server_*PubSub*` configuration;
  - tests use loopback or a deterministic local broker only when available.
- Security plugins:
  - native hooks for custom security policies, certificate groups, and access
    control plugins;
  - convenience helpers for bundled default policies only.
- History:
  - client history-read wrappers for common raw value reads;
  - server history backend registration through native callbacks and selected
    C89 callback adapters.
- Async services:
  - selected async read, write, browse, call, and add-node wrappers with C89
    callbacks;
  - raw async service pass-through through native callbacks.
- File/json server config:
  - create server from explicit JSON bytes or explicit file path;
  - no implicit config-file discovery.
- Alarms/conditions, custom data types, NodeSet loading, event loop plugins,
  reverse connect, and low-level network message encoding stay native-first
  unless a concrete downstream workflow needs a typed wrapper.

## Pass-Through Rules

Pass-through is a supported part of the facade, not a loophole.

- Every pass-through entry point must be named `*_native`, `*_native_config`, or
  another explicit name that tells users they are crossing into upstream-owned
  API territory.
- Native callbacks receive borrowed pointers and may not store them beyond the
  callback unless the upstream API explicitly transfers ownership.
- Facade functions must not include open62541 headers in public signatures.
- Pass-through wrappers must return facade results and, where applicable, carry
  upstream status through `cpkt_opcua_status`.
- Tests must cover that native callbacks are invoked with non-null upstream
  handles and that facade state remains usable after the callback returns.

## Memory And Error Contract

- All fallible functions return `cpkt_opcua_result`.
- Upstream service failures return `CPKT_OPCUA_ERR_UPSTREAM` and write the
  upstream status when `status_out` is non-null.
- Type mismatches return `CPKT_OPCUA_ERR_TYPE`; numeric narrowing failures return
  `CPKT_OPCUA_ERR_RANGE`; callback failures return `CPKT_OPCUA_ERR_CALLBACK`.
- Output parameters are set to safe values on failure when practical.
- String and byte outputs use either caller buffers with required-size reporting
  or facade-owned handles with matching cleanup functions.
- Callback input values are borrowed. Callback output values are copied before
  borrowed input storage is released.
- Cleanup functions tolerate null pointers unless documented otherwise.

## Integration Test Contract

Facade correctness must be proven with observable integration tests.

- Facade client against facade server for each Tier 0 and Tier 1 workflow.
- Every client/server workflow exposed by the facade must also have two
  boundary-crossing integration variants:
  - `server-is-c99-and-client-is-c89`: a native open62541 server built directly
    against the upstream C99 API, with a C89 facade client exercising the
    workflow only through `include/cpkt/opcua.h`.
  - `server-is-c89-and-client-is-c99`: a C89 facade server, with a native
    open62541 client built directly against the upstream C99 API exercising the
    same server-side behavior.
- Boundary-crossing variants are required for each facade slice as it lands, not
  only for final release. For method slices, the required assertions include
  metadata discovery (`InputArguments`/`OutputArguments`), executable metadata,
  successful calls, multi-output calls, string/byte-string callback aliasing,
  and invalid argument/range failures.
- The two boundary-crossing variants must cover read/write, browse, methods,
  subscriptions, discovery, namespace helpers, events, and selected security
  configuration as those facade surfaces land.
- Boundary tests must treat the native side as an external peer: no shared test
  internals, no direct access to facade private structs, and no assertions that
  depend on implementation details below the public API boundary.
- Native callback tests proving pass-through handles work without breaking
  facade-owned lifetime.
- Failure-mode tests for null arguments, invalid node ids, type mismatches,
  insufficient output buffers, upstream bad status codes, callback failures,
  string/value aliasing, subscription deletion, disconnect cleanup, and server
  shutdown cleanup.
- C89 compile tests for every installed public header and facade example.
- Installed SDK smoke tests that compile and link both static and shared
  consumers through CMake and pkg-config metadata.

## Packaging And Documentation Contract

- Binary SDK artifacts ship the facade header, `libcpkt` static/shared
  libraries, bundled open62541/mqtt-c libraries according to the package
  contract, CMake config, pkg-config metadata, examples, README material, and
  license files under `share/doc`.
- MPL-2.0 license text and open62541 notices are present in every artifact that
  redistributes open62541.
- mqtt-c license/provenance is present when mqtt-c is shipped or embedded in the
  open62541 build.
- Source artifacts include the docs directory, vendor patch metadata, tests,
  examples, scripts required to build from source, an injected `VERSION` file,
  and `RELEASE_MANIFEST`.
- Source archive payloads are derived from tracked non-ignored files plus
  deliberate generated release metadata. Archive entries are deterministic and
  owned by `0:0`.
- Package verification checks archive layout, license files, installed docs,
  C89 header compilation, static/shared consumers, source-tarball builds,
  privacy/relocatability, and stale artifact exclusion.

## Maintenance Contract

The documented Tier 1 surface and the selected Tier 2 convenience wrappers are
implemented. Future additions must preserve the C89 boundary, name native
escape hatches explicitly, document ownership and asynchronous callback
lifetimes in `include/cpkt/opcua.h`, and land with focused observable tests plus
installed-package coverage where the new surface is shipped.
