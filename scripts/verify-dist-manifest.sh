#!/usr/bin/env bash
set -eu

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
  grep -E "[[:space:]]$1\$" "$checksums_path" >/dev/null 2>&1
}

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

while IFS= read -r manifest_entry; do
  case "$manifest_entry" in
    ""|\#*) continue ;;
  esac
  artifact_name=${manifest_entry##* }
  case "$artifact_name" in
    */*|""|.*)
      printf 'checksum manifest contains invalid artifact name: %s\n' "$artifact_name" >&2
      exit 1
      ;;
  esac
  if [ ! -f "$dist_dir/$artifact_name" ]; then
    printf 'checksum-listed artifact does not exist under dist: %s\n' "$artifact_name" >&2
    exit 1
  fi
done < "$checksums_path"
