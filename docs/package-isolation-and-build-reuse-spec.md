# Package isolation and build reuse specification

## Status and scope

This is an implementation specification, not a description of functionality
already delivered. It records the agreed architecture and reviews the work
needed against the repository at release `v0.11.0`, commit
`b786205fa7b5501b356676b4808f619b0d316492`.

Keep one repository, one coordinated version, and one lifecycle. Produce three
binary SDK packages: **core**, **db**, and **misc**. Make their ownership a real
build, test, installation, and packaging boundary.

The intended result is that a db or misc change can be built and verified using
an existing verified core, without rebuilding core, rerunning core-owned tests,
or changing the other optional group's generated state. A full clean release
continues to verify every group and every supported target.

This document includes the dependency reuse and lead-time improvements discussed
before the package split, adapted to the new boundaries. It does not authorize
an upstream ABI transition, a version bump, release publication, or changes in
adjacent repositories.

## 1. Decisions and invariants

1. There is one repository and release version; these are not three independent
   release lifecycles.
2. Core is independently usable. Db requires core. Misc requires core. Db and
   misc must not require each other, including through exported metadata,
   installed examples, public headers, generators, or verification helpers.
3. Shared dependencies belong to core. A dependency used only inside one
   optional group may remain in that group.
4. Each package provides its complete supported surface: public headers,
   static libraries, shared libraries and their symlinks, metadata, notices,
   and applicable examples. No static-only or shared-only SDK variants.
5. Group development operations consume verified prerequisites. They do not
   silently build or test other groups to repair missing prerequisites.
6. Reuse depends on exact relevant inputs and successful verification, not on
   file existence, a version string, or an unchanged Git commit identifier.
7. The clean candidate `make release` rehearsal and the clean tagged
   `make release` both remain required. The proposal to replace the clean
   candidate rehearsal with incremental `prerelease` was rejected.
8. Both clean release runs exercise the complete coverage contract. Reuse
   within a run avoids repeating identical compilation; it does not replace
   required debug/release facade builds or artifact verification.
9. Keep all seven shipped targets, existing QEMU coverage, native memory and
   fuzz checks, source reconstruction, and native Darwin verification.
10. Use the build system's configured job limits. More host cores do not
    authorize higher job counts or concurrent top-level operations.
11. Preserve digest-verified archive caching and zero network requests on a
    verified digest hit. Keep compiled state repository-local and disposable.
12. Cmocka becomes a shipped core dependency, including downstream discovery
    and usage support; it is no longer only a private Linux test dependency.

The supported targets remain `x86_64-linux-gnu`, `x86_64-linux-musl`,
`aarch64-linux-gnu`, `aarch64-linux-musl`, `armhf-linux-gnu`,
`armhf-linux-musl`, and `arm64-apple-darwin`. Keep Linux's pinned Bootlin
collections, local osxcross SDK selection, host LLVM/clangd/clang-format,
native host Valgrind, and pinned AFL++ ownership unchanged.

## 2. Baseline evidence and interpretation

The recorded successful v0.11.0 candidate rehearsal took 9,417.41 seconds
(2h36m57s). The successful tagged run took 9,126.88 seconds (2h32m07s).
Together they consumed 5h09m04s of local execution. These two clean runs are
retained by design.

Selected measured portions of the tagged run:

| Work | Duration | Interpretation |
| --- | --- | --- |
| Seven release-target builds | 69m14s | Compilation is the largest measured opportunity. |
| OpenSSL build steps in those seven builds | 45m54s | Subset of release build time; other dependency work overlaps within each target. |
| Six Linux release CTest suites | 20m55s | Includes both native x86_64 suites and four ARM suites. |
| Four ARM QEMU suites | 13m46s | Subset of the Linux suite time. |
| Debug CTest | 4m28s | Excludes the debug build. |
| Native Memcheck CTest | 7m26s | Excludes its database e2e and configure/build preparation. |
| Source-reconstruction CTest | 4m51s | Excludes reconstruction's independent build. |

These rows are not an exhaustive additive breakdown. The logs do not provide
precise boundaries for every phase, especially source rebuilding and extracted
consumer verification. Add instrumentation before making claims about total
time saved. Retain transient timings and logs under ignored `build/`, not as
date-stamped audits committed into the source tree.

Within one `make release`, the four full ARM CTest suites run once each.
Extracted-package QEMU consumers provide different evidence: they prove the
delivered SDK rather than the build tree. Retain both. Plain database e2e and
database e2e under Valgrind likewise prove different properties.

Confirmed smaller repetitions include early matrix fixtures repeated in the
later suites, debug clangd coverage plus the explicit clangd invocation, and
two source-archive generations before one reconstruction. The debug and native
release graphs also rebuild dependencies in the same target roots. Do not
assume their root cause is known until contract and ExternalProject/Ninja
stamp behavior have been traced.

## 3. Package ownership

### 3.1 Component assignment

