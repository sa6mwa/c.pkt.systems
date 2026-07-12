#!/usr/bin/env bash
set -euo pipefail

version=5.02c
build_revision=1
archive_name="AFLplusplus-${version}.tar.gz"
archive_sha256=118415843e5d289d63bd6d8f2252c18212978f15ac9e86acbbc75766cd45acde

die() {
  printf 'cpkt-aflpp: %s\n' "$*" >&2
  exit 1
}

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
toolchain_resolver="$repo_root/scripts/cpkt-toolchains.sh"

cache_root() {
  printf '%s\n' "${CPKT_TOOLCHAIN_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/c.pkt.systems/toolchains}"
}

afl_root() {
  printf '%s/roots/aflplusplus-%s-x86_64-linux-gnu\n' "$(cache_root)" "$version"
}

bootlin_value() {
  local key=$1 description=$2
  sed -n "s/^${key}=//p" <<<"$description" | tail -n 1
}

bootlin_description() {
  [[ -x "$toolchain_resolver" ]] || die "Bootlin resolver is missing: $toolchain_resolver"
  "$toolchain_resolver" ensure x86_64-linux-gnu >/dev/null
  "$toolchain_resolver" discover x86_64-linux-gnu
}

afl_ready() {
  local root=$1
  [[ -x "$root/bin/afl-fuzz" ]] &&
    [[ -x "$root/bin/afl-showmap" ]] &&
    [[ -x "$root/bin/cpkt-afl-gcc" ]] &&
    [[ -x "$root/bin/cpkt-afl-g++" ]] &&
    [[ -f "$root/.cpkt-aflpp-revision-${build_revision}" ]] &&
    [[ -f "$root/lib/afl/afl-gcc-pass.so" ]] &&
    [[ -f "$root/lib/afl/afl-compiler-rt.o" ]]
}

download_archive() {
  local archive_root=$1 archive=$2 temporary
  mkdir -p "$archive_root"
  if [[ -f "$archive" ]] && printf '%s  %s\n' "$archive_sha256" "$archive" | sha256sum -c - >/dev/null 2>&1; then
    return
  fi
  rm -f "$archive"
  temporary="$archive.tmp.$$"
  trap 'rm -f "$temporary"' EXIT HUP INT TERM
  if command -v curl >/dev/null 2>&1; then
    curl -fL --retry 3 --connect-timeout 20 --output "$temporary" \
      "https://github.com/AFLplusplus/AFLplusplus/archive/refs/tags/v${version}.tar.gz"
  elif command -v wget >/dev/null 2>&1; then
    wget -O "$temporary" "https://github.com/AFLplusplus/AFLplusplus/archive/refs/tags/v${version}.tar.gz"
  else
    die 'curl or wget is required to download AFL++'
  fi
  printf '%s  %s\n' "$archive_sha256" "$temporary" | sha256sum -c - >/dev/null || die "checksum mismatch: $archive_name"
  mv "$temporary" "$archive"
  trap - EXIT HUP INT TERM
}

