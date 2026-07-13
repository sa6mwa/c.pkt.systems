# Dependencies And Provenance

## Dependencies

Projects consume external C dependencies as SDK bundles from `c.pkt.systems`. The lifecycle must support:

- Host debug dependency root.
- Host release dependency root.
- Cross dependency roots keyed by target ID.
- Separate dependency build roots and install roots.
- A shared verified archive cache plus repository-local disposable dependency state under `.cache/`.
- Clear dependency provenance in build metadata or manifests.
- Explicit failures for missing bundles, unsupported targets, checksum failures, or unavailable network fetches.
- Reuse downloaded SDK bundles and per-target dependency install roots across debug, release, hardening, e2e, fuzz, benchmark, and package builds.
- Bundled SDK mode, host dependency mode, and conservative auto mode when a project benefits from all three.

## Upstream Components

The usual upstream project owner for pkt.systems C lifecycle dependencies is `github.com/sa6mwa/`. Unless a project explicitly documents another source, fetch dependency archives from each upstream project's GitHub Releases page, not from source checkouts, branch archives, local sibling repositories, package-manager mirrors, or generated artifacts copied between worktrees.

Known upstream components:

| Component | Upstream project | Release archive source |
| --- | --- | --- |
| `lonejson` | `https://github.com/sa6mwa/lonejson` | `https://github.com/sa6mwa/lonejson/releases` |
| `cai` | `https://github.com/sa6mwa/cai` | `https://github.com/sa6mwa/cai/releases` |
| `libpslog` | `https://github.com/sa6mwa/libpslog` | `https://github.com/sa6mwa/libpslog/releases` |
| `c.pkt.systems` | `https://github.com/sa6mwa/c.pkt.systems` | `https://github.com/sa6mwa/c.pkt.systems/releases` |
| `lonehash` | `https://github.com/sa6mwa/lonehash` | `https://github.com/sa6mwa/lonehash/releases` |
| `liblockdc` | `https://github.com/sa6mwa/liblockdc` | `https://github.com/sa6mwa/liblockdc/releases` |
| `vectis` | `https://github.com/sa6mwa/vectis` | `https://github.com/sa6mwa/vectis/releases` |
| `libpid0` | `https://github.com/sa6mwa/libpid0` | `https://github.com/sa6mwa/libpid0/releases` |
| `liblql` | `https://github.com/sa6mwa/liblql` | `https://github.com/sa6mwa/liblql/releases` |

## Shared Verified Archive Cache

All external dependency archives use the shared cache below, regardless of whether they are pkt.systems SDK bundles or third-party sources:

```sh
${CPKT_DEPENDENCY_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/c.pkt.systems/deps}
```

This is a sibling of the toolchain cache's default `.../c.pkt.systems/toolchains` root. `CPKT_DEPENDENCY_CACHE` may override only the dependency archive cache; it must not redirect toolchains, and `CPKT_TOOLCHAIN_CACHE` must not redirect dependencies.

Expose `CPKT_DEPENDENCY_CACHE` as a CMake `PATH` cache variable. Resolve it once, with an explicit `-DCPKT_DEPENDENCY_CACHE=...` taking precedence over the environment variable, then `XDG_CACHE_HOME`, then `$HOME/.cache`; do not rediscover it independently in each dependency declaration.

Use this layout:

```text
deps/
  archives/sha256/<expected-sha256>/<archive-name>
  locks/<expected-sha256>.lock
```

- The global cache holds verified immutable archives only. Do not put extracted trees, CMake build directories, install prefixes, package-manager state, generated headers, or dependency stamps there.
- Keep extraction, build, install, and identity-stamp state under the consuming repository's `.cache/`, normally keyed by target ID and dependency identity. A project may delete that local state freely; the next dependency acquisition must reuse the verified global archive without network access.
- Key each archive by its required SHA-256. Archive names are for diagnostics only; never accept an archive because its filename, component name, version, or URL happens to match.
- Before every cache reuse, calculate SHA-256 and compare it to the dependency's pinned expected digest. A corrupt entry is not a cache hit.
- Serialize writers with a per-digest lock. The lifecycle serializes project operations, but independent downstream repositories can acquire the same shared cache concurrently.
- On a miss or corrupt entry, download to a uniquely named temporary file in the archive's final cache directory, verify the expected SHA-256, then atomically rename it to the final path. Never publish a partial download. Remove only the temporary or corrupt archive entry covered by the held lock.
- If a verified cache entry cannot be reused and the download fails, report the component, URL, expected digest, cache path, and download failure. Do not silently substitute a host package, a differently named file, an unpinned URL, or an unchecked archive.
- `make clean`, `make clean-dist`, `make prerelease`, `make release`, package targets, and normal dependency-clean targets must never remove this global archive cache. A project may offer an explicitly named, opt-in cache-prune command, but it must state that it affects all local pkt.systems projects sharing the cache and must never be a prerequisite of another lifecycle target.
- Never embed this global path in dependency manifests, generated CMake or pkg-config metadata, binaries, scripts, source archives, binary SDKs, or release artifacts.

