#!/usr/bin/env bash
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

help_output=$(make -C "$repo_root" help)

require_help_target() {
  target=$1
  if ! printf '%s\n' "$help_output" | grep -Eq "^[[:space:]]+$target[[:space:]]"; then
    printf 'make help does not list lifecycle target: %s\n' "$target" >&2
    exit 1
  fi
}

require_script() {
  path=$1
  if [ ! -x "$repo_root/$path" ]; then
    printf 'missing executable lifecycle script: %s\n' "$path" >&2
    exit 1
  fi
  bash -n "$repo_root/$path"
}

require_file_contains() {
  path=$1
  pattern=$2
  description=$3
  if ! grep -Eq -- "$pattern" "$repo_root/$path"; then
    printf '%s does not contain required lifecycle contract: %s\n' "$path" "$description" >&2
    exit 1
  fi
}

require_ordered_make_recipe() {
  target=$1
  expected=$2
  python3 "$repo_root/tests/lifecycle_recipe_contract.py" "$target" "$expected"

}

for target in \
  help deps deps-all deps-debug deps-release deps-cross build build-debug build-release \
  build-host cross-build test test-debug test-host test-cross cross-test test-all test-e2e \
  test-install-tree valgrind fuzz-smoke fuzz fuzz-long package package-source \
  package-source-smoke package-checksums package-verify verify-release-archives \
  verify-release-privacy release-matrix release-final-matrix finalize-slice prerelease prerelease-live \
  prerelease-hardening lifecycle-version-contract release print-release-version \
  format format-check dev-up dev-down dev-ps dev-logs dev-reset clean clean-dist; do
  require_help_target "$target"
done

for script in \
  scripts/build.sh \
  scripts/cpkt-toolchains.sh \
  scripts/cpkt-aflpp.sh \
  scripts/configure-preset.sh \
  scripts/fuzz.sh \
  scripts/test.sh \
  scripts/devenv.sh \
  scripts/test-e2e.sh \
  scripts/package.sh \
  scripts/run_linux_release_matrix.sh \
  scripts/clean.sh \
  scripts/release-version.sh \
  scripts/package-source.sh \
  scripts/source-archive-verify.sh \
  scripts/package-verify.sh \
  scripts/run-package-consumers.sh \
  tests/release_version_contract_test.sh; do
  require_script "$script"
done

require_file_contains \
  CMakePresets.json \
  'cmake/CpktReadOnlyToolchain.cmake' \
  'host Linux presets select the pinned Bootlin collection'
require_file_contains \
  CMakeLists.txt \
  'CPKT_DEPENDENCY_CACHE' \
  'CMake exposes the shared dependency archive cache'
require_file_contains \
  cmake/CpktDependencyArchiveCache.cmake \
  'file\(LOCK' \
  'dependency archives use per-digest shared-cache locks'
require_file_contains \
  cmake/CpktDependencyArchiveCache.cmake \
  'file\(RENAME' \
  'dependency archives publish by atomic rename'
require_file_contains \
  CMakePresets.json \
  'cmake/CpktReadOnlyAflToolchain.cmake' \
  'fuzz presets select pinned AFL++ GCC instrumentation'
if grep -Eq '"CMAKE_C_COMPILER"[[:space:]]*:[[:space:]]*"clang"' "$repo_root/CMakePresets.json"; then
  printf 'sanitizer presets must not select host clang\n' >&2
  exit 1
fi
require_file_contains \
  README.md \
  'GCC, Clang, and binutils are never Linux build fallbacks' \
  'documented pinned Linux toolchain policy'

grep -Eq '^/dist/$' "$repo_root/.gitignore"
grep -Eq '^/VERSION$' "$repo_root/.gitignore"
require_ordered_make_recipe \
  release-pipeline \
  'format
format-check
debug
e2e-postgres
clangd-surface
valgrind
fuzz-smoke
release-matrix'
require_ordered_make_recipe finalize-slice 'format
debug
clangd-surface
format-check'
require_ordered_make_recipe \
  release-matrix \
  'package
package-checksums
package-verify'
require_ordered_make_recipe \
  release-final-matrix \
  'package
package-source-smoke
package-checksums
package-verify'
grep -Eq 'if\(CPKT_TARGET_ID STREQUAL "x86_64-linux-gnu"\)' "$repo_root/CMakeLists.txt" || {
  printf 'clangd CTest registration must be restricted to the native host target\n' >&2
  exit 1
}
require_ordered_make_recipe prerelease 'release-pipeline'
require_ordered_make_recipe release 'lifecycle-version-contract
clean
format
format-check
debug
e2e-postgres
clangd-surface
valgrind
fuzz-smoke
release-final-matrix'
require_ordered_make_recipe prerelease-hardening 'prerelease
fuzz'
require_file_contains \
  scripts/run-afl-fuzz.sh \
  'smoke\|standard\|long' \
  'AFL++ runner supports the long fuzz mode'
