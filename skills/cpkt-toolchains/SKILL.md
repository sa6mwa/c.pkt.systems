---
name: cpkt-toolchains
description: Install, discover, and report C/C++ compiler collections for cpkt-supported targets. Use when Codex needs to set up downstream cpkt projects, verify C and C++ compilers for cpkt Linux GNU/musl architectures, auto-cache missing public cross toolchains, or explain the paths/env vars for cpkt toolchains.
---

# CPKT Toolchains

Use this skill to provision C and C++ compiler collections for cpkt targets without depending on any repository-local setup.

## Core Tool

Run the bundled script:

```sh
skills/cpkt-toolchains/scripts/cpkt-toolchains.sh <command> [target-id]
```

If using the skill outside this repository, resolve the path relative to the skill directory.

Commands:

- `targets`: list supported target ids.
- `discover [target]`: print status, source, root, compiler, sysroot, and cache paths.
- `ensure <target|all>`: prefer complete local compilers, otherwise install missing downloadable Linux toolchains into the persistent cache.
- `env <target>`: print shell exports for `CC`, `CXX`, `AR`, `RANLIB`, `STRIP`, `READELF`, `CPKT_TOOLCHAIN_ROOT`, `CPKT_TOOLCHAIN_PREFIX`, `CPKT_SYSROOT`, and Linux static runtime archive paths.

## Policy

- Prefer a complete local C/C++ compiler collection when present.
- Treat a Linux collection as complete only when C compiler, C++ compiler, `ar`, `ranlib`, libc headers, `libstdc++.a`, and `libgcc.a` are available.
- Download only public Linux toolchains. Apple/Darwin is discovery-only because SDK access is not public, and reports `libc++` runtime intent rather than GNU static runtime archives.
- Cache downloads outside project trees under `${CPKT_TOOLCHAIN_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/c.pkt.systems/toolchains}`.
- Use pinned Bootlin stable tarballs and SHA-256 checksums from the skill script. Update the manifest deliberately when Bootlin publishes a newer stable release.

## Typical Workflows

Install all downloadable cpkt Linux compiler collections:

```sh
skills/cpkt-toolchains/scripts/cpkt-toolchains.sh ensure all
```

Inspect all configured paths for a downstream project:

```sh
skills/cpkt-toolchains/scripts/cpkt-toolchains.sh discover
```

Emit environment for one target:

```sh
eval "$(skills/cpkt-toolchains/scripts/cpkt-toolchains.sh env aarch64-linux-musl)"
```

## References

Read `references/toolchains.md` when you need the exact target table, cache layout, override variables, or downstream setup notes.
