#!/usr/bin/env bash
set -eu

repo_root=${1:?repo root is required}

target_id=cachetest-linux-gnu
external_root="$repo_root/.cache/deps/$target_id"
build_root="$repo_root/.cache/deps-build/$target_id"
external_contract_file="$repo_root/.cache/dependency-contracts/$target_id-external.txt"
build_contract_file="$repo_root/.cache/dependency-contracts/$target_id-build.txt"
configure_dir="$repo_root/build/repo-dependency-stale-root-test"
root_association_configure_dir="$repo_root/build/repo-dependency-stale-root-test-root-association"
legacy_aggregate_configure_dir="$repo_root/build/repo-dependency-stale-root-test-legacy-aggregate"
cached_default_override_configure_dir="$repo_root/build/repo-dependency-stale-root-test-cached-default-override"
partial_external_configure_dir="$repo_root/build/repo-dependency-stale-root-test-partial-external"
post_partial_external_configure_dir="$repo_root/build/repo-dependency-stale-root-test-post-partial-external"
partial_build_configure_dir="$repo_root/build/repo-dependency-stale-root-test-partial-build"
lifecycle_override_configure_dir="$repo_root/build/repo-dependency-stale-root-test-lifecycle-override"
disabled_refresh_configure_dir="$repo_root/build/repo-dependency-stale-root-test-disabled-refresh"
disabled_configure_dir="$repo_root/build/repo-dependency-stale-root-test-disabled"
explicit_reuse_configure_dir="$repo_root/build/repo-dependency-stale-root-test-explicit-reuse"
non_disposable_configure_dir="$repo_root/build/repo-dependency-stale-root-test-non-disposable"
cached_default_caller_owned_configure_dir="$repo_root/build/repo-dependency-stale-root-test-cached-default-caller-owned"
symlink_configure_dir="$repo_root/build/repo-dependency-stale-root-test-symlink"
ancestor_symlink_fixture_dir="$repo_root/build/repo-dependency-stale-root-test-ancestor-symlink-src"
ancestor_symlink_configure_dir="$repo_root/build/repo-dependency-stale-root-test-ancestor-symlink"
ancestor_symlink_target="$repo_root/build/repo-dependency-stale-root-test-ancestor-symlink-target"
shared_cache="$repo_root/.cache/test-shared-dependency-cache"
override_external_root="$repo_root/.cache/deps/repo-dependency-stale-root-override-external"
root_association_override_external_root="$repo_root/.cache/deps/repo-dependency-stale-root-association-override-external"
override_build_root="$repo_root/.cache/deps-build/repo-dependency-stale-root-override-build"
lifecycle_override_external_root="$repo_root/.cache/deps/repo-dependency-stale-root-lifecycle-external"
lifecycle_override_build_root="$repo_root/.cache/deps-build/repo-dependency-stale-root-lifecycle-build"
non_disposable_external_root="$repo_root/build/repo-dependency-stale-root-non-disposable-external"
caller_owned_external_root="$repo_root/.cache/deps/repo-dependency-stale-root-caller-owned-external"
explicit_reuse_external_root="$repo_root/.cache/deps/repo-dependency-stale-root-explicit-reuse-external"
explicit_reuse_build_root="$repo_root/.cache/deps-build/repo-dependency-stale-root-explicit-reuse-build"
symlink_parent="$repo_root/.cache/deps/repo-dependency-stale-root-symlink-parent"
symlink_target="$repo_root/build/repo-dependency-stale-root-symlink-target"

cleanup() {
  rm -rf "$external_root" "$build_root" "$external_contract_file" "$build_contract_file" "$configure_dir" \
    "$root_association_configure_dir" "$legacy_aggregate_configure_dir" "$cached_default_override_configure_dir" "$partial_external_configure_dir" "$post_partial_external_configure_dir" "$partial_build_configure_dir" "$lifecycle_override_configure_dir" "$disabled_refresh_configure_dir" "$disabled_configure_dir" "$explicit_reuse_configure_dir" "$non_disposable_configure_dir" "$cached_default_caller_owned_configure_dir" "$symlink_configure_dir" "$ancestor_symlink_fixture_dir" "$ancestor_symlink_configure_dir" \
    "$shared_cache" "$override_external_root" "$root_association_override_external_root" "$override_build_root" "$lifecycle_override_external_root" "$lifecycle_override_build_root" "$non_disposable_external_root" "$caller_owned_external_root" "$explicit_reuse_external_root" "$explicit_reuse_build_root" "$symlink_parent" "$symlink_target" "$ancestor_symlink_target"
}
trap cleanup EXIT INT TERM
cleanup

mkdir -p "$external_root" "$build_root" "$shared_cache"
: > "$external_root/stale-install-sentinel"
: > "$build_root/stale-build-sentinel"

