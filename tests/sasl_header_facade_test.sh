#!/usr/bin/env bash
set -euo pipefail

repo_root=${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
header="$repo_root/include/cpkt/sasl.h"
include_dir="$repo_root/include"
cc=${CC:-cc}
mkdir -p "$repo_root/build"
work_root=$(mktemp -d "$repo_root/build/cpkt-sasl-header.XXXXXX")
trap 'rm -rf "$work_root"' EXIT

for forbidden in 'sasl/' 'stdint.h' 'stdbool.h' 'uint32_t' 'int32_t' 'long long' 'extern "C"' 'inline'; do
  if grep -F -- "$forbidden" "$header" >/dev/null 2>&1; then
    printf 'SASL C89 facade header contains forbidden token: %s\n' "$forbidden" >&2
    exit 1
  fi
done

cat > "$work_root/sasl_header_c89.c" <<'EOF'
#include <cpkt/sasl.h>

int main(void) {
  cpkt_sasl *client;
  int status;

  client = cpkt_sasl_client_new("imap", "mail.example.test", 0, 0, 0, 0,
      &status);
  if (client != 0) client->close(client);
  return 0;
}
EOF

"$cc" -std=c89 -Wall -Wextra -Wpedantic -Werror -I "$include_dir" \
  -c "$work_root/sasl_header_c89.c" -o "$work_root/sasl_header_c89.o"
