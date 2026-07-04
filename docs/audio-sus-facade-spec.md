# Audio And Susurro C89 Facade Specification

This document defines the initial implementation target for two new `cpkt`
C89 facades:

- `cpkt_audio`, a facade over miniaudio for audio decoding, conversion, and
  encoding.
- `cpkt_sus`, a facade over whisper.cpp for local speech-to-text inference.

The facades are not reimplementations of miniaudio or whisper.cpp. They provide
stable C89-compatible public handles, receiver methods, configuration records,
callbacks, model-cache policy, lifetime rules, and tests while keeping all
backend state and backend ABI details private.

This spec is intentionally iterative. It captures the current production
decisions so implementation can begin against a concrete target.

## Goals

- Ship `libcpktaudio.a`, `libcpktaudio.so.0`, and the Darwin equivalents as a
  stable ABI-0 facade over miniaudio.
- Ship `libcpktsus.a`, `libcpktsus.so.0`, and the Darwin equivalents as a
  stable ABI-0 facade over whisper.cpp.
- Keep `cpkt_audio` and `cpkt_sus` as separate libraries. Do not collapse them
  into one combined audio-plus-transcription library.
- Provide a C89-compatible public API that exposes no miniaudio, whisper.cpp,
  ggml, C++ standard library, or backend-specific types in installed headers.
- Make receiver-style handles the intended usage style for these new facades.
- Support streaming audio input from files, HTTP/HTTPS URLs, and callback
  readers.
- Decode supported audio into PCM suitable for whisper.cpp: `float32`, mono,
  16000 Hz.
- Support audio encoding where miniaudio provides a practical built-in encoder,
  while keeping exact supported formats queryable and test-covered.
- Ship the first `libcpktsus` implementation CPU-only so the same facade can
  build across all cpkt target architectures before GPU packaging policy is
  introduced.
- Provide streaming-first transcription output through callbacks, with explicit
  materialized-text helpers as convenience APIs only.
- Provide model loading from an explicit path and an explicit cache-backed model
  resolver.
- Pin model downloads with checksums by default, with explicit user-supplied
  checksum overrides and an explicit insecure/no-checksum mode that is off by
  default.
- Include license and provenance material for bundled upstream code and curated
  model-cache entries in binary and source artifacts.

## Non-Goals

- Do not expose miniaudio or whisper.cpp headers as part of the facade API.
- Do not guarantee miniaudio or whisper.cpp ABI stability to downstream
  consumers. The facades own the public ABI.
- Do not perform implicit network access from ordinary constructors.
- Do not silently download a model unless the caller explicitly uses the cached
  model path.
- Do not call APIs "streaming" when they materialize an entire audio stream,
  model, or transcript internally. Use "materialized", "buffered", or
  "cached" for those behaviors.
- Do not provide Vulkan, CUDA, Metal, BLAS, OpenCL, SYCL, OpenVINO, CoreML, or
  similar acceleration in the first implementation. GPU-enabled artifacts are a
  deliberate follow-up phase after dependency metadata, package verification,
  and downstream link behavior are defined.
- Do not implement JSON output in this repository. Future JSON sinks should use
  a JSON dependency such as lonejson in the consuming integration, without
  adding a parent-relative dependency to this repository.
- Do not treat third-party Hugging Face model files as equivalent to
  whisper.cpp-loadable GGML files unless the curated entry points to a verified
  GGML artifact.

## Upstream Dependency Pins

Initial source pins inspected for this specification:

| Dependency | Version | Source | SHA-256 |
| --- | --- | --- | --- |
| miniaudio | `0.11.25` | `https://github.com/mackron/miniaudio/archive/refs/tags/0.11.25.tar.gz` | `b900edcffe979816e2560a0580b9b1216d674b4f17fbadeca8f777a7f8ab0274` |
| whisper.cpp | `v1.9.1` | `https://github.com/ggml-org/whisper.cpp/archive/refs/tags/v1.9.1.tar.gz` | `147267177eef7b22ec3d2476dd514d1b12e160e176230b740e3d1bd600118447` |

These pins are build inputs for the facades, not public ABI promises. Changing
either upstream version requires updating dependency identity, license/provenance
metadata, package verification expectations, and facade tests as needed.

## Licensing And Provenance

- `cpkt_audio` must identify itself as a `cpkt` facade over miniaudio, not as
  the upstream miniaudio library.
