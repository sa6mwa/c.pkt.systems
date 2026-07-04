# KBLab Whisper Model Cache Entries

The `cpkt_sus` cached-model resolver includes curated metadata for GGML model
artifacts published by KBLab under the `KBLab/kb-whisper-*` Hugging Face
repositories.

These model files are not bundled into the c.pkt.systems SDK. The SDK ships
only resolver metadata: model names, repository identifiers, logical source
URLs, filenames, expected SHA-256 checksums, expected sizes, license labels, and
quantization labels. Runtime downloads are explicit through
`cpkt_sus_model_open_cached`.

The resolver table records these KBLab entries as `Apache-2.0`. The packaged
SDK includes this provenance note and the Apache License 2.0 text so downstream
consumers can inspect the license basis for KBLab model-cache metadata without
downloading a model first.
