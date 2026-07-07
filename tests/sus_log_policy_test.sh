#!/usr/bin/env sh
set -eu

if [ "$#" -ne 1 ]; then
  printf 'usage: sus_log_policy_test.sh <source-dir>\n' >&2
  exit 2
fi

source_dir=$1
sus_header=$source_dir/include/cpkt/sus.h
sus_source=$source_dir/src/sus.c

require_file_contains() {
  file=$1
  needle=$2
  description=$3
  if ! grep -F -- "$needle" "$file" >/dev/null 2>&1; then
    printf 'missing sus log policy: %s\n' "$description" >&2
    printf 'expected to find: %s\n' "$needle" >&2
    printf 'in: %s\n' "$file" >&2
    exit 1
  fi
}

require_file_contains "$sus_header" \
  'Reserved continuation level; backend continuations are normalized.' \
  'public CONT level is reserved and not delivered as backend state'
require_file_contains "$sus_header" \
  'The speech backend exposes logging as process-global state, so this facade' \
  'public API documents backend log state as process-wide'
require_file_contains "$sus_source" \
  'static int cpkt_sus_log_last_level = CPKT_SUS_LOG_INFO;' \
  'facade tracks the previous concrete backend log level'
require_file_contains "$sus_source" \
  'if (level == GGML_LOG_LEVEL_CONT) {' \
  'backend continuation logs are handled explicitly'
require_file_contains "$sus_source" \
  'event.level = cpkt_sus_log_last_level;' \
  'continuation logs inherit the previous concrete level'
require_file_contains "$sus_source" \
  'cpkt_sus_log_last_level = event.level;' \
  'non-continuation logs update the previous concrete level'
require_file_contains "$sus_source" \
  'cpkt_sus_log_last_level = CPKT_SUS_LOG_INFO;' \
  'installing a logger resets continuation state to a concrete level'
