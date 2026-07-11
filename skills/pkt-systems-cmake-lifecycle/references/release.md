# Release Procedure

## Release Procedure

Prerequisites:

- All intended source, version, lifecycle, and documentation changes are committed before release flow proceeds. The worktree must be clean except ignored generated files.
- Documentation has been reviewed for the release candidate, updated when needed, and committed on the feature or fix branch before the release flow begins. Documentation updates found at release decision time are preparation work, not release-gate repair work.
- If releasing from a feature or fix branch, that branch contains the committed work intended for release.
- If releasing directly from the release branch, that branch's current `HEAD` contains the committed work intended for release.
- All review issues are already addressed before release proceeds.
- Required local gates already pass before release proceeds.
- Release authority is clear.
- Release gates are stop-only. Reviews, tests, package verification, artifact checks, checksum checks, privacy checks, and publish preconditions must fail the release process when they find an issue. Do not fix code, amend commits, move tags, regenerate artifacts after a failed gate, or otherwise continue the release as part of the same flow unless the engineer explicitly starts a separate fix iteration.

Documentation preparation:

- When the engineer decides to release, first review the repository documentation against the candidate changes before starting the release flow.
- Review at least the public README, API documentation, examples, changelog or release notes when present, build/install/package instructions, and any user-facing CLI, CMake, Lua, or artifact documentation affected by the candidate changes.
- Update documentation when it is stale, incomplete, misleading, missing new behavior, or inconsistent with the release artifacts, supported platforms, command surfaces, examples, packaging surfaces, or compatibility promises.
- Commit required documentation updates on the feature or fix branch with a Conventional Commit before proceeding to branch decision, review gate, release plan preview, candidate-branch gates, squash, tag, artifact generation, push, or publish.
- If currently on the release branch and documentation updates are needed, stop before release flow begins and ask the engineer whether to create or switch to a feature or fix branch for the documentation preparation commit. Do not make documentation updates directly on the release branch unless the engineer explicitly instructs that release-branch documentation preparation is acceptable.
- If the documentation review finds no required changes, record that result in the release plan preview and continue only with a clean worktree.

Recommended Make target shape:

- `make prerelease` and `make release` must exercise the same release proof graph. Put that shared graph behind an internal `release-pipeline` target or an equivalent private script surface.
- `make prerelease` should run `release-pipeline` without cleaning generated state first. This gives fast release-equivalent feedback while engineers are still iterating.
- `make release` must clean generated state first, then invoke the same `release-pipeline`. It should not duplicate the prerelease commands, call a narrower target, or skip any proof already required by `prerelease`.
- `release-pipeline` should run the complete local proof in order: ordinary tests first, then the release matrix. Keep expensive optional hardening either inside the matrix when mandatory for release or behind an explicitly named target such as `prerelease-hardening`.
- `release-matrix` should build every supported release target, run host-executable tests for the host release target, produce every shipped artifact, generate the checksum manifest after all artifacts exist, and run package, archive, checksum, privacy, relocatability, instrumentation-leak, and loader-metadata verification from the checksum manifest.
- `prerelease-artifacts`, when kept for compatibility, should be an alias for `release-matrix`.
- `prerelease-hardening`, when no extra hardening tier exists, should be an alias for `prerelease` until a real hardening tier is defined.
- `prerelease-live` must fail closed unless live external-provider checks are explicitly enabled through a documented environment variable and credentials are available.
- `make help` must describe the public release targets and make clear that `release` is the clean final gate while `prerelease` is the same proof graph without the initial clean.

Executable lifecycle tests:

- Add a focused test that asserts `prerelease` and `release` share the same `release-pipeline`, that `release` cleans before invoking it, that ordinary tests run before the matrix, and that release does not bypass the shared pipeline.
- Add focused tests for checksum-manifest generation and upload-set selection: every release-looking artifact under `dist/` must be checksum-listed, every checksum-listed artifact must exist, and the checksum manifest itself must be included in release uploads.
- Add focused tests for artifact verification failures that previously could escape until publish time: local source/cache/build path leaks, local `file://` URLs, sanitizer or fuzzer instrumentation markers, non-relocatable RPATH/RUNPATH/install-name metadata, missing dependency manifests, and stale or omitted release artifacts.
- Tests should exercise observable release contracts through the public Make/script surfaces rather than only checking implementation details. Light structural tests are acceptable for target wiring because the target graph is part of the lifecycle contract.

