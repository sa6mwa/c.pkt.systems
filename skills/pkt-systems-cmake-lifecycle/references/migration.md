# Migration Procedure

## Migration Procedure

For an existing repository:

1. Inventory current public API, ABI, binaries, examples, tests, dependencies, release artifacts, e2e services, Lua artifacts, benchmarks, fuzz targets, vendored patches, and documented commands.
2. Classify each behavior into a lifecycle surface.
3. Keep product behavior and artifact compatibility unless the user explicitly requests a breaking cleanup.
4. Replace bespoke command names with standard Make targets. Keep compatibility aliases only when already documented public commands.
5. Move long orchestration into standard scripts.
6. Normalize presets and target IDs.
7. Normalize dependency roots and cache layout.
8. Add missing verification before deleting old behavior.
9. Remove dead lifecycle paths after the standard targets pass.
10. Run the relevant gates and report any remaining unsupported surfaces.

For non-trivial migrations, maintain `docs/lifecycle-migration.md` until the migration is complete. It should record:

- old command or behavior;
- new lifecycle command or surface;
- behavior preserved;
- verification added;
- behavior removed, deprecated, or intentionally changed;
- engineer decisions still required.

The migration ledger exists to prevent silent loss of bespoke project behavior. Keep it concise and delete it only when the repository has fully converged and the engineer does not want the ledger retained as documentation.


