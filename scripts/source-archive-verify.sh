#!/usr/bin/env bash
set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
case ${GROUP:-all} in all) ;; *) printf 'this operation requires GROUP=all\n' >&2; exit 2 ;; esac
if [[ -z ${CPKT_OPERATION_FD:-} ]]; then
  exec python3 "$repo_root/scripts/cpkt_operation.py" --group all -- bash "$0" "$@"
fi
python3 "$repo_root/scripts/cpkt_operation.py" --group all --check

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
archive_listing=$(cmake -E tar tf "$archive_path")
while IFS= read -r entry; do
  case "$entry" in
    /*|..|../*|*/../*|*/..)
      printf 'source archive contains unsafe entry: %s\n' "$entry" >&2
      exit 1
      ;;
  esac
done <<< "$archive_listing"

mkdir -p "$repo_root/build"
work_dir=$(mktemp -d "$repo_root/build/cpkt-source-verify.XXXXXXXXXX")
cleanup() {
  rm -rf "$work_dir"
}
trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

python3 "$repo_root/scripts/cpkt_archive_extract.py" "$archive_path" "$work_dir" "$archive_stem" --source

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
while IFS= read -r entry; do
  case "$entry" in
    "$archive_stem"|"$archive_stem/"|"$archive_stem/"*) ;;
    *)
      printf 'source archive contains entry outside its root: %s\n' "$entry" >&2
      exit 1
      ;;
  esac
done <<< "$archive_listing"

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

for scratch in "$source_root"/privacy-scan-*; do
  if [ -e "$scratch" ] || [ -L "$scratch" ]; then
    printf 'source archive includes generated/private path: %s\n' "${scratch##*/}" >&2
    exit 1
  fi
done