| Group | Upstream components | Project public surfaces |
| --- | --- | --- |
| core | OpenSSL, zlib, curl, nghttp2, libssh2, libxml2, Lua, MQTT-C, MIT Kerberos, Cyrus SASL, OpenLDAP, cmocka | Corresponding direct APIs; `cpkt_openssl`, `cpkt_nghttp2`, `cpkt_libssh2`, `cpkt_mqttc`, `cpkt_lua`, `cpkt_lua_runtime`, `cpkt_gssapi`, `cpkt_sasl`; any required cmocka C89 facade. |
| db | PostgreSQL client libraries, including libpq OAuth support; SQLite; iODBC manager and configuration libraries | `cpkt_postgres`, `cpkt_sqlite`, native supported iODBC APIs. |
| misc | open62541, libHaru, libpng, miniaudio, whisper.cpp and embedded ggml | `cpkt_opcua` and generated public types/plugins/constants, `cpkt_pdf`, public libpng API, `cpkt_audio`, `cpkt_sus`. |

PostgreSQL server binaries are not introduced by this split. IODBC remains a
driver manager/configuration API; database-specific ODBC drivers are not added.
Libpng remains with libHaru because the current SDK has no cross-group consumer
requiring it. Move it to core if a future supported cross-group dependency
requires it; do not duplicate it in two packages.

Kerberos, SASL, and OpenLDAP belong to core, so db does not need misc for its
authentication closure. Audio and speech can use core curl/TLS while remaining
independent of db. Cmocka is available to every group's tests through core, but
production facade libraries must not acquire a runtime cmocka dependency.

### 3.2 Authoritative inventory

Introduce a single declarative, source-controlled inventory consumed by CMake,
Make-facing helpers, test selection, staging, and verification. It must name:

- Component/group ownership and direct component dependencies.
- Recipe inputs and shared helper dependencies.
- Facade, generator, example, and tooling ownership.
- Expected installed files, library variants, export policies, and notices.
- Tests, execution modes, and required test executables or fixtures.
- Group-to-core prerequisite requirements.

Use this inventory to generate or validate all other lists. Do not maintain
independent handwritten group maps in the packager, smoke verifier, and CI.
Require explicit ownership for new targets/tests/files and detect omissions.
Dependency edges must form a DAG with no core-to-optional or db-to-misc edge.
Test-only usage of cmocka is an explicit edge distinct from production linkage.

## 4. Build graph and generated state

### 4.1 Ownership and layout

Keep repository-local dependency roots keyed by target and component, preserving
the lifecycle's existing path convention:

```text
.cache/deps-build/<target>/<component>/
.cache/deps/<target>/<component>/install/
.cache/dependency-contracts/<target>/
build/<target>/<group>/<configuration>/
build/verification/<target>/<group>/
build/package-stage/<target>/<group>/
build/devenv/
```

This is a proposed owned-build layout; migration must update callers of the
current `build/debug` and `build/<target>-release` paths coherently. Do not leave
parallel legacy graphs or compatibility symlinks unless an external commitment
requires them. Component roots must not acquire semantic hash/version suffixes;
store identity inside contract records.

Each dependency component has one producer graph for a given effective recipe
and target. Debug, release, package consumers, and group consumers import its
matching install outputs. They must not each register an independent producer
against the same ExternalProject stamps/source directory.

Repo-owned debug and release facades retain distinct compiler flags and builds.
Instrumented variants have distinct effective contracts and owned state; do not
pretend AFL instrumentation and ordinary dependency output are interchangeable.
Preserve the existing explicit borrowing of ordinary dependencies by applicable
facade-only hardening configurations.

Selected CMake configurations must not probe unrelated db/misc prerequisites,
register their generators, import their public targets, or require their files
at configure time. A filtered build target in an otherwise mandatory all-SDK
configuration does not meet the isolation requirement. Common configuration
code must be separated from group-specific registration and validated with the
other group's install/build directories deliberately absent.

### 4.2 Selected group behavior

For a db-only operation:

1. Resolve the selected target/toolchain and expected core input contracts.
2. Validate existing core outputs and verification evidence without generating
   core producer rules or rewriting core-owned state.
3. Refuse the operation if core is absent, stale, incomplete, or unverified.
   Report the reason and exact core command needed for that target.
4. Configure/build changed db components and their db-owned dependency closure.
5. Run the selected db tests and db integration/package consumers.
6. Leave misc state unchanged. Apply the symmetric rule to misc-only operations.

Reading/hashing core headers, libraries, manifests, and receipts is permitted.
Creating db-local imported targets or db-local consumer scratch is permitted.
Re-extracting core sources, rebuilding/reinstalling core, updating its stamps or
receipts, or running core-owned tests is forbidden in an optional-only operation.

A PostgreSQL TLS/GSSAPI test exercises existing core as part of db integration;
that does not select the standalone OpenSSL/GSSAPI suites. Report its db
ownership explicitly. Toolchain discovery may read verified cached tools; an
optional-only command must not secretly provision missing prerequisites.

