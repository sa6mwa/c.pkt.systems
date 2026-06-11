# Migration Procedure

## Migration Procedure

For an existing repository:

1. Inventory current public API, ABI, binaries, examples, tests, dependencies, release artifacts, e2e services, Lua artifacts, benchmarks, fuzz targets, vendored patches, and documented commands.
2. Classify each behavior into a lifecycle surface.
3. Keep product behavior and artifact compatibility unless the user explicitly requests a breaking cleanup.
4. Inventory public API style separately from implementation style. Prefer receiver-style handle functions for new usage, but do not remove public free-function compatibility surfaces without explicit approval.
5. Update examples and documentation snippets to the preferred public style and add executable checks that prevent regression to discouraged usage forms.
6. Replace bespoke command names with standard Make targets. Keep compatibility aliases only when already documented public commands.
7. Move long orchestration into standard scripts.
8. Normalize presets, target IDs, dependency roots, cache layout, host/bundled dependency modes, and target-tool discovery for packaging and verification.
9. If package, release, Darwin, or runtime-path scripts each discover tools independently, consolidate them behind a shared helper such as `scripts/discover_target_tools.sh` and add regression tests for configured CMake cache values, compiler sibling tools, osxcross-prefixed tools, and PATH fallback.
10. Add missing verification before deleting old behavior.
11. Remove dead lifecycle paths after the standard targets pass.
12. Run the relevant gates and report any remaining unsupported surfaces.

For non-trivial migrations, maintain `docs/lifecycle-migration.md` until the migration is complete. It should record:

- old command or behavior;
- new lifecycle command or surface;
- behavior preserved;
- verification added;
- behavior removed, deprecated, or intentionally changed;
- engineer decisions still required.

The migration ledger exists to prevent silent loss of bespoke project behavior. Keep it concise and delete it only when the repository has fully converged and the engineer does not want the ledger retained as documentation.
