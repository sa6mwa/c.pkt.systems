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

for target in \
  help deps-debug deps-release deps-cross build build-debug build-release \
  build-host cross-build test test-debug test-host test-cross cross-test test-all \
  test-install-tree valgrind fuzz-smoke fuzz fuzz-long package package-source \
  package-source-smoke package-checksums package-verify verify-release-archives \
  verify-release-privacy release-matrix finalize-slice prerelease prerelease-live \
  prerelease-hardening release print-release-version format clean clean-dist; do
  require_help_target "$target"
done

for script in \
  scripts/build.sh \
  scripts/cpkt-toolchains.sh \
  scripts/cpkt-aflpp.sh \
  scripts/configure-preset.sh \
  scripts/fuzz.sh \
  scripts/test.sh \
  scripts/package.sh \
  scripts/run_linux_release_matrix.sh \
  scripts/clean.sh \
  scripts/release-version.sh \
  scripts/package-source.sh \
  scripts/source-archive-verify.sh \
  scripts/package-verify.sh; do
  require_script "$script"
done

require_file_contains \
  CMakePresets.json \
  'cmake/toolchains/x86_64-linux-gnu.cmake' \
  'host Linux presets select the pinned Bootlin collection'
require_file_contains \
  CMakePresets.json \
  'cmake/toolchains/aflpp-x86_64-linux-gnu.cmake' \
  'fuzz presets select pinned AFL++ GCC instrumentation'
if grep -Eq '"CMAKE_C_COMPILER"[[:space:]]*:[[:space:]]*"clang"' "$repo_root/CMakePresets.json"; then
  printf 'sanitizer presets must not select host clang\n' >&2
  exit 1
fi
require_file_contains \
  README.md \
  'Host GCC, Clang, and binutils are never Linux build fallbacks' \
  'documented pinned Linux toolchain policy'

grep -Eq '^/dist/$' "$repo_root/.gitignore"
grep -Eq '^/VERSION$' "$repo_root/.gitignore"
grep -Eq '^release-matrix:.*package-checksums.*package-verify' "$repo_root/Makefile"
grep -Eq '^release-pipeline:.*format.*debug.*clangd-surface.*valgrind.*fuzz-smoke.*release-matrix' "$repo_root/Makefile" || {
  printf 'release-pipeline must run ordinary checks before the release matrix\n' >&2
  exit 1
}
grep -Eq 'if\(CPKT_TARGET_ID STREQUAL "x86_64-linux-gnu"\)' "$repo_root/CMakeLists.txt" || {
  printf 'clangd CTest registration must be restricted to the native host target\n' >&2
  exit 1
}
grep -Eq '^prerelease:[[:space:]]+release-pipeline[[:space:]]*$' "$repo_root/Makefile" || {
  printf 'prerelease must invoke the shared release-pipeline\n' >&2
  exit 1
}
grep -Eq '^release:[[:space:]]+clean[[:space:]]+release-pipeline[[:space:]]*$' "$repo_root/Makefile" || {
  printf 'release must clean before invoking the shared release-pipeline\n' >&2
  exit 1
}
grep -Eq '^prerelease-hardening:[[:space:]]+prerelease[[:space:]]+fuzz[[:space:]]*$' "$repo_root/Makefile" || {
  printf 'prerelease-hardening must extend prerelease with standard native fuzzing\n' >&2
  exit 1
}
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
require_file_contains \
  scripts/package.sh \
  'x86_64-linux-gnu-release x86_64-linux-musl-release aarch64-linux-gnu-release aarch64-linux-musl-release armhf-linux-gnu-release armhf-linux-musl-release' \
  'full Linux release preset matrix'
require_file_contains \
  scripts/package.sh \
  'package-arm64-apple-darwin-release' \
  'required arm64 Darwin package target'
require_file_contains \
  scripts/package.sh \
  'arm64-apple-darwin-release is required for c\.pkt\.systems releases' \
  'Darwin package prerequisite fails closed'
require_file_contains \
  scripts/package-verify.sh \
  'x86_64-linux-gnu x86_64-linux-musl aarch64-linux-gnu aarch64-linux-musl armhf-linux-gnu armhf-linux-musl arm64-apple-darwin' \
  'full Linux and Darwin package verification matrix'
require_file_contains \
  scripts/package-verify.sh \
  'arm64-apple-darwin package verification requires a complete local osxcross SDK toolchain' \
  'Darwin package verification prerequisite fails closed'
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
require_file_contains \
  scripts/fuzz.sh \
  '\$cmake" --build --preset debug --target cpkt_opcua_static' \
  'fuzz gates prepare the normal OPC UA facade dependency prerequisite before AFL++ fuzzing'
require_file_contains \
  scripts/configure-preset.sh \
  '-DCPKT_EXTERNAL_ROOT="\$external_root"' \
  'OPC UA fuzz configure reuses the debug dependency install root'
require_file_contains \
  CMakeLists.txt \
  'CPKT_ALLOW_DEPENDENCY_ROOT_OVERRIDE' \
  'explicit dependency root override is required before reusing a non-default dependency cache'
require_file_contains \
  scripts/configure-preset.sh \
  '\.cache/deps-build/x86_64-linux-gnu/\*/AFL_' \
  'fresh fuzz configure removes generated AFL dependency build caches'
require_file_contains \
  scripts/configure-preset.sh \
  '\.cache/deps/x86_64-linux-gnu/\*/AFL_' \
  'fresh fuzz configure removes generated AFL dependency install caches'
require_file_contains \
  scripts/package-install-smoke.sh \
  'cpkt_add_static_archive_pic_smoke' \
  'install-tree static archive PIC smoke'
require_file_contains \
  scripts/package-install-smoke.sh \
  'target_command_env=\("LD_LIBRARY_PATH=\$osxcross_root/lib\$\{LD_LIBRARY_PATH:\+:\$LD_LIBRARY_PATH\}"\)' \
  'Darwin package target-tool commands receive the osxcross linker runtime environment'
require_file_contains \
  scripts/package-install-smoke.sh \
  'cpkt_cmake_build_checked "cmake aggregate consumer build" "\$cmake_build_dir"' \
  'install-tree aggregate CMake consumer build uses the lifecycle build launcher'
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
  '-DENABLE_THREADED_RESOLVER=OFF' \
  'Darwin curl cross builds avoid threaded resolver target-thread probes'