build_afl() {
  local root=$1 source=$2 cc=$3 cxx=$4 bootlin_root=$5 temporary
  local helper="$root/lib/afl"
  temporary="$root.tmp.$$"
  rm -rf "$temporary"
  mkdir -p "$temporary/bin" "$temporary/lib/afl"

  (
    cd "$source"
    make -j1 NO_PYTHON=1 \
      CC="$cc" CXX="$cxx" \
      PREFIX="$temporary" HELPER_PATH="$helper" BIN_PATH="$temporary/bin" \
      afl-fuzz afl-showmap afl-tmin afl-gotcpu afl-analyze afl-cmin

    "$cc" -O3 -funroll-loops -fPIC -Wall -g \
      -I./include -I./instrumentation \
      -DAFL_PATH=\"$helper\" -DBIN_PATH=\"$temporary/bin\" \
      -DLLVM_BINDIR=\"\" -DVERSION=\"++${version}\" -DLLVM_LIBDIR=\"\" \
      -DLLVM_VERSION=\"\" -DAFL_CLANG_FLTO=\"\" -DAFL_REAL_LD=\"\" \
      -DAFL_CLANG_LDPATH=\"\" -DAFL_CLANG_FUSELD=\"\" \
      -DCLANG_BIN=\"$cc\" -DCLANGPP_BIN=\"$cxx\" -DUSE_BINDIR=1 \
      -Wno-unused-function -Wno-deprecated \
      -c src/afl-common.c -o instrumentation/afl-common.o
    "$cc" -O3 -funroll-loops -fPIC -Wall -g \
      -I./include -I./instrumentation \
      -DAFL_PATH=\"$helper\" -DBIN_PATH=\"$temporary/bin\" \
      -DLLVM_BINDIR=\"\" -DVERSION=\"++${version}\" -DLLVM_LIBDIR=\"\" \
      -DLLVM_VERSION=\"\" -DAFL_CLANG_FLTO=\"\" -DAFL_REAL_LD=\"\" \
      -DAFL_CLANG_LDPATH=\"\" -DAFL_CLANG_FUSELD=\"\" \
      -DCLANG_BIN=\"$cc\" -DCLANGPP_BIN=\"$cxx\" -DUSE_BINDIR=1 \
      -Wno-unused-function -Wno-deprecated \
      -DAFL_INCLUDE_PATH=\"$temporary/include/afl\" \
      src/afl-cc.c instrumentation/afl-common.o -o afl-cc \
      -DLLVM_MINOR=0 -DLLVM_MAJOR=0 -DCFLAGS_OPT=\"\" -lm
    ln -sf afl-cc afl-gcc-fast
    ln -sf afl-cc afl-g++-fast
    make -j1 -f GNUmakefile.gcc_plugin \
      CC="$cc" CXX="$cxx" \
      PREFIX="$temporary" HELPER_PATH="$helper" BIN_PATH="$temporary/bin" \
      CXXFLAGS="-O3 -g -funroll-loops -I${bootlin_root}/include" \
      LDFLAGS="-L${bootlin_root}/lib -Wl,-rpath,${bootlin_root}/lib"

    install -m 755 afl-fuzz afl-showmap afl-tmin afl-gotcpu afl-analyze afl-cmin "$temporary/bin/"
    install -m 755 afl-cc "$temporary/bin/"
    ln -sf afl-cc "$temporary/bin/afl-gcc-fast"
    ln -sf afl-cc "$temporary/bin/afl-g++-fast"
    printf '#!/usr/bin/env bash\nexport AFL_PATH=%q\nexport AFL_CC=%q\nexec %q "$@"\n' \
      "$root/lib/afl" "$cc" "$root/bin/afl-gcc-fast" > "$temporary/bin/cpkt-afl-gcc"
    printf '#!/usr/bin/env bash\nexport AFL_PATH=%q\nexport AFL_CC=%q\nexport AFL_CXX=%q\nexec %q "$@"\n' \
      "$root/lib/afl" "$cc" "$cxx" "$root/bin/afl-g++-fast" > "$temporary/bin/cpkt-afl-g++"
    chmod +x "$temporary/bin/cpkt-afl-gcc" "$temporary/bin/cpkt-afl-g++"
    install -m 755 afl-gcc-pass.so afl-gcc-cmplog-pass.so afl-gcc-cmptrs-pass.so "$temporary/lib/afl/"
    install -m 644 afl-compiler-rt.o "$temporary/lib/afl/"
    install -m 644 dynamic_list.txt "$temporary/lib/afl/"
    AFL_PATH="$temporary/lib/afl" AFL_CC="$cc" "$temporary/bin/afl-gcc-fast" -O0 test-instr.c -o test-instr
    "$temporary/bin/afl-showmap" -m none -q -o .cpkt-empty-map ./test-instr </dev/null
    printf '1\n' | "$temporary/bin/afl-showmap" -m none -q -o .cpkt-one-map ./test-instr
    cmp -s .cpkt-empty-map .cpkt-one-map && die 'Bootlin GCC AFL++ instrumentation did not record distinct paths'
    rm -f test-instr .cpkt-empty-map .cpkt-one-map
  )

  touch "$temporary/.cpkt-aflpp-revision-${build_revision}"
  afl_ready "$temporary" || die "incomplete AFL++ build: $temporary"
  rm -rf "$root"
  mv "$temporary" "$root"
}

