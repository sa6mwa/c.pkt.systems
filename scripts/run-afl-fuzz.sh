#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
  printf 'usage: %s <smoke|standard> <target> <seed-dir>\n' "$0" >&2
  exit 2
fi

mode=$1
target=$2
seed_dir=$3
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

case "$mode" in
  smoke) duration=2 ;;
  standard) duration=30 ;;
  *) printf 'unknown AFL++ duration mode: %s\n' "$mode" >&2; exit 2 ;;
esac

[ -x "$target" ] || { printf 'AFL++ target is not executable: %s\n' "$target" >&2; exit 1; }
[ -d "$seed_dir" ] || { printf 'AFL++ seed directory is missing: %s\n' "$seed_dir" >&2; exit 1; }

eval "$("$repo_root/scripts/cpkt-aflpp.sh" env)"
output_dir=$(mktemp -d "${target}.afl-output.XXXXXX")
trap 'rm -rf "$output_dir"' EXIT HUP INT TERM
AFL_SKIP_CPUFREQ=1 AFL_NO_AFFINITY=1 AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
  "$CPKT_AFLPP_ROOT/bin/afl-fuzz" -V "$duration" -i "$seed_dir" -o "$output_dir" -- "$target" @@
