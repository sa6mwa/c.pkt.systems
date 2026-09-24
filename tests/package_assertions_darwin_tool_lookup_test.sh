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
case "$1" in
  -D)
    printf '%s:\n' "$2"
    printf '@rpath/libmqttc.1.dylib\n'
    ;;
  -L)
    printf '%s:\n' "$2"
    printf '@rpath/libmqttc.1.dylib (compatibility version 1.0.0, current version 1.1.2)\n'
    printf '/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1351.0.0)\n'
    ;;
  -l)
    printf 'Load command 0\n'
    printf '          cmd LC_RPATH\n'
    printf '         path @loader_path (offset 12)\n'
    ;;
esac
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

output=$(
  OSXCROSS_ROOT="$osxcross_root" \
  CPKT_OSXCROSS_HOST="$osxcross_host" \
  cmake \
    -DCPKT_PACKAGE_ASSERTIONS_TEST_DARWIN_VERSION=ON \
    -DCPKT_PACKAGE_ASSERTIONS_TEST_DYLIB="$work_dir/libmqttc.1.1.2.dylib" \
    -DCPKT_PACKAGE_ASSERTIONS_TEST_EXPECTED_INSTALL_NAME='@rpath/libmqttc.1.dylib' \
    -DCPKT_PACKAGE_ASSERTIONS_TEST_EXPECTED_COMPATIBILITY=1.0.0 \
    -DCPKT_PACKAGE_ASSERTIONS_TEST_EXPECTED_CURRENT=1.1.2 \
    -P "$repo_root/cmake/package_assertions.cmake"
)
case "$output" in
  *"CPKT_TEST_DARWIN_VERSION=ok"*) ;;
  *)
    printf 'package assertion rejected valid Darwin dylib versions\n%s\n' "$output" >&2
    exit 1
    ;;
esac
if OSXCROSS_ROOT="$osxcross_root" \
    CPKT_OSXCROSS_HOST="$osxcross_host" \
    cmake \
      -DCPKT_PACKAGE_ASSERTIONS_TEST_DARWIN_VERSION=ON \
      -DCPKT_PACKAGE_ASSERTIONS_TEST_DYLIB="$work_dir/libmqttc.1.1.2.dylib" \
      -DCPKT_PACKAGE_ASSERTIONS_TEST_EXPECTED_INSTALL_NAME='@rpath/libmqttc.1.dylib' \
      -DCPKT_PACKAGE_ASSERTIONS_TEST_EXPECTED_COMPATIBILITY=9.0.0 \
      -DCPKT_PACKAGE_ASSERTIONS_TEST_EXPECTED_CURRENT=9.6.0 \
      -P "$repo_root/cmake/package_assertions.cmake" >/dev/null 2>&1; then
  printf 'package assertion accepted incompatible Darwin dylib versions\n' >&2
  exit 1
fi

output=$(
  OSXCROSS_ROOT="$osxcross_root" \
  CPKT_OSXCROSS_HOST="$osxcross_host" \
  cmake \
    -DCPKT_TARGET_ID=arm64-apple-darwin \
    -DCPKT_PACKAGE_ASSERTIONS_TEST_DARWIN_RELOCATABLE=ON \
    -DCPKT_PACKAGE_ASSERTIONS_TEST_DYLIB="$work_dir/libmqttc.1.1.2.dylib" \
    -P "$repo_root/cmake/package_assertions.cmake"
)

case "$output" in
  *"CPKT_TEST_DARWIN_RELOCATABLE=ok"*) ;;
  *)
    printf 'package assertion relocatable Mach-O check rejected valid Darwin dylib metadata\n%s\n' "$output" >&2
    exit 1
    ;;
esac

cat > "$osxcross_root/bin/$osxcross_host-otool" <<'SH'
#!/usr/bin/env sh
case "$1" in
  -hv) printf 'Mach header\nmagic cputype filetype\nMH_MAGIC_64 ARM64 BUNDLE flags\n' ;;
  -D) printf 'Mach-O bundle has no install name\n' >&2; exit 1 ;;
  -L)
    printf '%s:\n' "$2"
    printf '@rpath/libpq.5.dylib (compatibility version 5.0.0, current version 5.18.0)\n'
    printf '/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1351.0.0)\n'
    ;;
  -l) printf 'Load command 0\n' ;;
esac
SH
chmod +x "$osxcross_root/bin/$osxcross_host-otool"
touch "$work_dir/libpq-oauth-18.dylib"
output=$(
  OSXCROSS_ROOT="$osxcross_root" \
  CPKT_OSXCROSS_HOST="$osxcross_host" \
  cmake \
    -DCPKT_TARGET_ID=arm64-apple-darwin \
    -DCPKT_PACKAGE_ASSERTIONS_TEST_DARWIN_RELOCATABLE=ON \
    -DCPKT_PACKAGE_ASSERTIONS_TEST_DYLIB="$work_dir/libpq-oauth-18.dylib" \
    -P "$repo_root/cmake/package_assertions.cmake"
)
case "$output" in
  *"CPKT_TEST_DARWIN_RELOCATABLE=ok"*) ;;
  *)
    printf 'package assertion rejected a relocatable Mach-O bundle without an install name\n%s\n' "$output" >&2
    exit 1
    ;;