Changing a core input makes existing core unusable for selected optional work.
Fail with an actionable prerequisite diagnostic rather than widening scope.
An explicitly requested core/all workflow computes the affected downstream
closure, rebuilds changed producers, and reruns affected integration checks.

### 4.3 Cleanup and isolation

Group cleanup removes only that group's owned build, dependency, install,
verification, and staging state. It does not remove other groups, shared archive
or toolchain caches, or running services owned by unrelated workflows. Removing
core explicitly invalidates optional verification that refers to that core.

Global `make clean` and both `make release` runs still remove all
repository-local generated state. They preserve the shared verified caches.
Service-owning operations stop their Podman pods before cleanup. Temporary
fixtures and consumers belong under `build/` and remain removable by the user.

## 5. Public command contract

Extend the existing lifecycle surfaces with one documented selector:
`GROUP=core|db|misc|all`. Do not add three competing lifecycle command families.
Keep existing unqualified commands' all-group scope and current target defaults.
Selected group operations use explicit `PRESET` where needed; the current
default `PRESET=debug` remains the native development choice.

Proposed usage after implementation:

```sh
make build GROUP=core PRESET=debug
make test GROUP=core PRESET=debug
make finalize-slice GROUP=db PRESET=debug
make valgrind GROUP=db PRESET=debug
make test GROUP=misc PRESET=aarch64-linux-gnu-release
make package GROUP=db PRESET=x86_64-linux-gnu-release
make package-verify GROUP=db PRESET=x86_64-linux-gnu-release
make clean GROUP=db
make release
```

These commands do not exist with these semantics yet. Specify in `make help`
which selected commands configure/build and which require existing outputs.
Tests requiring a prepared core must print the matching core preparation
commands. `package-verify GROUP=db` verifies db with the matching core and does
not invoke core's standalone consumer suite.

`finalize-slice GROUP=db` formats project-owned sources, runs db debug and hover
checks, and asserts global formatting cleanliness. Global formatting remains
required; it is not permission to build/test other groups. Shared tooling
changes need their applicable tooling tests separately. Db currently has no
AFL target; do not silently run Lua/OPC UA fuzzing for a db-only request. A
specifically requested unavailable group gate must report its unsupported
status; an aggregate selected workflow reports that gate as not applicable.

`make prerelease` remains the all-group incremental proof graph. `make release`
is always all-group and exhaustive: reject a narrowed `GROUP` or incompatible
target override before cleaning or starting expensive work. There is no
group-only release/publish command in this design.

## 6. Contracts and verification reuse

### 6.1 Distinguish three identities

1. **Archive identity:** pinned SHA-256 of downloaded bytes. A verified hit
   performs zero network requests regardless of filename or source tree.
2. **Build identity:** effective inputs that determine a component's compiled
   output and installation. A matching successful producer permits import.
3. **Verification identity:** test definition, applicable binaries, dependency
   identities, runtime/execution mode, and successful completion.

Keep these concepts separate. A verified source archive does not prove a build;
a successful build does not prove tests; an uninstrumented test does not prove
Memcheck; a build-tree consumer does not prove an extracted archive.

### 6.2 Build identity requirements

Include, where applicable:

- Source digest, pin, patches, code generators, schema inputs, generated source
  policy, and repo-owned compiled sources/public headers.
- The component's recipe and every output-affecting helper it calls.
- Toolchain collection identity, relevant compiler/binutils identities,
  sysroot, Darwin SDK selection, deployment target, ABI and target/libc.
- Effective compiler/linker flags, definitions, features, build variant, and
  static/shared requirements.
- Direct dependency contracts, transitively propagated.
- Installation/metadata generation inputs that affect shipped outputs.

Do not invalidate compiled outputs because another group's pin changed, a
documentation file changed, Git topology changed, or a release was signed.
Avoid hashing an entire central recipe file merely because it contains the
component recipe. Extracting recipe functions alone is insufficient unless
shared helper changes also propagate to every actual user.

Review the current broad preamble hashing and operational fields such as
download timeout/job count. Operational settings should not force a rebuild
when they provably do not affect output; retain them in execution diagnostics.
Validate contract-engine changes/migrations conservatively rather than blindly
accepting old records with different meaning. Never weaken provenance checks to
make a reuse test pass.

### 6.3 Verified core and completed producers

Store generated local evidence for each component/group with its input digest,
required output inventory/content digests, completed build/install status, and
successful required verification. Publish completion records atomically only
after success. Missing outputs, modified bytes, interrupted installation, or a
changed contract invalidate reuse. File timestamps alone are not proof.

Db/misc preparation requires verified core for the selected target and mode.
For native development this includes the applicable core debug/integration
checks; cross preparation includes selected core runtime coverage and metadata
checks. Record the actual coverage, including unsupported runner modes.
Ordinary release core must never borrow unverified/instrumented output.