Branch decision:

- Release from the repository's release branch. Resolve both `<release-remote>` and `<release-branch>` before touching branch state. Prefer the configured remote default branch, whatever it is named, and remember the remote that owns it. If the remote default cannot be determined, fall back to the local branch that exists among `main`, `master`, and `trunk` and use its configured upstream remote when present, otherwise `origin`. Stop and ask if more than one plausible local release branch exists and the remote default is unclear.
- Detect the current branch.
- If already on the release branch, release from the current `HEAD` after confirming it contains the intended changes and the worktree is clean.
- If on a feature or fix branch, keep that branch intact, run review and the full clean candidate-branch release rehearsal there, then squash it onto the release branch as one Conventional Commit.
- If the resolved release branch exists only as `<release-remote>/<release-branch>`, create the local tracking branch before the squash/tag step and verify it points at the remote branch. Stop and ask if neither a local nor remote-tracking release branch exists, if `HEAD` is detached, if `HEAD` is already on a tag, if a merge/rebase/cherry-pick is in progress, or if the current branch has ambiguous release intent.
- Never push or publish from a dirty worktree.
- Do not push the release branch or the tag until tagged release artifact generation and verification have succeeded locally.

Version decision:

- Inspect existing tags matching `vX.Y.Z`.
- Determine the next semver from the change set for release planning only. Do not let this planned version become the build or artifact version until the matching lightweight `vX.Y.Z` tag exists on `HEAD`.
- Use patch for compatible fixes and internal lifecycle repairs.
- Use minor for backward-compatible features, new APIs, new optional artifacts, new optional targets, or new non-breaking lifecycle surfaces.
- Breaking API, ABI, artifact layout, command-line, CMake package, pkg-config, Lua facade, source archive, binary compatibility, dependency contract, or documented behavior changes require engineer discussion before choosing the bump.
- Do not bump the major version automatically. A major bump is a product decision and requires explicit engineer agreement.
- Treat any Conventional Commit with `!` or a `BREAKING CHANGE:` footer as a release-planning escalation, not an automatic major bump.
- Treat `fix:` as patch by default.
- Treat `feat:` as minor by default.
- Treat `perf:`, `refactor:`, `build:`, `ci:`, `test:`, `docs:`, and `chore:` as patch unless they introduce a public feature or breaking change.
- For `0.y.z` projects, breaking changes may stay within major version `0`; choose minor or patch only after stating the breaking nature of the change and getting engineer agreement on the bump.
- Ensure generated headers, package metadata, Lua rockspecs, source archives, generated source-archive `VERSION`, and single-header artifacts agree with the selected version after the selected version is represented by a lightweight `vX.Y.Z` tag on `HEAD`.
- Version detection for git worktrees must prefer only an exact lightweight `vX.Y.Z` tag on `HEAD`, then a deliberate project-prefixed version override for release candidates when explicitly supplied, and otherwise `0.0.0`. A git worktree with no exact lightweight `vX.Y.Z` tag on `HEAD` must never default to the planned next release version, `0.1.0`, or any other inferred semver.
- Git worktree version detection must not read `VERSION`. `/VERSION` should be ignored in git repositories. Version detection outside git should use a source-archive `VERSION` file injected during source archive staging. Source archives are produced from tagged releases, so their `VERSION` must reflect the lightweight `vX.Y.Z` tag used to create the archive.
- Release-candidate overrides must be tested through both CMake and Make surfaces so Lua artifacts, source archives, package metadata, and checksum names cannot silently fall back to `0.0.0`.
- Verify the selected `vX.Y.Z` tag does not already exist locally or on `<release-remote>`.
- Verify the selected version is greater than the highest existing stable `vX.Y.Z` tag.
- Ignore prerelease tags for stable version ordering unless the release being prepared is explicitly a prerelease.
- If the change set includes multiple categories, choose the highest required bump.
- If the bump cannot be determined from commits and diffs, stop and ask before touching the release branch.

Review gate:

- Run an independent local Codex review on the candidate branch with `codex review -c model=gpt-5.6-sol -c model_reasoning_effort=medium --base <release-branch>`.
- Use `codex review -c model=gpt-5.6-sol -c model_reasoning_effort=medium --uncommitted` only outside the release flow during a separate fix iteration.
- Do not run a second Codex review after squashing onto the local release branch when the squash commit contains the same code already reviewed against `<release-branch>`. The squash changes commit topology, not the reviewed tree content.
- Treat actionable findings as blockers and stop the release process.
- Do not address review findings during the release process. Report them and wait for the engineer to start or approve a separate fix iteration.

Release plan preview:

- Before touching the release branch, prepare and report a concise release plan.
- The plan must include selected version, tag, candidate branch, release remote, release branch name, target release branch commit or merge base, intended Conventional Commit summary, expected artifacts, candidate-branch clean release rehearsal, final tagged clean release gate, optional gates skipped, and engineer decisions required.
- The intended Conventional Commit summary must describe the durable repository change being squashed, such as what was added, fixed, changed, removed, or refactored. Do not use generic release-process wording such as "release version X.Y.Z", "prepare release", "preparing for release", "prep release", or similar; the commit is not the release action and must not be named as if it only performs release administration.
- If the release plan has unresolved engineer decisions, stop before touching the release branch.

Candidate-branch gate before squash:

Run this on the feature or fix branch before touching the release branch. The goal is to prove the branch from a clean slate before it is squashed.

1. `make clean release` when the repository defines `release`. This is a clean release rehearsal on the candidate branch, not a publishable tagged release. In an untagged git worktree it may resolve the version as `0.0.0` or use an explicit project-prefixed release-candidate override when supplied, but it must exercise the same build, test, package, checksum, and verification graph as the final release.
2. If the repository does not define `make release`, run `make clean`, then `make prerelease` when the repository defines it; otherwise run `make test-all` and the explicit local gates below.
3. `make asan` when supported and not already included in `prerelease`.
4. `make tsan` when supported and not already included in `prerelease`.
5. `make msan` when supported and not already included in `prerelease`.
6. `make fuzz-smoke` when fuzzing exists and is not already included in `prerelease`.
7. `make bench-gate` or `make perf-gate` when performance gates exist.
8. `make test-e2e` when deterministic e2e exists and is not already included in `prerelease`.
9. `make lua-test` when Lua exists and is not already included in `prerelease`.
10. `make package-source-smoke` when source archives are shipped.
11. `make package-verify`
12. `make prerelease-live` only when release authority includes live external-provider verification and required credentials are available.
13. `make prerelease-hardening` when the repository defines it and the engineer expects the expensive hardening tier before release.
14. Do not run or add a separate umbrella release gate. The final clean gate is `make release`.

If `make clean release` fails, stop the release process. If the failure is only that the repository refuses untagged release rehearsals, report that lifecycle gap and the closest available clean gate, but do not patch the lifecycle, squash, tag, push, or publish in the same release flow.

Release matrix gate:

- The release matrix builds every supported target preset.
- It runs host-executable release tests for the host target.
- It packages every target archive.
- It builds source, Lua, single-header, and smoke-bundle artifacts when those surfaces exist.
- It generates the checksum manifest after all artifacts exist.
- It runs artifact verification from the checksum manifest.
- It runs release privacy and relocatability verification from the checksum manifest, including nested release payloads such as source archives and source rocks.
- It may skip optional targets whose cross toolchain is unavailable only with a clear message and only when that target is not mandatory for the release.

Squash and tag:

1. If on a feature or fix branch, remember the branch name and commit range.
2. Switch to the release branch.
3. Update the release branch from `<release-remote>` with a fast-forward-only pull.
4. If releasing from a feature or fix branch, squash that branch onto the release branch as one Conventional Commit whose subject answers "this commit will..." and whose body describes only durable repository changes: behavior, APIs, build/test surfaces, packaging, artifacts, and release impacts. Do not include non-change process actions such as Codex review runs, verification commands, gates passed, local rehearsal status, sign-off discussion, or other work performed to gain confidence; those belong in the final report, not the commit message.
5. If releasing from the release branch directly, do not squash; use the current `HEAD`.
6. Stop before tagging and ask the engineer whether to sign or otherwise amend the squashed commit, or continue with the current commit unchanged. Do not create the tag until the engineer explicitly chooses. If the engineer signs or amends the commit, re-read `HEAD`, verify the commit message and contents still match the release plan, and continue from the new commit.
7. Ensure no checked-in version file is being used for the git-worktree version. The selected version becomes active only through the lightweight `vX.Y.Z` tag on `HEAD`; generated source-archive `VERSION` files are created later as non-git release artifacts.
8. Create a lightweight tag on `HEAD` with `git tag vX.Y.Z`. Do not create an annotated tag unless the engineer explicitly requests it.
9. Verify the tag did not already exist locally or on `<release-remote>` before creating it.