set +e
first_output=$(cmake -S "$repo_root" -B "$configure_dir" \
  -DCPKT_TARGET_ARCH=cachetest \
  -DCPKT_TARGET_OS=Linux \
  -DCPKT_TARGET_LIBC=gnu \
  -DCPKT_BUILD_DEPENDENCIES=ON \
  -DCMAKE_C_FLAGS=-DCPKT_STALE_ROOT_FIRST \
  -DCPKT_DEPENDENCY_CACHE="$shared_cache" 2>&1)
first_status=$?
set -e

case "$first_output" in
  *"Refreshed repo-local external dependency root for $target_id after dependency build contract change"*\
*"Refreshed repo-local build dependency root for $target_id after dependency build contract change"*) ;;
  *)
    printf 'configure did not report stale repo-local dependency root refresh\n%s\n' "$first_output" >&2
    exit 1
    ;;
esac
if [ -e "$external_root/stale-install-sentinel" ] ||
    [ -e "$build_root/stale-build-sentinel" ]; then
  printf 'stale repo-local dependency roots were not deleted\n' >&2
  exit 1
fi
if [ ! -f "$external_contract_file" ] || [ ! -f "$build_contract_file" ]; then
  printf 'per-root dependency build contract manifests were not written\n' >&2
  exit 1
fi

mkdir -p "$symlink_target/child"
: > "$symlink_target/child/must-not-delete"
ln -s "$symlink_target" "$symlink_parent"

set +e
symlink_output=$(cmake -S "$repo_root" -B "$symlink_configure_dir" \
  -DCPKT_TARGET_ARCH=symlinkescape \
  -DCPKT_TARGET_OS=Linux \
  -DCPKT_TARGET_LIBC=gnu \
  -DCPKT_BUILD_DEPENDENCIES=ON \
  -DCPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE=ON \
  -DCPKT_EXTERNAL_ROOT="$symlink_parent/child" \
  -DCPKT_DEPENDENCY_CACHE="$shared_cache" 2>&1)
symlink_status=$?
set -e

if [ "$symlink_status" -eq 0 ]; then
  printf 'configure unexpectedly accepted a lifecycle-owned dependency root with symlink components\n' >&2
  exit 1
fi
case "$symlink_output" in
  *"must not contain symlink"*"components:"*"$symlink_parent"*) ;;
  *)
    printf 'symlinked dependency root failure did not identify the symlink component\n%s\n' "$symlink_output" >&2
    exit 1
    ;;
esac
if [ ! -e "$symlink_target/child/must-not-delete" ]; then
  printf 'symlinked dependency root target was deleted before configure failed\n' >&2
  exit 1
fi

mkdir -p "$ancestor_symlink_fixture_dir" "$ancestor_symlink_target/deps/ancestor-linux-gnu"
ln -s "$ancestor_symlink_target" "$ancestor_symlink_fixture_dir/.cache"
: > "$ancestor_symlink_target/deps/ancestor-linux-gnu/must-not-delete"
cat > "$ancestor_symlink_fixture_dir/CMakeLists.txt" <<EOF
cmake_minimum_required(VERSION 3.21)
project(repo_dependency_ancestor_symlink NONE)
include("$repo_root/cmake/CpktDependencyContract.cmake")
set(CPKT_TARGET_ID ancestor-linux-gnu)
set(CPKT_TARGET_ARCH ancestor)
set(CPKT_TARGET_OS linux)
set(CPKT_TARGET_LIBC gnu)
set(CPKT_EXTERNAL_ROOT "\${CMAKE_SOURCE_DIR}/.cache/deps/\${CPKT_TARGET_ID}")
set(CPKT_DEPENDENCY_BUILD_ROOT "\${CMAKE_SOURCE_DIR}/.cache/deps-build/\${CPKT_TARGET_ID}")
set(CPKT_BUILD_DEPENDENCIES ON)
cpkt_refresh_repo_dependency_roots_if_stale(ON OFF)
EOF

set +e
ancestor_symlink_output=$(cmake -S "$ancestor_symlink_fixture_dir" -B "$ancestor_symlink_configure_dir" 2>&1)
ancestor_symlink_status=$?
set -e

if [ "$ancestor_symlink_status" -eq 0 ]; then
  printf 'configure unexpectedly accepted a lifecycle-owned dependency root below a symlinked .cache ancestor\n' >&2
  exit 1
fi
case "$ancestor_symlink_output" in
  *"ancestor must not be a symlink:"*"$ancestor_symlink_fixture_dir/.cache"*) ;;
  *)
    printf 'symlinked dependency root ancestor failure did not identify .cache\n%s\n' "$ancestor_symlink_output" >&2
    exit 1
    ;;
esac
if [ ! -e "$ancestor_symlink_target/deps/ancestor-linux-gnu/must-not-delete" ]; then
  printf 'symlinked dependency root ancestor target was deleted before configure failed\n' >&2
  exit 1
