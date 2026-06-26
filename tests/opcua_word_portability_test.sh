#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=${1:-$(CDPATH= cd -- "$script_dir/.." && pwd)}
open62541_include_dir=${2:-}
cc=${CC:-cc}
work_root=$(mktemp -d "${TMPDIR:-/tmp}/cpkt-opcua-word-portability.XXXXXX")
trap 'rm -rf "$work_root"' EXIT

compile_facade_word_width() {
  compiler=$1
  output_name=$2

  "$compiler" -std=c89 -Wall -Wextra -Wpedantic -Werror -I "$repo_root/include" \
    -c "$work_root/facade_word_width_c89.c" -o "$work_root/$output_name.o"
}

compile_facade_word_width_32_bit_abi() {
  compiler=$1
  output_name=$2

  "$compiler" -std=c89 -Wall -Wextra -Wpedantic -Werror -I "$repo_root/include" \
    -c "$work_root/facade_word_width_32_bit_abi_c89.c" -o "$work_root/$output_name.o"
}

compile_upstream_word_width() {
  compiler=$1
  include_dir=$2
  output_name=$3

  "$compiler" -std=c99 -D_GNU_SOURCE -Wall -Wextra -Wpedantic -Wno-unused-parameter \
    -I "$include_dir" \
    -c "$work_root/upstream_word_width_c99.c" -o "$work_root/$output_name.o"
}

for source in \
  "$repo_root/tests/opcua_facade_test.c" \
  "$repo_root/tests/opcua_c89_boundary_peer.c"
do
  if grep -En '0x[[:xdigit:]]{9,}UL|2147483648L' "$source" >/dev/null 2>&1; then
    printf 'OPC UA facade test uses a long literal that may not fit on 32-bit targets: %s\n' "$source" >&2
    grep -En '0x[[:xdigit:]]{9,}UL|2147483648L' "$source" >&2
    exit 1
  fi
done

if grep -F -- 'uint64_t' "$repo_root/include/cpkt/opcua.h" >/dev/null 2>&1; then
  printf 'OPC UA C89 facade header must expose split 32-bit words, not uint64_t\n' >&2
  exit 1
fi

if ! grep -F -- 'cpkt_valid_uint64_words(value->uint64_value.high32, value->uint64_value.low32)' \
    "$repo_root/src/opcua.c" >/dev/null 2>&1; then
  printf 'OPC UA facade no longer validates UInt64 high/low words before native conversion\n' >&2
  exit 1
fi

for required_source_assertion in \
  'cpkt_opcua_assert_upstream_uint64_is_64_bits' \
  'cpkt_opcua_assert_upstream_datetime_is_64_bits' \
  'cpkt_opcua_assert_upstream_status_is_32_bits'
do
  if ! grep -F -- "$required_source_assertion" "$repo_root/src/opcua.c" >/dev/null 2>&1; then
    printf 'OPC UA facade implementation must compile-assert upstream integer widths: %s\n' \
      "$required_source_assertion" >&2
    exit 1
  fi
done

if ! grep -F -- 'return ((UA_UInt64)value.high32 << 32) | (UA_UInt64)value.low32;' \
    "$repo_root/src/opcua.c" >/dev/null 2>&1; then
  printf 'OPC UA facade must widen split UInt64 words before shifting for 32-bit targets\n' >&2
  exit 1
fi

if grep -E 'return +\(?\(?value[.]high32 << 32|\(UA_UInt64\)\(value[.]high32 << 32|\(unsigned long\)value[.]high32 << 32' \
    "$repo_root/src/opcua.c" >/dev/null 2>&1; then
  printf 'OPC UA facade contains a 32-bit word shift before UInt64 widening\n' >&2
  grep -En 'return +\(?\(?value[.]high32 << 32|\(UA_UInt64\)\(value[.]high32 << 32|\(unsigned long\)value[.]high32 << 32' \
      "$repo_root/src/opcua.c" >&2
  exit 1
