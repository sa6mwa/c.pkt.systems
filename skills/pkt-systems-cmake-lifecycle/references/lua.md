# Lua Facades

## Lua Facades

Enable Lua facade support when the project ships Lua bindings, a Lua C module, Lua runner behavior, or Lua release artifacts.

Repository contract:

```text
lua/
<project>.rockspec.in
scripts/build_lua_rock.sh
scripts/render_release_rockspec.sh
scripts/stage_lua_rock_sources.sh
```

Build contract:

- `make lua-rock` renders a development rockspec and installs the Lua module into a repo-local tree under `build/luarocks`.
- `make lua-test` runs Lua smoke tests against the repo-local installed rock and the repository's supported system Lua runtime.
- `make lua-env` prints shell exports for running examples against the repo-local rock and installed C SDK prefix.
- Do not assume support for old Lua versions or alternate JIT Lua runtimes. pkt.systems Lua artifacts target the system Lua runtime available for the project; currently expect Lua 5.5 unless the engineer explicitly expands the support matrix.
- Missing non-target Lua runtimes are not failures because they are not part of the support contract.
- `scripts/build_lua_rock.sh` accepts the LuaRocks build arguments: compiler, flags, shared-library flag, object extension, library extension, and Lua include directory.
- If the Lua module links an installed C SDK, discover it through pkg-config first, then a project-prefixed prefix variable, and fail with an actionable message if neither is available.
- If optional C dependencies can be enabled, probe them at build time and provide force/disable environment variables.
- On Linux, allow undefined symbols in Lua module shared objects only when that is required by Lua module loading.
- Optimize the C-to-Lua boundary deliberately. Prefer APIs that cross the boundary once per batch, stream, buffer, record set, or operation group instead of once per item when throughput matters.
- Lua facade tests and benchmarks should cover both correctness and boundary-crossing shape for hot paths.
- If the Lua facade consumes a released C SDK, test it against the installed SDK layout, not only against the source-tree build.
- For local development, install the just-built C SDK into a repo-local prefix and build the Lua module against that installed layout. This catches C package metadata problems before release.
- Lua package-manager build directories and locks are generated state and belong under `build/` or another ignored generated directory.

Facade contract:

- The Lua facade should mirror the main public workflows: client, agent, session, response/output handles, streaming sinks/readers, tool registries, tool presets, auth/login helpers, and protocol handlers when those exist.
- Omit C-only embedding surfaces that do not map cleanly to Lua, such as custom allocators, `FILE *`, raw C source/sink constructors, C map definitions, or callback types that would force unsafe lifetime rules.
- Lua large-value APIs should expose spooled or callback-backed variants that avoid materializing full payloads when the C API can stream or spool.
- Lua errors should expose structured status, status string, HTTP status when relevant, message, detail, server code, and request id when the C error surface has them.
- Lua tests should cover method-call DX, ownership/finalizer cleanup, spooled readers/writers, and parity for major C workflows.

Lua/C embedder interop contract:

- Do not require every Lua facade to expose C embedder interop. Add it only when C embedders need to borrow Lua-created objects for a real workflow.
- Keep the core C SDK Lua-agnostic. Core public headers must not include `<lua.h>`, forward-declare `lua_State`, expose Lua userdata names, declare `project_lua_*` functions, or require Lua development headers.
- If embedders need a stable borrowed handle to a Lua-created core concept, expose a Lua-free generic view in the core C API only when that view is useful beyond Lua. Use ABI-stamped structs with `size` and `abi_version`, and fail closed on mismatch.
- Put Lua-specific interop in a separate boundary, normally `include/<project>_lua.h` or another clearly named Lua interop header. This header may include `<lua.h>` and declare functions that take `lua_State *`.
- The Lua C module owns userdata validation and conversion from Lua objects to generic core views. Downstream embedders must call the interop functions instead of mirroring private userdata structs.
- Lua-owned schema, route, policy, session, record, or similar objects remain owned by Lua and are valid only on their owning `lua_State`.
- Document that a C consumer retaining a Lua-owned object beyond the current stack frame must keep a Lua registry reference to the userdata, and must not free, resize, mutate, or retain raw record pointers past the documented lifetime.
- Adapter functions should return project status/error values for wrong stack values, wrong userdata, ABI mismatch, ownership mismatch, schema/record mismatch, allocation failure, parse failure, and conversion failure. Expected validation failures must not call Lua argument-error helpers that `longjmp`.
- Interop parse/serialize helpers must preserve true streaming. Reader-backed helpers call the core reader-backed parse API; writer-backed helpers feed the core writer/generator callbacks. Do not convert a Lua table to a full string, accumulate a full HTTP body, or concatenate all chunks and call that streaming.
- If a buffered, materialized, spooled, or file-backed interop helper is useful, name and document it as such.