Implement acquisition behind one project-owned CMake helper, for example `project_acquire_verified_archive()`, rather than letting each `FetchContent`, `ExternalProject`, or custom dependency builder download independently. The helper must accept a component identity, HTTPS URL, expected SHA-256, and output archive path. It must serialize writers with CMake `file(LOCK ...)`, hash an existing archive, download with `file(DOWNLOAD ... TLS_VERIFY ON)` only when needed, hash the temporary file explicitly against the expected digest, and use same-directory `file(RENAME ...)` publication. Do not rely on `EXPECTED_HASH` for this helper: a transfer or hash failure can terminate CMake before the helper removes its temporary path or tries a fallback URL. Consumers then extract or stage the returned archive into their repository-local `.cache/` tree. `FetchContent` and `ExternalProject` may consume that staged local result, but their default build-tree download cache is not the lifecycle cache.

Add executable cache-contract tests that prove: an initial miss downloads and publishes only a verified archive; deleting local extracted dependency state permits an offline cache hit; a corrupt cached archive is rejected and never extracted; concurrent acquisition does not expose a partial archive; `make clean` preserves the global archive; and package/privacy checks reject global-cache paths in released output.

Rules for component downloads:

- Pin each release dependency by component name, version, target ID when target-specific, exact GitHub release asset URL, and SHA-256.
- Prefer release assets whose names encode component, version, and target ID. Source code archives generated automatically by GitHub are not SDK bundles unless the project explicitly declares them as supported release artifacts.
- Reuse the same release URL and SHA-256 through every lifecycle surface that consumes the dependency: debug, release, hardening, fuzz, benchmark, e2e, package, and downstream smoke tests.
- Dependency fetchers must fail with an actionable diagnostic when a component has no pinned release URL for the requested target, when the URL is not under the declared upstream release page, when the checksum is missing, or when checksum verification fails.
- Dependency manifests in released SDKs must record the exact release asset URL used for each bundled component, but must not record local cache paths or source checkout paths.

Rules:

- Do not vendor generated dependency installs into release source.
- Dependency cache reuse is the default. Do not re-download a verified global archive or rebuild a local dependency root when the requested dependency identity has not changed.
- When dependency identity changes, invalidate only the relevant local `.cache/` extraction, build, and install roots through `make clean` or a narrower documented dependency-clean target. Retain the global archive cache; it is keyed by the expected SHA-256 and remains safe for any project that still pins that artifact.
- Dependency identity includes dependency name, version, target ID, source URL, SHA-256, toolchain file, ABI-relevant build options, and cache layout version.
- `scripts/deps.sh` should detect stale dependency roots when possible by comparing requested dependency identity to a cached manifest or stamp. On mismatch, fail with an actionable stale-cache diagnostic or refresh through the documented clean path.
- Do not leak dependency cache paths into package metadata, CMake config files, pkg-config files, binaries, scripts, or release archives.
- Imported CMake targets and pkg-config metadata must expose only the public dependency contract needed by downstream consumers.
- Static SDKs may require downstream consumers to provide dependency include and library roots; encode that clearly in CMake package config, pkg-config metadata, tests, and README examples.
- When a binary SDK bundles static third-party archives, the SDK owns the full static link interface for those archives. Ship relocatable CMake package configs and/or pkg-config files for every bundled library that downstream projects may link directly or transitively.
- Static imported targets must put private system and bundled-library requirements on the dependency target that needs them, not on downstream consumers or aggregate top-level targets. Examples: OpenSSL crypto owns `${CMAKE_DL_LIBS}` where DSO/dlfcn support needs it; libraries that use math own `m` where required; compression users declare zlib; thread users declare `Threads::Threads`.
- Prefer upstream-compatible CMake target names for bundled dependencies, such as `OpenSSL::SSL`, `OpenSSL::Crypto`, `CURL::libcurl`, `ZLIB::ZLIB`, `Libssh2::libssh2`, `nghttp2::nghttp2`, and `LibXml2::LibXml2`. Add project-namespaced aliases only when they clarify a bundle-specific variant without replacing the standard consumer path.
- Pkg-config metadata for static consumers must use `Requires.private` for dependencies that also ship `.pc` files and `Libs.private` for private system libraries or linker flags. Do not make consumers spell out `-ldl`, `-lm`, `-lz`, `-pthread`, framework flags, or similar workaround closures when those are requirements of bundled dependencies.
- Shared SDKs must not require bundled private static archives unless the project deliberately ships them as part of the artifact contract.
- Do not implement bespoke JSON parsers, serializers, tokenizers, compactors, escaping logic, or JSON framing code when the project declares `lonejson` as a dependency. `lonejson` owns all JSON parsing, serialization, validation, streaming, framing, escaping, and fixture normalization surfaces.
- If a project has legacy ad hoc JSON handling, migrate it behind `lonejson` during lifecycle consolidation and add tests that prove behavior is preserved.
- If the needed JSON behavior cannot be implemented through `lonejson`, stop and flag it to the engineer. Propose a change request for adding the missing capability to `lonejson` instead of implementing bespoke JSON behavior in the consuming project.
- Do not implement bespoke parsers, serializers, tokenizers, compactors, escaping logic, or framing code for a data format when the project declares a lifecycle component that owns that format.
- If a project has legacy ad hoc structured-data handling covered by a lifecycle dependency, migrate it behind that dependency during lifecycle consolidation and add tests that prove behavior is preserved.
- If the needed structured-data behavior cannot be implemented through the declared lifecycle dependency, stop and flag it to the engineer. Propose a change request for adding the missing capability to the owning dependency instead of implementing bespoke behavior in the consuming project.
- Exceptions require explicit engineer approval and must be documented as a narrow non-JSON-parser use case, such as fixed test fixture text or protocol examples that are not parsed or serialized by project code.
- Do not implement a bespoke structured logging subsystem when the project declares a pkt.systems logging dependency. Put logging behind a narrow adapter, keep it optional at the public API boundary, and test that disabling logging removes side effects.
- Logging dependencies must not leak into public headers unless the project deliberately accepts that type as part of the API. When a logger handle is accepted publicly, forward-declare it where possible and document that ownership stays with the caller.
- Host dependency modes must validate ABI-sensitive dependencies, not just headers. Wrong-ABI or partial host installs must fail with actionable diagnostics or fall back to bundled SDK mode when auto mode is explicitly supported.
- Package metadata must record logical dependency identity and ABI requirements, not local cache paths. CMake package config and pkg-config metadata should explain how static consumers supply external dependencies.
- For `c.pkt.systems` dependency bundles specifically, package metadata is part of the SDK product surface: every bundled library intended for downstream consumption must have CMake and pkg-config metadata in the released archive, and downstream pkt.systems projects should consume those targets instead of raw archive paths.
- Bundled SDK mode must pin per-target URL and SHA-256 for every dependency archive used by release builds.
- Downloads must use the shared verified archive cache: hash an existing global archive before reuse, retry transient download failures a bounded number of times, hash a temporary download before atomic publication, and extract only into a repository-local target-specific dependency root.
- Auto mode may choose host dependencies only when all required headers, libraries, package metadata, and ABI checks pass. Partial host installs must not shadow a valid bundled SDK configuration.
- Host mode must not require bundled SDK checksums for unsupported host target IDs.
- Deprecate old dependency-mode names with warnings and force them to the current spelling; remove the compatibility alias only as an explicit breaking change.