fi

if ! grep -F -- 'out.high32 = -1L - (long)(CPKT_OPCUA_UINT32_MAX_VALUE - high32);' \
    "$repo_root/src/opcua.c" >/dev/null 2>&1; then
  printf 'OPC UA facade must decode negative DateTime high words without signed overflow on 32-bit targets\n' >&2
  exit 1
fi

if grep -F -- '-((long)(CPKT_OPCUA_UINT32_MAX_VALUE - high32) + 1L)' \
    "$repo_root/src/opcua.c" >/dev/null 2>&1; then
  printf 'OPC UA facade contains the old DateTime decode expression that overflows for 0x80000000 on 32-bit long\n' >&2
  exit 1
fi

if ! grep -F -- 'out.uint64_value.high32 == 0xffffffffUL' \
    "$repo_root/tests/opcua_c89_boundary_peer.c" >/dev/null 2>&1; then
  printf 'OPC UA boundary tests must exercise UInt64 values with all high word bits set\n' >&2
  exit 1
fi

if ! grep -F -- 'native_uint64 >> 32' "$repo_root/tests/opcua_facade_test.c" >/dev/null 2>&1; then
  printf 'OPC UA native boundary tests must verify UInt64 high-word preservation\n' >&2
  exit 1
fi

if ! grep -F -- 'cpkt_valid_datetime_words(value->datetime_value.high32, value->datetime_value.low32)' \
    "$repo_root/src/opcua.c" >/dev/null 2>&1; then
  printf 'OPC UA facade no longer validates DateTime high/low words before native conversion\n' >&2
  exit 1
fi

cat > "$work_root/facade_word_width_c89.c" <<'EOF'
#include <limits.h>
#include <cpkt/opcua.h>

typedef char cpkt_assert_unsigned_long_has_32_bits[(ULONG_MAX >= 4294967295UL) ? 1 : -1];
typedef char cpkt_assert_long_has_32_bits[(LONG_MAX >= 2147483647L) ? 1 : -1];
typedef char cpkt_assert_uint64_high_word_has_32_bits[
    (sizeof(((cpkt_opcua_uint64 *)0)->high32) * CHAR_BIT >= 32) ? 1 : -1];
typedef char cpkt_assert_uint64_low_word_has_32_bits[
    (sizeof(((cpkt_opcua_uint64 *)0)->low32) * CHAR_BIT >= 32) ? 1 : -1];
typedef char cpkt_assert_datetime_high_word_has_32_bits[
    (sizeof(((cpkt_opcua_datetime *)0)->high32) * CHAR_BIT >= 32) ? 1 : -1];
typedef char cpkt_assert_datetime_low_word_has_32_bits[
    (sizeof(((cpkt_opcua_datetime *)0)->low32) * CHAR_BIT >= 32) ? 1 : -1];

int main(void) {
  cpkt_opcua_uint64 max_uint64;
  cpkt_opcua_datetime negative_datetime;

  max_uint64.high32 = 0xffffffffUL;
  max_uint64.low32 = 0xffffffffUL;
  negative_datetime.high32 = -1L;
  negative_datetime.low32 = 0xffffffffUL;
  return max_uint64.high32 == 0 || negative_datetime.high32 == 0;
}
EOF

compile_facade_word_width "$cc" facade_word_width_c89

cat > "$work_root/facade_word_width_32_bit_abi_c89.c" <<'EOF'
#include <limits.h>
#include <cpkt/opcua.h>

typedef char cpkt_assert_unsigned_long_is_32_bits[(ULONG_MAX == 4294967295UL) ? 1 : -1];
typedef char cpkt_assert_long_is_32_bits[(LONG_MAX == 2147483647L) ? 1 : -1];
typedef char cpkt_assert_pointer_is_32_bits[(sizeof(void *) * CHAR_BIT == 32) ? 1 : -1];
typedef char cpkt_assert_uint64_high_word_is_native_32_bit_word[
    (sizeof(((cpkt_opcua_uint64 *)0)->high32) == sizeof(unsigned long)) ? 1 : -1];
