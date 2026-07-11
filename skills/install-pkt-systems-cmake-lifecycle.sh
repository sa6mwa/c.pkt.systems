#!/usr/bin/env bash
set -euo pipefail

skill_name=pkt-systems-cmake-lifecycle
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
skill_source="$script_dir/$skill_name"
codex_home=${CODEX_HOME:-"${HOME:?HOME must be set}/.codex"}
skills_root="$codex_home/skills"
skill_destination="$skills_root/$skill_name"
validator="$skills_root/.system/skill-creator/scripts/quick_validate.py"
staging_dir=
backup_dir=
restore_backup=0
installed_destination=0

cleanup() {
  status=$?

  if [ -n "$staging_dir" ] && [ -d "$staging_dir" ]; then
    rm -rf "$staging_dir"
  fi
  if [ "$restore_backup" -eq 1 ] && [ -n "$backup_dir" ] && [ -d "$backup_dir" ]; then
    rm -rf "$skill_destination"
    mv "$backup_dir" "$skill_destination"
  elif [ "$installed_destination" -eq 1 ] && [ -d "$skill_destination" ]; then
    rm -rf "$skill_destination"
  fi

  exit "$status"
}
trap cleanup EXIT HUP INT TERM

fail() {
  printf '%s\n' "$*" >&2
  exit 1
}

validate_skill() {
  skill_path=$1
  if ! python3 "$validator" "$skill_path"; then
    fail "Codex skill validation failed for: $skill_path"
  fi
}

verify_installed_skill() {
  if ! diff -qr "$skill_source" "$skill_destination"; then
    fail "Installed skill does not match repository source: $skill_destination"
  fi
}

[ -d "$skill_source" ] || fail "Skill source directory not found: $skill_source"
[ -f "$validator" ] || fail "Codex skill validator not found: $validator"

validate_skill "$skill_source"

mkdir -p "$skills_root"
if [ -L "$skill_destination" ]; then
  fail "Refusing to replace symlinked skill destination: $skill_destination"
fi
if [ -e "$skill_destination" ] && [ ! -d "$skill_destination" ]; then
  fail "Skill destination is not a directory: $skill_destination"
fi

staging_dir=$(mktemp -d "$skills_root/.${skill_name}.staging.XXXXXXXXXX")
cp -R "$skill_source/." "$staging_dir/"
validate_skill "$staging_dir"

if [ -d "$skill_destination" ]; then
  backup_dir=$(mktemp -d "$skills_root/.${skill_name}.backup.XXXXXXXXXX")
  rmdir "$backup_dir"
  mv "$skill_destination" "$backup_dir"
  restore_backup=1
fi

mv "$staging_dir" "$skill_destination"
staging_dir=
installed_destination=1

verify_installed_skill

if [ "$restore_backup" -eq 1 ]; then
  rm -rf "$backup_dir"
  backup_dir=
  restore_backup=0
fi
installed_destination=0

printf 'Installed %s to %s\n' "$skill_name" "$skill_destination"