Core identity is independent of the coordinated release version. Development
reuse must survive db-only changes in an untagged checkout. Reuse component
outputs only when their actual inputs match; version-bearing core facades and
package metadata have their own effective inputs. Changing a version that
affects those outputs invalidates them. Strict same-version archive composition
applies to distributed packages, not to a requirement that every development
core be rebuilt after an unrelated db source edit.

A core test-definition change invalidates verification evidence without
necessarily invalidating compiled core. Diagnostics must distinguish
"build inputs changed", "verification changed", and "outputs missing/corrupt"
so prerequisite repair does only the necessary work. Changes confined to db
verification do not revoke core verification.

### 6.4 Verification records and duplicate suppression

Records must cover the test/helper code and configuration, binary and dependency
digests, target runtime/sysroot, runner configuration, instrumentation mode,
relevant environment, tool versions, selected cases, and required fixtures.
Do not include absolute workstation paths in release payloads. Keep local
records/logs under ignored `build/`.

Limit automatic test deduplication initially to identical successful invocations
within the current proof run. Group development can rely on existing verified
core evidence, but selected group tests should execute normally; this is not a
general persistent test-skipping system. Changes in inputs or environment revoke
reuse. A failed, skipped, timed-out, or partial test never supplies success.

Removing duplicate execution must preserve the fail-fast position of early
fixtures. When a full suite suppresses an early fixture, require an exact
matching success record. Direct CTest invocation without such evidence must
still run the fixture. Reject missing required tests and missing executables.

## 7. Lead-time improvements adapted to the groups

### 7.1 Debug/release dependency reuse

Trace the native debug-to-release dependency rebuild in the existing logs and
generated graphs. Both currently use the same x86_64 component roots; upstream
dependency flags/build type are generally release-oriented, while facades use
their own debug/release modes. Compare actual commands/contracts, generated
download/configure scripts, stamps, and Ninja command-history behavior.

Implement the single-producer/import boundary and reuse matching dependency
outputs across core debug/release consumers. Do the same for db/misc components
where effective inputs match. Retain separate outputs whenever they differ.
Add a regression using a fresh consumer build directory and an already completed
producer, proving no dependency extraction/configure/build/install occurs.

### 7.2 OpenSSL serial build

`cpkt_add_openssl` currently uses `make -j1` because its comment records a
generated-assembly race with the pinned cross environment. Investigate the
original failure and reproduce it at the existing configured job limit.
Identify the missing dependency/order edge, patch it narrowly if appropriate,
and add a regression capable of exposing the race.

Only enable configured parallelism after clean static/shared production,
runtime behavior, symbols/ABI, packaging, and warning checks succeed across all
seven targets, including native Darwin on the exact candidate commit. Do not
disable assembly, algorithms, or supported features to hide the race. If no
safe fix is established, retain serial OpenSSL and report the measured limit.
Other top-level operations remain serialized.

### 7.3 Repeated fixtures, clangd, archive generation

- Classify the ten early Linux matrix fixtures and the Darwin Kerberos fixture
  by ownership and target dependence. Run target-sensitive compilation with
  every applicable compiler. Host-only identical tooling checks need not run
  once per architecture. Preserve all cases and early failure behavior.
- Remove second execution of already completed identical fixtures in the same
  proof run using the verification rules above.
- Give public hover checks group ownership and a selected compile database;
  db hover validation must not require building misc examples. Execute an
  identical debug clangd gate once per proof run. Source reconstruction and
  genuinely different compiler/header configurations remain separate evidence.
- Make the final graph generate the source archive once before verifying it.
  Preserve `package-source-smoke`'s useful standalone preparation behavior.
- Keep final artifact checks even when staged trees passed earlier. Reuse
  extraction scratch only after confirming the exact archive digest; do not
  treat stage verification as proof of the archive bytes.
- Avoid invoking the full package verifier twice through overlapping aliases
  in orchestration. Standalone aliases remain useful public entrypoints.

### 7.4 Source reconstruction and consumer scheduling

The current source verifier uses a fresh Unix Makefiles configuration and a
plain `cmake --build` without an explicit build-parallel policy. Propagate the
configured job limit and supported generator policy to nested builds; measure
the result rather than raising limits to match host core count.

Source reconstruction remains an independent all-component rebuild from the
actual source archive with no pre-existing compiled/install outputs. Reuse the
shared verified archive/toolchain caches only. Build group producers once in
the reconstructed tree, verify their consumers, and produce/check all package
groups as needed to prove the source distribution's packaging path. Forward
explicit cache/toolchain overrides without falling back to host compilers.

Schedule extracted consumers serially at the top level and honor configured
parallelism inside each build. Measure repeated CMake configuration/compilation
cost before consolidating consumers into fewer group-owned build graphs. Keep
isolated package-combination coverage so aggregation cannot mask dependencies.

### 7.5 Test/e2e duration

After the build/reuse work, profile tests by group, target, and execution mode.
The existing SQLite owned-source contract is approximately 54 seconds in a
native suite; inspect its production warning mutation and both-generator
coverage before attempting to reduce repeated compilation. OPC UA type tests
are about 20 seconds each natively and longer under QEMU/Valgrind; use their
existing per-phase timings to identify unnecessary setup/waits.

