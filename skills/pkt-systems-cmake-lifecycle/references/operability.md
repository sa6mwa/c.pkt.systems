# Operability, Layout, And Command Surfaces

## Agent Operability Contract

This lifecycle exists to make C/CMake repositories human-operable and agent-operable through the same local commands. The operating loop is:

```text
inspect -> plan -> edit -> narrow gate -> repair -> broader gate -> summarize
```

Rules:

- Prefer fast, abundant local feedback. Remote CI/CD is not part of the default lifecycle.
- After code edits, run the narrowest relevant local gate first.
- After public API edits, run header, C-only consumer, install-tree, and package checks relevant to the changed surface.
- After package, dependency, release, RPATH/RUNPATH, install-name, or artifact layout edits, run package verification or the closest available local packaging gate.
- After e2e service edits, run `dev-reset`, `dev-up`, and `test-e2e` or the closest project-specific local e2e gate.
- After Lua facade edits, run `lua-rock`, `lua-test`, and any Lua benchmark gate that protects a hot path.
- Do not run `make clean` reflexively between ordinary build, test, debug, sanitizer, e2e, Lua, fuzz, or benchmark targets. Reuse configured builds and cached dependencies for fast iteration unless there is a concrete stale-state reason.
- Run `make clean` deliberately when dependency versions, dependency URLs, dependency checksums, toolchain files, target IDs, cache layout, package layout, release versioning, or generated dependency roots change, or when a failure plausibly comes from stale build/dependency state.
- When the skill already covers a lifecycle-mechanical decision, do not ask for permission. Implement, verify, and report.
- Ask only for product, architecture, ABI/API, release authority, external service, or unsupported-tool decisions.

Execution tiers:

- **Inner loop**: seconds to low minutes; targeted configure/build/test commands for the edited surface.
- **Confidence loop**: normal local verification such as `test-all`, deterministic e2e, Lua tests, fuzz smoke, benchmark gates, and package verification.
- **Release loop**: clean, serialized, no shortcuts; candidate branch gates, review, squash to local `main`, lightweight tag, final tagged build, package verification, push, and GitHub release.

Failure taxonomy:

- `build-graph`: CMake target, preset, dependency graph, or install rule failure.
- `compiler`: compiler error or warning policy failure.
- `test`: unit, fixture, example, or consumer test failure.
- `sanitizer`: ASan, UBSan, or related runtime failure.
- `e2e-service`: local service startup, readiness, endpoint, credential, or protocol failure.
- `dependency-acquisition`: SDK bundle download, checksum, unpack, cache, or target-root failure.
- `package-layout`: tarball root, directory contract, missing file, forbidden file, or stale artifact failure.
- `relocatability`: local path, absolute RPATH/RUNPATH, Darwin install-name, pkg-config, or CMake config relocation failure.
- `api-abi-risk`: public API, exported symbol, SONAME/SOVERSION, struct layout, or compatibility risk.
- `release-authority`: unclear version, branch, tag, GitHub release, or engineer approval state.
- `external-tool-unavailable`: optional tool unavailable; skip only when absence is acceptable and documented.

Structured diagnostics:

- Major lifecycle scripts should end important failures with a compact diagnostic block in plain key/value text.
- Diagnostics are for humans, agents, and wrappers. Human-readable logs remain authoritative.
- Use this shape when practical:

```text
PKT_DIAGNOSTIC_BEGIN
surface=<make-target-or-script>
phase=<specific-phase>
status=failed
class=<failure-taxonomy-class>
reason=<short-stable-reason>
artifact=<artifact-or-path-when-relevant>
next=<actionable-next-step>
PKT_DIAGNOSTIC_END
```

- Keep diagnostics short. Put long compiler, e2e, package, or benchmark logs before the diagnostic block or in named log files referenced by `artifact` or `next`.
- Do not make persistent generated report files part of the default lifecycle. Keep diagnostics in command output and the active agent context unless the engineer explicitly asks for report artifacts.


