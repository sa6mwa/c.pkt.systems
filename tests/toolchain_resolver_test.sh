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

bootlin_description=$(CPKT_TOOLCHAIN_CACHE="$cache" "$bootlin" discover x86_64-linux-gnu)
require_line 'source=bootlin' "$bootlin_description"
require_line 'status=ready' "$bootlin_description"
require_line "cc=$bootlin_root/bin/x86_64-linux-gcc" "$bootlin_description"
require_line "ld=$bootlin_root/bin/x86_64-linux-ld" "$bootlin_description"
require_line "libstdcxx_a=$bootlin_root/runtime/libstdc++.a" "$bootlin_description"
bootlin_env=$(CPKT_TOOLCHAIN_CACHE="$cache" "$bootlin" env x86_64-linux-gnu)
grep -Fq "export CC=$bootlin_root/bin/x86_64-linux-gcc" <<<"$bootlin_env" || fail 'Bootlin environment omitted the compiler'
grep -Fq "export LD=$bootlin_root/bin/x86_64-linux-ld" <<<"$bootlin_env" || fail 'Bootlin environment omitted the linker'

printf '[test] pinned toolchain resolvers passed\n'