if make -C "$repo_root" prerelease-live >/dev/null 2>&1; then
  printf 'prerelease-live must refuse external-provider checks without CPKT_LIVE_CHECKS=1\n' >&2
  exit 1
fi
if make -C "$repo_root" fuzz-long >/dev/null 2>&1; then
  printf 'fuzz-long must require CPKT_FUZZ_LONG_ENABLE=1\n' >&2
  exit 1
fi
python3 "$repo_root/tests/package_integration_contract_test.py" Fixtures.test_release_rejects_narrowing_before_clean Fixtures.test_exact_checksum_scopes
python3 "$repo_root/tests/github_actions_contract_test.py"
require_file_contains \
  CMakeLists.txt \
  'static_archive_pic_link' \
  'build-tree static archive PIC smoke'
require_file_contains \
  CMakeLists.txt \
  'RULE_LAUNCH_LINK' \
  'Darwin repo-owned link steps run with osxcross linker runtime environment'
require_file_contains \
  CMakeLists.txt \
  'NAME package_install_smoke_args' \
  'package install smoke argument policy runs under CTest prerelease coverage'
require_file_contains \
  CMakeLists.txt \
  'NAME mqttc_linker_flags' \
  'mqtt-c linker metadata policy runs under CTest prerelease coverage'
require_file_contains \
  CMakePresets.json \
  '"CPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE": "ON"' \
  'OPC UA facade fuzzer explicitly opts into normal dependency tree reuse'
require_file_contains \
  CMakePresets.json \
  '"CPKT_BUILD_DEPENDENCIES": "OFF"' \
  'fuzz presets do not build third-party dependency trees'
require_file_contains scripts/group-build.py 'CPKT_BORROW_ORDINARY_DEPENDENCIES=ON' 'fuzz imports ordinary dependencies without producers'
require_file_contains scripts/group-build.py 'validate_core' 'fuzz requires verified ordinary core'
require_file_contains \
  CMakeLists.txt \
  'CPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE' \
  'explicit dependency root override is required before reusing a non-default dependency cache'
require_file_contains \
  skills/pkt-systems-cmake-lifecycle/references/dependencies.md \
  'These roots are disposable, but deletion stays within the operation.s owned scope; borrowed prerequisites are read-only' \
  'dependency lifecycle keeps repo-local roots target-only and disposable'
require_file_contains \
  skills/pkt-systems-cmake-lifecycle/references/dependencies.md \
  'Normal no-clean entrypoints use explicit stale-component detection' \
  'dependency lifecycle documents no-clean component invalidation'
require_file_contains \
  skills/pkt-systems-cmake-lifecycle/references/toolchains.md \
  'locks/.*per-collection advisory lock files' \
  'lifecycle documents serialized shared toolchain publication'
require_script \
  skills/pkt-systems-cmake-lifecycle/scripts/test-release-tag-contract.sh
"$repo_root/skills/pkt-systems-cmake-lifecycle/scripts/test-release-tag-contract.sh" >/dev/null
require_file_contains \
  skills/pkt-systems-cmake-lifecycle/references/release.md \
  '`make release` is the only standard release-flow target that runs this check, and it must run it before `clean`, `release-pipeline`, `release-matrix`, `package-verify`, checksum generation, or artifact production\.' \
  'lifecycle runs tag-mutating checks first in make release'
require_file_contains \
  skills/pkt-systems-cmake-lifecycle/references/release.md \
  'Release orchestration belongs to Make, not CMake\.' \
  'release orchestration is Make-owned'
require_file_contains \
  skills/pkt-systems-cmake-lifecycle/references/local-ci.md \
  '`make release` must run that target first, before `clean`, build, package, checksum, or artifact work; `test`, `test-all`, `prerelease`, `release-pipeline`, `release-matrix`, `package-verify`, and other late release-flow commands must not run it\.' \
  'local CI places tag-mutating checks first in release only'
require_file_contains \
  skills/pkt-systems-cmake-lifecycle/references/local-ci.md \
  'the pre-clean tag-mutating contract is not a CMake pipeline and should not configure CMake solely to validate tag mutation' \
  'pre-clean tag contract is not a CMake pipeline'
require_file_contains \
  skills/pkt-systems-cmake-lifecycle/references/release.md \
  'Fail on a pre-existing reserved tag unless a lifecycle-owned recovery record under `build/` identifies the exact lightweight object created by this test\.' \
  'reserved tag recovery requires recorded ownership'
require_file_contains \
  skills/pkt-systems-cmake-lifecycle/references/release.md \
  'automatic recovery and trap cleanup must use compare-and-delete against the recorded object and preserve any changed or unowned ref\.' \
  'reserved tag cleanup preserves changed and unowned refs'
