#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

work_root=$(mktemp -d "${TMPDIR:-/tmp}/cpkt-dist-manifest-test.XXXXXXXXXX")
cleanup() {
  rm -rf "$work_root"
}
trap cleanup EXIT HUP INT TERM

dist_dir="$work_root/dist"
mkdir -p "$dist_dir"
touch "$dist_dir/c.pkt.systems-1.2.3-x86_64-linux-gnu.tar.gz"
touch "$dist_dir/c.pkt.systems-1.2.3.tar.gz"
touch "$dist_dir/c.pkt.systems-1.2.3-arm64-apple-darwin-smoke-test.zip"
cat > "$dist_dir/c.pkt.systems-1.2.3-CHECKSUMS" <<'EOF'
0000000000000000000000000000000000000000000000000000000000000000  c.pkt.systems-1.2.3-x86_64-linux-gnu.tar.gz
1111111111111111111111111111111111111111111111111111111111111111  c.pkt.systems-1.2.3.tar.gz
2222222222222222222222222222222222222222222222222222222222222222  c.pkt.systems-1.2.3-arm64-apple-darwin-smoke-test.zip
EOF

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

printf '[test] dist manifest passed\n'