Tagged release artifact generation from the release branch:

1. Run the repository's real release artifact generation pipeline from the tagged release branch, normally `make clean release`. This is the artifact-producing release build, even if the same command was already used as the clean candidate-branch rehearsal before squash.
2. The release pipeline must clean generated output, build release artifacts under `dist/` using the lightweight `vX.Y.Z` tag on `HEAD` as the version source, generate checksums, and run package verification. If no exact lightweight `vX.Y.Z` tag is on `HEAD`, the release pipeline must resolve the version as `0.0.0` and fail before publishing release artifacts rather than silently using `0.1.0` or another inferred version.
3. Run checksum verification when it is not already part of the release pipeline.
4. Run release privacy and relocatability verification when it is exposed as a focused target; otherwise confirm it is included in package verification or the release pipeline.
5. Do not rebuild artifacts after this point.
6. Verify `dist/<project>-<version>-CHECKSUMS` lists exactly the intended upload artifacts, and include that `CHECKSUMS` file itself in the GitHub release uploads.
7. Assert every checksum-listed artifact exists under `dist/`.
8. Assert no extra release-looking artifact under `dist/` is omitted from the checksum manifest unless deliberately excluded.
9. Verify `git rev-parse HEAD` matches `git rev-parse vX.Y.Z`.
10. Push the release branch to `<release-remote>`.
11. Verify `git rev-parse HEAD` matches `git rev-parse <release-remote>/<release-branch>`.
12. Push the lightweight tag to `<release-remote>`.
13. Verify the remote tag identifies the same commit as local `HEAD`.
14. Create the GitHub release with `gh release create vX.Y.Z` using the checksum-listed artifacts plus the checksum manifest itself. Do not upload from a `dist/` glob.

If tagged release artifact generation or verification fails after the local lightweight tag is created, stop the release process. Do not push the release branch, push the tag, create the GitHub release, delete or move the tag, fix code, amend the release commit, or rebuild artifacts in the same release flow. Report the failing gate and current local state so the engineer can decide whether to start a separate fix iteration and how to handle the local tag.

Release retry protocol:

- Retry only transient external publish failures that do not indicate a correctness problem in the release contents. Do not retry review, build, test, package, checksum, privacy, artifact, or metadata failures inside the release flow.
- If pushing the release branch fails, stop before pushing the tag. Retry only after verifying the failure was external or operational, the local release branch still points at the tagged commit, and no local content changed.
- If pushing the tag fails after the release branch was pushed, retry only after verifying the failure was external or operational, the remote release branch points at the same commit as the local release branch, local `vX.Y.Z` points at `HEAD`, and no remote tag with a different object exists.
- If `gh release create` fails after the release branch and the tag were pushed, do not rebuild artifacts and do not move the tag. Retry only after verifying the failure was external or operational, `<release-remote>/<release-branch>`, local `HEAD`, local `vX.Y.Z`, and remote `vX.Y.Z` all identify the same commit, and the checksum manifest still verifies the same checksum-listed artifacts.
- If a GitHub release was partially created, inspect it with `gh release view vX.Y.Z`. If it exists but assets are missing, upload only the missing checksum-listed assets after verifying their checksums. If it exists with wrong assets, stop and ask before deleting or replacing anything.
- Never force-push the release branch or retarget an already-pushed release tag without explicit engineer approval.
- If the pushed tag is wrong, stop immediately. Do not publish or repair silently.

Do not publish a release from an untagged commit, from a dirty worktree, from artifacts built before the final tag, from a tag that is not on `HEAD`, or from a `dist/` glob.
