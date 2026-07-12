---
name: pkt-systems-cmake-lifecycle
description: >-
  Self-contained lifecycle authority for pkt.systems-style C/CMake repositories
  that consume c.pkt.systems SDK bundles: bootstrap new components, migrate
  existing projects, standardize Make/CMake/dependency/test/e2e/Lua/package/release
  workflows, resolve and cache cpkt C/C++ toolchains, preserve bespoke behavior
  behind lifecycle extension points, verify thoroughly, squash/tag, build dist
  artifacts, and publish GitHub releases.
metadata:
  short-description: pkt.systems C/CMake lifecycle
---

# pkt.systems C/CMake Lifecycle

Use this skill from a C/CMake repository root when asked to work on a pkt.systems-style C component that consumes SDK bundles from `c.pkt.systems`. This includes ordinary engineering work, lifecycle consolidation, bootstrap, migration, verification, packaging, and release.

This skill is the process authority. It must not require external example repositories, prior local knowledge, or historical convention lookup. Everything needed to shape the repository lifecycle is encoded in this skill and its first-level references.

## Non-Negotiables

- Do not tell the user or future agents to derive this lifecycle from other repositories.
- Do not write source-repository provenance, workstation-local paths, parent-relative project paths, credentials, or temporary machine paths into generated repository files.
- Release artifacts must be relocatable and must not contain `$HOME`, source repository paths, build directory paths, dependency cache paths, package-manager temporary paths, or any absolute local workstation path. The release gate must expand and scan all checksum-listed artifacts, including nested source rocks and nested source archives, inspect runtime loader metadata, and fail before release on any local path or non-relocatable runtime path.
- Preserve project-specific behavior, but move lifecycle behavior behind the standard surfaces when doing lifecycle work. Do not broaden ordinary engineering requests into lifecycle migrations unless the requested change requires it.
- Verification is the release gate. Each new lifecycle behavior must have an executable check.
- `release` is the final local release action and gate. Do not introduce a separate umbrella target as the accepted final gate; put the complete clean release pipeline behind `make release` and expose narrower rehearsals through named targets such as `prerelease`, `prerelease-hardening`, and `release-matrix`.
- Release uploads must include the checksum manifest, normally `dist/<project>-<version>-CHECKSUMS`, and GitHub release assets must be selected from that manifest rather than from a `dist/` glob.
- Treat warnings as errors for all project-owned and otherwise controllable build outputs. Use compiler warning flags plus `-Werror` or the platform equivalent, and use fatal linker-warning flags where supported. Exclude noisy upstream dependency builds only when the warnings are outside the project’s practical control; wrappers, facades, generated project-owned code, package smoke consumers, and release verification helpers remain warning-clean release blockers.
- During release work, reviews and verification commands are gates only. If a release gate finds review issues, test failures, packaging errors, artifact problems, or other correctness issues, stop the release process and report the blocker. Do not fix, amend, retag, rebuild, push, or publish as part of the same release flow unless the engineer explicitly starts a separate fix iteration.
- Build, test, package, release, service, package-manager, and long-running operational commands run serially.
- Make `make help` the authoritative human command index.
- `dist/` is generated output, not the release manifest. Release uploads must come from a verified checksum or manifest file.
- Git commits created by this workflow use Conventional Commits.
- Local CI/CD is the default operating model. Do not scaffold or require remote CI/CD or GitHub Actions unless the engineer explicitly asks.
- Toolchain resolution is lifecycle-owned. Every Linux build uses a complete pinned Bootlin GCC collection, including compiler, linker, binutils, libc, headers, and C++ runtime; never select a host compiler or host binutils fallback. Native memory checking uses host-provided Valgrind; fuzzing uses a pinned cached AFL++ GCC-plugin build tied to the selected Bootlin GCC collection. Host `clang-format` and `clangd` are development-tool prerequisites only, never build compilers. C/C++ compiler discovery, automatic download, cache layout, static C++ runtime metadata, C89 compiler policy, and downstream setup must follow [references/toolchains.md](references/toolchains.md) and use the shared cache root `${CPKT_TOOLCHAIN_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/c.pkt.systems/toolchains}`. Do not create project-specific toolchain cache directories.

## Operating Posture

Use the lifecycle on every pkt.systems C/CMake task, but do not turn every request into a lifecycle migration. Determine the engineer's intent from the request and repository state, then load only the references needed for the affected surfaces.

- If the request is ordinary engineering work, use the lifecycle as constraints, verification policy, and command discovery. Make the requested change with the smallest coherent repository impact.
- If the request is lifecycle work, actively shape the repository toward the consolidated lifecycle while preserving product behavior and bespoke project value.
- If the request is release work, follow the release protocol exactly.
- If the request is ambiguous, inspect first. Ask only when ambiguity changes product behavior, API/ABI, release authority, external services, lifecycle architecture, or unsupported-tool decisions.