ensure() {
  local cache archive_root archive root description cc cxx bootlin_root extract source
  [[ "$(uname -s)" = Linux ]] || die 'AFL++ GCC-plugin fuzzing is supported only on native Linux hosts'
  case "$(uname -m)" in
    x86_64|amd64) ;;
    *) die "AFL++ GCC-plugin fuzzing requires an x86_64 Linux host, got: $(uname -m)" ;;
  esac
  cache=$(cache_root)
  archive_root="$cache/archives"
  archive="$archive_root/$archive_name"
  root=$(afl_root)
  afl_ready "$root" && return
  description=$(bootlin_description)
  cc=$(bootlin_value cc "$description")
  cxx=$(bootlin_value cxx "$description")
  bootlin_root=$(bootlin_value root "$description")
  [[ -x "$cc" && -x "$cxx" && -d "$bootlin_root/include" ]] || die 'Bootlin GCC collection is incomplete for AFL++'
  download_archive "$archive_root" "$archive"
  extract="$cache/.aflplusplus-extract.$$"
  CPKT_AFLPP_EXTRACT=$extract
  trap 'rm -rf "${CPKT_AFLPP_EXTRACT:-}"' EXIT HUP INT TERM
  rm -rf "$extract"
  mkdir -p "$extract"
  tar -xzf "$archive" -C "$extract"
  source="$extract/AFLplusplus-$version"
  [[ -d "$source" ]] || die "unexpected AFL++ archive layout: $archive_name"
  build_afl "$root" "$source" "$cc" "$cxx" "$bootlin_root"
  trap - EXIT HUP INT TERM
  unset CPKT_AFLPP_EXTRACT
  rm -rf "$extract"
}

report() {
  local root
  ensure
  root=$(afl_root)
  printf 'version=%s\ncache=%s\nsource=aflplusplus\nroot=%s\n' "$version" "$(cache_root)" "$root"
  printf 'afl_fuzz=%s\nafl_showmap=%s\ncc=%s\ncxx=%s\nhelper=%s\n' \
    "$root/bin/afl-fuzz" "$root/bin/afl-showmap" "$root/bin/cpkt-afl-gcc" "$root/bin/cpkt-afl-g++" "$root/lib/afl"
}

print_env() {
  local description root bootlin_cc bootlin_cxx
  description=$(bootlin_description)
  root=$(afl_root)
  ensure
  bootlin_cc=$(bootlin_value cc "$description")
  bootlin_cxx=$(bootlin_value cxx "$description")
  printf 'export AFL_PATH=%q\n' "$root/lib/afl"
  printf 'export CPKT_AFLPP_ROOT=%q\n' "$root"
  printf 'export AFL_CC=%q\n' "$bootlin_cc"
  printf 'export AFL_CXX=%q\n' "$bootlin_cxx"
  printf 'export CC=%q\n' "$root/bin/cpkt-afl-gcc"
  printf 'export CXX=%q\n' "$root/bin/cpkt-afl-g++"
  printf 'export PATH=%q\n' "$root/bin:$PATH"
}

case "${1:-}" in
  ensure) [[ $# -eq 1 ]] || die 'usage: cpkt-aflpp.sh ensure'; ensure ;;
  discover) [[ $# -eq 1 ]] || die 'usage: cpkt-aflpp.sh discover'; report ;;
  env) [[ $# -eq 1 ]] || die 'usage: cpkt-aflpp.sh env'; print_env ;;
  *) die 'usage: cpkt-aflpp.sh {ensure|discover|env}' ;;
esac