Replace fixed waits only when readiness/completion can be observed reliably.
Preserve timeout, truncation, interruption, allocation-failure, logging, TLS,
and ownership regressions. Keep static/shared and instrumented modes distinct.
Do not reduce iterations or drop coverage just to obtain a smaller number.
PostgreSQL/CockroachDB e2e remains db-owned, with success/failure/interruption
teardown and user-removable `build/devenv` state. Live speech/provider tests
remain explicit opt-ins. Group isolation must not start unrelated services.

## 8. Binary package and installation contract

### 8.1 Artifacts

For each existing target, produce:

```text
c.pkt.systems-<version>-core-<target>.tar.gz
c.pkt.systems-<version>-db-<target>.tar.gz
c.pkt.systems-<version>-misc-<target>.tar.gz
```

Use a common top-level directory, `c.pkt.systems-<version>-<target>/`, so
extracting selected archives into the same parent composes one SDK prefix.
Directory entries may overlap; regular files and symlinks have one owner.
Test both optional-group extraction orders. Group-specific staged trees cannot
silently overwrite core files.

Keep one all-source archive and one checksum manifest. Preserve the Darwin
smoke bundle, adapting it to the three groups and recording their identities.
There is no fourth monolithic binary SDK tarball in the planned cutover. With
seven targets, the expected upload set is 21 SDK archives, one source archive,
one Darwin smoke ZIP, and the checksum manifest itself: 24 assets unless a
separately approved artifact is added.

### 8.2 Metadata, file ownership, and notices

Each archive carries a group-specific package manifest under
`share/c.pkt.systems/packages/`, with group, release version, target/libc,
required core identity, component versions, and payload file inventory/digests.
Choose one documented machine-readable schema during implementation. It is
independently versioned; do not increment it automatically with release tags.

Core owns common project license/base documentation and shared SDK definitions.
Each group owns its component licenses, notices, package documentation and
examples in non-colliding paths. The complete source archive retains the full
dependency documentation/notices. Do not distribute a group's required notice
only in an optional archive that its consumer need not install.

Every archive also carries the project license in its own group documentation
directory, for example `share/doc/c.pkt.systems/db/LICENSE`. Equal license text
at distinct group-owned paths is permitted; overlapping regular-file payload
paths are not. This preserves the lifecycle's artifact-local license rule even
before sibling archives are installed.

Preserve public library ABI identities, installed API names, CMake target names,
and pkg-config names. Adapt paths/discovery to the shared prefix with lazy,
component-specific imports; loading a db package must not look for misc, and
loading misc must not look for db. Keep platform system libraries and static
C++ runtime closure correct. Retain exact facade export/import policies and
full public API coverage checks against installed headers/libraries.

Release combination checks require the same coordinated version and exact
target/libc, as well as the expected core contract identity. Wrong/missing core
must fail clearly. Ordinary tar extraction cannot enforce this: provide
manifest-aware SDK validation and hook it into CMake/pkg-config consumer
verification and lifecycle acquisition. Do not claim that raw pkg-config or tar
already validates sibling package versions. Determine how direct pkg-config
users receive equivalent required-core validation without inventing a second
installer protocol; document the supported entrypoint.

### 8.3 Consumer combinations

Verify these four installations from the exact distribution archives:

1. Core alone, with db and misc absent.
2. Core + db, with misc absent.
3. Core + misc, with db absent.
4. Core + db + misc, including both optional-group extraction orders.

Use isolated prefixes and prevent discovery of host/sibling packages. Test
static/shared CMake and pkg-config consumers, strict C89 headers, applicable
examples, runtime loading, export/import constraints, relocatability, static
PIC/C++ closure, and notices. Run Linux consumers directly or with their
configured QEMU/sysroot; native Darwin supplies runtime evidence.

For partial development verification, run only the selected group consumers
with imported core. In the full release, run complete core-only checks once;
reuse exact common fixture proofs where valid while retaining all four
installation combinations. Installed db/misc consumers exercise core linkage
without repeating unrelated standalone core cases.

Negative tests cover missing core, mismatched release/target/libc/core identity,
unexpected cross-group payload/dependencies, missing notices, duplicate files,
and discovery accidentally succeeding through a host or previously installed
optional group.

Partial development packaging must not overwrite the authoritative all-release
checksum manifest with an incomplete set. Keep scoped checksum/verification
records under `build/` and label their scope. Full release accepts only the
complete artifact inventory. An optional-only extracted-package check requires
an existing matching core archive and its evidence; if absent, it reports an
explicit core packaging prerequisite instead of generating/retesting core.
Reusing a compiled development core does not make a core archive from another
release version valid for a newly packaged db/misc archive.

## 9. Cmocka promotion to a shipped core dependency

The current pin is `2.0.2`. Preserve that pin initially; this feature is not an
instruction to upgrade it. The verified source archive's `LICENSE` is Apache
2.0, consistent with the permissive licensing policy. Include its required
license/notice material and record source/digest/version provenance.

