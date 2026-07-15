# OPC UA C89 Facade Specification

This document records the production contract for the `cpkt` C89 facade over
the bundled open62541 OPC UA stack. The facade is not a protocol
reimplementation. It is a C89-compatible public boundary that owns stable
handles, values, configuration records, callbacks, lifetime rules, and tests
while delegating OPC UA semantics to open62541.

## Goals

- Ship open62541 v1.5.4 as part of the `c.pkt.systems` SDK, with license,
  dependency provenance, docs, examples, static libraries, shared libraries, and
  source distribution artifacts covered by package verification.
- Provide a C89-compatible `include/cpkt/opcua.h` facade that lets C89 consumers
  build practical OPC UA clients and servers without including open62541 public
  headers or using C99-only upstream types in public signatures.
- Keep advanced open62541 surfaces reachable through explicit native escape
  hatches instead of reimplementing large generated OPC UA structures.
- Prove facade behavior with integration tests that cross the facade/native
  boundary in both directions.

## Non-Goals

- Do not fork open62541 into a new implementation.
- Do not translate every generated OPC UA service request/response structure into
  a bespoke `cpkt` struct. Where a structure is very large, generated, or
  unstable, provide native pass-through and a small C89 convenience wrapper for
  common workflows.
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
  from the tarball and must use the repository `VERSION` fallback when `.git` is
  absent.

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

This is the Tier 1 practical C89 facade surface. Tier 2 advanced upstream
features remain native-first unless a concrete downstream workflow needs a
typed convenience wrapper.

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
| Generated request/response services | `client.h`, `types_generated.h` | Native pass-through plus selected convenience wrappers | Full generated structure mirroring would be a reimplementation burden. |
| Security and certificates | `server_config_default.h`, `client_config_default.h`, plugin security headers | First-class buffer configuration wrappers plus native config callback | Common secure setup needs DX; file loading and custom policies/stores are application-specific and can be handled explicitly in native callbacks. |
| Access control | `plugin/accesscontrol*.h` | First-class username/password callback adapter for common login decisions; native pass-through for full plugin | Callback ABI can be C89; full plugin model stays upstream-owned. |
| History | `client.h`, `plugin/historydatabase.h`, `plugin/historydata/*` | Native-first server/backend setup plus common raw-history client reads | History backends are application-specific; raw value reads map cleanly to the C89 `DataValue` layer. |
| Events and alarms/conditions | `server.h`, `client_subscriptions.h` | First-class event creation/trigger and event monitored items; alarms/conditions native-first | Events are common; alarms/conditions are broad generated models. |
| PubSub and MQTT | `pubsub.h`, `server_pubsub.h` | Native-first plus common MQTT connection, publisher, subscriber, and config byte-string wrappers | PubSub is extensive and config-heavy. open62541 owns the MQTT integration; the facade should avoid duplicating generated config structures. |
| Async services | `client_highlevel_async.h`, `server.h` async operations | Native-first, with selected callbacks only when a concrete workflow needs them | Async callbacks can be C89, but full async service mirroring is large. |
| File/json server config | `server_config_file_based.h` | Explicit JSON bytes/file constructors plus native config callback | Useful, but file I/O must stay explicit at the application boundary. |
| Logging and event loop plugins | `plugin/log*.h`, `plugin/eventloop.h` | Native config callback initially | These are integration hooks, not OPC UA domain APIs. |

## Facade API Tiers

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
  - native variant/data-value callbacks for unsupported generated or extension
    object payloads.
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
  - username/password and anonymous policy helpers;
  - native config callback before startup.
- Event helpers:
  - create event, set event field, trigger event;
  - client event monitored items with field extraction through the value layer.

### Tier 2: Advanced Pass-Through With Convenience Entry Points

Tier 2 keeps advanced upstream features reachable and documented without
attempting to mirror every generated structure.

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
