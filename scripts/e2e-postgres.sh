#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  printf 'usage: %s POSTGRES-INTEGRATION-EXECUTABLE\n' "$0" >&2
  exit 2
fi

: "${CPKT_POSTGRES_E2E_CONNINFO:?set a PostgreSQL libpq connection string}"
: "${CPKT_COCKROACH_E2E_CONNINFO:?set a CockroachDB libpq connection string}"

executable=$1
"$executable" postgresql "$CPKT_POSTGRES_E2E_CONNINFO"
"$executable" cockroachdb "$CPKT_COCKROACH_E2E_CONNINFO"
printf '[e2e-postgres] PostgreSQL and CockroachDB passed\n'
