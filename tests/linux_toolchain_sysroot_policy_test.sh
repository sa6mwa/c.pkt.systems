#!/usr/bin/env sh
set -eu

if [ "$#" -ne 1 ]; then
  printf 'usage: linux_toolchain_sysroot_policy_test.sh <source-dir>\n' >&2
  exit 2
fi

source_dir=$1
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

cmake_script=$work_dir/check.cmake
cat > "$cmake_script" <<EOF
include("${source_dir}/cmake/toolchains/cpkt_linux_toolchain_common.cmake")

function(make_fake_toolchain root prefix sysroot)
  file(MAKE_DIRECTORY "\${root}/bin" "\${sysroot}/include")
  foreach(tool gcc g++ ar ranlib strip readelf)
    file(WRITE "\${root}/bin/\${prefix}-\${tool}" "")
  endforeach()
  file(WRITE "\${sysroot}/include/stdio.h" "")
endfunction()

set(fake_root "${work_dir}/fake-root")
set(fake_sysroot "${work_dir}/fake-sysroot")
make_fake_toolchain("\${fake_root}" aarch64-linux-gnu "\${fake_sysroot}")
cpkt_select_linux_toolchain(
  aarch64-linux-gnu
  "\${fake_root}"
  aarch64-linux-gnu
  "\${fake_sysroot}"
  aarch64-linux
  aarch64-buildroot-linux-gnu/sysroot
  selected_root
  selected_prefix
  selected_sysroot
  selected_find_root)
if(NOT selected_sysroot STREQUAL "")
  message(FATAL_ERROR "local GNU cross toolchain must not force CMAKE_SYSROOT: \${selected_sysroot}")
endif()
if(NOT selected_find_root STREQUAL "\${fake_sysroot}")
  message(FATAL_ERROR "local GNU cross toolchain must keep target find root")
endif()
cpkt_configure_linux_toolchain(
  "\${selected_root}"
  "\${selected_prefix}"
  "\${selected_sysroot}"
  "\${selected_find_root}")
if(NOT CMAKE_SYSROOT STREQUAL "")
  message(FATAL_ERROR "configured local GNU cross sysroot must remain empty")
endif()
list(GET CMAKE_FIND_ROOT_PATH 0 first_find_root)
if(NOT first_find_root STREQUAL "\${fake_sysroot}")
  message(FATAL_ERROR "configured local GNU cross find root is wrong: \${CMAKE_FIND_ROOT_PATH}")
endif()
cpkt_configure_linux_toolchain(
  /usr
  aarch64-linux-gnu
  ""
  "\${fake_sysroot}")
list(LENGTH CMAKE_FIND_ROOT_PATH host_root_count)
if(NOT host_root_count EQUAL 1)
  message(FATAL_ERROR "distro GNU cross find roots must exclude host /usr: \${CMAKE_FIND_ROOT_PATH}")
endif()

set(fake_musl_root "${work_dir}/fake-musl-root")
set(fake_musl_sysroot "${work_dir}/fake-musl-sysroot")
make_fake_toolchain("\${fake_musl_root}" x86_64-linux-musl "\${fake_musl_sysroot}")
cpkt_select_linux_toolchain(
  x86_64-linux-musl
  "\${fake_musl_root}"
  x86_64-linux-musl
  "\${fake_musl_sysroot}"
  x86_64-linux
  x86_64-buildroot-linux-musl/sysroot
  selected_musl_root
  selected_musl_prefix
  selected_musl_sysroot
  selected_musl_find_root)
if(NOT selected_musl_sysroot STREQUAL "\${fake_musl_sysroot}")
  message(FATAL_ERROR "local musl cross toolchain must retain its sysroot")
endif()
if(NOT selected_musl_find_root STREQUAL "\${fake_musl_sysroot}")
  message(FATAL_ERROR "local musl cross toolchain must keep target find root")
endif()

message(STATUS "linux toolchain sysroot policy passed")
EOF

cmake -P "$cmake_script"

if ! grep -F -- 'CMAKE_CROSSCOMPILING_EMULATOR /usr/bin/qemu-aarch64;-L;${CPKT_SELECTED_FIND_ROOT}' \
    "$source_dir/cmake/toolchains/aarch64-linux-gnu.cmake" >/dev/null 2>&1; then
  printf 'aarch64 GNU emulator must use selected find root, not compiler sysroot\n' >&2
  exit 1
fi
if ! grep -F -- 'CMAKE_CROSSCOMPILING_EMULATOR /usr/bin/qemu-arm;-L;${CPKT_SELECTED_FIND_ROOT}' \
    "$source_dir/cmake/toolchains/armhf-linux-gnu.cmake" >/dev/null 2>&1; then
  printf 'armhf GNU emulator must use selected find root, not compiler sysroot\n' >&2
  exit 1
fi