## Lifecycle Spine

Every repository should converge to this spine:

```text
deps -> configure -> build -> test -> hardening -> e2e -> package -> verify -> review -> release
```

The lifecycle has mandatory surfaces and optional extension surfaces.

Mandatory surfaces:

- Public C API and source layout.
- CMake build graph.
- CMake presets.
- Make command surface.
- Dependency acquisition from `c.pkt.systems` SDK bundles.
- Fast host tests.
- Sanitizer tests.
- Release target matrix.
- Binary SDK packaging.
- Checksums.
- Release artifact verification.
- Privacy and host-path scans.
- Relocatable runtime paths for shipped binaries and shared libraries.
- Install-tree downstream consumer tests.

Optional extension surfaces, enabled when the project needs them:

- Coverage reports.
- Fuzzing.
- Benchmarks and performance gates.
- Docker Compose-backed e2e.
- External integration tests.
- Lua facade and Lua rock artifacts.
- Single-header artifacts.
- Source archives.
- Vendored upstream patch workflows.
- Darwin smoke bundles.
- Language parity or foreign-runtime benchmark harnesses.


## Repository Layout

Use this layout unless the project has a proven product reason to diverge:

```text
CMakeLists.txt
CMakePresets.json
Makefile
VERSION
README.md
LICENSE
cmake/
scripts/
include/
src/
tests/
examples/
dist/
build/
.cache/
```

Optional directories:

```text
bench/
fuzz/
lua/
gobencher/
devenv/
docker/
vendor/
performance-logs/
perflogs/
```

Rules:

- `Makefile` is the public command surface.
- `CMakePresets.json` is the build configuration surface.
- `scripts/` holds stateful orchestration and long logic.
- `cmake/` holds CMake modules, toolchains, package scripts, archive assertions, version logic, and config templates.
- `dist/`, `build/`, generated dependency roots, local service state, and package-manager build directories are generated.
- Source archives may include static Docker config, release scripts, examples, tests, and fixture descriptors. They must not include generated service state, dependency caches, build trees, package-manager temp trees, credentials, or VCS internals.
- `make clean` is the go-to full generated-state reset. It removes `build/`, `dist/`, generated dependency/cache roots under `.cache/`, and package-manager build state.
- `make clean-dist` removes only release artifacts under `dist/`.
- Do not make normal build/test targets depend on `make clean`; fast local CI/CD depends on cache reuse.


## CMake Presets

Required configure presets:

- `base`: hidden, Ninja generator, build directory `build/${presetName}`, compile commands on.
- `debug`: host Debug build with tests and examples.
- `asan`: host Debug build with ASan and UBSan.
- `x86_64-linux-gnu-release`
- `x86_64-linux-musl-release`
- `aarch64-linux-gnu-release`
- `aarch64-linux-musl-release`
- `armhf-linux-gnu-release`
- `armhf-linux-musl-release`
- `arm64-apple-darwin-release`, when Darwin artifacts are supported.

Optional configure presets:

- `release`: alias or host release preset only when useful.
- `host`: host-native Release or benchmark build when separate from `debug`.
- `coverage`
- `fuzz`
- `e2e`
- `integration`
- `profile`
- install-tree or shared-check presets when package validation needs a dedicated configuration.

Build presets mirror configure presets. Test presets exist for each executable test configuration. Release presets must set target identity explicitly through project-prefixed variables.

Use project-prefixed CMake options:

- `<P>_BUILD_STATIC`
- `<P>_BUILD_SHARED`
- `<P>_BUILD_BINARY`
- `<P>_BUILD_EXAMPLES`
- `<P>_BUILD_TESTS`
- `<P>_BUILD_E2E_TESTS`
- `<P>_BUILD_INTEGRATION_TESTS`
- `<P>_BUILD_BENCHMARKS`
- `<P>_BUILD_FUZZERS`
- `<P>_ENABLE_COVERAGE`
- `<P>_INSTALL`
- `<P>_DIST_DIR`
- `<P>_EXTERNAL_ROOT`
- `<P>_DEPENDENCY_BUILD_ROOT`
- `<P>_TARGET_ID`
- `<P>_TARGET_ARCH`
- `<P>_TARGET_OS`
- `<P>_TARGET_LIBC`


