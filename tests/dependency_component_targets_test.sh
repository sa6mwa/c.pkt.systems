#!/usr/bin/env bash
set -euo pipefail

repo_root=${1:?repo root is required}
build_dir="$repo_root/build/dependency-component-targets-test"
external_root="$build_dir/deps"
dependency_build_root="$build_dir/deps-build"
contract_root="$build_dir/contracts"

cleanup() {
  cmake -E remove_directory "$build_dir"
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
  *"cpkt_deps_all"*|*"cpkt_openssl_project"*|*"cpkt_curl_project"*|*"cpkt_postgresql_project"*|*"cpkt_krb5_static_project"*)
    printf 'sqlite facade build plan includes an unrelated dependency closure\n' >&2
    exit 1
    ;;
esac

lua_plan=$(cmake --build "$build_dir" --target cpkt_lua_static -- -n)
case "$lua_plan" in
  *"cpkt_lua_project"*) ;;
  *) printf 'Lua facade build plan does not include Lua\n' >&2; exit 1 ;;
esac
case "$lua_plan" in
  *"cpkt_deps_all"*|*"cpkt_openssl_project"*|*"cpkt_curl_project"*|*"cpkt_postgresql_project"*|*"cpkt_sqlite_project"*|*"cpkt_open62541_static_project"*)
    printf 'Lua facade build plan includes an unrelated dependency closure\n' >&2
    exit 1
    ;;
esac

lua_generator_inputs=$(ninja -C "$build_dir" -t query generated/lua/include/cpkt/lua.h)
for lua_header in lua.h luaconf.h lauxlib.h lualib.h; do
  case "$lua_generator_inputs" in
    *"deps/lua/install/include/$lua_header"*) ;;
    *)
      printf 'Lua facade generator does not depend on %s\n' "$lua_header" >&2
      exit 1
      ;;
  esac
done

postgres_plan=$(cmake --build "$build_dir" --target cpkt_postgres_static -- -n)
case "$postgres_plan" in
  *"cpkt_postgresql_project"*) ;;
  *) printf 'PostgreSQL facade build plan does not include libpq\n' >&2; exit 1 ;;
esac
case "$postgres_plan" in
  *"cpkt_deps_all"*|*"cpkt_sqlite_project"*|*"cpkt_lua_project"*|*"cpkt_miniaudio_project"*|*"cpkt_whisper_static_project"*|*"cpkt_open62541_static_project"*)
    printf 'PostgreSQL facade build plan includes an unrelated dependency closure\n' >&2
    exit 1
    ;;
esac

sus_plan=$(cmake --build "$build_dir" --target cpkt_sus_static -- -n)
case "$sus_plan" in
  *"cpkt_whisper_static_project"*) ;;
  *) printf 'speech facade build plan does not include static whisper.cpp\n' >&2; exit 1 ;;
esac
case "$sus_plan" in
  *"cpkt_deps_all"*|*"cpkt_whisper_shared_project"*|*"cpkt_open62541_static_project"*|*"cpkt_open62541_shared_project"*|*"cpkt_postgresql_project"*|*"cpkt_sqlite_project"*)
    printf 'speech facade build plan includes an unrelated or shared dependency closure\n' >&2
    exit 1
    ;;
esac

opcua_plan=$(cmake --build "$build_dir" --target cpkt_opcua_static -- -n)
for required in cpkt_open62541_static_project cpkt_openssl_project cpkt_mqttc_project; do
  case "$opcua_plan" in
    *"$required"*) ;;
    *) printf 'OPC UA facade build plan lacks required dependency: %s\n' "$required" >&2; exit 1 ;;
  esac
done
case "$opcua_plan" in
  *"cpkt_deps_all"*|*"cpkt_open62541_shared_project"*|*"cpkt_whisper_static_project"*|*"cpkt_whisper_shared_project"*|*"cpkt_postgresql_project"*|*"cpkt_sqlite_project"*)
    printf 'OPC UA facade build plan includes an unrelated or shared dependency closure\n' >&2
    exit 1
    ;;
esac

krb5_plan=$(cmake --build "$build_dir" --target cpkt_deps_krb5 -- -n)
case "$krb5_plan" in
  *"cpkt_krb5_static_project"*|*"cpkt_krb5_shared_project"*) ;;
  *) printf 'Kerberos component build plan does not include Kerberos\n' >&2; exit 1 ;;
esac
case "$krb5_plan" in
  *"cpkt_deps_all"*|*"cpkt_cyrus_sasl_project"*|*"cpkt_openldap_project"*|*"cpkt_postgresql_project"*)
    printf 'Kerberos component build plan includes an unrelated dependency closure\n' >&2
    exit 1
    ;;
esac
