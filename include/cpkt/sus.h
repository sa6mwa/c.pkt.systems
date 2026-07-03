#ifndef CPKT_SUS_H
#define CPKT_SUS_H

#ifdef __cplusplus
extern "C" {
#endif

/** Handle for a single speech transcription facade instance. */
typedef struct cpkt_sus cpkt_sus;

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
  CPKT_SUS_ERR_UPSTREAM = 4
} cpkt_sus_result;

/** Speech facade construction options. */
typedef struct cpkt_sus_config {
  /** Path to a model file on disk. */
  const char *model_path;
  /** Non-zero requests CPU-only execution when the backend supports it. */
  int cpu_only;
} cpkt_sus_config;

/** Runtime information for an opened speech facade instance. */
typedef struct cpkt_sus_info {
  /** Backend version string owned by the facade. */
  const char *backend_version;
  /** Backend system description string owned by the facade. */
  const char *backend_system_info;
  /** Non-zero when this instance was configured for CPU-only execution. */
  int cpu_only;
} cpkt_sus_info;

/** Receiver shell for speech facade operations. */
struct cpkt_sus {
  /** Private implementation pointer. Callers must not inspect or modify it. */
  void *impl;
  /** Fills info with backend and execution details for this instance. */
  cpkt_sus_result (*info)(const cpkt_sus *self, cpkt_sus_info *info);
  /** Releases the instance and all resources owned by the handle. */
  void (*destroy)(cpkt_sus *self);
};

/**
 * Opens a speech facade instance from a model path.
 *
 * On success, *out receives a handle that must be destroyed with its destroy
 * receiver. On failure, *out is set to NULL when out is non-NULL.
 */
cpkt_sus_result cpkt_sus_open_model(
    cpkt_sus **out,
    const cpkt_sus_config *config);

/** Returns the linked backend version string. */
const char *cpkt_sus_backend_version(void);
/** Returns the linked backend system information string. */
const char *cpkt_sus_backend_system_info(void);
/** Returns the public facade ABI version string. */
const char *cpkt_sus_facade_version(void);
/** Returns a stable human-readable string for a speech result code. */
const char *cpkt_sus_result_string(cpkt_sus_result result);

#ifdef __cplusplus
}
#endif

#endif
