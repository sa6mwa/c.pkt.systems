#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

find_gnu_tar() {
  if [ "${CPKT_GNU_TAR:-}" != "" ]; then
    if "$CPKT_GNU_TAR" --version 2>&1 | grep -F 'GNU tar' >/dev/null 2>&1; then
      printf '%s\n' "$CPKT_GNU_TAR"
      return 0
    fi
    printf 'CPKT_GNU_TAR is not GNU tar: %s\n' "$CPKT_GNU_TAR" >&2
    return 1
  fi

  for candidate_name in gtar tar; do
    candidate_path=$(command -v "$candidate_name" 2>/dev/null || true)
    if [ "$candidate_path" = "" ]; then
      continue
    fi
    if "$candidate_path" --version 2>&1 | grep -F 'GNU tar' >/dev/null 2>&1; then
      printf '%s\n' "$candidate_path"
      return 0
    fi
  done

  printf 'GNU tar is required for source archive verifier failure fixtures\n' >&2
  return 1
}

gnu_tar=$(find_gnu_tar)
work_root=$(mktemp -d "${TMPDIR:-/tmp}/cpkt-source-verify-failure.XXXXXXXXXX")
cleanup() {
  rm -rf "$work_root"
}
trap cleanup EXIT HUP INT TERM

required_payloads='
CMakeLists.txt
Makefile
README.md
docs/opcua-c89-facade-spec.md
include/cpkt/opcua.h
src/opcua.c
tests/opcua_facade_test.c
tests/opcua_c89_boundary_peer.c
tests/opcua_header_facade_test.sh
examples/opcua-c89/main.c
scripts/package-source.sh
scripts/package-verify.sh
scripts/release-version.sh
scripts/source-archive-verify.sh
cmake/CpktDependencies.cmake
vendor/open62541/patches/series
vendor/open62541/patches/0001-prefix-embedded-mqtt-c-symbols.patch
vendor/open62541/patches/0002-avoid-glibc-private-stdio-limit-header-on-musl.patch
vendor/open62541/patches/0003-stub-posix-ethernet-when-packet-headers-are-missing.patch
vendor/open62541/patches/0004-avoid-cert-store-path-strncpy-warning.patch
'

make_archive() {
  fixture_name=$1
  shift
  archive_path="$work_root/$fixture_name/c.pkt.systems-1.2.3.tar.gz"
  mkdir -p "$(dirname -- "$archive_path")"
  (
    cd "$work_root/$fixture_name/stage"
    "$gnu_tar" --sort=name --owner=0 --group=0 --numeric-owner -czf "$archive_path" -- "$@"
  )
  printf '%s\n' "$archive_path"
}

make_non_root_archive() {
  fixture_name=$1
  shift
  archive_path="$work_root/$fixture_name/c.pkt.systems-1.2.3.tar.gz"
  mkdir -p "$(dirname -- "$archive_path")"
  (
    cd "$work_root/$fixture_name/stage"
    "$gnu_tar" --sort=name --owner=1 --group=1 --numeric-owner -czf "$archive_path" -- "$@"
  )
  printf '%s\n' "$archive_path"
}

expect_verify_failure() {
  archive_path=$1
  expected_message=$2
  stderr_path="$work_root/$(basename -- "$archive_path").stderr"

  if bash "$repo_root/scripts/source-archive-verify.sh" "$archive_path" 1.2.3 >"$work_root/verify.stdout" 2>"$stderr_path"; then
    printf 'source archive verifier accepted malformed archive: %s\n' "$archive_path" >&2
    exit 1
  fi
  if ! grep -F -- "$expected_message" "$stderr_path" >/dev/null 2>&1; then
    printf 'source archive verifier failure did not include expected message: %s\n' "$expected_message" >&2
    printf 'actual stderr:\n' >&2
    cat "$stderr_path" >&2
    exit 1
  fi
}

write_release_version_script() {
  root=$1
  mkdir -p "$root/scripts"
  cp "$repo_root/scripts/release-version.sh" "$root/scripts/release-version.sh"
}

fixture_root="$work_root/root-mismatch/stage/c.pkt.systems-wrong"
mkdir -p "$fixture_root"
printf '1.2.3\n' > "$fixture_root/VERSION"
expect_verify_failure "$(make_archive root-mismatch c.pkt.systems-wrong)" "source archive root is c.pkt.systems-wrong"

mkdir -p "$work_root/multiple-roots/stage/c.pkt.systems-1.2.3" "$work_root/multiple-roots/stage/extra-root"
expect_verify_failure "$(make_archive multiple-roots c.pkt.systems-1.2.3 extra-root)" \
  "source archive must contain exactly one root directory"

fixture_root="$work_root/non-root-owner/stage/c.pkt.systems-1.2.3"
mkdir -p "$fixture_root"
printf '1.2.3\n' > "$fixture_root/VERSION"
expect_verify_failure "$(make_non_root_archive non-root-owner c.pkt.systems-1.2.3)" \
  "source archive entries must be owned by 0/0"

fixture_root="$work_root/missing-version/stage/c.pkt.systems-1.2.3"
mkdir -p "$fixture_root"
expect_verify_failure "$(make_archive missing-version c.pkt.systems-1.2.3)" \
  "source archive is missing VERSION"

fixture_root="$work_root/version-mismatch/stage/c.pkt.systems-1.2.3"
mkdir -p "$fixture_root"
printf '9.9.9\n' > "$fixture_root/VERSION"
expect_verify_failure "$(make_archive version-mismatch c.pkt.systems-1.2.3)" \
  "source archive VERSION 9.9.9 does not match expected 1.2.3"

fixture_root="$work_root/missing-manifest/stage/c.pkt.systems-1.2.3"
mkdir -p "$fixture_root"
printf '1.2.3\n' > "$fixture_root/VERSION"
write_release_version_script "$fixture_root"
expect_verify_failure "$(make_archive missing-manifest c.pkt.systems-1.2.3)" \
  "source archive is missing RELEASE_MANIFEST"

fixture_root="$work_root/forbidden-dist/stage/c.pkt.systems-1.2.3"
mkdir -p "$fixture_root/dist"
printf '1.2.3\n' > "$fixture_root/VERSION"
write_release_version_script "$fixture_root"
printf 'VERSION\nRELEASE_MANIFEST\nscripts/release-version.sh\n' > "$fixture_root/RELEASE_MANIFEST"
printf 'stale\n' > "$fixture_root/dist/stale.txt"
expect_verify_failure "$(make_archive forbidden-dist c.pkt.systems-1.2.3)" \
  "source archive includes generated/private path: dist"

fixture_root="$work_root/manifest-mismatch/stage/c.pkt.systems-1.2.3"
mkdir -p "$fixture_root"
printf '1.2.3\n' > "$fixture_root/VERSION"
while IFS= read -r required; do
  if [ "$required" = "" ]; then
    continue
  fi
  mkdir -p "$fixture_root/$(dirname -- "$required")"
  cp "$repo_root/$required" "$fixture_root/$required"
  printf '%s\n' "$required" >> "$fixture_root/RELEASE_MANIFEST"
done <<EOF
$required_payloads
EOF
printf 'VERSION\nRELEASE_MANIFEST\n' >> "$fixture_root/RELEASE_MANIFEST"
printf 'extra\n' > "$fixture_root/extra-unlisted.txt"
expect_verify_failure "$(make_archive manifest-mismatch c.pkt.systems-1.2.3)" \
  "source archive payload does not match RELEASE_MANIFEST"

printf '[test] source archive verifier failure modes passed\n'
