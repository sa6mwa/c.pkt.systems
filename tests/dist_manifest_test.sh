#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

mkdir -p "$repo_root/build"
work_root=$(mktemp -d "$repo_root/build/cpkt-dist-manifest-test.XXXXXXXXXX")
cleanup() {
  rm -rf "$work_root"
}
trap cleanup EXIT HUP INT TERM

dist_dir="$work_root/dist"
mkdir -p "$dist_dir"
touch "$dist_dir/c.pkt.systems-1.2.3-x86_64-linux-gnu.tar.gz"
touch "$dist_dir/c.pkt.systems-1.2.3.tar.gz"
touch "$dist_dir/c.pkt.systems-1.2.3-arm64-apple-darwin-smoke-test.zip"
manifest="$dist_dir/c.pkt.systems-1.2.3-CHECKSUMS"
for artifact in "$dist_dir"/*.tar.gz "$dist_dir"/*.zip; do
  hash=$(cmake -E sha256sum "$artifact")
  printf '%s  %s\n' "${hash%% *}" "${artifact##*/}" >> "$manifest"
done
cp "$manifest" "$work_root/valid-CHECKSUMS"

expect_failure() {
  local diagnostic=$1
  if bash "$repo_root/scripts/verify-dist-manifest.sh" "$dist_dir" c.pkt.systems 1.2.3 > "$work_root/failure.log" 2>&1; then
    printf 'dist manifest accepted invalid fixture: %s\n' "$diagnostic" >&2
    exit 1
  fi
  grep -F -- "$diagnostic" "$work_root/failure.log" >/dev/null || {
    cat "$work_root/failure.log" >&2
    exit 1
  }
}

bash "$repo_root/scripts/verify-dist-manifest.sh" "$dist_dir" c.pkt.systems 1.2.3

touch "$dist_dir/c.pkt.systems-1.2.3-x86_64-linux-musl.tar.gz"
if bash "$repo_root/scripts/verify-dist-manifest.sh" "$dist_dir" c.pkt.systems 1.2.3 >/dev/null 2>&1; then
  printf 'dist manifest accepted an unlisted current-version artifact\n' >&2
  exit 1
fi
rm "$dist_dir/c.pkt.systems-1.2.3-x86_64-linux-musl.tar.gz"

touch "$dist_dir/c.pkt.systems-1.2.3-extra-smoke-test.zip"
if bash "$repo_root/scripts/verify-dist-manifest.sh" "$dist_dir" c.pkt.systems 1.2.3 >/dev/null 2>&1; then
  printf 'dist manifest accepted an unlisted current-version smoke artifact\n' >&2
  exit 1
fi
rm "$dist_dir/c.pkt.systems-1.2.3-extra-smoke-test.zip"

touch "$dist_dir/c.pkt.systems-1.2.2-x86_64-linux-gnu.tar.gz"
if bash "$repo_root/scripts/verify-dist-manifest.sh" "$dist_dir" c.pkt.systems 1.2.3 >/dev/null 2>&1; then
  printf 'dist manifest accepted a stale binary artifact\n' >&2
  exit 1
fi
rm "$dist_dir/c.pkt.systems-1.2.2-x86_64-linux-gnu.tar.gz"

touch "$dist_dir/c.pkt.systems-1.2.2-CHECKSUMS"
if bash "$repo_root/scripts/verify-dist-manifest.sh" "$dist_dir" c.pkt.systems 1.2.3 >/dev/null 2>&1; then
  printf 'dist manifest accepted a stale checksum manifest\n' >&2
  exit 1
fi

rm "$dist_dir/c.pkt.systems-1.2.2-CHECKSUMS"

for artifact in "$dist_dir"/*.tar.gz "$dist_dir"/*.zip; do
  printf 'corrupt\n' > "$artifact"
  expect_failure 'SHA-256 mismatch'
  : > "$artifact"
done
cat "$work_root/valid-CHECKSUMS" >> "$manifest"
expect_failure 'more than once'
printf 'malformed  c.pkt.systems-1.2.3.tar.gz\n' > "$manifest"
expect_failure 'malformed SHA-256'
: > "$manifest"
expect_failure 'contains no release artifacts'
cp "$work_root/valid-CHECKSUMS" "$manifest"
touch "$dist_dir/SHA256SUMS"
expect_failure 'obsolete checksum'
rm "$dist_dir/SHA256SUMS"
hash=$(cmake -E sha256sum "$dist_dir/c.pkt.systems-1.2.3.tar.gz")
for unexpected in c.pkt.systems-1.2.2.tar.gz arbitrary.txt ../outside.tar.gz; do
  printf '%s  %s\n' "${hash%% *}" "$unexpected" >> "$manifest"
  expect_failure 'checksum manifest contains'
  cp "$work_root/valid-CHECKSUMS" "$manifest"
done
bash "$repo_root/scripts/verify-dist-manifest.sh" "$dist_dir" c.pkt.systems 1.2.3

printf '[test] dist manifest passed\n'
