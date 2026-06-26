#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

work_root=$(mktemp -d "${TMPDIR:-/tmp}/cpkt-source-parent-test.XXXXXXXXXX")
cleanup() {
  rm -rf "$work_root"
}
trap cleanup EXIT HUP INT TERM

parent_git="$work_root/parent"
source_root="$parent_git/third_party/c.pkt.systems-7.8.9"
mkdir -p "$source_root/scripts" "$source_root/tests"
git -C "$parent_git" init -q
git -C "$parent_git" config user.email test@example.invalid
git -C "$parent_git" config user.name "c.pkt.systems test"
printf 'parent-only\n' > "$parent_git/parent-only.txt"
git -C "$parent_git" add parent-only.txt
git -C "$parent_git" commit -q -m parent
git -C "$parent_git" tag v1.2.3

cp "$repo_root/scripts/package-source.sh" "$source_root/scripts/package-source.sh"
cp "$repo_root/scripts/release-version.sh" "$source_root/scripts/release-version.sh"
cp "$repo_root/tests/privacy_scan.cmake" "$source_root/tests/privacy_scan.cmake"
printf '7.8.9\n' > "$source_root/VERSION"
printf 'payload\n' > "$source_root/payload.txt"
printf 'payload.txt\n' > "$source_root/RELEASE_MANIFEST"

bash "$source_root/scripts/package-source.sh" >/dev/null

archive_path="$source_root/dist/c.pkt.systems-7.8.9.tar.gz"
if [ ! -f "$archive_path" ]; then
  printf 'nested source archive was not created at %s\n' "$archive_path" >&2
  exit 1
fi

listing=$(tar -tzf "$archive_path")
case "$listing" in
  *"c.pkt.systems-7.8.9/payload.txt"*) ;;
  *)
    printf 'nested source archive did not include RELEASE_MANIFEST payload\n' >&2
    exit 1
    ;;
esac
case "$listing" in
  *"parent-only.txt"*)
    printf 'nested source archive incorrectly packaged parent git files\n' >&2
    exit 1
    ;;
esac

printf '[test] source archive parent git passed\n'