## Make Surface

Required targets:

- `make help`
- `make deps-debug`
- `make deps-release`
- `make deps-cross`
- `make build`
- `make build-debug`
- `make build-release`
- `make test`
- `make test-debug`
- `make test-all`
- `make asan`
- `make package`
- `make package-checksums`
- `make package-verify`
- `make verify-release-archives`
- `make release`
- `make format`
- `make clean`
- `make clean-dist`

Optional targets, only when the surface exists:

- `make build-host`
- `make test-host`
- `make test-cross`
- `make cross-build`
- `make cross-test`
- `make coverage`
- `make test-coverage`
- `make fuzz`
- `make fuzz-smoke`
- `make fuzz-long`
- `make bench`
- `make benchmarks`
- `make bench-check`
- `make bench-gate`
- `make bench-compare`
- `make bench-freeze-baseline`
- `make benchmarks-go`
- `make benchmarks-gobencher`
- `make perf-gate`
- `make test-e2e`
- `make test-integration`
- `make dev-up`
- `make dev-down`
- `make dev-reset`
- `make dev-ps`
- `make dev-logs`
- `make test-install-tree`
- `make world`
- `make lua-rock`
- `make lua-test`
- `make release-lua-artifacts`
- `make lua-bench`
- `make lua-bench-gate`
- `make package-source`
- `make package-source-smoke`
- `make package-single-header`
- `make release-darwin-smoke-bundle`
- `make vendor-<name>`
- `make vendor-<name>-apply`
- `make vendor-<name>-status`
- `make vendor-<name>-upgrade`
- `make build-<name>`
- `make verify-<name>-patches`

Every target listed in `make help` must work or fail with an actionable missing-prerequisite message.


## Script Surface

Use these script names when the behavior exists:

- `scripts/deps.sh`
- `scripts/build.sh`
- `scripts/test.sh`
- `scripts/host_test.sh`
- `scripts/cross_build.sh`
- `scripts/cross_test.sh`
- `scripts/fuzz.sh`
- `scripts/package.sh`
- `scripts/package-verify.sh`
- `scripts/run_linux_release_matrix.sh`
- `scripts/world.sh`
- `scripts/clean.sh`
- `scripts/compose.sh`
- `scripts/dev-up.sh`
- `scripts/dev-down.sh`
- `scripts/dev-reset.sh`
- `scripts/dev-ps.sh`
- `scripts/dev-logs.sh`
- `scripts/test-e2e.sh`
- `scripts/run_timed.sh`
- `scripts/osxcross_available.sh`
- `scripts/release_version.sh`
- `scripts/stage_release_sources.sh`
- `scripts/test_release_from_source.sh`
- `scripts/verify_release_artifacts.sh`
- `scripts/build_lua_rock.sh`
- `scripts/render_release_rockspec.sh`
- `scripts/stage_lua_rock_sources.sh`
- `scripts/validate_luarocks.sh`

Project-specific scripts are allowed only behind the standard Make targets.

Script safety contract:

- Use strict shell behavior for lifecycle scripts: fail on errors and unset variables where practical.
- Quote paths and variables.
- Resolve the repository root once and operate relative to it.
- Trap cleanup for temporary directories, child processes, local daemons, and service state created by the script.
- Destructive cleanup must be limited to known generated directories such as `build/`, `dist/`, `.cache/`, package-manager build roots, temporary directories, and `devenv/volumes`.
- Never remove source-controlled files, parent directories, home directories, or arbitrary user-provided paths.
- Print actionable errors with the failed surface, phase, and next step. Use the structured diagnostic block for important lifecycle failures.
- Keep long orchestration in scripts and expose it through Make targets.


