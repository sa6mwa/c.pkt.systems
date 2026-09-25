#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
  printf 'usage: %s <arm64-apple-darwin SDK archive>\n' "$0" >&2
  exit 2
fi
if [ "$(uname -s)" != Darwin ]; then
  printf 'Darwin SDK runtime verification requires macOS\n' >&2
  exit 2
fi
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
archive=$1
case "$archive" in
  /*) ;;
  *) archive="$(pwd)/$archive" ;;
esac
work_root="$repo_root/build/darwin-curl-package-test"
rm -rf "$work_root"
mkdir -p "$work_root/extracted" "$work_root/consumer"
(cd "$work_root/extracted" && cmake -E tar xf "$archive")
prefix_count=0
prefix=
for candidate in "$work_root/extracted"/*; do
  [ -d "$candidate" ] || continue
  prefix_count=$((prefix_count + 1))
  prefix=$candidate
done
if [ "$prefix_count" -ne 1 ] || [ ! -f "$prefix/lib/libcurl.a" ] ||
    [ ! -f "$prefix/lib/libcurl.dylib" ]; then
  printf 'SDK archive has no unique prefix with static and shared libcurl\n' >&2
  exit 1
fi
cat > "$work_root/consumer/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.21)
project(cpkt_darwin_curl_package_test LANGUAGES C)
include(CTest)
find_package(Threads REQUIRED)
find_package(CURL CONFIG REQUIRED)
foreach(linkage IN ITEMS static shared)
  add_executable(curl_async_dns_${linkage} "${CPKT_SOURCE_DIR}/tests/curl_async_dns_test.c")
  target_compile_options(curl_async_dns_${linkage} PRIVATE
    -std=c99 -Wall -Wextra -Wpedantic -Werror)
  if(linkage STREQUAL "static")
    target_link_libraries(curl_async_dns_${linkage} PRIVATE CURL::libcurl)
  else()
    target_include_directories(curl_async_dns_${linkage} PRIVATE "${CPKT_SDK_PREFIX}/include")
    target_link_libraries(curl_async_dns_${linkage} PRIVATE
      "${CPKT_SDK_PREFIX}/lib/libcurl.dylib")
    set_target_properties(curl_async_dns_${linkage} PROPERTIES
      BUILD_RPATH "${CPKT_SDK_PREFIX}/lib")
  endif()
  add_test(NAME curl_async_dns_${linkage} COMMAND curl_async_dns_${linkage})
  add_test(NAME curl_multi_socket_${linkage}
    COMMAND "${CPKT_PYTHON3}" "${CPKT_SOURCE_DIR}/tests/curl_async_dns_server.py"
      "$<TARGET_FILE:curl_async_dns_${linkage}>")
  set_tests_properties(curl_async_dns_${linkage} curl_multi_socket_${linkage}
    PROPERTIES TIMEOUT 30)
endforeach()
EOF
cmake -S "$work_root/consumer" -B "$work_root/consumer-build" -G Ninja \
  "-DCMAKE_PREFIX_PATH=$prefix" \
  "-DCPKT_SDK_PREFIX=$prefix" \
  "-DCPKT_SOURCE_DIR=$repo_root" \
  "-DCPKT_PYTHON3=$(command -v python3)"
cmake --build "$work_root/consumer-build"
ctest --test-dir "$work_root/consumer-build" --output-on-failure
