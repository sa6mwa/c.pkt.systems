#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

if git -C "$repo_root" ls-files --error-unmatch VERSION >/dev/null 2>&1; then
  if [ -e "$repo_root/VERSION" ]; then
    printf 'git worktree must not contain tracked repository-local VERSION\n' >&2
    exit 1
  fi
fi
if ! git -C "$repo_root" check-ignore -q VERSION; then
  if ! grep -qxF '/VERSION' "$repo_root/.gitignore"; then
    printf 'git worktree must ignore repository-local VERSION\n' >&2
    exit 1
  fi
fi

work_dir=$(mktemp -d "${TMPDIR:-/tmp}/cpkt-version-test.XXXXXXXXXX")
cleanup() {
  rm -rf "$work_dir"
}
trap cleanup EXIT HUP INT TERM

non_git="$work_dir/non-git"
mkdir -p "$non_git"
printf '1.2.3\n' > "$non_git/VERSION"
resolved=$(bash "$repo_root/scripts/release-version.sh" "$non_git")
if [ "$resolved" != "1.2.3" ]; then
  printf 'non-git VERSION resolved to %s, expected 1.2.3\n' "$resolved" >&2
  exit 1
fi

parent_git="$work_dir/parent-git"
archive_under_parent="$parent_git/third_party/c.pkt.systems-4.5.6"
mkdir -p "$archive_under_parent"
git -C "$parent_git" init -q
git -C "$parent_git" config user.email test@example.invalid
git -C "$parent_git" config user.name "c.pkt.systems test"
printf 'parent\n' > "$parent_git/parent.txt"
git -C "$parent_git" add parent.txt
git -C "$parent_git" commit -q -m parent
git -C "$parent_git" tag v9.9.9
printf '4.5.6\n' > "$archive_under_parent/VERSION"
resolved=$(bash "$repo_root/scripts/release-version.sh" "$archive_under_parent")
if [ "$resolved" != "4.5.6" ]; then
  printf 'source tree under parent git resolved to %s, expected 4.5.6\n' "$resolved" >&2
  exit 1
fi

missing="$work_dir/missing"
mkdir -p "$missing"
if bash "$repo_root/scripts/release-version.sh" "$missing" >/dev/null 2>&1; then
  printf 'non-git directory without VERSION unexpectedly resolved a release version\n' >&2
  exit 1
fi

git_repo="$work_dir/git-repo"
mkdir -p "$git_repo"
git -C "$git_repo" init -q
git -C "$git_repo" config user.email test@example.invalid
git -C "$git_repo" config user.name "c.pkt.systems test"
printf 'content\n' > "$git_repo/file.txt"
printf '/VERSION\n' > "$git_repo/.gitignore"
git -C "$git_repo" add .gitignore file.txt
git -C "$git_repo" commit -q -m init
printf '9.9.9\n' > "$git_repo/VERSION"
resolved=$(bash "$repo_root/scripts/release-version.sh" "$git_repo")
if [ "$resolved" != "0.0.0" ]; then
  printf 'untagged git worktree resolved to %s, expected 0.0.0\n' "$resolved" >&2
  exit 1
fi

git -C "$git_repo" tag v2.3.4
resolved=$(bash "$repo_root/scripts/release-version.sh" "$git_repo")
if [ "$resolved" != "2.3.4" ]; then
  printf 'tagged git worktree resolved to %s, expected 2.3.4\n' "$resolved" >&2
  exit 1
fi

git -C "$git_repo" tag -d v2.3.4 >/dev/null
git -C "$git_repo" tag -a v3.4.5 -m annotated
resolved=$(bash "$repo_root/scripts/release-version.sh" "$git_repo")
if [ "$resolved" != "0.0.0" ]; then
  printf 'annotated tagged git worktree resolved to %s, expected 0.0.0\n' "$resolved" >&2
  exit 1
fi

git -C "$git_repo" tag v4.5.6
resolved=$(bash "$repo_root/scripts/release-version.sh" "$git_repo")
if [ "$resolved" != "4.5.6" ]; then
  printf 'lightweight tag must win over annotated tags; resolved to %s, expected 4.5.6\n' "$resolved" >&2
  exit 1
fi

printf '[test] version resolution passed\n'
