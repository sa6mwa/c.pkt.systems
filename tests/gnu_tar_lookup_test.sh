#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
real_tar=$(command -v tar)

work_root=$(mktemp -d "${TMPDIR:-/tmp}/cpkt-gnu-tar-test.XXXXXXXXXX")
cleanup() {
  rm -rf "$work_root"
}
trap cleanup EXIT HUP INT TERM

fake_bin="$work_root/bin"
mkdir -p "$fake_bin"

cat > "$fake_bin/tar" <<'EOF'
#!/usr/bin/env sh
echo "unexpected tar invocation: $*" >> "$CPKT_FAKE_TAR_LOG"
if [ "$1" = "--version" ]; then
  echo "bsdtar 3.7.0"
  exit 0
fi
exit 42
EOF
chmod +x "$fake_bin/tar"

cat > "$fake_bin/gtar" <<EOF
#!/usr/bin/env sh
echo "gtar invocation: \$*" >> "\$CPKT_FAKE_GTAR_LOG"
if [ "\$1" = "--version" ]; then
  echo "gtar (GNU tar) test"
  exit 0
fi
exec "$real_tar" "\$@"
EOF
chmod +x "$fake_bin/gtar"

export CPKT_FAKE_TAR_LOG="$work_root/tar.log"
export CPKT_FAKE_GTAR_LOG="$work_root/gtar.log"
export PATH="$fake_bin:$PATH"

cmake_probe="$work_root/probe.cmake"
cmake_result="$work_root/cmake-gnu-tar.txt"
cat > "$cmake_probe" <<EOF
include("$repo_root/cmake/gnu_tar.cmake")
cpkt_find_gnu_tar(CPKT_TEST_GNU_TAR)
file(WRITE "$cmake_result" "\${CPKT_TEST_GNU_TAR}")
EOF
cmake -P "$cmake_probe" >/dev/null

if [ "$(cat "$cmake_result")" != "$fake_bin/gtar" ]; then
  printf 'CMake GNU tar lookup selected %s, expected %s\n' "$(cat "$cmake_result")" "$fake_bin/gtar" >&2
  exit 1
fi

source_root="$work_root/source"
mkdir -p "$source_root/scripts" "$source_root/tests"
cp "$repo_root/scripts/package-source.sh" "$source_root/scripts/package-source.sh"
cp "$repo_root/scripts/release-version.sh" "$source_root/scripts/release-version.sh"
cp "$repo_root/tests/privacy_scan.cmake" "$source_root/tests/privacy_scan.cmake"
printf '1.2.3\n' > "$source_root/VERSION"
printf 'payload\n' > "$source_root/payload.txt"
printf 'payload.txt\n' > "$source_root/RELEASE_MANIFEST"

bash "$source_root/scripts/package-source.sh" >/dev/null

if [ ! -f "$source_root/dist/c.pkt.systems-1.2.3.tar.gz" ]; then
  printf 'source archive was not created through selected GNU tar\n' >&2
  exit 1
fi

if grep -F 'unexpected tar invocation' "$CPKT_FAKE_TAR_LOG" >/dev/null 2>&1; then
  cat "$CPKT_FAKE_TAR_LOG" >&2
  exit 1
fi

if ! grep -F 'gtar invocation: --sort=name --owner=0 --group=0 --numeric-owner -czf' "$CPKT_FAKE_GTAR_LOG" >/dev/null 2>&1; then
  printf 'gtar was not used for archive creation\n' >&2
  cat "$CPKT_FAKE_GTAR_LOG" >&2
  exit 1
fi

printf '[test] GNU tar lookup passed\n'
