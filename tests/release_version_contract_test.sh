#!/usr/bin/env bash
set -euo pipefail

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
test_tag=v99.99.99
test_version=99.99.99
created_test_tag=

fail() {
  printf 'release_version_contract_test: %s\n' "$*" >&2
  exit 1
}

cleanup() {
  if [ -n "$created_test_tag" ]; then
    git -C "$repo_root" tag -d "$created_test_tag" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT HUP INT TERM

if ! git -C "$repo_root" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  fail "$repo_root is not a git worktree"
fi

git_top_level=$(git -C "$repo_root" rev-parse --show-toplevel)
git_top_level=$(CDPATH= cd -- "$git_top_level" && pwd -P)
if [ "$git_top_level" != "$repo_root" ]; then
  fail "run from repository root; got git top-level $git_top_level"
fi

if git -C "$repo_root" rev-parse -q --verify "refs/tags/$test_tag" >/dev/null; then
  git -C "$repo_root" tag -d "$test_tag" >/dev/null
fi

exact_tag=$(
  git -C "$repo_root" tag --points-at HEAD --list 'v[0-9]*.[0-9]*.[0-9]*' |
    sed -n '/^v[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*$/p' |
    sort -V |
    tail -n1
)

if [ -n "$exact_tag" ]; then
  exact_tag_type=$(git -C "$repo_root" cat-file -t "$exact_tag")
  [ "$exact_tag_type" = "commit" ] ||
    fail "exact HEAD tag $exact_tag must be a lightweight tag; got git object type $exact_tag_type"
  expected=${exact_tag#v}
  actual=$(bash "$repo_root/scripts/release-version.sh" "$repo_root")
  [ "$actual" = "$expected" ] ||
    fail "exact HEAD tag $exact_tag must resolve as $expected; got $actual"
  make_actual=$(make -s -C "$repo_root" print-release-version)
  [ "$make_actual" = "$expected" ] ||
    fail "make print-release-version must resolve exact HEAD tag $exact_tag as $expected; got $make_actual"
  printf 'release version contract passed for tagged HEAD %s\n' "$exact_tag"
  exit 0
fi

actual=$(bash "$repo_root/scripts/release-version.sh" "$repo_root")
[ "$actual" = "0.0.0" ] ||
  fail "untagged git worktree must resolve as 0.0.0 before reserved tag test; got $actual"

git -C "$repo_root" -c tag.gpgSign=false tag "$test_tag"
created_test_tag=$test_tag
test_tag_type=$(git -C "$repo_root" cat-file -t "$test_tag")
[ "$test_tag_type" = "commit" ] ||
  fail "reserved tag $test_tag must be lightweight; got git object type $test_tag_type"

actual=$(bash "$repo_root/scripts/release-version.sh" "$repo_root")
[ "$actual" = "$test_version" ] ||
  fail "reserved tag $test_tag must resolve as $test_version; got $actual"

make_actual=$(make -s -C "$repo_root" print-release-version)
[ "$make_actual" = "$test_version" ] ||
  fail "make print-release-version must resolve $test_tag as $test_version; got $make_actual"

git -C "$repo_root" tag -d "$test_tag" >/dev/null
created_test_tag=

actual=$(bash "$repo_root/scripts/release-version.sh" "$repo_root")
[ "$actual" = "0.0.0" ] ||
  fail "untagged git worktree must resolve as 0.0.0 after reserved tag cleanup; got $actual"

printf 'release version contract passed with reserved tag %s\n' "$test_tag"
