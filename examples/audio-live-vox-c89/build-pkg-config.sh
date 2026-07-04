#!/usr/bin/env sh
set -eu

if [ "${CPKT_SDK_PREFIX:-}" = "" ]; then
  printf 'CPKT_SDK_PREFIX is required and must point at an extracted c.pkt.systems SDK\n' >&2
  exit 2
fi

cc=${CC:-cc}
pkg_config=${PKG_CONFIG:-pkg-config}
output=${1:-./cpkt_audio_live_vox_c89_example}
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
output_dir=$(dirname -- "$output")

mkdir -p "$output_dir"

pkg_config_cflags=$(
  PKG_CONFIG_PATH= \
  PKG_CONFIG_LIBDIR="$CPKT_SDK_PREFIX/lib/pkgconfig" \
    "$pkg_config" --cflags cpkt-audio
)
pkg_config_libs=$(
  PKG_CONFIG_PATH= \
  PKG_CONFIG_LIBDIR="$CPKT_SDK_PREFIX/lib/pkgconfig" \
    "$pkg_config" --static --libs cpkt-audio
)

"$cc" ${CPKT_EXAMPLE_CFLAGS:-} -std=c89 -Wall -Wextra -Wpedantic \
  "$script_dir/main.c" -o "$output" \
  $pkg_config_cflags \
  $pkg_config_libs \
  ${CPKT_EXAMPLE_LDFLAGS:-}
