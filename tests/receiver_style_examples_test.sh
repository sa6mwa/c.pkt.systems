#!/usr/bin/env bash
set -euo pipefail

SOURCE_DIR="${1:?usage: receiver_style_examples_test.sh <source-dir>}"

python3 - "${SOURCE_DIR}" <<'PY'
import pathlib
import re
import sys

source_dir = pathlib.Path(sys.argv[1])
paths = [
    source_dir / "docs" / "audio-sus-facade-spec.md",
    source_dir / "examples" / "audio-sus-c89" / "main.c",
    source_dir / "examples" / "audio-vox-intro-c89" / "main.c",
    source_dir / "examples" / "sus-vox-intro-c89" / "main.c",
]

patterns = [
    (
        re.compile(r"\bmodel->open_cached\s*\("),
        "cached model opening is a constructor; use cpkt_sus_model_open_cached",
    ),
    (
        re.compile(r"\bcpkt_audio_decoder_(?:read_f32_mono_16k|info|destroy)\s*\("),
        "decoder handle operations in docs/examples must use decoder->method",
    ),
    (
        re.compile(r"\bcpkt_audio_encoder_(?:write_f32|close|destroy)\s*\("),
        "encoder handle operations in docs/examples must use encoder->method",
    ),
    (
        re.compile(r"\bcpkt_audio_vox_(?:push_f32_mono_16k|flush|destroy)\s*\("),
        "VOX handle operations in docs/examples must use vox->method",
    ),
    (
        re.compile(r"\bcpkt_sus_model_(?:info|create_transcriber|destroy)\s*\("),
        "model handle operations in docs/examples must use model->method",
    ),
    (
        re.compile(
            r"\bcpkt_sus_transcriber_(?:transcribe_f32_mono_16k|"
            r"transcribe_f32_mono_16k_text|transcribe_audio_decoder_realtime|"
            r"transcribe_audio_decoder_realtime_text|revised_text|destroy)\s*\("
        ),
        "transcriber handle operations in docs/examples must use transcriber->method",
    ),
]

failures = []
for path in paths:
    text = path.read_text(encoding="utf-8")
    for pattern, message in patterns:
        for match in pattern.finditer(text):
            line = text.count("\n", 0, match.start()) + 1
            failures.append(f"{path}:{line}: {message}")

if failures:
    for failure in failures:
        print(failure, file=sys.stderr)
    sys.exit(1)
PY