Do not maintain an exhaustive list of possible engineering task types. Instead, map the work to affected surfaces: public API, ABI, dependencies, build graph, tests, hardening, Docker Compose e2e, Lua, packaging, release, benchmarks, fuzzing, documentation, or local developer workflow.

The normal operating loop is:

```text
inspect -> classify affected surfaces -> load references -> edit -> narrow gate -> repair -> broader gate -> summarize
```

For release work, replace the edit/repair loop with a stop-only gate loop: inspect -> classify -> load release references -> run the prescribed gates -> stop on any issue or publish only after all gates pass. Release gates are not invitations to repair during release.

When the skill already covers a lifecycle-mechanical decision, do not ask for permission. Implement, verify, and report.

## Reference Router

Read references only after the request and repository state indicate they are relevant. All references are first-level files under `references/`.

- Read [references/api-design.md](references/api-design.md) when creating or changing public C APIs, handle/object boundaries, receiver-style functions, examples, streaming APIs, error/ownership rules, or implementation layering.
- Read [references/operability.md](references/operability.md) for repository layout, CMake presets, Make targets, script surfaces, cache discipline, diagnostics, and lifecycle command shape.
- Read [references/toolchains.md](references/toolchains.md) when touching C/C++ compiler discovery, cross-target setup, autodownloaded compiler collections, CMake toolchain files or presets, static C++ runtime closure, downstream setup instructions, release target matrices, or package metadata that exposes compiler/runtime requirements.
- Read [references/dependencies.md](references/dependencies.md) when touching SDK dependencies, cache invalidation, dependency provenance, bundled/external dependency rules, license provenance, any JSON behavior owned by `lonejson`, or project-owned dependency behavior.
- Read [references/local-ci.md](references/local-ci.md) when touching build/test gates, API or ABI behavior, sanitizer coverage, fuzzing, benchmarks, install-tree consumers, or quality contracts.
- Read [references/docker-compose-e2e.md](references/docker-compose-e2e.md) when the repository has or needs deterministic local service e2e.
- Read [references/lua.md](references/lua.md) when the repository has or needs Lua facades, Lua C modules, source rocks, Lua benchmarks, or Lua release artifacts.
- Read [references/packaging.md](references/packaging.md) when touching `dist/`, binary SDKs, checksums, source archives, single-header artifacts, vendored upstreams, RPATH/RUNPATH, Darwin install names, or artifact verification.
- Read [references/bootstrap.md](references/bootstrap.md) when creating a new repository or filling an intentionally blank lifecycle from a spec.
- Read [references/migration.md](references/migration.md) when consolidating an existing repository, retiring bespoke command shapes, or preserving behavior behind standard lifecycle surfaces.
- Read [references/release.md](references/release.md) for any release, version, squash, tag, push, or GitHub release work.

Suggested starting sets:

- Blank repository from a spec: `bootstrap`, `api-design`, `operability`, `toolchains`, `dependencies`, `local-ci`, `packaging`; add `lua` or `docker-compose-e2e` only when the spec calls for them.
- Existing repository consolidation: `migration`, `operability`, `api-design`, then inspect and load the affected surface references.
- Ordinary feature or fix: inspect first, then load only the affected surface references. Do not run the migration procedure unless the feature requires lifecycle restructuring.
- Cross-target, compiler, or C++ facade work: `toolchains`, `operability`, `packaging`, and any API/dependency references affected by the product change.
- Release: `release`, `packaging`, `local-ci`, `toolchains`, and any optional surface references whose artifacts or gates are part of the release.

## Toolchain Command Surface

Use `scripts/cpkt-toolchains.sh` from this skill to inspect or provision the pinned Bootlin Linux compiler collections. Native fuzzing uses a pinned AFL++ GCC-plugin build tied to the x86_64 Bootlin collection:

```sh
skills/pkt-systems-cmake-lifecycle/scripts/cpkt-toolchains.sh discover
skills/pkt-systems-cmake-lifecycle/scripts/cpkt-toolchains.sh ensure <target|all>
eval "$(skills/pkt-systems-cmake-lifecycle/scripts/cpkt-toolchains.sh env <target>)"

skills/pkt-systems-cmake-lifecycle/scripts/cpkt-aflpp.sh ensure
eval "$(skills/pkt-systems-cmake-lifecycle/scripts/cpkt-aflpp.sh env)"
```

Downstream projects may vendor or call this lifecycle script, but the policy and cache root stay identical across pkt.systems C projects.

## Completion Report

When finished, report:

- Lifecycle surfaces added or changed.
- Gates run and results.
- Artifacts produced.
- Release tag and GitHub release URL when applicable.
- Any skipped optional gates and why.
- Any remaining risks or decisions for the engineer.
