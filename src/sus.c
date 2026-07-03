#include <cpkt/sus.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <whisper.h>

#ifndef CPKT_SUS_FACADE_VERSION
#define CPKT_SUS_FACADE_VERSION "0"
#endif

struct cpkt_sus_impl {
  struct whisper_context *context;
  int cpu_only;
};

static cpkt_sus_result cpkt_sus_info_impl(
    const cpkt_sus *self,
    cpkt_sus_info *info) {
  struct cpkt_sus_impl *impl;

  if (info != NULL) {
    memset(info, 0, sizeof(*info));
  }
  if (self == NULL || self->impl == NULL || info == NULL) {
    return CPKT_SUS_ERR_ARG;
  }

  impl = (struct cpkt_sus_impl *)self->impl;
  info->backend_version = whisper_version();
  info->backend_system_info = whisper_print_system_info();
  info->cpu_only = impl->cpu_only;
  return CPKT_SUS_OK;
}

static void cpkt_sus_destroy_impl(cpkt_sus *self) {
  struct cpkt_sus_impl *impl;

  if (self == NULL) {
    return;
  }

  impl = (struct cpkt_sus_impl *)self->impl;
  if (impl != NULL) {
    if (impl->context != NULL) {
      whisper_free(impl->context);
    }
    free(impl);
  }
  free(self);
}

/** Opens a receiver-shell speech facade instance for a model path. */
cpkt_sus_result cpkt_sus_open_model(
    cpkt_sus **out,
    const cpkt_sus_config *config) {
  cpkt_sus *sus;
  struct cpkt_sus_impl *impl;
  struct whisper_context_params params;

  if (out != NULL) {
    *out = NULL;
  }
  if (out == NULL || config == NULL || config->model_path == NULL ||
      config->model_path[0] == '\0') {
    return CPKT_SUS_ERR_ARG;
  }

  sus = (cpkt_sus *)calloc(1, sizeof(*sus));
  if (sus == NULL) {
    return CPKT_SUS_ERR_ALLOC;
  }
  impl = (struct cpkt_sus_impl *)calloc(1, sizeof(*impl));
  if (impl == NULL) {
    free(sus);
    return CPKT_SUS_ERR_ALLOC;
  }

  params = whisper_context_default_params();
  if (config->cpu_only) {
    params.use_gpu = false;
  }

  impl->context = whisper_init_from_file_with_params(config->model_path, params);
  if (impl->context == NULL) {
    free(impl);
    free(sus);
    return CPKT_SUS_ERR_MODEL;
  }
  impl->cpu_only = config->cpu_only ? 1 : 0;

  sus->impl = impl;
  sus->info = cpkt_sus_info_impl;
  sus->destroy = cpkt_sus_destroy_impl;
  *out = sus;
  return CPKT_SUS_OK;
}

/** Returns the linked backend version string. */
const char *cpkt_sus_backend_version(void) {
  return whisper_version();
}

/** Returns the linked backend system information string. */
const char *cpkt_sus_backend_system_info(void) {
  return whisper_print_system_info();
}

/** Returns the public facade ABI version string. */
const char *cpkt_sus_facade_version(void) {
  return CPKT_SUS_FACADE_VERSION;
}

/** Converts a speech result code into a stable diagnostic string. */
const char *cpkt_sus_result_string(cpkt_sus_result result) {
  switch (result) {
  case CPKT_SUS_OK:
    return "ok";
  case CPKT_SUS_ERR_ARG:
    return "invalid argument";
  case CPKT_SUS_ERR_ALLOC:
    return "allocation failed";
  case CPKT_SUS_ERR_MODEL:
    return "model load failed";
  case CPKT_SUS_ERR_UPSTREAM:
    return "upstream error";
  default:
    return "unknown result";
  }
}
