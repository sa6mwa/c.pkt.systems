#!/usr/bin/env bash
set -euo pipefail

repo_root=${1:?repo root is required}
build_dir="$repo_root/build/dependency-component-targets-test"
external_root="$repo_root/.cache/deps/dependency-component-targets-test"
dependency_build_root="$repo_root/.cache/deps-build/dependency-component-targets-test"
contract_root="$repo_root/build/dependency-component-targets-contracts"

cleanup() {
  rm -rf "$build_dir" "$external_root" "$dependency_build_root" "$contract_root"
}
trap cleanup EXIT INT TERM
cleanup

if rg -n 'add_dependencies\\([^)]*cpkt_deps\\)' "$repo_root/CMakeLists.txt"; then
  printf 'facades, examples, and tests must not depend on the universal cpkt_deps target\n' >&2
  exit 1
fi

cmake -S "$repo_root" --preset debug -B "$build_dir" \
  -DCPKT_BUILD_DEPENDENCIES=ON \
  -DCPKT_BUILD_TESTS=OFF \
  -DCPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE=ON \
  -DCPKT_EXTERNAL_ROOT="$external_root" \
  -DCPKT_DEPENDENCY_BUILD_ROOT="$dependency_build_root" \
  -DCPKT_DEPENDENCY_CONTRACT_ROOT="$contract_root" >/dev/null
target_help=$(cmake --build "$build_dir" --target help)
for component in \
  openssl zlib nghttp2 libssh2 curl libxml2 lua miniaudio whisper mqttc \
  open62541 krb5 cyrus_sasl openldap postgresql sqlite; do
  case "$target_help" in
    *"cpkt_deps_$component"*) ;;
    *)
      printf 'component target is missing: cpkt_deps_%s\n' "$component" >&2
      exit 1
      ;;
  esac
done
case "$target_help" in
  *"cpkt_deps_all"*) ;;
  *) printf 'all-dependencies target is missing\n' >&2; exit 1 ;;
esac

sqlite_plan=$(cmake --build "$build_dir" --target cpkt_sqlite_static -- -n)
case "$sqlite_plan" in
  *"cpkt_sqlite_project"*) ;;
  *) printf 'sqlite facade build plan does not include the SQLite dependency\n' >&2; exit 1 ;;
esac
case "$sqlite_plan" in
  *"cpkt_deps_all"*|*"cpkt_openssl_project"*|*"cpkt_curl_project"*)
    printf 'sqlite facade build plan includes an unrelated dependency closure\n' >&2
    exit 1
    ;;
esac
