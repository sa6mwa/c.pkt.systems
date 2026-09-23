#!/usr/bin/env bash
set -euo pipefail

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
validator=${CODEX_HOME:-"${HOME:?HOME must be set}/.codex"}/skills/.system/skill-creator/scripts/quick_validate.py
[ -f "$validator" ] || { printf 'Skill validator is missing: %s\n' "$validator" >&2; exit 1; }

mkdir -p "$repo_root/build"
test_root=$(mktemp -d "$repo_root/build/skill-installer-test.XXXXXXXXXX")
trap 'rm -rf -- "$test_root"' EXIT

source_root="$test_root/source"
codex_home="$test_root/codex"
skills_root="$codex_home/skills"
mkdir -p "$source_root" "$skills_root/.system/skill-creator/scripts"
cp "$repo_root/skills/install-pkt-systems-cmake-lifecycle.sh" "$source_root/"
cp -R "$repo_root/skills/pkt-systems-cmake-lifecycle" "$source_root/"
cp -R "$repo_root/skills/codex-review-and-fix-loop" "$source_root/"
cp "$validator" "$skills_root/.system/skill-creator/scripts/quick_validate.py"

mkdir -p "$skills_root/pkt-systems-cmake-lifecycle"
printf 'obsolete\n' > "$skills_root/pkt-systems-cmake-lifecycle/obsolete.txt"
CODEX_HOME="$codex_home" bash "$source_root/install-pkt-systems-cmake-lifecycle.sh" > "$test_root/install.log"
for skill_name in pkt-systems-cmake-lifecycle codex-review-and-fix-loop; do
  diff -qr "$source_root/$skill_name" "$skills_root/$skill_name"
done
[ ! -e "$skills_root/pkt-systems-cmake-lifecycle/obsolete.txt" ] || {
  printf 'Installer retained obsolete installed content\n' >&2
  exit 1
}
CODEX_HOME="$codex_home" bash "$source_root/install-pkt-systems-cmake-lifecycle.sh" > "$test_root/reinstall.log"
for skill_name in pkt-systems-cmake-lifecycle codex-review-and-fix-loop; do
  diff -qr "$source_root/$skill_name" "$skills_root/$skill_name"
done

# A preflight failure must leave both existing installations untouched.
printf 'keep\n' > "$skills_root/pkt-systems-cmake-lifecycle/keep.txt"
mv "$skills_root/codex-review-and-fix-loop" "$test_root/review-installed"
ln -s "$test_root/review-installed" "$skills_root/codex-review-and-fix-loop"
if CODEX_HOME="$codex_home" bash "$source_root/install-pkt-systems-cmake-lifecycle.sh" > "$test_root/preflight.log" 2>&1; then
  printf 'Installer accepted a symlinked destination\n' >&2
  exit 1
fi
[ -f "$skills_root/pkt-systems-cmake-lifecycle/keep.txt" ] || {
  printf 'Preflight failure changed the lifecycle installation\n' >&2
  exit 1
}
[ -L "$skills_root/codex-review-and-fix-loop" ] || {
  printf 'Preflight failure changed the review installation\n' >&2
  exit 1
}

# A failure after replacing both skills must restore their prior contents.
rm "$skills_root/codex-review-and-fix-loop"
mv "$test_root/review-installed" "$skills_root/codex-review-and-fix-loop"
printf 'keep\n' > "$skills_root/codex-review-and-fix-loop/keep.txt"
mkdir -p "$test_root/fake-bin"
cat > "$test_root/fake-bin/diff" <<'EOF'
#!/usr/bin/env bash
if [ "${3:-}" = "$CPKT_TEST_CODEX_HOME/skills/codex-review-and-fix-loop" ]; then
  exit 1
fi
exec /usr/bin/diff "$@"
EOF
chmod +x "$test_root/fake-bin/diff"
if CPKT_TEST_CODEX_HOME="$codex_home" CODEX_HOME="$codex_home" PATH="$test_root/fake-bin:$PATH" \
    bash "$source_root/install-pkt-systems-cmake-lifecycle.sh" > "$test_root/rollback.log" 2>&1; then
  printf 'Installer accepted a failed installed-copy check\n' >&2
  exit 1
fi
for skill_name in pkt-systems-cmake-lifecycle codex-review-and-fix-loop; do
  [ -f "$skills_root/$skill_name/keep.txt" ] || {
    printf 'Installer did not restore %s after failure\n' "$skill_name" >&2
    exit 1
  }
done
if find "$skills_root" -maxdepth 1 -name '.*.backup.*' -print -quit | rg -q .; then
  printf 'Installer left a backup after rollback\n' >&2
  exit 1
fi

printf '[test] both lifecycle skills install and rollback correctly\n'
