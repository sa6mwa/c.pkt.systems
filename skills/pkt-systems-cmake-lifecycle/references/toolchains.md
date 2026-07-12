# CPKT Toolchain Reference

This lifecycle owns C and C++ compiler resolution for pkt.systems C/CMake projects. Linux builds use only complete, pinned compiler collections cached outside the checkout. A Linux target must not select a host-installed GCC, Clang, linker, binutils collection, libc, headers, or C++ runtime.

## Compiler Policy

- Every ordinary Linux build uses the pinned Bootlin GCC collection for its target. Its triple-prefixed `gcc`, `g++`, `ld`, `ar`, `ranlib`, `strip`, `nm`, `objcopy`, `objdump`, `addr2line`, `gdb`, and `readelf`, plus its sysroot libc and headers, are one inseparable collection.
- Do not use `/usr/bin/cc`, `gcc`, `clang`, distro cross compilers, or unpinned compiler paths as a fallback. A cached Bootlin collection is the only Linux default.
- `arm64-apple-darwin` remains local-osxcross-only. The lifecycle discovers a complete osxcross collection but must not download Apple SDKs or Darwin compiler collections.
- Native memory checking uses host-provided Valgrind against executables compiled by the selected Bootlin collection. It is a required gate on the native x86_64 Linux host, but it is not an MSan substitute. Never run Valgrind through cross-compilation, an emulator, or QEMU.
- Native fuzzing uses a pinned cached AFL++ release built with the matching x86_64 Bootlin GCC plugin headers. AFL++ compiler wrappers must delegate to the selected Bootlin `gcc`/`g++`; never use host GCC or Clang for project targets. Never run fuzzing through cross-compilation, an emulator, or QEMU.
- `clang-format` and `clangd` are host OS development-tool prerequisites only. They must not enter CMake compiler or linker discovery.

## Linux Targets

| Target | Pinned Bootlin collection | Compiler prefix | Sysroot |
| --- | --- | --- | --- |
| `x86_64-linux-gnu` | `x86-64--glibc--stable-2025.08-1` | `x86_64-linux` | `x86_64-buildroot-linux-gnu/sysroot` |
| `x86_64-linux-musl` | `x86-64--musl--stable-2025.08-1` | `x86_64-linux` | `x86_64-buildroot-linux-musl/sysroot` |
| `aarch64-linux-gnu` | `aarch64--glibc--stable-2025.08-1` | `aarch64-linux` | `aarch64-buildroot-linux-gnu/sysroot` |
| `aarch64-linux-musl` | `aarch64--musl--stable-2025.08-1` | `aarch64-linux` | `aarch64-buildroot-linux-musl/sysroot` |
| `armhf-linux-gnu` | `armv7-eabihf--glibc--stable-2025.08-1` | `arm-linux` | `arm-buildroot-linux-gnueabihf/sysroot` |
| `armhf-linux-musl` | `armv7-eabihf--musl--stable-2025.08-1` | `arm-linux` | `arm-buildroot-linux-musleabihf/sysroot` |

The resolver pins each tarball SHA-256. Change a Bootlin pin only by updating its archive name, URL architecture, checksum, compiler prefix, and sysroot as one atomic lifecycle change.

## Cache Layout

Default root:

```sh
${CPKT_TOOLCHAIN_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/c.pkt.systems/toolchains}
```

- `archives/` contains verified Bootlin and AFL++ source tarballs.
- `roots/` contains extracted immutable compiler collections.

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
