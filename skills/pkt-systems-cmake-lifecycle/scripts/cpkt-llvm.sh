#!/usr/bin/env bash
set -euo pipefail

# Pinned diagnostics toolchain. Normal Linux builds use cpkt-toolchains.sh.

version='22.1.6'
resource_version='22'
runtime_triple='x86_64-unknown-linux-gnu'
archive_name="LLVM-${version}-Linux-X64.tar.xz"
archive_sha256='c5ac8ef89ca39d30cb32e9b83772f995dd891c685ebc188d593c943a64d5f8b5'
root_name="llvm-${version}-linux-x64"
url="https://github.com/llvm/llvm-project/releases/download/llvmorg-${version}/${archive_name}"

die() { printf 'cpkt-llvm: %s\n' "$*" >&2; exit 1; }

cache_root() {
  if [[ -n "${CPKT_TOOLCHAIN_CACHE:-}" ]]; then printf '%s\n' "$CPKT_TOOLCHAIN_CACHE"
  elif [[ -n "${XDG_CACHE_HOME:-}" ]]; then printf '%s/c.pkt.systems/toolchains\n' "$XDG_CACHE_HOME"
  elif [[ -n "${HOME:-}" ]]; then printf '%s/.cache/c.pkt.systems/toolchains\n' "$HOME"
  else die 'HOME, XDG_CACHE_HOME, or CPKT_TOOLCHAIN_CACHE is required'; fi
}

sha256_file() {
  if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | awk '{print $1}'
  elif command -v shasum >/dev/null 2>&1; then shasum -a 256 "$1" | awk '{print $1}'
  else die 'sha256sum or shasum is required'; fi
}

root_path() { printf '%s/roots/%s\n' "$(cache_root)" "$root_name"; }

toolchain_ready() {
  local root=$1 runtime="$root/lib/clang/${resource_version}/lib/${runtime_triple}"
  [[ -x "$root/bin/clang" ]] && [[ -x "$root/bin/clang++" ]] &&
    [[ -x "$root/bin/ld.lld" ]] && [[ -x "$root/bin/llvm-ar" ]] &&
    [[ -x "$root/bin/llvm-ranlib" ]] && [[ -x "$root/bin/llvm-strip" ]] &&
    [[ -x "$root/bin/llvm-nm" ]] && [[ -x "$root/bin/llvm-objcopy" ]] &&
    [[ -x "$root/bin/llvm-objdump" ]] && [[ -x "$root/bin/llvm-addr2line" ]] &&
    [[ -x "$root/bin/llvm-readelf" ]] &&
    [[ -f "$runtime/libclang_rt.asan.a" ]] &&
    [[ -f "$runtime/libclang_rt.fuzzer.a" ]] && [[ -f "$runtime/libclang_rt.msan.a" ]]
}

ensure() {
  local root archive_dir archive tmp extract actual
  root=$(root_path)
  toolchain_ready "$root" && return
  archive_dir="$(cache_root)/archives"
  archive="$archive_dir/$archive_name"
  mkdir -p "$archive_dir" "$(cache_root)/roots"
  if [[ ! -f "$archive" ]]; then
    tmp="$archive.tmp.$$"
    trap 'rm -f "$tmp"' EXIT HUP INT TERM
    if command -v curl >/dev/null 2>&1; then curl -fL --retry 3 --connect-timeout 20 --output "$tmp" "$url"
    elif command -v wget >/dev/null 2>&1; then wget -O "$tmp" "$url"
    else die 'curl or wget is required to download LLVM'; fi
    actual=$(sha256_file "$tmp")
    [[ "$actual" == "$archive_sha256" ]] || die "checksum mismatch for $archive_name: expected $archive_sha256, got $actual"
    mv "$tmp" "$archive"
    trap - EXIT HUP INT TERM
  else
    actual=$(sha256_file "$archive")
    [[ "$actual" == "$archive_sha256" ]] || die "cached archive checksum mismatch for $archive"
  fi
  extract="$(cache_root)/roots/.extract-$root_name.$$"
  trap 'rm -rf "$extract"' EXIT HUP INT TERM
  mkdir -p "$extract"
  tar -C "$extract" -xf "$archive"
  [[ -d "$extract/LLVM-${version}-Linux-X64" ]] || die "unexpected archive layout for $archive_name"
  rm -rf "$root"
  mv "$extract/LLVM-${version}-Linux-X64" "$root"
  rm -rf "$extract"
  trap - EXIT HUP INT TERM
  toolchain_ready "$root" || die "incomplete extracted LLVM toolchain: $root"
}

report() {
  local root runtime
  root=$(root_path)
  toolchain_ready "$root" || die "missing LLVM ${version}; run: $0 ensure"
  runtime="$root/lib/clang/${resource_version}/lib/${runtime_triple}"
  printf 'version=%s\ncache=%s\nsource=llvm-project\nroot=%s\n' "$version" "$(cache_root)" "$root"
  printf 'cc=%s\ncxx=%s\nld=%s\nar=%s\nranlib=%s\nstrip=%s\nnm=%s\nobjcopy=%s\nobjdump=%s\naddr2line=%s\nreadelf=%s\n' \
    "$root/bin/clang" "$root/bin/clang++" "$root/bin/ld.lld" "$root/bin/llvm-ar" "$root/bin/llvm-ranlib" "$root/bin/llvm-strip" "$root/bin/llvm-nm" "$root/bin/llvm-objcopy" "$root/bin/llvm-objdump" "$root/bin/llvm-addr2line" "$root/bin/llvm-readelf"
  printf 'asan_runtime=%s\nfuzzer_runtime=%s\nmsan_runtime=%s\n' \
    "$runtime/libclang_rt.asan.a" "$runtime/libclang_rt.fuzzer.a" "$runtime/libclang_rt.msan.a"
}

print_env() {
  local description key value
  description=$(report)
  for key in root cc cxx ld ar ranlib strip nm objcopy objdump addr2line readelf asan_runtime fuzzer_runtime msan_runtime; do
    value=$(printf '%s\n' "$description" | sed -n "s/^${key}=//p")
    printf 'export CPKT_LLVM_%s=%q\n' "${key^^}" "$value"
  done
  for key in CC CXX LD AR RANLIB STRIP NM; do
    value=$(printf '%s\n' "$description" | sed -n "s/^${key,,}=//p")
    printf 'export %s=%q\n' "$key" "$value"
  done
}

case "${1:-}" in
  ensure) [[ $# -eq 1 ]] || die 'usage: cpkt-llvm.sh ensure'; ensure ;;
  discover) [[ $# -eq 1 ]] || die 'usage: cpkt-llvm.sh discover'; report ;;
  env) [[ $# -eq 1 ]] || die 'usage: cpkt-llvm.sh env'; print_env ;;
  -h|--help|'') printf 'usage: %s <ensure|discover|env>\n' "$0" ;;
  *) die "unknown command: $1" ;;
esac