- `cpkt_sus` must identify itself as a `cpkt` facade over whisper.cpp and ggml,
  not as the upstream whisper.cpp library.
- Binary SDKs and source archives must include:
  - the project license;
  - miniaudio license text when miniaudio source or binaries are bundled;
  - whisper.cpp/ggml license text when whisper.cpp/ggml source or binaries are
    bundled;
  - Apache-2.0 license/provenance for KBLab model-cache entries.
- Dependency and model manifests must use logical source URLs and artifact
  checksums. They must not contain local checkout paths, cache paths, build
  directories, temporary directories, or workstation-specific paths.

## Build Policy

### cpkt_audio

Build miniaudio as an internal dependency, through a controlled wrapper or
upstream `miniaudio.c/.h` build. The facade should compile only the features
needed for the shipped public surface.

Initial miniaudio build intent:

- Keep decoding enabled.
- Keep WAV, FLAC, and MP3 decoding enabled when available from miniaudio's
  built-in codecs.
- Keep encoding enabled where miniaudio supports it.
- Disable device I/O, engine, resource manager, node graph, generation, and
  other playback/capture surfaces unless a future facade explicitly needs them.
- Avoid optional external Vorbis/Opus extras in the first release unless their
  dependency and license closure is deliberately added and verified.

### cpkt_sus

Build the first whisper.cpp integration CPU-only. This keeps the initial
`libcpktsus` artifact portable across all cpkt targets and avoids making
Vulkan/CUDA runtime libraries, CMake/pkg-config transitive link metadata, loader
paths, and optional backend packaging part of the first cut.

Initial whisper.cpp/ggml build intent:

- `WHISPER_BUILD_TESTS=OFF`
- `WHISPER_BUILD_EXAMPLES=OFF`
- `WHISPER_BUILD_SERVER=OFF`
- `WHISPER_CURL=OFF`
- `WHISPER_SDL2=OFF`
- `WHISPER_COREML=OFF`
- `WHISPER_OPENVINO=OFF`
- `GGML_NATIVE=OFF`, unless a future per-target optimized variant is explicitly
  introduced.
- `GGML_OPENMP=OFF`, unless OpenMP becomes a deliberate packaged dependency.
- `GGML_METAL=OFF`
- `GGML_BLAS=OFF`
- `GGML_ACCELERATE=OFF`
- `GGML_CUDA=OFF`
- `GGML_HIP=OFF`
- `GGML_VULKAN=OFF`
- `GGML_OPENCL=OFF`
- `GGML_SYCL=OFF`
- `GGML_RPC=OFF`
- `GGML_BACKEND_DL=OFF`

Expose a project build option such as `CPKT_SUS_CPU_ONLY=ON`, default `ON` for
the first implementation. When GPU-enabled artifacts are introduced later, this
option remains the force-CPU build switch.

The exact option set should be kept in the dependency build logic and covered by
a build metadata/provenance test so host-native acceleration does not
accidentally enter release artifacts. Package metadata must record the compiled
backend set, initially `cpu`. The public facade reports the same compiled set
through `cpkt_sus_backend_capabilities()`.

Future GPU-enabled artifacts must follow target-specific dependency detection.
Backend autodetection must inspect the configured target toolchain and
dependency root, not incidental host packages. With whisper.cpp v1.9.1 and
`GGML_BACKEND_DL=OFF`, ggml links backend libraries into the `ggml` target; the
Vulkan backend links `Vulkan::Vulkan`. Therefore a future `libcpktsus` artifact
compiled with Vulkan or CUDA must either bundle the required runtime/link
closure, declare it in CMake/pkg-config metadata, or use a deliberate dynamic
backend strategy that keeps optional GPU backends out of the mandatory consumer
link path. CPU fallback at runtime does not remove link-time requirements from a
GPU-enabled artifact.

### C++ Runtime Packaging

`libcpktsus` is a C89 facade over C++ implementation code. Downstream consumers
must not need a C++ compiler driver, host `libstdc++`, or host C++ runtime
package in order to consume cpkt Linux SDK artifacts. The build and package
contract for Linux targets is:

- every target toolchain used for `cpkt_sus` must provide both `libstdc++.a`
  and `libgcc.a`;
- binary SDKs must ship the cpkt-selected static C++ runtime archives needed by
  the target under a project-owned runtime directory;
