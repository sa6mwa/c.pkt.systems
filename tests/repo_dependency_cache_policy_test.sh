#!/usr/bin/env bash
set -eu

repo_root=${1:?repo root is required}

if grep -R -n -E 'CPKT_DEPENDENCY_SET_ID|CpktDependency(Set|Toolchain)Identity|cpkt_dependency_(set|toolchain)_cache_id|dependency_(set|toolchain)_identity|CPKT_CURL_BUILD_CACHE_ID|CPKT_TOOLCHAIN_IDENTITY' \
    "$repo_root/CMakeLists.txt" "$repo_root/cmake" "$repo_root/skills"; then
  printf 'repo-local dependency cache identity pathing must not be reintroduced\n' >&2
  exit 1
fi

if grep -n -E 'dependency_(set|toolchain)_identity' "$repo_root/CMakeLists.txt"; then
  printf 'identity-specific dependency cache tests must not be reintroduced\n' >&2
  exit 1
fi

if grep -R -n -E 'keyed by target ID and dependency identity|dependency cache identity|toolchain cache identity|semantic repo-local cache IDs' \
    "$repo_root/skills"; then
  printf 'lifecycle skill must not teach semantic repo-local cache ids\n' >&2
  exit 1
fi

if ! grep -F 'set(_cpkt_default_external_root "${CMAKE_SOURCE_DIR}/.cache/deps/${CPKT_TARGET_ID}")' \
    "$repo_root/CMakeLists.txt" >/dev/null 2>&1; then
  printf 'CPKT_EXTERNAL_ROOT must stay fixed at .cache/deps/<target>\n' >&2
  exit 1
fi

if ! grep -F 'set(_cpkt_default_dependency_build_root "${CMAKE_SOURCE_DIR}/.cache/deps-build/${CPKT_TARGET_ID}")' \
    "$repo_root/CMakeLists.txt" >/dev/null 2>&1; then
  printf 'CPKT_DEPENDENCY_BUILD_ROOT must stay fixed at .cache/deps-build/<target>\n' >&2
  exit 1
fi

if ! grep -F 'cpkt_refresh_repo_dependency_roots_if_stale(' \
    "$repo_root/CMakeLists.txt" >/dev/null 2>&1; then
  printf 'repo-local dependency roots must be explicitly invalidated when their build contract changes\n' >&2
  exit 1
fi

if ! grep -F '.cache/dependency-contracts' \
    "$repo_root/cmake/CpktDependencyContract.cmake" >/dev/null 2>&1; then
  printf 'repo-local dependency invalidation must record a short per-target build contract manifest\n' >&2
  exit 1
fi

if ! grep -F 'file(REMOVE_RECURSE "${_root}")' \
    "$repo_root/cmake/CpktDependencyContract.cmake" >/dev/null 2>&1; then
  printf 'stale repo-local dependency roots must be deleted rather than hidden behind longer path names\n' >&2
  exit 1
fi

if ! grep -F 'cpkt_validate_repo_dependency_root_contract(' \
    "$repo_root/cmake/CpktDependencyContract.cmake" >/dev/null 2>&1; then
  printf 'stale repo-local dependency roots must be rejected when dependency builds are disabled\n' >&2
  exit 1
fi

if ! grep -F 'cpkt_append_lifecycle_owned_dependency_root(' \
    "$repo_root/cmake/CpktDependencyContract.cmake" >/dev/null 2>&1; then
  printf 'repo-local dependency invalidation must operate on actual lifecycle-owned roots\n' >&2
  exit 1
fi

if ! grep -F 'set(_disposable_root_base "${CMAKE_SOURCE_DIR}/.cache/deps")' \
    "$repo_root/cmake/CpktDependencyContract.cmake" >/dev/null 2>&1 ||
    ! grep -F 'set(_disposable_root_base "${CMAKE_SOURCE_DIR}/.cache/deps-build")' \
    "$repo_root/cmake/CpktDependencyContract.cmake" >/dev/null 2>&1 ||
    ! grep -F '_root_abs STREQUAL "${_disposable_root_base_abs}"' \
    "$repo_root/cmake/CpktDependencyContract.cmake" >/dev/null 2>&1; then
  printf 'repo-local dependency invalidation must confine recursive deletes to disposable dependency roots\n' >&2
  exit 1
