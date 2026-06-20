#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

osxcross_root="$work_dir/osxcross"
osxcross_host="custom-apple-darwin99"
mkdir -p "$osxcross_root/bin"
cat > "$osxcross_root/bin/$osxcross_host-otool" <<'SH'
#!/usr/bin/env sh
exit 0
SH
chmod +x "$osxcross_root/bin/$osxcross_host-otool"

output=$(
  OSXCROSS_ROOT="$osxcross_root" \
  CPKT_OSXCROSS_HOST="$osxcross_host" \
  cmake \
    -DCPKT_PACKAGE_ASSERTIONS_TEST_DARWIN_OTOOL_LOOKUP=ON \
    -P "$repo_root/cmake/package_assertions.cmake"
)

case "$output" in
  *"CPKT_TEST_OTOOL=$osxcross_root/bin/$osxcross_host-otool"*) ;;
  *)
    printf 'package assertion otool lookup did not honor CPKT_OSXCROSS_HOST\n%s\n' "$output" >&2
    exit 1
    ;;
esac
