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
  plus generated full C89 access-control, history-database and history-backend
  plugin records with persistent borrowed-value storage.
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
| Generated request/response services | `client.h`, `types_generated.h` | Generated C89 types, 14 synchronous/asynchronous public services and specialized subscription service bindings | Reuse upstream parsing and declarations; convert recursively without changing the upstream ABI. |
| Security and certificates | `server_config_default.h`, `client_config_default.h`, plugin security headers | First-class buffer configuration wrappers plus native config callback | Common secure setup needs DX; file loading and custom policies/stores are application-specific and can be handled explicitly in native callbacks. |
| Access control | `plugin/accesscontrol*.h` | Complete generated C89 access-control callback record plus common login wrappers | Native authorization semantics are retained; schema arguments are converted at the callback boundary. |
| History | `client.h`, `plugin/historydatabase.h`, `plugin/historydata/*` | Full generated C89 HistoryDatabase callback record plus typed client history services | Full HistoryDataBackend, persistent borrowed values and default gathering/database bindings; custom gathering callbacks remain separate coverage work. |
| Events and alarms/conditions | `server.h`, `client_subscriptions.h` | First-class event creation/trigger and event monitored items; alarms/conditions native-first | Events are common; alarms/conditions are broad generated models. |
| PubSub and MQTT | `pubsub.h`, `server_pubsub.h` | Native-first plus common MQTT connection, publisher, subscriber, and config byte-string wrappers | PubSub is extensive and config-heavy. open62541 owns the MQTT integration; the facade should avoid duplicating generated config structures. |
| Async services | `client_highlevel_async.h`, `server.h` async operations | Generated typed async client services and full subscription/event callback records | Typed local read/write/call submissions and cancellation preserve native completion; value-source completion tokens and other callback/configuration boundaries remain separate coverage work. |
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
model coverage does not imply that every handwritten public interface has a C89
binding. PubSub component configuration, custom history gathering, custom
security/event-loop/nodestore plugins, full server method/value-source/lifecycle
callbacks and asynchronous value-source completion tokens remain distinct coverage work.

### Typed services and subscriptions

The generated client service bindings mirror native `UA_Client_Service_*`
operations in both synchronous and asynchronous forms. Generic
`cpkt_opcua_client_service_async` checks the request/response descriptor pair
before submission. Specialized subscription/monitored-item operations invoke
upstream's specialized functions, retaining native client bookkeeping. Request
storage may be released or changed after submission returns; the facade adds no
pending request buffer or queue. Native timeout, disconnect and destruction
complete each accepted request once. Destruction may process an already-arrived
successful response. Do not destroy the client from its callback.

The callback's conversion status is separate from `responseHeader.serviceResult`
and per-operation statuses. A response is borrowed until callback return; make
an owned typed copy to retain it. On conversion failure the response is NULL.
Native cancellation operates on request IDs/handles and does not replace the
original completion that controls user-data lifetime.

`cpkt/opcua_callbacks.h` exposes every DataValue field, subscription status and
deletion, monitored-item deletion, and the full named event KeyValueMap with
arbitrary generated Variants. Registration records are copied during creation;
caller contexts remain caller-owned until deletion. Subscription creation exposes
the native scalar subscription ID even if response conversion fails, so the
caller can delete it. If a successful monitored-item batch cannot be converted,
delete the known subscription before releasing contexts. Calls on one facade
handle are serialized as required by its lifecycle contract.

### Public API coverage contract

`tools/opcua/public_api.py` inventories every installed public header using the
configured target compiler's preprocessor. It includes enabled functions,
typedefs, enums, complete configuration/plugin records and global defaults.
System-header declarations and generated native compile assertions are not
consumer interfaces. Schema declarations and lifecycle helpers are matched
against the actual upstream-generated C89 model without duplicating its schema.

`tools/opcua/public_api_contract.json` records reviewed bindings, explicit pending
work and narrowly explained implementation helpers. The check rejects new or
changed native declarations, missing C89 declarations, incomplete public
records, changed enum values and native escape hatches used as bindings.
Binding existence is a structural gate; behavioral, ownership and lifetime
tests remain required. Existing convenience and aggregate bindings still need
semantic classification where an entry is pending. Pending counts therefore
measure unclassified declarations, not the number of missing implementations.
Linux-only syslog, Ethernet and filestore declarations follow the target's
actual header guards; their absence on Darwin does not waive Linux coverage.

