#ifndef CPKT_SUS_H
#define CPKT_SUS_H

#ifdef __cplusplus
extern "C" {
#endif

/** Handle for a loaded speech model. */
typedef struct cpkt_sus_model cpkt_sus_model;
/** Handle for a transcription instance bound to a loaded model. */
typedef struct cpkt_sus_transcriber cpkt_sus_transcriber;
/** Opaque audio decoder handle accepted by audio streaming helpers. */
#ifndef CPKT_AUDIO_DECODER_TYPEDEF
#define CPKT_AUDIO_DECODER_TYPEDEF
typedef struct cpkt_audio_decoder cpkt_audio_decoder;
#endif
#ifndef CPKT_AUDIO_VOX_SEGMENT_TYPEDEF
#define CPKT_AUDIO_VOX_SEGMENT_TYPEDEF
/** Pullable audio VOX segment accepted by speech streaming helpers. */
typedef struct cpkt_audio_vox_segment cpkt_audio_vox_segment;
#endif
/** Compatibility alias for the initial combined model handle name. */
typedef cpkt_sus_model cpkt_sus;

/** Result codes returned by the speech facade. */
typedef enum cpkt_sus_result {
  /** Operation completed successfully. */
  CPKT_SUS_OK = 0,
  /** A required argument was missing, invalid, or inconsistent. */
  CPKT_SUS_ERR_ARG = 1,
  /** Memory allocation failed. */
  CPKT_SUS_ERR_ALLOC = 2,
  /** The requested model file could not be loaded. */
  CPKT_SUS_ERR_MODEL = 3,
  /** The backend returned an error not covered by a narrower result code. */
  CPKT_SUS_ERR_UPSTREAM = 4,
  /** A caller callback reported failure. */
  CPKT_SUS_ERR_CALLBACK = 5,
  /** A named cached model is not in the curated resolver table. */
  CPKT_SUS_ERR_LOOKUP = 6,
  /** A required local file or cache path could not be used. */
  CPKT_SUS_ERR_IO = 7,
  /** A model file checksum did not match the pinned or caller checksum. */
  CPKT_SUS_ERR_CHECKSUM = 8,
  /** A cache download failed. */
  CPKT_SUS_ERR_NETWORK = 9,
  /** The caller abort callback requested stopping transcription. */
  CPKT_SUS_ABORTED = 10
} cpkt_sus_result;

/** Speech model construction options. */
typedef struct cpkt_sus_model_config {
  /** Path to a model file on disk. */
  const char *model_path;
  /** Non-zero requests CPU-only execution when the backend supports it. */
  int cpu_only;
} cpkt_sus_model_config;

/** Compatibility alias for the initial model-open configuration name. */
typedef cpkt_sus_model_config cpkt_sus_config;

/** Cache resolver status phases emitted while opening cached models. */
typedef enum cpkt_sus_cache_status_phase {
  /** The resolver selected a cached-model catalog entry. */
  CPKT_SUS_CACHE_STATUS_LOOKUP = 1,
  /** The expected cache file already exists and will be validated. */
  CPKT_SUS_CACHE_STATUS_HIT = 2,
  /** The expected cache file is absent and will be fetched. */
  CPKT_SUS_CACHE_STATUS_MISS = 3,
  /** The resolver is downloading the selected model to a temporary file. */
  CPKT_SUS_CACHE_STATUS_DOWNLOAD_BEGIN = 4,
  /** The temporary downloaded model file is complete. */
  CPKT_SUS_CACHE_STATUS_DOWNLOAD_COMPLETE = 5,
  /** The resolver is validating checksum and model loadability. */
  CPKT_SUS_CACHE_STATUS_VERIFY_BEGIN = 6,
  /** The resolver finished validation and will atomically publish the file. */
  CPKT_SUS_CACHE_STATUS_VERIFY_COMPLETE = 7,
  /** The resolver is loading the validated cached model. */
  CPKT_SUS_CACHE_STATUS_LOAD_BEGIN = 8
} cpkt_sus_cache_status_phase;

/** Cache resolver status event delivered during cached model opens. */
typedef struct cpkt_sus_cache_status_event {
  /** One of cpkt_sus_cache_status_phase. Kept int-sized for ABI stability. */
  int phase;
  /** Public model name selected by the resolver. */
  const char *model;
  /** Cache path being checked, downloaded, verified, or loaded. */
  const char *cache_path;
  /** Source URL used for download phases, otherwise NULL. */
  const char *source_url;
} cpkt_sus_cache_status_event;

/**
 * Receives cache resolver status. Return zero to continue; non-zero aborts the
 * cached open with CPKT_SUS_ERR_CALLBACK.
 */
typedef int (*cpkt_sus_cache_status_sink)(
    const cpkt_sus_cache_status_event *event, void *user);

/** Cache-backed model resolver options. Network access is explicit to this
 * path. */
typedef struct cpkt_sus_cache_config {
  /** Public model name such as "small" or a provider-specific alias. */
  const char *model;
  /** Optional cache directory. NULL selects the documented cpkt cache path. */
  const char *cache_dir;
  /** Optional lowercase hex SHA-256 override for the selected model. */
  const char *sha256;
  /** Optional source URL override for controlled mirrors or tests. */
  const char *source_url;
  /** Non-zero disables checksum enforcement. This is insecure and off by
   * default. */
  int insecure_no_checksum;
  /** Non-zero disables downloading and requires an existing cache file. */
  int offline;
  /** Non-zero requests CPU-only execution when the backend supports it. */
  int cpu_only;
  /** Optional cache resolver status sink. */
  cpkt_sus_cache_status_sink status_sink;
  /** User value passed to status_sink. */
  void *status_user;
} cpkt_sus_cache_config;

/** Runtime information for an opened speech facade instance. */
typedef struct cpkt_sus_info {
  /** Backend version string owned by the facade. */
  const char *backend_version;
  /** Backend system description string owned by the facade. */
  const char *backend_system_info;
  /** Non-zero when this instance was configured for CPU-only execution. */
  int cpu_only;
} cpkt_sus_info;

/** Transcript segment delivered to caller sinks. */
typedef struct cpkt_sus_segment {
  /** Segment text owned by the facade and valid only during the callback. */
  const char *text;
  /** Segment text length in bytes, excluding the terminating NUL. */
  unsigned long text_length;
  /** Start time in 10 ms units. */
  long t0;
  /** End time in 10 ms units. */
  long t1;
} cpkt_sus_segment;

/** Committed streaming transcript state delivered after a VOX segment closes.
 */
typedef struct cpkt_sus_segmented_event {
  /** Current session transcript owned by the facade during the callback. */
  const char *text;
  /** Transcript text length in bytes, excluding the terminating NUL. */
  unsigned long text_length;
  /** Start time in 10 ms units on the input PCM timeline. */
  long t0;
  /** End time in 10 ms units on the input PCM timeline. */
  long t1;
  /** Number of committed speech segments completed before this event. */
  unsigned long step_index;
  /** Non-zero only when the event is the final end-of-stream event. */
  int is_final;
} cpkt_sus_segmented_event;

/** Backend log levels exposed through the speech facade. */
typedef enum cpkt_sus_log_level {
  CPKT_SUS_LOG_NONE = 0,
  CPKT_SUS_LOG_DEBUG = 1,
  CPKT_SUS_LOG_INFO = 2,
  CPKT_SUS_LOG_WARN = 3,
  CPKT_SUS_LOG_ERROR = 4,
  /** Reserved continuation level; backend continuations are normalized. */
  CPKT_SUS_LOG_CONT = 5
} cpkt_sus_log_level;

/** Backend log event delivered to caller logging sinks. */
typedef struct cpkt_sus_log_event {
  /** One of cpkt_sus_log_level. Kept int-sized for ABI stability. */
  int level;
  /** Facade component that produced the log event, such as "backend". */
  const char *component;
  /** Backend-owned log text valid only during the callback. */
  const char *message;
} cpkt_sus_log_event;

/** Receives backend log events. The sink must not retain event pointers. */
typedef void (*cpkt_sus_log_sink)(const cpkt_sus_log_event *event, void *user);

/** Read-only curated cached-model catalog entry. */
typedef struct cpkt_sus_model_entry {
  /** Stable public model name accepted by cpkt_sus_model_open_cached. */
  const char *name;
  /** Provider and repository identifier for the model artifact. */
  const char *provider;
  /** Source URL used by the default cache fetcher. */
  const char *source_url;
  /** Cache filename used for this model. */
  const char *filename;
  /** Lowercase hex SHA-256 expected for the source artifact. */
  const char *sha256;
  /** Expected source artifact size in bytes when known, or zero. */
  unsigned long size_bytes;
  /** License or provenance label for the model artifact. */
  const char *license;
  /** Quantization label such as "f16", "q5_0", or "q5_1". */
  const char *quantization;
  /** Non-zero when this entry is the default cached model. */
  int is_default;
} cpkt_sus_model_entry;

/**
 * Receives a transcript segment.
 *
 * Return zero to continue. Returning non-zero requests CPKT_SUS_ERR_CALLBACK
 * after the backend call completes.
 */
typedef int (*cpkt_sus_segment_sink)(const cpkt_sus_segment *segment,
                                     void *user);

/**
 * Receives committed streaming transcript updates.
 *
 * Return zero to continue. Returning non-zero requests CPKT_SUS_ERR_CALLBACK
 * after the active backend call completes.
 */
typedef int (*cpkt_sus_segmented_sink)(const cpkt_sus_segmented_event *event,
                                      void *user);

/** Receives backend or facade progress in percent. Return non-zero to fail. */
typedef int (*cpkt_sus_progress_sink)(int progress, void *user);

/** Return non-zero to request aborting the active transcription. */
typedef int (*cpkt_sus_abort_fn)(void *user);

/** Segmentation policy for VOX-driven transcription. */
typedef enum cpkt_sus_segment_mode {
  /**
   * Simplex turn mode. Zero-initialized configs select this mode. A zero
   * length_ms disables the time cap and relies on VOX release or
   * max_spool_bytes for a forced segment boundary.
   */
  CPKT_SUS_SEGMENT_MODE_SIMPLEX = 0,
  /**
   * Continuous segmented transcription mode. Input may come from a file, URL,
   * stream, or capture device. A zero length_ms selects the library continuous
   * default segment budget, currently 7000 ms.
   */
  CPKT_SUS_SEGMENT_MODE_CONTINUOUS = 1
} cpkt_sus_segment_mode;

/** Transcriber construction options. */
typedef struct cpkt_sus_transcriber_config {
  /** Number of backend threads. Zero selects the backend default. */
  int threads;
  /** Non-zero requests CPU-only runtime behavior where supported. */
  int cpu_only;
  /** Language code. NULL, empty, or "auto" selects auto-detection. */
  const char *language;
  /** Non-zero translates to English instead of transcribing. */
  int translate;
  /** Non-zero enables timestamped segment output. */
  int timestamps;
  /** Optional initial prompt passed to the backend. */
  const char *initial_prompt;
  /** Optional segment sink for callback-oriented output. */
  cpkt_sus_segment_sink segment_sink;
  /** User value passed to segment_sink. */
  void *segment_user;
  /** Optional progress sink. */
  cpkt_sus_progress_sink progress_sink;
  /** User value passed to progress_sink. */
  void *progress_user;
  /** Optional abort callback. */
  cpkt_sus_abort_fn abort;
  /** User value passed to abort. */
  void *abort_user;
} cpkt_sus_transcriber_config;

/** VOX-segmented audio-decoder transcription options. Zero initializes
 * defaults. */
typedef struct cpkt_sus_segmented_config {
  /**
   * Segmentation policy. Zero selects CPKT_SUS_SEGMENT_MODE_SIMPLEX.
   */
  cpkt_sus_segment_mode mode;
  /**
   * Frames pulled from the audio decoder per read. Zero selects 4096 frames.
   */
  unsigned long read_frames;
  /**
   * Deprecated compatibility field. The VOX path does not run fixed-step
   * inference.
   */
  unsigned long step_ms;
  /**
   * Maximum VOX speech segment passed to one inference call, in milliseconds.
   * In CPKT_SUS_SEGMENT_MODE_SIMPLEX, zero disables the time cap. In
   * CPKT_SUS_SEGMENT_MODE_CONTINUOUS, zero selects 7000 ms. A continuous speech
   * run beyond a non-zero budget is hard cut and continued with prior prompt
   * tokens, not prior audio.
   */
  unsigned long length_ms;
  /**
   * Silence duration that releases VOX, in milliseconds. Zero selects 1500 ms.
   */
  unsigned long keep_ms;
  /**
   * Prompt-token carry policy. Zero selects the default enabled behavior.
   * Negative disables prompt carry. Positive enables prompt carry.
   */
  int keep_context;
  /** VOX threshold for mono f32 16 kHz PCM. Zero selects 0.001. */
  float vox_threshold;
  /**
   * Audio retained before VOX opens, in milliseconds. Zero selects the audio
   * VOX default, currently 10 ms.
   */
  unsigned long prebuffer_ms;
  /**
   * Bytes kept in memory for one open VOX speech segment before spilling to an
   * anonymous temporary file. Zero selects the audio VOX default, currently
   * 1 MiB.
   */
  unsigned long memory_spool_bytes;
  /**
   * Maximum RAM plus disk-backed spool bytes for one open VOX speech segment
   * before a forced hard cut. Zero selects the audio VOX default, currently
   * 1 GiB.
   */
  unsigned long max_spool_bytes;
  /** Backend audio context override. Zero keeps the backend default. */
  unsigned long audio_ctx;
  /** Maximum tokens per inference call. Zero keeps the backend default. */
  unsigned long max_tokens;
  /** Optional sink for committed VOX-segment transcript updates. */
  cpkt_sus_segmented_sink segmented_sink;
  /** User value passed to segmented_sink. */
  void *segmented_user;
} cpkt_sus_segmented_config;

/** Receiver shell for loaded model operations. */
struct cpkt_sus_model {
  /** Private implementation pointer. Callers must not inspect or modify it. */
  void *impl;
  /** Fills info with backend and execution details for this instance. */
  cpkt_sus_result (*info)(const cpkt_sus_model *self, cpkt_sus_info *info);
  /** Creates a transcriber bound to this model. */
  cpkt_sus_result (*create_transcriber)(
      cpkt_sus_model *self, cpkt_sus_transcriber **out,
      const cpkt_sus_transcriber_config *config);
  /** Releases the loaded model and all resources owned by the handle. */
  void (*destroy)(cpkt_sus_model *self);
};

/** Receiver shell for transcription operations. */
struct cpkt_sus_transcriber {
  /** Private implementation pointer. Callers must not inspect or modify it. */
  void *impl;
  /**
   * Runs inference over a caller-provided float32 mono 16000 Hz PCM buffer.
   *
   * Segment and progress callbacks from the transcriber configuration are
   * invoked during the call when the backend reports them.
   */
  cpkt_sus_result (*transcribe_f32_mono_16k)(cpkt_sus_transcriber *self,
                                             const float *samples,
                                             unsigned long sample_count);
  /**
   * Runs inference and materializes the final text into a facade-owned string.
   *
   * The caller must release *text_out with cpkt_sus_string_free.
   */
  cpkt_sus_result (*transcribe_f32_mono_16k_text)(cpkt_sus_transcriber *self,
                                                  const float *samples,
                                                  unsigned long sample_count,
                                                  char **text_out);
  /**
   * Runs VOX-segmented streaming transcription over decoded audio.
   *
   * The decoder must produce float32 mono 16000 Hz PCM, as cpkt_audio_decoder
   * does. The transcriber reads until end-of-stream, uses audio VOX to cut
   * bounded speech segments, and runs inference once per emitted segment.
   * Previous audio is never retranscribed; prompt tokens from the previous
   * segment are used for continuity when prompt carry is enabled.
   */
  cpkt_sus_result (*transcribe_audio_decoder_segmented)(
      cpkt_sus_transcriber *self, cpkt_audio_decoder *decoder,
      const cpkt_sus_segmented_config *config);
  /**
   * Transcribes one cpktaudio VOX segment into this streaming session.
   *
   * The segment is consumed during the call. Previous audio is not
   * retranscribed; prompt tokens captured from prior calls on this transcriber
   * are used for continuity unless disabled by config->keep_context. The
   * session transcript is updated and delivered through config->segmented_sink
   * when provided.
   */
  cpkt_sus_result (*transcribe_audio_vox_segment)(
      cpkt_sus_transcriber *self, cpkt_audio_vox_segment *segment,
      const cpkt_sus_segmented_config *config);
  /**
   * Runs VOX-segmented decoder transcription and returns session text.
   *
   * Audio remains streaming and bounded as with
   * transcribe_audio_decoder_segmented. The returned text is assembled from
   * committed segment updates and must be released with cpkt_sus_string_free.
   */
  cpkt_sus_result (*transcribe_audio_decoder_segmented_text)(
      cpkt_sus_transcriber *self, cpkt_audio_decoder *decoder,
      const cpkt_sus_segmented_config *config, char **text_out);
  /**
   * Copies the latest committed streaming transcript from this transcriber.
   *
   * The text is produced by the most recent streaming decoder transcription
   * call. It is empty before any streaming call that produced text. The caller
   * must release *text_out with cpkt_sus_string_free.
   */
  cpkt_sus_result (*revised_text)(cpkt_sus_transcriber *self, char **text_out);
  /** Releases the transcriber. The loaded model remains owned by its model
   * handle. */
  void (*destroy)(cpkt_sus_transcriber *self);
};

/**
 * Opens a speech facade instance from a model path.
 *
 * On success, *out receives a handle that must be destroyed with its destroy
 * receiver. On failure, *out is set to NULL when out is non-NULL.
 */
cpkt_sus_result cpkt_sus_open_model(cpkt_sus **out,
                                    const cpkt_sus_config *config);

/** Opens a speech model from an explicit model path without network access. */
cpkt_sus_result cpkt_sus_model_open_path(cpkt_sus_model **out,
                                         const cpkt_sus_model_config *config);

/**
 * Opens a speech model through the explicit cache-backed resolver.
 *
 * A NULL or empty model name selects the default "small" multilingual model.
 * The resolver checks an existing cache entry at cache_dir, XDG_CACHE_HOME, or
 * HOME/.cache, verifies SHA-256 unless insecure_no_checksum is non-zero, and
 * then loads the model. Missing cache entries are downloaded through libcurl
 * unless offline is non-zero. source_url may override the curated URL, but the
 * checksum rules still apply.
 */
cpkt_sus_result cpkt_sus_model_open_cached(cpkt_sus_model **out,
                                           const cpkt_sus_cache_config *config);

/** Creates a transcriber bound to an opened model. */
cpkt_sus_result
cpkt_sus_model_create_transcriber(cpkt_sus_model *model,
                                  cpkt_sus_transcriber **out,
                                  const cpkt_sus_transcriber_config *config);

/** Releases strings allocated by cpkt_sus materialized-text helpers. */
void cpkt_sus_string_free(char *text);

/**
 * Sets the process-wide backend log sink used by cpktsus.
 *
 * The speech backend exposes logging as process-global state, so this facade
 * surface is also process-wide. Passing NULL silences backend logs, which is
 * the default behavior installed before model loading and inference.
 */
void cpkt_sus_log_set(cpkt_sus_log_sink sink, void *user);

/** Returns the number of curated cached-model catalog entries. */
unsigned long cpkt_sus_model_catalog_count(void);
/**
 * Copies a curated cached-model catalog entry by index.
 *
 * The strings referenced by *entry are owned by the facade for process
 * lifetime. Returns CPKT_SUS_ERR_LOOKUP when index is out of range.
 */
cpkt_sus_result cpkt_sus_model_catalog_entry(unsigned long index,
                                             cpkt_sus_model_entry *entry);
/**
 * Copies the curated cached-model catalog entry for name.
 *
 * NULL or empty name selects the default entry.
 */
cpkt_sus_result cpkt_sus_model_catalog_find(const char *name,
                                            cpkt_sus_model_entry *entry);
/** Copies the default curated cached-model catalog entry. */
cpkt_sus_result cpkt_sus_model_catalog_default(cpkt_sus_model_entry *entry);

/** Returns the linked backend version string. */
const char *cpkt_sus_backend_version(void);
/** Returns the linked backend system information string. */
const char *cpkt_sus_backend_system_info(void);
/** Returns compiled backend capabilities such as "cpu". */
const char *cpkt_sus_backend_capabilities(void);
/** Returns the public facade ABI version string. */
const char *cpkt_sus_facade_version(void);
/** Returns a stable human-readable string for a speech result code. */
const char *cpkt_sus_result_string(cpkt_sus_result result);

#ifdef __cplusplus
}
#endif

#endif
