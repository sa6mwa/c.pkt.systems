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
- Do not run `make clean` reflexively between ordinary build, test, debug, Valgrind, e2e, Lua, fuzz, or benchmark targets. Reuse configured builds and cached dependencies for fast iteration unless there is a concrete stale-state reason.
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
- `memory-check`: Valgrind Memcheck or related native memory-check failure.
- `e2e-service`: local service startup, readiness, endpoint, credential, or protocol failure.
- `dependency-acquisition`: SDK bundle download, checksum, unpack, cache, or target-root failure.
- `package-layout`: tarball root, directory contract, missing file, forbidden file, or stale artifact failure.
- `relocatability`: local path, absolute RPATH/RUNPATH, Darwin install-name, pkg-config, or CMake config relocation failure.
- `api-abi-risk`: public API, exported symbol, SONAME/SOVERSION, struct layout, or compatibility risk.
- `release-authority`: unclear version, branch, tag, GitHub release, or engineer approval state.
- `external-tool-unavailable`: optional tool unavailable; skip only when absence is acceptable and documented.

External tool discovery:

- Lifecycle scripts must not assume cross-target inspection or fixup tools are on `PATH`.
- The pkt.systems default osxcross root is `${OSXCROSS_ROOT:-$HOME/.local/cross/osxcross}` and the default Darwin host is `${CPKT_OSXCROSS_HOST:-arm64-apple-darwin25}`. The default Darwin tools are therefore under `$HOME/.local/cross/osxcross/bin/arm64-apple-darwin25-*`, for example `$HOME/.local/cross/osxcross/bin/arm64-apple-darwin25-otool`, `$HOME/.local/cross/osxcross/bin/arm64-apple-darwin25-install_name_tool`, and `$HOME/.local/cross/osxcross/bin/arm64-apple-darwin25-strip`.
- osxcross Darwin compiler drivers may select the host linker when the osxcross `bin` directory is not first on `PATH`. Darwin toolchain files and wrapper scripts must prepend `${OSXCROSS_ROOT}/bin` to `PATH` before configure, compiler-identification, try-compile, dependency configure/build, package smoke, and example/consumer link steps. Do not assume that invoking `${host}-clang` by absolute path is enough to select `${host}-ld`.
- Darwin toolchain files should also set `CMAKE_LINKER` to the target `${host}-ld` and inject an absolute `--ld-path=${CMAKE_LINKER}` into executable, shared-library, and module linker flags. Keep the `PATH` fix and the explicit linker-path fix together: the former protects compiler-driver and upstream build-system discovery, while the latter makes generated CMake link lines auditable. Do not pass an absolute path through `-fuse-ld`: modern Clang treats that option as a linker-flavor selector and deprecates path use.
- Add an executable regression test for the Darwin linker route when osxcross is available. The test should compile or dry-run link a minimal executable, prove the unfixed route would choose the host linker, and prove the lifecycle route chooses `${OSXCROSS_ROOT}/bin/${CPKT_OSXCROSS_HOST}-ld`.
- Discover target tools from configured project state before falling back to ambient tools. Prefer CMake cache or toolchain values such as `CMAKE_C_COMPILER`, `CMAKE_STRIP`, `CMAKE_INSTALL_NAME_TOOL`, `CPKT_OTOOL`, `CMAKE_OTOOL` when present, `CMAKE_READELF`, and project-prefixed tool override variables when the repository defines them.
- When a configured compiler path is known, inspect the compiler directory for sibling target-prefixed tools before falling back to `PATH`. For osxcross-style Darwin toolchains, look for names such as `<host>-otool`, `<host>-install_name_tool`, and `<host>-strip` next to `<host>-cc` or `<host>-clang`; with pkt.systems defaults, `<host>` is `arm64-apple-darwin25`.
- Package generation must use discovered target-correct mutation tools, such as `strip` and `install_name_tool`, rather than host tools with the same basename when mutation is deliberately required. For Darwin final artifacts, prefer verify-only packaging with correct link/install-time Mach-O metadata over post-package mutation.
- Package verification must use the discovered target-correct inspection tools, such as `readelf` and `otool`, and should report the exact lookup path tried when a required verification tool is unavailable.
- Absence of an optional inspection tool may skip only that optional inspection and only with an explicit message. Absence of a tool required to prove a release invariant is a verification failure, not a silent pass.
- Repositories that package cross-target artifacts should centralize this logic in one helper instead of reimplementing lookup in package, smoke, and privacy scripts. Use `scripts/discover_target_tools.sh` when the behavior exists.
- `scripts/discover_target_tools.sh` should accept at least a configured build directory or preset-derived build directory, a target ID, and optional project-prefixed overrides. It should print stable `KEY=value` shell assignments or another simple machine-readable format for `CC`, `STRIP`, `INSTALL_NAME_TOOL`, `OTOOL`, `READELF`, and any target host prefix it derived.
- The helper should read configured CMake cache values from the same build directory that produced the artifact being packaged or verified. It must not infer target tools from an unrelated host/debug build.
- Package generation, package verification, Darwin smoke bundle creation, and release privacy verification should consume the same discovered tool values. A mismatch between generation and verification tool discovery is a lifecycle bug. Missing Darwin mutation tools are acceptable only for verify-only package flows that do not mutate final Mach-O artifacts.
- Add tests for the discovery helper with temporary fake toolchain directories. Cover configured CMake cache values, target-prefixed osxcross sibling tools, unprefixed compiler sibling tools, `PATH` fallback, and refusal to select a known host tool for a cross-built Darwin artifact.

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
- Native Valgrind memory-check tests.
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
.clang-format
.gitignore
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
- `.clang-format` is checked in and starts from `clang-format -style=llvm -dump-config`; project style changes are explicit edits to that file, not hidden formatter defaults.
- `VERSION` is not checked into git for normal repositories. Add `/VERSION` to `.gitignore`; generate or inject it only for source archives and other non-git build contexts.
- `scripts/` holds stateful orchestration and long logic.
- `cmake/` holds CMake modules, toolchains, package scripts, archive assertions, version logic, and config templates.
- `dist/`, `build/`, generated dependency roots, local service state, and package-manager build directories are generated.
- Source archives may include static Docker config, release scripts, examples, tests, and fixture descriptors. They must not include generated service state, dependency caches, build trees, package-manager temp trees, credentials, or VCS internals.
- `make clean` is the go-to full generated-state reset. It removes `build/`, `dist/`, generated dependency/cache roots under the repository's `.cache/`, and package-manager build state. It must not remove or mutate the shared `${CPKT_DEPENDENCY_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/c.pkt.systems/deps}` archive cache or the sibling toolchain cache.
- `make clean-dist` removes only release artifacts under `dist/`.
- Do not make normal build/test targets depend on `make clean`; fast local CI/CD depends on cache reuse.


