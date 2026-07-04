#include <cpkt/sus.h>

#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <whisper.h>

#ifndef CPKT_SUS_FACADE_VERSION
#define CPKT_SUS_FACADE_VERSION "0"
#endif

struct cpkt_sus_model_impl {
  struct whisper_context *context;
  int cpu_only;
};

struct cpkt_sus_transcriber_impl {
  cpkt_sus_model *model;
  cpkt_sus_transcriber_config config;
  int callback_error;
};

static long cpkt_sus_i64_to_long(int64_t value) {
  if (value > (int64_t)LONG_MAX) {
    return LONG_MAX;
  }
  if (value < (int64_t)LONG_MIN) {
    return LONG_MIN;
  }
  return (long)value;
}

static cpkt_sus_result cpkt_sus_info_impl(const cpkt_sus_model *self,
                                          cpkt_sus_info *info) {
  struct cpkt_sus_model_impl *impl;

  if (info != NULL) {
    memset(info, 0, sizeof(*info));
  }
  if (self == NULL || self->impl == NULL || info == NULL) {
    return CPKT_SUS_ERR_ARG;
  }

  impl = (struct cpkt_sus_model_impl *)self->impl;
  info->backend_version = whisper_version();
  info->backend_system_info = whisper_print_system_info();
  info->cpu_only = impl->cpu_only;
  return CPKT_SUS_OK;
}

static void cpkt_sus_model_destroy_impl(cpkt_sus_model *self) {
  struct cpkt_sus_model_impl *impl;

  if (self == NULL) {
    return;
  }

  impl = (struct cpkt_sus_model_impl *)self->impl;
  if (impl != NULL) {
    if (impl->context != NULL) {
      whisper_free(impl->context);
    }
    free(impl);
  }
  free(self);
}

static void
cpkt_sus_whisper_new_segment_callback(struct whisper_context *context,
                                      struct whisper_state *state,
                                      int new_count, void *user_data) {
  struct cpkt_sus_transcriber_impl *impl;
  cpkt_sus_segment segment;
  const char *text;
  int first;
  int i;
  int total;
  int sink_result;

  (void)context;
  impl = (struct cpkt_sus_transcriber_impl *)user_data;
  if (impl == NULL || impl->config.segment_sink == NULL ||
      impl->callback_error) {
    return;
  }

  total = whisper_full_n_segments_from_state(state);
  first = total - new_count;
  if (first < 0) {
    first = 0;
  }

  for (i = first; i < total; ++i) {
    text = whisper_full_get_segment_text_from_state(state, i);
    memset(&segment, 0, sizeof(segment));
    segment.text = text;
    segment.text_length = text == NULL ? 0UL : (unsigned long)strlen(text);
    segment.t0 =
        cpkt_sus_i64_to_long(whisper_full_get_segment_t0_from_state(state, i));
    segment.t1 =
        cpkt_sus_i64_to_long(whisper_full_get_segment_t1_from_state(state, i));

    sink_result =
        impl->config.segment_sink(&segment, impl->config.segment_user);
    if (sink_result != 0) {
      impl->callback_error = 1;
      return;
    }
  }
}

static void cpkt_sus_whisper_progress_callback(struct whisper_context *context,
                                               struct whisper_state *state,
                                               int progress, void *user_data) {
  struct cpkt_sus_transcriber_impl *impl;

  (void)context;
  (void)state;
  impl = (struct cpkt_sus_transcriber_impl *)user_data;
  if (impl == NULL || impl->config.progress_sink == NULL ||
      impl->callback_error) {
    return;
  }
  if (impl->config.progress_sink(progress, impl->config.progress_user) != 0) {
    impl->callback_error = 1;
  }
}

static bool cpkt_sus_whisper_abort_callback(void *user_data) {
  struct cpkt_sus_transcriber_impl *impl;

  impl = (struct cpkt_sus_transcriber_impl *)user_data;
  if (impl == NULL || impl->config.abort == NULL) {
    return impl != NULL && impl->callback_error ? true : false;
  }
  return impl->callback_error ||
                 impl->config.abort(impl->config.abort_user) != 0
             ? true
             : false;
}