Run the standard contract and negative tests with:

```sh
ctest --preset debug -R '^opcua_public_api_' --output-on-failure
```

The strict completion gate is:

```sh
cmake --build --preset debug --target cpkt_opcua_public_api_complete
```

It fails while any enabled declaration remains pending and writes its coverage
report only under the build directory. Run the equivalent target for every
shipped preset before declaring the full interface complete. During this
ongoing migration the standard contract test permits explicitly recorded
pending entries; switching it to strict mode is a remaining completion task.
The contract retains the source-header license/copyright notices in `_origin`.

### Typed server operations

The generator derives schema-only server functions directly from `server.h`:
all specialized attribute reads/writes, all seven non-method node classes,
context and namespace access, owned session attributes, browse/browseNext,
path translation, method-call results, object properties and references.
Names retain the upstream operation spelling as
`cpkt_opcua_server_<operation>_typed`. Native errors and per-result statuses are
preserved. In particular, an exported setter can still return
`BadWriteNotSupported` for an immutable attribute such as BrowseName.

`cpkt_opcua_<Attributes>_default` makes an owned C89 copy of the actual native
defaults. Outputs must start empty. On conversion failure after node creation,
the new node is deleted using its native identifier. Tests exercise every node
class, 64-bit arrays, attribute changes, browse paths and error outputs.

### Typed server timers and reference iteration

`cpkt_opcua_server_addTimedCallback_typed` and
`cpkt_opcua_server_addRepeatedCallback_typed` install callbacks directly into
the native server EventLoop. DateTime and callback IDs use the generated
two-word C89 representation; all 64 bits are preserved. There is no facade
scheduler or timer queue. Native interval changes, ordering, cancellation and
missed-execution behavior apply.
Changing a one-shot timer's interval makes it repeated, including a change
from inside its callback. Its context then remains borrowed until removal or
server destruction.

Callback context remains caller-owned. A one-shot timer borrows it until its
callback returns; a repeated timer borrows it until removal or server
destruction. A callback may remove itself or another timer. Calls on one handle
must be serialized, and the server must not be destroyed from its callback.
Facade bookkeeping survives cancellation of a running callback. Destruction
removes outstanding native timers before releasing their callback contexts,
including when an externally owned EventLoop outlives the server. The facade
does not release the caller's context.

`cpkt_opcua_server_forEachChildNodeCall_typed` invokes the native synchronous
reference iterator. Each callback borrows converted NodeIds until return.
Nonzero callback statuses and conversion failures stop native traversal with
that status. Native browse allocation remains upstream behavior; the facade
does not gather a second collection of references. Static/shared and
allocation-failure tests cover past/future deadlines, optional IDs, interval
changes, reentrant cancellation, shutdown cleanup, inverse references and
early termination.

### Subscription binding coverage

The public coverage contract recognizes the complete specialized client
subscription and batch monitored-item interfaces already implemented by the
C89 facade. This includes synchronous/asynchronous creation, subscription and
item context access/replacement, native single deletion, and the generated
modify/delete/publishing/monitoring/triggering service boundaries. Creation
uses the native specialized functions, retaining their local subscription and
monitored-item state rather than sending an equivalent raw service request.

All native status, data, event, deletion and specialized completion callbacks
retain their public data and original application contexts through C89 types.
The facade adds explicit conversion status to distinguish representation
failure from native service and per-operation status. Native data-change/event
batch creation share a typed entry point with an explicit event selector;
no event fields or nested values are discarded. Single-item creation helpers,
attribute callbacks and other client helpers remain separate pending entries.
Existing native-peer, integration, allocation-failure and destruction tests
exercise these bindings; coverage classification does not add new behavior.

### Value-source and method producers

The generated `cpkt_opcua_CallbackValueSource` and `cpkt_opcua_MethodCallback`
retain the full upstream signatures with C89 records. Install copied source
slots with `cpkt_opcua_server_setVariableNode_callbackValueSource_typed` and
method callbacks with `cpkt_opcua_server_setMethodNodeCallback_typed`. Original
node, method, object and session contexts pass through unchanged. Get returns
the original typed method pointer; callbacks installed outside this full C89
interface return BadNotSupported rather than an ABI-incompatible cast.
Callback replacement is safe inside a running callback. Callback NodeIds,
ranges and input arrays are borrowed until return. Do not retain or clear them.
Read/method outputs own C89 allocations; write input values remain borrowed.

