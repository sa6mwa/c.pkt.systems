#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 4 ]; then
  printf 'usage: audio_sus_shared_abi_test.sh <libcpktaudio.so> <libcpktsus.so> <audio-abi> <sus-abi>\n' >&2
  exit 2
fi

audio_lib=$1
sus_lib=$2
audio_abi=$3
sus_abi=$4

require_tool() {
  tool=$1
  if ! command -v "$tool" >/dev/null 2>&1; then
    printf '%s is required to inspect shared facade ABI metadata\n' "$tool" >&2
    exit 1
  fi
}

require_file() {
  file_path=$1
  description=$2
  if [ ! -f "$file_path" ]; then
    printf 'missing %s: %s\n' "$description" "$file_path" >&2
    exit 1
  fi
}

assert_soname() {
  file_path=$1
  expected=$2
  description=$3

  if ! readelf -d "$file_path" | grep -E '\(SONAME\).*\['"$expected"'\]' >/dev/null 2>&1; then
    printf '%s does not have expected SONAME %s\n' "$description" "$expected" >&2
    readelf -d "$file_path" >&2
    exit 1
  fi
}

assert_needed_lacks() {
  file_path=$1
  forbidden=$2
  description=$3

  if readelf -d "$file_path" | grep -E '\(NEEDED\).*\['"$forbidden"'\]' >/dev/null 2>&1; then
    printf '%s has forbidden runtime dependency matching %s\n' "$description" "$forbidden" >&2
    readelf -d "$file_path" >&2
    exit 1
  fi
}

assert_exports_prefix() {
  file_path=$1
  prefix=$2
  description=$3
  count=0

  while IFS= read -r symbol; do
    [ -n "$symbol" ] || continue
    count=$((count + 1))
    case "$symbol" in
      "$prefix"*) ;;
      *)
        printf '%s exports non-facade symbol: %s\n' "$description" "$symbol" >&2
        nm -D --defined-only "$file_path" >&2
        exit 1
        ;;
    esac
  done <<EOF
$(nm -D --defined-only "$file_path" | awk 'NF >= 3 { print $3 }')
EOF

  if [ "$count" -eq 0 ]; then
    printf '%s exports no public facade symbols\n' "$description" >&2
    exit 1
  fi
}

require_tool readelf
require_tool nm
require_file "$audio_lib" "libcpktaudio shared library"
require_file "$sus_lib" "libcpktsus shared library"

assert_soname "$audio_lib" "libcpktaudio.so.$audio_abi" "libcpktaudio"
assert_soname "$sus_lib" "libcpktsus.so.$sus_abi" "libcpktsus"
assert_exports_prefix "$audio_lib" "cpkt_audio_" "libcpktaudio"
assert_exports_prefix "$sus_lib" "cpkt_sus_" "libcpktsus"
assert_needed_lacks "$audio_lib" 'libminiaudio\.so[^]]*' "libcpktaudio"
assert_needed_lacks "$sus_lib" 'libstdc\+\+\.so[^]]*' "libcpktsus"
assert_needed_lacks "$sus_lib" 'libgcc_s\.so[^]]*' "libcpktsus"