fi

mkdir -p "$external_root" "$build_root"
: > "$external_root/current-install-sentinel"
: > "$build_root/current-build-sentinel"

set +e
second_output=$(cmake -S "$repo_root" -B "$configure_dir" \
  -DCPKT_TARGET_ARCH=cachetest \
  -DCPKT_TARGET_OS=Linux \
  -DCPKT_TARGET_LIBC=gnu \
  -DCPKT_BUILD_DEPENDENCIES=ON \
  -DCMAKE_C_FLAGS=-DCPKT_STALE_ROOT_FIRST \
  -DCPKT_DEPENDENCY_CACHE="$shared_cache" 2>&1)
second_status=$?
set -e

if [ "$first_status" -ne "$second_status" ]; then
  printf 'stale-root fixture changed configure exit status across identical contracts\nfirst=%s second=%s\n' "$first_status" "$second_status" >&2
  exit 1
fi
case "$second_output" in
  *"Refreshed repo-local "*" dependency root for $target_id after dependency build contract change"*)
    printf 'repo-local dependency roots were refreshed despite an unchanged build contract\n%s\n' "$second_output" >&2
    exit 1
    ;;
esac
if [ ! -e "$external_root/current-install-sentinel" ] ||
    [ ! -e "$build_root/current-build-sentinel" ]; then
  printf 'current repo-local dependency roots were deleted despite an unchanged build contract\n' >&2
  exit 1
fi

set +e
legacy_aggregate_output=$(cmake -S "$repo_root" -B "$legacy_aggregate_configure_dir" \
  -DCPKT_TARGET_ARCH=cachetest \
  -DCPKT_TARGET_OS=Linux \
  -DCPKT_TARGET_LIBC=gnu \
  -DCPKT_BUILD_DEPENDENCIES=ON \
  -DCPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE=ON \
  -DCPKT_EXTERNAL_ROOT="$repo_root/.cache/deps" \
  -DCPKT_DEPENDENCY_BUILD_ROOT="$repo_root/.cache/deps-build" \
  -DCMAKE_C_FLAGS=-DCPKT_STALE_ROOT_LEGACY_AGGREGATE \
  -DCPKT_DEPENDENCY_CACHE="$shared_cache" 2>&1)
legacy_aggregate_status=$?
set -e

if [ "$first_status" -ne "$legacy_aggregate_status" ]; then
  printf 'stale-root fixture changed configure exit status with legacy aggregate roots\nfirst=%s legacy=%s\n' "$first_status" "$legacy_aggregate_status" >&2
  exit 1
fi
case "$legacy_aggregate_output" in
  *"Refreshed repo-local external dependency root for $target_id after dependency build contract change"*\
*"Refreshed repo-local build dependency root for $target_id after dependency build contract change"*) ;;
  *)
    printf 'legacy aggregate dependency-root paths were normalized but not lifecycle-refreshed\n%s\n' "$legacy_aggregate_output" >&2
    exit 1
    ;;
esac
if [ -e "$external_root/current-install-sentinel" ] ||
    [ -e "$build_root/current-build-sentinel" ]; then
  printf 'legacy aggregate dependency-root paths were misclassified as caller-owned roots\n' >&2
  exit 1
fi

mkdir -p "$external_root" "$build_root"
: > "$external_root/cached-default-first-install-sentinel"
: > "$build_root/cached-default-first-build-sentinel"

set +e
cached_default_first_output=$(cmake -S "$repo_root" -B "$cached_default_override_configure_dir" \
  -DCPKT_TARGET_ARCH=cachetest \
  -DCPKT_TARGET_OS=Linux \
  -DCPKT_TARGET_LIBC=gnu \
  -DCPKT_BUILD_DEPENDENCIES=ON \
  -DCPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE=ON \
  -DCMAKE_C_FLAGS=-DCPKT_STALE_ROOT_CACHED_DEFAULT_FIRST \
  -DCPKT_DEPENDENCY_CACHE="$shared_cache" 2>&1)
cached_default_first_status=$?
set -e

if [ "$first_status" -ne "$cached_default_first_status" ]; then
  printf 'stale-root fixture changed configure exit status with cached default roots\nfirst=%s cached_first=%s\n' "$first_status" "$cached_default_first_status" >&2
  exit 1
fi
case "$cached_default_first_output" in
  *"Refreshed repo-local external dependency root for $target_id after dependency build contract change"*\
*"Refreshed repo-local build dependency root for $target_id after dependency build contract change"*) ;;
  *)
    printf 'cached default dependency roots were not lifecycle-refreshed before cache reconfigure\n%s\n' "$cached_default_first_output" >&2
    exit 1
    ;;
esac