for required in \
  CMakeLists.txt \
  Makefile \
  README.md \
  scripts/run-selected-tests.py \
  tests/selected_test_executables_test.py \
  tests/darwin_test_runtime_paths_test.py \
  tests/sqlite_vfs_handle_views_test.c \
  tests/auth_package_discovery_test.py \
  tests/postgres_pending_registration_test.c \
  tests/postgres_tracking_lifetime_test.c \
  tests/postgres_notice_registration_test.c \
  devenv.yaml.in \
  docs/dependencies.md \
  docs/logging-hooks.md \
  docs/opcua-c89-facade-spec.md \
  docs/gssapi-c89-facade-spec.md \
  docs/sasl-c89-facade-spec.md \
  docs/postgres-c89-facade-spec.md \
  docs/sqlite-c89-facade-spec.md \
  docs/sqlite-c89-facade-surface.md \
  docs/third_party/sqlite/LICENSE \
  include/cpkt/opcua.h \
  include/cpkt/gssapi.h \
  include/cpkt/sasl.h \
  include/cpkt/postgres.h \
  include/cpkt/sqlite.h \
  include/cpkt/sqlite_constants.h \
  cmake/exports/cpkt_postgres.txt \
  cmake/exports/cpkt_sqlite.txt \
  scripts/db_api_inventory.py \
  scripts/db_api_contract.py \
  scripts/configured_build.py \
  tests/contracts/db_native_api.json \
  tests/contracts/db_facade_api.json \
  tests/contracts/db_function_bindings.json \
  tests/contracts/db_variable_bindings.json \
  tests/contracts/db_record_bindings.json \
  tests/contracts/db_typedef_bindings.json \
  tests/contracts/db_enum_bindings.json \
  tests/contracts/db_constant_bindings.json \
  tests/contracts/db_operation_bindings.json \
  tests/db_contract_mutation_test.py \
  tests/contracts_out_of_source_test.py \
  tests/sqlite_owned_source_contract_test.py \
  tests/db_export_policy_test.sh \
  tests/postgres_events_test.c \
  tests/postgres_event_allocation_test.c \
  tests/postgres_result_retrieval_native_peer.c \
  tests/postgres_event_concurrent_release_test.c \
  tests/sqlite_native_views_test.c \
  tests/sqlite_completion_test.c \
  tests/sqlite_log_native_peer_test.c \
  tests/sqlite_error_code_native_peer_test.c \
  tests/sqlite_loadable_extension.c \
  tests/sqlite_loadable_extension_test.c \
  tests/sqlite_stream_failure_test.c \
  tests/sqlite_temp_directory_test.c \
  tests/sqlite_temp_directory_native_peer.c \
  src/postgres_events.c \
  src/postgres_events_private.h \
  src/postgres_event_alloc.c \
  src/sqlite_native_views.c \
  src/sqlite_native_views.h \
  src/sqlite_native_methods.c \
  src/sqlite_native_methods.h \
  src/sqlite_native_amalgamation.c \
  include/cpkt/opcua_types_base.h \
  include/cpkt/opcua_util.h \
  include/cpkt/opcua_callbacks.h \
  src/opcua_internal.h \
  src/opcua_arrays.c \
  src/opcua_browse.c \
  src/opcua_client.c \
  src/opcua_client_async.c \
  src/opcua_client_attributes.c \
  src/opcua_client_nodes.c \
  src/opcua_client_values.c \
  src/opcua_events.c \
  src/opcua_files.c \
  src/opcua_history.c \
  src/opcua_identifiers.c \
  src/opcua_methods.c \
  src/opcua_pubsub.c \
  src/opcua_server.c \
  src/opcua_server_attributes.c \
  src/opcua_server_nodes.c \
  src/opcua_server_values.c \
  src/opcua_subscriptions.c \
  src/opcua_value.c \
  src/opcua_facade_internal.h \
  src/opcua_logging.c src/opcua_logger_impl.h \
  src/opcua_security_impl.h \
  src/opcua_pubsub_internal.h src/opcua_codec_impl.h src/opcua_events_impl.h src/opcua_dynamic_internal.h src/opcua_dynamic_impl.h src/opcua_eventloop_impl.h src/opcua_eventloop_lifecycle_impl.h src/opcua_range_impl.h src/opcua_config_impl.h src/opcua_config_accesscontrol_impl.h src/opcua_operations_impl.h src/opcua_config_discovery_impl.h src/opcua_config_types_impl.h src/opcua_config_plugins_impl.h src/opcua_nodestore_impl.h src/opcua_nodestore_nodes_impl.h \
  src/opcua_pubsub_impl.h \
  src/opcua_callbacks_impl.h \
  src/opcua_client_impl.h \
  src/opcua_map_impl.h \
  src/opcua_util_impl.h \
  src/opcua_identifiers_impl.h \
  src/opcua_plugins_impl.h \
  src/opcua_producers_impl.h \
  src/opcua_producers_install_impl.h \
  src/opcua_creation_impl.h \
  src/opcua_nodes_impl.h \
  src/opcua_history_impl.h \
  src/opcua_nodes_install_impl.h \
  src/opcua_types.c \
  src/opcua_types_internal.h \
  tools/opcua/generate.py \
  tools/opcua/c89_emitter.py \
  tools/opcua/plugin_emitter.py \
  tools/opcua/history_emitter.py \
  tools/opcua/server_emitter.py \
  tools/opcua/producer_emitter.py \
  tools/opcua/creation_emitter.py \
  tools/opcua/async_client_emitter.py \
  tools/opcua/core_client_emitter.py \
  tools/opcua/value_emitter.py \
  tools/opcua/certificate_emitter.py \
  tools/opcua/utility_emitter.py tools/opcua/format_emitter.py \
  tools/opcua/security_emitter.py \
  tools/opcua/policy_emitter.py \
  tools/opcua/pubsub_emitter.py tools/opcua/codec_emitter.py tools/opcua/event_emitter.py tools/opcua/dynamic_emitter.py tools/opcua/eventloop_emitter.py tools/opcua/config_emitter.py tools/opcua/config_plugin_emitter.py tools/opcua/config_server_plugin_emitter.py tools/opcua/nodestore_emitter.py \
  tools/opcua/client_emitter.py \
  tools/opcua/node_emitter.py \
  tools/opcua/public_api.py \
  tools/opcua/header_contract.py \
  tests/opcua_header_contract_test.py \
  tests/opcua_format_optimized_compile_test.py tests/dependency_install_order_test.py tests/zlib_feature_namespace_test.py \
  tests/open62541_openssl_provenance_test.cmake tests/open62541_openssl_provenance_fixture_test.py \
  tests/libssh2_poll_elapsed_test.c cmake/patch_libssh2_poll_elapsed.cmake \
  tests/krb5_error_format_test.c cmake/patches/krb5_error_va_list.patch \
  tests/gssapi_logging_test.c cmake/patches/krb5_gss_trace_callback.patch \
  tests/openldap_sasl_logging_test.c \
  cmake/patches/openldap_sasl_logging.patch \
  cmake/patches/openldap_c89_log_setter.patch \
  tests/postgres_diagnostic_test.c \
  cmake/patches/postgresql_logging.patch \
  cmake/patches/whisper_ggml_validation_logging.patch \
  tools/opcua/public_api_contract.json \
  tests/opcua_public_api_test.py \
  tests/opcua_types_test.c \
  tests/opcua_certificate_test.c tests/opcua_certificate_native_peer.c \
  tests/opcua_config_test.c tests/opcua_config_native_peer.c \
  tests/opcua_format_test.c tests/opcua_transport_test.c \
  tests/opcua_operations_test.c tests/opcua_nodestore_test.c \
  tests/opcua_callbacks_test.c \
  tests/opcua_plugins_test.c \
  tests/opcua_history_backend_test.c \
  tests/opcua_memory_backend_test.c \
  tests/opcua_server_types_test.c \
  tests/opcua_util_test.c \
  tests/opcua_identifiers_test.c \
  tests/opcua_producers_test.c \
  tests/opcua_creation_test.c \
  tests/opcua_external_test.c \
  tests/opcua_async_client_test.c \
  tests/opcua_core_client_test.c \
  tests/opcua_client_test.c \
  tests/opcua_nodes_test.c \
  tests/opcua_callbacks_test.h \
  tests/opcua_types_native_peer.c \
  tests/opcua_types_peer.h \
  tests/opcua_types_alloc_failure.c \
  tests/opcua_stdout_contention_test.c \
  tests/opcua_types_fixture.bsd \
  tests/opcua_types_fixture_test.c \
  tests/opcua_types_cpp_test.cpp \
  tests/opcua_types_exports_test.py \
  tests/opcua_types_generator_test.py \
  docs/third_party/opcua-schema/LICENSE \
  docs/third_party/opcua-formatter/LICENSE \
  src/opcua.c \
  src/gssapi.c \
  src/sasl.c \
  src/postgres.c \
  src/sqlite.c \
  tests/opcua_facade_test.c \
  tests/opcua_c89_boundary_peer.c \
  tests/opcua_header_facade_test.sh \
  tests/gssapi_header_facade_test.sh \
  tests/sasl_facade_test.c \
  tests/sasl_header_facade_test.sh \
  tests/postgres_e2e_harness_test.sh \
  tests/postgres_integration_test.c \
  tests/valgrind.supp \
  tests/postgres_header_facade_test.sh \
  tests/sqlite_facade_test.c \
  tests/sqlite_api_coverage_test.sh \
  tests/sqlite_header_facade_test.sh \
  examples/opcua-c89/main.c \
  scripts/cpkt_afl_discover.py \
  scripts/cpkt_build_guard.py \
  scripts/cpkt_make_program.py \
  scripts/cpkt_clangd_check.py \
  scripts/cpkt_cmake_inputs.py \
  scripts/cpkt_configure_guard.py \
  scripts/cpkt_expected_tests.py \
  scripts/cpkt_fuzz_run.py \
  scripts/cpkt_helper_dispatch.py \
  scripts/cpkt_helper_proof.py \
  scripts/cpkt_inventory.py \
  scripts/cpkt_inventory_cli.py \
  scripts/cpkt_lifecycle.py \
  scripts/cpkt_operation.py \
  scripts/cpkt_packages.py \
  scripts/cpkt_package_command.py \
  scripts/cpkt_archive_assert.py \
  scripts/cpkt_archive_extract.py \
  scripts/cpkt_source_reconstruct.py \
  scripts/cpkt_source_proof.py \
  scripts/cpkt_github_handoff.py \
  scripts/cpkt_darwin.py \
  scripts/group-build.py \
  src/cmocka_c89.c \
  cmake/CpktPackage.cmake \
  cmake/cmocka_metadata.cmake \
  cmake/exports/cpkt_cmocka.txt \
  tests/cmocka_downstream_behavior.c \
  tests/cmocka_downstream_compile.py \
  tests/cmocka_downstream_failure.py \
  tests/package_recipe_graph_test.py \
  tests/package_smoke_contract_test.py \
  tests/lifecycle_recipe_contract.py \
  tests/dist_manifest_test.py \
  scripts/cpkt_verify_manifest.py \
  scripts/cpkt_sdk_examples.py \
  tests/github_actions_contract_test.py \
  tests/package_integration_contract_test.py \
  tests/cmocka_c89_surface.c \
  docs/sdk-installation.md \
  docs/cmocka-c89.md \
  scripts/cpkt_preflight.py \
  scripts/cpkt_presets.py \
  scripts/cpkt_receipt_cli.py \
  scripts/cpkt_receipts.py \
  scripts/cpkt_reserved_tag.py \
  scripts/cpkt_sdk_consumer.py \
  scripts/cpkt_service_exec.py \
  scripts/validate-sdk.py \
  cmake/CpktCmockaFacade.cmake \
  cmake/CpktComposition.cmake \
  cmake/CpktDependencies.cmake \
  cmake/CpktDependencyArchiveCache.cmake \
  cmake/CpktDependencyContract.cmake \
  cmake/CpktGroups.cmake \
  cmake/CpktLocalRuntime.cmake \
  cmake/CpktOperation.cmake \
  cmake/CpktReadOnlyAflToolchain.cmake \
  cmake/CpktReadOnlyToolchain.cmake \
  cmake/CpktSDKValidate.cmake \
  cmake/CpktTestPslog.cmake \
  cmake/CpktToolchainDiscovery.cmake \
  cmake/components.json \
  cmake/package_inspection.cmake \
  cmake/package_metadata.cmake \
  tools/generate_cmocka_c89.py \
  scripts/package-source.sh \
  scripts/cpkt-archive-cache.sh \
  skills/pkt-systems-cmake-lifecycle/scripts/cpkt-archive-cache.sh \
  tests/dependency_cache_network_hits_test.py \
  scripts/e2e-postgres.sh \
  scripts/devenv.sh \
  scripts/test-e2e.sh \
  scripts/package-verify.sh \
  scripts/release-version.sh \
  scripts/source-archive-verify.sh \
  cmake/CpktDependencies.cmake \
  cmake/CpktTestPslog.cmake \
  tests/opcua_logging_test.c \
  tests/opcua_logging_native_peer.c \
  tests/opcua_logging_peer.h \
  tests/opcua_logging_alloc_failure.c \
  tests/opcua_logging_output_test.py \
  tests/opcua_logging_sdk_isolation_test.cmake \
  cmake/build_openldap_libraries.cmake \
  cmake/cyrus_sasl_md5global.h.in \
  cmake/patch_openldap_lutil_link.cmake \
  cmake/patch_postgresql_buildinfo.cmake \
  vendor/open62541/patches/series \
  vendor/open62541/patches/0001-prefix-embedded-mqtt-c-symbols.patch \
  vendor/open62541/patches/0003-stub-posix-ethernet-when-packet-headers-are-missing.patch \
  vendor/open62541/patches/0004-link-bundled-openssl-crypto.patch \
  vendor/open62541/patches/0005-route-key-derivation-errors-through-logger.patch \
  vendor/open62541/patches/0006-check-default-history-allocations.patch \
  vendor/open62541/patches/0007-safe-reentrant-async-result-callbacks.patch \
  vendor/open62541/patches/0008-preserve-date-parser-overflow-guard-direction.patch \
  vendor/open62541/patches/0009-release-method-argument-ownership.patch \
  vendor/open62541/patches/0010-use-external-source-notification-slots.patch \
  vendor/open62541/patches/0011-check-memory-history-backend-allocations.patch \
  vendor/open62541/patches/0012-check-certificate-trust-allocations.patch \
  vendor/open62541/patches/0013-own-filestore-policy-metadata.patch \
  vendor/open62541/patches/0014-own-datatype-copy-metadata.patch \
  vendor/open62541/patches/0015-use-mutable-ec-keygen-argument.patch \
  vendor/open62541/patches/0016-fix-variant-range-moves.patch \
  vendor/open62541/patches/0017-preserve-event-source-free-failures.patch \
  vendor/open62541/patches/0018-copy-client-config-owned-values.patch \
  vendor/open62541/patches/0019-check-node-copy-and-filestore-helpers.patch \
  vendor/open62541/patches/0020-check-discovery-and-reverse-connect.patch \
  vendor/open62541/patches/0021-install-native-formatter-and-fix-minima.patch \
  vendor/open62541/patches/0022-check-default-access-control-allocations.patch \
  vendor/open62541/patches/0023-pin-reverse-connect-iteration-handles.patch \
  vendor/open62541/patches/0024-define-discovery-callback-and-unavailable-policies.patch \
  vendor/open62541/patches/0025-preserve-accept-all-certificate-logger.patch \
  vendor/open62541/patches/0026-count-json-string-closing-quote.patch \
  vendor/open62541/patches/0027-block-on-posix-stdout-lock.patch \
  examples/db-sdk/CMakeLists.txt \
  examples/db-sdk/main.c \
  tests/audio_sus_package_policy_test.py \
  tests/lua_symbol_test_registration_fixture.py \
  tests/package_isolation_build_test.py \
  tests/package_producer_reuse_test.py \
  tests/sdk-consumers/cmocka_native.c \
  tests/sdk-consumers/cpkt_composition.c \
  tests/sdk-consumers/cpkt_all.c \
  tests/sdk-consumers/cpkt_audio_facade_strict.c \
  tests/sdk-consumers/cpkt_audio_sus_facade_strict.c \
  tests/sdk-consumers/cpkt_crypto.c \
  tests/sdk-consumers/cpkt_curl.c \
  tests/sdk-consumers/cpkt_gssapi_facade_strict.c \
  tests/sdk-consumers/cpkt_iodbc_strict.c \
  tests/sdk-consumers/cpkt_libssh2.c \
  tests/sdk-consumers/cpkt_libssh2_facade_strict.c \
  tests/sdk-consumers/cpkt_libssh2_private_sentinel.c \
  tests/sdk-consumers/cpkt_libxml2.c \
  tests/sdk-consumers/cpkt_lua.c \
  tests/sdk-consumers/cpkt_lua_facade_strict.c \
  tests/sdk-consumers/cpkt_lua_runtime_module.c \
  tests/sdk-consumers/cpkt_lua_runtime_strict.c \
  tests/sdk-consumers/cpkt_mqttc.c \
  tests/sdk-consumers/cpkt_mqttc_facade_strict.c \
  tests/sdk-consumers/cpkt_mqttc_private_sentinel.c \
  tests/sdk-consumers/cpkt_nghttp2.c \
  tests/sdk-consumers/cpkt_nghttp2_facade_strict.c \
  tests/sdk-consumers/cpkt_nghttp2_private_sentinel.c \
  tests/sdk-consumers/cpkt_opcua_facade_strict.c \
  tests/sdk-consumers/cpkt_open62541.c \
  tests/sdk-consumers/cpkt_openldap.c \
  tests/sdk-consumers/cpkt_openssl_facade_strict.c \
  tests/sdk-consumers/cpkt_openssl_private_sentinel.c \
  tests/sdk-consumers/cpkt_pdf_facade_strict.c \
  tests/sdk-consumers/cpkt_postgres_facade_strict.c \
  tests/sdk-consumers/cpkt_sasl_facade_strict.c \
  tests/sdk-consumers/cpkt_sqlite_facade_strict.c \
  tests/sdk-consumers/cpkt_ssl.c \
  tests/sdk-consumers/cpkt_sus_facade_strict.c \
  tests/sdk-consumers/cpkt_zlib.c \
  tests/sdk-consumers/sqlite_extension_from_package.c \
  tests/sdk-consumers/sqlite_extension_package_consumer.c \
  tests/sdk-consumers/sus_mixed_main.c \
  tests/sdk-consumers/sus_mixed_probe.cpp \
  tests/sus_split_runtime_policy_test.py \
  vendor/open62541/patches/0028-keep-disabled-lock-assert-warning-clean.patch
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


