# Release Procedure

## Release Procedure

Prerequisites:

- All intended source, version, lifecycle, and documentation changes are committed before release flow proceeds. The worktree must be clean except ignored generated files.
- If releasing from a feature or fix branch, that branch contains the committed work intended for release.
- If releasing directly from the release branch, that branch's current `HEAD` contains the committed work intended for release.
- All review issues are addressed.
- Required local gates pass.
- Release authority is clear.

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

- Run an independent local Codex review on the candidate branch with `codex review --base <release-branch>`.
- Use `codex review --uncommitted` only for local uncommitted fixes before committing or amending.
- After squashing onto the local release branch, use `codex review --commit HEAD` when a final commit-level review is useful before tagging.
- Treat actionable findings as blockers.
- Iterate with the engineer until there are no unresolved review issues.

Release plan preview:

- Before touching the release branch, prepare and report a concise release plan.
- The plan must include selected version, tag, candidate branch, release remote, release branch name, target release branch commit or merge base, intended Conventional Commit summary, expected artifacts, candidate-branch clean release rehearsal, final tagged clean release gate, optional gates skipped, and engineer decisions required.
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
14. `make world` only when the repository defines it and it is the accepted exhaustive gate.

If `make clean release` fails only because the repository currently refuses untagged release builds, treat that as a lifecycle gap to fix when in scope. Otherwise run the closest clean full-release rehearsal available, document the gap in the release plan, and do not squash until the candidate branch has passed the strongest available clean gate.

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
4. If releasing from a feature or fix branch, squash that branch onto the release branch as one Conventional Commit with a factual body covering behavior, tests, packaging, and release impacts.
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
5. Do not rebuild artifacts after this point unless the release pipeline failed and the local tag recovery path below is followed.
6. Verify `dist/<project>-<version>-CHECKSUMS` lists exactly the intended upload artifacts.
7. Assert every checksum-listed artifact exists under `dist/`.
8. Assert no extra release-looking artifact under `dist/` is omitted from the checksum manifest unless deliberately excluded.
9. Verify `git rev-parse HEAD` matches `git rev-parse vX.Y.Z`.
10. Push the release branch to `<release-remote>`.
11. Verify `git rev-parse HEAD` matches `git rev-parse <release-remote>/<release-branch>`.
12. Push the lightweight tag to `<release-remote>`.
13. Verify the remote tag identifies the same commit as local `HEAD`.
14. Create the GitHub release with `gh release create vX.Y.Z` using only checksum-listed artifacts.

If tagged release artifact generation or verification fails after the local lightweight tag is created, do not push the release branch, do not push the tag, and do not create the GitHub release. Delete the local tag, fix the problem on the local release branch, amend the squashed release commit when the fix belongs to the same release change, recreate the lightweight tag, and rerun the tagged release artifact generation pipeline. This path should be rare because the feature branch pre-release gates are expected to catch failures before the release branch is touched.

Release retry protocol:

- If pushing the release branch fails, stop before pushing the tag. Resolve the branch push problem, verify the local release branch still points at the tagged commit, rerun only the checks needed to prove no local state changed, then retry pushing the release branch to `<release-remote>`.
- If pushing the tag fails after the release branch was pushed, verify the remote release branch points at the same commit as the local release branch, verify local `vX.Y.Z` points at `HEAD`, verify no remote tag with a different object exists, then retry pushing the tag.
- If `gh release create` fails after the release branch and the tag were pushed, do not rebuild artifacts and do not move the tag. Verify `<release-remote>/<release-branch>`, local `HEAD`, local `vX.Y.Z`, and remote `vX.Y.Z` all identify the same commit. Re-verify the checksum manifest and retry only the GitHub release creation using the same checksum-listed artifacts.
- If a GitHub release was partially created, inspect it with `gh release view vX.Y.Z`. If it exists but assets are missing, upload only the missing checksum-listed assets after verifying their checksums. If it exists with wrong assets, stop and ask before deleting or replacing anything.
- Never force-push the release branch or retarget an already-pushed release tag without explicit engineer approval.
- If the pushed tag is wrong, stop immediately. Do not publish or repair silently.

Do not publish a release from an untagged commit, from a dirty worktree, from artifacts built before the final tag, from a tag that is not on `HEAD`, or from a `dist/` glob.
