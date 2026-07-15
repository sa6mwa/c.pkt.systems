#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

CMAKE=${CMAKE:-cmake}
CTEST=${CTEST:-ctest}

release_presets="x86_64-linux-gnu-release x86_64-linux-musl-release aarch64-linux-gnu-release aarch64-linux-musl-release armhf-linux-gnu-release armhf-linux-musl-release"

cd "$repo_root"

if ! bash "$repo_root/scripts/osxcross_available.sh"; then
  printf '[package] arm64-apple-darwin-release is required for c.pkt.systems releases; configure a complete local osxcross SDK toolchain\n' >&2
  exit 1
fi

for preset in $release_presets; do
  "$CMAKE" --preset "$preset"
  "$CMAKE" --build --preset "$preset"
  "$CTEST" --preset "$preset"
  "$CMAKE" --build --preset "package-$preset"
done

"$CMAKE" --preset arm64-apple-darwin-release
"$CMAKE" --build --preset arm64-apple-darwin-release
"$CMAKE" --build --preset package-arm64-apple-darwin-release