static cpkt_sus_result cpkt_sus_transcriber_run(cpkt_sus_transcriber *self,
                                                const float *samples,
                                                unsigned long sample_count) {
  struct cpkt_sus_transcriber_impl *impl;
  struct cpkt_sus_model_impl *model_impl;
  struct whisper_full_params params;
  const char *language;
  int full_result;

  if (self == NULL || self->impl == NULL ||
      (samples == NULL && sample_count != 0UL) ||
      sample_count > (unsigned long)INT_MAX) {
    return CPKT_SUS_ERR_ARG;
  }

  impl = (struct cpkt_sus_transcriber_impl *)self->impl;
  if (impl->model == NULL || impl->model->impl == NULL) {
    return CPKT_SUS_ERR_ARG;
  }
  model_impl = (struct cpkt_sus_model_impl *)impl->model->impl;
  if (model_impl->context == NULL) {
    return CPKT_SUS_ERR_ARG;
  }

  impl->callback_error = 0;
  params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
  if (impl->config.threads > 0) {
    params.n_threads = impl->config.threads;
  }
  params.translate = impl->config.translate ? true : false;
  params.no_timestamps = impl->config.timestamps ? false : true;
  params.print_progress = false;
  params.print_realtime = false;
  params.print_timestamps = false;
  params.initial_prompt = impl->config.initial_prompt;

  language = impl->config.language;
  if (language == NULL || language[0] == '\0' ||
      strcmp(language, "auto") == 0) {
    params.language = NULL;
  } else {
    params.language = language;
  }

  if (impl->config.segment_sink != NULL) {
    params.new_segment_callback = cpkt_sus_whisper_new_segment_callback;
    params.new_segment_callback_user_data = impl;
  }
  if (impl->config.progress_sink != NULL) {
    params.progress_callback = cpkt_sus_whisper_progress_callback;
    params.progress_callback_user_data = impl;
  }
  if (impl->config.abort != NULL || impl->config.segment_sink != NULL ||
      impl->config.progress_sink != NULL) {
    params.abort_callback = cpkt_sus_whisper_abort_callback;
    params.abort_callback_user_data = impl;
  }

  full_result =
      whisper_full(model_impl->context, params, samples, (int)sample_count);
  if (impl->callback_error) {
    return CPKT_SUS_ERR_CALLBACK;
  }
  if (full_result != 0) {
    return CPKT_SUS_ERR_UPSTREAM;
  }
  return CPKT_SUS_OK;
}

static cpkt_sus_result
cpkt_sus_transcriber_transcribe_f32_mono_16k_impl(cpkt_sus_transcriber *self,
                                                  const float *samples,
                                                  unsigned long sample_count) {
  return cpkt_sus_transcriber_run(self, samples, sample_count);
}

static cpkt_sus_result cpkt_sus_transcriber_transcribe_f32_mono_16k_text_impl(
    cpkt_sus_transcriber *self, const float *samples,
    unsigned long sample_count, char **text_out) {
  struct cpkt_sus_transcriber_impl *impl;
  struct cpkt_sus_model_impl *model_impl;
  cpkt_sus_result result;
  char *text;
  const char *segment_text;
  size_t length;
  size_t segment_length;
  int count;
  int i;

  if (text_out != NULL) {
    *text_out = NULL;
  }
  if (text_out == NULL) {
    return CPKT_SUS_ERR_ARG;
  }

  result = cpkt_sus_transcriber_run(self, samples, sample_count);
  if (result != CPKT_SUS_OK) {
    return result;
  }

  impl = (struct cpkt_sus_transcriber_impl *)self->impl;
  model_impl = (struct cpkt_sus_model_impl *)impl->model->impl;
  count = whisper_full_n_segments(model_impl->context);
  length = 0U;
  for (i = 0; i < count; ++i) {
    segment_text = whisper_full_get_segment_text(model_impl->context, i);
    if (segment_text != NULL) {
      segment_length = strlen(segment_text);
      if (segment_length > ((size_t)-1) - length - 1U) {
        return CPKT_SUS_ERR_ALLOC;
      }
      length += segment_length;
    }
  }

  text = (char *)malloc(length + 1U);
  if (text == NULL) {
    return CPKT_SUS_ERR_ALLOC;
  }
  length = 0U;
  for (i = 0; i < count; ++i) {
    segment_text = whisper_full_get_segment_text(model_impl->context, i);
    if (segment_text != NULL) {
      segment_length = strlen(segment_text);
      memcpy(text + length, segment_text, segment_length);
      length += segment_length;
    }
  }
  text[length] = '\0';
  *text_out = text;
  return CPKT_SUS_OK;
}

static void cpkt_sus_transcriber_destroy_impl(cpkt_sus_transcriber *self) {
  if (self == NULL) {
    return;
  }
  free(self->impl);
  free(self);
}

