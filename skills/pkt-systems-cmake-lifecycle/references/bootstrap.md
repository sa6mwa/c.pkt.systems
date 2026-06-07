# Bootstrap Procedure

## Bootstrap Procedure

For a new repository:

1. Extract product name, C library name, namespace, binary name, public headers, examples, dependencies, Lua needs, e2e services, target matrix, and artifact types from the spec.
2. Define the public C API shape before implementation: handle style, receiver shells or opaque handles, transparent config/value structs, required free functions, ownership rules, error surface, streaming/spooling names, examples, and compatibility promises.
3. Create the repository layout and `VERSION`.
4. Create `CMakeLists.txt` with project-prefixed options, static/shared library targets, install rules, package config generation, tests, examples, fuzz/bench/e2e hooks as needed, dependency root variables, and strict warning policy for project-owned C.
5. Create `CMakePresets.json` with the required presets and only relevant optional presets.
6. Create `Makefile` with the standard surface, including slice, prerelease, hardening, packaging, and optional service/Lua/fuzz targets when those surfaces exist.
7. Create lifecycle scripts only where Make would become stateful or unreadable.
8. Add tests first for public API, preferred API style, error behavior, allocator/ownership behavior, install-tree consumers, dependency modes, version resolution, source archive smoke, packaging, and any e2e/Lua/fuzz/benchmark surfaces.
9. Implement the code to satisfy the tests.
10. Run the narrow gates, then the prerelease tier, then release-matrix or `make package-verify`.
