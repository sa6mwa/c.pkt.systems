#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
  printf 'usage: %s <smoke|standard|long>\n' "$0" >&2
  exit 2
fi

mode=$1
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
cmake=${CMAKE:-cmake}

case "$mode" in
  smoke|standard|long) ;;
  *)
    printf 'unknown AFL++ fuzz mode: %s\n' "$mode" >&2
    exit 2
    ;;
esac

cd "$repo_root"

bash ./scripts/configure-preset.sh --fresh fuzz
"$cmake" --build --preset fuzz
bash ./scripts/run-afl-fuzz.sh "$mode" build/fuzz/cpkt_lua_runtime_fuzz fuzz/seeds/lua

bash ./scripts/configure-preset.sh debug
"$cmake" --build --preset debug --target cpkt_opcua_static
bash ./scripts/configure-preset.sh --fresh opcua-fuzz
"$cmake" --build --preset opcua-fuzz
bash ./scripts/run-afl-fuzz.sh "$mode" build/opcua-fuzz/cpkt_opcua_facade_fuzz fuzz/seeds/opcua
