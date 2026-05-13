#!/usr/bin/env bash
set -eu

if [ "$#" -ne 3 ]; then
  printf 'usage: %s <archive> <target-id> <source-file>\n' "$0" >&2
  exit 2
fi

archive=$1
target_id=$2
source_file=$3

case "$target_id" in
  x86_64-linux-gnu)
    cc=${CC:-/usr/bin/cc}
    run_prefix=
    static_extra_libs=
    ;;
  x86_64-linux-musl)
    cc=${CC:-/usr/bin/musl-gcc}
    run_prefix=
    static_extra_libs=
    ;;
  aarch64-linux-gnu)
    cc=${CC:-/usr/bin/aarch64-linux-gnu-gcc}
    run_prefix="/usr/bin/qemu-aarch64 -L /usr/aarch64-linux-gnu"
    static_extra_libs=
    ;;
  armhf-linux-gnu)
    cc=${CC:-/usr/bin/arm-linux-gnueabihf-gcc}
    run_prefix="/usr/bin/qemu-arm -L /usr/arm-linux-gnueabihf"
    static_extra_libs=-latomic
    ;;
  aarch64-linux-musl)
    musl_prefix=${CPKT_AARCH64_MUSL_PREFIX:-"$HOME/.local/cross/aarch64-linux-musl"}
    cc=${CC:-"$musl_prefix/bin/aarch64-linux-musl-gcc"}
    run_prefix="/usr/bin/qemu-aarch64 -L $musl_prefix/aarch64-linux-musl"
    static_extra_libs=
    ;;
  armhf-linux-musl)
    musl_prefix=${CPKT_ARMHF_MUSL_PREFIX:-"$HOME/.local/cross/arm-linux-musleabihf"}
    cc=${CC:-"$musl_prefix/bin/arm-linux-musleabihf-gcc"}
    run_prefix="/usr/bin/qemu-arm -L $musl_prefix/arm-linux-musleabihf"
    static_extra_libs=-latomic
    ;;
  *)
    printf 'package install smoke only supports Linux targets, got: %s\n' "$target_id" >&2
    exit 2
    ;;
esac

if [ ! -x "$cc" ]; then
  printf 'compiler for %s is not executable: %s\n' "$target_id" "$cc" >&2
  exit 1
fi

if [ ! -f "$archive" ]; then
  printf 'archive does not exist: %s\n' "$archive" >&2
  exit 1
fi
case "$archive" in
  /*) ;;
  *) archive=$(CDPATH= cd -- "$(dirname -- "$archive")" && pwd)/$(basename -- "$archive") ;;
esac

if [ ! -f "$source_file" ]; then
  printf 'source file does not exist: %s\n' "$source_file" >&2
  exit 1
fi

work_root=$(mktemp -d "${TMPDIR:-/tmp}/cpkt-install-smoke.XXXXXX")
trap 'rm -rf "$work_root"' EXIT

(cd "$work_root" && cmake -E tar xf "$archive")
prefix=$(find "$work_root" -mindepth 1 -maxdepth 1 -type d | head -n1)
if [ -z "$prefix" ]; then
  printf 'archive did not extract to a top-level prefix: %s\n' "$archive" >&2
  exit 1
fi

mkdir -p "$work_root/bin"

common_flags="-std=c89 -Wall -Wextra -Wpedantic -isystem $prefix/include"
shared_out="$work_root/bin/cpkt-package-shared-smoke"
static_out="$work_root/bin/cpkt-package-static-smoke"

"$cc" $common_flags "$source_file" \
  -L"$prefix/lib" -Wl,-rpath,"$prefix/lib" \
  -o "$shared_out" \
  -lcurl -lssh2 -lnghttp2 -lssl -lcrypto -lz -ldl -pthread

"$cc" $common_flags "$source_file" \
  -o "$static_out" \
  "$prefix/lib/libcurl.a" \
  "$prefix/lib/libssh2.a" \
  "$prefix/lib/libnghttp2.a" \
  "$prefix/lib/libssl.a" \
  "$prefix/lib/libcrypto.a" \
  "$prefix/lib/libz.a" \
  -ldl -pthread $static_extra_libs

if [ -z "$run_prefix" ]; then
  "$shared_out"
  "$static_out"
else
  # shellcheck disable=SC2086
  $run_prefix "$shared_out"
  # shellcheck disable=SC2086
  $run_prefix "$static_out"
fi
