#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  printf 'usage: package_verify_toolchain_tools_test.sh <source-dir>\n' >&2
  exit 2
fi

source_dir=$1
package_verify="$source_dir/scripts/package-verify.sh"
assertions="$source_dir/cmake/package_assertions.cmake"

if ! grep -Fq 'cpkt-toolchains.sh" discover "$target_id"' "$package_verify"; then
  printf 'package verification does not resolve the pinned Bootlin collection\n' >&2
  exit 1
fi

for tool in NM AR READELF; do
  lower_tool=$(printf '%s' "$tool" | tr '[:upper:]' '[:lower:]')
  if ! grep -Fq "toolchain_value $lower_tool" "$package_verify"; then
    printf 'package verification does not propagate pinned %s\n' "$tool" >&2
    exit 1
  fi
  if ! grep -Fq -- "-DCPKT_$tool" "$package_verify"; then
    printf 'package assertions do not receive pinned %s\n' "$tool" >&2
    exit 1
  fi
done

if grep -Fq 'find_program(CPKT_READELF_BIN NAMES readelf)' "$assertions"; then
  printf 'ELF package assertions must use the configured pinned readelf\n' >&2
  exit 1
fi
