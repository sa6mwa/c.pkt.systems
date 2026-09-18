# CPKT Toolchain Reference

This lifecycle owns C and C++ compiler resolution for pkt.systems C/CMake projects. Linux builds use only complete, pinned compiler collections cached outside the checkout. A Linux target must not select a host-installed GCC, Clang, linker, binutils collection, libc, headers, or C++ runtime.

## Compiler Policy

- Every ordinary Linux build uses the pinned Bootlin GCC collection for its target. Its triple-prefixed `gcc`, `g++`, `ld`, `ar`, `ranlib`, `strip`, `nm`, `objcopy`, `objdump`, `addr2line`, `gdb`, and `readelf`, plus its sysroot libc and headers, are one inseparable collection.
- Do not use `/usr/bin/cc`, `gcc`, `clang`, distro cross compilers, or unpinned compiler paths as a fallback. A cached Bootlin collection is the only Linux default.
- `arm64-apple-darwin` uses a local, project-pinned osxcross collection. The lifecycle must not download Apple SDKs or Darwin compiler collections. Once that collection is ready, the lifecycle may provision its separately pinned, Linux-host `mig` helper as described below.
- Native memory checking uses host-provided Valgrind against executables compiled by the selected Bootlin collection. It is a required gate on the native x86_64 Linux host, but it is not an MSan substitute. Never run Valgrind through cross-compilation, an emulator, or QEMU.
- Native fuzzing uses a pinned cached AFL++ release built with the matching x86_64 Bootlin GCC plugin headers. AFL++ compiler wrappers must delegate to the selected Bootlin `gcc`/`g++`; never use host GCC or Clang for project targets. Never run fuzzing through cross-compilation, an emulator, or QEMU.
- `clang-format` and `clangd` are host OS development-tool prerequisites only. They must not enter CMake compiler or linker discovery. `clangd` validation is a native development-host editor gate: register and run it only against the native host compile database. Cross-target CTest, package, and release configurations must not invoke it or rely on host `clangd` to emulate a target compiler or sysroot ABI; prove those targets through their selected compiler, supported target runner, and package verification gates.

## Linux Targets

| Target | Pinned Bootlin collection | Compiler prefix | Sysroot |
| --- | --- | --- | --- |
| `x86_64-linux-gnu` | `x86-64--glibc--stable-2026.08-1` | `x86_64-linux` | `x86_64-buildroot-linux-gnu/sysroot` |
| `x86_64-linux-musl` | `x86-64--musl--stable-2026.08-1` | `x86_64-linux` | `x86_64-buildroot-linux-musl/sysroot` |
| `aarch64-linux-gnu` | `aarch64--glibc--stable-2026.08-1` | `aarch64-linux` | `aarch64-buildroot-linux-gnu/sysroot` |
| `aarch64-linux-musl` | `aarch64--musl--stable-2026.08-1` | `aarch64-linux` | `aarch64-buildroot-linux-musl/sysroot` |
| `armhf-linux-gnu` | `armv7-eabihf--glibc--stable-2026.08-1` | `arm-linux` | `arm-buildroot-linux-gnueabihf/sysroot` |
| `armhf-linux-musl` | `armv7-eabihf--musl--stable-2026.08-1` | `arm-linux` | `arm-buildroot-linux-musleabihf/sysroot` |

The resolver pins each tarball SHA-256. Change a Bootlin pin only by updating its archive name, URL architecture, checksum, compiler prefix, and sysroot as one atomic lifecycle change.

## Cache Layout

Default root:

```sh
${CPKT_TOOLCHAIN_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/c.pkt.systems/toolchains}
```

- `archives/` contains verified Bootlin and AFL++ source tarballs.
- `roots/` contains extracted immutable compiler collections.
- `locks/` contains per-collection advisory lock files. Provisioning must hold the matching lock from its post-lock readiness check through publication; a waiting process must recheck readiness and never remove a root another process has already published. Use host `flock` for these lifecycle cache locks, with `CPKT_TOOLCHAIN_LOCK_TIMEOUT` (default `600` seconds) as the bounded wait.

The cache survives project cleans and is shared by all downstream pkt.systems projects. Do not create project-local compiler caches.

## Provisioning

Use the lifecycle resolvers directly or vendor their exact content into a downstream repository:

```sh
skills/pkt-systems-cmake-lifecycle/scripts/cpkt-toolchains.sh ensure all
skills/pkt-systems-cmake-lifecycle/scripts/cpkt-toolchains.sh discover aarch64-linux-gnu
eval "$(skills/pkt-systems-cmake-lifecycle/scripts/cpkt-toolchains.sh env aarch64-linux-gnu)"

skills/pkt-systems-cmake-lifecycle/scripts/cpkt-aflpp.sh ensure
eval "$(skills/pkt-systems-cmake-lifecycle/scripts/cpkt-aflpp.sh env)"
```