Dependency provenance contract:

- When dependency provenance matters, ship a logical dependency manifest under `share/<project>/dependencies.json` inside each binary SDK archive.
- The manifest records dependency name, version, target ID, source system, source URL, SHA-256, archive name, license identifier, bundled/external status, and the logical install role used by the package.
- Manifest paths must be logical or artifact-relative. They must not contain local cache roots, source roots, build roots, temporary directories, or workstation-local paths.
- If a dependency is bundled in the SDK, include its license text or required notices under `share/<project>/licenses/` or `share/doc/<package>/licenses/`, and mark it as bundled in the manifest.
- If a dependency is an external requirement for static consumers, mark it as external in the manifest and ensure CMake/pkg-config metadata and README examples describe how the downstream consumer provides it.
- Package verification must validate the dependency manifest when present, verify it contains no local paths, and verify it agrees with CMake package and pkg-config metadata.
- Installed CMake config files should include helper validation modules needed by downstream consumers, such as ABI probes for dependency SONAME/install-name checks.
- Installed pkg-config files should use relocatable `prefix=${pcfiledir}/...` derivation that works for both `lib` and multiarch libdirs.
- In host dependency mode, build-tree metadata may include host hints needed for local tests. Redistributable binary SDK metadata must not contain local build/cache/source paths. If a host-only artifact deliberately records host paths, it is not a portable SDK and must be named and verified as such.

License and provenance contract:

- Every release artifact must include the project license in the appropriate artifact-local documentation location.
- Binary SDK archives include the project license under `share/doc/<package>/LICENSE`.
- Source archives include the project license at the archive root.
- Lua source packages and source rocks include the project license, version, release manifest, rockspec template or rendered rockspec, Lua sources, Lua C binding/facade code, required public C headers, and any license text required by included source.
- If a C SDK bundles third-party dependency headers, libraries, tools, or data, include required license and notice text for each bundled dependency under `share/<project>/licenses/` or `share/doc/<package>/licenses/`.
- If a dependency is only an external static-consumer requirement, list it as external in the dependency manifest. Do not copy its license into the C SDK unless redistribution or included source requires it.
- Source archives and Lua source packages must be staged from explicit inclusion manifests.
- Package verification must fail when the project license is missing, a bundled dependency license or notice is missing, the dependency manifest marks a dependency as bundled but no corresponding license/notice exists, generated/private files are included, or license/provenance metadata contains local paths.
