#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/cpkt-darwin-install-name.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT

install_root="$work_dir/install"
tool_log="$work_dir/install-name-tool.log"
mkdir -p "$install_root/lib" "$work_dir/bin"
touch "$install_root/lib/libmqttc.1.1.2.dylib"

cat > "$work_dir/bin/install_name_tool" <<'SH'
#!/usr/bin/env sh
printf '%s\n' "$*" >> "$CPKT_TEST_INSTALL_NAME_TOOL_LOG"
exit 0
SH
chmod +x "$work_dir/bin/install_name_tool"

cat > "$work_dir/bin/otool" <<'SH'
#!/usr/bin/env sh
printf '%s:\n' "$2"
exit 0
SH
chmod +x "$work_dir/bin/otool"

cat > "$work_dir/bin/strip" <<'SH'
#!/usr/bin/env sh
exit 0
SH
chmod +x "$work_dir/bin/strip"

CPKT_TEST_INSTALL_NAME_TOOL_LOG="$tool_log" \
  cmake \
    -DCPKT_STRIP_ROOT="$install_root" \
    -DCPKT_STRIP_BIN="$work_dir/bin/strip" \
    -DCPKT_STRIP_STATIC_ARCHIVES=OFF \
    -DCPKT_DARWIN_DEPENDENCY_ROOT="$install_root" \
    -DCPKT_INSTALL_NAME_TOOL="$work_dir/bin/install_name_tool" \
    -DCPKT_OTOOL="$work_dir/bin/otool" \
    -P "$repo_root/cmake/strip_dependency_install_tree.cmake"

if ! grep -F -- "-id @rpath/libmqttc.1.dylib $install_root/lib/libmqttc.1.1.2.dylib" \
    "$tool_log" >/dev/null 2>&1; then
  printf 'Darwin dependency fixup did not preserve MQTT-C ABI install name\n' >&2
  printf 'install_name_tool calls:\n' >&2
  cat "$tool_log" >&2
  exit 1
fi
