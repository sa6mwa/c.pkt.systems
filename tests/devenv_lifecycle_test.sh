#!/usr/bin/env bash
set -euo pipefail

repo_root=${1:?repository root is required}
fixture=$(mktemp -d "$repo_root/build/devenv-lifecycle.XXXXXX")
trap 'cmake -E remove_directory "$fixture"' EXIT
mkdir -p "$fixture/scripts" "$fixture/bin" "$fixture/pods"
cp "$repo_root/scripts/devenv.sh" "$fixture/scripts/devenv.sh"
cp "$repo_root/scripts/clean.sh" "$fixture/scripts/clean.sh"
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
    while read -r pod; do
      cmake -E rm -f "$CPKT_MOCK_PODS/$pod"
    done < <(awk '/^  name: cpkt-/{print $2}' "${@: -1}")
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

bash "$devenv" up >/dev/null
mkdir -p "$fixture/.cache" "$fixture/dist"
: > "$fixture/.cache/keep-on-teardown-failure"
: > "$fixture/dist/keep-on-teardown-failure"
if CPKT_MOCK_POD_RM_FAIL=1 bash "$fixture/scripts/clean.sh" all >/dev/null 2>&1; then
  printf 'clean succeeded while database pods remained running\n' >&2
  exit 8
fi
if [[ ! -d $fixture/build/devenv || ! -f $fixture/.cache/keep-on-teardown-failure ||
      ! -f $fixture/dist/keep-on-teardown-failure ]] ||
    ! find "$CPKT_MOCK_PODS" -type f | grep -q .; then
  printf 'clean deleted generated state after pod teardown failed\n' >&2
  exit 9
fi
bash "$fixture/scripts/clean.sh" all
if [[ -e $fixture/build || -e $fixture/.cache || -e $fixture/dist ]] ||
    find "$CPKT_MOCK_PODS" -type f | grep -q .; then
  printf 'clean left database pods or generated state behind\n' >&2
  exit 10
fi