mkdir -p "$external_root" "$build_root"
: > "$external_root/cached-default-second-install-sentinel"
: > "$build_root/cached-default-second-build-sentinel"

set +e
cached_default_second_output=$(cmake -S "$repo_root" -B "$cached_default_override_configure_dir" \
  -DCPKT_TARGET_ARCH=cachetest \
  -DCPKT_TARGET_OS=Linux \
  -DCPKT_TARGET_LIBC=gnu \
  -DCPKT_BUILD_DEPENDENCIES=ON \
  -DCPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE=ON \
  -DCMAKE_C_FLAGS=-DCPKT_STALE_ROOT_CACHED_DEFAULT_SECOND \
  -DCPKT_DEPENDENCY_CACHE="$shared_cache" 2>&1)
cached_default_second_status=$?
set -e

if [ "$first_status" -ne "$cached_default_second_status" ]; then
  printf 'stale-root fixture changed configure exit status after cached default root reconfigure\nfirst=%s cached_second=%s\n' "$first_status" "$cached_default_second_status" >&2
  exit 1
fi
case "$cached_default_second_output" in
  *"Refreshed repo-local external dependency root for $target_id after dependency build contract change"*\
*"Refreshed repo-local build dependency root for $target_id after dependency build contract change"*) ;;
  *)
    printf 'cached default dependency roots were treated as caller-owned on reconfigure\n%s\n' "$cached_default_second_output" >&2
    exit 1
    ;;
esac
if [ -e "$external_root/cached-default-second-install-sentinel" ] ||
    [ -e "$build_root/cached-default-second-build-sentinel" ]; then
  printf 'cached default dependency roots bypassed lifecycle invalidation on reconfigure\n' >&2
  exit 1
fi

mkdir -p "$caller_owned_external_root" "$build_root"
: > "$caller_owned_external_root/caller-owned-external-sentinel"
: > "$build_root/cached-default-caller-owned-build-sentinel"

set +e
cached_default_caller_owned_first_output=$(cmake -S "$repo_root" -B "$cached_default_caller_owned_configure_dir" \
  -DCPKT_TARGET_ARCH=cachetest \
  -DCPKT_TARGET_OS=Linux \
  -DCPKT_TARGET_LIBC=gnu \
  -DCPKT_BUILD_DEPENDENCIES=ON \
  -DCPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE=ON \
  -DCPKT_CALLER_OWNED_DEPENDENCY_ROOTS=ON \
  -DCPKT_EXTERNAL_ROOT="$caller_owned_external_root" \
  -DCMAKE_C_FLAGS=-DCPKT_STALE_ROOT_CALLER_OWNED_EXTERNAL_FIRST \
  -DCPKT_DEPENDENCY_CACHE="$shared_cache" 2>&1)
cached_default_caller_owned_first_status=$?
set -e

if [ "$first_status" -ne "$cached_default_caller_owned_first_status" ]; then
  printf 'stale-root fixture changed configure exit status with caller-owned external root and default build root\nfirst=%s caller_owned_first=%s\n' "$first_status" "$cached_default_caller_owned_first_status" >&2
  exit 1
fi
case "$cached_default_caller_owned_first_output" in
  *"Refreshed repo-local build dependency root for $target_id after dependency build contract change"*) ;;
  *)
    printf 'default build root was not lifecycle-refreshed when paired with caller-owned external root\n%s\n' "$cached_default_caller_owned_first_output" >&2
    exit 1
    ;;
esac
if [ ! -e "$caller_owned_external_root/caller-owned-external-sentinel" ]; then
  printf 'caller-owned external root was deleted by lifecycle invalidation\n' >&2
  exit 1
fi

mkdir -p "$build_root"
: > "$build_root/cached-default-caller-owned-reconfigure-build-sentinel"

set +e
cached_default_caller_owned_second_output=$(cmake -S "$repo_root" -B "$cached_default_caller_owned_configure_dir" \
  -DCPKT_TARGET_ARCH=cachetest \
  -DCPKT_TARGET_OS=Linux \
  -DCPKT_TARGET_LIBC=gnu \
  -DCPKT_BUILD_DEPENDENCIES=ON \
  -DCPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE=ON \
  -DCPKT_CALLER_OWNED_DEPENDENCY_ROOTS=ON \
  -DCPKT_EXTERNAL_ROOT="$caller_owned_external_root" \
  -DCMAKE_C_FLAGS=-DCPKT_STALE_ROOT_CALLER_OWNED_EXTERNAL_SECOND \
  -DCPKT_DEPENDENCY_CACHE="$shared_cache" 2>&1)
cached_default_caller_owned_second_status=$?
set -e

if [ "$first_status" -ne "$cached_default_caller_owned_second_status" ]; then
  printf 'stale-root fixture changed configure exit status after caller-owned external-root reconfigure\nfirst=%s caller_owned_second=%s\n' "$first_status" "$cached_default_caller_owned_second_status" >&2
  exit 1
