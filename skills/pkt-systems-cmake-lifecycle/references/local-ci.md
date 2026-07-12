# Local Build, Test, Quality, Fuzzing, And Benchmarking

## Build And Test Gates

Fast local confidence:

1. `make build`
2. `make test`
3. `make valgrind`

Broader local confidence:

1. `make test-all`
2. `make package-verify`

Release confidence:

1. the explicit applicable pre-release gates
2. independent review on the feature branch
3. final clean `make release` package build from the tagged main commit

`make test-all` includes fast host tests, host release tests when useful, cross tests that can execute locally or through an approved runner, sanitizer tests when affordable, deterministic e2e when part of normal confidence, and benchmark gates when performance is a product property.

Do not add a separate umbrella target as a standard lifecycle target. The exhaustive clean-slate release gate is `make release`; expensive pre-release confidence belongs in `make prerelease-hardening` or another documented rehearsal target that does not replace final release.

Recommended production-loop tiers:

- `make finalize-slice`: formatting plus the narrow debug tests needed before committing a small slice.
- `make prerelease`: local, deterministic pre-release confidence. Include formatting, debug unit tests, sanitizer targets supported by the project, fuzz smoke, Lua tests, local example smoke, and deterministic local e2e when those surfaces exist.
- `make prerelease-live`: opt-in external-provider or credentialed integration tests. Refuse to run unless a project-prefixed environment variable explicitly enables them.
- `make prerelease-hardening`: the expensive tier. Include `prerelease`, live checks when explicitly enabled, long fuzz runs, benchmark gates when applicable, and the release matrix.
- `make release-matrix`: incrementally build, test, package, checksum, and verify every release target and optional release artifact. This is the artifact production rehearsal before the final clean release.

These names are preferred over project-specific gate names. Compatibility aliases are acceptable only for already documented public commands.


## Quality Contracts

Add tests that assert observable behavior:

- Public API success and failure paths.
- Preferred public API usage style, including receiver-style handle operations when that is the project convention.
- ABI and exported symbol expectations where ABI is promised.
- Header self-sufficiency and C-only consumer builds.
- C89 or project-selected C standard compatibility for installed SDK consumers.
- Optional C++ consumer builds when headers must be C++ compatible.
- Warning-clean builds with strict flags for project-owned code.
- Warning-clean release builds with warnings treated as errors for all project-owned or otherwise controllable code, including facades, wrappers, generated project-owned sources, examples, tests, package smoke consumers, and release verification helpers. Exclude upstream dependency warnings only when they are outside practical project control, and document the boundary in the warning regression test.
- Install-tree downstream consumers through CMake `find_package`.
- Install-tree downstream consumers through pkg-config when pkg-config is shipped.
- Examples built from the source tree and, when shipped, from installed examples.
- Example help/version smoke tests and deterministic local example workflows.
- Version resolution from lightweight `vX.Y.Z` tags in git worktrees, and from injected `VERSION` files only in non-git source archive builds. Include negative checks proving a git worktree with no exact lightweight `vX.Y.Z` tag on `HEAD` resolves to `0.0.0`, not `0.1.0`, a planned next release version, or a repository-local `VERSION` file.
- Version override behavior for release-candidate builds, including Make, CMake, Lua artifacts, source archives, and dry-run packaging targets.
- CMake preset presence and option defaults that are part of the lifecycle contract.
- Dependency mode behavior, including host mode, bundled SDK mode, auto mode fallback, unsupported targets, and wrong-ABI dependency rejection.
- Package archive layout and forbidden payload checks.
- Release artifact privacy and relocatability checks that expand checksum-listed artifacts and nested payloads before scanning and inspecting runtime loader metadata.
- Negative release privacy fixtures that create release-shaped throwaway artifacts containing representative leaks and prove the gate fails with the exact artifact/file path. Cover at least repository paths, `$HOME`, absolute local `file://` URLs, absolute local or non-system ELF RPATH/RUNPATH, local Darwin install names, Darwin non-system absolute dependency paths such as `/lib/libfoo.dylib`, and Darwin local rpaths when the platform/tooling surface exists.
- Lua release artifact checks, when Lua artifacts exist, that build `dist/<project>-lua-<version>.tar.gz`, render the release rockspec, build the `.src.rock`, verify the standalone Lua source package layout and manifest, unpack the `.src.rock`, expand the embedded Lua source package, and fail on absolute local `file://` URLs or live-worktree source paths.
- Lua/C interop checks, when a Lua facade deliberately exposes C embedder interop, that validate a Lua-created object from C, obtain a generic core borrowed view, preserve fragmented callback-backed streaming across the boundary without full payload materialization, return status/error values for wrong stack values or wrong userdata, reject wrong `size` or `abi_version`, reject mismatched schema/record pairs, prove Lua registry references preserve retained-object lifetime, verify clear/reset behavior invalidates or updates record view state according to the documented contract, prove Lua-facing and C-interop behavior have parity for success and policy failures, and allow only intentional Lua interop exports while catching accidental core symbol leaks.
- Source archive extraction, configure, build, version agreement, and test smoke.
- Source archive manifest exactness: tracked non-ignored files plus deliberate generated release files, no more and no less.
- Release privacy and relocatability scans for `$HOME`, repository paths, build roots, dependency caches, package-manager temporary paths, parent-relative paths, absolute local `file://` URLs, credentials, VCS metadata, generated service state, sanitizer artifacts, ELF RPATH/RUNPATH, Darwin install names/dependency paths/rpaths, Darwin invalid post-mutation signature risk when `LC_CODE_SIGNATURE` is present, and unstripped binary metadata that exposes local toolchain or home paths.