- `libcpktsus.a` must not merge or absorb `libstdc++.a` objects internally;
- `libcpktsus.a` static consumers must link through cpkt-provided metadata,
  such as `pkg-config --static`, which places `libcpktsus.a` first and the
  cpkt-provided C++ runtime archives after it;
- C and C++ downstream objects in the same final static link must resolve
  against that one cpkt-provided C++ runtime closure, not a second host or
  vendor `libstdc++`;
- package metadata must not require downstreams to add raw workaround flags or
  system paths by hand.

`libcpktsus.so` should keep the C++ implementation private:

- export only intended `cpkt_sus_*` ABI symbols;
- hide whisper.cpp, ggml, and C++ standard library symbols where the platform
  supports it;
- avoid a runtime dependency on a downstream-provided `libstdc++.so`;
- verify loader metadata so no `NEEDED libstdc++.so` or `NEEDED libgcc_s.so`
  appears in shipped Linux `libcpktsus.so` artifacts unless a later explicit
  runtime-package policy replaces this contract.

Do not ship a bundled dynamic `libstdc++.so` as the default solution. It would
make C++ runtime ABI, loader path behavior, and CVE updates part of the cpkt
runtime support surface.

Darwin is a separate runtime model. The Linux `libstdc++.a`/`libgcc.a` closure
rule does not apply to `arm64-apple-darwin` artifacts. Darwin builds use the
Apple/clang C++ runtime model, normally `libc++`, and package metadata must
emit the target-correct Darwin static link requirements rather than inventing a
cpkt-shipped `libstdc++.a`.

Darwin contract:

- keep the public ABI C-only and export only intended facade symbols;
- use the configured osxcross/Apple target compiler and linker metadata;
- for `libcpktsus.dylib`, verify Mach-O loader metadata with target-correct
  `otool` and allow only normal system C++ runtime dependencies;
- for `libcpktsus.a`, make pkg-config/CMake static metadata provide the
  required Darwin C++ runtime and system-library flags, expected to include
  `-lc++` when C++ runtime symbols are needed;
- do not bundle `libstdc++` for Darwin;
- do not require downstream C consumers to invoke `clang++` as the final linker
  driver.

## Public API Style

These facades use public receiver-shell handles. A public handle contains stable
method pointers and an opaque `impl` pointer. All mutable backend state lives
behind `impl`.

Example shape:

```c
typedef struct cpkt_audio_decoder cpkt_audio_decoder;

struct cpkt_audio_decoder {
  void *impl;
  cpkt_audio_result (*read_f32_mono_16k)(
      cpkt_audio_decoder *self,
      float *out,
      size_t frame_capacity,
      size_t *frames_read);
  void (*destroy)(cpkt_audio_decoder *self);
};
```

Receiver method calls are the documented happy path. Free-function wrappers may
exist only when they materially help ABI smoke tests, internal dispatch, or
downstream linking. They are not the intended user style.

Public headers must compile as C89 and C++. They must not require `<stdbool.h>`,
`<stdint.h>`, compound literals, inline semantics, variadic macros, C++ types,
or upstream typedefs.

Public value/config structs may be transparent when field order, zero defaults,
and ownership rules are stable. Use zero-initializable configs where practical.

Handles are single-thread-at-a-time by contract. A handle may move between
threads only when not used concurrently. Multiple independent handles may exist
and run concurrently in separate threads subject to upstream miniaudio and
whisper.cpp constraints.

No facade-level global mutable state is allowed. Curated model tables may be
static read-only data.

## cpkt_audio Surface

### Handles

- `cpkt_audio_decoder`
- `cpkt_audio_encoder`

### Common Types

Expected public types:

- `cpkt_audio_result`
- `cpkt_audio_format`
- `cpkt_audio_stream_info`
- `cpkt_audio_reader`
- `cpkt_audio_writer`
- `cpkt_audio_decoder_config`
- `cpkt_audio_encoder_config`

Reader and writer callbacks must define:

- ownership of callback context;
- partial read/write behavior;
- EOF signaling;
- error signaling;
- seek/tell availability;
- close/destroy responsibilities;
- callback failure propagation.

Seekless audio readers are permitted at the API boundary, but backend format
support is capability-dependent. For the initial miniaudio-backed WAV path, a
reader without seek support is rejected safely with `CPKT_AUDIO_ERR_IO` rather
than hidden materialization or an unsafe backend call. Chunked callback readers
that can seek must support partial reads without requiring each callback to fill
the backend's requested byte count.

