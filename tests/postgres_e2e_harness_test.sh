#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
set +e
output=$(env -u CPKT_POSTGRES_E2E_CONNINFO -u CPKT_COCKROACH_E2E_CONNINFO \
  bash "$repo_root/scripts/e2e-postgres.sh" /bin/true 2>&1)
status=$?
set -e

if [[ $status -eq 0 ]]; then
  printf 'PostgreSQL e2e harness accepted missing connection strings\n' >&2
  exit 1
fi
if [[ $output != *CPKT_POSTGRES_E2E_CONNINFO* ]]; then
  printf 'PostgreSQL e2e harness did not identify the missing PostgreSQL input\n' >&2
  exit 1
fi
