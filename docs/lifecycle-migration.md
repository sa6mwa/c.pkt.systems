# Lifecycle Migration Ledger

This repository is being aligned to the pkt.systems C/CMake lifecycle while preserving the shipped C SDK behavior, public C headers, package names, and release target set.

| Old command or behavior | Lifecycle surface | Behavior preserved | Verification |
| --- | --- | --- | --- |
| Release build/test/package loops lived directly in `Makefile`. | `scripts/build.sh`, `scripts/test.sh`, `scripts/package.sh`, `scripts/run_linux_release_matrix.sh` behind standard Make targets. | `make build`, `make test`, `make package`, `make release-matrix`, and `make release` still build the same Linux target set and optional Darwin target. | `tests/lifecycle_surface_test.sh`; existing package and release gates. |
| Generated-state removal lived directly in `Makefile`. | `scripts/clean.sh`. | `make clean` removes `build/`, `.cache/`, and `dist/`; `make clean-dist` removes `dist/` only. | `tests/lifecycle_surface_test.sh`. |
| No standard `finalize-slice`, `format`, `print-release-version`, `build-host`, `test-host`, `cross-build`, `test-cross`, or `test-install-tree` surface. | Standard lifecycle Make vocabulary. | Existing debug, release, package, and install-tree verification behavior is exposed through lifecycle names. | `tests/lifecycle_surface_test.sh`. |
| Checksum verification was available as `package-checksums` but not explicitly named in `release-matrix`. | Release matrix checksum surface. | Release matrix now names package, source archive, checksum, and package verification stages explicitly. | `tests/lifecycle_surface_test.sh`; `scripts/verify-dist-manifest.sh`. |
| Static archive PIC coverage existed in build and package checks but was not tied to lifecycle surface regression. | Lifecycle smoke asserts both build-tree and install-tree static archive PIC smoke coverage. | `.a` archives for bundled deps and facades remain usable from shared-library consumers across the release matrix. | `static_archive_pic_link`; `scripts/package-install-smoke.sh`; `tests/lifecycle_surface_test.sh`. |

Open lifecycle work:

- Continue replacing any future bespoke orchestration with standard script surfaces before adding new Make targets.
- Keep this ledger until the release process has completed with the lifecycle skill and the engineer decides whether to retain it as durable documentation.
