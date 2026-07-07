#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'USAGE'
usage: scripts/ensure-toolchain.sh <target-id>

Ensures that a pinned Linux cross toolchain exists in the persistent toolchain
cache and prints its root directory. The cache root defaults to:

  ${XDG_CACHE_HOME:-$HOME/.cache}/c.pkt.systems/toolchains

Apple/Darwin toolchains are intentionally not managed here.
USAGE
}

die() {
  printf 'ensure-toolchain: %s\n' "$*" >&2
  exit 1
}

sha256_file() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | awk '{print $1}'
  elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$1" | awk '{print $1}'
  else
    die "sha256sum or shasum is required"
  fi
}

download_file() {
  url=$1
  dst=$2
  if command -v curl >/dev/null 2>&1; then
    curl -fL --retry 3 --connect-timeout 20 --output "$dst" "$url"
  elif command -v wget >/dev/null 2>&1; then
    wget -O "$dst" "$url"
  else
    die "curl or wget is required to download toolchains"
  fi
}

target_id=${1:-}
if [ -z "$target_id" ] || [ "$target_id" = "-h" ] || [ "$target_id" = "--help" ]; then
  usage
  exit 2
fi

case "$target_id" in
  x86_64-linux-musl)
    bootlin_arch=x86-64
    toolchain_name=x86-64--musl--stable-2025.08-1
    sha256=09fca3aa89540f1b01b5f4210d488cbeb00f522044c53e9989b1dd8a38076912
    ;;
  aarch64-linux-gnu)
    bootlin_arch=aarch64
    toolchain_name=aarch64--glibc--stable-2025.08-1
    sha256=dfb47eee874eef9e8a7fc042eee4e0a183f444b6bcde6a82fef8f009918389c9
    ;;
  aarch64-linux-musl)
    bootlin_arch=aarch64
    toolchain_name=aarch64--musl--stable-2025.08-1
    sha256=defba831ffa1175236f137069333e21ed46d4d19feb5080a90cf248b6fc2cb08
    ;;
  armhf-linux-gnu)
    bootlin_arch=armv7-eabihf
    toolchain_name=armv7-eabihf--glibc--stable-2025.08-1
    sha256=97d6fbaf19832002f3d6aa8fd31b2d29c1dc7b0752f4ae8ed35860fd33c1f9b4
    ;;
  armhf-linux-musl)
    bootlin_arch=armv7-eabihf
    toolchain_name=armv7-eabihf--musl--stable-2025.08-1
    sha256=2f3a34458c3a8b961bd09f89669130fcdc4c1dbc6e31ada720527e4ad3741c11
    ;;
  *)
    die "unsupported auto-toolchain target: $target_id"
    ;;
esac

if [ -n "${CPKT_TOOLCHAIN_CACHE:-}" ]; then
  cache_root=$CPKT_TOOLCHAIN_CACHE
elif [ -n "${XDG_CACHE_HOME:-}" ]; then
  cache_root=$XDG_CACHE_HOME/c.pkt.systems/toolchains
elif [ -n "${HOME:-}" ]; then
  cache_root=$HOME/.cache/c.pkt.systems/toolchains
else
  die "HOME or XDG_CACHE_HOME is required when CPKT_TOOLCHAIN_CACHE is unset"
fi

archive_name=$toolchain_name.tar.xz
base_url=https://toolchains.bootlin.com/downloads/releases/toolchains/$bootlin_arch/tarballs
archive_url=$base_url/$archive_name
archive_dir=$cache_root/archives
root_dir=$cache_root/roots/$toolchain_name
archive_path=$archive_dir/$archive_name

if [ -d "$root_dir/bin" ]; then
  printf '%s\n' "$root_dir"
  exit 0
fi

mkdir -p "$archive_dir" "$cache_root/roots"

if [ ! -f "$archive_path" ]; then
  tmp_archive=$archive_path.tmp.$$
  trap 'rm -f "$tmp_archive"' EXIT HUP INT TERM
  download_file "$archive_url" "$tmp_archive"
  actual=$(sha256_file "$tmp_archive")
  if [ "$actual" != "$sha256" ]; then
    die "checksum mismatch for $archive_name: expected $sha256, got $actual"
  fi
  mv "$tmp_archive" "$archive_path"
  trap - EXIT HUP INT TERM
else
  actual=$(sha256_file "$archive_path")
  if [ "$actual" != "$sha256" ]; then
    die "cached archive checksum mismatch for $archive_path: expected $sha256, got $actual"
  fi
fi

tmp_extract=$cache_root/roots/.extract-$toolchain_name.$$
trap 'rm -rf "$tmp_extract"' EXIT HUP INT TERM
mkdir -p "$tmp_extract"
tar -C "$tmp_extract" -xf "$archive_path"
if [ ! -d "$tmp_extract/$toolchain_name/bin" ]; then
  die "downloaded toolchain has unexpected layout: $archive_name"
fi
rm -rf "$root_dir"
mv "$tmp_extract/$toolchain_name" "$root_dir"
rm -rf "$tmp_extract"
trap - EXIT HUP INT TERM

printf '%s\n' "$root_dir"
