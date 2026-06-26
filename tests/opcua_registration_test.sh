#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
cmake_file="$repo_root/CMakeLists.txt"
test_file="$repo_root/tests/opcua_facade_test.c"

assert_contains() {
  file_path=$1
  expected=$2
  description=$3

  if ! grep -F -- "$expected" "$file_path" >/dev/null 2>&1; then
    printf '%s\nmissing: %s\nin: %s\n' "$description" "$expected" "$file_path" >&2
    exit 1
  fi
}

assert_contains "$cmake_file" \
  'add_test(NAME opcua_facade_integration COMMAND cpkt_opcua_facade_test)' \
  'OPC UA default integration test must stay registered with CTest'
assert_contains "$cmake_file" \
  'NAME opcua_boundary_server_c99_client_c89' \
  'OPC UA C99-server/C89-client boundary test must stay registered with CTest'
assert_contains "$cmake_file" \
  'COMMAND cpkt_opcua_facade_test server-is-c99-and-client-is-c89)' \
  'OPC UA C99-server/C89-client boundary test must invoke the matching test mode'
assert_contains "$cmake_file" \
  'NAME opcua_boundary_server_c89_client_c99' \
  'OPC UA C89-server/C99-client boundary test must stay registered with CTest'
assert_contains "$cmake_file" \
  'COMMAND cpkt_opcua_facade_test server-is-c89-and-client-is-c99)' \
  'OPC UA C89-server/C99-client boundary test must invoke the matching test mode'
assert_contains "$cmake_file" \
  'NAME opcua_boundary_server_c89_client_c89' \
  'OPC UA C89-server/C89-client boundary test must stay registered with CTest'
assert_contains "$cmake_file" \
  'COMMAND cpkt_opcua_facade_test server-is-c89-and-client-is-c89)' \
  'OPC UA C89-server/C89-client boundary test must invoke the matching test mode'
assert_contains "$cmake_file" \
  'add_test(NAME opcua_facade_failure_modes COMMAND cpkt_opcua_facade_test failure-modes)' \
  'OPC UA failure-mode group must stay registered with CTest'

assert_contains "$test_file" \
  'strcmp(argv[1], "server-is-c99-and-client-is-c89")' \
  'OPC UA test binary must keep the C99-server/C89-client dispatch mode'
assert_contains "$test_file" \
  'cmocka_run_group_tests(server_c99_client_c89_tests, NULL, NULL)' \
  'OPC UA C99-server/C89-client dispatch must run the intended group'
assert_contains "$test_file" \
  'strcmp(argv[1], "server-is-c89-and-client-is-c99")' \
  'OPC UA test binary must keep the C89-server/C99-client dispatch mode'
assert_contains "$test_file" \
  'cmocka_run_group_tests(server_c89_client_c99_tests, NULL, NULL)' \
  'OPC UA C89-server/C99-client dispatch must run the intended group'
assert_contains "$test_file" \
  'strcmp(argv[1], "server-is-c89-and-client-is-c89")' \
  'OPC UA test binary must keep the C89-server/C89-client dispatch mode'
assert_contains "$test_file" \
  'cmocka_run_group_tests(facade_client_server_tests, NULL, NULL)' \
  'OPC UA C89-server/C89-client dispatch must run the intended group'
assert_contains "$test_file" \
  'strcmp(argv[1], "failure-modes")' \
  'OPC UA test binary must keep the failure-mode dispatch mode'
assert_contains "$test_file" \
  'cmocka_run_group_tests(failure_tests, NULL, NULL)' \
  'OPC UA failure-mode dispatch must run the intended group'

printf '[test] OPC UA registration passed\n'