static cpkt_sus_result cpkt_sus_model_create_transcriber_impl(
    cpkt_sus_model *self, cpkt_sus_transcriber **out,
    const cpkt_sus_transcriber_config *config) {
  cpkt_sus_transcriber *transcriber;
  struct cpkt_sus_transcriber_impl *impl;

  if (out != NULL) {
    *out = NULL;
  }
  if (out == NULL || self == NULL || self->impl == NULL) {
    return CPKT_SUS_ERR_ARG;
  }

  transcriber = (cpkt_sus_transcriber *)calloc(1, sizeof(*transcriber));
  if (transcriber == NULL) {
    return CPKT_SUS_ERR_ALLOC;
  }
  impl = (struct cpkt_sus_transcriber_impl *)calloc(1, sizeof(*impl));
  if (impl == NULL) {
    free(transcriber);
    return CPKT_SUS_ERR_ALLOC;
  }

  impl->model = self;
  if (config != NULL) {
    impl->config = *config;
  }

  transcriber->impl = impl;
  transcriber->transcribe_f32_mono_16k =
      cpkt_sus_transcriber_transcribe_f32_mono_16k_impl;
  transcriber->transcribe_f32_mono_16k_text =
      cpkt_sus_transcriber_transcribe_f32_mono_16k_text_impl;
  transcriber->destroy = cpkt_sus_transcriber_destroy_impl;
  *out = transcriber;
  return CPKT_SUS_OK;
}

/** Opens a receiver-shell speech model from a model path. */
cpkt_sus_result cpkt_sus_model_open_path(cpkt_sus_model **out,
                                         const cpkt_sus_model_config *config) {
  cpkt_sus_model *model;
  struct cpkt_sus_model_impl *impl;
  struct whisper_context_params params;

  if (out != NULL) {
    *out = NULL;
  }
  if (out == NULL || config == NULL || config->model_path == NULL ||
      config->model_path[0] == '\0') {
    return CPKT_SUS_ERR_ARG;
  }

  model = (cpkt_sus_model *)calloc(1, sizeof(*model));
  if (model == NULL) {
    return CPKT_SUS_ERR_ALLOC;
  }
  impl = (struct cpkt_sus_model_impl *)calloc(1, sizeof(*impl));
  if (impl == NULL) {
    free(model);
    return CPKT_SUS_ERR_ALLOC;
  }

  params = whisper_context_default_params();
  if (config->cpu_only) {
    params.use_gpu = false;
  }

  impl->context =
      whisper_init_from_file_with_params(config->model_path, params);
  if (impl->context == NULL) {
    free(impl);
    free(model);
    return CPKT_SUS_ERR_MODEL;
  }
  impl->cpu_only = config->cpu_only ? 1 : 0;

  model->impl = impl;
  model->info = cpkt_sus_info_impl;
  model->create_transcriber = cpkt_sus_model_create_transcriber_impl;
  model->destroy = cpkt_sus_model_destroy_impl;
  *out = model;
  return CPKT_SUS_OK;
}

/** Opens a compatibility speech model handle from a model path. */
cpkt_sus_result cpkt_sus_open_model(cpkt_sus **out,
                                    const cpkt_sus_config *config) {
  return cpkt_sus_model_open_path((cpkt_sus_model **)out, config);
}

/** Opens a cache-backed model handle once the resolver is wired. */
cpkt_sus_result
cpkt_sus_model_open_cached(cpkt_sus_model **out,
                           const cpkt_sus_cache_config *config) {
  if (out != NULL) {
    *out = NULL;
  }
  if (out == NULL || config == NULL || config->model == NULL ||
      config->model[0] == '\0') {
    return CPKT_SUS_ERR_ARG;
  }
  return CPKT_SUS_ERR_MODEL;
}

/** Creates a transcriber bound to an opened model. */
cpkt_sus_result
cpkt_sus_model_create_transcriber(cpkt_sus_model *model,
                                  cpkt_sus_transcriber **out,
                                  const cpkt_sus_transcriber_config *config) {
  if (out != NULL) {
    *out = NULL;
  }
  if (model == NULL || model->create_transcriber == NULL) {
    return CPKT_SUS_ERR_ARG;
  }
  return model->create_transcriber(model, out, config);
}

/** Releases strings allocated by materialized transcription helpers. */
void cpkt_sus_string_free(char *text) { free(text); }

/** Returns the linked backend version string. */
const char *cpkt_sus_backend_version(void) { return whisper_version(); }

/** Returns the linked backend system information string. */
const char *cpkt_sus_backend_system_info(void) {
  return whisper_print_system_info();
}

/** Returns the public facade ABI version string. */
const char *cpkt_sus_facade_version(void) { return CPKT_SUS_FACADE_VERSION; }

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
  case CPKT_SUS_ERR_CALLBACK:
    return "callback error";
  default:
    return "unknown result";
  }
}
