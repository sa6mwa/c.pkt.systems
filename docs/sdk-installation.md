# Installing composable SDK groups

Download the core tarball and any desired optional tarballs for the same release
and target. Verify their hashes against the appropriate complete release
CHECKSUMS manifest, then extract them into a common parent directory. Each
archive has the root `c.pkt.systems-<version>-<target>` and owns distinct regular
files and symlinks. The supported groups are core, core+db, core+misc, and all;
optional extraction order does not matter. There is no monolithic SDK tarball.

Core owns common support, the validator and native cmocka. Each group owns its
licenses, notices, documentation and examples under
`share/doc/c.pkt.systems/<group>/`. The authoritative component, facade, metadata
and consumer ownership is `cmake/components.json` in the source archive.

Validate the actual extracted bytes, modes and links before discovery:

```sh
sdk=/path/to/c.pkt.systems-<version>-x86_64-linux-gnu
python3 "$sdk/share/c.pkt.systems/validate-sdk.py" --prefix "$sdk" --groups core,db
cmake -S . -B build -DCMAKE_PREFIX_PATH="$sdk"
```

CMake package configuration runs the same installed standard-library validator
before creating imported targets. Optional imports are lazy and require the exact
core ID recorded by their manifest. Validation successes are cached only within
one configure for the same prefix, groups and IDs; a later configure rehashes the
actual installation. Unrelated optional corruption does not invalidate a selected
closure. Validation never changes permissions, libraries or signatures.

For direct pkg-config, run the validator explicitly first and prevent host lookup:

```sh
python3 "$sdk/share/c.pkt.systems/validate-sdk.py" --prefix "$sdk" --groups core,db
PKG_CONFIG_PATH= PKG_CONFIG_LIBDIR="$sdk/lib/pkgconfig" pkg-config --static --cflags --libs cpkt-sqlite
```

Existing native and facade package/target names remain available. Static and
shared link closures are separate. `cpkt-core.pc` is a version identity marker
with no libraries; it does not introduce an artificial linkage cycle.

The full GNU shared SDK has the existing glibc floor of 2.43. Individual binaries
may require less, but the complete SDK support contract remains that floor. Musl
and native Darwin use their existing platform requirements; Darwin's floor is
15.0. Existing SONAMEs and Darwin install names/compatibility versions are kept.

Selected commands write only under their build namespace and require explicit
Release PRESET. `make package GROUP=core PRESET=x86_64-linux-gnu-release` prepares
core. Then the same command with `GROUP=db` or `GROUP=misc` borrows its exact
verified archive without running core tests or touching sibling products. Full
binary scope contains 21 group tarballs plus the Darwin smoke ZIP. Release scope
also requires the current successful independent source reconstruction; its sole
checksum manifest is written only after all 23 payloads are present and verified.
Standalone `make verify-release-archives` and release-scope checksum commands
accept prior successful source reconstruction for the identical archive digest.
Release production requires source reconstruction from its current operation.
Filtered all-group tests run complete Core coverage once to establish the
optional suites' prerequisite; optional filtered coverage never supplies full
readiness or runs the unfiltered composition suite.
Unqualified `scripts/build.sh` and `scripts/test.sh` retain the six-target Linux
Release matrix. Selected groups default to native Debug; an explicit `PRESET`
selects that configuration. Aggregate configuration establishes complete Core
readiness before optional configuration. Producer reuse compares the requested
preset flags with the selected consumer configuration before accepting cached
producer inputs; changed flags rebuild the affected producer once.

The native workflow has separate source and final tagged producer-artifact lanes.
Artifact handoff requires an unpublished authenticated draft, exact producer
commit/lightweight tag, and all 24 upload assets with IDs, sizes and API SHA-256
digests. Native verification executes the supplied libraries without rebuilding
or resigning them. Draft staging is an explicitly authorized final release
operation; automatic gates do not create or publish releases.
