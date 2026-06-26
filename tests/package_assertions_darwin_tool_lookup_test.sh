#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

osxcross_root="$work_dir/osxcross"
osxcross_host="custom-apple-darwin99"
mkdir -p "$osxcross_root/bin"
cat > "$osxcross_root/bin/$osxcross_host-otool" <<'SH'
#!/usr/bin/env sh
exit 0
SH
chmod +x "$osxcross_root/bin/$osxcross_host-otool"
cat > "$osxcross_root/bin/$osxcross_host-nm" <<'SH'
#!/usr/bin/env sh
exit 0
SH
chmod +x "$osxcross_root/bin/$osxcross_host-nm"

output=$(
  OSXCROSS_ROOT="$osxcross_root" \
  CPKT_OSXCROSS_HOST="$osxcross_host" \
  cmake \
    -DCPKT_PACKAGE_ASSERTIONS_TEST_DARWIN_OTOOL_LOOKUP=ON \
    -P "$repo_root/cmake/package_assertions.cmake"
)

case "$output" in
  *"CPKT_TEST_OTOOL=$osxcross_root/bin/$osxcross_host-otool"*) ;;
  *)
    printf 'package assertion otool lookup did not honor CPKT_OSXCROSS_HOST\n%s\n' "$output" >&2
    exit 1
    ;;
esac

output=$(
  OSXCROSS_ROOT="$osxcross_root" \
  CPKT_OSXCROSS_HOST="$osxcross_host" \
  cmake \
    -DCPKT_TARGET_ID=arm64-apple-darwin \
    -DCPKT_PACKAGE_ASSERTIONS_TEST_NM_LOOKUP=ON \
    -P "$repo_root/cmake/package_assertions.cmake"
)

case "$output" in
  *"CPKT_TEST_NM=$osxcross_root/bin/$osxcross_host-nm"*) ;;
  *)
    printf 'package assertion nm lookup did not honor CPKT_OSXCROSS_HOST\n%s\n' "$output" >&2
    exit 1
    ;;
esac

cat > "$osxcross_root/bin/$osxcross_host-otool" <<'SH'
#!/usr/bin/env sh
printf '%s:\n' "$2"
printf '@rpath/libmqttc.1.dylib\n'
SH
chmod +x "$osxcross_root/bin/$osxcross_host-otool"
touch "$work_dir/libmqttc.1.1.2.dylib"

output=$(
  OSXCROSS_ROOT="$osxcross_root" \
  CPKT_OSXCROSS_HOST="$osxcross_host" \
  cmake \
    -DCPKT_PACKAGE_ASSERTIONS_TEST_DARWIN_INSTALL_NAME=ON \
    -DCPKT_PACKAGE_ASSERTIONS_TEST_DYLIB="$work_dir/libmqttc.1.1.2.dylib" \
    -DCPKT_PACKAGE_ASSERTIONS_TEST_EXPECTED_INSTALL_NAME='@rpath/libmqttc.1.dylib' \
    -P "$repo_root/cmake/package_assertions.cmake"
)

case "$output" in
  *"CPKT_TEST_DARWIN_INSTALL_NAME=ok"*) ;;
  *)
    printf 'package assertion install-name check did not accept exact Darwin install name\n%s\n' "$output" >&2
    exit 1
    ;;
esac

cat > "$osxcross_root/bin/$osxcross_host-nm" <<'SH'
#!/usr/bin/env sh
case " $* " in
  *" --defined-only "*)
    printf 'GNU-only nm flag was used\n' >&2
    exit 1
    ;;
  *" -gU "*)
    printf '00000000 T _cpkt_open62541_mqtt_connect\n'
    exit 0
    ;;
  *)
    printf 'unexpected nm flags: %s\n' "$*" >&2
    exit 1
    ;;
esac
SH
chmod +x "$osxcross_root/bin/$osxcross_host-nm"
touch "$work_dir/libopen62541.a"

output=$(
  OSXCROSS_ROOT="$osxcross_root" \
  CPKT_OSXCROSS_HOST="$osxcross_host" \
  cmake \
    -DCPKT_TARGET_ID=arm64-apple-darwin \
    -DCPKT_PACKAGE_ASSERTIONS_TEST_NM_SYMBOL_READ=ON \
    -DCPKT_PACKAGE_ASSERTIONS_TEST_ARCHIVE="$work_dir/libopen62541.a" \
    -P "$repo_root/cmake/package_assertions.cmake"
)

case "$output" in
  *"CPKT_TEST_SYMBOLS=00000000 T _cpkt_open62541_mqtt_connect"*) ;;
  *)
    printf 'package assertion nm symbol read did not use Darwin-compatible flags\n%s\n' "$output" >&2
    exit 1
    ;;
esac

if grep -F -- '"uint"' "$repo_root/cmake/package_assertions.cmake" >/dev/null 2>&1 ||
    grep -F -- '"int32"' "$repo_root/cmake/package_assertions.cmake" >/dev/null 2>&1 ||
    grep -F -- '"int64"' "$repo_root/cmake/package_assertions.cmake" >/dev/null 2>&1; then
  printf 'package assertions must not reject C89-safe facade identifiers such as cpkt_opcua_uint64\n' >&2
  exit 1
fi
if ! grep -F -- '"uint64_t"' "$repo_root/cmake/package_assertions.cmake" >/dev/null 2>&1; then
  printf 'package assertions no longer reject C99 fixed-width integer typedef leaks\n' >&2
  exit 1
fi
