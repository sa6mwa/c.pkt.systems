#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/cpkt-darwin-install-name.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT

dependency_root="$work_dir/deps"
mqttc_root="$dependency_root/mqtt-c/install"
openssl_root="$dependency_root/openssl/install"
libxml2_root="$dependency_root/libxml2/install"
tool_log="$work_dir/install-name-tool.log"
mkdir -p "$mqttc_root/lib" "$openssl_root/lib" "$libxml2_root/lib" "$work_dir/bin"
touch "$mqttc_root/lib/libmqttc.1.1.2.dylib"
touch "$openssl_root/lib/libssl.3.dylib"
touch "$openssl_root/lib/libcrypto.3.dylib"
touch "$libxml2_root/lib/libxml2.16.dylib"

cat > "$work_dir/bin/install_name_tool" <<'SH'
#!/usr/bin/env sh
printf '%s\n' "$*" >> "$CPKT_TEST_INSTALL_NAME_TOOL_LOG"
exit 1
SH
chmod +x "$work_dir/bin/install_name_tool"

cat > "$work_dir/bin/strip" <<'SH'
#!/usr/bin/env sh
printf '%s\n' "$*" >> "$CPKT_TEST_STRIP_LOG"
exit 0
SH
chmod +x "$work_dir/bin/strip"

strip_log="$work_dir/strip.log"
touch "$tool_log" "$strip_log"

CPKT_TEST_INSTALL_NAME_TOOL_LOG="$tool_log" \
CPKT_TEST_STRIP_LOG="$strip_log" \
  cmake \
    -DCPKT_STRIP_ROOT="$mqttc_root" \
    -DCPKT_STRIP_BIN="$work_dir/bin/strip" \
    -DCPKT_STRIP_STATIC_ARCHIVES=OFF \
    -DCPKT_STRIP_SHARED_LIBRARIES=OFF \
    -P "$repo_root/cmake/strip_dependency_install_tree.cmake"

CPKT_TEST_INSTALL_NAME_TOOL_LOG="$tool_log" \
CPKT_TEST_STRIP_LOG="$strip_log" \
  cmake \
    -DCPKT_STRIP_ROOT="$openssl_root" \
    -DCPKT_STRIP_BIN="$work_dir/bin/strip" \
    -DCPKT_STRIP_STATIC_ARCHIVES=OFF \
    -DCPKT_STRIP_SHARED_LIBRARIES=OFF \
    -P "$repo_root/cmake/strip_dependency_install_tree.cmake"

CPKT_TEST_INSTALL_NAME_TOOL_LOG="$tool_log" \
CPKT_TEST_STRIP_LOG="$strip_log" \
  cmake \
    -DCPKT_STRIP_ROOT="$libxml2_root" \
    -DCPKT_STRIP_BIN="$work_dir/bin/strip" \
    -DCPKT_STRIP_STATIC_ARCHIVES=OFF \
    -DCPKT_STRIP_SHARED_LIBRARIES=OFF \
    -P "$repo_root/cmake/strip_dependency_install_tree.cmake"

if [ -s "$tool_log" ]; then
  printf 'Darwin dependency cleanup invoked install_name_tool unexpectedly\n' >&2
  printf 'install_name_tool calls:\n' >&2
  cat "$tool_log" >&2
  exit 1
fi

if [ -s "$strip_log" ]; then
  printf 'Darwin dependency cleanup stripped shared libraries unexpectedly\n' >&2
  printf 'strip calls:\n' >&2
  cat "$strip_log" >&2
  exit 1
fi

printf '[test] Darwin dependency install-name cleanup passed\n'
