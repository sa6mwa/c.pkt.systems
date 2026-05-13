#!/usr/bin/env bash
set -eu

repo_root=${1:-}
if [ -z "$repo_root" ]; then
  repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
fi

version_tag=$(
  git -C "$repo_root" tag --points-at HEAD --list 'v[0-9]*.[0-9]*.[0-9]*' |
    sed -n '/^v[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*$/p' |
    sort -V |
    tail -n1
)

if [ -z "$version_tag" ]; then
  printf '0.0.0\n'
else
  printf '%s\n' "${version_tag#v}"
fi
