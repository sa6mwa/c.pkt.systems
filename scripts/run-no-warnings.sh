#!/usr/bin/env bash
set -eu

if [ "$#" -lt 2 ]; then
  printf 'usage: run-no-warnings.sh <description> <command> [args...]\n' >&2
  exit 2
fi

description=$1
shift

work_root=$(mktemp -d "${TMPDIR:-/tmp}/cpkt-no-warnings.XXXXXX")
trap 'rm -rf "$work_root"' EXIT
log_file="$work_root/command.log"

if ! "$@" >"$log_file" 2>&1; then
  printf '%s failed\n' "$description" >&2
  cat "$log_file" >&2
  exit 1
fi

if grep -E '(^|[[:space:]:])warning:' "$log_file" >/dev/null 2>&1; then
  printf '%s emitted warnings\n' "$description" >&2
  cat "$log_file" >&2
  exit 1
fi

cat "$log_file"