require_file_contains \
  skills/pkt-systems-cmake-lifecycle/references/release.md \
  'git -c tag\.gpgSign=false tag v99\.99\.99' \
  'reserved temp tag creation disables signing'
require_file_contains \
  skills/pkt-systems-cmake-lifecycle/references/release.md \
  'annotated or signed tag objects must not satisfy the release contract or produce a release version from shared version resolver surfaces' \
  'shared resolver rejects annotated and signed release tags'
require_file_contains \
  scripts/release-version.sh \
  'candidate_type=\$\(git -C "\$repo_root" cat-file -t "\$candidate_tag"\)' \
  'shared version resolver checks tag object type'
python3 "$repo_root/tests/package_smoke_contract_test.py"
require_file_contains \
  cmake/CpktDependencies.cmake \
  'LD_LIBRARY_PATH=\$\{CPKT_OSXCROSS_ROOT\}/lib:\$ENV\{LD_LIBRARY_PATH\}' \
  'Darwin dependency commands expose osxcross runtime libraries to target tools'
require_file_contains \
  cmake/CpktDependencies.cmake \
  'string\(APPEND _flags " -include stdint.h -include sys/types.h"\)' \
  'Darwin external dependency compile flags include SDK and fixed-width integer definitions'
require_file_contains \
  cmake/CpktDependencies.cmake \
  'function\(cpkt_get_external_cmake_step_commands build_out_var install_out_var\)' \
  'CMake-driven dependency build steps share a lifecycle wrapper'
require_file_contains \
  cmake/CpktDependencies.cmake \
  'function\(cpkt_get_external_cmake_configure_command out_var\)' \
  'CMake-driven dependency configure steps can share a lifecycle wrapper'
require_file_contains \
  cmake/CpktDependencies.cmake \
  'PKG_CONFIG_LIBDIR=\$\{_pkg_config_libdir\}' \
  'cross dependency configure steps isolate pkg-config metadata to bundled dependency prefixes'
require_file_contains \
  cmake/CpktDependencies.cmake \
  'CONFIGURE_COMMAND \$\{cmake_configure_command\}' \
  'libxml2 configure uses the wrapped CMake configure command'
require_file_contains \
  cmake/CpktDependencies.cmake \
  '-DBUILD_SHARED_LIBS=OFF' \
  'libxml2 static configure remains static-only'
require_file_contains \
  cmake/CpktDependencies.cmake \
  '-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY' \
  'cross CMake dependency configure checks do not require target linker execution'
require_file_contains \
  cmake/CpktDependencies.cmake \
  'CONFIGURE_COMMAND \$\{cmake_configure_command\} \$\{curl_cmake_args\}' \
  'curl configure runs with osxcross environment for CMake feature probes'
require_file_contains \
  cmake/CpktDependencies.cmake \
  'set\(_build_command \$\{CMAKE_COMMAND\} -E env \$\{_env_args\} \$\{_build_command\}\)' \
  'Darwin CMake-driven dependency build commands run with osxcross environment'
require_file_contains \
  cmake/CpktDependencies.cmake \
  'COMMAND \$\{CMAKE_COMMAND\} -E env \$\{mqttc_env_args\}' \
  'Darwin mqtt-c dependency compile and link commands run with osxcross environment'
require_file_contains \
  cmake/CpktDependencies.cmake \
  '\$\{CMAKE_COMMAND\} -E env \$\{lua_env_args\} MAKEFLAGS= make' \
  'Darwin Lua dependency build and link commands run with osxcross environment'
require_file_contains \
  cmake/CpktDependencies.cmake \
  'string\(REPLACE " -include stdint.h -include sys/types.h" "" openssl_cflags "\$\{openssl_cflags\}"\)' \
  'OpenSSL Darwin builds do not pass forced SDK includes through assembly CFLAGS'
require_file_contains \
  cmake/CpktDependencies.cmake \
  '\$\{CMAKE_COMMAND\} -E env \$\{openssl_env_args\} \$\{build_command\}' \
  'OpenSSL build commands run with osxcross environment'
require_file_contains \
  cmake/CpktDependencies.cmake \
  '\$\{CMAKE_COMMAND\} -E env \$\{nghttp2_env_args\} make -C lib' \
  'nghttp2 build commands run with osxcross environment'
require_file_contains \
  cmake/CpktDependencies.cmake \
  'COMMAND \$\{CMAKE_COMMAND\} -E env \$\{miniaudio_env_args\}' \
  'miniaudio manual compiler and linker commands run with osxcross environment'
require_file_contains \
  cmake/CpktDependencies.cmake \
  '-DENABLE_THREADED_RESOLVER=ON' \
  'curl builds enable threaded asynchronous DNS'
