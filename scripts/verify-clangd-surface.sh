#!/usr/bin/env bash
set -euo pipefail

SOURCE_DIR="${1:?usage: verify-clangd-surface.sh <source-dir> <build-dir>}"
BUILD_DIR="${2:?usage: verify-clangd-surface.sh <source-dir> <build-dir>}"
COMPILE_COMMANDS="${BUILD_DIR}/compile_commands.json"
HEADER="${SOURCE_DIR}/include/cpkt/lua_runtime.h"

if [[ ! -f "${COMPILE_COMMANDS}" ]]; then
  printf 'compile database not found: %s\n' "${COMPILE_COMMANDS}" >&2
  exit 1
fi

require_compile_command() {
  local source_file
  source_file="$1"
  if ! grep -F "\"${SOURCE_DIR}/${source_file}\"" "${COMPILE_COMMANDS}" >/dev/null; then
    printf 'compile database does not contain %s\n' "${source_file}" >&2
    exit 1
  fi
}

require_hover_comment() {
  local symbol
  symbol="$1"
  awk -v symbol="${symbol}" '
    index($0, symbol) {
      for (i = NR - 1; i >= 1; --i) {
        if (lines[i] ~ /^[[:space:]]*$/) {
          continue;
        }
        if (lines[i] ~ /\*\/[[:space:]]*$/) {
          found = 1;
          exit 0;
        }
        printf("public symbol %s is missing an adjacent Doxygen comment\n", symbol) > "/dev/stderr";
        exit 1;
      }
    }
    { lines[NR] = $0 }
    END {
      if (!found) {
        printf("public symbol %s was not found in %s\n", symbol, FILENAME) > "/dev/stderr";
        exit 1;
      }
    }
  ' "${HEADER}"
}

require_compile_command "examples/abi_smoke.c"
require_compile_command "examples/lua-runtime-c89/main.c"
require_compile_command "examples/lua-runtime-c89/host_module.c"

require_hover_comment "typedef struct cpkt_lua_runtime cpkt_lua_runtime;"
require_hover_comment "typedef enum cpkt_lua_runtime_status"
require_hover_comment "typedef enum cpkt_lua_runtime_flags"
require_hover_comment "typedef enum cpkt_lua_runtime_libs"
require_hover_comment "typedef int (*cpkt_lua_runtime_c_module_open_fn)"
require_hover_comment "typedef void *(*cpkt_lua_runtime_alloc_fn)"
require_hover_comment "typedef void *(*cpkt_lua_runtime_realloc_fn)"
require_hover_comment "typedef void (*cpkt_lua_runtime_free_fn)"
require_hover_comment "typedef struct cpkt_lua_runtime_allocator_config"
require_hover_comment "const char *cpkt_lua_runtime_lua_version"
require_hover_comment "const char *cpkt_lua_runtime_facade_version"
require_hover_comment "cpkt_lua_runtime_status cpkt_lua_runtime_new("
require_hover_comment "cpkt_lua_runtime_status cpkt_lua_runtime_new_with_limit"
require_hover_comment "cpkt_lua_runtime_status cpkt_lua_runtime_new_with_allocator"
require_hover_comment "void cpkt_lua_runtime_free"
require_hover_comment "void cpkt_lua_runtime_set_context"
require_hover_comment "void *cpkt_lua_runtime_context("
require_hover_comment "void *cpkt_lua_runtime_context_from_state"
require_hover_comment "cpkt_lua_runtime_status cpkt_lua_runtime_openlibs"
require_hover_comment "cpkt_lua_runtime_status cpkt_lua_runtime_open_libs"
require_hover_comment "cpkt_lua_runtime_status cpkt_lua_runtime_set_traceback"
require_hover_comment "cpkt_lua_runtime_status cpkt_lua_runtime_set_instruction_limit"
require_hover_comment "cpkt_lua_runtime_status cpkt_lua_runtime_clear_instruction_limit"
require_hover_comment "cpkt_lua_runtime_status cpkt_lua_runtime_set_package_path"
require_hover_comment "cpkt_lua_runtime_status cpkt_lua_runtime_prepend_package_path"
require_hover_comment "cpkt_lua_runtime_status cpkt_lua_runtime_set_package_cpath"
require_hover_comment "cpkt_lua_runtime_status cpkt_lua_runtime_prepend_package_cpath"
require_hover_comment "cpkt_lua_runtime_status cpkt_lua_runtime_set_global_string"
require_hover_comment "cpkt_lua_runtime_status cpkt_lua_runtime_set_global_boolean"
require_hover_comment "cpkt_lua_runtime_status cpkt_lua_runtime_set_global_number"
require_hover_comment "cpkt_lua_runtime_status cpkt_lua_runtime_set_global_integer"
require_hover_comment "cpkt_lua_runtime_status cpkt_lua_runtime_register_c_module"
require_hover_comment "cpkt_lua_runtime_status cpkt_lua_runtime_register_lua_module"
require_hover_comment "cpkt_lua_runtime_status cpkt_lua_runtime_require"
require_hover_comment "cpkt_lua_runtime_status cpkt_lua_runtime_run_file"
require_hover_comment "cpkt_lua_runtime_status cpkt_lua_runtime_run_buffer"
require_hover_comment "const char *cpkt_lua_runtime_error"
require_hover_comment "void cpkt_lua_runtime_clear_error"
require_hover_comment "const char *cpkt_lua_runtime_status_string"

if ! command -v clangd >/dev/null 2>&1; then
  printf 'SKIP clangd --check: clangd is not installed\n'
  exit 0
fi

clangd --check="${SOURCE_DIR}/examples/abi_smoke.c" --compile-commands-dir="${BUILD_DIR}" >/dev/null
clangd --check="${SOURCE_DIR}/examples/lua-runtime-c89/main.c" --compile-commands-dir="${BUILD_DIR}" >/dev/null
clangd --check="${SOURCE_DIR}/examples/lua-runtime-c89/host_module.c" --compile-commands-dir="${BUILD_DIR}" >/dev/null
