#!/usr/bin/env sh
set -eu

if [ "$#" -ne 1 ]; then
  printf 'usage: sus_network_boundary_test.sh <source-dir>\n' >&2
  exit 2
fi

source_dir=$1
python3 - "$source_dir" <<'PY'
import pathlib
import re
import sys

source_dir = pathlib.Path(sys.argv[1])
sus_source = source_dir / "src" / "sus.c"
sus_header = source_dir / "include" / "cpkt" / "sus.h"
source = sus_source.read_text(encoding="utf-8")
header = sus_header.read_text(encoding="utf-8")


def fail(message):
    raise SystemExit(message)


def function_body(name):
    matches = list(
        re.finditer(
            r"(?:static\s+)?cpkt_sus_result\s+" + re.escape(name) + r"\s*\(",
            source,
        )
    )
    if not matches:
        fail(f"missing function {name} in {sus_source}")
    match = None
    brace = -1
    for candidate in matches:
        candidate_brace = source.find("{", candidate.end())
        candidate_semicolon = source.find(";", candidate.end())
        if candidate_brace >= 0 and (
            candidate_semicolon < 0 or candidate_brace < candidate_semicolon
        ):
            match = candidate
            brace = candidate_brace
            break
    if match is None:
        fail(f"missing function body for {name} in {sus_source}")
    if brace < 0:
        fail(f"function {name} has no body")
    depth = 0
    for index in range(brace, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[match.start() : index + 1]
    fail(f"function {name} body did not terminate")


def require_contains(text, needle, description):
    if needle not in text:
        fail(f"missing SUS network-boundary policy: {description}: {needle}")


def require_lacks(text, needle, description):
    if needle in text:
        fail(f"forbidden SUS network-boundary drift: {description}: {needle}")


require_contains(
    header,
    "Cache-backed model resolver options. Network access is explicit to this\n * path.",
    "public cache config documents network access as cache-only",
)

open_path = function_body("cpkt_sus_model_open_path")
compat_open = function_body("cpkt_sus_open_model")
open_cached = function_body("cpkt_sus_model_open_cached")
download_to_file = function_body("cpkt_sus_download_to_file")
fetch_cached_file = function_body("cpkt_sus_fetch_cached_file")

for name, body in (
    ("cpkt_sus_model_open_path", open_path),
    ("cpkt_sus_open_model", compat_open),
):
    require_lacks(body, "curl_", f"{name} must not call libcurl")
    require_lacks(
        body,
        "cpkt_sus_fetch_cached_file",
        f"{name} must not enter the cache fetch path",
    )
    require_lacks(
        body,
        "cpkt_sus_download_to_file",
        f"{name} must not enter the cache download path",
    )
    require_lacks(body, "source_url", f"{name} must not consume cache source URLs")

require_contains(
    compat_open,
    "return cpkt_sus_model_open_path((cpkt_sus_model **)out, config);",
    "compatibility constructor remains a direct path-open wrapper",
)
require_contains(
    open_path,
    "whisper_init_from_file_with_params",
    "path constructor calls the backend file loader",
)
require_contains(
    open_path,
    "config->model_path",
    "path constructor loads only the caller-provided local model path",
)
require_contains(
    open_cached,
    "cpkt_sus_fetch_cached_file(model_path, cache_dir, entry, config)",
    "cache constructor is the only public constructor that can fetch models",
)
require_contains(
    fetch_cached_file,
    "url = config->source_url;",
    "source_url override is consumed only by the cache fetch path",
)
require_contains(
    fetch_cached_file,
    "cpkt_sus_download_to_file(url, temp_path)",
    "cache fetch path owns model download",
)

for needle in ("curl_global_init", "curl_easy_perform"):
    if source.count(needle) != 1:
        fail(f"{needle} must occur exactly once in {sus_source}")
    require_contains(
        download_to_file,
        needle,
        f"{needle} must remain isolated in cpkt_sus_download_to_file",
    )

for name, body in (
    ("cpkt_sus_model_open_path", open_path),
    ("cpkt_sus_open_model", compat_open),
    ("cpkt_sus_model_open_cached", open_cached),
    ("cpkt_sus_fetch_cached_file", fetch_cached_file),
):
    require_lacks(
        body,
        "curl_easy_perform",
        f"{name} must not perform network I/O directly",
    )

print("validated cpktsus network boundary")
PY
