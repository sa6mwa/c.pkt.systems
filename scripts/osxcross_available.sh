#!/usr/bin/env bash
set -eu

if [ -z "${OSXCROSS_ROOT:-}" ]; then
  if [ -n "${HOME:-}" ]; then
    OSXCROSS_ROOT="$HOME/.local/cross/osxcross"
  else
    exit 1
  fi
fi

host=${CPKT_OSXCROSS_HOST:-arm64-apple-darwin25}
for tool in clang clang++ ar ranlib ld install_name_tool otool; do
  test -x "$OSXCROSS_ROOT/bin/$host-$tool" || exit 1
done

for sdk in "$OSXCROSS_ROOT"/SDK/MacOSX*.sdk; do
  if [ -d "$sdk/usr/include" ]; then
    exit 0
  fi
done

exit 1
