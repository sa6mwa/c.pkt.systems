# Public API And Implementation Intent

## C API Shape

Design pkt.systems C libraries as small handle-oriented systems with explicit ownership, predictable naming, and examples that read like the intended public usage.

Public API contract:

- Install public headers under `include/<project>/` plus an umbrella header when useful.
- Keep implementation headers under `src/` or another private path. Do not install private headers.
- Choose the public handle shape deliberately. Use opaque typedefs plus free functions when ABI opacity is the dominant concern. Use public receiver shells when DX is the dominant concern: an installed `struct project_type` contains function pointers for receiver methods and a private `impl` pointer for state.
- Receiver-shell function-pointer fields are public API. Their names, signatures, and initialization guarantees require the same care as ordinary exported functions.
- Keep mutable implementation state behind the private `impl` pointer. If any non-method data field is exposed in an installed handle struct, treat it as source API and ABI-relevant for shared libraries even when comments say it is private.
- Public value/config structs may be transparent when they are stable input/output records. Treat field names, field order, defaults, and zero-initialization behavior as API.
- Prefer zero-initializable config structs with documented defaults. Avoid mandatory initializer functions unless the config has invariants that cannot be represented by zero plus explicit fields.
- Return a project status enum or integer status for fallible operations. Use out-parameters for produced handles, buffers, counts, and records, and set those out-parameters to a safe value on failure when practical.
- Put detailed error data in an explicit project error object or callback-owned diagnostic surface. Error messages must be actionable and ownership must be documented.
- Use `destroy`, `close`, or `free` verbs consistently. Cleanup functions must tolerate `NULL` when practical and must document whether they are idempotent.
- Provide one project-owned string cleanup function when public APIs return allocated strings. Do not require consumers to know which allocator produced a public string.
- Constructor/open functions own creation and validation. Receiver functions operate on an existing handle and should not perform surprising global initialization.
- Avoid hidden global state. Environment and default-path helpers are allowed only when explicit in the API name and tests cover precedence.
- Public headers must be C89-compatible unless the project has selected a newer standard, and must be compatible with C++ consumers when the project claims that.
- Public symbols, public struct fields, callback types, macros, and receiver method fields should have concise documentation comments in the same change that introduces them. Documentation should describe ownership, defaults, side effects, and error behavior, not repeat the symbol name.

Receiver-style API rule:

- Prefer receiver-style method calls for handle operations. For receiver shells, this means `handle->verb(handle, ...)`. For free-function compatibility, the equivalent exported function is `project_type_verb(project_type *self, ...)`.
- The receiver handle is the first parameter in both method-pointer signatures and free-function compatibility wrappers. Use `const project_type *self` for read-only operations.
- Keep free functions as public surface for constructors, pure helpers, factory helpers, registration helpers, serialization helpers, compatibility wrappers, and operations that genuinely do not belong to one receiver.
- If both receiver-style and legacy/free-function handle operations exist, examples and documentation must prefer the receiver-style form.
- Add an executable API-style check when a project has a preferred public usage style. For example, scan examples and documentation snippets for discouraged handle-operation forms and fail with a clear message. Keep the allowlist explicit so public compatibility functions remain available but do not become the documented happy path by accident.
- Do not rename or remove public functions silently. Naming cleanups that affect public users require engineer discussion and compatibility strategy.
- Raw protocol/builders may use free functions or receiver methods according to clarity. High-level application workflows should prefer receiver-style handles because they keep multi-object flows readable.

Naming:

- Use one project prefix for all public C symbols, CMake options, CMake targets, pkg-config variables, environment variables, and Make/service variables.
- Use `project_type_verb` for receiver functions, `project_verb_type` only when the operation is clearly a factory or cross-type helper, and `project_constant_name` for macros.
- Name buffered, spooled, source-backed, and streaming APIs precisely. Do not call an API streaming unless bytes or records flow from producer to consumer without full-message materialization.
- Name configuration helpers by the side effect they perform, such as `config_use_provider`, `load_dotenv_key`, or `auth_default_path`. Do not hide environment, filesystem, or credential reads inside generic constructors.

## Implementation Boundaries

Structure implementation so product behavior, dependencies, transport, parsing, and packaging remain independently testable.

Recommended source split:

- `<project>_client.c` or equivalent owns transport-facing client construction and defaults.
- `<project>_http.c`, `<project>_transport.c`, or equivalent owns network protocol exchange.
- `<project>_response.c`, `<project>_stream.c`, and similar files own parsing and lifecycle of protocol objects.
- `<project>_source.c` and `<project>_sink.c` own streaming input/output adapters.
- `<project>_tool.c`, `<project>_registry.c`, or equivalent owns user callback registration and invocation.
- `<project>_error.c` owns status-to-message and detailed error cleanup.
- `<project>_allocator.c` owns custom allocator bridging when the project supports it.
- A project-prefixed version CMake module or equivalent owns version detection and generated version headers. Do not duplicate version inference across Make, CMake, Lua packaging, and release scripts.
- `tools/` or another subdirectory contains optional built-in tools or extension surfaces.
- `src/<project>_internal.h` is private glue only. Keep it out of installed headers and release SDK includes.

