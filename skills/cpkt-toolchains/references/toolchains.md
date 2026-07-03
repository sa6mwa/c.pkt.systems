# CPKT Toolchain Reference

## Targets

| Target | Local-first rule | Download fallback |
| --- | --- | --- |
| `x86_64-linux-gnu` | native `/usr/bin/cc` and `/usr/bin/c++` | Bootlin `x86-64--glibc--stable-2025.08-1` |
| `x86_64-linux-musl` | `CPKT_X86_64_MUSL_PREFIX` | Bootlin `x86-64--musl--stable-2025.08-1` |
| `aarch64-linux-gnu` | `CPKT_AARCH64_GNU_PREFIX`, then `/usr` cross package | Bootlin `aarch64--glibc--stable-2025.08-1` |
| `aarch64-linux-musl` | `CPKT_AARCH64_MUSL_PREFIX`, then `$HOME/.local/cross/aarch64-linux-musl` | Bootlin `aarch64--musl--stable-2025.08-1` |
| `armhf-linux-gnu` | `CPKT_ARMHF_GNU_PREFIX`, then `/usr` cross package | Bootlin `armv7-eabihf--glibc--stable-2025.08-1` |
| `armhf-linux-musl` | `CPKT_ARMHF_MUSL_PREFIX`, then `$HOME/.local/cross/arm-linux-musleabihf` | Bootlin `armv7-eabihf--musl--stable-2025.08-1` |
| `arm64-apple-darwin` | `OSXCROSS_ROOT` and `CPKT_OSXCROSS_HOST` | none |

## Cache Layout

Default root:

```sh
${CPKT_TOOLCHAIN_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/c.pkt.systems/toolchains}
```

Subdirectories:

- `archives/`: verified tarballs.
- `roots/`: extracted compiler collection roots.

The cache is intended to survive project `make clean`, CMake clean, and package rebuilds.

## Downstream Setup

Use discovery first:

```sh
path/to/skills/cpkt-toolchains/scripts/cpkt-toolchains.sh discover
```

Install a target and export its environment:

```sh
path/to/skills/cpkt-toolchains/scripts/cpkt-toolchains.sh ensure aarch64-linux-gnu
eval "$(path/to/skills/cpkt-toolchains/scripts/cpkt-toolchains.sh env aarch64-linux-gnu)"
```

For CMake projects, pass these values into the downstream toolchain file or configure command:

```sh
cmake -S . -B build-aarch64 \
  -DCMAKE_C_COMPILER="$CC" \
  -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_AR="$AR" \
  -DCMAKE_RANLIB="$RANLIB" \
  -DCMAKE_SYSROOT="$CPKT_SYSROOT"
```

The emitted `env` command is deliberately shell-only and does not mutate global login shell files.

## Update Rule

When Bootlin publishes a newer stable toolchain set, update the script manifest in one commit:

1. Toolchain archive name.
2. Download architecture path.
3. SHA-256 checksum.
4. Bootlin compiler prefix.
5. Bootlin sysroot path.

Then run:

```sh
bash -n skills/cpkt-toolchains/scripts/cpkt-toolchains.sh
skills/cpkt-toolchains/scripts/cpkt-toolchains.sh discover
skills/cpkt-toolchains/scripts/cpkt-toolchains.sh ensure <one changed target>
```