## CMake Presets

Required configure presets:

- `base`: hidden, Ninja generator, build directory `build/${presetName}`, compile commands on.
- `debug`: host Debug build with tests and examples.
- `debug-lua`, when Lua is supported.
- `valgrind`: native Debug facade subset checked by host-provided Valgrind.
- `fuzz`, when fuzzing exists.
- `integration`, when opt-in integration tests exist.
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
- `e2e`
- `profile`
- install-tree or shared-check presets when package validation needs a dedicated configuration.

Build presets mirror configure presets. Test presets exist for each executable test configuration. Release presets must set target identity explicitly through project-prefixed variables.

Preset rules:

- The hidden `base` preset pins the default dependency mode. Do not let stale CMake cache state silently change Valgrind, fuzz, integration, or release dependency resolution.
- Keep `valgrind` as an explicit native-host public target; do not silently turn a normal debug build into a memory-check run.
- Release presets set `<P>_DIST_DIR` and `<P>_TARGET_ID` explicitly.
- Optional cross presets may reference standard toolchain files or documented environment-owned compiler paths. If a cross toolchain is unavailable, release matrix scripts may skip that target only with an explicit message.
- Add a local test that verifies required presets, required cache variables, and dependency-mode defaults.

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
- `<P>_DEPENDENCY_MODE`, when the project supports bundled SDK, host, or auto dependency resolution.
- `<P>_VERSION_OVERRIDE`, for local release-candidate builds only.


