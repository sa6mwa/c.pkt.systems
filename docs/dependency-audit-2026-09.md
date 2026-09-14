# September 2026 dependency audit

Audited on 2026-09-13. OpenSSL remains on the existing 3.6 series by maintainer
instruction. The previous-version comparison baseline is tag `v0.9.0`.
New selections exclude release candidates and development snapshots; the
existing MQTT-C commit pin is retained as described below.

| Component | Previous | Selected | Upstream evidence |
| --- | --- | --- | --- |
| OpenSSL | 3.6.2 | 3.6.4 | [Supported releases](https://openssl-library.org/source/) |
| curl | 8.20.0 | 8.22.0 | [Downloads](https://curl.se/download.html) |
| nghttp2 | 1.69.0 | 1.70.0 | [Release](https://github.com/nghttp2/nghttp2/releases/tag/v1.70.0) |
| libxml2 | 2.15.3 | 2.15.4 | [Release notes](https://download.gnome.org/sources/libxml2/2.15/libxml2-2.15.4.news) |
| Lua | 5.5.0 | 5.5.1 | [Downloads and checksums](https://www.lua.org/ftp/) |
| whisper.cpp | 1.9.1 | 1.9.4 | [Release](https://github.com/ggml-org/whisper.cpp/releases/tag/v1.9.4) |
| open62541 | 1.5.4 | 1.5.8 | [Release](https://github.com/open62541/open62541/releases/tag/v1.5.8) |
| zlib | 1.3.2 | unchanged | [Downloads](https://zlib.net/) |
| libssh2 | 1.11.1 | unchanged | [Downloads](https://libssh2.org/) |
| miniaudio | 0.11.25 | unchanged | [Release](https://github.com/mackron/miniaudio/releases/tag/0.11.25) |
| cmocka | 2.0.2 | unchanged | [Release archive index](https://cmocka.org/files/2.0/) |
| MQTT-C | commit 0f4c34c8 | unchanged | [open62541 submodule](https://github.com/open62541/open62541/tree/v1.5.8/deps/mqtt-c) |

MQTT-C's selected commit is newer than its latest tagged release v1.1.6 and
remains the exact revision used by open62541 1.5.8. Its upstream CMake project
version is still 1.1.2; that metadata is not the tag-selection authority.
whisper.cpp 1.9.4 includes ggml 0.23.0, replacing 0.15.1.

## Security and integration

- OpenSSL 3.6.4 includes the June and August 2026 security fixes. See the
  [upstream timeline](https://openssl-library.org/news/timeline/).
- curl 8.22.0 supersedes the published vulnerabilities affecting 8.20.0,
  including OpenSSL pinning, provider lifetime, and native trust-store reuse
  fixes. Applicability varies by enabled feature; see the
  [version-specific advisory list](https://curl.se/docs/vuln-8.20.0.html).
- libxml2 2.15.4 fixes bounds, integer-overflow, and XInclude parse-flag issues.
- open62541 1.5.8 includes allocation-overflow, message-boundary, and lifetime
  fixes. Upstream now handles the private glibc header and certificate-path
  copying issues, so the corresponding local patches are retired. The MQTT
  symbol-prefix and missing Linux packet-header patches remain.
  Its new anonymous-session callback invocation is handled by the facade so
  `allow_anonymous` keeps its existing meaning and credential callbacks still
  authenticate username/password sessions only. Tests also reject anonymous
  access when disabled and empty username/password credentials.
- Lua 5.5.1 is a bug-fix release; see the [upstream bug list](https://www.lua.org/bugs.html).

Source URLs and SHA-256 pins are in `cmake/CpktDependencies.cmake`. The package
checks track the selected upstream shared-library filenames. Facade ABI majors
remain unchanged because this update does not change their public interfaces.

This audit records release selection. It does not claim that every component
is free of vulnerabilities or that downstream binary compatibility has been
exhaustively verified.

## Verification evidence

The candidate at `7f936ea` passed the following serialized checks on 2026-09-13:

- `make test-all`: 65 host tests, clangd surface checks, Lua mock Memcheck
  without errors or leaks, and Lua/OPC UA fuzz smoke checks.
- `CPKT_LIVE_CHECKS=1 make E2E_SUS_PRESET=debug prerelease-live`: both speech
  facade and cpktxscribe e2e workflows passed with live model/provider inputs.
- `make fuzz` and `CPKT_FUZZ_LONG_ENABLE=1 make fuzz-long`: both Lua and OPC UA
  harnesses passed, including five minutes per harness in the extended run.
- `make release`: a clean local rehearsal passed the host gates, all six Linux
  target suites, Darwin build/link checks, binary/source packaging, checksums,
  archive privacy and relocatability checks, and extracted SDK consumer tests.

| Linux target | Passing CTest tests |
| --- | ---: |
| x86_64 glibc | 65 |
| x86_64 musl | 66 |
| aarch64 glibc | 63 |
| aarch64 musl | 66 |
| armhf glibc | 63 |
| armhf musl | 66 |

Darwin artifacts were cross-compiled and link/package checked on Linux; their
binaries were not executed on macOS. The untagged rehearsal produced `0.0.0`
artifacts, not a published release. Subsequent documentation-only changes are
not part of that recorded artifact revision.

## Compatibility coverage and shipment boundary

Runtime/header version checks, OpenSSL 3 consumer checks, facade regression
tests, and shared/static install-tree consumers passed. These rebuild consumers
against the candidate; they are not an exhaustive comparison of public type
layouts, symbols, or existing binaries built against the released bundle.
The maintainer confirmed that supported downstream whisper.cpp/ggml consumers
use the C89 `cpkt_sus` facade exclusively. Direct upstream API/ABI compatibility
for external consumers is outside the c.pkt.systems support commitment.
The facade public interface is unchanged, and its integration, shared/static
consumer, and speech e2e gates passed. Under that confirmed scope, the whisper.cpp
1.9.1 to 1.9.4 upgrade, including ggml 0.15.1 to 0.23.0, is accepted; direct ggml
binary compatibility assessment is not an outstanding shipment gate.

The [dependency ABI policy](../AGENTS.md) still applies to the facade, bundled
consumers, and other supported dependency interfaces. This scope clarification
does not authorize breaking those interfaces or changing OpenSSL's ABI series.

## Bootlin toolchain update

After the dependency rehearsal, the maintainer requested upgrading all six
Linux compiler collections from Bootlin stable `2025.08-1` to `2026.08-1` before
release. The earlier verification evidence above predates this toolchain change.

| Component | Previous collection | Selected collection |
| --- | --- | --- |
| GCC | 14.3.0 | 15.3.0 |
| binutils | 2.43.1 | 2.45.1 |
| GDB | 15.2 | 16.3 |
| Linux headers | 5.4.296 | 5.10.269 |
| glibc | 2.41 | 2.44 |
| musl | 1.2.5 | 1.2.6 |

The [Bootlin x86-64](https://toolchains.bootlin.com/releases_x86-64.html),
[aarch64](https://toolchains.bootlin.com/releases_aarch64.html), and
[armv7-eabihf](https://toolchains.bootlin.com/releases_armv7-eabihf.html) release
indexes provide the collection metadata and SHA-256 files. Both repository
resolver copies pin those published archive checksums. Darwin remains on the
existing local osxcross collection.

This updates the entire compiler, binutils, C/C++ runtime, libc, and sysroot
collection. Kernel-header and libc build versions alone do not establish the
minimum runtime requirement of each shipped binary. Before rebuilding, the
previous candidate artifacts required at most `GLIBC_2.38` across the three
glibc targets; per-library symbol requirements were retained for comparison.
The new collection requires fresh build/test/package and GCC-plugin fuzz
verification, plus comparison of the resulting runtime requirements.

### Verification result and required runtime decision

All six new archives passed checksum verification and provisioning. Repository
and bundled-skill resolver tests passed, including download/extraction failure
handling. AFL++ rebuilt against GCC 15.3 and passed its instrumentation self-test.

The clean native build completed, but 17 of 65 host CTests failed at process
startup because the new x86_64 glibc binaries require `GLIBC_2.43`, while the
verification host provides glibc 2.42. Concrete new imports include
`memset_explicit@GLIBC_2.43` in libcurl and audio consumers, and
`sqrtf@GLIBC_2.43` in the speech backend. The previous candidate artifacts
required at most `GLIBC_2.38`. Running the audio facade through the matching
Bootlin loader/libc passed all 27 unit tests; this is diagnostic evidence, not
a substitute for completing the full matrix or preserving the deployment floor.

The maintainer requires all six latest complete Bootlin collections. Retaining
an older glibc collection or combining a new compiler with an older sysroot is
not the selected approach. No host libc, release tag, or published artifact has
been changed. The release remains paused pending the full target matrix and an
explicit deployment-runtime commitment.

The isolated compiler/linker experiment traced the new `sqrtf` requirement to
glibc's libm default symbol: either old or new compiler/binutils selects
`GLIBC_2.43` when linking the new libm, while the old libm selects the old symbol.
The new libm retains the old compatibility symbol. This is consistent with the
[glibc 2.43 release notes](https://sourceware.org/pipermail/libc-announce/2026/000052.html),
which move legacy SVID math behavior to compatibility symbols. Curl separately
detects the newly available `memset_explicit`. Disabling that curl probe alone
would not preserve the former deployment floor.

### Local development executable runtime policy

All non-shipped native development executables use the Bootlin runtime directly:
cpktxscribe, tests, helpers, fuzzers, in-tree examples, and temporary SDK consumers.
Build type does not change this classification. `cmake/CpktLocalRuntime.cmake`
provides the shared link-time settings. Generated CMake consumers apply its
helper; pkg-config verification builds reuse its compiler/linker options.
Native CTest, e2e, examples, and extracted SDK consumers execute normally.
There is no separate runtime launcher or launcher-specific test fixture.
Foreign-target QEMU execution is unchanged.

Runtime settings remain private to local executable links. Shipped libraries,
installed example sources, and SDK CMake/pkg-config metadata do not inherit
workstation paths. Native SDK checks do not export runtime library paths to
host subprocesses. A policy test checks all local executable interpreters and
rejects mismatches; package verification checks temporary consumers as well.
The process regression exercises actual libc/loader mappings, `/proc/self/exe`
re-execution, and independent host-shell execution.

Final link-time cutover validation passed:

- All 68 native release CTests and all 68 native debug CTests, including Lua,
  examples, process-runtime checks, and the negative interpreter-policy check.
- `make test-all`, including public-surface clangd checks, Valgrind with zero
  errors/leaks, and both Lua/OPC UA AFL++ smoke campaigns without saved crashes
  or timeouts.
- All three focused musl tests and both live speech/cpktxscribe e2e workflows.
- Native release archive privacy and extracted CMake/pkg-config/example consumer
  checks. All 55 temporary consumer executables passed runtime-policy inspection
  before direct execution, with no native verification LD_LIBRARY_PATH export.

The runtime launcher, its Make target, and its dedicated fixtures were removed.
The repository lifecycle skill now describes this single link-time approach.

The full target matrix, long fuzz campaigns, deployment-runtime commitment,
and final release gate remain outstanding. This work does not resume shipment
or establish complete exclusion of all possible host-loaded runtime libraries.

### Follow-up runtime and skill audit

The follow-up audit checked runtime selection, native test/e2e environments,
SDK verification and packaging boundaries, dependency compatibility policy,
and the repository lifecycle skill and related documentation. It found and
corrected three gaps:

- Native audio/speech tests and two speech scripts still exported dependency
  `LD_LIBRARY_PATH` values. These can affect host child processes even without
  exporting the collection's libc directory. Native execution now relies on
  private ELF paths; foreign-target and Darwin handling is preserved. CMake
  rejects native CTests which introduce this environment variable.
- The executable-policy check treated every ELF without an interpreter as
  static. It now rejects non-executable ELF types, zero entry points and dynamic
  dependencies in that case, and checks the selected loader directory in the
  private RPATH of dynamic executables. A shared-library negative CTest covers
  the earlier false acceptance.
- The skill now explicitly covers project dependency environment contamination,
  Lua example module-search variables versus native runtime selection, and
  published dependency/ABI/deployment baselines for upgrades. Duplicate Lua
  wording was removed. No dependency version or public ABI changed in this audit.

Follow-up evidence:

- 69/69 native release CTests and 69/69 native debug CTests passed.
- `make test-all` passed, including public-surface clangd checks, zero Valgrind
  errors/leaks, and both AFL++ smoke campaigns without saved crashes/timeouts.
- Native release packaging/privacy and extracted SDK consumers passed; all 55
  temporary executables passed the strengthened runtime-policy inspection.
- Live speech e2e and the standalone speech VOX intro example passed without
  dependency library environment exports. Three focused musl tests passed.
- Negative configure injection proved a native CTest library-path export is
  rejected. Additional Bootlin-built probes proved missing RPATH is rejected
  and a fully static executable is accepted. Shell syntax, diff whitespace,
  and skill structure validation passed.

These are native cutover audit results, not a new full release rehearsal or a
fresh upstream security-release survey. Full target-matrix execution, long fuzz
campaigns, and the deployment-runtime commitment remain the release blockers
listed above. Host-loaded optional libraries and inherited user loader settings
are not proven hermetic by these checks.

### Second cutover sweep

A second sweep traced active Make/CMake, script, test, SDK-consumer, and skill
references for the abandoned runtime approaches. No active native launcher,
Bubblewrap configuration, duplicate test CLI, or old Bootlin collection pin
remains in those surfaces. Foreign-target QEMU and compiler instrumentation
wrappers serve their existing purposes and are not native runtime launchers.
Historical audit notes and ignored diagnostic material are retained as evidence.

Two verification gaps were corrected:

- Runtime-policy registration now runs at CMake directory completion, including
  after the facade-only early return. Valgrind, Lua fuzz, and focused musl builds
  now check every local executable's runtime metadata and reject injected native
  test library-path environments just like the full configuration.
- The six direct `find_package` consumers and the default OpenSSL pkg-config
  consumer are now executed, not only compiled and inspected. They use ordinary
  native execution or the existing foreign-target QEMU path. No launcher was
  introduced.

Verification passed: 69 debug CTests; four tests each in the Valgrind, fuzz, and
focused musl configurations; extracted native SDK verification with all seven
newly executed consumers and all 55 runtime-policy inspections; a negative
facade-only configure test proving environment rejection survives the early
return; package argument and lifecycle surface tests; shell syntax and skill
validation. This sweep changes verification coverage only, not shipped ABI or
runtime metadata. The full release matrix and deployment decision remain open.


## Final clean rehearsal and approved 0.10.0 deployment contract

At candidate `aef9ed1`, `make test-all`, both live speech e2e workflows,
standard fuzz campaigns, five minutes each of Lua and OPC UA fuzzing, and a
subsequent clean `make release` all passed. The release command completed with
exit status zero. It passed 69 debug tests and the complete Linux matrix:

| Target | Passing tests |
| --- | ---: |
| x86_64 glibc | 69 |
| x86_64 musl | 70 |
| aarch64 glibc | 63 |
| aarch64 musl | 66 |
| armhf glibc | 63 |
| armhf musl | 66 |

Darwin cross-build/link/package checks, all extracted SDK consumers, source
archive checks, checksums, and privacy/relocatability checks passed. Valgrind
reported no errors/leaks; the standard and extended fuzz campaigns saved no
crashes or timeouts. The nine checksum-listed artifacts have rehearsal version
0.0.0. They are not the final tagged 0.10.0 artifacts. This evidence supersedes
the earlier outstanding full-matrix and long-fuzz items in this historical log.
Darwin runtime execution and exhaustive downstream existing-binary comparisons
remain outside this evidence.

Direct inspection of the published 0.9.0 glibc SDK archives confirms their highest
required glibc symbol version is GLIBC_2.38 on all three architectures. The final
rehearsal archives require GLIBC_2.43 on all three. libcurl, libggml-base,
libggml-cpu, and libwhisper carry that requirement. The public facade ABI majors
and upstream libc SONAME are unchanged; the deployment requirement is not.

The approved 0.10.0 contract is glibc 2.43 or newer for the complete shared SDK
set. Applications using the affected dependencies on glibc 2.42 or older cannot
load the new shared objects. They must upgrade their deployment runtime or
build a fully static musl application and verify its application-specific OS,
networking, dynamic-loading, and other runtime behavior. Merely choosing static
SDK archives while dynamically linking libc is insufficient. No extra glibc is
bundled. Exact Bootlin loader paths remain private to non-shipped verification
executables and are not a deployment mechanism shipped in SDK metadata.

This transition follows the maintainer's choice of the latest complete Bootlin
collections, rather than mixing a newer compiler with an older libc/sysroot.
The old bundle has the former floor but does not contain this dependency/security
update; retaining it is not equivalent to receiving the new fixes. Moving back
to that bundle requires restoring a compatible downstream build as well as its
libraries; newly linked applications may retain newer runtime requirements.
The complete bundled shared/static consumers now pass, but this does not prove
every external application's source or behavior compatibility.

On 2026-09-14, the maintainer explicitly approved glibc 2.43 or newer as the
requirement for the affected shared libraries. The release notes lead with this
deployment compatibility change. The final lightweight v0.10.0 tag must receive
its own clean release build and verification before publication; the rehearsal
archives are not reused.
