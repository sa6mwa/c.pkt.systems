#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

bundle_version=$(bash "$repo_root/scripts/release-version.sh" "$repo_root")
if [ -z "$bundle_version" ]; then
  printf 'failed to resolve c.pkt.systems bundle version\n' >&2
  exit 1
fi

archive_stem="c.pkt.systems-$bundle_version"
archive_name="$archive_stem.tar.gz"
dist_dir="$repo_root/dist"
archive_path="$dist_dir/$archive_name"
checksums_path="$dist_dir/c.pkt.systems-$bundle_version-CHECKSUMS"

find_gnu_tar() {
  if [ "${CPKT_GNU_TAR:-}" != "" ]; then
    if "$CPKT_GNU_TAR" --version 2>&1 | grep -F 'GNU tar' >/dev/null 2>&1; then
      printf '%s\n' "$CPKT_GNU_TAR"
      return 0
    fi
    printf 'CPKT_GNU_TAR is not GNU tar: %s\n' "$CPKT_GNU_TAR" >&2
    return 1
  fi

  for candidate_name in gtar tar; do
    candidate_path=$(command -v "$candidate_name" 2>/dev/null || true)
    if [ "$candidate_path" = "" ]; then
      continue
    fi
    if "$candidate_path" --version 2>&1 | grep -F 'GNU tar' >/dev/null 2>&1; then
      printf '%s\n' "$candidate_path"
      return 0
    fi
  done

  printf 'GNU tar is required for deterministic source archives; install gtar or set CPKT_GNU_TAR\n' >&2
  return 1
}

gnu_tar=$(find_gnu_tar)

stage_parent=$(mktemp -d "${TMPDIR:-/tmp}/cpkt-source-stage.XXXXXXXXXX")
cleanup() {
  rm -rf "$stage_parent"
}
trap cleanup EXIT HUP INT TERM

stage_root="$stage_parent/$archive_stem"
manifest_tmp="$stage_parent/source-files.txt"
manifest_with_generated="$stage_parent/source-files-with-generated.txt"
mkdir -p "$stage_root" "$dist_dir"

git_top_level=
if git_top_level=$(git -C "$repo_root" rev-parse --show-toplevel 2>/dev/null); then
  git_top_level=$(CDPATH= cd -- "$git_top_level" && pwd)
fi

if [ "$git_top_level" = "$repo_root" ]; then
  git -C "$repo_root" ls-files -z > "$stage_parent/git-files.z"
  tr '\0' '\n' < "$stage_parent/git-files.z" | sed '/^$/d' | sort > "$manifest_tmp"
else
  if [ ! -f "$repo_root/RELEASE_MANIFEST" ]; then
    printf 'RELEASE_MANIFEST is required when packaging source outside a git worktree\n' >&2
    exit 1
  fi
  sed '/^$/d' "$repo_root/RELEASE_MANIFEST" | sort > "$manifest_tmp"
fi

while IFS= read -r relative_path; do
  case "$relative_path" in
    ""|/*|*"/../"*|../*|*"/.."|.)
      printf 'invalid source archive manifest path: %s\n' "$relative_path" >&2
      exit 1
      ;;
    VERSION|RELEASE_MANIFEST)
      continue
      ;;
  esac
  if [ ! -f "$repo_root/$relative_path" ]; then
    printf 'source archive manifest path is not a regular file: %s\n' "$relative_path" >&2
    exit 1
  fi
  mkdir -p "$stage_root/$(dirname -- "$relative_path")"
  cp -p "$repo_root/$relative_path" "$stage_root/$relative_path"
done < "$manifest_tmp"

printf '%s\n' "$bundle_version" > "$stage_root/VERSION"
{
  grep -v -E '^(VERSION|RELEASE_MANIFEST)$' "$manifest_tmp" || true
  printf '%s\n' VERSION RELEASE_MANIFEST
} | sort > "$manifest_with_generated"
cp "$manifest_with_generated" "$stage_root/RELEASE_MANIFEST"

(
  cd "$stage_parent"
  "$gnu_tar" --sort=name --owner=0 --group=0 --numeric-owner -czf "$archive_path" -- "$archive_stem"
)

sha_output=$(cmake -E sha256sum "$archive_path")
sha_hash=${sha_output%% *}
tmp_checksums="$stage_parent/CHECKSUMS"
if [ -f "$checksums_path" ]; then
  grep -v -E "[[:space:]]$archive_name\$" "$checksums_path" > "$tmp_checksums" || true
fi
printf '%s  %s\n' "$sha_hash" "$archive_name" >> "$tmp_checksums"
mv "$tmp_checksums" "$checksums_path"

cmake \
  -DCPKT_ROOT="$repo_root" \
  -DCPKT_SCAN_LABEL="source archive" \
  -DCPKT_SCAN_PATHS="$archive_path" \
  -P "$repo_root/tests/privacy_scan.cmake"

printf '[package] wrote %s\n' "$archive_path"
