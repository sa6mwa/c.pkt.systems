# Lifecycle Migration Ledger

This repository is aligned to the pkt.systems C/CMake lifecycle. The core/db/misc
cutover preserves public native and facade package/target names and the seven
release targets, while changing binary archive names and acquisition to three
composable groups. The source implementation is integrated; release readiness
still requires the parent quality gates described below.

| Old command or behavior | Lifecycle surface | Behavior preserved | Verification |
| --- | --- | --- | --- |
| Release build/test/package loops lived directly in `Makefile`. | `scripts/build.sh`, `scripts/test.sh`, `scripts/package.sh`, `scripts/run_linux_release_matrix.sh` behind standard Make targets. | `make build` and `make test` cover six Linux targets. `make package`, `make release-matrix`, and `make release` also build the mandatory arm64 Darwin target. | `tests/lifecycle_surface_test.sh`; existing package and release gates. |
| Generated-state removal lived directly in `Makefile`. | `scripts/clean.sh` and inventory-owned selected clean. | Unqualified clean removes generated state while preserving the operation-lock inode and reserved-tag recovery ownership. Selected clean removes only its owned group directories. Shared verified archives remain cached. | `tests/lifecycle_surface_test.sh`; `tests/package_isolation_build_test.py`; `tests/dependency_archive_cache_test.sh`. |
| No standard `finalize-slice`, `format`, `print-release-version`, `build-host`, `test-host`, `cross-build`, `test-cross`, or `test-install-tree` surface. | Standard lifecycle Make vocabulary. | Existing debug, release, package, and install-tree verification behavior is exposed through lifecycle names. | `tests/lifecycle_surface_test.sh`. |
| Checksum verification was available as `package-checksums` but not explicitly named in `release-matrix`. | Serialized binary/release scope and selected group checksums. | Binary scope verifies 22 payloads without certifying an old source archive. Release scope requires all 23 payloads and current independent reconstruction before its sole complete checksum manifest. Selected commands use only their build namespace. | `tests/dist_manifest_test.py`; `tests/package_integration_contract_test.py`; `scripts/verify-dist-manifest.sh`. |
| `prerelease` was a source-only confidence gate and `release` bypassed its proof. | Shared internal `release-pipeline`. | `prerelease` now executes the complete package-producing proof graph without a clean; `release` checks the version contract, then cleans and invokes the same graph. | `tests/lifecycle_surface_test.sh`; `make prerelease`; `make release`. |
| Darwin package generation and verification were conditionally skipped with unavailable osxcross. | c.pkt.systems mandatory Darwin release extension. | The arm64 Darwin bundle is required alongside all six Linux targets; absent osxcross fails before package generation or verification. | `tests/lifecycle_surface_test.sh`; `scripts/package.sh`; `scripts/package-verify.sh`. |
| Native fuzzing had smoke and standard modes but no standard extended surface; external checks had no lifecycle gate. | `fuzz-long` and fail-closed `prerelease-live` targets. | Existing AFL++ targets and external e2e workflows remain available behind explicit opt-ins. | `tests/lifecycle_surface_test.sh`; `scripts/fuzz.sh`; `scripts/run-afl-fuzz.sh`. |
| Static archive PIC coverage existed in build and package checks but was not tied to lifecycle surface regression. | Lifecycle smoke asserts both build-tree and install-tree static archive PIC smoke coverage. | `.a` archives for bundled deps and facades remain usable from shared-library consumers across the release matrix. | `static_archive_pic_link`; `scripts/package-install-smoke.sh`; `tests/lifecycle_surface_test.sh`. |

## Composable package cutover

`GROUP=core|db|misc|all`, `PRESET=<configured preset>` and
`SCOPE=selected|binary|release` are validated before operational work. Unqualified
build/test still uses six Linux Release targets. A selected optional operation
imports verified core and fails with an explicit prepare command if core is
missing, stale or corrupt. It does not rebuild core, test core, or provision a
toolchain. All-group bootstrap prepares core before optional groups. Public Make
and direct mutating script entrypoints share a live root operation lock and
scoped delegation; service children do not retain these descriptors.

Binary SDKs are `c.pkt.systems-<version>-<group>-<target>.tar.gz`. Their common
prefix has unique file/symlink ownership and canonical schema-1 group manifests.
Packaging and installed CMake discovery use the same standard-library validator;
direct pkg-config requires an explicit validator call first. See
[SDK installation](sdk-installation.md) and [dependencies](dependencies.md).
The native cmocka API remains available alongside the genuinely C89 facade;
neither becomes a production-library link dependency.

Independent source reconstruction starts from extracted source and fresh compiled
state, retains the verified archive cache, and builds the native GNU Release
group producers once with eight jobs. It runs owned suites, composition and
installed group combinations; it does not invoke a nested seven-target release.
Original-workspace evidence cannot publish a reconstruction proof. New source
files must be included in Git's index before source archive construction.

The native Darwin workflow has separate source and producer-artifact lanes.
Development branches support manual source dispatch after push. Artifact mode
requires the actual final lightweight tag and an authenticated unpublished draft
handoff containing all 24 upload assets; no automatic gate creates a release.
Both lanes retain native runtime cases, arm64 macOS 26, deployment floor 15.0,
two jobs and the 120-minute deadline.

Focused GNU native group/package/composition, ARM32 QEMU cmocka, mutation,
transport and command fixtures are recorded in the ignored integration report.
Cold source reconstruction, complete matrices and hardening/e2e, final
`finalize-slice`, exact-commit hosted Darwin, and clean release remain parent
integration gates. No full release readiness is inferred from focused results.