Returning GoodCompletesAsynchronously keeps the original C89 read/write value
address or method output-array address alive. Use the corresponding
`setAsyncReadResult_typed`, `setAsyncWriteResult_typed` or
`setAsyncCallMethodResult_typed` after returning. Native open62541 owns the
actual operation queue, completion scheduling, timeouts and cancellation.
The facade owns representation/identity metadata only. A Good completion
invalidates the C89 address immediately. Conversion failure during completion
preserves it for retry. Method output count is fixed, including zero-output
methods whose addresses remain distinct. Do not complete an active callback,
resize the output array, or access it after cancellation/completion.

Set `cpkt_opcua_server_set_async_operation_cancel_callback_typed` before
startup. It receives the producer's original C89 address until callback return;
required cleanup still runs with a NULL hook. An existing native cancellation
hook also receives its original native address. Initial async output conversion
failure invokes cancellation before releasing the C89 address and reports that
conversion status instead of queuing the operation. Do not clear/complete the
cancelled output or recursively cancel the same operation/context. Destruction
already cancels all native work; facade cancel requests during destruction are
no-ops. All calls on a server must be serialized, and callbacks must not destroy
that server.

Open62541 adds/filters timestamps after read producers return. Typed local
submissions and `server_iterate` synchronize this metadata once into newly
pending C89 results before returning to their caller. Later application edits
are retained. Native escape-hatch event-loop callers can explicitly invoke
`cpkt_opcua_server_refresh_async_producer_metadata` after native processing.
Partial async outputs are converted before upstream continues, retaining the
native cancellation/result behavior for values prepared before deferral.

For direct native borrowing, use `valueSourceBorrow_typed` for a read or
`methodResultBorrow_typed` for a selected method slot. Both borrow the stable
native storage already provided by `cpkt_opcua_history_value`; its name reflects
its first history-backend use, but the same holder supports these producers.
The bridge passes native payload/dimension addresses with NODELETE and makes
no payload copy. Normal C89 outputs require representation conversion. The
holder must remain unchanged and alive through every native use, including
encoding and local completion callbacks; quiesce all borrowers before set/free.
Upstream controls any subsequent copying: this open62541 version copies a
synchronous borrowed read value inside its native read implementation. The
facade preserves that mechanism. NULL holder resumes normal C89 conversion.

Native-peer and C89 tests cover full inputs/contexts/ranges, exact signed 64-bit
outputs, synchronous/deferred/error results, timestamp synchronization,
reentrant callback replacement, cancellation/shutdown, partial preparation
failure, every allocation during registration/conversion/completion, zero-output
identities, and native borrowing without a facade payload clone. External
double-pointer value sources remain tracked separately in the coverage contract.

### Native node creation

The complete typed callback-source variable and method creation interfaces are
available: `addCallbackValueSourceVariableNode_typed`, `addMethodNode_typed`,
`addMethodNodeEx_typed`, `addMethodNode_finish_typed`, `addNode_begin_typed` and
`addNode_finish_typed`. They call the corresponding native public function;
the facade does not substitute its own creation or begin/finish algorithm.
All attributes and Argument arrays use generated C89 types, including exact
64-bit/nested values. The generic begin function takes a C89 Type descriptor
for its attribute record. Requested/assigned method argument IDs and optional
output IDs retain the public native behavior.

Scoped dispatch metadata follows the actual native out-ID address, populated
before constructors/value-source callbacks run. Automatically assigned IDs,
recursive creation, original contexts, and reentrant callback replacement work
during the native call. Replacement wins over the original staged registration.
After return, live nodes retain their dispatch record without another metadata
allocation. Native failure may leave an assigned callback-source node alive;
the facade preserves its ID and callbacks rather than silently changing that
native rollback behavior. Output IDs own storage and must be cleared even on
failure. Output conversion failure after successful native creation clears all
C89 outputs and rolls back the new node; method argument properties are removed
before their method. Existing native failures use upstream's own cleanup rules.

