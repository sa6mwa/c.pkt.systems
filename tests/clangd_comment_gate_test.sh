#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  printf 'usage: clangd_comment_gate_test.sh <verify-clangd-surface.sh>\n' >&2
  exit 2
fi

checker=$1
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT HUP INT TERM

source_dir="$work_dir/source"
build_dir="$work_dir/build"
mkdir -p "$source_dir/include/cpkt" "$source_dir/src" "$source_dir/examples" "$build_dir" "$work_dir/bin"

printf '#!/usr/bin/env bash\nexit 0\n' > "$work_dir/bin/clangd"
chmod +x "$work_dir/bin/clangd"

write_header() {
  local comment=$1
  printf '%s\nvoid cpkt_documented(void);\n' "$comment" > "$source_dir/include/cpkt/facade.h"
}

write_source() {
  local comment=$1
  printf '%s\nvoid cpkt_documented(void) {}\n' "$comment" > "$source_dir/src/facade.c"
}

for source_file in \
  examples/abi_smoke.c \
  examples/audio-sus-c89/main.c \
  examples/audio-vox-intro-c89/main.c \
  examples/sus-vox-intro-c89/main.c \
  examples/lua-runtime-c89/main.c \
  examples/lua-runtime-c89/host_module.c \
  examples/opcua-c89/main.c; do
  mkdir -p "$(dirname "$source_dir/$source_file")"
  : > "$source_dir/$source_file"
  printf '"%s/%s"\n' "$source_dir" "$source_file" >> "$build_dir/compile_commands.json"
done

run_gate() {
  PATH="$work_dir/bin:$PATH" bash "$checker" "$source_dir" "$build_dir"
}

write_header '/** Public facade declaration. */'
write_source '/** Public facade definition. */'
run_gate

write_header '/* Ordinary block comment is not Doxygen. */'
if run_gate >"$work_dir/header.out" 2>"$work_dir/header.err"; then
  printf 'clangd comment gate accepted an ordinary header block comment\n' >&2
  exit 1
fi
grep -F 'public facade symbol is missing an adjacent Doxygen comment' "$work_dir/header.err" >/dev/null

write_header '/** Public facade declaration. */'
write_source '/* Ordinary block comment is not Doxygen. */'
if run_gate >"$work_dir/source.out" 2>"$work_dir/source.err"; then
  printf 'clangd comment gate accepted an ordinary source block comment\n' >&2
  exit 1
fi
grep -F 'public facade symbol is missing an adjacent Doxygen comment' "$work_dir/source.err" >/dev/null