Test registration:

- Large unit suites should expose named test groups or labels so agents can run narrow checks while still allowing broad gates.
- Generate or verify the list of registered unit groups when possible, so adding a test function without registering it fails locally.
- Use CTest labels consistently: `unit`, `smoke`, `local`, `packaging`, `fuzz`, `integration`, `example-smoke`, and additional project-specific labels only when they help select meaningful gates.
- Keep deterministic local integration separate from live external integration. Use a distinct label such as `offline` for integration-shaped tests that do not require credentials or real providers.
- Live tests must be opt-in and must never be required by `make test` or fast local confidence.
- Put explicit timeouts on CTest tests. Short local tests should fail quickly; live or long integration tests should have bounded, documented longer timeouts.
- Tests that intentionally skip unless an opt-in variable is set should print a clear `SKIP` line and exit successfully only when absence is acceptable for that gate.


## API And ABI Contract

The public API is the installed C header set, documented CLI behavior, documented Lua facade behavior, exported CMake targets, pkg-config metadata, and release artifact layout.

The ABI is promised only when the project ships shared libraries with an explicit stable ABI contract. Static-only libraries still need source compatibility and downstream rebuild compatibility, but do not imply a stable dynamic ABI by themselves.

Rules:

- Public headers installed under `include/` define the C source API.
- Public headers must compile standalone.
- Public headers must support C consumers with the selected project C standard.
- Public headers must support C++ consumers when the project claims C++ compatibility.
- Public structs should be opaque unless their layout is intentionally part of the API. Receiver-shell structs with public method fields and private `impl` pointers are an intentional public layout and must be treated accordingly.
- Changing function signatures, public struct layout, enum values, constants, ownership rules, error semantics, CLI behavior, CMake target names, pkg-config names, or artifact paths can be breaking and requires engineer discussion.
- Shared libraries must set `SOVERSION` or the platform equivalent explicitly when ABI stability is promised.
- Linux shared libraries must have the intended SONAME and symlink set.
- Darwin shared libraries must have explicit install name and compatibility/current version policy when shared libraries are shipped.
- Removing or changing exported ABI symbols is breaking unless the symbol was explicitly private.
- ABI symbol checks are required when the project promises stable ABI; otherwise, at minimum verify the shipped shared library exposes only intended public symbols.
- Breaking API or ABI changes do not automatically imply a major bump. They require engineer discussion under the semver contract.


## Native memory checking

Valgrind Memcheck is the first-class native memory hardening gate.

Contract:

- `valgrind` builds a native Bootlin debug facade subset and runs it with leak checking, origin tracking, and a nonzero error exit code.
- The gate runs serially and fails clearly when the host has not installed Valgrind.
- Valgrind does not provide true MemorySanitizer coverage; document that boundary rather than claiming MSan equivalence.
- Release package verification must fail if hardening runtime paths or build paths appear in shipped artifacts.


## Fuzzing

Enable fuzzing when the project parses, frames, serializes, accepts untrusted bytes, handles protocols, or exposes complex state transitions.

Contract:

- `fuzz/` or `tests/fuzz/` contains fuzz targets and seeds.
- `fuzz` preset builds with the selected engine; the Bootlin GCC lifecycle selects pinned AFL++ GCC-plugin instrumentation.
- `make fuzz-smoke` runs bounded short jobs suitable for `prerelease`, `prerelease-hardening`, or `release` when fuzzing is part of the release gate.
- `make fuzz` runs standard bounded local jobs.
- `make fuzz-long` is opt-in and may run longer.
- Regression seeds that fixed bugs should be committed.
- Fuzzer targets should be added through a small CMake helper so compile flags, link flags, corpus paths, labels, and smoke tests remain consistent.
- Generated large corpora stay out of source unless deliberately curated.


## Benchmarks

Enable benchmarks when performance is part of the product contract.

Contract:

- `bench/` contains native benchmark sources, scripts, baselines, and README notes.
- `make bench` or `make benchmarks` runs normal local benchmarks.
- `make bench-gate` or `make perf-gate` enforces regression thresholds.
- `make bench-freeze-baseline` updates committed baselines intentionally.
- Language parity benchmarks, including Go benchmark harnesses, live behind explicit targets such as `benchmarks-go` or `benchmarks-gobencher`.
- Benchmark-generated source or fixture data must have deterministic regeneration targets.
- Benchmarks used as gates must use stable thresholds and actionable failure output.
