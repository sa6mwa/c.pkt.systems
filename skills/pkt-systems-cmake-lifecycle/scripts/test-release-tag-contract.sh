#!/usr/bin/env bash
set -euo pipefail

skill_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
release_ref="$skill_dir/references/release.md"
local_ci_ref="$skill_dir/references/local-ci.md"

# Reviewer note: the lifecycle intentionally requires downstream tests to
# mutate the checkout under test with a reserved temporary lightweight tag.
# Exact release-version discovery must be proven against the same repository
# metadata that CMake, Make, and packaging see during a real release. A copied
# fixture, extra checkout, or git worktree can mask the active HEAD contract and
# move failures into late release packaging.

fail() {
  printf 'test-release-tag-contract: %s\n' "$*" >&2
  exit 1
}

require_text() {
  local file=$1 text=$2 description=$3
  grep -Fq -- "$text" "$file" || fail "$file is missing lifecycle contract: $description"
}

require_text "$release_ref" \
  'Tag-dependent version and manifest contract tests are ordinary tests, not late package-verification tests.' \
  'tag-dependent checks run before package verification'
require_text "$release_ref" \
  'before `release-matrix`' \
  'tag-dependent checks are pre-matrix'
require_text "$release_ref" \
  'temporary lightweight semver tag such as `v99.99.99` on the current `HEAD`' \
  'temporary lightweight tag pattern'
require_text "$release_ref" \
  'exact tag on `HEAD`' \
  'release metadata must be tested on active HEAD'
require_text "$release_ref" \
  'delete any stale copy before untagged assertions, recreate it only inside the tagged assertion block, and delete it again before continuing' \
  'reserved test tag stale cleanup'
require_text "$release_ref" \
  'If `HEAD` already has an exact release tag, assert that the exact tag wins and skip the temporary-tag block.' \
  'already-tagged HEAD behavior'
require_text "$release_ref" \
  'Do not use extra git checkouts, git worktrees, copied repositories, or generated source archives' \
  'heavyweight tag simulation is prohibited'

require_text "$local_ci_ref" \
  'Lightweight-tag version checks must be early ordinary tests in `test`, `test-all`, `prerelease`, and therefore the pre-matrix portion of `release`.' \
  'local CI timing for tag checks'
require_text "$local_ci_ref" \
  'for example `v99.99.99`' \
  'documented temporary test tag value'
require_text "$local_ci_ref" \
  'removes the tag with a trap' \
  'documented cleanup mechanism'
require_text "$local_ci_ref" \
  'remove any stale copy before untagged assertions, recreate it only inside the tagged assertion block, and delete it again before later assertions continue' \
  'local CI stale reserved tag cleanup'
require_text "$local_ci_ref" \
  'When `HEAD` already has an exact release tag, test that the exact tag takes precedence and skip the temporary-tag block.' \
  'local CI already-tagged HEAD behavior'
require_text "$local_ci_ref" \
  'Do not use extra git checkouts, git worktrees, copied repositories, or source-archive staging as a substitute' \
  'local CI prohibits heavyweight substitute topology'

printf 'release tag lifecycle contract tests passed\n'