fi
case "$cached_default_caller_owned_second_output" in
  *"Refreshed repo-local build dependency root for $target_id after dependency build contract change"*) ;;
  *)
    printf 'cached default build root became caller-owned on reconfigure\n%s\n' "$cached_default_caller_owned_second_output" >&2
    exit 1
    ;;
esac
if [ -e "$build_root/cached-default-caller-owned-reconfigure-build-sentinel" ]; then
  printf 'cached default build root bypassed lifecycle invalidation after caller-owned external-root reconfigure\n' >&2
  exit 1
fi

mkdir -p "$build_root" "$root_association_override_external_root"
: > "$build_root/root-association-build-sentinel"
: > "$root_association_override_external_root/root-association-external-sentinel"

set +e
root_association_output=$(cmake -S "$repo_root" -B "$root_association_configure_dir" \
  -DCPKT_TARGET_ARCH=cachetest \
  -DCPKT_TARGET_OS=Linux \
  -DCPKT_TARGET_LIBC=gnu \
  -DCPKT_BUILD_DEPENDENCIES=ON \
  -DCPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE=ON \
  -DCPKT_EXTERNAL_ROOT="$root_association_override_external_root" \
  -DCMAKE_C_FLAGS=-DCPKT_STALE_ROOT_FIRST \
  -DCPKT_DEPENDENCY_CACHE="$shared_cache" 2>&1)
root_association_status=$?
set -e

if [ "$first_status" -ne "$root_association_status" ]; then
  printf 'stale-root fixture changed configure exit status after changing only the external-root association\nfirst=%s association=%s\n' "$first_status" "$root_association_status" >&2
  exit 1
fi
case "$root_association_output" in
  *"Refreshed repo-local external dependency root for $target_id after dependency build contract change"*\
*"Refreshed repo-local build dependency root for $target_id after dependency build contract change"*) ;;
  *)
    printf 'lifecycle-owned roots were not both refreshed after only the external-root association changed\n%s\n' "$root_association_output" >&2
    exit 1
    ;;
esac
if [ -e "$build_root/root-association-build-sentinel" ]; then
  printf 'default build root survived after its external-root association changed\n' >&2
  exit 1
fi
if [ -e "$root_association_override_external_root/root-association-external-sentinel" ]; then
  printf 'lifecycle-owned external override survived after its root association changed\n' >&2
  exit 1
fi

set +e
third_output=$(cmake -S "$repo_root" -B "$configure_dir" \
  -DCPKT_TARGET_ARCH=cachetest \
  -DCPKT_TARGET_OS=Linux \
  -DCPKT_TARGET_LIBC=gnu \
  -DCPKT_BUILD_DEPENDENCIES=ON \
  -DCMAKE_C_FLAGS=-DCPKT_STALE_ROOT_SECOND \
  -DCPKT_DEPENDENCY_CACHE="$shared_cache" 2>&1)
third_status=$?
set -e

if [ "$first_status" -ne "$third_status" ]; then
  printf 'stale-root fixture changed configure exit status after a contract-relevant flag change\nfirst=%s third=%s\n' "$first_status" "$third_status" >&2
  exit 1
fi
case "$third_output" in
  *"Refreshed repo-local external dependency root for $target_id after dependency build contract change"*\
*"Refreshed repo-local build dependency root for $target_id after dependency build contract change"*) ;;
  *)
    printf 'repo-local dependency roots were not refreshed after a contract-relevant flag change\n%s\n' "$third_output" >&2
    exit 1
    ;;
esac
if [ -e "$external_root/current-install-sentinel" ] ||
    [ -e "$build_root/current-build-sentinel" ]; then
  printf 'contract-relevant flag changes did not delete stale repo-local dependency roots\n' >&2
  exit 1
fi

mkdir -p "$build_root" "$override_external_root"
: > "$build_root/partial-override-build-sentinel"
: > "$override_external_root/partial-override-external-sentinel"

set +e
partial_external_output=$(cmake -S "$repo_root" -B "$partial_external_configure_dir" \
  -DCPKT_TARGET_ARCH=cachetest \
  -DCPKT_TARGET_OS=Linux \
  -DCPKT_TARGET_LIBC=gnu \
  -DCPKT_BUILD_DEPENDENCIES=ON \
  -DCPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE=ON \
  -DCPKT_EXTERNAL_ROOT="$override_external_root" \
  -DCMAKE_C_FLAGS=-DCPKT_STALE_ROOT_PARTIAL_EXTERNAL \
  -DCPKT_DEPENDENCY_CACHE="$shared_cache" 2>&1)
partial_external_status=$?
set -e

