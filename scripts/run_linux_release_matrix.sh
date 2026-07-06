#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

cd "$repo_root"

bash "$repo_root/scripts/package.sh"
bash "$repo_root/scripts/package-source.sh"
bash "$repo_root/scripts/verify-dist-manifest.sh" \
  "$repo_root/dist" \
  c.pkt.systems \
  "$(bash "$repo_root/scripts/release-version.sh" "$repo_root")"
bash "$repo_root/scripts/package-verify.sh"