Release contract:

- `make release-lua-artifacts` writes all Lua release artifacts under `dist/`.
- Render a release rockspec with the exact release version.
- Treat development rockspecs and release rockspecs as different outputs when local development needs repo-local sources. A development rockspec may point at generated local state under `build/`, but no rendered release rockspec, source rock, checksum-listed Lua artifact, or nested source archive may contain a repo-local or `$HOME` source URL.
- Stage a minimal Lua source tree with `LICENSE`, `README.md`, injected source-package `VERSION`, `RELEASE_MANIFEST`, the rockspec template, build/render scripts, Lua sources, C facade sources, docs needed by Lua users, and required public C headers.
- Stage Lua interop headers and implementation fragments required to build the Lua C module, including `<project>_lua.h` when the Lua facade ships C embedder interop.
- Produce a standalone Lua source package under `dist/` named `dist/<project>-lua-<version>.tar.gz`. This package is separate from the C source archive and from per-target C SDK tarballs.
- Build the release `.src.rock` through LuaRocks from a staged Lua source package, not from the live worktree. The source rock must contain the rendered rockspec and the same `<project>-lua-<version>.tar.gz` payload, with the rockspec `source.url` rewritten to a release-local or public source URL and `source.dir` set to the staged root such as `<project>-<version>` when needed.
- Keep the rendered release rockspec under `dist/<project>-<version>-1.rockspec` when useful for inspection and checksum coverage.
- Include the standalone Lua source package, rendered release rockspec, and `.src.rock` in the checksum manifest alongside C release artifacts.
- Render release rockspec source URLs as release-appropriate logical or public URLs, such as the final release source archive URL or another deliberately public source location. Do not use absolute `file://` URLs pointing at the repository, `$HOME`, package-manager temporary directories, or staged local source trees.
- Verify rendered release rockspecs, standalone Lua source packages, and source rock contents for private paths, generated state, credentials, absolute local `file://` URLs, and missing source inputs.
- `scripts/validate_luarocks.sh`, when present, must inspect the rendered release rockspec, unpack each `.src.rock`, inspect the rockspec inside it, and expand and scan any nested source archive payloads.
- Expand `.src.rock` artifacts during release privacy verification and scan their nested Lua source package payloads, not only the outer rock file.
- Add a Lua artifact regression test that proves validation fails when a rendered release rockspec, standalone Lua source package, or `.src.rock` contains `file://$HOME`, `file://<repo>`, a package-manager temporary path, or a live-worktree source path.
- Verify Lua release artifact versions agree with the C release version, generated headers, source archive `VERSION`, and checksum manifest.
- When Lua e2e mirrors C behavior, include deterministic local Lua e2e in local smoke gates and live Lua e2e in the explicit live prerelease tier.
- Do not install Lua runtime files, Lua rockspecs, Lua source files, Lua package-manager state, or Lua C binding/facade source files into C binary SDK artifacts.
- Lua C binding/facade code belongs in standalone Lua source packages and source rocks, not in per-target C SDK tarballs, unless the engineer explicitly defines a combined artifact.
- Lua interop headers that include `<lua.h>` belong with the Lua facade/source-rock surface by default. If the engineer deliberately ships them to C embedders outside the Lua source rock, CMake/pkg-config/package metadata must declare the Lua development header requirement explicitly.
- Lua module export-boundary tests must distinguish intentional `project_lua_*` interop exports from accidental leaked `project_*` core implementation symbols.