fi

if grep -F 'STREQUAL "${default_external_root}"' \
    "$repo_root/cmake/CpktDependencyContract.cmake" >/dev/null 2>&1 ||
    grep -F 'STREQUAL "${default_dependency_build_root}"' \
    "$repo_root/cmake/CpktDependencyContract.cmake" >/dev/null 2>&1; then
  printf 'lifecycle-owned dependency-root overrides must not bypass stale-root invalidation\n' >&2
  exit 1
fi

if ! grep -F 'AND CPKT_CALLER_OWNED_DEPENDENCY_ROOTS' \
    "$repo_root/CMakeLists.txt" >/dev/null 2>&1; then
  printf 'cached default dependency roots must not become caller-owned without an explicit marker\n' >&2
  exit 1
fi

if ! grep -F 'OR "${CPKT_EXTERNAL_ROOT}" STREQUAL "${_cpkt_default_external_root}"' \
    "$repo_root/CMakeLists.txt" >/dev/null 2>&1 ||
    ! grep -F 'OR "${CPKT_DEPENDENCY_BUILD_ROOT}" STREQUAL "${_cpkt_default_dependency_build_root}"' \
    "$repo_root/CMakeLists.txt" >/dev/null 2>&1; then
  printf 'cached default dependency roots must not become explicit overrides on reconfigure\n' >&2
  exit 1
fi

if ! grep -F 'build/dependency-cache-disabled-external' "$repo_root/CMakeLists.txt" >/dev/null 2>&1 ||
    ! grep -F 'build/dependency-cache-disabled-build' "$repo_root/CMakeLists.txt" >/dev/null 2>&1; then
  printf 'disabled dependency-cache test must use dedicated caller-owned fixture roots\n' >&2
  exit 1
fi

if sed -n '/NAME dependency_cache_disabled/,/)/p' "$repo_root/CMakeLists.txt" |
    grep -E '\$\{CPKT_(EXTERNAL_ROOT|DEPENDENCY_BUILD_ROOT)\}' >/dev/null 2>&1; then
  printf 'disabled dependency-cache test must not reuse lifecycle-owned configured roots\n' >&2
  exit 1
fi

if ! grep -F 'external_root="$build_dir/deps"' "$repo_root/scripts/configure-preset.sh" >/dev/null 2>&1 ||
    ! grep -F 'dependency_build_root="$build_dir/deps-build"' "$repo_root/scripts/configure-preset.sh" >/dev/null 2>&1 ||
    ! grep -F 'ln -sfn "$debug_external_root" "$external_root"' "$repo_root/scripts/configure-preset.sh" >/dev/null 2>&1; then
  printf 'opcua-fuzz must reuse debug dependencies through explicit non-default caller-owned roots\n' >&2
  exit 1
fi

if grep -F 'if [ ! -f "$debug_cache" ]' "$repo_root/scripts/configure-preset.sh" >/dev/null 2>&1; then
  printf 'opcua-fuzz must not skip debug dependency-contract validation when a debug cache already exists\n' >&2
  exit 1
fi