typedef char cpkt_assert_uint64_low_word_is_native_32_bit_word[
    (sizeof(((cpkt_opcua_uint64 *)0)->low32) == sizeof(unsigned long)) ? 1 : -1];

int main(void) {
  cpkt_opcua_uint64 value;

  value.high32 = 0xffffffffUL;
  value.low32 = 0xffffffffUL;
  return value.high32 != 0xffffffffUL || value.low32 != 0xffffffffUL;
}
EOF

cat > "$work_root/upstream_word_width_c99.c" <<'EOF'
#include <limits.h>
#include <open62541/types.h>

typedef char cpkt_assert_upstream_uint64_is_64_bits[(sizeof(UA_UInt64) * CHAR_BIT == 64) ? 1 : -1];
typedef char cpkt_assert_upstream_datetime_is_64_bits[(sizeof(UA_DateTime) * CHAR_BIT == 64) ? 1 : -1];
typedef char cpkt_assert_upstream_status_is_32_bits[(sizeof(UA_StatusCode) * CHAR_BIT == 32) ? 1 : -1];

int main(void) {
  UA_UInt64 uint64_value;
  UA_DateTime datetime_value;

  uint64_value = ((UA_UInt64)0xffffffffU << 32) | (UA_UInt64)0xffffffffU;
  datetime_value = (UA_DateTime)((UA_Int64)-1);
  return uint64_value == 0 || datetime_value == 0;
}
EOF

if [ -n "$open62541_include_dir" ] && [ -f "$open62541_include_dir/open62541/types.h" ]; then
  compile_upstream_word_width "$cc" "$open62541_include_dir" upstream_word_width_c99
fi

for staged_include_dir in \
  "$repo_root"/build/*/package-stage/c.pkt.systems-*/include
do
  if [ ! -f "$staged_include_dir/open62541/types.h" ]; then
    continue
  fi

  case "$staged_include_dir" in
    *armhf-linux-gnu*)
      if ! command -v arm-linux-gnueabihf-gcc >/dev/null 2>&1; then
        printf 'armhf GNU SDK is staged, but arm-linux-gnueabihf-gcc is missing; cannot verify 32-bit UInt64 ABI\n' >&2
        exit 1
      fi
      compile_facade_word_width arm-linux-gnueabihf-gcc facade_word_width_c89_armhf_linux_gnu
      compile_facade_word_width_32_bit_abi \
        arm-linux-gnueabihf-gcc \
        facade_word_width_32_bit_abi_c89_armhf_linux_gnu
      compile_upstream_word_width \
        arm-linux-gnueabihf-gcc \
        "$staged_include_dir" \
        upstream_word_width_c99_armhf_linux_gnu
      ;;
    *armhf-linux-musl*)
      armhf_musl_prefix=${CPKT_ARMHF_MUSL_PREFIX:-"$HOME/.local/cross/arm-linux-musleabihf"}
      armhf_musl_cc="$armhf_musl_prefix/bin/arm-linux-musleabihf-gcc"
      if [ ! -x "$armhf_musl_cc" ]; then
        printf 'armhf musl SDK is staged, but %s is missing; cannot verify 32-bit UInt64 ABI\n' "$armhf_musl_cc" >&2
        exit 1
      fi
      compile_facade_word_width "$armhf_musl_cc" facade_word_width_c89_armhf_linux_musl
      compile_facade_word_width_32_bit_abi \
        "$armhf_musl_cc" \
        facade_word_width_32_bit_abi_c89_armhf_linux_musl
      compile_upstream_word_width \
        "$armhf_musl_cc" \
        "$staged_include_dir" \
        upstream_word_width_c99_armhf_linux_musl
      ;;
  esac
done
