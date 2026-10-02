#!/usr/bin/env bash
set -euo pipefail

repo_root=$1
contract="$repo_root/cmake/CpktDependencyContract.cmake"
dependencies="$repo_root/cmake/CpktDependencies.cmake"

if grep -R -n -E 'CPKT_DEPENDENCY_SET_ID|CpktDependency(Set|Toolchain)Identity|cpkt_dependency_(set|toolchain)_cache_id|dependency_(set|toolchain)_identity|CPKT_CURL_BUILD_CACHE_ID|CPKT_TOOLCHAIN_IDENTITY' "$repo_root/CMakeLists.txt" "$repo_root/cmake" "$repo_root/skills"; then
  printf 'repo-local dependency cache identity pathing must not be reintroduced\n' >&2
  exit 1
fi

for required in \
  'set(_cpkt_default_external_root "${CMAKE_SOURCE_DIR}/.cache/deps/${CPKT_TARGET_ID}")' \
  'set(_cpkt_default_dependency_build_root "${CMAKE_SOURCE_DIR}/.cache/deps-build/${CPKT_TARGET_ID}")' \
  'cpkt_prepare_dependency_component' \
  'CPKT_DEPENDENCY_CONTRACT_ROOT' \
  'file(REMOVE_RECURSE "${_root}")' \
  'Dependency rebuilding is disabled; reconfigure with CPKT_BUILD_DEPENDENCIES=ON to refresh only this component.' \
  'caller-owned dependency state' \
  'must not contain symlinks'; do
  if ! grep -F "$required" "$repo_root/CMakeLists.txt" "$contract" >/dev/null 2>&1; then
    printf 'component dependency cache policy is missing: %s\n' "$required" >&2
    exit 1
  fi
done

if grep -n -E 'add_dependencies\\([^)]*cpkt_deps\\)' "$repo_root/CMakeLists.txt"; then
  printf 'facades and test targets must not depend on the universal cpkt_deps target\n' >&2
  exit 1
fi

for component in \
  openssl zlib nghttp2 libssh2 curl libxml2 lua miniaudio whisper mqttc \
  open62541 krb5 cyrus_sasl openldap postgresql sqlite; do
  if ! grep -F "add_custom_target(cpkt_deps_$component" "$dependencies" >/dev/null 2>&1; then
    printf 'independent dependency target is missing: %s\n' "$component" >&2
    exit 1
  fi
done

if ! grep -F 'add_custom_target(cpkt_deps_all DEPENDS ${dep_targets})' "$dependencies" >/dev/null 2>&1; then
  printf 'complete dependency closure target is missing\n' >&2
  exit 1
fi

if ! rg -U 'DEPENDS\s+cpkt_deps_all' "$repo_root/CMakeLists.txt" >/dev/null 2>&1; then
  printf 'SDK bundle assembly must retain the complete dependency closure\n' >&2
  exit 1
fi

if ! grep -F 'clean_one "$repo_root/.cache"' "$repo_root/scripts/clean.sh" >/dev/null 2>&1; then
  printf 'make clean must delete repo-local dependency state under .cache\n' >&2
  exit 1
fi

for lifecycle_script in build.sh test.sh package.sh; do
  if grep -F 'bash "$repo_root/scripts/clean.sh" all' "$repo_root/scripts/$lifecycle_script" >/dev/null 2>&1; then
    printf 'scripts/%s must not clean generated state during normal lifecycle work\n' "$lifecycle_script" >&2
    exit 1
  fi
done
