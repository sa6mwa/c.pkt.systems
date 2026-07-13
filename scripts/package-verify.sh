#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

bundle_version=$(bash "$repo_root/scripts/release-version.sh" "$repo_root")
if [ -z "$bundle_version" ]; then
  printf 'failed to resolve c.pkt.systems bundle version\n' >&2
  exit 1
fi

explicit_targets=0
if [ "$#" -gt 0 ]; then
  explicit_targets=1
  targets="$*"
else
  targets="x86_64-linux-gnu x86_64-linux-musl aarch64-linux-gnu aarch64-linux-musl armhf-linux-gnu armhf-linux-musl arm64-apple-darwin"
  if ! bash "$repo_root/scripts/osxcross_available.sh"; then
    printf 'arm64-apple-darwin package verification requires a complete local osxcross SDK toolchain\n' >&2
    exit 1
  fi
fi

bash "$repo_root/tests/package_install_smoke_args_test.sh"
bash "$repo_root/tests/lifecycle_surface_test.sh"
bash "$repo_root/tests/osxcross_linker_route_test.sh"
bash "$repo_root/tests/package_assertions_darwin_tool_lookup_test.sh"
bash "$repo_root/tests/darwin_dependency_install_name_test.sh"
bash "$repo_root/tests/darwin_dependency_metadata_config_test.sh"
bash "$repo_root/tests/darwin_generated_install_name_patch_test.sh"
bash "$repo_root/tests/run_no_warnings_test.sh"
bash "$repo_root/tests/privacy_scan_failure_test.sh"
bash "$repo_root/tests/mqttc_linker_flags_test.sh"
bash "$repo_root/tests/sus_cpu_backend_policy_test.sh"
bash "$repo_root/tests/opcua_registration_test.sh"
bash "$repo_root/tests/opcua_header_facade_test.sh"
bash "$repo_root/tests/audio_sus_header_facade_test.sh"
bash "$repo_root/tests/opcua_word_portability_test.sh"
bash "$repo_root/tests/version_resolution_test.sh"
bash "$repo_root/tests/dist_manifest_test.sh"
bash "$repo_root/tests/gnu_tar_lookup_test.sh"
bash "$repo_root/tests/source_archive_portability_test.sh"
bash "$repo_root/tests/source_archive_verify_failure_test.sh"
bash "$repo_root/tests/source_archive_git_ignore_test.sh"
bash "$repo_root/tests/source_archive_git_parent_test.sh"
bash "$repo_root/scripts/verify-dist-manifest.sh" "$repo_root/dist" c.pkt.systems "$bundle_version"

for target_id in $targets; do
  archive="$repo_root/dist/c.pkt.systems-$bundle_version-$target_id.tar.gz"
  package_assertion_tool_args=()
  case "$target_id" in
    *-linux-*)
      toolchain_report=$("$repo_root/scripts/cpkt-toolchains.sh" discover "$target_id")
      toolchain_value() {
        local key=$1 value
        value=$(printf '%s\n' "$toolchain_report" | sed -n "s/^${key}=//p")
        if [ -z "$value" ]; then
          printf 'pinned Bootlin toolchain did not report %s for %s\n' "$key" "$target_id" >&2
          exit 1
        fi
        printf '%s\n' "$value"
      }
      toolchain_nm=$(toolchain_value nm)
      toolchain_ar=$(toolchain_value ar)
      toolchain_readelf=$(toolchain_value readelf)
      package_assertion_tool_args=(
        -DCPKT_NM="$toolchain_nm"
        -DCPKT_AR="$toolchain_ar"
        -DCPKT_READELF="$toolchain_readelf")
      ;;
  esac
  cmake \
    -DCPKT_ARCHIVE="$archive" \
    -DCPKT_TARGET_ID="$target_id" \
    -DCPKT_BUNDLE_VERSION="$bundle_version" \
    "${package_assertion_tool_args[@]}" \
    -P "$repo_root/cmake/package_assertions.cmake"
  cmake \
    -DCPKT_ROOT="$repo_root" \
    -DCPKT_SCAN_LABEL="bundle" \
    -DCPKT_SCAN_PATHS="$archive" \
    -P "$repo_root/tests/privacy_scan.cmake"
  case "$target_id" in
    arm64-apple-darwin)
      smoke_zip="$repo_root/dist/c.pkt.systems-$bundle_version-$target_id-smoke-test.zip"
      if [ -f "$smoke_zip" ]; then
        cmake \
          -DCPKT_ROOT="$repo_root" \
          -DCPKT_SCAN_LABEL="darwin smoke test bundle" \
          -DCPKT_SCAN_PATHS="$smoke_zip" \
          -P "$repo_root/tests/privacy_scan.cmake"
      fi
      ;;
  esac
  case "$target_id" in
    *-linux-*)
      bash "$repo_root/scripts/package-install-smoke.sh" \
        "$archive" \
        "$target_id" \
        "$repo_root/examples/abi_smoke.c"
      ;;
    arm64-apple-darwin)
      bash "$repo_root/scripts/package-install-smoke.sh" \
        "$archive" \
        "$target_id" \
        "$repo_root/examples/abi_smoke.c"
      ;;
  esac
done

source_archive="$repo_root/dist/c.pkt.systems-$bundle_version.tar.gz"
if [ -f "$source_archive" ]; then
  bash "$repo_root/scripts/source-archive-verify.sh" "$source_archive" "$bundle_version"
elif [ "$explicit_targets" -eq 0 ]; then
  printf 'missing source archive: %s\n' "$source_archive" >&2
  exit 1
fi
