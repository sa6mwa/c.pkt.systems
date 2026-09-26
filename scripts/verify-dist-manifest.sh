#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
  printf 'usage: %s <dist-dir> <project> <version>\n' "$0" >&2
  exit 2
fi

dist_dir=$1
project=$2
version=$3
checksums_name="$project-$version-CHECKSUMS"
checksums_path="$dist_dir/$checksums_name"

if [ ! -d "$dist_dir" ]; then
  printf 'dist directory does not exist: %s\n' "$dist_dir" >&2
  exit 1
fi
if [ ! -f "$checksums_path" ]; then
  printf 'missing checksum manifest: %s\n' "$checksums_path" >&2
  exit 1
fi
if [ -e "$dist_dir/SHA256SUMS" ]; then
  printf 'obsolete checksum manifest remains under dist: %s\n' "$dist_dir/SHA256SUMS" >&2
  exit 1
fi

is_release_artifact() {
  case "$1" in
    "$project"-*.tar.gz|"$project"-*-smoke-test.zip|"$project"-*-CHECKSUMS)
      return 0
      ;;
    *)
      return 1
      ;;
  esac
}

is_current_artifact() {
  case "$1" in
    "$project-$version".tar.gz|"$project-$version"-*.tar.gz|"$project-$version"-*-smoke-test.zip|"$checksums_name")
      return 0
      ;;
    *)
      return 1
      ;;
  esac
}

manifest_contains() {
  awk -v name="$1" '$2 == name { found = 1 } END { exit !found }' "$checksums_path"
}


seen_names=
entry_count=0
while IFS= read -r manifest_entry || [ -n "$manifest_entry" ]; do
  case "$manifest_entry" in
    ""|\#*) continue ;;
  esac
  if [[ ! $manifest_entry =~ ^([[:xdigit:]]{64})[[:blank:]][[:blank:]]([^[:space:]]+)$ ]]; then
    printf 'checksum manifest contains malformed SHA-256 entry: %s\n' "$manifest_entry" >&2
    exit 1
  fi
  expected_hash=${BASH_REMATCH[1]}
  expected_hash=$(printf '%s' "$expected_hash" | tr '[:upper:]' '[:lower:]')
  artifact_name=${BASH_REMATCH[2]}
  case "$artifact_name" in
    */*|""|.*)
      printf 'checksum manifest contains invalid artifact name: %s\n' "$artifact_name" >&2
      exit 1
      ;;
  esac
  if ! is_release_artifact "$artifact_name" || ! is_current_artifact "$artifact_name" ||
      [ "$artifact_name" = "$checksums_name" ]; then
    printf 'checksum manifest contains unexpected release artifact: %s\n' "$artifact_name" >&2
    exit 1
  fi
  if printf '%s\n' "$seen_names" | grep -Fxq -- "$artifact_name"; then
    printf 'checksum manifest lists artifact more than once: %s\n' "$artifact_name" >&2
    exit 1
  fi
  seen_names="$seen_names
$artifact_name"
  if [ ! -f "$dist_dir/$artifact_name" ]; then
    printf 'checksum-listed artifact does not exist under dist: %s\n' "$artifact_name" >&2
    exit 1
  fi
  actual_hash=$(cmake -E sha256sum "$dist_dir/$artifact_name")
  actual_hash=${actual_hash%% *}
  if [ "$expected_hash" != "$actual_hash" ]; then
    printf 'SHA-256 mismatch for release artifact: %s\n' "$artifact_name" >&2
    exit 1
  fi
  entry_count=$((entry_count + 1))
done < "$checksums_path"
if [ "$entry_count" -eq 0 ]; then
  printf 'checksum manifest contains no release artifacts: %s\n' "$checksums_path" >&2
  exit 1
fi

while IFS= read -r artifact_path; do
  artifact_name=$(basename -- "$artifact_path")
  if ! is_release_artifact "$artifact_name"; then
    continue
  fi
  if ! is_current_artifact "$artifact_name"; then
    printf 'stale release artifact for a different version remains under dist: %s\n' "$artifact_path" >&2
    exit 1
  fi
  if [ "$artifact_name" != "$checksums_name" ] && ! manifest_contains "$artifact_name"; then
    printf 'release-looking artifact is not listed in %s: %s\n' "$checksums_path" "$artifact_path" >&2
    exit 1
  fi
done <<EOF
$(find "$dist_dir" -maxdepth 1 -type f | sort)
EOF