### Decoder

The first decoder tier must support:

- open from explicit file path;
- open from HTTP/HTTPS URL through libcurl without pre-downloading the full
  response;
- open from callback reader;
- decode to `float32` mono 16000 Hz for whisper.cpp;
- report source format where miniaudio exposes it;
- report output format selected by the facade;
- destroy/close cleanup that tolerates partially initialized handles.

The decoder may expose generic PCM output later, but the initial required
workflow is `float32` mono 16000 Hz.

Supported decoding formats should follow miniaudio's built-in decoder support
for this build. At the time of inspection, the relevant built-in decode targets
are WAV, FLAC, and MP3. The public API must provide a queryable capability list
or documented constants so callers do not infer support from upstream miniaudio
headers.

### Encoder

Prepare `cpkt_audio_encoder` in the initial public design. Implement encoding
where miniaudio's built-in APIs make this practical without new external
dependencies.

The first encoder tier should support:

- open from explicit file path;
- open from callback writer when miniaudio supports the required write/seek
  semantics;
- encode PCM from caller-provided buffers;
- close/finalize explicitly;
- query supported output formats.

Do not imply that every decoded format can also be encoded. Encoding support
must be capability-driven and tested. WAV is expected to be the first-class
encoding format. Additional formats require explicit confirmation from
miniaudio's built-in encoder surface or a deliberate dependency expansion.

## cpkt_sus Surface

### Handles

- `cpkt_sus_model`
- `cpkt_sus_transcriber`

### Common Types

Expected public types:

- `cpkt_sus_result`
- `cpkt_sus_model_config`
- `cpkt_sus_cache_config`
- `cpkt_sus_transcriber_config`
- `cpkt_sus_segment`
- `cpkt_sus_segment_sink`
- `cpkt_sus_progress_sink`
- `cpkt_sus_abort_fn`

### Model Loading

Model loading must support:

- open from explicit path;
- open from explicit cache configuration;
- explicit model name selection;
- explicit cache directory override;
- default cache directory selection;
- pinned checksum validation by default;
- user-supplied checksum override;
- explicit insecure/no-checksum mode, off by default;
- atomic download/write/verify/rename behavior.

Current implementation status:

- explicit-path loading is implemented;
- cache-backed loading resolves curated model names, default `small`, explicit
  cache directories, XDG/HOME cache directories, existing local cache files,
  pinned checksums, user checksum overrides, and explicit insecure checksum
  bypass;
- missing local cache entries are downloaded with libcurl unless `offline` is
  non-zero;
- `source_url` may override a curated entry's URL for controlled mirrors or
  deterministic tests, while checksum rules still apply;
- downloaded files are written to a unique temporary path, flushed, checksum
  verified, model-load verified, and then atomically renamed into place;
- cache loading returns distinct lookup, I/O, checksum, model-load, and future
  network result codes.

Opening from a path must not perform network access.

Opening from cache may perform network access only because the caller selected
the cached route. The function name and documentation must make this side effect
clear.

Whisper.cpp has a callback-based model loader, but `open_cached` should
materialize model files on disk. The cache is part of the product behavior:
large model downloads need restartable storage, integrity verification, and
re-use across processes.

### Transcription

The first transcription tier must support:

- transcribe `float32` mono 16000 Hz PCM buffers;
- transcribe realtime rolling buffers from a `cpkt_audio_decoder` integration
  path;
- segment callback output using whisper.cpp's new-segment callback and segment
  accessors;
- progress callback;
- abort callback, where a non-zero callback return stops inference and reports
  `CPKT_SUS_ABORTED` instead of collapsing cancellation into a generic upstream
  or callback error;
- explicit materialized-text helpers that allocate final or revised transcript
  strings;
- project-owned free function for materialized strings.

Whisper.cpp realtime transcription is not a separate incremental decoder API.
Its own `examples/stream` path repeatedly calls `whisper_full` over a rolling
PCM buffer built from `step_ms` of new audio, up to `length_ms` of retained audio
context, and `keep_ms` of boundary audio after committed steps. The facade
should expose those upstream concepts directly for decoder integration and
describe the behavior as realtime rolling-buffer inference with streaming
segment callbacks. Do not hide full-audio materialization behind a
streaming-looking API, and do not invent a separate seconds/window contract that
does not match upstream semantics.

