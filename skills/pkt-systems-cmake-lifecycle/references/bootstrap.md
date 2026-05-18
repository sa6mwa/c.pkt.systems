# Bootstrap Procedure

## Bootstrap Procedure

For a new repository:

1. Extract product name, C library name, namespace, binary name, public headers, examples, dependencies, Lua needs, e2e services, target matrix, and artifact types from the spec.
2. Create the repository layout and `VERSION`.
3. Create `CMakeLists.txt` with project-prefixed options, library targets, install rules, package config generation, tests, examples, fuzz/bench/e2e hooks as needed, and dependency root variables.
4. Create `CMakePresets.json` with the required presets and only relevant optional presets.
5. Create `Makefile` with the standard surface.
6. Create lifecycle scripts only where Make would become stateful or unreadable.
7. Add tests first for public API, error behavior, install-tree consumers, version resolution, packaging, and any e2e/Lua/fuzz/benchmark surfaces.
8. Implement the code to satisfy the tests.
9. Run the narrow gates, then `make test-all`, then `make package-verify`.


