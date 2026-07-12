#!/usr/bin/env sh
set -eu

if [ "$#" -ne 1 ]; then
  printf 'usage: sus_static_runtime_policy_test.sh <source-dir>\n' >&2
  exit 2
fi

source_dir=$1
top_cmake=$source_dir/CMakeLists.txt
bundle=$source_dir/cmake/package_bundle.cmake
assertions=$source_dir/cmake/package_assertions.cmake
smoke=$source_dir/scripts/package-install-smoke.sh
toolchains=$source_dir/skills/pkt-systems-cmake-lifecycle/references/toolchains.md

require_file_contains() {
  file=$1
  needle=$2
  description=$3

  if ! grep -F -- "$needle" "$file" >/dev/null 2>&1; then
    printf 'missing sus static-runtime policy: %s\n' "$description" >&2
    printf 'expected to find: %s\n' "$needle" >&2
    printf 'in: %s\n' "$file" >&2
    exit 1
  fi
}

require_file_lacks() {
  file=$1
  needle=$2
  description=$3

  if grep -F -- "$needle" "$file" >/dev/null 2>&1; then
    printf 'forbidden sus static-runtime policy drift: %s\n' "$description" >&2
    printf 'unexpected text: %s\n' "$needle" >&2
    printf 'in: %s\n' "$file" >&2
    exit 1
  fi
}

require_file_contains "$top_cmake" \
  'COMMAND "${CMAKE_CXX_COMPILER}" -print-file-name=libstdc++.a' \
  'top-level configure must discover static libstdc++.a from the selected C++ compiler'
require_file_contains "$top_cmake" \
  'COMMAND "${CMAKE_CXX_COMPILER}" -print-libgcc-file-name' \
  'top-level configure must discover static libgcc.a from the selected C++ compiler'

require_file_contains "$bundle" \
  'lib/cpkt-cxx/libstdc++.a' \
  'binary SDK must stage cpkt-provided libstdc++.a'
require_file_contains "$bundle" \
  'lib/cpkt-cxx/libgcc.a' \
  'binary SDK must stage cpkt-provided libgcc.a'
require_file_contains "$bundle" \
  '_cpkt_cxx_stdlib_static' \
  'CMake package metadata must resolve packaged libstdc++.a'
require_file_contains "$bundle" \
  '_cpkt_cxx_libgcc_static' \
  'CMake package metadata must resolve packaged libgcc.a'
require_file_contains "$bundle" \
  '\${_cpkt_whisper_prefix}/lib/cpkt-cxx/libstdc++.a' \
  'CMake package metadata must point libstdc++.a at the packaged prefix'
require_file_contains "$bundle" \
  '\${_cpkt_whisper_prefix}/lib/cpkt-cxx/libgcc.a' \
  'CMake package metadata must point libgcc.a at the packaged prefix'
require_file_contains "$bundle" \
  '\${_cpkt_cxx_stdlib_static};\${_cpkt_cxx_libgcc_static}' \
  'CMake imported targets must place packaged C++ runtime archives in the link closure'
require_file_contains "$bundle" \
  'Libs.private: -lggml -lggml-base -lggml-cpu ${_cpkt_cxx_stdlib_static_pc_lib} ${_cpkt_cxx_libgcc_static_pc_lib} ${_cpkt_whisper_static_cxx_runtime_pc_libs} -lm -pthread' \
  'pkg-config static metadata must emit packaged C++ runtime archives'

require_file_contains "$assertions" \
  'lib/cpkt-cxx/libstdc\\+\\+\\.a' \
  'package assertions must require packaged libstdc++.a'
require_file_contains "$assertions" \
  'lib/cpkt-cxx/libgcc\\.a' \
  'package assertions must require packaged libgcc.a'
require_file_contains "$assertions" \
  'whisper metadata does not reference packaged libstdc++.a' \
  'package assertions must reject missing libstdc++.a metadata'
require_file_contains "$assertions" \
  'whisper metadata does not reference packaged libgcc.a' \
  'package assertions must reject missing libgcc.a metadata'
require_file_contains "$assertions" \
  'libstdc\\+\\+\\.so[^]]*' \
  'shared cpktsus verification must reject downstream libstdc++.so requirements'
require_file_contains "$assertions" \
  'libgcc_s\\.so[^]]*' \
  'shared cpktsus verification must reject downstream libgcc_s.so requirements'

require_file_contains "$smoke" \
  'assert_file_contains "$cmake_link_dir/cpkt_cmake_sus_facade.dir/link.txt" "$prefix/lib/cpkt-cxx/libstdc++.a"' \
  'CMake consumer smoke must verify cpkt::sus links packaged libstdc++.a'
require_file_contains "$smoke" \
  'assert_file_contains "$cmake_link_dir/cpkt_cmake_sus_facade.dir/link.txt" "$prefix/lib/cpkt-cxx/libgcc.a"' \
  'CMake consumer smoke must verify cpkt::sus links packaged libgcc.a'
require_file_contains "$smoke" \
  'assert_words_contain "$sus_words" "$pkg_config_libdir/../../lib/cpkt-cxx/libstdc++.a"' \
  'pkg-config smoke must verify cpkt-sus.pc emits packaged libstdc++.a'
require_file_contains "$smoke" \
  'assert_words_contain "$sus_words" "$pkg_config_libdir/../../lib/cpkt-cxx/libgcc.a"' \
  'pkg-config smoke must verify cpkt-sus.pc emits packaged libgcc.a'
require_file_contains "$smoke" \
  'cpkt_pkg_config_static_smoke cpkt-sus cpkt_sus_facade_strict.c' \
  'C-only static pkg-config consumer must be smoke-tested'
require_file_contains "$smoke" \
  'cpkt_pkg_config_static_mixed_cxx_smoke' \
  'mixed C/C++ static pkg-config consumer must be smoke-tested'
require_file_contains "$smoke" \
  'pkg-config static cpkt-sus mixed C-final link' \
  'mixed C/C++ final link must be performed by the configured C compiler'
require_file_contains "$smoke" \
  '\"$cc\" $pkg_config_static_flag $pkg_config_link_toolchain_flags' \
  'package smoke final static links must use cc, not g++'
require_file_lacks "$smoke" \
  '"$cxx" $pkg_config_static_flag' \
  'package smoke must not use c++ driver for final static links'

require_file_contains "$toolchains" \
  'Do not merge GNU runtime archives into facade archives.' \
  'lifecycle toolchain policy must forbid merging libstdc++.a into facades'
require_file_contains "$toolchains" \
  'Ship the selected runtime archives in the SDK.' \
  'lifecycle toolchain policy must require shipping selected runtime archives'
require_file_contains "$toolchains" \
  'Make static metadata place facade archives before the selected runtime archives.' \
  'lifecycle toolchain policy must require metadata-owned runtime closure'
