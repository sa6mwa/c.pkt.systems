#!/usr/bin/env bash
set -euo pipefail

repo_root=${1:?repository root is required}
# The miniature repository gets its own lock. Close inherited capabilities;
# the parent process continues holding the real repository operation lock.
for inherited in CPKT_OPERATION_FD CPKT_OPERATION_CAP_FD; do
  descriptor=${!inherited:-}
  if [[ $descriptor =~ ^[0-9]+$ ]]; then exec {descriptor}>&-; fi
done
for key in ${!CPKT_OPERATION_@}; do unset "$key"; done
fixture=$(mktemp -d "$repo_root/build/devenv-lifecycle.XXXXXX")
trap 'cmake -E remove_directory "$fixture"' EXIT
mkdir -p "$fixture/scripts" "$fixture/bin" "$fixture/pods"
cp "$repo_root/scripts/devenv.sh" "$fixture/scripts/devenv.sh"
cp "$repo_root/scripts/clean.sh" "$fixture/scripts/clean.sh"
cp "$repo_root"/scripts/cpkt_*.py "$fixture/scripts/"
cp "$repo_root/scripts/group-build.py" "$fixture/scripts/"
mkdir -p "$fixture/cmake"
cp "$repo_root/cmake/components.json" "$fixture/cmake/"
cp "$repo_root/CMakePresets.json" "$fixture/"
cp "$repo_root/scripts/test-e2e.sh" "$fixture/scripts/test-e2e.sh"
cp "$repo_root/scripts/e2e-postgres.sh" "$fixture/scripts/e2e-postgres.sh"
cp "$repo_root/devenv.yaml.in" "$fixture/devenv.yaml.in"

cat > "$fixture/bin/podman" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
printf '%s\n' "$*" >> "$CPKT_MOCK_PODMAN_LOG"
case "$1 $2" in
  'info --format') printf 'true\n' ;;
  'pod exists') test -f "$CPKT_MOCK_PODS/$3" ;;
  'pod inspect') printf 'Running\n' ;;
  'pod rm')
    [[ ${CPKT_MOCK_POD_RM_FAIL:-} != 1 ]] || exit 1
    cmake -E rm -f "$CPKT_MOCK_PODS/${@: -1}"
    ;;
  'kube play')
    while read -r pod; do
      touch "$CPKT_MOCK_PODS/$pod"
    done < <(awk '/^  name: cpkt-/{print $2}' "${@: -1}")
    ;;
  'kube down')
    [[ ${CPKT_MOCK_POD_RM_FAIL:-} != 1 ]] || exit 1
    had_pods=0
    while read -r pod; do
      if [[ -f $CPKT_MOCK_PODS/$pod ]]; then had_pods=1; fi
      cmake -E rm -f "$CPKT_MOCK_PODS/$pod"
    done < <(awk '/^  name: cpkt-/{print $2}' "${@: -1}")
    if [[ ${CPKT_MOCK_KUBE_DOWN_FAIL:-} == 1 && $had_pods == 1 ]]; then exit 1; fi
    ;;
  'exec '*) ;;
  *) printf 'unexpected mock Podman command: %s\n' "$*" >&2; exit 1 ;;
esac
SH
chmod +x "$fixture/bin/podman"
export PATH="$fixture/bin:$PATH"
export CPKT_MOCK_PODS="$fixture/pods"
export CPKT_MOCK_PODMAN_LOG="$fixture/podman.log"
unset CPKT_DEV_POSTGRES_PORT CPKT_DEV_COCKROACH_PORT
devenv="$fixture/scripts/devenv.sh"
manifest="$fixture/build/devenv/devenv.yaml"

bash "$devenv" up >/dev/null
bash "$devenv" is-up
if ! grep -q '^kube play --network=pasta ' "$CPKT_MOCK_PODMAN_LOG"; then
  printf 'devenv did not select per-pod rootless networking\n' >&2
  exit 11
fi
if CPKT_DEV_POSTGRES_PORT=5543 bash "$devenv" is-up; then
  printf 'devenv accepted a partial port match\n' >&2
  exit 1