`step_ms` is the realtime cadence, not the whole inference context. Realtime
output is a rolling hypothesis surface: later audio can revise earlier text while
that audio remains inside the retained `length_ms` context. The facade must not
append every rolling hypothesis as final transcript text. It should support both
streaming hypothesis callbacks and an after-the-fact revised text result for the
session. A caller must be able to run the streaming realtime method with a live
hypothesis sink and then retrieve the latest revised transcript from the same
transcriber after completion. The revised text path may materialize text, but it
must not materialize decoded audio.

Initial transcription options should expose only stable, high-value knobs:

- model handle;
- thread count;
- `cpu_only`, default `0`, which forces `whisper_context_params.use_gpu = false`
  at runtime even when Vulkan or CUDA is compiled in later. In the first
  CPU-only implementation it is accepted for API stability and has no practical
  backend effect;
- language (`NULL`, empty string, or `"auto"` means auto-detect);
- translate vs transcribe;
- timestamps on/off;
- realtime decoder options named after upstream `whisper-stream`: `step_ms`,
  `length_ms`, `keep_ms`, optional prompt-token context carryover, audio context,
  and max tokens;
- realtime hypothesis callbacks, a materialized revised-text helper, and a
  transcriber receiver method for copying the latest revised transcript after a
  streaming realtime call, all without buffering decoded audio;
- initial prompt;
- progress and abort callbacks.

Advanced whisper.cpp options may be added later after concrete downstream
workflow demand.

## Audio And Sus Integration

`cpkt_audio` and `cpkt_sus` are separate libraries. `cpkt_sus` may provide
convenience entry points that accept `cpkt_audio_decoder` only if the build and
package metadata keep library dependencies clear.

Current implementation status:

- `libcpktsus` does not link `libcpktaudio`;
- realtime integration accepts the public `cpkt_audio_decoder` receiver shell
  and calls its decoder method, so the two libraries remain independently
  consumable;
- examples and integration tests link both libraries when demonstrating the
  end-to-end audio-to-text workflow.

The desired workflow is:

1. Open audio through `cpkt_audio_decoder`.
2. Decode as `float32` mono 16000 Hz.
3. Feed bounded rolling PCM slices into `cpkt_sus_transcriber` using the
   whisper.cpp realtime stream loop (`step_ms`, `length_ms`, `keep_ms`).
4. Emit realtime transcript hypotheses through a callback sink.

If the dependency direction causes undesirable coupling, put cross-library
helpers in examples or an optional integration target instead of making
`libcpktsus` require `libcpktaudio`.

## Model Cache Table

The curated table must contain only whisper.cpp-loadable GGML model artifacts.
Each entry must include:

- stable public model name;
- provider/repository;
- source URL;
- filename;
- expected SHA-256;
- expected size when practical;
- license/provenance label;
- quantization label, if any;
- whether it is the default.

Current implementation status:

- curated OpenAI-derived and KBLab GGML entries are compiled into
  `libcpktsus`;
- public query helpers expose catalog count, lookup by index, lookup by name,
  and default entry selection;
- catalog strings are facade-owned and valid for process lifetime;
- exact checksums and sizes are pinned in the compiled catalog and covered by
  unit tests for representative default, KBLab, and quantized entries.

Initial OpenAI-derived whisper.cpp GGML entries:

- `tiny`
- `tiny.en`
- `tiny:q5_1`
- `tiny.en:q5_1`
- `base`
- `base.en`
- `base:q5_1`
- `base.en:q5_1`
- `small`
- `small.en`
- `small:q5_1`
- `small.en:q5_1`
- `medium`
- `medium.en`
- `medium:q5_0`
- `medium.en:q5_0`
- `large-v3`
- `large-v3:q5_0`
- `large-v3-turbo`
- `large-v3-turbo:q5_0`

The default cached model is `small`, the multilingual OpenAI-derived
whisper.cpp GGML model.

Initial KBLab Swedish-optimized GGML entries:

- `kb-whisper-tiny`
- `kb-whisper-tiny:q5_0`
- `kb-whisper-base`
- `kb-whisper-base:q5_0`
- `kb-whisper-small`
- `kb-whisper-small:q5_0`
- `kb-whisper-medium`
- `kb-whisper-medium:q5_0`
- `kb-whisper-large`
- `kb-whisper-large:q5_0`

For example:

