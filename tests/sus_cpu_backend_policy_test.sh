#!/usr/bin/env bash
set -euo pipefail

repo_root=${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
top_cmake="$repo_root/CMakeLists.txt"
deps_cmake="$repo_root/cmake/CpktDependencies.cmake"
package_bundle="$repo_root/cmake/package_bundle.cmake"

require_file_contains() {
  file_path=$1
  expected=$2
  description=$3

  if ! grep -F -- "$expected" "$file_path" >/dev/null 2>&1; then
    printf '%s\nmissing: %s\nin: %s\n' "$description" "$expected" "$file_path" >&2
    exit 1
  fi
}

require_file_contains \
  "$top_cmake" \
  'option(CPKT_SUS_CPU_ONLY "Build the cpkt_sus whisper.cpp dependency set with CPU backends only." ON)' \
  'cpkt_sus CPU-only build option must default ON'

require_file_contains \
  "$top_cmake" \
  'message(FATAL_ERROR "CPKT_SUS_CPU_ONLY=OFF is reserved for a later GPU-backend packaging phase")' \
  'top-level configure must reject GPU backend builds until packaging policy exists'

require_file_contains \
  "$top_cmake" \
  'set(CPKT_SUS_BACKEND_CAPABILITIES "cpu" CACHE INTERNAL "Compiled cpkt_sus whisper.cpp backend capability list.")' \
  'cpkt_sus backend capability metadata must report cpu for the first implementation'

require_file_contains \
  "$deps_cmake" \
  'message(FATAL_ERROR "cpkt_sus currently supports only CPU-only whisper.cpp dependency builds")' \
  'dependency build must reject non-CPU-only whisper.cpp builds'

for cmake_arg in \
    '-DWHISPER_BUILD_TESTS=OFF' \
    '-DWHISPER_BUILD_EXAMPLES=OFF' \
    '-DWHISPER_BUILD_SERVER=OFF' \
    '-DWHISPER_CURL=OFF' \
    '-DWHISPER_SDL2=OFF' \
    '-DWHISPER_COREML=OFF' \
    '-DWHISPER_COREML_ALLOW_FALLBACK=OFF' \
    '-DWHISPER_OPENVINO=OFF' \
    '-DGGML_NATIVE=OFF' \
    '-DGGML_OPENMP=OFF' \
    '-DGGML_METAL=OFF' \
    '-DGGML_BLAS=OFF' \
    '-DGGML_ACCELERATE=OFF' \
    '-DGGML_CUDA=OFF' \
    '-DGGML_HIP=OFF' \
    '-DGGML_VULKAN=OFF' \
    '-DGGML_OPENCL=OFF' \
    '-DGGML_SYCL=OFF' \
    '-DGGML_RPC=OFF' \
    '-DGGML_BACKEND_DL=OFF' \
    '-DGGML_CPU_ALL_VARIANTS=OFF'; do
  require_file_contains \
    "$deps_cmake" \
    "$cmake_arg" \
    "whisper.cpp dependency build must keep first-release CPU-only flag: $cmake_arg"
done

require_file_contains \
  "$package_bundle" \
  '"sus_backend_capabilities=${CPKT_SUS_BACKEND_CAPABILITIES}\n"' \
  'package manifest must record compiled cpkt_sus backend capabilities'
