#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  printf 'usage: package_assertions_cleanup_test.sh <source-dir>\n' >&2
  exit 2
fi

source_dir=$1
assertion_parent="$source_dir/.cache/package-assertions"
work_dir=$(mktemp -d)
trap 'rm -rf -- "$work_dir"' EXIT

if [[ -d "$assertion_parent" ]]; then
  parent_existed=1
  before=$(find "$assertion_parent" -mindepth 1 -printf '%P\n' | LC_ALL=C sort)
else
  parent_existed=0
  before=
fi
legacy_before=$(find "$source_dir" -mindepth 1 -maxdepth 1 -type d -name 'package-assertions-*' -printf '%f\n' | LC_ALL=C sort)

archive_root='c.pkt.systems-0.0.0-x86_64-linux-gnu'
mkdir -p "$work_dir/payload/$archive_root"
archive="$work_dir/c.pkt.systems-0.0.0-x86_64-linux-gnu.tar.gz"
tar --owner=0 --group=0 --numeric-owner \
  -C "$work_dir/payload" \
  -czf "$archive" \
  "$archive_root"
: > "$work_dir/c.pkt.systems-0.0.0-CHECKSUMS"

if output=$(bash "$source_dir/scripts/run-package-assertions.sh" \
    -DCPKT_ARCHIVE="$archive" \
    -DCPKT_TARGET_ID=x86_64-linux-gnu \
    -DCPKT_BUNDLE_VERSION=0.0.0 2>&1); then
  printf 'package assertions accepted an archive without required metadata\n%s\n' "$output" >&2
  exit 1
fi

case "$output" in
  *'missing package manifest:'*) ;;
  *)
    printf 'package assertions cleanup regression did not reach extraction failure\n%s\n' "$output" >&2
    exit 1
    ;;
esac

if [[ "$parent_existed" -eq 1 ]]; then
  after=$(find "$assertion_parent" -mindepth 1 -printf '%P\n' | LC_ALL=C sort)
  if [[ "$before" != "$after" ]]; then
    printf 'package assertion workspace contents changed after a failed assertion\n' >&2
    exit 1
  fi
elif [[ -e "$assertion_parent" ]]; then
  printf 'failed package assertions left workspace parent: %s\n' "$assertion_parent" >&2
  exit 1
fi

legacy_after=$(find "$source_dir" -mindepth 1 -maxdepth 1 -type d -name 'package-assertions-*' -printf '%f\n' | LC_ALL=C sort)
if [[ "$legacy_before" != "$legacy_after" ]]; then
  printf 'failed package assertions left a repository-root extraction directory\n' >&2
  exit 1
fi

mkdir -p "$work_dir/bin"
cat > "$work_dir/bin/cmake" <<EOF
#!/usr/bin/env bash
for arg in "\$@"; do
  case "\$arg" in
    -DCPKT_ASSERTION_WORK_ROOT=*)
      printf '%s\\n' "\${arg#-DCPKT_ASSERTION_WORK_ROOT=}" > '$work_dir/signalled-workspace'
      ;;
  esac
done
kill -TERM "\$PPID"
exit 0
EOF
chmod +x "$work_dir/bin/cmake"

if output=$(PATH="$work_dir/bin:$PATH" bash "$source_dir/scripts/run-package-assertions.sh" \
    -DCPKT_ARCHIVE="$archive" \
    -DCPKT_TARGET_ID=x86_64-linux-gnu \
    -DCPKT_BUNDLE_VERSION=0.0.0 2>&1); then
  printf 'package assertion wrapper reported success after TERM\n%s\n' "$output" >&2
  exit 1
fi

if [[ ! -f "$work_dir/signalled-workspace" ]]; then
  printf 'signal regression did not capture the package assertion workspace\n' >&2
  exit 1
fi
signalled_workspace=$(<"$work_dir/signalled-workspace")
if [[ -e "$signalled_workspace" ]]; then
  printf 'terminated package assertions left workspace: %s\n' "$signalled_workspace" >&2
  exit 1
fi

clean_fixture="$work_dir/clean-fixture"
mkdir -p \
  "$clean_fixture/scripts" \
  "$clean_fixture/build" \
  "$clean_fixture/.cache" \
  "$clean_fixture/dist" \
  "$clean_fixture/package-assertions-stale"
cp "$source_dir/scripts/clean.sh" "$clean_fixture/scripts/clean.sh"
bash "$clean_fixture/scripts/clean.sh" all
for removed_path in build .cache dist package-assertions-stale; do
  if [[ -e "$clean_fixture/$removed_path" ]]; then
    printf 'clean left generated package assertion state: %s\n' "$removed_path" >&2
    exit 1
  fi
done

printf '[test] package assertion workspace cleanup passed\n'
