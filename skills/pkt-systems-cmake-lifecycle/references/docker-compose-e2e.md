# Docker Compose E2E

## Docker Compose E2E

Use Docker Compose for deterministic local services unless in-process fixtures are clearly enough.

Required layout for compose-backed e2e:

```text
docker-compose.yaml
scripts/compose.sh
scripts/dev-up.sh
scripts/dev-down.sh
scripts/dev-reset.sh
scripts/dev-ps.sh
scripts/dev-logs.sh
scripts/test-e2e.sh
devenv/
devenv/volumes/
docker/
```

Rules:

- Use root `docker-compose.yaml`.
- Set `name: <project>-e2e`.
- `scripts/compose.sh` is the only Make-facing compose entry point.
- The wrapper chooses `nerdctl compose` first, then `docker compose`, always passing the root compose file.
- Host ports are project-prefixed environment variables with safe defaults.
- Mutable service state lives under `devenv/volumes`.
- Static source-controlled service config may live under `docker/` and should be mounted read-only where possible.
- Generated certificates, local credentials, logs, databases, object-store data, queue state, and service state are generated and ignored.
- Use pinned image tags or a documented digest policy.
- Avoid privileged containers, host networking, broad host mounts, fixed container names, and real credentials unless explicitly accepted.
- Use idempotent one-shot bootstrap services for buckets, users, certificates, schemas, or service config.

`scripts/compose.sh` shape:

```sh
#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=${script_dir%/scripts}
compose_file="$repo_root/docker-compose.yaml"

if command -v nerdctl >/dev/null 2>&1; then
  exec nerdctl compose -f "$compose_file" "$@"
fi

if command -v docker >/dev/null 2>&1; then
  exec docker compose -f "$compose_file" "$@"
fi

printf '%s\n' "Neither nerdctl nor docker is available in PATH." >&2
exit 1
```

`dev-up` creates state directories, starts services detached, runs bootstrap jobs, and waits for readiness. Readiness must be explicit: HTTP polling, TCP polling, Unix socket existence, generated file existence, bootstrap exit status, or command probes. `depends_on` alone is not enough.

`test-e2e` builds or verifies the debug/e2e preset, starts or checks services, creates a temporary work directory, starts any local example daemons under process groups when possible, waits for readiness, runs success paths, runs expected failure paths, runs language facade examples when relevant, and cleans up child processes and services unless a documented keep flag is set.

Compose-backed e2e may cover HTTP test sinks, reverse proxies, TLS terminators, object stores, local certificate authorities, SSH/SFTP, MQTT, search, queues, databases, caches, Unix socket services, or project daemons.

Real cloud services and real credentials belong under `test-integration`, require explicit opt-in environment variables, and are excluded from `world` unless the engineer explicitly makes them mandatory.

Local e2e automation policy:

- E2E is local agent-run automation. Do not require or assume remote CI.
- Use project-prefixed environment variables for every host port and endpoint so parallel checkouts can avoid collisions.
- Default ports should be high, non-privileged, and documented by `make help` or `dev-up` output.
- `dev-up` must print the effective endpoints, generated credential locations, and service state root.
- `test-e2e` should either allocate ephemeral ports for project-owned local daemons or derive low-collision defaults from the process ID. Compose service host ports remain overrideable through project-prefixed environment variables.
- On failure, e2e scripts must print enough diagnostics to debug without rerunning blindly: failed command, service endpoint, relevant server logs, compose service state, and compose logs for involved services.
- `test-e2e` must clean child processes and temporary work directories on exit. It must clean compose services when it started them unless a documented keep flag is set.
- Provide a keep flag such as `<P>_E2E_KEEP_DEVSERVICES=1` for debugging service failures.
- `dev-reset` must remove generated service state under `devenv/volumes` but must not remove source-controlled config or placeholders.
- Local CI/CD may include deterministic e2e in `test-all` or release gates when it is reliable and bounded. If e2e is slow or requires optional services, keep it as an explicit `make test-e2e` release gate rather than a default fast-test dependency.