## Make Surface

Core targets:

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
- `make valgrind`
- `make package`
- `make package-source`
- `make package-source-smoke`
- `make package-checksums`
- `make package-verify`
- `make verify-release-archives`
- `make verify-release-privacy`
- `make release-matrix`
- `make finalize-slice`
- `make prerelease`
- `make prerelease-hardening`
- `make release`
- `make print-release-version`
- `make format`
- `make clean`
- `make clean-dist`

Conditional standard targets, required when the surface exists:

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
- `make fuzz`
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
- `make example-smoke-local`
- `make example-smoke-live`
- `make prerelease-live`
- `make lua-rock`
- `make lua-env`
- `make lua-test`
- `make release-lua-artifacts`
- `make lua-bench`
- `make lua-bench-gate`
- `make package-single-header`
- `make release-darwin-smoke-bundle`
- `make vendor-<name>`
- `make vendor-<name>-apply`
- `make vendor-<name>-status`
- `make vendor-<name>-upgrade`
- `make build-<name>`
- `make verify-<name>-patches`

Make rules:

- `make help` must list every root target intended for humans or agents, including required opt-in environment variables for integration, live, service, and package-manager targets.
- `make format` formats project-owned C, headers, examples, tests, and generated single-header inputs with clang-format using the checked-in `.clang-format`.
- `make print-release-version` prints exactly the version that packaging/release targets will use.
- `make finalize-slice` is the default pre-commit gate for ordinary implementation slices: format plus the narrow local tests that catch common regressions quickly.
- `make prerelease` is deterministic local verification. It must not require real credentials or live external providers.
- `make prerelease-live` is credentialed or external-provider verification and must refuse to run without an explicit project-prefixed opt-in variable.
- `make prerelease-hardening` is expensive and may combine deterministic, live, long fuzz, benchmark, and release-matrix gates.
- `make release-matrix` builds, tests, packages, checksums, and verifies the release target set without requiring a clean tree. `make release` is the clean final pipeline.
- `make package-verify` must include release privacy verification for checksum-listed artifacts. `make verify-release-privacy` may exist as a focused gate for the same invariant, but it is not a substitute for including the check in package verification.
- `make package-source-smoke` extracts the source archive and proves it can configure, build, test, and resolve the same version without repository metadata.
- `make release` is the final clean release action and gate. It must fail on warnings for project-owned and otherwise controllable code using `-Werror` or the platform equivalent, while allowing documented exclusions for upstream dependency warnings outside practical project control.
- Core targets are the default lifecycle vocabulary. Conditional standard targets are not optional once the matching surface exists in the project.

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
- `scripts/discover_target_tools.sh`
- `scripts/release_version.sh`
- `scripts/stage_release_sources.sh`
- `scripts/test_release_from_source.sh`
- `scripts/verify_release_artifacts.sh`
- `scripts/verify_release_privacy.sh`
- `scripts/build_lua_rock.sh`
- `scripts/render_release_rockspec.sh`
- `scripts/stage_lua_rock_sources.sh`
- `scripts/validate_luarocks.sh`

Project-specific scripts are allowed only behind the standard Make targets.

Script safety contract:

- Use strict shell behavior for lifecycle scripts: fail on errors and unset variables where practical.
- Quote paths and variables.
- Resolve the repository root once and operate relative to it.
- Validate argument count and required files before mutating generated state.
- Trap cleanup for temporary directories, child processes, local daemons, and service state created by the script.
- Destructive cleanup must be limited to known generated directories inside the repository such as `build/`, `dist/`, `.cache/`, package-manager build roots, temporary directories, and `devenv/volumes`. It must not reach the shared XDG/HOME `c.pkt.systems/deps` archive cache or the toolchain cache.
- Scripts that delete or recreate a directory must refuse empty paths, `/`, the repository root, parent directories, home directories, and any path outside the expected generated-state root.
- Never remove source-controlled files, parent directories, home directories, or arbitrary user-provided paths.
- Print actionable errors with the failed surface, phase, and next step. Use the structured diagnostic block for important lifecycle failures.
- Keep long orchestration in scripts and expose it through Make targets.
