# Dependencies And Provenance

## Dependencies

Projects consume external C dependencies as SDK bundles from `c.pkt.systems`. The lifecycle must support:

- Host debug dependency root.
- Host release dependency root.
- Cross dependency roots keyed by target ID.
- Separate dependency build roots and install roots.
- Cacheability under `.cache/`.
- Clear dependency provenance in build metadata or manifests.
- Explicit failures for missing bundles, unsupported targets, checksum failures, or unavailable network fetches.
- Reuse downloaded SDK bundles and per-target dependency install roots across debug, release, sanitizer, e2e, fuzz, benchmark, and package builds.

Rules:

- Do not vendor generated dependency installs into release source.
- Dependency cache reuse is the default. Do not re-download or rebuild dependencies for every target when the requested dependency identity has not changed.
- When dependency identity changes, invalidate the relevant `.cache/` dependency roots through `make clean` or a narrower documented dependency-clean target before rebuilding.
- Dependency identity includes dependency name, version, target ID, source URL, SHA-256, toolchain file, ABI-relevant build options, and cache layout version.
- `scripts/deps.sh` should detect stale dependency roots when possible by comparing requested dependency identity to a cached manifest or stamp. On mismatch, fail with an actionable stale-cache diagnostic or refresh through the documented clean path.
- Do not leak dependency cache paths into package metadata, CMake config files, pkg-config files, binaries, scripts, or release archives.
- Imported CMake targets and pkg-config metadata must expose only the public dependency contract needed by downstream consumers.
- Static SDKs may require downstream consumers to provide dependency include and library roots; encode that clearly in CMake package config, pkg-config metadata, tests, and README examples.
- Shared SDKs must not require bundled private static archives unless the project deliberately ships them as part of the artifact contract.
- Do not implement bespoke JSON parsers, serializers, tokenizers, compactors, escaping logic, or JSON framing code when the project declares `lonejson` as a dependency. `lonejson` owns all JSON parsing, serialization, validation, streaming, framing, escaping, and fixture normalization surfaces.
- If a project has legacy ad hoc JSON handling, migrate it behind `lonejson` during lifecycle consolidation and add tests that prove behavior is preserved.
- If the needed JSON behavior cannot be implemented through `lonejson`, stop and flag it to the engineer. Propose a change request for adding the missing capability to `lonejson` instead of implementing bespoke JSON behavior in the consuming project.
- Exceptions require explicit engineer approval and must be documented as a narrow non-JSON-parser use case, such as fixed test fixture text or protocol examples that are not parsed or serialized by project code.

Dependency provenance contract:

- When dependency provenance matters, ship a logical dependency manifest under `share/<project>/dependencies.json` inside each binary SDK archive.
- The manifest records dependency name, version, target ID, source system, source URL, SHA-256, archive name, license identifier, bundled/external status, and the logical install role used by the package.
- Manifest paths must be logical or artifact-relative. They must not contain local cache roots, source roots, build roots, temporary directories, or workstation-local paths.
- If a dependency is bundled in the SDK, include its license text or required notices under `share/<project>/licenses/` or `share/doc/<package>/licenses/`, and mark it as bundled in the manifest.
- If a dependency is an external requirement for static consumers, mark it as external in the manifest and ensure CMake/pkg-config metadata and README examples describe how the downstream consumer provides it.
- Package verification must validate the dependency manifest when present, verify it contains no local paths, and verify it agrees with CMake package and pkg-config metadata.

License and provenance contract:

- Every release artifact must include the project license in the appropriate artifact-local documentation location.
- Binary SDK archives include the project license under `share/doc/<package>/LICENSE`.
- Source archives include the project license at the archive root.
- Lua source packages and source rocks include the project license, version, release manifest, rockspec template or rendered rockspec, Lua sources, Lua C binding/facade code, required public C headers, and any license text required by included source.
- If a C SDK bundles third-party dependency headers, libraries, tools, or data, include required license and notice text for each bundled dependency under `share/<project>/licenses/` or `share/doc/<package>/licenses/`.
- If a dependency is only an external static-consumer requirement, list it as external in the dependency manifest. Do not copy its license into the C SDK unless redistribution or included source requires it.
- Source archives and Lua source packages must be staged from explicit inclusion manifests.
- Package verification must fail when the project license is missing, a bundled dependency license or notice is missing, the dependency manifest marks a dependency as bundled but no corresponding license/notice exists, generated/private files are included, or license/provenance metadata contains local paths.


