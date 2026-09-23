#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  printf 'usage: %s POSTGRES-INTEGRATION-EXECUTABLE\n' "$0" >&2
  exit 2
fi

: "${CPKT_POSTGRES_E2E_CONNINFO:?set a PostgreSQL libpq connection string}"
: "${CPKT_COCKROACH_E2E_CONNINFO:?set a CockroachDB libpq connection string}"

executable=$1
runner=("$executable")
if [[ ${CPKT_POSTGRES_E2E_MEMCHECK:-0} == 1 ]]; then
  repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
  runner=(valgrind --error-exitcode=1 --leak-check=full --track-origins=yes
    --show-leak-kinds=definite,indirect
    "--suppressions=$repo_root/tests/valgrind.supp" "$executable")
fi
"${runner[@]}" postgresql "$CPKT_POSTGRES_E2E_CONNINFO"
"${runner[@]}" cockroachdb "$CPKT_COCKROACH_E2E_CONNINFO"
printf '[e2e-postgres] PostgreSQL and CockroachDB passed\n'