if [ "$first_status" -ne "$partial_external_status" ]; then
  printf 'stale-root fixture changed configure exit status after an external-root partial override\nfirst=%s partial=%s\n' "$first_status" "$partial_external_status" >&2
  exit 1
fi
case "$partial_external_output" in
  *"Refreshed repo-local external dependency root for $target_id after dependency build contract change"*\
*"Refreshed repo-local build dependency root for $target_id after dependency build contract change"*) ;;
  *)
    printf 'lifecycle-owned roots were not both refreshed when only external root was overridden\n%s\n' "$partial_external_output" >&2
    exit 1
    ;;
esac
if [ -e "$build_root/partial-override-build-sentinel" ]; then
  printf 'default build root survived a stale contract while external root was overridden\n' >&2
  exit 1
fi
if [ -e "$override_external_root/partial-override-external-sentinel" ]; then
  printf 'lifecycle-owned external override survived stale contract invalidation\n' >&2
  exit 1
fi

mkdir -p "$external_root" "$build_root"
: > "$external_root/post-partial-stale-install-sentinel"
: > "$build_root/post-partial-current-build-sentinel"

set +e
post_partial_external_output=$(cmake -S "$repo_root" -B "$post_partial_external_configure_dir" \
  -DCPKT_TARGET_ARCH=cachetest \
  -DCPKT_TARGET_OS=Linux \
  -DCPKT_TARGET_LIBC=gnu \
  -DCPKT_BUILD_DEPENDENCIES=ON \
  -DCMAKE_C_FLAGS=-DCPKT_STALE_ROOT_PARTIAL_EXTERNAL \
  -DCPKT_DEPENDENCY_CACHE="$shared_cache" 2>&1)
post_partial_external_status=$?
set -e

if [ "$first_status" -ne "$post_partial_external_status" ]; then
  printf 'stale-root fixture changed configure exit status after returning to both default roots\nfirst=%s post_partial=%s\n' "$first_status" "$post_partial_external_status" >&2
  exit 1
fi
case "$post_partial_external_output" in
  *"Refreshed repo-local external dependency root for $target_id after dependency build contract change"*) ;;
  *)
    printf 'default external root was incorrectly certified by a previous partial override\n%s\n' "$post_partial_external_output" >&2
    exit 1
    ;;
esac
case "$post_partial_external_output" in
  *"Refreshed repo-local build dependency root for $target_id after dependency build contract change"*) ;;
  *)
    printf 'default build root did not refresh after returning from an overridden external-root association\n%s\n' "$post_partial_external_output" >&2
    exit 1
    ;;
esac
if [ -e "$external_root/post-partial-stale-install-sentinel" ]; then
  printf 'default external root survived stale contract certification after a partial override\n' >&2
  exit 1
fi
if [ -e "$build_root/post-partial-current-build-sentinel" ]; then
  printf 'default build root survived after returning from an overridden external-root association\n' >&2
  exit 1
fi

mkdir -p "$external_root" "$override_build_root"
: > "$external_root/partial-override-install-sentinel"
: > "$override_build_root/partial-override-build-sentinel"

set +e
partial_build_output=$(cmake -S "$repo_root" -B "$partial_build_configure_dir" \
  -DCPKT_TARGET_ARCH=cachetest \
  -DCPKT_TARGET_OS=Linux \
  -DCPKT_TARGET_LIBC=gnu \
  -DCPKT_BUILD_DEPENDENCIES=ON \
  -DCPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE=ON \
  -DCPKT_CALLER_OWNED_DEPENDENCY_ROOTS=ON \
  -DCPKT_DEPENDENCY_BUILD_ROOT="$override_build_root" \
  -DCMAKE_C_FLAGS=-DCPKT_STALE_ROOT_PARTIAL_BUILD \
  -DCPKT_DEPENDENCY_CACHE="$shared_cache" 2>&1)
partial_build_status=$?
set -e

if [ "$partial_build_status" -eq 0 ]; then
  printf 'stale default external root paired with caller-owned build root was accepted\n' >&2
  exit 1
fi
case "$partial_build_output" in
  *"repo-local external dependency root for $target_id is stale"*\
*"CMake will not delete caller-owned dependency build state."*) ;;
  *)
    printf 'stale default external root paired with caller-owned build root did not fail safely\n%s\n' "$partial_build_output" >&2
    exit 1
    ;;
esac
if [ ! -e "$external_root/partial-override-install-sentinel" ]; then
  printf 'default external root was deleted before rejecting caller-owned build-root pairing\n' >&2
  exit 1
fi
if [ ! -e "$override_build_root/partial-override-build-sentinel" ]; then
  printf 'caller-owned build root was deleted by repo-local dependency invalidation\n' >&2
  exit 1
fi

mkdir -p "$lifecycle_override_external_root" "$lifecycle_override_build_root"
: > "$lifecycle_override_external_root/lifecycle-override-external-sentinel"
: > "$lifecycle_override_build_root/lifecycle-override-build-sentinel"