The existing recipe is Linux test-only, builds `BUILD_SHARED_LIBS=OFF`, disables
upstream tests/examples, and exposes an internal `cpkt::cmocka` target. Current
package assertions explicitly forbid cmocka. Replace those policies coherently:

- Build/install static and shared cmocka for all seven targets, including
  Darwin, independently of `CPKT_BUILD_TESTS`.
- Export correct upstream-compatible CMake/pkg-config discovery for each
  variant, preserving required public definitions such as `CMOCKA_STATIC`.
- Ship public headers, all library symlinks/ABI metadata and required notices.
- Let repository db/misc tests consume the prepared core cmocka. Avoid a
  second private cmocka extraction/build under each group.
- Add extracted-core consumers exercising real assertions, setup/teardown,
  mocks/expectations, successful tests, and intentionally failing tests with
  expected nonzero exit/status. Cover static/shared discovery and all targets.
- Verify native Darwin behavior without silently retaining its exclusion.

The current upstream public header includes `stdbool.h`, `stdint.h`, and an
inline helper. Strict C89 usability is therefore an implementation gate, not
something proved by existing internal tests. Audit the full public API/macros
with actual strict-C89 consumers. If the native surface fails, supply the
required complete C89 facade/compatibility public surface while also preserving
the upstream headers/API for existing downstream cmocka users. Preserve source
locations, mock value widths on 32-bit targets, setjmp/longjmp behavior,
fixture ownership, and test result semantics. Do not globally weaken C89 flags
or hide a partial interface under a claim of complete support.

Do not link cmocka into production core/db/misc facades. Packaging it makes it
available to downstream test builds; it does not add a production requirement.
Exact facade export rules apply if project-owned shared facade code is added;
preserve upstream ABI/export identity for the upstream libraries.

## 10. Test ownership and regressions

Use inventory-derived group labels plus execution/property labels such as
memcheck, packaging, smoke, or facade. An integration test has a primary owner
and declared prerequisites. Shared tooling checks have their own ownership;
they do not require compiling all libraries just to run a parser/manifest test.
All-group registration must remain complete and fail if a required case is
missing, disabled, or lacks its executable.

Required behavioral regression matrix:

| Scenario | Required observation |
| --- | --- |
| Db pin, db recipe, db facade/header or db test changes | Core producer/test invocations are zero; misc outputs are unchanged; affected db checks run. |
| Misc pin/recipe/facade/generator changes | Core producer/test invocations are zero; db outputs are unchanged; affected misc checks run. |
| New consumer build directory imports completed core | No re-extract, configure, build, install, stamp refresh, or standalone core test. |
| Core recipe/patch/compiler/flags or required output changes | Optional-only operation fails before changing core; explicit preparation repairs only the affected closure. |
| Missing/corrupt core output, absent/failed verification or interrupted install | Reuse is refused with the component, target and corrective command. |
| Shared output-affecting helper changes | Every actual component user invalidates; unrelated components remain reusable. |
| Documentation or another group's pin changes | Unrelated component contracts remain unchanged. |
| Group cleanup | Only owned state disappears; caches and other groups survive. |
| Completed early fixture followed by full suite | Identical case executes once; changed inputs or standalone invocation execute it. |
| Debug versus release / plain versus Memcheck / build tree versus archive | Distinct required modes execute; only identical prerequisites are reused. |
| Verified archive digest hit, including source reconstruction | Zero network requests, even under renamed asset/URL or empty local extraction state. |
| Group archive composition | Exact file ownership, core-only and both optional combinations work; forbidden dependency leakage fails. |
| Cmocka production disabled tests configuration | Core still ships both cmocka variants; downstream pass/fail/mock behavior is correct. |
| Clean all-group release | Seven targets and all groups build; complete suites/packages/source evidence remain mandatory. |

Use real recipe/command fixtures, recorded invocations, and before/after content
and timestamp checks to falsify cross-group mutation. Synthetic fixtures belong
under `build/`; include representative real pinned-component transitions and
Ninja/Unix Makefiles where supported. Existing configure-only reuse coverage
does not prove absence of dependency builds or tests.

## 11. Release and native Darwin execution

Keep the existing release protocol: candidate review and clean rehearsal,
squash/sign decision, lightweight tag, clean tagged artifact generation,
verification, push, exact-commit native Darwin success, manifest-selected upload.
Do not modify release skill authority to remove either clean run.

Within each clean release run, build/verify core prerequisites once per effective
target contract, build db and misc against them, and run their assigned suites.
Retain full debug, release, Valgrind, fuzz smoke, database e2e, facade API/export,
all Linux QEMU, installed package, privacy and source-reconstruction coverage.
A receipt from the previous clean rehearsal cannot satisfy the tagged run.
Global clean deletes local build/test records before each run.