fi
if CPKT_DEV_POSTGRES_PORT=56257 CPKT_DEV_COCKROACH_PORT=55432 \
    bash "$devenv" is-up; then
  printf 'devenv accepted swapped service ports\n' >&2
  exit 2
fi
CPKT_DEV_POSTGRES_PORT=56257 CPKT_DEV_COCKROACH_PORT=55432 \
  bash "$devenv" up >/dev/null
python3 - "$manifest" <<'PY'
import pathlib
import sys

pods = pathlib.Path(sys.argv[1]).read_text().split('---')
if len(pods) != 2 or 'hostPort: 56257' not in pods[0] or 'hostPort: 55432' not in pods[1]:
    raise SystemExit('devenv did not assign ports to the intended pods')
PY
if [[ $(grep -c '^kube play ' "$CPKT_MOCK_PODMAN_LOG") != 2 ]]; then
  printf 'devenv did not replace pods after changing ports\n' >&2
  exit 3
fi

cmake -E rm -f "$manifest"
bash "$devenv" down
if find "$CPKT_MOCK_PODS" -type f | grep -q .; then
  printf 'devenv left pods running after the manifest was removed\n' >&2
  exit 4
fi
if [[ $(grep -c '^pod rm --force ' "$CPKT_MOCK_PODMAN_LOG") != 2 ]]; then
  printf 'devenv did not remove both checkout-owned pods by name\n' >&2
  exit 5
fi

bash "$devenv" up >/dev/null
cmake -E rm -f "$manifest"
bash "$devenv" up >/dev/null
if [[ $(grep -c '^kube play ' "$CPKT_MOCK_PODMAN_LOG") != 4 ]]; then
  printf 'devenv did not replace stale pods when the manifest was missing\n' >&2
  exit 6
fi
bash "$devenv" reset
if [[ -e $fixture/build/devenv ]] || find "$CPKT_MOCK_PODS" -type f | grep -q .; then
  printf 'devenv reset left state or pods behind\n' >&2
  exit 7
fi

# Even when Podman removes both pods, a teardown error must fail the public
# e2e command rather than disappear behind a successful integration result.
if CPKT_MOCK_KUBE_DOWN_FAIL=1 bash "$fixture/scripts/test-e2e.sh" /bin/true \
    >"$fixture/e2e.log" 2>&1; then
  printf 'e2e succeeded after Podman reported a teardown error\n' >&2
  exit 12
fi
if find "$CPKT_MOCK_PODS" -type f | grep -q .; then
  printf 'e2e did not finish removing pods after a teardown error\n' >&2
  exit 13
fi
grep -q 'failed to stop database pods' "$fixture/e2e.log"
grep -q 'PostgreSQL and CockroachDB passed' "$fixture/e2e.log"

bash "$devenv" up >/dev/null
mkdir -p "$fixture/.cache" "$fixture/dist"
: > "$fixture/.cache/keep-on-teardown-failure"
: > "$fixture/dist/keep-on-teardown-failure"
clean_fixture() (
  for key in ${!CPKT_OPERATION_@}; do unset "$key"; done
  export GROUP=all PRESET=debug
  bash "$fixture/scripts/clean.sh" all
)
if CPKT_MOCK_POD_RM_FAIL=1 clean_fixture >/dev/null 2>&1; then
  printf 'clean succeeded while database pods remained running\n' >&2
  exit 8
fi
if [[ ! -d $fixture/build/devenv || ! -f $fixture/.cache/keep-on-teardown-failure ||
      ! -f $fixture/dist/keep-on-teardown-failure ]] ||
    ! find "$CPKT_MOCK_PODS" -type f | grep -q .; then
  printf 'clean deleted generated state after pod teardown failed\n' >&2
  exit 9
fi
clean_fixture
if [[ ! -f $fixture/build/control/operation.lock || -e $fixture/.cache || -e $fixture/dist ]] ||
    [[ $(find "$fixture/build" -mindepth 1 -maxdepth 1 -printf '%f\n') != control ]] ||
    find "$CPKT_MOCK_PODS" -type f | grep -q .; then
  printf 'clean left database pods or generated state behind\n' >&2
  exit 10
fi