set +e
lifecycle_override_output=$(cmake -S "$repo_root" -B "$lifecycle_override_configure_dir" \
  -DCPKT_TARGET_ARCH=cachetest \
  -DCPKT_TARGET_OS=Linux \
  -DCPKT_TARGET_LIBC=gnu \
  -DCPKT_BUILD_DEPENDENCIES=ON \
  -DCPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE=ON \
  -DCPKT_EXTERNAL_ROOT="$lifecycle_override_external_root" \
  -DCPKT_DEPENDENCY_BUILD_ROOT="$lifecycle_override_build_root" \
  -DCMAKE_C_FLAGS=-DCPKT_STALE_ROOT_LIFECYCLE_OVERRIDE \
  -DCPKT_DEPENDENCY_CACHE="$shared_cache" 2>&1)
lifecycle_override_status=$?
set -e

if [ "$first_status" -ne "$lifecycle_override_status" ]; then
  printf 'stale-root fixture changed configure exit status with lifecycle-owned override roots\nfirst=%s lifecycle_override=%s\n' "$first_status" "$lifecycle_override_status" >&2
  exit 1
fi
case "$lifecycle_override_output" in
  *"Refreshed repo-local external dependency root for $target_id after dependency build contract change"*\
*"Refreshed repo-local build dependency root for $target_id after dependency build contract change"*) ;;
  *)
    printf 'lifecycle-owned dependency-root overrides were not both refreshed\n%s\n' "$lifecycle_override_output" >&2
    exit 1
    ;;
esac
if [ -e "$lifecycle_override_external_root/lifecycle-override-external-sentinel" ] ||
    [ -e "$lifecycle_override_build_root/lifecycle-override-build-sentinel" ]; then
  printf 'lifecycle-owned dependency-root overrides bypassed stale-contract invalidation\n' >&2
  exit 1
fi

mkdir -p "$non_disposable_external_root"
: > "$non_disposable_external_root/non-disposable-sentinel"

set +e
non_disposable_output=$(cmake -S "$repo_root" -B "$non_disposable_configure_dir" \
  -DCPKT_TARGET_ARCH=cachetest \
  -DCPKT_TARGET_OS=Linux \
  -DCPKT_TARGET_LIBC=gnu \
  -DCPKT_BUILD_DEPENDENCIES=ON \
  -DCPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE=ON \
  -DCPKT_EXTERNAL_ROOT="$non_disposable_external_root" \
  -DCMAKE_C_FLAGS=-DCPKT_STALE_ROOT_NON_DISPOSABLE \
  -DCPKT_DEPENDENCY_CACHE="$shared_cache" 2>&1)
non_disposable_status=$?
set -e

if [ "$non_disposable_status" -eq 0 ]; then
  printf 'non-disposable lifecycle-owned external root was accepted\n' >&2
  exit 1
fi
case "$non_disposable_output" in
  *"lifecycle-owned external dependency root must be under"*"$repo_root/.cache/deps"*) ;;
  *)
    printf 'non-disposable lifecycle-owned external root did not fail safely\n%s\n' "$non_disposable_output" >&2
    exit 1
    ;;
esac
if [ ! -e "$non_disposable_external_root/non-disposable-sentinel" ]; then
  printf 'non-disposable lifecycle-owned external root was deleted before rejection\n' >&2
  exit 1
fi

mkdir -p "$external_root" "$build_root"

set +e
disabled_refresh_output=$(cmake -S "$repo_root" -B "$disabled_refresh_configure_dir" \
  -DCPKT_TARGET_ARCH=cachetest \
  -DCPKT_TARGET_OS=Linux \
  -DCPKT_TARGET_LIBC=gnu \
  -DCPKT_BUILD_DEPENDENCIES=ON \
  -DCMAKE_C_FLAGS=-DCPKT_STALE_ROOT_DISABLED_CURRENT \
  -DCPKT_DEPENDENCY_CACHE="$shared_cache" 2>&1)
disabled_refresh_status=$?
set -e

if [ "$first_status" -ne "$disabled_refresh_status" ]; then
  printf 'stale-root fixture changed configure exit status while preparing current disabled-build contracts\nfirst=%s refresh=%s\n' "$first_status" "$disabled_refresh_status" >&2
  exit 1
fi
case "$disabled_refresh_output" in
  *"Refreshed repo-local external dependency root for $target_id after dependency build contract change"*\
*"Refreshed repo-local build dependency root for $target_id after dependency build contract change"*) ;;
  *)
    printf 'repo-local dependency roots were not refreshed while preparing disabled-build contracts\n%s\n' "$disabled_refresh_output" >&2
    exit 1
    ;;
esac

mkdir -p "$external_root" "$build_root"
: > "$external_root/disabled-current-install-sentinel"
: > "$build_root/disabled-current-build-sentinel"