```c
config.model = "kb-whisper-small";
cpkt_sus_model_open_cached(&model, &config);
```

This should select KBLab's Swedish-optimized GGML model, not its Transformers,
ONNX, PyTorch, or safetensors artifacts.

Quantized variants should be explicit. Recommended naming:

```text
kb-whisper-small:q5_0
small:q5_0
```

The default for each model name without a quantization suffix should be the
full GGML model file, not a quantized variant.

## Cache Path Policy

Cache directory precedence:

1. explicit `cache_dir` in `cpkt_sus_cache_config`;
2. `$XDG_CACHE_HOME/cpkt/susurro/models`;
3. `$HOME/.cache/cpkt/susurro/models`;
4. platform-specific fallback only if the repository already has or introduces
   a tested cpkt path helper.

Downloaded models must be written atomically:

1. download to a unique temporary file in the destination directory;
2. fsync or platform-equivalent flush where practical;
3. verify expected checksum or caller override;
4. verify that whisper.cpp can open/load the model;
5. atomically rename into the final path.

If an existing cache file fails checksum or load validation, `open_cached` may
retry by replacing it atomically. Failure diagnostics must identify whether the
problem was lookup, download, checksum, file I/O, or model load.

## Verification Requirements

Each new public behavior needs executable verification.

Required checks:

- public headers compile standalone as C89 and C++;
- receiver-style examples build and avoid free-function happy-path usage;
- `cpkt_audio` decoder opens WAV/MP3/FLAC fixtures when supported;
- `cpkt_audio` URL decoder streams through libcurl without full-response
  materialization;
- decoder callback reader handles fragmented reads, EOF, seek failures, and
  callback errors;
- seekless callback readers are rejected safely when the backend format needs
  seeking;
- decoder output is `float32` mono 16000 Hz for whisper input;
- encoder support is capability-tested and does not overclaim formats;
- `cpkt_sus` model open from path reports load failures cleanly;
- `cpkt_sus` cache resolver does not perform network access from non-cache
  constructors;
- cache resolver rejects missing or mismatched checksums by default;
- user checksum override replaces the pinned catalog checksum before model-load
  validation;
- insecure/no-checksum mode is explicit and test-covered;
- atomic cache replacement does not leave corrupt final files after simulated
  failures;
- segment callbacks receive text and timestamps;
- progress callbacks are observable;
- abort callbacks are observable and return `CPKT_SUS_ABORTED`;
- materialized transcript helpers and after-the-fact revised text retrieval use
  project-owned allocation/free;
- package artifacts include correct licenses and provenance manifests;
- package metadata records which whisper.cpp/ggml backends were compiled into
  each target artifact;
- backend capability reporting returns at least `cpu` for the first
  implementation;
- toolchain/package verification proves each Linux `cpkt_sus` target has
  cpkt-provided `libstdc++.a` and `libgcc.a` runtime archives;
- static package smoke tests link a C consumer with `cc` through
  `pkg-config --static cpktsus`, without `g++` and without host/system
  `libstdc++` paths;
- static package smoke tests link a mixed C/C++ consumer where the final link is
  still performed by `cc` and all C++ runtime resolution comes from the
  cpkt-provided runtime archives emitted by package metadata;
- shared package verification confirms `libcpktsus.so` exports only intended
  public facade symbols and does not require downstream `libstdc++.so` or
  `libgcc_s.so`;
- package verification rejects local paths and verifies runtime loader metadata.

Model-download tests that require network access must be opt-in. Local tests
should use small fixtures, fake download transports, or injected local model
files.

## Deferred Decisions

The first implementation has resolved the initial library split, encoder
format, model catalog, and quantized-alias policy:

- `cpktsus` remains independent from `cpktaudio` at link time;
- WAV is the only advertised encoder format until another miniaudio encoder
  path is deliberately enabled and tested;
- curated OpenAI-derived and KBLab GGML model entries carry pinned SHA-256 and
  size metadata in source and mirrored documentation;
- quantized model aliases use explicit `name:q5_0` or `name:q5_1` suffixes.

Remaining deferred decisions:

- Whether a later release exposes whisper.cpp VAD or tinydiarize features.
- When to introduce Vulkan/CUDA artifacts and whether they use direct linked
  backends or `GGML_BACKEND_DL`.
- Whether runtime GPU device selection is exposed with GPU-enabled artifacts or
  deferred until a concrete multi-device workflow requires it.
