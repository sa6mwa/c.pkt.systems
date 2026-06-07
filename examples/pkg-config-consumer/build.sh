#!/usr/bin/env sh
set -eu

if [ "${CPKT_SDK_PREFIX:-}" = "" ]; then
  printf 'CPKT_SDK_PREFIX is required and must point at an extracted c.pkt.systems SDK\n' >&2
  exit 2
fi

cc=${CC:-cc}
pkg_config=${PKG_CONFIG:-pkg-config}
output=${1:-./cpkt_bundle_pkg_config_consumer}
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source_file="$script_dir/../abi_smoke.c"
pkg_config_words=$(
  PKG_CONFIG_PATH= \
  PKG_CONFIG_LIBDIR="$CPKT_SDK_PREFIX/lib/pkgconfig" \
    "${pkg_config}" --static --cflags --libs libcurl
)

"$cc" ${CPKT_EXAMPLE_CFLAGS:-} "$source_file" -o "$output" \
  $pkg_config_words \
  ${CPKT_EXAMPLE_LDFLAGS:-}
