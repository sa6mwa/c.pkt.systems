#!/usr/bin/env bash
set -euo pipefail

SOURCE_DIR="${1:?usage: sus_model_catalog_metadata_test.sh <source-dir>}"

python3 - "${SOURCE_DIR}" <<'PY'
import csv
import pathlib
import re
import sys

source_dir = pathlib.Path(sys.argv[1])
source_path = source_dir / "src" / "sus.c"
metadata_path = source_dir / "docs" / "sus-model-catalog.tsv"
expected_header = [
    "name",
    "provider",
    "filename",
    "source_url",
    "sha256",
    "size_bytes",
    "license",
    "quantization",
    "is_default",
]

text = source_path.read_text(encoding="utf-8")
body = text.split(
    "static const struct cpkt_sus_catalog_entry cpkt_sus_catalog[] = {", 1
)[1].split("};", 1)[0]

entries = []
current = ""
depth = 0
for ch in body:
    if ch == "{":
        depth += 1
        current = ch
    elif ch == "}" and depth:
        depth -= 1
        current += ch
        if depth == 0:
            entries.append(current)
            current = ""
    elif depth:
        current += ch

compiled_rows = []
for entry in entries:
    strings = re.findall(r'"((?:[^"\\]|\\.)*)"', entry)
    sha_index = None
    for index, value in enumerate(strings):
        if re.fullmatch(r"[0-9a-f]{64}", value):
            sha_index = index
            break
    if sha_index is None:
        raise SystemExit(f"catalog entry is missing a SHA-256: {entry}")
    sizes = re.findall(r"([0-9]+)UL,\s*\"", entry)
    default = re.search(r'"[^"\\]*(?:\\.[^"\\]*)*"\s*,\s*([01])\s*}', entry)
    if not sizes or default is None:
        raise SystemExit(f"catalog entry is missing size/default: {entry}")
    compiled_rows.append(
        {
            "name": strings[0],
            "provider": strings[1],
            "filename": strings[2],
            "source_url": "".join(strings[3:sha_index]),
            "sha256": strings[sha_index],
            "size_bytes": sizes[-1],
            "license": strings[sha_index + 1],
            "quantization": strings[sha_index + 2],
            "is_default": default.group(1),
        }
    )

with metadata_path.open("r", encoding="utf-8", newline="") as handle:
    reader = csv.DictReader(handle, delimiter="\t")
    if reader.fieldnames != expected_header:
        raise SystemExit(f"unexpected catalog header: {reader.fieldnames!r}")
    metadata_rows = list(reader)

if compiled_rows != metadata_rows:
    compiled_by_name = {row["name"]: row for row in compiled_rows}
    metadata_by_name = {row["name"]: row for row in metadata_rows}
    missing = sorted(set(compiled_by_name) - set(metadata_by_name))
    extra = sorted(set(metadata_by_name) - set(compiled_by_name))
    changed = sorted(
        name
        for name in set(compiled_by_name) & set(metadata_by_name)
        if compiled_by_name[name] != metadata_by_name[name]
    )
    raise SystemExit(
        "sus model catalog metadata does not match src/sus.c "
        f"(missing={missing}, extra={extra}, changed={changed})"
    )

defaults = [row["name"] for row in metadata_rows if row["is_default"] == "1"]
if defaults != ["small"]:
    raise SystemExit(f"expected exactly one default model named small, got {defaults}")

for row in metadata_rows:
    if not row["source_url"].startswith("https://huggingface.co/"):
        raise SystemExit(f"model source URL is not a logical Hugging Face URL: {row}")
    if not re.fullmatch(r"[0-9a-f]{64}", row["sha256"]):
        raise SystemExit(f"model SHA-256 is not lowercase hex: {row}")
    if not row["size_bytes"].isdigit() or int(row["size_bytes"]) <= 0:
        raise SystemExit(f"model size must be a positive byte count: {row}")

print(f"validated {len(metadata_rows)} sus model catalog entries")
PY
