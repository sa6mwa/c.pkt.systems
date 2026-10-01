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
mkdir -p "$repo_root/build"
work_root=$(mktemp -d "$repo_root/build/cpkt-source-verify-failure.XXXXXXXXXX")
cleanup() {
  rm -rf "$work_root"
}
trap cleanup EXIT HUP INT TERM

required_payloads='
CMakeLists.txt
Makefile
README.md
devenv.yaml.in
docs/dependencies.md
docs/logging-hooks.md
docs/opcua-c89-facade-spec.md
docs/gssapi-c89-facade-spec.md
docs/sasl-c89-facade-spec.md
docs/postgres-c89-facade-spec.md
docs/sqlite-c89-facade-spec.md
docs/sqlite-c89-facade-surface.md
docs/third_party/sqlite/LICENSE
include/cpkt/opcua.h
include/cpkt/gssapi.h
include/cpkt/sasl.h
include/cpkt/postgres.h
include/cpkt/sqlite.h
include/cpkt/sqlite_constants.h
cmake/exports/cpkt_postgres.txt
cmake/exports/cpkt_sqlite.txt
scripts/db_api_inventory.py
scripts/db_api_contract.py
scripts/configured_build.py
tests/contracts/db_native_api.json
tests/contracts/db_facade_api.json
tests/contracts/db_function_bindings.json
tests/contracts/db_variable_bindings.json
tests/contracts/db_record_bindings.json
tests/contracts/db_typedef_bindings.json
tests/contracts/db_enum_bindings.json
tests/contracts/db_constant_bindings.json
tests/contracts/db_operation_bindings.json
tests/db_contract_mutation_test.py
tests/contracts_out_of_source_test.py
tests/sqlite_owned_source_contract_test.py
tests/db_export_policy_test.sh
tests/postgres_events_test.c
tests/postgres_event_allocation_test.c
tests/postgres_event_concurrent_release_test.c
tests/sqlite_native_views_test.c
tests/sqlite_completion_test.c
tests/sqlite_log_native_peer_test.c
tests/sqlite_error_code_native_peer_test.c
tests/sqlite_loadable_extension.c
tests/sqlite_loadable_extension_test.c
tests/sqlite_stream_failure_test.c
tests/sqlite_temp_directory_test.c
tests/sqlite_temp_directory_native_peer.c
src/postgres_events.c
src/postgres_events_private.h
src/postgres_event_alloc.c
src/sqlite_native_views.c
src/sqlite_native_views.h
src/sqlite_native_methods.c
src/sqlite_native_methods.h
src/sqlite_native_amalgamation.c
include/cpkt/opcua_types_base.h
include/cpkt/opcua_util.h
include/cpkt/opcua_callbacks.h
src/opcua_internal.h
src/opcua_arrays.c
src/opcua_browse.c
src/opcua_client.c
src/opcua_client_async.c
src/opcua_client_attributes.c
src/opcua_client_nodes.c
src/opcua_client_values.c
src/opcua_events.c
src/opcua_files.c
src/opcua_history.c
src/opcua_identifiers.c
src/opcua_methods.c
src/opcua_pubsub.c
src/opcua_server.c
src/opcua_server_attributes.c
src/opcua_server_nodes.c
src/opcua_server_values.c
src/opcua_subscriptions.c
src/opcua_value.c
src/opcua_facade_internal.h
src/opcua_logging.c
src/opcua_logger_impl.h
src/opcua_security_impl.h
src/opcua_pubsub_internal.h
src/opcua_codec_impl.h
src/opcua_events_impl.h
src/opcua_dynamic_internal.h
src/opcua_dynamic_impl.h
src/opcua_eventloop_impl.h
src/opcua_eventloop_lifecycle_impl.h
src/opcua_config_impl.h
src/opcua_config_accesscontrol_impl.h
src/opcua_operations_impl.h
src/opcua_config_discovery_impl.h
src/opcua_config_types_impl.h
src/opcua_config_plugins_impl.h
src/opcua_nodestore_impl.h
src/opcua_nodestore_nodes_impl.h
src/opcua_range_impl.h
src/opcua_pubsub_impl.h
src/opcua_callbacks_impl.h
src/opcua_client_impl.h
src/opcua_map_impl.h
src/opcua_util_impl.h
src/opcua_identifiers_impl.h
src/opcua_plugins_impl.h
src/opcua_producers_impl.h
src/opcua_producers_install_impl.h
src/opcua_creation_impl.h
src/opcua_nodes_impl.h
src/opcua_history_impl.h
src/opcua_nodes_install_impl.h
src/opcua_types.c
src/opcua_types_internal.h
tools/opcua/generate.py
tools/opcua/c89_emitter.py
tools/opcua/plugin_emitter.py
tools/opcua/history_emitter.py
tools/opcua/server_emitter.py
tools/opcua/producer_emitter.py
tools/opcua/creation_emitter.py
tools/opcua/async_client_emitter.py
tools/opcua/core_client_emitter.py
tools/opcua/value_emitter.py
tools/opcua/certificate_emitter.py
tools/opcua/utility_emitter.py
tools/opcua/format_emitter.py
tools/opcua/security_emitter.py
tools/opcua/policy_emitter.py
tools/opcua/pubsub_emitter.py
tools/opcua/codec_emitter.py
tools/opcua/event_emitter.py
tools/opcua/dynamic_emitter.py
tools/opcua/eventloop_emitter.py
tools/opcua/config_emitter.py
tools/opcua/config_plugin_emitter.py
tools/opcua/config_server_plugin_emitter.py
tools/opcua/nodestore_emitter.py
tools/opcua/client_emitter.py
tools/opcua/node_emitter.py
tools/opcua/public_api.py
tools/opcua/header_contract.py
tests/opcua_header_contract_test.py
tests/opcua_format_optimized_compile_test.py
tests/dependency_install_order_test.py
tests/open62541_openssl_provenance_test.cmake
tests/open62541_openssl_provenance_fixture_test.py
tests/zlib_feature_namespace_test.py
tests/libssh2_poll_elapsed_test.c
cmake/patch_libssh2_poll_elapsed.cmake
tests/krb5_error_format_test.c
cmake/patches/krb5_error_va_list.patch
tests/gssapi_logging_test.c
cmake/patches/krb5_gss_trace_callback.patch
tests/openldap_sasl_logging_test.c
cmake/patches/openldap_sasl_logging.patch
cmake/patches/openldap_c89_log_setter.patch
tests/postgres_diagnostic_test.c
cmake/patches/postgresql_logging.patch
cmake/patches/whisper_ggml_validation_logging.patch
tools/opcua/public_api_contract.json
tests/opcua_public_api_test.py
tests/opcua_types_test.c
tests/opcua_certificate_test.c
tests/opcua_certificate_native_peer.c
tests/opcua_config_test.c
tests/opcua_config_native_peer.c
tests/opcua_format_test.c
tests/opcua_transport_test.c
tests/opcua_operations_test.c
tests/opcua_nodestore_test.c
tests/opcua_callbacks_test.c
tests/opcua_plugins_test.c
tests/opcua_history_backend_test.c
tests/opcua_memory_backend_test.c
tests/opcua_server_types_test.c
tests/opcua_util_test.c
tests/opcua_identifiers_test.c
tests/opcua_producers_test.c
tests/opcua_creation_test.c
tests/opcua_external_test.c
tests/opcua_async_client_test.c
tests/opcua_core_client_test.c
tests/opcua_client_test.c
tests/opcua_nodes_test.c
tests/opcua_callbacks_test.h
tests/opcua_types_native_peer.c
tests/opcua_types_peer.h
tests/opcua_types_alloc_failure.c
tests/opcua_types_fixture.bsd
tests/opcua_types_fixture_test.c
tests/opcua_types_cpp_test.cpp
tests/opcua_types_exports_test.py
tests/opcua_types_generator_test.py
docs/third_party/opcua-schema/LICENSE
docs/third_party/opcua-formatter/LICENSE
src/opcua.c
src/gssapi.c
src/sasl.c
src/postgres.c
src/sqlite.c
tests/opcua_facade_test.c
tests/opcua_c89_boundary_peer.c
tests/opcua_header_facade_test.sh
tests/gssapi_header_facade_test.sh
tests/sasl_facade_test.c
tests/sasl_header_facade_test.sh
tests/postgres_e2e_harness_test.sh
tests/postgres_integration_test.c
tests/valgrind.supp
tests/postgres_header_facade_test.sh
tests/sqlite_facade_test.c
tests/sqlite_api_coverage_test.sh
tests/sqlite_header_facade_test.sh
examples/opcua-c89/main.c
scripts/package-source.sh
scripts/cpkt-archive-cache.sh
skills/pkt-systems-cmake-lifecycle/scripts/cpkt-archive-cache.sh
tests/dependency_cache_network_hits_test.py
scripts/e2e-postgres.sh
scripts/devenv.sh
scripts/test-e2e.sh
scripts/package-verify.sh
scripts/release-version.sh
scripts/source-archive-verify.sh
cmake/CpktDependencies.cmake
cmake/CpktTestPslog.cmake
tests/opcua_logging_test.c
tests/opcua_logging_native_peer.c
tests/opcua_logging_peer.h
tests/opcua_logging_alloc_failure.c
tests/opcua_logging_output_test.py
tests/opcua_logging_sdk_isolation_test.cmake
cmake/build_openldap_libraries.cmake
cmake/cyrus_sasl_md5global.h.in
cmake/patch_openldap_lutil_link.cmake
cmake/patch_postgresql_buildinfo.cmake
vendor/open62541/patches/series
vendor/open62541/patches/0001-prefix-embedded-mqtt-c-symbols.patch
vendor/open62541/patches/0003-stub-posix-ethernet-when-packet-headers-are-missing.patch
vendor/open62541/patches/0004-link-bundled-openssl-crypto.patch
vendor/open62541/patches/0005-route-key-derivation-errors-through-logger.patch
vendor/open62541/patches/0006-check-default-history-allocations.patch
vendor/open62541/patches/0007-safe-reentrant-async-result-callbacks.patch
vendor/open62541/patches/0008-preserve-date-parser-overflow-guard-direction.patch
vendor/open62541/patches/0009-release-method-argument-ownership.patch
vendor/open62541/patches/0010-use-external-source-notification-slots.patch
vendor/open62541/patches/0011-check-memory-history-backend-allocations.patch
vendor/open62541/patches/0012-check-certificate-trust-allocations.patch
vendor/open62541/patches/0013-own-filestore-policy-metadata.patch
vendor/open62541/patches/0014-own-datatype-copy-metadata.patch
vendor/open62541/patches/0015-use-mutable-ec-keygen-argument.patch
vendor/open62541/patches/0016-fix-variant-range-moves.patch
vendor/open62541/patches/0017-preserve-event-source-free-failures.patch
vendor/open62541/patches/0018-copy-client-config-owned-values.patch
vendor/open62541/patches/0019-check-node-copy-and-filestore-helpers.patch
vendor/open62541/patches/0020-check-discovery-and-reverse-connect.patch
vendor/open62541/patches/0021-install-native-formatter-and-fix-minima.patch
vendor/open62541/patches/0022-check-default-access-control-allocations.patch
vendor/open62541/patches/0023-pin-reverse-connect-iteration-handles.patch
vendor/open62541/patches/0024-define-discovery-callback-and-unavailable-policies.patch
vendor/open62541/patches/0025-preserve-accept-all-certificate-logger.patch
vendor/open62541/patches/0026-count-json-string-closing-quote.patch
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

