#!/usr/bin/env bash
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

openssl_makefile="$work_dir/Makefile"
cat > "$openssl_makefile" <<'MAKE'
	$(CC) -dynamiclib -install_name $(libdir)/libcrypto.3.dylib -o libcrypto.3.dylib
	$(CC) -dynamiclib -install_name $(libdir)/libssl.3.dylib -o libssl.3.dylib
MAKE

cmake \
  -DCPKT_DARWIN_INSTALL_NAME_FILE="$openssl_makefile" \
  -P "$repo_root/cmake/patch_darwin_generated_install_names.cmake"

if ! grep -F -- "-install_name @rpath/libcrypto.3.dylib" "$openssl_makefile" >/dev/null 2>&1; then
  printf 'OpenSSL generated Makefile install name was not patched\n' >&2
  exit 1
fi

libtool_file="$work_dir/libtool"
cat > "$libtool_file" <<'LIBTOOL'
archive_cmds="$CC -dynamiclib -o $lib $libobjs -install_name \$rpath/\$soname"
archive_expsym_cmds="$CC -dynamiclib -o $lib $libobjs -install_name \$rpath/\$soname"
LIBTOOL

cmake \
  -DCPKT_DARWIN_INSTALL_NAME_FILE="$libtool_file" \
  -P "$repo_root/cmake/patch_darwin_generated_install_names.cmake"

if ! grep -F -- "-install_name @rpath/\\\$soname" "$libtool_file" >/dev/null 2>&1; then
  printf 'libtool generated install name was not patched\n' >&2
  exit 1
fi

unmatched_file="$work_dir/unmatched"
printf 'no install name here\n' > "$unmatched_file"
if cmake \
    -DCPKT_DARWIN_INSTALL_NAME_FILE="$unmatched_file" \
    -P "$repo_root/cmake/patch_darwin_generated_install_names.cmake" \
    >/dev/null 2>&1; then
  printf 'generated install-name patch unexpectedly accepted an unmatched file\n' >&2
  exit 1
fi

printf '[test] Darwin generated install-name patch passed\n'
