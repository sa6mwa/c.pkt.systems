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

python3 "$source_dir/tests/sus_split_runtime_policy_test.py" "$source_dir"

require_file_contains "$toolchains" \
  'Do not merge GNU runtime archives into facade archives.' \
  'lifecycle toolchain policy must forbid merging libstdc++.a into facades'
require_file_contains "$toolchains" \
  'Ship the selected runtime archives in the SDK.' \
  'lifecycle toolchain policy must require shipping selected runtime archives'
require_file_contains "$toolchains" \
  'Make static metadata place facade archives before the selected runtime archives.' \
  'lifecycle toolchain policy must require metadata-owned runtime closure'
