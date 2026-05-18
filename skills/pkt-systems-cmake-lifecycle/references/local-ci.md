# Local Build, Test, Quality, Fuzzing, And Benchmarking

## Build And Test Gates

Fast local confidence:

1. `make build`
2. `make test`
3. `make asan`

Broader local confidence:

1. `make test-all`
2. `make package-verify`

Release confidence:

1. the explicit applicable pre-release gates, or `make world` when the repository defines it as the accepted umbrella gate
2. independent review on the feature branch
3. final clean release package build from the tagged main commit

`make test-all` includes fast host tests, host release tests when useful, cross tests that can execute locally or through an approved runner, sanitizer tests when affordable, deterministic e2e when part of normal confidence, and benchmark gates when performance is a product property.

`make world`, when present, is the expensive clean-slate umbrella gate. It runs clean, dependency setup, debug tests, host/release tests, cross tests, sanitizer tests, fuzz smoke or fuzz, deterministic e2e, benchmark gates, package verification, and release archive verification. Optional unavailable tools may be skipped only with explicit messages when absence is acceptable.

Do not require `make world` merely because the lifecycle is being consolidated. Add it when the repository benefits from one top-level exhaustive local gate. Otherwise, make the release procedure call the explicit applicable gates directly.


## Quality Contracts

Add tests that assert observable behavior:

- Public API success and failure paths.
- ABI and exported symbol expectations where ABI is promised.
- Header self-sufficiency and C-only consumer builds.
- C89 or project-selected C standard compatibility for installed SDK consumers.
- Optional C++ consumer builds when headers must be C++ compatible.
- Warning-clean builds with strict flags for project-owned code.
- Install-tree downstream consumers through CMake `find_package`.
- Install-tree downstream consumers through pkg-config when pkg-config is shipped.
- Examples built from the source tree and, when shipped, from installed examples.
- Version resolution from `VERSION` and `vX.Y.Z` tags.
- Package archive layout and forbidden payload checks.
- Release privacy scans for local paths, parent-relative paths, credentials, VCS metadata, generated service state, dependency caches, and sanitizer artifacts.


## API And ABI Contract

The public API is the installed C header set, documented CLI behavior, documented Lua facade behavior, exported CMake targets, pkg-config metadata, and release artifact layout.

The ABI is promised only when the project ships shared libraries with an explicit stable ABI contract. Static-only libraries still need source compatibility and downstream rebuild compatibility, but do not imply a stable dynamic ABI by themselves.

Rules:

- Public headers installed under `include/` define the C source API.
- Public headers must compile standalone.
- Public headers must support C consumers with the selected project C standard.
- Public headers must support C++ consumers when the project claims C++ compatibility.
- Public structs should be opaque unless their layout is intentionally part of the API.
- Changing function signatures, public struct layout, enum values, constants, ownership rules, error semantics, CLI behavior, CMake target names, pkg-config names, or artifact paths can be breaking and requires engineer discussion.
- Shared libraries must set `SOVERSION` or the platform equivalent explicitly when ABI stability is promised.
- Linux shared libraries must have the intended SONAME and symlink set.
- Darwin shared libraries must have explicit install name and compatibility/current version policy when shared libraries are shipped.
- Removing or changing exported ABI symbols is breaking unless the symbol was explicitly private.
- ABI symbol checks are required when the project promises stable ABI; otherwise, at minimum verify the shipped shared library exposes only intended public symbols.
- Breaking API or ABI changes do not automatically imply a major bump. They require engineer discussion under the semver contract.


## Fuzzing

Enable fuzzing when the project parses, frames, serializes, accepts untrusted bytes, handles protocols, or exposes complex state transitions.

Contract:

- `fuzz/` or `tests/fuzz/` contains fuzz targets and seeds.
- `fuzz` preset builds with libFuzzer or the selected engine.
- `make fuzz-smoke` runs bounded short jobs suitable for `world`.
- `make fuzz` runs standard bounded local jobs.
- `make fuzz-long` is opt-in and may run longer.
- Regression seeds that fixed bugs should be committed.
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