if [[ -n ${CPKT_SOURCE_ARCHIVE_TOOLCHAIN_FILE:-} && ! -f $CPKT_SOURCE_ARCHIVE_TOOLCHAIN_FILE ]]; then
  printf 'source archive toolchain file does not exist: %s\n' "$CPKT_SOURCE_ARCHIVE_TOOLCHAIN_FILE" >&2
  exit 1
fi
if [[ -n ${CPKT_SOURCE_ARCHIVE_TOOLCHAIN_FILE:-} && $(realpath "$CPKT_SOURCE_ARCHIVE_TOOLCHAIN_FILE") != $(realpath "$repo_root/cmake/toolchains/x86_64-linux-gnu.cmake") ]]; then
  printf 'source reconstruction requires the selected pinned native GNU toolchain, not an arbitrary override\n' >&2
  exit 1
fi
reconstruction_environment=$(python3 "$repo_root/scripts/cpkt_source_reconstruct.py" --environment-from "$repo_root")
# JSON is parsed into quoted shell exports; configured paths are never shell code.
eval "$(python3 -c 'import json,shlex,sys; print("\n".join("export "+k+"="+shlex.quote(v) for k,v in json.loads(sys.argv[1]).items()))' "$reconstruction_environment")"
mkdir -p "$repo_root/build/verification/source/$expected_version"
reconstruction_log="$repo_root/build/verification/source/$expected_version/reconstruction.log"
python3 "$repo_root/scripts/cpkt_operation.py" --root "$repo_root" --group all \
  --source-root "$source_root" -- python3 "$source_root/scripts/cpkt_source_reconstruct.py" 2>&1 | tee "$reconstruction_log"
python3 "$repo_root/scripts/cpkt_source_proof.py" "$archive_path" "$expected_version" "$source_root"
printf '[package] verified source archive %s\n' "$archive_path"