if ! awk '
  /\[ "\$preset" = "opcua-fuzz" \]/ { in_opcua = 1; next }
  in_opcua && /debug_external_root=\$\(sed/ {
    checked_root_read = 1
    exit saw_debug_reconfigure ? 0 : 1
  }
  in_opcua && /cmake --preset debug/ { saw_debug_reconfigure = 1 }
  END { if (!in_opcua || !checked_root_read) exit 1 }
' "$repo_root/scripts/configure-preset.sh"; then
  printf 'opcua-fuzz must reconfigure debug before reusing debug dependency roots\n' >&2
  exit 1
fi

if ! grep -F '${CPKT_TARGET_ID}-${_root_label}.txt' \
    "$repo_root/cmake/CpktDependencyContract.cmake" >/dev/null 2>&1; then
  printf 'repo-local dependency invalidation must track contracts per default root\n' >&2
  exit 1
fi

if ! grep -F 'CMake will not delete caller-owned dependency build state.' \
    "$repo_root/cmake/CpktDependencyContract.cmake" >/dev/null 2>&1; then
  printf 'repo-local dependency invalidation must reject stale default install roots paired with caller-owned build roots\n' >&2
  exit 1
fi

if grep -F 'file(REMOVE_RECURSE "${CPKT_DEPENDENCY_BUILD_ROOT}")' \
    "$repo_root/CMakeLists.txt" "$repo_root/cmake/CpktDependencyContract.cmake" >/dev/null 2>&1; then
  printf 'repo-local dependency invalidation must never delete caller-owned dependency build roots\n' >&2
  exit 1
fi

if ! grep -F 'foreach(_root_index RANGE 0 ${_last_root_index})' \
    "$repo_root/cmake/CpktDependencyContract.cmake" >/dev/null 2>&1; then
  printf 'repo-local dependency invalidation must refresh default roots independently when the other root is overridden\n' >&2
  exit 1
fi

if ! grep -F 'IS_SYMLINK "${_disposable_root_base_abs}"' \
    "$repo_root/cmake/CpktDependencyContract.cmake" >/dev/null 2>&1 ||
    ! grep -F 'IS_SYMLINK "${_disposable_root_base_component_path}"' \
      "$repo_root/cmake/CpktDependencyContract.cmake" >/dev/null 2>&1 ||
    ! grep -F 'IS_SYMLINK "${_root_component_path}"' \
      "$repo_root/cmake/CpktDependencyContract.cmake" >/dev/null 2>&1 ||
    ! grep -F 'ancestor must not be a symlink' \
      "$repo_root/cmake/CpktDependencyContract.cmake" >/dev/null 2>&1 ||
    ! grep -F 'must not contain symlink components' \
      "$repo_root/cmake/CpktDependencyContract.cmake" >/dev/null 2>&1; then
  printf 'repo-local dependency invalidation must reject symlinked lifecycle-owned roots before recursive deletion\n' >&2
  exit 1
fi

if ! grep -F 'vendor/open62541/patches/*' \
    "$repo_root/cmake/CpktDependencyContract.cmake" >/dev/null 2>&1; then
  printf 'dependency build contract must include vendored dependency patch inputs\n' >&2
  exit 1
fi

contract_block=$(sed -n '/set(_contract_files/,/file(GLOB _contract_open62541_patch_files/p' "$repo_root/cmake/CpktDependencyContract.cmake")

case "$contract_block" in
  *'${CMAKE_SOURCE_DIR}/cmake/*.cmake'*)
  printf 'dependency build contract must not hash every cmake helper file\n' >&2
  exit 1
    ;;
esac

case "$contract_block" in
  *'${CMAKE_SOURCE_DIR}/CMakeLists.txt'*)
    printf 'dependency build contract must not hash top-level CMakeLists.txt\n' >&2
    exit 1
    ;;
esac

for excluded_contract_file in \
    gnu_tar.cmake \
    package_assertions.cmake \
    package_bundle.cmake \
    package_darwin_smoke_libraries.cmake \
    prune_dependency_install_tree.cmake \
    update_checksum_manifest.cmake; do
  case "$contract_block" in
    *"\${CMAKE_SOURCE_DIR}/cmake/$excluded_contract_file"*)
    printf 'dependency build contract must not include packaging-only helper %s\n' "$excluded_contract_file" >&2
    exit 1
      ;;
  esac
done

for included_contract_file in \
    CpktDependencyContract.cmake \
    CpktDependencies.cmake \
    CpktDependencyArchiveCache.cmake \
    apply_patch_series.cmake \
    install_lua.cmake \
    strip_dependency_install_tree.cmake; do
  case "$contract_block" in
    *"\${CMAKE_SOURCE_DIR}/cmake/$included_contract_file"*) ;;
    *)
    printf 'dependency build contract must include dependency helper %s\n' "$included_contract_file" >&2
    exit 1
      ;;
  esac
done