Dependency boundaries:

- `lonejson` owns JSON parsing, serialization, validation, streaming, framing, escaping, and fixture normalization. Do not write bespoke JSON logic in consuming projects.
- Logging belongs behind the project logging adapter and the selected logging dependency. Public APIs should not force downstream users to accept hidden logging side effects.
- Transport dependencies, SDK bundle roots, and host dependency probes stay below the public API. Public C headers must not expose dependency cache paths or private build details.
- If a host dependency mode exists, probe ABI-sensitive dependencies and fail with actionable diagnostics on missing or wrong-ABI libraries. Auto modes must fall back conservatively rather than accepting partial host installs.
- Downstream-facing CMake config and pkg-config files must preserve dependency intent: public requirements, private static-link requirements, minimum versions, ABI generation, and official package URL/checksum hints when a dependency is expected to come from a pkt.systems SDK.

Streaming and spooling:

- Streaming APIs must stream for real. They may use bounded parser, serializer, transport, and OS buffers, but must not materialize the whole message behind a streaming-looking API.
- File-backed or memory-backed spill behavior is acceptable only when explicitly named and documented as spooled or file-backed.
- Source/sink callback APIs must define ownership, retry behavior, EOF, close semantics, partial read/write behavior, and error propagation.
- Source/sink adapters should support callback-backed streams, file-backed streams where appropriate, and interop wrappers for dependency-native source/sink types when the dependency is part of the public integration contract.
- APIs that open a streaming source from a response, output, history, state, or generated value must document whether the producer is live, spooled, memory-backed, or file-backed, and who closes the source.
- Add tests for chunk boundaries, fragmented frames, midstream errors, large fields, cancellation, callback failures, and cleanup after partial writes.

Security and local side effects:

- APIs that execute commands, read files, write files, use credentials, or call external services must require explicit configuration and must have tests for denied paths, host-path leakage, size limits, and failure reporting.
- Dotenv helpers, auth-file helpers, browser openers, command execution helpers, file readers, and clipboard or local-machine integrations are explicit opt-in surfaces. They must not be triggered by generic client/session construction.
- Browser or command opener helpers must avoid shell invocation. Pass user-controlled values as argv values and test command construction.
- Integration tests may use real credentials only behind explicit opt-in environment variables and must print the selected provider/model/endpoint without printing secrets.

## Facade Layering

When a library has low-level protocol builders and high-level workflow helpers, expose both deliberately:

- Low-level builders model the wire/API surface closely and are useful for tests, exact request construction, and advanced users.
- High-level facades own defaults, sessions, tool registration, continuity, auto-run loops, limits, and common application workflows.
- High-level facades may internally use low-level builders, but low-level builders must not depend on high-level facade state.
- Provide tests that prove high-level defaults produce the intended low-level request shape.
- Keep provider-specific escape hatches explicit and named as raw JSON, hosted tool JSON, provider metadata, or equivalent. Validate raw JSON through the JSON dependency before sending it.
- Budget, usage, context-window, retry, and continuity behavior must be observable through public APIs and tests, not inferred from logs.

## Generated Knowledge Surfaces

When a project ships generated or curated tables such as model metadata, pricing, protocol versions, default endpoints, tool schemas, or capability maps:

- Keep the generated/curated data in a source-controlled documentation or source file with clear ownership.
- Expose stable query helpers instead of making consumers parse documentation.
- Mark incomplete, inferred, deprecated, provider-specific, or externally verified entries explicitly.
- Fail closed when safety or spend accounting depends on missing metadata.
- Add tests for default selection, capability flags, pricing/budget behavior, and stale or unknown entries.

## Examples As Contract

Examples are executable API documentation.

Rules:

- Build examples by default when dependencies are available.
- Include deterministic local examples for each major public workflow.
- Keep live-provider examples non-interactive and opt-in.
- Example help/version smoke tests should run in local confidence gates.
- Installed examples, when shipped, must build against the installed SDK rather than the source tree.
- Examples must use the preferred API style and must not include private headers, source-tree-relative dependency paths, local credentials, or test-only symbols.
- Example Makefiles may exist as a secondary developer surface, but root `make help` remains the lifecycle authority. Example commands must build through the standard presets and avoid hidden dependency roots.

## API Verification Checklist

Add executable checks for:

- Every public header compiles standalone.
- A C-only consumer builds with the selected C standard.
- A CMake `find_package` consumer builds from an installed tree.
- A pkg-config consumer builds when a `.pc` file is shipped.
- Public examples build and smoke-test.
- Receiver-style or other project-specific API-style rules are enforced.
- Error paths preserve diagnostics and cleanup ownership.
- Custom allocators are honored and do not cross allocator ownership boundaries.
- Explicit environment, dotenv, auth-file, browser-opener, local-file, and command-execution helpers do not run implicitly.
- High-level facade behavior matches low-level request/response contracts.
- Version macros, package metadata, CMake config version, pkg-config version, Lua rockspec version, and source archive `VERSION` agree.
- Source archives build and test from an extracted tree without repository metadata.