mkdir -p "$work_root/outside-file/stage/c.pkt.systems-1.2.3"
printf 'extra\n' > "$work_root/outside-file/stage/extra.txt"
expect_verify_failure "$(make_archive outside-file c.pkt.systems-1.2.3 extra.txt)" \
  "source archive contains entry outside its root"

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

fixture_root="$work_root/forbidden-privacy-scan/stage/c.pkt.systems-1.2.3"
mkdir -p "$fixture_root/privacy-scan-fixture"
printf '1.2.3\n' > "$fixture_root/VERSION"
write_release_version_script "$fixture_root"
printf 'VERSION\nRELEASE_MANIFEST\nscripts/release-version.sh\n' > "$fixture_root/RELEASE_MANIFEST"
expect_verify_failure "$(make_archive forbidden-privacy-scan c.pkt.systems-1.2.3)" \
  "source archive includes generated/private path: privacy-scan-fixture"

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

# A parent-only signal during configure must stop before build, not merely
# remove the extraction directory and continue into the next release command.
rm "$fixture_root/extra-unlisted.txt"
valid_archive=$(make_archive manifest-mismatch c.pkt.systems-1.2.3)
real_cmake=$(command -v cmake)
mkdir -p "$work_root/bin"
cat > "$work_root/bin/cmake" <<'EOF'
#!/usr/bin/env bash
if [[ $1 == -S ]]; then
  kill -TERM "$PPID"
  exit 0
fi
if [[ $1 == --build ]]; then
  printf 'build ran after interruption\n' > "$CPKT_SIGNAL_BUILD_LOG"
fi
exec "$CPKT_SIGNAL_REAL_CMAKE" "$@"
EOF
chmod +x "$work_root/bin/cmake"
signal_status=0
PATH="$work_root/bin:$PATH" CPKT_SIGNAL_REAL_CMAKE="$real_cmake" \
  CPKT_SOURCE_ARCHIVE_TOOLCHAIN_FILE="$repo_root/cmake/toolchains/x86_64-linux-gnu.cmake" \
  CPKT_SIGNAL_BUILD_LOG="$work_root/signal-build.log" \
  bash "$repo_root/scripts/source-archive-verify.sh" "$valid_archive" 1.2.3 \
    > "$work_root/signal.log" 2>&1 || signal_status=$?
if [[ $signal_status != 143 || -e "$work_root/signal-build.log" ]]; then
  printf 'source verifier continued after SIGTERM (status=%s)\n' "$signal_status" >&2
  cat "$work_root/signal.log" >&2
  exit 1
fi

printf '[test] source archive verifier failure modes passed\n'