for required_contract_input in \
    CMAKE_GENERATOR \
    CMAKE_MAKE_PROGRAM \
    CMAKE_C_FLAGS \
    CMAKE_CXX_FLAGS \
    CMAKE_EXE_LINKER_FLAGS \
    CMAKE_SHARED_LINKER_FLAGS \
    CMAKE_MODULE_LINKER_FLAGS \
    CMAKE_STATIC_LINKER_FLAGS \
    CPKT_EXTERNAL_ROOT \
    CPKT_DEPENDENCY_BUILD_ROOT; do
  if ! grep -F "$required_contract_input" "$repo_root/cmake/CpktDependencyContract.cmake" >/dev/null 2>&1; then
    printf 'dependency build contract must include %s\n' "$required_contract_input" >&2
    exit 1
  fi
done

if ! grep -F 'clean_one "$repo_root/.cache"' "$repo_root/scripts/clean.sh" >/dev/null 2>&1; then
  printf 'make clean must delete the repo-local dependency cache under .cache\n' >&2
  exit 1
fi

for lifecycle_script in build.sh test.sh package.sh; do
  if grep -F 'bash "$repo_root/scripts/clean.sh" all' "$repo_root/scripts/$lifecycle_script" >/dev/null 2>&1; then
    printf 'scripts/%s must not clean generated state during normal lifecycle work; make release owns the clean final gate\n' "$lifecycle_script" >&2
    exit 1
  fi
done

package_osxcross_line=$(grep -n -F 'scripts/osxcross_available.sh' "$repo_root/scripts/package.sh" | head -n 1 | cut -d: -f1)
package_first_cmake_line=$(grep -n -E '"\$CMAKE" --preset|"\$CMAKE" --build --preset' "$repo_root/scripts/package.sh" | head -n 1 | cut -d: -f1)
if [ -z "$package_osxcross_line" ] || [ -z "$package_first_cmake_line" ] ||
    [ "$package_osxcross_line" -ge "$package_first_cmake_line" ]; then
  printf 'scripts/package.sh must prove required osxcross availability before package configure/build work\n' >&2
  exit 1
fi

if ! grep -F 'arm64-apple-darwin-release is required for c.pkt.systems releases' \
    "$repo_root/scripts/package.sh" >/dev/null 2>&1; then
  printf 'scripts/package.sh must keep Darwin/osxcross mandatory for releases\n' >&2
  exit 1
fi

if grep -A3 '^debug:' "$repo_root/Makefile" | grep -F '$(MAKE) clean' >/dev/null 2>&1; then
  printf 'make debug must not clean generated state; make release owns the clean final gate\n' >&2
  exit 1
fi

if ! awk '
  $0 == "release:" { found = 1; next }
  found && /^[^[:space:]#]/ { exit }
  found && /^\t\$\(MAKE\) lifecycle-version-contract$/ { saw_contract = 1; next }
  found && saw_contract && /^\t\$\(MAKE\) clean$/ { saw_clean = 1; next }
  found && saw_clean && /^\t\$\(MAKE\) release-pipeline$/ { saw_pipeline = 1 }
  END { exit saw_pipeline ? 0 : 1 }
' "$repo_root/Makefile"; then
  printf 'make release must run lifecycle-version-contract, then clean, then release-pipeline\n' >&2
  exit 1
fi

if awk '
  $0 == "prerelease:" || $0 == "release-pipeline:" || $0 == "release-matrix:" { found = 1; target = $0; next }
  found && /^[^[:space:]#]/ { found = 0 }
  found && /\$\(MAKE\) clean/ { print target; bad = 1 }
  END { exit bad ? 0 : 1 }
' "$repo_root/Makefile"; then
  printf 'prerelease, release-pipeline, and release-matrix must not run make clean\n' >&2
  exit 1
fi

if grep -R -n -E '\\.cache/(deps|deps-build)/\\$\\{CPKT_TARGET_ID\\}/[^"]*/[^"]*' \
    "$repo_root/CMakeLists.txt" "$repo_root/cmake" "$repo_root/scripts" "$repo_root/tests"; then
  printf 'repo-local dependency roots must not add cache-id path components after <target>\n' >&2
  exit 1
fi

if grep -n -F '`make clean`, `make prerelease`, `make release`' \
    "$repo_root/skills/pkt-systems-cmake-lifecycle/references/dependencies.md"; then
  printf 'lifecycle skill must not claim prerelease deletes repo-local dependency state\n' >&2
  exit 1
fi

printf 'repo dependency cache policy passed\n'
