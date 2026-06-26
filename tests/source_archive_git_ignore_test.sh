#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

work_root=$(mktemp -d "${TMPDIR:-/tmp}/cpkt-source-ignore-test.XXXXXXXXXX")
cleanup() {
  rm -rf "$work_root"
}
trap cleanup EXIT HUP INT TERM

source_root="$work_root/source"
mkdir -p "$source_root/scripts" "$source_root/tests" "$source_root/build" "$source_root/dist"
git -C "$source_root" init -q
git -C "$source_root" config user.email test@example.invalid
git -C "$source_root" config user.name "c.pkt.systems test"

cp "$repo_root/scripts/package-source.sh" "$source_root/scripts/package-source.sh"
cp "$repo_root/scripts/release-version.sh" "$source_root/scripts/release-version.sh"
cp "$repo_root/tests/privacy_scan.cmake" "$source_root/tests/privacy_scan.cmake"
printf 'build/\ndist/\nignored.txt\n' > "$source_root/.gitignore"
printf 'tracked\n' > "$source_root/tracked.txt"
git -C "$source_root" add .gitignore scripts/package-source.sh scripts/release-version.sh tests/privacy_scan.cmake tracked.txt
git -C "$source_root" commit -q -m source

printf 'ignored\n' > "$source_root/ignored.txt"
printf 'generated\n' > "$source_root/build/generated.txt"
printf 'stale\n' > "$source_root/dist/stale.txt"
printf 'untracked\n' > "$source_root/untracked-visible.txt"

bash "$source_root/scripts/package-source.sh" >/dev/null

archive_path="$source_root/dist/c.pkt.systems-0.0.0.tar.gz"
if [ ! -f "$archive_path" ]; then
  printf 'source archive was not created at %s\n' "$archive_path" >&2
  exit 1
fi

listing=$(tar -tzf "$archive_path")
for expected in \
  "c.pkt.systems-0.0.0/.gitignore" \
  "c.pkt.systems-0.0.0/tracked.txt" \
  "c.pkt.systems-0.0.0/VERSION" \
  "c.pkt.systems-0.0.0/RELEASE_MANIFEST"
do
  case "$listing" in
    *"$expected"*) ;;
    *)
      printf 'source archive missing expected tracked/generated path: %s\n' "$expected" >&2
      exit 1
      ;;
  esac
done

for forbidden in \
  "ignored.txt" \
  "build/generated.txt" \
  "dist/stale.txt" \
  "untracked-visible.txt" \
  ".git/"
do
  case "$listing" in
    *"$forbidden"*)
      printf 'source archive included ignored/untracked/generated path: %s\n' "$forbidden" >&2
      exit 1
      ;;
  esac
done

tmp_extract="$work_root/extract"
mkdir -p "$tmp_extract"
(
  cd "$tmp_extract"
  tar -xzf "$archive_path"
)
if ! diff -u \
    <(cd "$tmp_extract/c.pkt.systems-0.0.0" && find . -type f | sed 's#^\./##' | sort) \
    <(sort "$tmp_extract/c.pkt.systems-0.0.0/RELEASE_MANIFEST"); then
  printf 'source archive RELEASE_MANIFEST does not match payload\n' >&2
  exit 1
fi

printf '[test] source archive git ignore passed\n'