`ensure all` downloads the six Linux Bootlin collections and reports Darwin osxcross status. It never installs an Apple SDK. `discover` reports all resolved paths, including the selected compiler, linker, binutils, sysroot, static GNU C++ runtime archives, and source. `env` emits shell exports only; it does not modify login-shell files.

## Development-machine provisioning

These instructions establish a Linux development workstation for both
pkt.systems Go work and C/CMake work. They are workstation prerequisites, not
SDK contents and not release-artifact dependencies. Use the host package
manager with explicit operator authorization where it needs `sudo`; do not
embed package-manager actions in ordinary project builds.

On a Debian/Ubuntu host, install the following capability groups before
provisioning toolchains:

- build and Autotools tooling: `build-essential`, `binutils`, `clang`, `lld`,
  `cmake`, `ninja-build`, `make`, `autoconf`, `automake`, `libtool`, `patch`,
  `pkg-config`, `perl`, `python3`, `python3-venv`, `bison`, `flex`, and
  `gawk`;
- archive, download, and source tools: `git`, `git-lfs`, `git-crypt`, `curl`,
  `wget`, `ca-certificates`, `bzip2`, `xz-utils`, `zip`, `unzip`, `xar`, and
  `cpio`;
- native development and dependency headers: `libbz2-dev`, `liblzma-dev`,
  `libssl-dev`, `libxml2-dev`, `uuid-dev`, `zlib1g-dev`, `libx11-dev`,
  `libcurl4-openssl-dev`, and `libcairo2-dev`;
- target verification and local integration tools: `qemu-user`, `podman`,
  `fuse-overlayfs`, `slirp4netns`, `uidmap`, `help2man`, and `texinfo`;
- native-only quality tools: `valgrind`, `clang-format`, and `clangd`.

Install Go separately according to the Go components' selected Go-version
policy. The C/CMake provisioning baseline deliberately does not select, pin,
or update Go. Do not substitute distro cross compilers or `musl-cross-make`
outputs for the lifecycle's pinned Bootlin collections; the resolver owns the
shipped Linux target toolchains.

### Darwin osxcross input and setup

The Apple SDK is proprietary input supplied by the developer. Before Darwin
provisioning, the developer must sign in to Apple Developer Downloads with an
account entitled to obtain Xcode, download the approved Xcode archive manually,
and place it at a local path they control. For the currently used SDK source,
that archive is normally named `Xcode_26.4.1_Apple_silicon.xip`; accept an
equivalent locally extracted Xcode/SDK only when its SDK version is the
project-approved one.

Never put that archive in a repository, a c.pkt.systems dependency cache, an
SDK artifact, or a source archive. Never try to fetch it with `curl`, request
Apple credentials, or use `sudo` to discover, extract, or install it. If it is
absent, stop with an actionable prerequisite naming the expected local archive
or extracted SDK path. `xar`, `cpio`, `xz`, `bzip2`, `libxml2` development
headers, OpenSSL development headers, Python, and the normal build tools above
are required to turn the approved local Xcode input into osxcross's packaged
`MacOSX*.sdk` input.

Provision osxcross from a project-approved pinned source revision, preserve the
revision and SDK-version provenance in the workstation setup, build only the
needed Darwin architectures, and publish the resulting local collection outside
the repository (the conventional location is
`$HOME/.local/cross/osxcross`). Set `OSXCROSS_ROOT` when another location is
used. A moving `master` checkout is not a reproducible lifecycle toolchain.

After osxcross can produce a Darwin arm64 Mach-O smoke executable, provision
the host-side MIG helper and verify the complete collection:

```sh
scripts/cpkt-toolchains.sh ensure arm64-apple-darwin
scripts/cpkt-toolchains.sh discover arm64-apple-darwin
```

`ensure arm64-apple-darwin` does not obtain Apple content. It uses the pinned
x86_64 Linux GNU Bootlin collection to build the pinned PureDarwin-derived MIG
as a static Linux host executable in the shared toolchain cache. `mig` invokes
the local osxcross target compiler to preprocess Darwin definitions. It is
build-only tooling: do not bundle it, its source, or its license in the SDK.
Require `discover` to report `status=ready`, the osxcross compiler/binutils,
`mig`, `migcom`, and the pinned `mig_revision` before configuring a Darwin
build.

## CMake Setup

Resolve the collection before `project()` through a CMake toolchain file or a compiler bootstrap module. This reusable pattern parses the resolver output and sets every relevant tool, not only `CMAKE_C_COMPILER`:

```cmake
function(project_configure_bootlin_toolchain target_id)
  set(resolver "${CMAKE_SOURCE_DIR}/scripts/cpkt-toolchains.sh")
  execute_process(COMMAND "${resolver}" ensure "${target_id}"
    RESULT_VARIABLE result ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Unable to install the pinned Bootlin toolchain: ${error}")
  endif()
  execute_process(COMMAND "${resolver}" discover "${target_id}"
    RESULT_VARIABLE result OUTPUT_VARIABLE description ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Unable to inspect the pinned Bootlin toolchain: ${error}")
  endif()
  foreach(key cc cxx ld ar ranlib strip nm objcopy objdump addr2line readelf sysroot root)
    string(REGEX MATCH "${key}=([^\r\n]+)" match "${description}")
    if(NOT match)
      message(FATAL_ERROR "Bootlin resolver did not report ${key} for ${target_id}")
    endif()
    set(bootlin_${key} "${CMAKE_MATCH_1}")
  endforeach()
  set(CMAKE_C_COMPILER "${bootlin_cc}" CACHE FILEPATH "" FORCE)
  set(CMAKE_CXX_COMPILER "${bootlin_cxx}" CACHE FILEPATH "" FORCE)
  set(CMAKE_LINKER "${bootlin_ld}" CACHE FILEPATH "" FORCE)
  set(CMAKE_AR "${bootlin_ar}" CACHE FILEPATH "" FORCE)
  set(CMAKE_RANLIB "${bootlin_ranlib}" CACHE FILEPATH "" FORCE)
  set(CMAKE_STRIP "${bootlin_strip}" CACHE FILEPATH "" FORCE)
  set(CMAKE_NM "${bootlin_nm}" CACHE FILEPATH "" FORCE)
  set(CMAKE_OBJCOPY "${bootlin_objcopy}" CACHE FILEPATH "" FORCE)
  set(CMAKE_OBJDUMP "${bootlin_objdump}" CACHE FILEPATH "" FORCE)
  set(CMAKE_ADDR2LINE "${bootlin_addr2line}" CACHE FILEPATH "" FORCE)
  set(CMAKE_READELF "${bootlin_readelf}" CACHE FILEPATH "" FORCE)
  set(CMAKE_SYSROOT "${bootlin_sysroot}" CACHE PATH "" FORCE)
  set(CMAKE_FIND_ROOT_PATH "${bootlin_sysroot}" "${bootlin_root}" CACHE STRING "" FORCE)
  set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER CACHE STRING "" FORCE)
  set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY CACHE STRING "" FORCE)
  set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY CACHE STRING "" FORCE)
  set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY CACHE STRING "" FORCE)
endfunction()
```

For a cross target, the enclosing toolchain file must additionally set `CMAKE_SYSTEM_NAME` to `Linux`, set the target processor, and use `CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY` before calling the function.

For AFL++ fuzzing, first configure the ordinary Bootlin x86_64 collection, then resolve the pinned AFL++ wrapper. The wrapper must export `AFL_CC`/`AFL_CXX` as the matching Bootlin drivers and `AFL_PATH` as the cached helper root before it invokes `afl-gcc-fast` or `afl-g++-fast`. Fuzzing is native x86_64 Linux-only: no cross target, emulator, or QEMU runner is permitted.

An AFL++ CMake toolchain file must call the Bootlin setup before `project()`, then replace only the C/C++ compiler drivers with the resolver-reported wrappers. Keep the linker and all binary utilities from Bootlin:

```cmake
cpkt_configure_bootlin_toolchain(x86_64-linux-gnu)
execute_process(COMMAND "${CMAKE_SOURCE_DIR}/scripts/cpkt-aflpp.sh" discover
  RESULT_VARIABLE result OUTPUT_VARIABLE description ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Unable to provision pinned AFL++: ${error}")
endif()
foreach(key cc cxx helper root)
  string(REGEX MATCH "${key}=([^\r\n]+)" match "${description}")
  if(NOT match)
    message(FATAL_ERROR "AFL++ resolver did not report ${key}")
  endif()
  set(afl_${key} "${CMAKE_MATCH_1}")
endforeach()
set(ENV{AFL_PATH} "${afl_helper}")
set(CMAKE_C_COMPILER "${afl_cc}" CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${afl_cxx}" CACHE FILEPATH "" FORCE)
```

Assert collection integrity in the downstream bootstrap: the C compiler's reported linker must be inside the selected compiler root, and its libc must be inside the selected sysroot. This prevents an accidental host linker or host libc from entering an otherwise cross-target build.

## C89 CMake Policy

For project-owned C89 targets, do not set `CMAKE_C_STANDARD` or `C_STANDARD` to C90. CMake's C90 mapping does not express this lifecycle's intended compiler invocation. Apply these options directly to each project-owned target, in this order:

```cmake
function(project_configure_c89_target target)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(${target} PRIVATE
      -std=c89
      -Wall
      -Wextra
      -Wpedantic
      -pedantic-errors)
  endif()
endfunction()
```