set +e
disabled_current_output=$(cmake -S "$repo_root" -B "$disabled_configure_dir" \
  -DCPKT_TARGET_ARCH=cachetest \
  -DCPKT_TARGET_OS=Linux \
  -DCPKT_TARGET_LIBC=gnu \
  -DCPKT_BUILD_DEPENDENCIES=OFF \
  -DCMAKE_C_FLAGS=-DCPKT_STALE_ROOT_DISABLED_CURRENT \
  -DCPKT_DEPENDENCY_CACHE="$shared_cache" 2>&1)
disabled_current_status=$?
set -e

if [ "$first_status" -ne "$disabled_current_status" ]; then
  printf 'stale-root fixture changed configure exit status with dependency builds disabled and current contracts\nfirst=%s disabled=%s\n' "$first_status" "$disabled_current_status" >&2
  exit 1
fi
case "$disabled_current_output" in
  *"is stale"*)
    printf 'current repo-local dependency roots were rejected with dependency builds disabled\n%s\n' "$disabled_current_output" >&2
    exit 1
    ;;
esac
if [ ! -e "$external_root/disabled-current-install-sentinel" ] ||
    [ ! -e "$build_root/disabled-current-build-sentinel" ]; then
  printf 'current repo-local dependency roots were deleted with dependency builds disabled\n' >&2
  exit 1
fi

mkdir -p "$explicit_reuse_external_root" "$explicit_reuse_build_root"
: > "$explicit_reuse_external_root/explicit-reuse-install-sentinel"
: > "$explicit_reuse_build_root/explicit-reuse-build-sentinel"

set +e
explicit_reuse_output=$(cmake -S "$repo_root" -B "$explicit_reuse_configure_dir" \
  -DCPKT_TARGET_ARCH=cachetest \
  -DCPKT_TARGET_OS=Linux \
  -DCPKT_TARGET_LIBC=gnu \
  -DCPKT_BUILD_DEPENDENCIES=OFF \
  -DCPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE=ON \
  -DCPKT_CALLER_OWNED_DEPENDENCY_ROOTS=ON \
  -DCPKT_EXTERNAL_ROOT="$explicit_reuse_external_root" \
  -DCPKT_DEPENDENCY_BUILD_ROOT="$explicit_reuse_build_root" \
  -DCMAKE_C_FLAGS=-DCPKT_STALE_ROOT_EXPLICIT_REUSE \
  -DCPKT_DEPENDENCY_CACHE="$shared_cache" 2>&1)
explicit_reuse_status=$?
set -e

if [ "$first_status" -ne "$explicit_reuse_status" ]; then
  printf 'stale-root fixture changed configure exit status when explicit dependency roots were reused\nfirst=%s explicit=%s\n' "$first_status" "$explicit_reuse_status" >&2
  exit 1
fi
case "$explicit_reuse_output" in
  *"is stale"*)
    printf 'explicit dependency-root reuse was rejected as stale lifecycle-owned state\n%s\n' "$explicit_reuse_output" >&2
    exit 1
    ;;
esac
if [ ! -e "$explicit_reuse_external_root/explicit-reuse-install-sentinel" ] ||
    [ ! -e "$explicit_reuse_build_root/explicit-reuse-build-sentinel" ]; then
  printf 'explicit dependency-root reuse deleted caller-owned roots\n' >&2
  exit 1
fi

set +e
disabled_stale_output=$(cmake -S "$repo_root" -B "$disabled_configure_dir" \
  -DCPKT_TARGET_ARCH=cachetest \
  -DCPKT_TARGET_OS=Linux \
  -DCPKT_TARGET_LIBC=gnu \
  -DCPKT_BUILD_DEPENDENCIES=OFF \
  -DCMAKE_C_FLAGS=-DCPKT_STALE_ROOT_DISABLED_STALE \
  -DCPKT_DEPENDENCY_CACHE="$shared_cache" 2>&1)
disabled_stale_status=$?
set -e

if [ "$disabled_stale_status" -eq 0 ]; then
  printf 'stale repo-local dependency roots were accepted with dependency builds disabled\n' >&2
  exit 1
fi
case "$disabled_stale_output" in
  *"repo-local external dependency root for $target_id is stale"*) ;;
  *)
    printf 'stale repo-local dependency roots did not produce an actionable disabled-build diagnostic\n%s\n' "$disabled_stale_output" >&2
    exit 1
    ;;
esac
if [ ! -e "$external_root/disabled-current-install-sentinel" ] ||
    [ ! -e "$build_root/disabled-current-build-sentinel" ]; then
  printf 'stale repo-local dependency roots were deleted even though dependency builds were disabled\n' >&2
  exit 1
fi

printf 'repo dependency stale-root invalidation passed\n'