esac

cat > "$osxcross_root/bin/$osxcross_host-otool" <<'SH'
#!/usr/bin/env sh
case "$1" in
  -hv) printf 'Mach header\nmagic cputype filetype\nMH_MAGIC_64 ARM64 BUNDLE flags\n' ;;
  -L)
    printf '%s:\n' "$2"
    printf '@rpath/libkrb5.3.3.dylib (compatibility version 3.0.0, current version 3.3.0)\n'
    printf '@rpath/libssl.3.dylib (compatibility version 3.0.0, current version 3.0.0)\n'
    ;;
  -l)
    printf 'Load command 0\n'
    printf '          cmd LC_RPATH\n'
    printf '         path %s (offset 12)\n' "$CPKT_TEST_TLS_RPATH"
    ;;
esac
SH
chmod +x "$osxcross_root/bin/$osxcross_host-otool"
mkdir -p "$work_dir/lib/krb5/plugins/tls"
touch "$work_dir/lib/krb5/plugins/tls/k5tls.so"

if ! output=$(
    CPKT_TEST_TLS_RPATH='@loader_path/../../..' \
    OSXCROSS_ROOT="$osxcross_root" \
    CPKT_OSXCROSS_HOST="$osxcross_host" \
    cmake \
      -DCPKT_TARGET_ID=arm64-apple-darwin \
      -DCPKT_PACKAGE_ASSERTIONS_TEST_DARWIN_RELOCATABLE=ON \
      -DCPKT_PACKAGE_ASSERTIONS_TEST_DYLIB="$work_dir/lib/krb5/plugins/tls/k5tls.so" \
      -P "$repo_root/cmake/package_assertions.cmake"
  ); then
  printf 'package assertion rejected the bundled Kerberos TLS module\n%s\n' "$output" >&2
  exit 1
fi

if output=$(
    CPKT_TEST_TLS_RPATH='@loader_path' \
    OSXCROSS_ROOT="$osxcross_root" \
    CPKT_OSXCROSS_HOST="$osxcross_host" \
    cmake \
      -DCPKT_TARGET_ID=arm64-apple-darwin \
      -DCPKT_PACKAGE_ASSERTIONS_TEST_DARWIN_RELOCATABLE=ON \
      -DCPKT_PACKAGE_ASSERTIONS_TEST_DYLIB="$work_dir/lib/krb5/plugins/tls/k5tls.so" \
      -P "$repo_root/cmake/package_assertions.cmake" 2>&1
  ); then
  printf 'package assertion accepted a Kerberos TLS module without its bundled library path\n%s\n' "$output" >&2
  exit 1
fi
case "$output" in
  *"cannot resolve bundled sibling libraries from"*"lib/krb5/plugins/tls"*) ;;
  *)
    printf 'Kerberos TLS module rpath failure was not actionable\n%s\n' "$output" >&2
    exit 1
    ;;
esac

cat > "$osxcross_root/bin/$osxcross_host-otool" <<'SH'
#!/usr/bin/env sh
case "$1" in
  -D)
    printf '%s:\n' "$2"
    printf '@rpath/libssl.3.dylib\n'
    ;;
  -L)
    printf '%s:\n' "$2"
    printf '@rpath/libssl.3.dylib (compatibility version 3.0.0, current version 3.0.0)\n'
    printf '/lib/libcrypto.3.dylib (compatibility version 3.0.0, current version 3.0.0)\n'
    ;;
  -l)
    printf 'Load command 0\n'
    printf '          cmd LC_RPATH\n'
    printf '         path @loader_path (offset 12)\n'
    ;;
esac
SH
chmod +x "$osxcross_root/bin/$osxcross_host-otool"
touch "$work_dir/libssl.3.dylib"

if output=$(
    OSXCROSS_ROOT="$osxcross_root" \
    CPKT_OSXCROSS_HOST="$osxcross_host" \
    cmake \
      -DCPKT_TARGET_ID=arm64-apple-darwin \
      -DCPKT_PACKAGE_ASSERTIONS_TEST_DARWIN_RELOCATABLE=ON \
      -DCPKT_PACKAGE_ASSERTIONS_TEST_DYLIB="$work_dir/libssl.3.dylib" \
      -P "$repo_root/cmake/package_assertions.cmake" 2>&1
  ); then
  printf 'package assertion relocatable Mach-O check accepted /lib dependency\n%s\n' "$output" >&2
  exit 1
fi

case "$output" in
  *"non-relocatable Darwin dependency"*"/lib/libcrypto.3.dylib"*) ;;
  *)
    printf 'package assertion relocatable Mach-O check failed with unexpected output\n%s\n' "$output" >&2
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
