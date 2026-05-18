# Release Procedure

## Release Procedure

Prerequisites:

- All intended source, version, lifecycle, and documentation changes are committed before release flow proceeds. The worktree must be clean except ignored generated files.
- If releasing from a feature or fix branch, that branch contains the committed work intended for release.
- If releasing directly from `main`, the current `main` `HEAD` contains the committed work intended for release.
- All review issues are addressed.
- Required local gates pass.
- Release authority is clear.

Branch decision:

- Release from `main`.
- Detect the current branch.
- If already on `main`, release from the current `HEAD` after confirming it contains the intended changes and the worktree is clean.
- If on a feature or fix branch, keep that branch intact, run review and pre-release gates there, then squash it onto `main` as one Conventional Commit.
- Stop and ask if there is no local `main`, if `HEAD` is detached, if `HEAD` is already on a tag, if a merge/rebase/cherry-pick is in progress, or if the current branch has ambiguous release intent.
- Never push or publish from a dirty worktree.
- Do not push `main` or the tag until final tagged release tests and artifact verification have succeeded locally.

Version decision:

- Inspect existing tags matching `vX.Y.Z`.
- Determine the next semver from the change set.
- Use patch for compatible fixes and internal lifecycle repairs.
- Use minor for backward-compatible features, new APIs, new optional artifacts, new optional targets, or new non-breaking lifecycle surfaces.
- Breaking API, ABI, artifact layout, command-line, CMake package, pkg-config, Lua facade, source archive, binary compatibility, dependency contract, or documented behavior changes require engineer discussion before choosing the bump.
- Do not bump the major version automatically. A major bump is a product decision and requires explicit engineer agreement.
- Treat any Conventional Commit with `!` or a `BREAKING CHANGE:` footer as a release-planning escalation, not an automatic major bump.
- Treat `fix:` as patch by default.
- Treat `feat:` as minor by default.
- Treat `perf:`, `refactor:`, `build:`, `ci:`, `test:`, `docs:`, and `chore:` as patch unless they introduce a public feature or breaking change.
- For `0.y.z` projects, breaking changes may stay within major version `0`; choose minor or patch only after stating the breaking nature of the change and getting engineer agreement on the bump.
- Ensure `VERSION`, generated headers, package metadata, Lua rockspecs, source archives, and single-header artifacts agree with the selected version.
- Verify the selected `vX.Y.Z` tag does not already exist locally or on origin.
- Verify the selected version is greater than the highest existing stable `vX.Y.Z` tag.
- Ignore prerelease tags for stable version ordering unless the release being prepared is explicitly a prerelease.
- If the change set includes multiple categories, choose the highest required bump.
- If the bump cannot be determined from commits and diffs, stop and ask before touching `main`.

Review gate:

- Run an independent local Codex review on the candidate branch with `codex review --base main`.
- Use `codex review --uncommitted` only for local uncommitted fixes before committing or amending.
- After squashing onto local `main`, use `codex review --commit HEAD` when a final commit-level review is useful before tagging.
- Treat actionable findings as blockers.
- Iterate with the engineer until there are no unresolved review issues.

Release plan preview:

- Before touching `main`, prepare and report a concise release plan.
- The plan must include selected version, tag, candidate branch, target `main` commit or merge base, intended Conventional Commit summary, expected artifacts, gates to run, optional gates skipped, and engineer decisions required.
- If the release plan has unresolved engineer decisions, stop before touching `main`.

Pre-release gate on the candidate branch:

1. `make clean`
2. `make test-all`
3. `make asan`
4. `make fuzz-smoke` when fuzzing exists
5. `make bench-gate` or `make perf-gate` when performance gates exist
6. `make test-e2e` when deterministic e2e exists
7. `make package-verify`
8. `make world` only when the repository defines it and it is the accepted exhaustive gate

Squash and tag:

1. If on a feature or fix branch, remember the branch name and commit range.
2. Switch to `main`.
3. Update `main` from origin with a fast-forward-only pull.
4. If releasing from a feature or fix branch, squash that branch onto `main` as one Conventional Commit with a factual body covering behavior, tests, packaging, and release impacts.
5. If releasing from `main` directly, do not squash; use the current `HEAD`.
6. Ensure the selected version is reflected in committed version files before tagging.
7. Create a lightweight tag on `HEAD` with `git tag vX.Y.Z`. Do not create an annotated tag unless the engineer explicitly requests it.
8. Verify the tag did not already exist locally or on origin before creating it.

Final release gate from tagged main:

1. Clean generated output.
2. Run final confirmation tests.
3. Build release artifacts under `dist/` using the lightweight tag on `HEAD` as the version source.
4. Run package verification and checksum verification.
5. Verify `dist/<project>-<version>-CHECKSUMS` lists exactly the intended upload artifacts.
6. Assert every checksum-listed artifact exists under `dist/`.
7. Assert no extra release-looking artifact under `dist/` is omitted from the checksum manifest unless deliberately excluded.
8. Verify `git rev-parse HEAD` matches `git rev-parse vX.Y.Z`.
9. Push `main` to origin.
10. Verify `git rev-parse HEAD` matches `git rev-parse origin/main`.
11. Push the lightweight tag to origin.
12. Verify the remote tag identifies the same commit as local `HEAD`.
13. Create the GitHub release with `gh release create vX.Y.Z` using only checksum-listed artifacts.

If any final gate fails after the local lightweight tag is created, do not push `main`, do not push the tag, and do not create the GitHub release. Delete the local tag, fix the problem on local `main`, amend the squashed release commit when the fix belongs to the same release change, recreate the lightweight tag, and rerun the final tagged release gate. This path should be rare because the feature branch pre-release gates are expected to catch failures before `main` is touched.

Release retry protocol:

- If pushing `main` fails, stop before pushing the tag. Resolve the branch push problem, verify local `main` still points at the tagged commit, rerun only the checks needed to prove no local state changed, then retry pushing `main`.
- If pushing the tag fails after `main` was pushed, verify remote `main` points at the same commit as local `main`, verify local `vX.Y.Z` points at `HEAD`, verify no remote tag with a different object exists, then retry pushing the tag.
- If `gh release create` fails after `main` and the tag were pushed, do not rebuild artifacts and do not move the tag. Verify `origin/main`, local `HEAD`, local `vX.Y.Z`, and remote `vX.Y.Z` all identify the same commit. Re-verify the checksum manifest and retry only the GitHub release creation using the same checksum-listed artifacts.
- If a GitHub release was partially created, inspect it with `gh release view vX.Y.Z`. If it exists but assets are missing, upload only the missing checksum-listed assets after verifying their checksums. If it exists with wrong assets, stop and ask before deleting or replacing anything.
- Never force-push `main` or retarget an already-pushed release tag without explicit engineer approval.
- If the pushed tag is wrong, stop immediately. Do not publish or repair silently.

Do not publish a release from an untagged commit, from a dirty worktree, from artifacts built before the final tag, from a tag that is not on `HEAD`, or from a `dist/` glob.

