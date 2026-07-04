#!/usr/bin/env sh
set -eu

if [ "$#" -lt 1 ]; then
  printf 'usage: static_executable_policy_test.sh <binary>...\n' >&2
  exit 2
fi

for binary in "$@"; do
  if [ ! -x "$binary" ]; then
    printf 'static executable policy: missing executable: %s\n' "$binary" >&2
    exit 1
  fi

  file_output=$(file "$binary")
  case "$file_output" in
    *"statically linked"*) ;;
    *)
      printf 'static executable policy: not statically linked: %s\n' "$binary" >&2
      printf '%s\n' "$file_output" >&2
      exit 1
      ;;
  esac

  ldd_output=$(ldd "$binary" 2>&1 || true)
  case "$ldd_output" in
    *"not a dynamic executable"*) ;;
    *)
      printf 'static executable policy: ldd found dynamic dependencies: %s\n' "$binary" >&2
      printf '%s\n' "$ldd_output" >&2
      exit 1
      ;;
  esac
done