C89/native tests cover all six functions, constructors that immediately read
or call newly created nodes, automatic and string IDs, explicit argument IDs,
optional outputs, nested creation, reentrant replacement, rejected constructors,
independent native failure parity, dynamic attribute validation, every
allocation-failure position, and failed output conversion/property rollback.

### Configuration key/value maps

The full native `KeyValueMap` operation surface is available through
`cpkt_opcua_KeyValueMap_*` in `<cpkt/opcua_util.h>`. Maps retain the public
`mapSize`/generated `KeyValuePair` fields and linear, namespace-aware,
byte-exact key lookup. NULL const maps are empty. `get` and `getScalar` return
borrowed pointers into actual C89 storage without conversion or a temporary
native cache. An insertion can reallocate the array; removal moves its last
entry into the deleted slot. Existing borrowed views must respect these
native invalidation rules.

`set` and `setScalar` deep-copy inputs. `setShallow` and
`setScalarShallow` copy the key and borrow the original value payload, forcing
`DATA_NODELETE` exactly as the upstream implementation does. Caller-owned
payloads and array dimensions must remain alive until the map stops using them;
clearing the map does not release them. Do not pass a map-owned value as the
source of shallow replacement. Deep replacement may use the existing value
as its source. Notification maps remain borrowed and must not be cleared.
`copy` needs an empty destination. `merge` stages a complete replacement and
preserves both maps on allocation failure, including self-merge. Removal still
succeeds when shrinking its allocation fails, preserving native behavior.

The representation-specific implementation in `src/opcua_map_impl.h` adapts
open62541's handwritten map operations to C89 record layouts, preserving its
MPL-2.0 copyright/license notice. It does not duplicate schema definitions.
Native peer tests compare statuses, shallow pointer identity, lookup type
checks, key replacement/removal order and empty-array distinctions. C89 tests
also cover exact signed 64-bit values, namespace and binary keys, alias-safe
deep overwrite, self-merge, cleanup, and failure at every allocation in deep
insertion, replacement, shallow insertion, copying and merging.

### Generated public plugins

Handwritten plugin records have no upstream schema generator. The maintained
plugin emitter derives their complete declarations and typed trampolines from
the actual installed public headers. Unknown fields, callback signatures and
conditionals fail generation rather than silently dropping public slots.

`cpkt_opcua_server_set_access_control_plugin` installs the complete enabled
AccessControl record before startup. Callback slots and token policies are
copied; original caller context transfers only on successful installation.
Callbacks borrow a facade-owned policy copy and converted schema inputs.
`clear` releases caller context only, never policy arrays. Failed authorization
conversion denies the operation. A failed closeSession conversion still calls
closeSession with NULL sessionId and the original sessionContext, and logs the
failure, allowing the application to release its session resources.

`cpkt_opcua_server_set_history_database_plugin` exposes all ten history operations
and clear. Mutable response/result records own their C89 allocations and are
converted back after the callback. Allocate output records with typed new/array
helpers. History-data pointers alias the payloads inside the converted response,
matching native callback semantics; the temporary pointer array is borrowed.
Native result updates are staged so a failed output conversion leaves the
original native storage intact and reports the failure through its status.
Void notification failures go to the configured logger. No file staging or
additional history queue is introduced.

Replacing a plugin stages all allocations before clearing the installed plugin.
Failure preserves the previous plugin and leaves the new context with its
caller. Static/shared C89 tests invoke every enabled callback slot across the
native boundary. Allocation-failure tests cover staging, partial nested arrays,
closeSession cleanup and history-result conversion. Generated plugin records,
constants and all generator inputs are included in package/source checks.

## History storage and borrowed values

The generated `cpkt_opcua_HistoryDataBackend` exposes every public upstream
storage callback. The authoritative inputs are upstream's handwritten public
history headers; upstream has no generator for those records. Their callback
signatures, timestamp match enum, collection strategy, per-node settings and
numeric-range declarations are derived and checked during generation. Unknown
fields or unsupported signatures fail generation.