Native Darwin workflow changes must cover all group archives and their supported
combinations. Its current trigger list names `feat/postgres-client` and `trunk`;
implementation must ensure the actual candidate branch can trigger the workflow.
Preserve the configured native job limit and diagnostic artifact uploads. Local
osxcross builds are not substitutes for native runtime proof. Push and require
success on the exact implementation commit before declaring macOS-affecting
work complete, under the repository's existing authorization/gate rules.

Regenerate/check the final checksum manifest for the exact 23 payload artifacts
and upload it as the 24th asset. Verify server sizes/digests after publication.
Existing version/tag, privacy, loader, license and warning gates remain intact.

## 12. Repository review and required changes

The following findings come from current source inspection. Paths below are
implementation surfaces, not a claim that all needed files have been listed.

| Current surface | Finding | Required change |
| --- | --- | --- |
| `Makefile` | Lifecycle gates configure/build whole presets; no group selector. `finalize-slice` runs full debug and clangd. | Add consistent selection/precondition semantics and scoped verification while retaining all-group defaults and clean release. |
| `CMakeLists.txt`, `CMakePresets.json` | Central target/test/generator registrations and per-preset directories. Existing individual dependency targets offer a starting point. | Introduce inventory-derived group registration, independent producer/import graphs, group build state and complete all-group coverage. |
| `cmake/CpktDependencies.cmake` | Component recipes/contracts already exist, but all components are configured together; cmocka is conditional Linux/static-only. | Separate ownership and prerequisite evaluation; promote cmocka; ensure consumers do not register duplicate producers. |
| `cmake/CpktDependencyContract.cmake` | Per-component and transitive identities exist. Recipe preamble is broadly hashed; contracts include generator and operational settings. | Preserve good invalidation tests; refine effective identity/helper tracking and conservative evidence validation. Investigate shared-stamp behavior before claiming a rebuild fix. |
| `tests/dependency_reuse_configure_test.sh` | Checks successful configure with dependency building disabled. | Extend evidence with build/test invocation and cross-group mutation regressions. |
| `scripts/configure-preset.sh`, `scripts/fuzz.sh` | Fuzz setup has special dependency reuse and root overrides. | Preserve instrumentation distinctions; assign Lua fuzz to core and OPC UA fuzz to misc; scope consumers without damaging borrowed core. |
| `scripts/build.sh`, `scripts/test.sh`, `scripts/configured_build.py` | Preset selection and configured consumer paths assume current graphs. | Resolve groups/targets consistently; preserve pinned toolchain/runtime and explicit missing-prerequisite failures. |
| `scripts/package.sh` | Builds/tests/packages seven targets and repeats early fixtures in full suites. | Build producers once, package three inventories, retain target-sensitive preflight, deduplicate exact same-run fixture proof. |
| `cmake/package_bundle.cmake` | Copies a fixed list of all dependency installs and stages all public APIs/metadata into one archive. | Generate independent group stages from authoritative inventory and preserve composable common-prefix metadata. |
| `cmake/package_assertions.cmake` | Assumes complete monolithic payload and explicitly rejects cmocka. | Assert per-group payload/closure plus combinations; require cmocka in core and forbid its production linkage leakage. |
| `scripts/package-verify.sh`, `scripts/run-package-assertions.sh` | Construct one archive path per target and rerun helper regressions. | Resolve group archives/manifest, separate tooling from artifact checks, retain distinct final-byte verification. |
| `scripts/package-install-smoke.sh`, `scripts/run-package-consumers.sh` | Large all-SDK consumer graph and all-example expectations. | Group-specific consumers, isolated combination prefixes and negative closure tests; preserve static/shared/runtime evidence. |
| `examples/abi_smoke.c` | Mixes curl/Lua/TLS/XML with miniaudio/open62541 in one consumer. | Provide group-owned smoke examples; retain an all-installed composition check without requiring misc for core. |
| `scripts/verify-clangd-surface.sh` | Checks a broad public API/example inventory from one compile database. | Select owned surfaces/generated headers, preserve full coverage in the all-group gate, avoid duplicate identical debug invocation. |
| `scripts/package-source.sh`, `scripts/source-archive-verify.sh` | Single all-source archive; independent Makefiles rebuild; final matrix invokes source generation twice. | Keep one all-source archive, generate once, honor configured jobs, reconstruct all groups with shared archive cache and empty compiled state. |
| `scripts/verify-dist-manifest.sh`, version/manifest tests | Artifact patterns and expected inventory describe current monolithic SDKs. | Validate group inventory and exact complete/partial development sets without allowing partial sets to satisfy release. |
| `scripts/clean.sh`, Podman helpers | Global generated-state removal; db state already under `build/devenv`. | Add owned group cleanup and core-dependent evidence invalidation; retain teardown/global clean safety. |
| `.github/workflows/darwin-bundle.yml`, `tests/darwin_curl_package_test.sh` | Workflow/test expects one Darwin SDK tarball and mixed curl/OpenLDAP/Lua package consumers. | Verify three archives/combinations, cmocka and selected native group behavior; enable exact candidate branch execution. |
| `skills/pkt-systems-cmake-lifecycle/` | Authoritative source skill describes one SDK bundle and current generated-state paths. | Document group selection/acquisition/core requirements and reuse rules; preserve both clean release gates and shared cache policy. Update source, not installed skill. |
| `README.md`, `docs/dependencies.md`, `docs/lifecycle-migration.md`, notices | Current docs describe one binary bundle; README says cmocka is not shipped. Migration ledger has older graph descriptions. | Update implemented command/artifact/dependency policy and notices at cutover; keep this spec distinct from current user instructions until then. |

