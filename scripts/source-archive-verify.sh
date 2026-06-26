#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
  printf 'usage: %s <archive.tar.gz> [expected-version]\n' "$0" >&2
  exit 2
fi

archive_path=$1
expected_version=${2:-}
case "$archive_path" in
  /*) ;;
  *)
    archive_dir=$(CDPATH= cd -- "$(dirname -- "$archive_path")" && pwd)
    archive_path="$archive_dir/$(basename -- "$archive_path")"
    ;;
esac

case "$archive_path" in
  *.tar.gz) ;;
  *)
    printf 'source archive must be a .tar.gz file: %s\n' "$archive_path" >&2
    exit 1
    ;;
esac
if [ ! -f "$archive_path" ]; then
  printf 'source archive does not exist: %s\n' "$archive_path" >&2
  exit 1
fi

archive_name=$(basename -- "$archive_path")
archive_stem=${archive_name%.tar.gz}
case "$archive_stem" in
  c.pkt.systems-*) ;;
  *)
    printf 'unexpected source archive name: %s\n' "$archive_name" >&2
    exit 1
    ;;
esac
archive_version=${archive_stem#c.pkt.systems-}
if [ -z "$expected_version" ]; then
  expected_version=$archive_version
fi
if [ "$archive_version" != "$expected_version" ]; then
  printf 'source archive version %s does not match expected %s\n' "$archive_version" "$expected_version" >&2
  exit 1
fi

if tar --numeric-owner -tvf "$archive_path" | awk '$2 != "0/0" { print; bad = 1 } END { exit bad }'; then
  :
else
  printf 'source archive entries must be owned by 0/0\n' >&2
  exit 1
fi

work_dir=$(mktemp -d "${TMPDIR:-/tmp}/cpkt-source-verify.XXXXXXXXXX")
cleanup() {
  rm -rf "$work_dir"
}
trap cleanup EXIT HUP INT TERM

(
  cd "$work_dir"
  cmake -E tar xf "$archive_path" >/dev/null
)

root_count=$(find "$work_dir" -mindepth 1 -maxdepth 1 -type d | wc -l | tr -d ' ')
if [ "$root_count" != "1" ]; then
  printf 'source archive must contain exactly one root directory\n' >&2
  exit 1
fi
source_root="$work_dir/$archive_stem"
if [ ! -d "$source_root" ]; then
  actual_root=
  for candidate in "$work_dir"/*; do
    if [ -d "$candidate" ]; then
      actual_root=$(basename -- "$candidate")
      break
    fi
  done
  printf 'source archive root is %s, expected %s\n' "$actual_root" "$archive_stem" >&2
  exit 1
fi

if [ ! -f "$source_root/VERSION" ]; then
  printf 'source archive is missing VERSION\n' >&2
  exit 1
fi
source_version=$(sed -n '1{s/[[:space:]]*$//;p;q;}' "$source_root/VERSION")
if [ "$source_version" != "$expected_version" ]; then
  printf 'source archive VERSION %s does not match expected %s\n' "$source_version" "$expected_version" >&2
  exit 1
fi

resolved_version=$(bash "$source_root/scripts/release-version.sh" "$source_root")
if [ "$resolved_version" != "$expected_version" ]; then
  printf 'non-git version resolution returned %s, expected %s\n' "$resolved_version" "$expected_version" >&2
  exit 1
fi

if [ ! -f "$source_root/RELEASE_MANIFEST" ]; then
  printf 'source archive is missing RELEASE_MANIFEST\n' >&2
  exit 1
fi

for forbidden in .git .cache build dist; do
  if [ -e "$source_root/$forbidden" ]; then
    printf 'source archive includes generated/private path: %s\n' "$forbidden" >&2
    exit 1
  fi
done

for required in \
  CMakeLists.txt \
  Makefile \
  README.md \
  docs/opcua-c89-facade-spec.md \
  include/cpkt/opcua.h \
  src/opcua.c \
  tests/opcua_facade_test.c \
  tests/opcua_c89_boundary_peer.c \
  tests/opcua_header_facade_test.sh \
  examples/opcua-c89/main.c \
  scripts/package-source.sh \
  scripts/package-verify.sh \
  scripts/release-version.sh \
  scripts/source-archive-verify.sh \
  cmake/CpktDependencies.cmake \
  vendor/open62541/patches/series \
  vendor/open62541/patches/0001-prefix-embedded-mqtt-c-symbols.patch \
  vendor/open62541/patches/0002-avoid-glibc-private-stdio-limit-header-on-musl.patch \
  vendor/open62541/patches/0003-stub-posix-ethernet-when-packet-headers-are-missing.patch \
  vendor/open62541/patches/0004-avoid-cert-store-path-strncpy-warning.patch
do
  if [ ! -f "$source_root/$required" ]; then
    printf 'source archive is missing required payload: %s\n' "$required" >&2
    exit 1
  fi
done

(
  cd "$source_root"
  find . -type f | sed 's#^\./##' | sort > "$work_dir/actual-files.txt"
)
sort "$source_root/RELEASE_MANIFEST" > "$work_dir/manifest-files.txt"
if ! diff -u "$work_dir/manifest-files.txt" "$work_dir/actual-files.txt"; then
  printf 'source archive payload does not match RELEASE_MANIFEST\n' >&2
  exit 1
fi

cmake \
  -DCPKT_ROOT="$repo_root" \
  -DCPKT_SCAN_LABEL="source archive" \
  -DCPKT_SCAN_PATHS="$archive_path" \
  -P "$repo_root/tests/privacy_scan.cmake"

build_dir="$work_dir/build"
cmake -S "$source_root" -B "$build_dir" \
  -DCPKT_FACADE_ONLY=ON \
  -DCPKT_BUILD_TESTS=ON >/dev/null
bash "$source_root/scripts/run-no-warnings.sh" \
  "source archive facade-only build" \
  cmake --build "$build_dir" >/dev/null
ctest --test-dir "$build_dir" --output-on-failure

printf '[package] verified source archive %s\n' "$archive_path"
