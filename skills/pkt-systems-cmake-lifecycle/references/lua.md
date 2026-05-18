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
- Do not assume support for old Lua versions or alternate JIT Lua runtimes. pkt.systems Lua artifacts target the system Lua runtime available for the project; currently expect Lua 5.5 unless the engineer explicitly expands the support matrix.
- Missing non-target Lua runtimes are not failures because they are not part of the support contract.
- `scripts/build_lua_rock.sh` accepts the LuaRocks build arguments: compiler, flags, shared-library flag, object extension, library extension, and Lua include directory.
- If the Lua module links an installed C SDK, discover it through pkg-config first, then a project-prefixed prefix variable, and fail with an actionable message if neither is available.
- If optional C dependencies can be enabled, probe them at build time and provide force/disable environment variables.
- On Linux, allow undefined symbols in Lua module shared objects only when that is required by Lua module loading.
- Optimize the C-to-Lua boundary deliberately. Prefer APIs that cross the boundary once per batch, stream, buffer, record set, or operation group instead of once per item when throughput matters.
- Lua facade tests and benchmarks should cover both correctness and boundary-crossing shape for hot paths.
- If the Lua facade consumes a released C SDK, test it against the installed SDK layout, not only against the source-tree build.

Release contract:

- `make release-lua-artifacts` writes all Lua release artifacts under `dist/`.
- Render a release rockspec with the exact release version.
- Stage a minimal Lua source tree with `LICENSE`, `README.md`, `VERSION`, `RELEASE_MANIFEST`, the rockspec template, build/render scripts, Lua sources, C facade sources, and required public C headers.
- Build a source archive and `.src.rock` through LuaRocks.
- Include Lua artifacts in the checksum manifest.
- Verify rockspec and rock contents for private paths, generated state, credentials, and missing source inputs.
- Do not install Lua runtime files, Lua rockspecs, Lua source files, Lua package-manager state, or Lua C binding/facade source files into C binary SDK artifacts.
- Lua C binding/facade code belongs in Lua source packages and source rocks, not in per-target C SDK tarballs, unless the engineer explicitly defines a combined artifact.