Keep this policy off Lua sources and known C99 dependency implementations that are deliberately hidden behind C89 facades. Add project-specific `-Werror` policy separately when required by the lifecycle quality gate.

## Static C++ Runtime Contract

Linux resolver output includes `libstdcxx_a` and `libgcc_a` from the same Bootlin collection. Use them when building SDK metadata for C++-implemented C facades:

- Do not merge GNU runtime archives into facade archives.
- Ship the selected runtime archives in the SDK.
- Make static metadata place facade archives before the selected runtime archives.
- Require a downstream final static link that uses C++ to consume the same runtime closure.

For Darwin, do not apply the GNU runtime archive contract. osxcross/Apple Clang generally uses `libc++`; package metadata must emit target-correct runtime and system flags.

## Verification

Run these checks after changing either resolver or the toolchain policy:

```sh
bash -n skills/pkt-systems-cmake-lifecycle/scripts/cpkt-toolchains.sh
skills/pkt-systems-cmake-lifecycle/scripts/test-cpkt-toolchain-resolvers.sh
skills/pkt-systems-cmake-lifecycle/scripts/test-cpkt-aflpp-resolver.sh
skills/pkt-systems-cmake-lifecycle/scripts/cpkt-toolchains.sh discover
```

For a changed pin, also run `ensure` and a configure/build using that target. For AFL++ changes, run `cpkt-aflpp.sh ensure`, compile a small target through the wrapper, and prove `afl-showmap` observes distinct execution paths.

## Local execution with the selected libc

The complete Bootlin collection owns the runtime used to verify target code,
including on a native host. Classify executables by whether they are shipped,
not by build type: release-mode tests and in-tree examples are still local
verification artifacts.

Use one CMake helper to configure the collection ELF interpreter and private
runtime search paths on every non-shipped development executable: the local
CLI, unit/integration tests, compiled helpers, examples, benchmarks, fuzzers,
and instrumentation builds. Apply this to existing targets; do not create a
second test CLI. Native CTest, e2e scripts, and Make example targets execute
these binaries directly. Their project-built children select the same runtime
from their own ELF metadata. Fully static executables need no dynamic loader.
Foreign targets retain QEMU and the matching sysroot; do not apply a native
loader path to foreign executables. Never apply ELF flags to Darwin targets.

Keep these settings private to executable targets. Account for indirect
runtime dependencies: ELF DT_RUNPATH alone does not propagate to grandchildren;
local-executable DT_RPATH can provide this coverage. Include required compiler,
C++, and instrumentation runtimes where applicable. Check actual resolution;
a loader path or RPATH alone is not evidence of complete host-library exclusion.

Shipped libraries, executable artifacts, installed example sources, and exported
CMake/pkg-config metadata must not acquire collection-cache paths. An install
rule alone does not settle whether a target is shipped: prove that packaging
excludes local executables. Keep shared/static release behavior as defined by
the project and test actual release artifacts separately from local builds.

Temporary SDK verification consumers are also non-shipped executables. Configure
their CMake targets with the same helper; non-CMake/pkg-config verification links
use that helper's compiler/linker settings as well. Apply these settings only to
the generated verification project or its local build flags, never to installed
SDK metadata or example sources. Execute the resulting binaries directly.
Do not retain a generic runtime launcher, wrapper-specific tests, or a parallel
execution mode. Direct ELF interpreter selection preserves normal child exec
and `/proc/self/exe` behavior.

Do not export collection or project dependency library paths into native test
or example environments: even a bundled libcurl can contaminate a host child
process. Use private executable search paths instead. Host shells, Python, Git, ripgrep,
CMake, and Valgrind remain host programs. Do not intercept arbitrary execs or
introduce a process broker, container, or namespace solely to select libc.
Native AFL++ and Valgrind must be verified against the chosen target runtime.
Host tools do not establish the correctness of target-library runtime selection.

For Lua modules, the process loading the module owns the runtime. Prefer a local
embedding/interpreter executable built with this same policy. Changing module
linking cannot select its process's libc; host-only Lua tooling can remain host
programs. Installed-SDK consumers must compile with the selected collection and
run with its runtime without exporting local verification flags through the SDK.

Verify actual loaded runtime objects, direct and child execution, self-executable
behavior where used, independent host commands, Lua behavior, shared/static
consumers, and final artifact metadata. Add negative checks for missing or
mismatched runtimes and accidental host resolution. Record coverage limitations;
this is not hermetic execution or proof of older deployment-libc compatibility.
Ensure runtime checks also cover reduced/facade-only configurations; early
configuration returns must not bypass the checks. Execute runnable SDK consumers
as well as building and inspecting them, including direct-package metadata paths.
Run supported development/instrumentation configurations and the actual release
artifact checks. Measure workload changes caused by libc updates rather than
assuming unchanged performance.
