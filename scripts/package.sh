#!/usr/bin/env bash
set -Eeuo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

CMAKE=${CMAKE:-cmake}
CTEST=${CTEST:-ctest}

release_presets="x86_64-linux-gnu-release x86_64-linux-musl-release aarch64-linux-gnu-release aarch64-linux-musl-release armhf-linux-gnu-release armhf-linux-musl-release"

phase=preflight
preset=arm64-apple-darwin-release

package_phase() {
  phase=$1
  printf '[package] target=%s phase=%s started pid=%s elapsed=%ss\n' \
    "$preset" "$phase" "$$" "$SECONDS" >&2
}

package_failure() {
  local status=$1 command=$2 signal_name
  trap - ERR
  printf '[package] FAILED target=%s phase=%s status=%s pid=%s elapsed=%ss command=%s\n' \
    "$preset" "$phase" "$status" "$$" "$SECONDS" "$command" >&2
  if (( status > 128 )) && signal_name=$(kill -l "$status" 2>/dev/null); then
    printf '[package] Status %s can represent SIG%s or an explicit exit(%s); exit status alone does not identify a signal sender. See README release diagnostics.\n' \
      "$status" "${signal_name#SIG}" "$status" >&2
  fi
  exit "$status"
}

package_interrupted() {
  trap '' HUP INT TERM
  trap - ERR
  printf '[package] INTERRUPTED target=%s phase=%s received %s status=%s pid=%s elapsed=%ss; sender identity is unavailable to a shell trap. See README release diagnostics.\n' \
    "$preset" "$phase" "$1" "$2" "$$" "$SECONDS" >&2
  exit "$2"
}

trap 'package_failure "$?" "$BASH_COMMAND"' ERR
trap 'package_interrupted SIGHUP 129' HUP
trap 'package_interrupted SIGINT 130' INT
trap 'package_interrupted SIGTERM 143' TERM

cd "$repo_root"

package_phase preflight
if ! bash "$repo_root/scripts/osxcross_available.sh"; then
  printf '[package] arm64-apple-darwin-release is required for c.pkt.systems releases; configure a complete local osxcross SDK toolchain\n' >&2
  exit 1
fi

for preset in $release_presets; do
  package_phase configure
  "$CMAKE" --preset "$preset"
  package_phase build
  "$CMAKE" --build --preset "$preset"
  package_phase test
  "$CTEST" --preset "$preset" --verbose
  package_phase package
  "$CMAKE" --build --preset "package-$preset"
done

preset=arm64-apple-darwin-release
package_phase configure
"$CMAKE" --preset arm64-apple-darwin-release
package_phase build
"$CMAKE" --build --preset arm64-apple-darwin-release
package_phase package
"$CMAKE" --build --preset package-arm64-apple-darwin-release
printf '[package] All target build, test, and package phases passed\n'
