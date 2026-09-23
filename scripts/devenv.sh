#!/usr/bin/env bash
set -Eeuo pipefail

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
state_root="$repo_root/build/devenv"
manifest="$state_root/devenv.yaml"
checkout_id=$(printf '%s' "$repo_root" | sha256sum | cut -c1-10)
postgres_pod="cpkt-pg-$checkout_id"
cockroach_pod="cpkt-crdb-$checkout_id"
postgres_port=${CPKT_DEV_POSTGRES_PORT:-55432}
cockroach_port=${CPKT_DEV_COCKROACH_PORT:-56257}

usage() {
  printf 'usage: %s {render|up|down|reset|ps|logs|is-up}\n' "$0" >&2
  exit 2
}

require_podman() {
  command -v podman >/dev/null || { printf 'rootless Podman is required\n' >&2; exit 1; }
  [[ $(podman info --format '{{.Host.Security.Rootless}}') == true ]] || {
    printf 'Podman must run rootlessly as the invoking user\n' >&2
    exit 1
  }
}

render() {
  local port
  for port in "$postgres_port" "$cockroach_port"; do
    [[ $port =~ ^[0-9]+$ ]] && (( port >= 1024 && port <= 65535 )) || {
      printf 'local database ports must be numeric and between 1024 and 65535\n' >&2
      exit 2
    }
  done
  [[ $postgres_port != "$cockroach_port" ]] || { printf 'database ports must differ\n' >&2; exit 2; }
  mkdir -p "$state_root/state/postgres" "$state_root/state/cockroach" "$state_root/tmp" "$state_root/logs"
  python3 - "$repo_root/devenv.yaml.in" "$manifest" \
    "$postgres_pod" "$cockroach_pod" "$postgres_port" "$cockroach_port" \
    "$state_root/state/postgres" "$state_root/state/cockroach" <<'PY'
import pathlib
import sys

source, destination, pg_pod, crdb_pod, pg_port, crdb_port, pg_state, crdb_state = sys.argv[1:]
content = pathlib.Path(source).read_text()
for key, value in {
    'CPKT_POSTGRES_POD': pg_pod,
    'CPKT_COCKROACH_POD': crdb_pod,
    'CPKT_POSTGRES_PORT': pg_port,
    'CPKT_COCKROACH_PORT': crdb_port,
    'CPKT_POSTGRES_STATE': pg_state,
    'CPKT_COCKROACH_STATE': crdb_state,
}.items():
    content = content.replace(key, value)
path = pathlib.Path(destination)
path.write_text(content)
path.chmod(0o600)
PY
  printf '%s\n' "$manifest"
}

readiness() {
  local label=$1 container=$2
  shift 2
  for (( attempt=0; attempt<60; attempt++ )); do
    if podman exec "$container" "$@" >/dev/null 2>&1; then
      printf '[devenv] %s ready\n' "$label"
      return 0
    fi
    sleep 1
  done
  printf '[devenv] %s did not become ready; pod logs follow\n' "$label" >&2
  podman logs "$container" >&2 || true
  return 1
}

is_up() {
  [[ -f $manifest ]] &&
      grep -Fq "hostPort: $postgres_port" "$manifest" &&
      grep -Fq "hostPort: $cockroach_port" "$manifest" &&
      podman pod exists "$postgres_pod" && podman pod exists "$cockroach_pod" &&
      [[ $(podman pod inspect "$postgres_pod" --format '{{.State}}') == Running ]] &&
      [[ $(podman pod inspect "$cockroach_pod" --format '{{.State}}') == Running ]]
}

up() {
  require_podman
  if is_up; then
    printf '[devenv] existing pods %s and %s\n' "$postgres_pod" "$cockroach_pod"
  else
    if [[ -f $manifest ]]; then
      podman kube down "$manifest" >/dev/null
    fi
    render
    podman kube play --userns=keep-id:uid=70,gid=70 "$manifest"
  fi
  readiness postgresql "$postgres_pod-postgres" pg_isready -h 127.0.0.1 -U postgres
  readiness cockroachdb "$cockroach_pod-cockroach" /cockroach/cockroach sql --insecure --host=127.0.0.1:26257 --execute='SELECT 1'
  printf '[devenv] PostgreSQL 127.0.0.1:%s; CockroachDB 127.0.0.1:%s; state %s\n' \
    "$postgres_port" "$cockroach_port" "$state_root"
}

down() {
  require_podman
  if [[ -f $manifest ]]; then
    podman kube down "$manifest"
  fi
  if podman pod exists "$postgres_pod" || podman pod exists "$cockroach_pod"; then
    printf '[devenv] database pods remain after shutdown\n' >&2
    return 1
  fi
}

case ${1:-} in
  render) render ;;
  up) up ;;
  down) down ;;
  reset) down; rm -rf -- "$state_root" ;;
  ps) require_podman; podman pod ps --filter "name=cpkt-.*-$checkout_id" ;;
  logs) require_podman; podman logs "$postgres_pod-postgres"; podman logs "$cockroach_pod-cockroach" ;;
  is-up) require_podman; is_up ;;
  *) usage ;;
esac