A backend is the application's historical-value storage provider. It is separate
from the higher-level `HistoryDatabase` plugin that handles history services.
Use `cpkt_opcua_server_set_default_history_database(server, initial_capacity)`
to install upstream's default gathering/database, then
`cpkt_opcua_server_register_history_backend(server, &node, &settings)` for each
historized node. Set the node's Historizing and HistoryRead/HistoryWrite access
attributes as needed; registration does not change them. Capacity and maximum
response size must be nonzero. The default gathering can grow during registration.

The settings/backend records are copied. On successful registration, backend
context ownership transfers to its `deleteMembers` callback. Failed or duplicate
registration leaves context ownership with the caller and preserves existing
registrations. Each successful registration has its own cleanup callback; shared
contexts need application-managed reference counting. `userContext` is borrowed.
Replacement of the database or server destruction stops polling, destroys native
gathering state, then invokes each backend's `deleteMembers` once. That callback
can free its persistent values. No callback inputs may be retained without an
explicit copy.

Choose the native `getHistoryData` high-level callback or the complete low-level
read interface. Both preserve the upstream mechanism. History callbacks use C89
schema records, including two-word Int64/DateTime on 32-bit targets. Mutable
outputs own C89 allocations and are converted into staged native outputs, then
cleared by the facade. `copyDataValues` receives an empty array of `valueSize`
records and must report no more than that many values. Excess counts are rejected
before native output is changed. Numeric-range dimensions borrow until return.
These are upstream's materialized history service outputs, with no facade queue,
file spool, or change to the storage mechanism.

### Persistent values for `getDataValue`

Native `getDataValue` returns a **borrowed native DataValue pointer**. A converted
stack record, temporary allocation freed at callback return, or one shared slot
replaced by the next callback cannot meet that contract. The C89 callback instead
returns a backend-owned `const cpkt_opcua_history_value *`. The facade forwards
the stable native address embedded in that object directly to open62541. It does
not convert/cache the returned value per call or release it after the callback.
The input NodeIds borrow their byte payloads, so this pointer-return trampoline
performs no allocation.

Create stored objects with `cpkt_opcua_history_value_new(&value, &stored)`.
Creation deep-copies the ordinary C89 DataValue; the original can be cleared or
changed immediately. Distinct retained values need distinct objects. Returning
another object must leave earlier borrowed values valid. For a valid index,
return a valid stored value as required by the native default history engine;
the facade preserves NULL, but upstream may dereference it.

`cpkt_opcua_history_value_set(stored, &value)` stages a deep copy and preserves
the old value on failure. The object address remains stable, but **all borrowers
must finish before set or free**: replacing it invalidates the old nested data.
There is no automatic reference counting, locking or detection of outstanding
native borrowers. Follow the upstream server synchronization rules and keep
storage unchanged throughout a history operation. `history_value_get` produces
an independent owned C89 copy into an empty output; clear it with
`cpkt_opcua_DataValue_clear`. Neither get nor set exposes a native pointer.
`history_value_free(NULL)` is safe. Ownership comments accompany these APIs and
`getDataValue` in the generated public header for clangd.

The storage object and borrowed-pointer bridge require no upstream ABI change.
The bundle includes a small upstream allocation-safety patch: default history
constructors report empty records on allocation failure, gathering growth keeps
existing storage on realloc failure, and NodeId-copy failures are propagated. Parsed numeric ranges are released
after either backend read path, including failure.
The facade turns empty constructors into BADOUTOFMEMORY and retains the prior
installed database. These guards do not change native history behavior on success.

### Collection policy and polling

USER leaves value insertion to the application. VALUESET forwards native write
notifications to `serverSetHistoryData`. POLL uses native local monitored items;
start/stop with `cpkt_opcua_server_history_start_poll` and `_stop_poll`.
Register all nodes **before starting any polling**. Later registration is rejected
because growing upstream's gathering array would move monitored-item contexts.
Database replacement/destruction stops polling automatically. Application code
must still obey upstream restrictions on destruction/reconfiguration from inside
callbacks. Custom `HistoryDataGathering` vtables and bindings for the stock memory/circular
backend factories remain separate coverage work.

