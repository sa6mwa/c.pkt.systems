# Packaging, Runtime Paths, And Release Artifacts

## Packaging

Release artifacts are built under `dist/`.

Default target IDs:

- `x86_64-linux-gnu`
- `x86_64-linux-musl`
- `aarch64-linux-gnu`
- `aarch64-linux-musl`
- `armhf-linux-gnu`
- `armhf-linux-musl`
- `arm64-apple-darwin`, optional when the Darwin cross toolchain exists

Binary SDK naming:

```text
dist/<project>-<version>-<target-id>.tar.gz
```

Checksum manifest:

```text
dist/<project>-<version>-CHECKSUMS
```

The checksum manifest is the release upload manifest. It must be SHA-256, must list every artifact intended for the GitHub release, and must be the only source used to select `gh release create` upload arguments.

Optional artifacts:

```text
dist/<project>-<version>.tar.gz
dist/<project>-<version>.h.gz
dist/<project>-<version>-<target-id>-smoke-test.zip
dist/<lua-artifacts>
```

Binary SDK archive contract:

- The tarball's first and only top-level directory is `<project>-<version>-<target-id>/`.
- Every architecture target uses the same internal directory contract. File formats and suffixes vary by target, but paths do not.
- `include/` contains project public C headers only. Prefer `include/<project>/` for multi-header libraries and `include/<project>.h` only for deliberate single-header public APIs.
- `bin/` contains project-owned executables and CLI tools for that target, when the project ships runtime binaries.
- `lib/` contains project-owned target libraries:
  - static libraries: `lib<project>.a`;
  - Linux shared libraries: `lib<project>.so`, ABI symlinks such as `lib<project>.so.<major>`, and the real SONAME file when shared libraries are shipped;
  - Darwin shared libraries: `lib<project>.dylib` or versioned `.dylib` files when Darwin shared libraries are shipped.
- `lib/pkgconfig/<project>.pc` is present when pkg-config is supported.
- `lib/cmake/<project>/<project>Config.cmake` is present for CMake package consumers.
- `lib/cmake/<project>/<project>ConfigVersion.cmake` is present for CMake package consumers.
- `share/doc/<package>/LICENSE` contains the shipped license text.
- `share/doc/<package>/README.md` contains release-consumer documentation.
- `share/doc/<package>/examples/` contains installed examples only when examples are part of the SDK contract.
- `share/<project>/manifest.txt`, `share/<project>/package-metadata.*`, or `share/<project>/dependencies.*` may contain package metadata or dependency provenance when needed.
- Do not include generated build trees, dependency cache roots, package-manager build directories, local service state, test service volumes, fuzz corpora generated during release, benchmark logs, `.git`, `.env`, or private credentials.
- Do not include Lua runtime files, Lua rockspecs, Lua source modules, Lua package-manager state, or Lua C binding/facade source files in C binary SDK tarballs.
- Do not include benchmark harness internals, fuzz-only helpers, e2e fixtures, local dev-service config, vendored upstream worktrees, migration ledgers, or lifecycle diagnostics in C binary SDK tarballs unless the engineer explicitly defines them as product artifacts.
- Do not include third-party dependency headers or libraries unless the project explicitly ships a bundled SDK artifact. If bundled dependencies are shipped, their layout and license files must be specified and verified as part of the artifact contract.

For each target archive, answer these questions in the package verification tests:

- Does `<project>-<version>-<target-id>/` exist as the only root?
- Are all public headers under `include/` and no private headers leaked?
- Are all target libraries under `lib/`, with correct static/shared names and symlinks for the target platform?
- Are all target executables under `bin/`, executable, and built for the archive target?
- Are CMake package files under `lib/cmake/<project>/` relocatable and free of build/cache paths?
- Is pkg-config metadata under `lib/pkgconfig/` relocatable and free of build/cache paths?
- Are docs, license, examples, and metadata under `share/` only?
- Are Lua files and Lua C binding/facade sources absent from the C binary SDK archive?
- Are all runtime paths relocatable according to the runtime path invariant?
- Are forbidden generated or private files absent?

Package generation must:

- Clean `dist/` before release packaging.
- Build each requested target from the correct preset and dependency root.
- Stage through `cmake --install`.
- Strip project-owned libraries where appropriate.
- Fix Darwin install names when shipping Darwin dynamic libraries.
- Produce deterministic tar/gzip output as far as the toolchain reasonably allows.
- Generate checksums after all artifacts are present.

Package verification must:

- Verify checksums.
- Verify `dist/<project>-<version>-CHECKSUMS` is present and is the only active checksum manifest for the release.
- Verify every checksum-listed artifact exists under `dist/`.
- Verify every checksum-listed artifact validates successfully.
- Verify every release-looking artifact under `dist/` is listed in the checksum manifest unless explicitly ignored by a documented rule.
- Verify no stale release artifact for a different version remains under `dist/`.
- Verify deprecated checksum files such as `SHA256SUMS` are absent unless the project explicitly preserves them as compatibility artifacts.
- Verify `gh release create` arguments are derived from checksum-listed files only, never from a `dist/` glob.
- Verify archive layout and single root.
- Verify required headers, libraries, CMake config, pkg-config file, docs, examples, and metadata.
- Verify forbidden bundled headers and libraries are absent unless explicitly part of the artifact contract.
- Verify install-tree CMake consumers for static and shared imported targets.
- Verify pkg-config consumers when pkg-config metadata is shipped.
- Verify example builds from installed examples when examples are shipped.
- Verify shared-library runpaths use relocatable paths and do not contain build roots.
- Verify every shipped ELF executable and shared object that has runtime library lookup metadata uses `$ORIGIN`-relative RPATH/RUNPATH only.
- Verify no shipped Mach-O dynamic library or executable contains local build paths in install names or dependency paths; project-owned Darwin install names should be `@rpath`-relative.
- Verify no sanitizer runtime, sanitizer symbols, debug-only paths, generated service state, package-manager build state, credentials, VCS metadata, dependency caches, or local paths appear in artifacts.

Per-target SDK smoke contract:

- `package-verify` must extract each `dist/<project>-<version>-<target-id>.tar.gz` into a temporary directory and test the extracted SDK, not only the staging tree.
- Assert the extracted archive has exactly one root: `<project>-<version>-<target-id>/`.
- Assert `include/`, `bin/`, `lib/`, `lib/cmake/`, `lib/pkgconfig/`, and `share/` contents match the binary SDK archive contract.
- Use `file`, compiler target metadata, or target-specific inspection tools to verify shipped libraries and binaries match `<target-id>` when tooling is available.
- Compile a minimal C consumer against extracted public headers.
- Configure and build a minimal CMake consumer with `find_package(<project> CONFIG REQUIRED)`.
- Configure and build a minimal pkg-config consumer when `lib/pkgconfig/<project>.pc` is shipped.
- Link static and shared consumers when both static and shared libraries are shipped.
- Run consumers and shipped CLI binaries only when executable on the host or through an explicitly supported runner. Otherwise, build/link smoke checks are enough for cross targets.
- For CLI binaries, run `--version` or the project equivalent when execution is supported.
- Verify runtime paths after extraction.
- For Darwin artifacts, inspect install names and dependency paths with `otool` when available, and run Darwin smoke bundles only when the required runtime/toolchain exists.

Runtime path invariant:

- Shipped Linux and other ELF artifacts must never contain absolute RPATH/RUNPATH entries, dependency cache paths, build directories, source directories, temporary directories, or workstation-local paths.
- Prefer no RPATH/RUNPATH when the artifact does not need one.
- When runtime lookup metadata is needed, use `$ORIGIN` or `$ORIGIN/<relative-lib-dir>` so the artifact is relocatable inside the extracted SDK.
- Package verification must inspect every shipped executable and shared object with `readelf -d` or an equivalent tool and fail on absolute paths, local paths, sanitizer runtime dependencies, or non-relocatable runpaths.
- Do not rely on CMake defaults for this. Set install/build RPATH policy explicitly for shipped targets and test the installed package tree, not only the build tree.


## Source And Single-Header Artifacts

Source archives must be staged from an explicit manifest, not from an unfiltered repository copy. Include source, public headers, CMake, scripts needed to build from source, tests or smoke tests, examples, docs, license, version file, and release manifest. Exclude generated output.

Single-header artifacts, when present, must be generated from public header and implementation parts, formatted, version-stamped from the release version, compressed under `dist/`, and verified by decompressing and checking version macros plus a compile smoke test.


## Vendored Upstreams

If a project patches or builds a vendored upstream, put it behind a named lifecycle extension:

```text
vendor/<name>/upstream/
vendor/<name>/patches/
vendor/<name>/patches/series
```

Expose:

- `make vendor-<name>`
- `make vendor-<name>-apply`
- `make vendor-<name>-status`
- `make vendor-<name>-upgrade`
- `make build-<name>`
- `make verify-<name>-patches`

Rules:

- Apply patches only to a clean upstream checkout.
- `status` prints upstream revision, branch, and patch count.
- `upgrade` refuses dirty upstream state.
- Verification clones or copies the upstream to a temporary generated directory, applies the patch series, and builds it there.
- Vendored build output must not leak into release artifacts.


