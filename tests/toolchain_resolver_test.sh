#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  printf 'usage: toolchain_resolver_test.sh <source-dir>\n' >&2
  exit 2
fi

source_dir=$1
cache=$(mktemp -d)
trap 'rm -rf "$cache"' EXIT HUP INT TERM
bootlin="$source_dir/scripts/cpkt-toolchains.sh"
llvm="$source_dir/scripts/cpkt-llvm.sh"
fail() { printf 'toolchain resolver test: %s\n' "$*" >&2; exit 1; }
require_line() { grep -Fxq "$1" <<<"$2" || fail "missing output: $1"; }
make_executable() { printf '%b\n' "$2" > "$1"; chmod +x "$1"; }

bootlin_name=x86-64--glibc--stable-2025.08-1
bootlin_root="$cache/roots/$bootlin_name"
bootlin_sysroot="$bootlin_root/x86_64-buildroot-linux-gnu/sysroot"
mkdir -p "$bootlin_root/bin" "$bootlin_sysroot/usr/include" "$bootlin_sysroot/usr/lib" "$bootlin_root/runtime"
: > "$bootlin_sysroot/usr/include/stdio.h"; : > "$bootlin_sysroot/usr/lib/libc.so"
: > "$bootlin_root/runtime/libstdc++.a"; : > "$bootlin_root/runtime/libgcc.a"
for tool in gcc ld ar ranlib strip nm objcopy objdump addr2line gdb readelf; do make_executable "$bootlin_root/bin/x86_64-linux-$tool" '#!/bin/sh\nexit 0'; done
make_executable "$bootlin_root/bin/x86_64-linux-g++" "#!/bin/sh\ncase \"\$1\" in\n  -print-file-name=libstdc++.a) printf '%s\\n' '$bootlin_root/runtime/libstdc++.a' ;;\n  -print-file-name=libgcc.a) printf '%s\\n' '$bootlin_root/runtime/libgcc.a' ;;\n  *) exit 1 ;;\nesac"

make_llvm_collection() {
  llvm_collection_root=$1
  llvm_runtime_triple=$2
  llvm_collection_runtime="$llvm_collection_root/lib/clang/22/lib/$llvm_runtime_triple"
  mkdir -p "$llvm_collection_root/bin" "$llvm_collection_runtime"
  for tool in clang clang++ ld.lld llvm-ar llvm-ranlib llvm-strip llvm-nm llvm-objcopy llvm-objdump llvm-addr2line llvm-readelf; do make_executable "$llvm_collection_root/bin/$tool" '#!/bin/sh\nexit 0'; done
  : > "$llvm_collection_runtime/libclang_rt.asan.a"; : > "$llvm_collection_runtime/libclang_rt.fuzzer.a"; : > "$llvm_collection_runtime/libclang_rt.msan.a"
}

llvm_root="$cache/roots/llvm-22.1.6-linux-x64"
make_llvm_collection "$llvm_root" x86_64-unknown-linux-gnu
llvm_arm_root="$cache/roots/llvm-22.1.6-linux-arm64"
make_llvm_collection "$llvm_arm_root" aarch64-unknown-linux-gnu

bootlin_description=$(CPKT_TOOLCHAIN_CACHE="$cache" "$bootlin" discover x86_64-linux-gnu)
require_line 'source=bootlin' "$bootlin_description"
require_line 'status=ready' "$bootlin_description"
require_line "cc=$bootlin_root/bin/x86_64-linux-gcc" "$bootlin_description"
require_line "ld=$bootlin_root/bin/x86_64-linux-ld" "$bootlin_description"
require_line "libstdcxx_a=$bootlin_root/runtime/libstdc++.a" "$bootlin_description"
bootlin_env=$(CPKT_TOOLCHAIN_CACHE="$cache" "$bootlin" env x86_64-linux-gnu)
grep -Fq "export CC=$bootlin_root/bin/x86_64-linux-gcc" <<<"$bootlin_env" || fail 'Bootlin environment omitted the compiler'
grep -Fq "export LD=$bootlin_root/bin/x86_64-linux-ld" <<<"$bootlin_env" || fail 'Bootlin environment omitted the linker'

llvm_description=$(CPKT_TOOLCHAIN_CACHE="$cache" "$llvm" discover)
require_line 'version=22.1.6' "$llvm_description"
require_line "cc=$llvm_root/bin/clang" "$llvm_description"
require_line "ld=$llvm_root/bin/ld.lld" "$llvm_description"
llvm_env=$(CPKT_TOOLCHAIN_CACHE="$cache" "$llvm" env)
grep -Fq "export AR=$llvm_root/bin/llvm-ar" <<<"$llvm_env" || fail 'LLVM environment omitted the archiver'
grep -Fq "export LDFLAGS=-fuse-ld=$llvm_root/bin/ld.lld" <<<"$llvm_env" || fail 'LLVM environment did not force the pinned linker'

fake_bin="$cache/fake-bin"
mkdir -p "$fake_bin"
make_executable "$fake_bin/uname" '#!/bin/sh\nprintf "%s\\n" aarch64'
llvm_arm_description=$(PATH="$fake_bin:$PATH" CPKT_TOOLCHAIN_CACHE="$cache" "$llvm" discover)
require_line 'architecture=ARM64' "$llvm_arm_description"
require_line "cc=$llvm_arm_root/bin/clang" "$llvm_arm_description"
require_line "asan_runtime=$llvm_arm_root/lib/clang/22/lib/aarch64-unknown-linux-gnu/libclang_rt.asan.a" "$llvm_arm_description"

make_executable "$fake_bin/uname" '#!/bin/sh\nprintf "%s\\n" ppc64le'
if PATH="$fake_bin:$PATH" CPKT_TOOLCHAIN_CACHE="$cache" "$llvm" discover >/dev/null 2>&1; then
  fail 'LLVM resolver accepted an unsupported host architecture'
fi

printf '[test] pinned toolchain resolvers passed\n'