Static/shared C89 tests exercise low-level reads, bounds, numeric ranges, Int64
values, high-level continuation points, all write/update/delete hooks, duplicate
registration and cleanup through the native default history engine. Allocation
failure tests cover constructors, registration/growth, persistent new/get/set,
old-value preservation and old-database preservation. Polling tests cover
start/stop, registration closure, replacement and server destruction with active
polling. Native attribute-read allocation failures retain upstream
fail-closed access/history checks. Valgrind verifies range cleanup.

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
  - native hooks for custom security policies and certificate groups;
  - full generated C89 access-control callbacks;
  - convenience helpers for bundled default policies only.
- History:
  - client history-read wrappers for common raw value reads;
  - full generated C89 HistoryDatabase callbacks;
  - full generated C89 HistoryDataBackend callbacks and native default gathering;
  - custom gathering callbacks and stock backend factories remain pending.
- Async services:
  - selected async read, write, browse, call, and add-node wrappers with C89
    callbacks;
  - generic and generated typed async client service bindings;
  - typed server-local read/write/call submissions and cancellation;
  - native hooks remain for asynchronous value-source completion tokens.
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

- Convenience functions return `cpkt_opcua_result`. Generated typed boundaries
  return exact `cpkt_opcua_StatusCode`; service/result statuses remain in their
  generated response records, separate from conversion status.
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

### Server-local asynchronous operation ownership

`cpkt_opcua_server_read_async_typed`, `write_async_typed`, and
`call_async_typed` submit complete C89 records to the native server. Requests
are borrowed only during submission. The caller retains its callback context
until completion, which can occur before submission returns, during an EventLoop
iteration, cancellation, or server deletion. Accepted operations complete once;
failed submissions invoke no completion. Native timeouts use milliseconds,
with zero meaning infinite. Serialize operations on one server handle.

Read and method callbacks receive a separately reported conversion status;
conversion failure produces a NULL result. Successful conversion preserves
native operation errors in the DataValue or CallMethodResult. Callback records
are borrowed until return; use the generated copy helpers to retain them.
The facade owns only callback dispatch metadata, and adds no response queue.

`cpkt_opcua_server_cancelAsync_typed` matches the original caller context,
including NULL, and forwards cancellation to native operations using that
context outside the typed facade. It pins the existing dispatch records before
calling upstream, allowing reentrant callbacks without invalidating traversal.
Operations submitted by a callback require their own cancellation. Immediate
cancellation completes matching callbacks before return; otherwise drive the
EventLoop before freeing their context. Already-ready results retain their
native status. Server deletion completes pending operations with BadShutdown
and rejects new typed submissions with BadShutdown. Do not destroy the server
from one of its callbacks.

These submissions do not yet expose the producer-side value-source and method
completion tokens. Those require a distinct lifetime binding for upstream's
stable callback output addresses and remain tracked as pending public API.

### Public value utilities

`<cpkt/opcua_util.h>` is also included by the generated types header. It exposes
native Variant predicates, ownership setters and copy setters, decoded
ExtensionObject ownership/type helpers, status predicates/names, sized-string
allocation/append, native base64/hash/secure-zero helpers, and array
resize/move-append/copy-append. Predicates pass borrowed native views without
allocating or interpreting the payload. Ownership setters transfer C89 storage
without duplicating it. Clear old destination contents before setters: native
setters initialize their destinations rather than freeing previous contents.
Scalar-copy failure leaves the destination unchanged; array-copy and
ExtensionObject-copy failure leave initialized empty destinations.

Array resizing uses the upstream allocation procedure with C89 element sizes
and clear helpers. Growth initializes the new elements. Shrinking stages the
removed elements' shallow representations before realloc, freeing their nested
storage only after success. Failure leaves the original array, length, and
owned elements intact. Move-append empties the source only on success; its
source must not alias the array. Copy-append can copy an existing array element
and retains upstream's 512-byte native-type-size limit. Zero-length no-op resize
preserves NULL versus empty sentinel; shrinking to zero produces the sentinel.

`cpkt_opcua_type_order` converts complete records and invokes native `UA_order`.
It reports conversion/allocation errors separately from the order value; ignore
the order output unless status is Good. The existing Boolean equality helper
still returns false for conversion/allocation errors, as its header documents.
String buffers use the configured native allocator. Append destinations must
own their buffer and sources must not alias it. Base64 codecs preserve native
empty-message and error behavior, including the upstream decoder's
BadInternalError for allocation failure. These codecs materialize one value,
like their native APIs, and make no streaming claim.

