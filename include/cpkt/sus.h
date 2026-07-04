#ifndef CPKT_SUS_H
#define CPKT_SUS_H

#ifdef __cplusplus
extern "C" {
#endif

/** Handle for a loaded speech model. */
typedef struct cpkt_sus_model cpkt_sus_model;
/** Handle for a transcription instance bound to a loaded model. */
typedef struct cpkt_sus_transcriber cpkt_sus_transcriber;
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

/** Receives backend progress in percent. Return non-zero to report failure. */
typedef int (*cpkt_sus_progress_sink)(int progress, void *user);

/** Return non-zero to request aborting the active transcription. */
typedef int (*cpkt_sus_abort_fn)(void *user);

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
   * Runs chunked/windowed inference over float32 mono 16000 Hz PCM samples.
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