In particular, `scripts/test.sh` currently configures and builds before CTest;
it is not a test-only operation. Selection must constrain that build as well as
the subsequent cases. The early `CPKT_FACADE_ONLY` return in `CMakeLists.txt`
currently registers a Lua mock/fuzz surface; it is not a generic group-isolation
implementation and must not be treated as proof that db/misc can be isolated.
Current `cpkt_deps_<component>` targets are useful narrow producers, but some
select one library variant; group packaging must still prepare both variants.

Also search generated CMake/pkg-config helpers, source manifests, installed
examples, release-note templates, lifecycle fixtures and skill installation
scripts for archive/path assumptions. Downstream repositories consume the old
archive names; enumerate migration consequences but do not edit them without
explicit authorization.

## 13. Delivery sequence and acceptance

### A. Establish inventory and observability

Create ownership/dependency/test inventories and fail-fast structural fixtures.
Add phase timing for configuration, component producers, group tests, e2e,
archive staging, reconstruction and consumer checks. Record executed/reused
work with reasons under `build/`, including selected scope and coverage counts.

### B. Implement producer reuse and group development isolation

Fix dependency producer ownership and evidence validation. Implement selected
configuration/build/test/cleanup behavior and both-generator regressions.
Prove db/misc mutations do not execute core work or affect the other group.
Keep artifact behavior unchanged until the group graphs are trustworthy.

### C. Promote cmocka and complete group-owned verification

Implement both cmocka library variants, notices, C89 audit/facade as required,
native Darwin support, and downstream tests. Complete group hover/API/export,
Valgrind/fuzz ownership, db e2e, and missing-core diagnostics.

### D. Cut over packaging and lifecycle consumption together

Replace monolithic staging with three archives and combination tests. Update
metadata, manifest/verification, source reconstruction, native CI, lifecycle
source skill and user docs coherently. Do not ship an intermediate release that
publishes split artifacts with old acquisition/verification assumptions.

### E. Remove redundant work and repair measured bottlenecks

Deduplicate exact same-run fixtures/clangd, generate source once, propagate
configured build jobs, investigate/repair OpenSSL parallel ordering, then tune
measured test/e2e setup. Implement broad coherent changes before running the
expensive full matrix; use focused fixtures for iteration.

### F. Final evidence

- All ownership, mutation, cache, missing-prerequisite and combination negative
  tests pass, with no hidden core work in optional-only operations.
- Existing supported public APIs, ABI identities, full facade coverage, strict
  C89 policy and static/shared behavior remain correct.
- Every target's core-only, core+db, core+misc and full installation passes.
- Cmocka is shipped and usable on all seven targets without becoming a
  production facade dependency.
- Full clean release and independent all-source reconstruction pass, including
  native memory/fuzz and Podman teardown checks.
- Native Darwin workflow succeeds on the exact implementation commit.
- Final manifest, privacy, relocation, license and warning gates pass.
- Before/after timing reports identify actual savings and unchanged coverage;
  no numerical time target is promised without measurements.
- `make finalize-slice` with the relevant scope and `make format-check` pass
  before implementation commits. Keep global gates before release.

The split changes artifact names/acquisition and is a deliberate distribution
contract change even if C library ABIs remain unchanged. Preserve upstream ABI
identities and apply the existing published-baseline compatibility policy to
any actual API/ABI or runtime change. Record downstream installation changes
and the coordinated migration in release notes; decide the release version
through the normal lifecycle rather than through this document.

## 14. Implementation questions to resolve with evidence

These do not block the specification; they are required investigation points
before claiming the corresponding implementation complete:

1. Exact cause of debug-to-release producer repetition, including shared stamp
   commands and Ninja's independent consumer build histories.
2. Exact OpenSSL assembly dependency race, a reproducible regression, and a
   safe repair at configured parallelism.
3. Full cmocka public C89 compatibility and the necessary facade boundary,
   including mock value width and macro/control-flow semantics.
4. Complete authoritative payload/test inventory, especially mixed examples,
   shared generated definitions, helper dependencies, and optional API imports.
5. Manifest schema and manifest-aware acquisition/validation for direct
   pkg-config users, with explicit supported installation behavior.
6. Downstream release/acquisition migration needs, without assuming access or
   permission to change adjacent repositories.
7. Measured source reconstruction, extracted consumer, and e2e wall times after
   instrumentation; the baseline currently lacks complete phase boundaries.