Independent C99 peers verify Variant and ExtensionObject predicate combinations,
status severities, and raw-byte hashing. C89 tests verify exact signed 64-bit
ordering, ownership transfer, nested deep copies, array-element aliasing in
copy-append, shrink/growth, base64 byte fidelity, secure zeroing, invalid inputs,
and injected allocation failures preserving the documented state.

### Identifiers, namespace mappings and time

The utility header exposes full NodeId, ExpandedNodeId, QualifiedName and Guid
parse/print operations, including extended namespace/server URI forms. Native
parsers own their result payloads; supply an empty destination and clear it after
use, even after a failed parse. The facade transfers native output ownership
without cloning strings. Unknown URI handling, escaping, hashing, predicates and
ordering use native implementations. NULL query values represent initialized
empty values. Namespace/server URI views marshal record arrays while borrowing
all byte buffers, and may report allocation failure.

An empty print output requests native allocation and owns the result. A nonempty
output supplies a writable buffer with length as capacity. Native length changes
and error statuses are retained, including insufficient-capacity failures. Do
not clear stack or borrowed output buffers. NamespaceMapping URI lookup returns
borrowed bytes, which remain valid only while the mapping storage does; do not
clear the lookup result. A mapping may borrow arrays for lookups, but clear/delete
require ownership of all populated arrays and their URI payloads.

DateTime parsing and calendar/Unix conversions delegate to upstream. Signed
64-bit timestamps and Unix seconds retain their complete high/low bit patterns
on 32-bit targets. Keep arithmetic within upstream's representable range; no
saturation or replacement calendar implementation is introduced. Monotonic
values are for durations. Native random functions retain per-thread state and
all 64 seed bits; UInt32_random is not cryptographic entropy.

Independent native peers compare parse statuses, allocated and preallocated
printing, binary encodings, hashes, predicates, ordering, calendar fields, Unix
seconds, local offsets and seeded GUID/random values. C89 tests cover namespace
translation, borrowed URI identity, owned mapping cleanup and injected allocation
failures. A tracked native date-parser patch fixes overflow-guard reversal at
the Unix epoch and the next second, verified by exact timestamp regressions.

### Native node value notifications and lifecycle callbacks

The generated plugin header derives the complete ValueSourceNotifications,
NodeTypeLifecycle and GlobalNodeLifecycle records from the installed native
server header. Unsupported or changed callback declarations fail generation.
Node/session/type contexts remain exactly the native caller's contexts; mutable
context pointers pass through directly. NodeIds are allocation-free borrowed
views, so destructor dispatch does not fail because of conversion allocation.
Never clear or retain callback NodeIds. Numeric ranges and notification values
are borrowed C89 copies valid only until return; copy explicitly to retain them.
Failed value/range conversion is logged through the configured server logger and
skips a void notification, which has no native error return channel.

The internal value-source setter forwards NULL values directly to the native
source-switching implementation; NULL notifications disable both hooks. It stages complete callback records and value conversions before native
installation. Failed installation retains the previous dispatch record. Node-type
callback replacement follows the same staged ownership and can occur reentrantly
inside a callback. The global lifecycle setter copies all four slots before
startup; NULL disables its native pointer. generateChildNodeId produces an owned
C89 NodeId that is converted to native ownership and then cleared by the bridge.
Application contexts remain caller-owned throughout.

These native records have no callback userdata slot. Private dispatch metadata is
indexed by server and node, preserving all native context fields. A mutex protects
cross-server registry lookup and never covers application callbacks. Operations
on each server must remain serialized, and the server must not be freed from a
callback. An active callback pins its old dispatch record across replacement.
Metadata is retained until replacement or server destruction; native node/type
and global destructors run before the registry is released. No value source or
node lifecycle operation is reimplemented, and no native callback queue is added.

Independent C99 peers invoke all eight notification/lifecycle slots and perform
real native read, write, instance creation and deletion. Strict C89 tests cover
complete 64-bit values/timestamps, numeric ranges, original and mutable contexts,
two independent servers sharing NodeIds (including concurrent native read/write
dispatch), reentrant replacement, NULL slots,
unknown/wrong node types, owned child IDs, allocation failures preserving prior
bindings and logger reporting. Producer-side async value/method callbacks remain
separate pending interfaces.
