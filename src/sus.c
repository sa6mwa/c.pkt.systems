#include <cpkt/sus.h>

#include <cpkt/audio.h>

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <curl/curl.h>
#include <openssl/evp.h>
#include <whisper.h>

#ifndef CPKT_SUS_FACADE_VERSION
#define CPKT_SUS_FACADE_VERSION "0"
#endif

#ifndef CPKT_SUS_BACKEND_CAPABILITIES
#define CPKT_SUS_BACKEND_CAPABILITIES "cpu"
#endif

struct cpkt_sus_model_impl {
  struct whisper_context *context;
  int cpu_only;
  int preserve_initial_space_after_first_transcriber;
  int segmented_initial_space_seen;
};

struct cpkt_sus_transcriber_impl {
  cpkt_sus *model;
  cpkt_sus_transcriber_config config;
  whisper_token *prompt_tokens;
  size_t prompt_count;
  size_t prompt_capacity;
  unsigned long segmented_segment_count;
  char *revised_text;
  size_t revised_length;
  size_t revised_capacity;
  int aborted;
  int callback_error;
};

struct cpkt_sus_catalog_entry {
  const char *name;
  const char *repo;
  const char *filename;
  const char *url;
  const char *sha256;
  unsigned long size_bytes;
  const char *license;
  const char *quantization;
  int is_default;
};

struct cpkt_sus_download_sink {
  FILE *file;
  int failed;
};

static cpkt_sus_log_sink cpkt_sus_log_callback = NULL;
static void *cpkt_sus_log_user = NULL;
static int cpkt_sus_log_installed = 0;
static int cpkt_sus_log_last_level = CPKT_SUS_LOG_INFO;

cpkt_sus_result cpkt_sus_open_path(cpkt_sus **out,
                                   const cpkt_sus_config *config);

static void cpkt_sus_whisper_log_bridge(enum ggml_log_level level,
                                        const char *text, void *user_data) {
  cpkt_sus_log_event event;

  (void)user_data;
  if (cpkt_sus_log_callback == NULL) {
    return;
  }
  memset(&event, 0, sizeof(event));
  if (level == GGML_LOG_LEVEL_CONT) {
    event.level = cpkt_sus_log_last_level;
  } else {
    event.level = (int)level;
    cpkt_sus_log_last_level = event.level;
  }
  event.component = "whisper";
  event.message = text != NULL ? text : "";
  cpkt_sus_log_callback(&event, cpkt_sus_log_user);
}

static void cpkt_sus_log_ensure_installed(void) {
  if (!cpkt_sus_log_installed) {
    whisper_log_set(cpkt_sus_whisper_log_bridge, NULL);
    cpkt_sus_log_installed = 1;
  }
}

/** Sets or clears the process-wide backend log sink. */
void cpkt_sus_log_set(cpkt_sus_log_sink sink, void *user) {
  cpkt_sus_log_callback = sink;
  cpkt_sus_log_user = user;
  cpkt_sus_log_last_level = CPKT_SUS_LOG_INFO;
  whisper_log_set(cpkt_sus_whisper_log_bridge, NULL);
  cpkt_sus_log_installed = 1;
}

static const struct cpkt_sus_catalog_entry cpkt_sus_catalog[] = {
    {"tiny", "ggerganov/whisper.cpp", "ggml-tiny.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.bin",
     "be07e048e1e599ad46341c8d2a135645097a538221678b7acdd1b1919c6e1b21",
     77691713UL, "MIT", "f16", 0},
    {"tiny.en", "ggerganov/whisper.cpp", "ggml-tiny.en.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/"
     "ggml-tiny.en.bin",
     "921e4cf8686fdd993dcd081a5da5b6c365bfde1162e72b08d75ac75289920b1f",
     77704715UL, "MIT", "f16", 0},
    {"tiny:q5_1", "ggerganov/whisper.cpp", "ggml-tiny-q5_1.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/"
     "ggml-tiny-q5_1.bin",
     "818710568da3ca15689e31a743197b520007872ff9576237bda97bd1b469c3d7",
     32152673UL, "MIT", "q5_1", 0},
    {"tiny.en:q5_1", "ggerganov/whisper.cpp", "ggml-tiny.en-q5_1.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/"
     "ggml-tiny.en-q5_1.bin",
     "c77c5766f1cef09b6b7d47f21b546cbddd4157886b3b5d6d4f709e91e66c7c2b",
     32166155UL, "MIT", "q5_1", 0},
    {"base", "ggerganov/whisper.cpp", "ggml-base.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin",
     "60ed5bc3dd14eea856493d334349b405782ddcaf0028d4b5df4088345fba2efe",
     147951465UL, "MIT", "f16", 0},
    {"base.en", "ggerganov/whisper.cpp", "ggml-base.en.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/"
     "ggml-base.en.bin",
     "a03779c86df3323075f5e796cb2ce5029f00ec8869eee3fdfb897afe36c6d002",
     147964211UL, "MIT", "f16", 0},
    {"base:q5_1", "ggerganov/whisper.cpp", "ggml-base-q5_1.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/"
     "ggml-base-q5_1.bin",
     "422f1ae452ade6f30a004d7e5c6a43195e4433bc370bf23fac9cc591f01a8898",
     59707625UL, "MIT", "q5_1", 0},
    {"base.en:q5_1", "ggerganov/whisper.cpp", "ggml-base.en-q5_1.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/"
     "ggml-base.en-q5_1.bin",
     "4baf70dd0d7c4247ba2b81fafd9c01005ac77c2f9ef064e00dcf195d0e2fdd2f",
     59721011UL, "MIT", "q5_1", 0},
    {"small", "ggerganov/whisper.cpp", "ggml-small.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/"
     "ggml-small.bin",
     "1be3a9b2063867b937e64e2ec7483364a79917e157fa98c5d94b5c1fffea987b",
     487601967UL, "MIT", "f16", 1},
    {"small.en", "ggerganov/whisper.cpp", "ggml-small.en.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/"
     "ggml-small.en.bin",
     "c6138d6d58ecc8322097e0f987c32f1be8bb0a18532a3f88f734d1bbf9c41e5d",
     487614201UL, "MIT", "f16", 0},
    {"small:q5_1", "ggerganov/whisper.cpp", "ggml-small-q5_1.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/"
     "ggml-small-q5_1.bin",
     "ae85e4a935d7a567bd102fe55afc16bb595bdb618e11b2fc7591bc08120411bb",
     190085487UL, "MIT", "q5_1", 0},
    {"small.en:q5_1", "ggerganov/whisper.cpp", "ggml-small.en-q5_1.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/"
     "ggml-small.en-q5_1.bin",
     "bfdff4894dcb76bbf647d56263ea2a96645423f1669176f4844a1bf8e478ad30",
     190098681UL, "MIT", "q5_1", 0},
    {"medium", "ggerganov/whisper.cpp", "ggml-medium.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/"
     "ggml-medium.bin",
     "6c14d5adee5f86394037b4e4e8b59f1673b6cee10e3cf0b11bbdbee79c156208",
     1533763059UL, "MIT", "f16", 0},
    {"medium.en", "ggerganov/whisper.cpp", "ggml-medium.en.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/"
     "ggml-medium.en.bin",
     "cc37e93478338ec7700281a7ac30a10128929eb8f427dda2e865faa8f6da4356",
     1533774781UL, "MIT", "f16", 0},
    {"medium:q5_0", "ggerganov/whisper.cpp", "ggml-medium-q5_0.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/"
     "ggml-medium-q5_0.bin",
     "19fea4b380c3a618ec4723c3eef2eb785ffba0d0538cf43f8f235e7b3b34220f",
     539212467UL, "MIT", "q5_0", 0},
    {"medium.en:q5_0", "ggerganov/whisper.cpp", "ggml-medium.en-q5_0.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/"
     "ggml-medium.en-q5_0.bin",
     "76733e26ad8fe1c7a5bf7531a9d41917b2adc0f20f2e4f5531688a8c6cd88eb0",
     539225533UL, "MIT", "q5_0", 0},
    {"large-v3", "ggerganov/whisper.cpp", "ggml-large-v3.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/"
     "ggml-large-v3.bin",
     "64d182b440b98d5203c4f9bd541544d84c605196c4f7b845dfa11fb23594d1e2",
     3095033483UL, "MIT", "f16", 0},
    {"large-v3:q5_0", "ggerganov/whisper.cpp", "ggml-large-v3-q5_0.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/"
     "ggml-large-v3-q5_0.bin",
     "d75795ecff3f83b5faa89d1900604ad8c780abd5739fae406de19f23ecd98ad1",
     1081140203UL, "MIT", "q5_0", 0},
    {"large-v3-turbo", "ggerganov/whisper.cpp", "ggml-large-v3-turbo.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/"
     "ggml-large-v3-turbo.bin",
     "1fc70f774d38eb169993ac391eea357ef47c88757ef72ee5943879b7e8e2bc69",
     1624555275UL, "MIT", "f16", 0},
    {"large-v3-turbo:q5_0", "ggerganov/whisper.cpp",
     "ggml-large-v3-turbo-q5_0.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/"
     "ggml-large-v3-turbo-q5_0.bin",
     "394221709cd5ad1f40c46e6031ca61bce88931e6e088c188294c6d5a55ffa7e2",
     574041195UL, "MIT", "q5_0", 0},
    {"tiny.sv", "KBLab/kb-whisper-tiny", "ggml-tiny.sv.bin",
     "https://huggingface.co/KBLab/kb-whisper-tiny/resolve/main/"
     "ggml-model.bin",
     "054187c95948ee0455d428db0c0d6c84d6c6157dab72e86857ced13233118b03",
     77691730UL, "Apache-2.0", "f16", 0},
    {"tiny.sv:q5_0", "KBLab/kb-whisper-tiny", "ggml-tiny.sv-q5_0.bin",
     "https://huggingface.co/KBLab/kb-whisper-tiny/resolve/main/"
     "ggml-model-q5_0.bin",
     "98d46b7d23e5528d006e8a42e29eb0cb39b44bed94e1329f10f57d1fd15c658b",
     29875738UL, "Apache-2.0", "q5_0", 0},
    {"base.sv", "KBLab/kb-whisper-base", "ggml-base.sv.bin",
     "https://huggingface.co/KBLab/kb-whisper-base/resolve/main/"
     "ggml-model.bin",
     "f5e3cdb33e537eedfa2a749b5cae28c4c511873a1b13362f87dffbe07891d3fe",
     147951482UL, "Apache-2.0", "f16", 0},
    {"base.sv:q5_0", "KBLab/kb-whisper-base", "ggml-base.sv-q5_0.bin",
     "https://huggingface.co/KBLab/kb-whisper-base/resolve/main/"
     "ggml-model-q5_0.bin",
     "aead29b356bca8840e72a8dc2286e2d69e6702639751a1e60cb3c8eacefec546",
     55295450UL, "Apache-2.0", "q5_0", 0},
    {"small.sv", "KBLab/kb-whisper-small", "ggml-small.sv.bin",
     "https://huggingface.co/KBLab/kb-whisper-small/resolve/main/"
     "ggml-model.bin",
     "de6911330cbdc131362f7a955682b65c8a5a2394caba73e7ea821a9822efb8c6",
     487601984UL, "Apache-2.0", "f16", 0},
    {"small.sv:q5_0", "KBLab/kb-whisper-small", "ggml-small.sv-q5_0.bin",
     "https://huggingface.co/KBLab/kb-whisper-small/resolve/main/"
     "ggml-model-q5_0.bin",
     "6768836a51abc902e420c613153e6d418c90ea2774e913274d02ab23170225b7",
     175209680UL, "Apache-2.0", "q5_0", 0},
    {"medium.sv", "KBLab/kb-whisper-medium", "ggml-medium.sv.bin",
     "https://huggingface.co/KBLab/kb-whisper-medium/resolve/main/"
     "ggml-model.bin",
     "1b7842bc1c3f79fb3bf043a0a3590961d625a49ef3ccbdceb00e738c5dd8b015",
     1533763076UL, "Apache-2.0", "f16", 0},
    {"medium.sv:q5_0", "KBLab/kb-whisper-medium", "ggml-medium.sv-q5_0.bin",
     "https://huggingface.co/KBLab/kb-whisper-medium/resolve/main/"
     "ggml-model-q5_0.bin",
     "7f8762e0ade9e0073674c0d5acae942a0b1ea98add9baa008ee89c94eaba43d0",
     539212484UL, "Apache-2.0", "q5_0", 0},
    {"large.sv", "KBLab/kb-whisper-large", "ggml-large.sv.bin",
     "https://huggingface.co/KBLab/kb-whisper-large/resolve/main/"
     "ggml-model.bin",
     "b66f2dda369a88f6c03fe37326d7cc37aa216f6f34e6fc1be686e497ba9c2f39",
     3095033483UL, "Apache-2.0", "f16", 0},
    {"large.sv:q5_0", "KBLab/kb-whisper-large", "ggml-large.sv-q5_0.bin",
     "https://huggingface.co/KBLab/kb-whisper-large/resolve/main/"
     "ggml-model-q5_0.bin",
     "6d2863812d7410322bb7d8647a5c7260761300fa946714c9ed66d22bb30bcb19",
     1081140203UL, "Apache-2.0", "q5_0", 0},
    {"kb-whisper-tiny", "KBLab/kb-whisper-tiny", "ggml-tiny.sv.bin",
     "https://huggingface.co/KBLab/kb-whisper-tiny/resolve/main/"
     "ggml-model.bin",
     "054187c95948ee0455d428db0c0d6c84d6c6157dab72e86857ced13233118b03",
     77691730UL, "Apache-2.0", "f16", 0},
    {"kb-whisper-tiny:q5_0", "KBLab/kb-whisper-tiny", "ggml-tiny.sv-q5_0.bin",
     "https://huggingface.co/KBLab/kb-whisper-tiny/resolve/main/"
     "ggml-model-q5_0.bin",
     "98d46b7d23e5528d006e8a42e29eb0cb39b44bed94e1329f10f57d1fd15c658b",
     29875738UL, "Apache-2.0", "q5_0", 0},
    {"kb-whisper-base", "KBLab/kb-whisper-base", "ggml-base.sv.bin",
     "https://huggingface.co/KBLab/kb-whisper-base/resolve/main/"
     "ggml-model.bin",
     "f5e3cdb33e537eedfa2a749b5cae28c4c511873a1b13362f87dffbe07891d3fe",
     147951482UL, "Apache-2.0", "f16", 0},
    {"kb-whisper-base:q5_0", "KBLab/kb-whisper-base", "ggml-base.sv-q5_0.bin",
     "https://huggingface.co/KBLab/kb-whisper-base/resolve/main/"
     "ggml-model-q5_0.bin",
     "aead29b356bca8840e72a8dc2286e2d69e6702639751a1e60cb3c8eacefec546",
     55295450UL, "Apache-2.0", "q5_0", 0},
    {"kb-whisper-small", "KBLab/kb-whisper-small", "ggml-small.sv.bin",
     "https://huggingface.co/KBLab/kb-whisper-small/resolve/main/"
     "ggml-model.bin",
     "de6911330cbdc131362f7a955682b65c8a5a2394caba73e7ea821a9822efb8c6",
     487601984UL, "Apache-2.0", "f16", 0},
    {"kb-whisper-small:q5_0", "KBLab/kb-whisper-small",
     "ggml-small.sv-q5_0.bin",
     "https://huggingface.co/KBLab/kb-whisper-small/resolve/main/"
     "ggml-model-q5_0.bin",
     "6768836a51abc902e420c613153e6d418c90ea2774e913274d02ab23170225b7",
     175209680UL, "Apache-2.0", "q5_0", 0},
    {"kb-whisper-medium", "KBLab/kb-whisper-medium", "ggml-medium.sv.bin",
     "https://huggingface.co/KBLab/kb-whisper-medium/resolve/main/"
     "ggml-model.bin",
     "1b7842bc1c3f79fb3bf043a0a3590961d625a49ef3ccbdceb00e738c5dd8b015",
     1533763076UL, "Apache-2.0", "f16", 0},
    {"kb-whisper-medium:q5_0", "KBLab/kb-whisper-medium",
     "ggml-medium.sv-q5_0.bin",
     "https://huggingface.co/KBLab/kb-whisper-medium/resolve/main/"
     "ggml-model-q5_0.bin",
     "7f8762e0ade9e0073674c0d5acae942a0b1ea98add9baa008ee89c94eaba43d0",
     539212484UL, "Apache-2.0", "q5_0", 0},
    {"kb-whisper-large", "KBLab/kb-whisper-large", "ggml-large.sv.bin",
     "https://huggingface.co/KBLab/kb-whisper-large/resolve/main/"
     "ggml-model.bin",
     "b66f2dda369a88f6c03fe37326d7cc37aa216f6f34e6fc1be686e497ba9c2f39",
     3095033483UL, "Apache-2.0", "f16", 0},
    {"kb-whisper-large:q5_0", "KBLab/kb-whisper-large",
     "ggml-large.sv-q5_0.bin",
     "https://huggingface.co/KBLab/kb-whisper-large/resolve/main/"
     "ggml-model-q5_0.bin",
     "6d2863812d7410322bb7d8647a5c7260761300fa946714c9ed66d22bb30bcb19",
     1081140203UL, "Apache-2.0", "q5_0", 0},
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

static const char *cpkt_sus_default_model_name(void) { return "small"; }

static unsigned long cpkt_sus_catalog_count_internal(void) {
  return (unsigned long)(sizeof(cpkt_sus_catalog) /
                         sizeof(cpkt_sus_catalog[0]));
}

static void
cpkt_sus_copy_catalog_entry(cpkt_sus_model_entry *out,
                            const struct cpkt_sus_catalog_entry *entry) {
  memset(out, 0, sizeof(*out));
  out->name = entry->name;
  out->provider = entry->repo;
  out->source_url = entry->url;
  out->filename = entry->filename;
  out->sha256 = entry->sha256;
  out->size_bytes = entry->size_bytes;
  out->license = entry->license;
  out->quantization = entry->quantization;
  out->is_default = entry->is_default;
}

static const struct cpkt_sus_catalog_entry *
cpkt_sus_find_catalog_entry(const char *name) {
  size_t i;

  if (name == NULL || name[0] == '\0') {
    name = cpkt_sus_default_model_name();
  }
  for (i = 0U; i < sizeof(cpkt_sus_catalog) / sizeof(cpkt_sus_catalog[0]);
       ++i) {
    if (strcmp(cpkt_sus_catalog[i].name, name) == 0) {
      return &cpkt_sus_catalog[i];
    }
  }
  return NULL;
}

static int cpkt_sus_is_sha256_hex(const char *text) {
  size_t i;
  char c;

  if (text == NULL || strlen(text) != 64U) {
    return 0;
  }
  for (i = 0U; i < 64U; ++i) {
    c = text[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
      return 0;
    }
  }
  return 1;
}

static char *cpkt_sus_strdup_range(const char *text, size_t length) {
  char *copy;

  copy = (char *)malloc(length + 1U);
  if (copy == NULL) {
    return NULL;
  }
  memcpy(copy, text, length);
  copy[length] = '\0';
  return copy;
}

static cpkt_sus_result cpkt_sus_join2(char **out, const char *left,
                                      const char *right) {
  size_t left_len;
  size_t right_len;
  size_t needs_slash;
  char *joined;

  if (out != NULL) {
    *out = NULL;
  }
  if (out == NULL || left == NULL || left[0] == '\0' || right == NULL ||
      right[0] == '\0') {
    return CPKT_SUS_ERR_ARG;
  }

  left_len = strlen(left);
  right_len = strlen(right);
  needs_slash = left[left_len - 1U] == '/' ? 0U : 1U;
  if (left_len > ((size_t)-1) - needs_slash - right_len - 1U) {
    return CPKT_SUS_ERR_ALLOC;
  }

  joined = (char *)malloc(left_len + needs_slash + right_len + 1U);
  if (joined == NULL) {
    return CPKT_SUS_ERR_ALLOC;
  }
  memcpy(joined, left, left_len);
  if (needs_slash) {
    joined[left_len] = '/';
  }
  memcpy(joined + left_len + needs_slash, right, right_len);
  joined[left_len + needs_slash + right_len] = '\0';
  *out = joined;
  return CPKT_SUS_OK;
}

static cpkt_sus_result cpkt_sus_default_cache_dir(char **out) {
  const char *xdg_cache_home;
  const char *home;
  char *base;
  cpkt_sus_result result;

  if (out != NULL) {
    *out = NULL;
  }
  if (out == NULL) {
    return CPKT_SUS_ERR_ARG;
  }

  xdg_cache_home = getenv("XDG_CACHE_HOME");
  if (xdg_cache_home != NULL && xdg_cache_home[0] != '\0') {
    return cpkt_sus_join2(out, xdg_cache_home, "cpkt/susurro/models");
  }

  home = getenv("HOME");
  if (home == NULL || home[0] == '\0') {
    return CPKT_SUS_ERR_IO;
  }

  result = cpkt_sus_join2(&base, home, ".cache");
  if (result != CPKT_SUS_OK) {
    return result;
  }
  result = cpkt_sus_join2(out, base, "cpkt/susurro/models");
  free(base);
  return result;
}

static cpkt_sus_result
cpkt_sus_cache_dir_from_config(char **out,
                               const cpkt_sus_cache_config *config) {
  const char *configured;

  if (out != NULL) {
    *out = NULL;
  }
  if (out == NULL || config == NULL) {
    return CPKT_SUS_ERR_ARG;
  }

  configured = config->cache_dir;
  if (configured != NULL && configured[0] != '\0') {
    *out = cpkt_sus_strdup_range(configured, strlen(configured));
    return *out == NULL ? CPKT_SUS_ERR_ALLOC : CPKT_SUS_OK;
  }
  return cpkt_sus_default_cache_dir(out);
}

static cpkt_sus_result
cpkt_sus_emit_cache_status(const cpkt_sus_cache_config *config, int phase,
                           const struct cpkt_sus_catalog_entry *entry,
                           const char *cache_path, const char *source_url) {
  cpkt_sus_cache_status_event event;

  if (config == NULL || config->status_sink == NULL) {
    return CPKT_SUS_OK;
  }
  memset(&event, 0, sizeof(event));
  event.phase = phase;
  event.model = entry != NULL ? entry->name : NULL;
  event.cache_path = cache_path;
  event.source_url = source_url;
  if (config->status_sink(&event, config->status_user) != 0) {
    return CPKT_SUS_ERR_CALLBACK;
  }
  return CPKT_SUS_OK;
}

static int cpkt_sus_file_exists(const char *path) {
  struct stat st;

  if (path == NULL || path[0] == '\0') {
    return 0;
  }
  if (stat(path, &st) != 0) {
    return 0;
  }
  return S_ISREG(st.st_mode) ? 1 : 0;
}

static cpkt_sus_result cpkt_sus_mkdir_one(const char *path) {
  struct stat st;

  if (path == NULL || path[0] == '\0') {
    return CPKT_SUS_ERR_ARG;
  }
  if (mkdir(path, 0700) == 0) {
    return CPKT_SUS_OK;
  }
  if (errno == EEXIST) {
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
      return CPKT_SUS_OK;
    }
  }
  return CPKT_SUS_ERR_IO;
}

static cpkt_sus_result cpkt_sus_mkdirs(const char *path) {
  char *copy;
  char *cursor;
  cpkt_sus_result result;

  if (path == NULL || path[0] == '\0') {
    return CPKT_SUS_ERR_ARG;
  }

  copy = cpkt_sus_strdup_range(path, strlen(path));
  if (copy == NULL) {
    return CPKT_SUS_ERR_ALLOC;
  }

  cursor = copy;
  if (cursor[0] == '/') {
    ++cursor;
  }

  for (; *cursor != '\0'; ++cursor) {
    if (*cursor == '/') {
      *cursor = '\0';
      if (copy[0] != '\0') {
        result = cpkt_sus_mkdir_one(copy);
        if (result != CPKT_SUS_OK) {
          free(copy);
          return result;
        }
      }
      *cursor = '/';
    }
  }

  result = cpkt_sus_mkdir_one(copy);
  free(copy);
  return result;
}

static cpkt_sus_result cpkt_sus_temp_path_for(char **out,
                                              const char *model_path) {
  static const char suffix[] = ".tmp.XXXXXX";
  size_t path_len;
  char *temp_path;
  int fd;

  if (out != NULL) {
    *out = NULL;
  }
  if (out == NULL || model_path == NULL || model_path[0] == '\0') {
    return CPKT_SUS_ERR_ARG;
  }

  path_len = strlen(model_path);
  if (path_len > ((size_t)-1) - sizeof(suffix)) {
    return CPKT_SUS_ERR_ALLOC;
  }
  temp_path = (char *)malloc(path_len + sizeof(suffix));
  if (temp_path == NULL) {
    return CPKT_SUS_ERR_ALLOC;
  }
  memcpy(temp_path, model_path, path_len);
  memcpy(temp_path + path_len, suffix, sizeof(suffix));

  fd = mkstemp(temp_path);
  if (fd < 0) {
    free(temp_path);
    return CPKT_SUS_ERR_IO;
  }
  if (close(fd) != 0) {
    (void)remove(temp_path);
    free(temp_path);
    return CPKT_SUS_ERR_IO;
  }

  *out = temp_path;
  return CPKT_SUS_OK;
}

static cpkt_sus_result cpkt_sus_sha256_file(char out_hex[65],
                                            const char *path) {
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len;
  unsigned char buffer[32768];
  EVP_MD_CTX *ctx;
  FILE *file;
  size_t count;
  static const char hex[] = "0123456789abcdef";
  unsigned int i;

  if (out_hex == NULL || path == NULL || path[0] == '\0') {
    return CPKT_SUS_ERR_ARG;
  }

  file = fopen(path, "rb");
  if (file == NULL) {
    return CPKT_SUS_ERR_IO;
  }

  ctx = EVP_MD_CTX_new();
  if (ctx == NULL) {
    fclose(file);
    return CPKT_SUS_ERR_ALLOC;
  }
  if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
    EVP_MD_CTX_free(ctx);
    fclose(file);
    return CPKT_SUS_ERR_UPSTREAM;
  }

  while ((count = fread(buffer, 1U, sizeof(buffer), file)) > 0U) {
    if (EVP_DigestUpdate(ctx, buffer, count) != 1) {
      EVP_MD_CTX_free(ctx);
      fclose(file);
      return CPKT_SUS_ERR_UPSTREAM;
    }
  }
  if (ferror(file)) {
    EVP_MD_CTX_free(ctx);
    fclose(file);
    return CPKT_SUS_ERR_IO;
  }
  if (EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1 || digest_len != 32U) {
    EVP_MD_CTX_free(ctx);
    fclose(file);
    return CPKT_SUS_ERR_UPSTREAM;
  }

  EVP_MD_CTX_free(ctx);
  fclose(file);

  for (i = 0U; i < digest_len; ++i) {
    out_hex[i * 2U] = hex[(digest[i] >> 4U) & 0x0FU];
    out_hex[(i * 2U) + 1U] = hex[digest[i] & 0x0FU];
  }
  out_hex[64] = '\0';
  return CPKT_SUS_OK;
}

static cpkt_sus_result
cpkt_sus_validate_cached_file(const char *path,
                              const struct cpkt_sus_catalog_entry *entry,
                              const cpkt_sus_cache_config *config) {
  const char *expected_sha256;
  char actual_sha256[65];
  cpkt_sus_result result;

  if (path == NULL || entry == NULL || config == NULL) {
    return CPKT_SUS_ERR_ARG;
  }
  if (config->insecure_no_checksum) {
    return CPKT_SUS_OK;
  }

  expected_sha256 = entry->sha256;
  if (config->sha256 != NULL && config->sha256[0] != '\0') {
    if (!cpkt_sus_is_sha256_hex(config->sha256)) {
      return CPKT_SUS_ERR_ARG;
    }
    expected_sha256 = config->sha256;
  }
  if (!cpkt_sus_is_sha256_hex(expected_sha256)) {
    return CPKT_SUS_ERR_CHECKSUM;
  }

  result = cpkt_sus_sha256_file(actual_sha256, path);
  if (result != CPKT_SUS_OK) {
    return result;
  }
  if (strcmp(actual_sha256, expected_sha256) != 0) {
    return CPKT_SUS_ERR_CHECKSUM;
  }
  return CPKT_SUS_OK;
}

static size_t cpkt_sus_curl_write(void *buffer, size_t size, size_t nmemb,
                                  void *user_data) {
  struct cpkt_sus_download_sink *sink;
  size_t bytes;
  size_t written;

  sink = (struct cpkt_sus_download_sink *)user_data;
  if (sink == NULL || sink->file == NULL) {
    return 0U;
  }
  if (size != 0U && nmemb > ((size_t)-1) / size) {
    sink->failed = 1;
    return 0U;
  }
  bytes = size * nmemb;
  written = fwrite(buffer, 1U, bytes, sink->file);
  if (written != bytes) {
    sink->failed = 1;
  }
  return written;
}

static cpkt_sus_result cpkt_sus_download_to_file(const char *url,
                                                 const char *path) {
  struct cpkt_sus_download_sink sink;
  CURL *curl;
  CURLcode code;

  if (url == NULL || url[0] == '\0' || path == NULL || path[0] == '\0') {
    return CPKT_SUS_ERR_ARG;
  }

  if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
    return CPKT_SUS_ERR_NETWORK;
  }

  memset(&sink, 0, sizeof(sink));
  sink.file = fopen(path, "wb");
  if (sink.file == NULL) {
    return CPKT_SUS_ERR_IO;
  }

  curl = curl_easy_init();
  if (curl == NULL) {
    fclose(sink.file);
    return CPKT_SUS_ERR_NETWORK;
  }

  (void)curl_easy_setopt(curl, CURLOPT_URL, url);
  (void)curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  (void)curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  (void)curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cpkt_sus_curl_write);
  (void)curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
  (void)curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
  (void)curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
  (void)curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);
  (void)curl_easy_setopt(curl, CURLOPT_USERAGENT, "c.pkt.systems-cpkt-sus/0");

  code = curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  if (fflush(sink.file) != 0) {
    sink.failed = 1;
  }
  if (fsync(fileno(sink.file)) != 0) {
    sink.failed = 1;
  }
  if (fclose(sink.file) != 0) {
    sink.failed = 1;
  }

  if (code != CURLE_OK || sink.failed) {
    return code == CURLE_OK ? CPKT_SUS_ERR_IO : CPKT_SUS_ERR_NETWORK;
  }
  return CPKT_SUS_OK;
}

static cpkt_sus_result
cpkt_sus_verify_model_file(const char *path,
                           const cpkt_sus_cache_config *config) {
  cpkt_sus_config model_config;
  cpkt_sus *model;
  cpkt_sus_result result;

  memset(&model_config, 0, sizeof(model_config));
  model_config.model_path = path;
  model_config.cpu_only = config != NULL ? config->cpu_only : 0;
  model_config.preserve_initial_space_after_first_transcriber =
      config != NULL ? config->preserve_initial_space_after_first_transcriber
                     : 0;
  model = NULL;
  result = cpkt_sus_open_path(&model, &model_config);
  if (model != NULL) {
    model->destroy(model);
  }
  return result;
}

static cpkt_sus_result
cpkt_sus_open_validated_cached_file(cpkt_sus **out, const char *path,
                                    const struct cpkt_sus_catalog_entry *entry,
                                    const cpkt_sus_cache_config *config) {
  cpkt_sus_config model_config;
  cpkt_sus_result result;

  result = cpkt_sus_validate_cached_file(path, entry, config);
  if (result != CPKT_SUS_OK) {
    return result;
  }

  result = cpkt_sus_emit_cache_status(
      config, CPKT_SUS_CACHE_STATUS_LOAD_BEGIN, entry, path, NULL);
  if (result != CPKT_SUS_OK) {
    return result;
  }

  memset(&model_config, 0, sizeof(model_config));
  model_config.model_path = path;
  model_config.cpu_only = config->cpu_only;
  model_config.preserve_initial_space_after_first_transcriber =
      config->preserve_initial_space_after_first_transcriber;
  return cpkt_sus_open_path((cpkt_sus **)out, &model_config);
}

static cpkt_sus_result
cpkt_sus_fetch_cached_file(const char *model_path, const char *cache_dir,
                           const struct cpkt_sus_catalog_entry *entry,
                           const cpkt_sus_cache_config *config) {
  const char *url;
  char *temp_path;
  cpkt_sus_result result;

  if (model_path == NULL || cache_dir == NULL || entry == NULL ||
      config == NULL) {
    return CPKT_SUS_ERR_ARG;
  }
  if (config->offline) {
    return CPKT_SUS_ERR_IO;
  }

  result = cpkt_sus_mkdirs(cache_dir);
  if (result != CPKT_SUS_OK) {
    return result;
  }

  temp_path = NULL;
  result = cpkt_sus_temp_path_for(&temp_path, model_path);
  if (result != CPKT_SUS_OK) {
    return result;
  }

  url = entry->url;
  if (config->source_url != NULL && config->source_url[0] != '\0') {
    url = config->source_url;
  }

  result = cpkt_sus_emit_cache_status(
      config, CPKT_SUS_CACHE_STATUS_DOWNLOAD_BEGIN, entry, model_path, url);
  if (result != CPKT_SUS_OK) {
    (void)remove(temp_path);
    free(temp_path);
    return result;
  }
  result = cpkt_sus_download_to_file(url, temp_path);
  if (result == CPKT_SUS_OK) {
    result = cpkt_sus_emit_cache_status(
        config, CPKT_SUS_CACHE_STATUS_DOWNLOAD_COMPLETE, entry, model_path,
        url);
  }
  if (result == CPKT_SUS_OK) {
    result = cpkt_sus_emit_cache_status(
        config, CPKT_SUS_CACHE_STATUS_VERIFY_BEGIN, entry, model_path, NULL);
  }
  if (result == CPKT_SUS_OK) {
    result = cpkt_sus_validate_cached_file(temp_path, entry, config);
  }
  if (result == CPKT_SUS_OK) {
    result = cpkt_sus_verify_model_file(temp_path, config);
  }
  if (result == CPKT_SUS_OK) {
    result = cpkt_sus_emit_cache_status(
        config, CPKT_SUS_CACHE_STATUS_VERIFY_COMPLETE, entry, model_path,
        NULL);
  }
  if (result == CPKT_SUS_OK && rename(temp_path, model_path) != 0) {
    result = CPKT_SUS_ERR_IO;
  }

  if (result != CPKT_SUS_OK) {
    (void)remove(temp_path);
  }
  free(temp_path);
  return result;
}

static cpkt_sus_result cpkt_sus_info_impl(const cpkt_sus *self,
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

static void cpkt_sus_destroy_impl(cpkt_sus *self) {
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
  if (impl->callback_error) {
    return true;
  }
  if (impl->config.abort(impl->config.abort_user) != 0) {
    impl->aborted = 1;
    return true;
  }
  return false;
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

  impl->aborted = 0;
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

  cpkt_sus_log_ensure_installed();
  full_result =
      whisper_full(model_impl->context, params, samples, (int)sample_count);
  if (impl->callback_error) {
    return CPKT_SUS_ERR_CALLBACK;
  }
  if (impl->aborted) {
    return CPKT_SUS_ABORTED;
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

static int cpkt_sus_ul_to_int(unsigned long value, int *out) {
  if (out == NULL || value > (unsigned long)INT_MAX) {
    return 0;
  }
  *out = (int)value;
  return 1;
}

static int cpkt_sus_ascii_space(char ch) {
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' ||
         ch == '\v';
}

static int cpkt_sus_ascii_punctuation(char ch) {
  return ch == '.' || ch == ',' || ch == ';' || ch == ':' || ch == '!' ||
         ch == '?' || ch == ')' || ch == ']' || ch == '}';
}

static cpkt_sus_result
cpkt_sus_build_segmented_text(char **out, struct whisper_context *context) {
  const char *segment_text;
  char *text;
  size_t length;
  size_t segment_length;
  int count;
  int i;

  if (out != NULL) {
    *out = NULL;
  }
  if (out == NULL || context == NULL) {
    return CPKT_SUS_ERR_ARG;
  }

  count = whisper_full_n_segments(context);
  length = 0U;
  for (i = 0; i < count; ++i) {
    segment_text = whisper_full_get_segment_text(context, i);
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
    segment_text = whisper_full_get_segment_text(context, i);
    if (segment_text != NULL) {
      segment_length = strlen(segment_text);
      memcpy(text + length, segment_text, segment_length);
      length += segment_length;
    }
  }
  text[length] = '\0';
  *out = text;
  return CPKT_SUS_OK;
}

static cpkt_sus_result
cpkt_sus_transcriber_append_revised_text(struct cpkt_sus_transcriber_impl *impl,
                                         const char *text, size_t text_len);

static int
cpkt_sus_transcriber_should_strip_initial_space(
    struct cpkt_sus_transcriber_impl *impl) {
  struct cpkt_sus_model_impl *model_impl;

  if (impl == NULL || impl->revised_length != 0U || impl->model == NULL ||
      impl->model->impl == NULL) {
    return 0;
  }

  model_impl = (struct cpkt_sus_model_impl *)impl->model->impl;
  if (!model_impl->preserve_initial_space_after_first_transcriber) {
    return 1;
  }
  if (model_impl->segmented_initial_space_seen) {
    return 0;
  }

  model_impl->segmented_initial_space_seen = 1;
  return 1;
}

static cpkt_sus_result
cpkt_sus_emit_segmented_event(struct cpkt_sus_transcriber_impl *impl,
                             const cpkt_sus_segmented_config *config,
                             const char *text, size_t text_len,
                             unsigned long step_index, long t0, long t1,
                             int final) {
  cpkt_sus_segmented_event event;
  cpkt_sus_result result;
  int callback_result;

  if (impl == NULL) {
    return CPKT_SUS_OK;
  }

  if (text != NULL && text_len > 0U && text[0] == ' ' &&
      cpkt_sus_transcriber_should_strip_initial_space(impl)) {
    ++text;
    --text_len;
  }
  if (text != NULL && text_len > 0U) {
    result = cpkt_sus_transcriber_append_revised_text(impl, text, text_len);
    if (result != CPKT_SUS_OK) {
      return result;
    }
  }
  if (config == NULL || config->segmented_sink == NULL) {
    return CPKT_SUS_OK;
  }

  memset(&event, 0, sizeof(event));
  event.text = impl->revised_text != NULL ? impl->revised_text : "";
  event.text_length = (unsigned long)impl->revised_length;
  event.t0 = t0;
  event.t1 = t1;
  event.step_index = step_index;
  event.is_final = final;
  callback_result = config->segmented_sink(&event, config->segmented_user);
  if (callback_result != 0) {
    impl->callback_error = 1;
    return CPKT_SUS_ERR_CALLBACK;
  }
  return CPKT_SUS_OK;
}

struct cpkt_sus_segmented_text_state {
  cpkt_sus_segmented_sink forward_sink;
  void *forward_user;
  char *text;
  size_t length;
  size_t capacity;
  cpkt_sus_result result;
};

static int cpkt_sus_char_equal_ci(char left, char right) {
  if (left >= 'A' && left <= 'Z') {
    left = (char)(left - 'A' + 'a');
  }
  if (right >= 'A' && right <= 'Z') {
    right = (char)(right - 'A' + 'a');
  }
  return left == right;
}

static size_t cpkt_sus_common_prefix_ci(const char *left, size_t left_len,
                                        const char *right, size_t right_len) {
  size_t i;
  size_t limit;

  limit = left_len < right_len ? left_len : right_len;
  for (i = 0U; i < limit; ++i) {
    if (!cpkt_sus_char_equal_ci(left[i], right[i])) {
      break;
    }
  }
  return i;
}

static cpkt_sus_result cpkt_sus_text_reserve(char **text, size_t *capacity,
                                             size_t needed) {
  char *grown;
  size_t next_capacity;

  if (text == NULL || capacity == NULL) {
    return CPKT_SUS_ERR_ARG;
  }
  if (needed <= *capacity) {
    return CPKT_SUS_OK;
  }
  next_capacity = *capacity == 0U ? 256U : *capacity;
  while (next_capacity < needed) {
    if (next_capacity > ((size_t)-1) / 2U) {
      return CPKT_SUS_ERR_ALLOC;
    }
    next_capacity *= 2U;
  }
  grown = (char *)realloc(*text, next_capacity);
  if (grown == NULL) {
    return CPKT_SUS_ERR_ALLOC;
  }
  *text = grown;
  *capacity = next_capacity;
  return CPKT_SUS_OK;
}

static cpkt_sus_result
cpkt_sus_segmented_text_apply_storage(char **text, size_t *length,
                                     size_t *capacity, const char *hypothesis,
                                     size_t hypothesis_len) {
  size_t best_start;
  size_t best_common;
  size_t start;
  size_t min_common;
  size_t search_start;
  size_t needed;
  cpkt_sus_result result;

  if (text == NULL || length == NULL || capacity == NULL ||
      hypothesis == NULL) {
    return CPKT_SUS_ERR_ARG;
  }
  if (hypothesis_len == 0U) {
    return CPKT_SUS_OK;
  }
  if (*length == 0U) {
    needed = hypothesis_len + 1U;
    result = cpkt_sus_text_reserve(text, capacity, needed);
    if (result != CPKT_SUS_OK) {
      return result;
    }
    memcpy(*text, hypothesis, hypothesis_len);
    (*text)[hypothesis_len] = '\0';
    *length = hypothesis_len;
    return CPKT_SUS_OK;
  }

  best_start = *length;
  best_common = 0U;
  min_common = hypothesis_len < 16U ? hypothesis_len : 16U;
  search_start = *length > 4096U ? *length - 4096U : 0U;
  for (start = search_start; start < *length; ++start) {
    size_t common;

    common = cpkt_sus_common_prefix_ci(*text + start, *length - start,
                                       hypothesis, hypothesis_len);
    if (common > best_common) {
      best_common = common;
      best_start = start;
    }
  }

  if (best_common >= min_common) {
    needed = best_start + hypothesis_len + 1U;
    result = cpkt_sus_text_reserve(text, capacity, needed);
    if (result != CPKT_SUS_OK) {
      return result;
    }
    memcpy(*text + best_start, hypothesis, hypothesis_len);
    *length = best_start + hypothesis_len;
    (*text)[*length] = '\0';
    return CPKT_SUS_OK;
  }

  needed = *length + hypothesis_len + 1U;
  result = cpkt_sus_text_reserve(text, capacity, needed);
  if (result != CPKT_SUS_OK) {
    return result;
  }
  memcpy(*text + *length, hypothesis, hypothesis_len);
  *length += hypothesis_len;
  (*text)[*length] = '\0';
  return CPKT_SUS_OK;
}

static cpkt_sus_result
cpkt_sus_segmented_text_apply(struct cpkt_sus_segmented_text_state *state,
                             const char *hypothesis, size_t hypothesis_len) {
  if (state == NULL) {
    return CPKT_SUS_ERR_ARG;
  }
  return cpkt_sus_segmented_text_apply_storage(&state->text, &state->length,
                                              &state->capacity, hypothesis,
                                              hypothesis_len);
}

static void cpkt_sus_transcriber_reset_revised_text(
    struct cpkt_sus_transcriber_impl *impl) {
  if (impl == NULL) {
    return;
  }
  impl->revised_length = 0U;
  if (impl->revised_text != NULL) {
    impl->revised_text[0] = '\0';
  }
}

static cpkt_sus_result
cpkt_sus_transcriber_append_revised_text(struct cpkt_sus_transcriber_impl *impl,
                                         const char *text, size_t text_len) {
  cpkt_sus_result result;
  size_t needed;
  size_t separator;

  if (impl == NULL || text == NULL) {
    return CPKT_SUS_ERR_ARG;
  }
  if (text_len == 0U) {
    return CPKT_SUS_OK;
  }
  separator = 0U;
  if (impl->revised_length > 0U &&
      !cpkt_sus_ascii_space(impl->revised_text[impl->revised_length - 1U]) &&
      !cpkt_sus_ascii_punctuation(text[0])) {
    separator = 1U;
  }
  if (text_len > ((size_t)-1) - impl->revised_length - separator - 1U) {
    return CPKT_SUS_ERR_ALLOC;
  }
  needed = impl->revised_length + separator + text_len + 1U;
  result = cpkt_sus_text_reserve(&impl->revised_text, &impl->revised_capacity,
                                 needed);
  if (result != CPKT_SUS_OK) {
    return result;
  }
  if (separator) {
    impl->revised_text[impl->revised_length] = ' ';
    ++impl->revised_length;
  }
  memcpy(impl->revised_text + impl->revised_length, text, text_len);
  impl->revised_length += text_len;
  impl->revised_text[impl->revised_length] = '\0';
  return CPKT_SUS_OK;
}

static int cpkt_sus_segmented_text_sink(const cpkt_sus_segmented_event *event,
                                       void *user) {
  struct cpkt_sus_segmented_text_state *state;
  cpkt_sus_result result;

  state = (struct cpkt_sus_segmented_text_state *)user;
  if (state == NULL || event == NULL || event->text == NULL) {
    return 1;
  }
  result = cpkt_sus_segmented_text_apply(state, event->text,
                                        (size_t)event->text_length);
  if (result != CPKT_SUS_OK) {
    state->result = result;
    return 1;
  }
  if (state->forward_sink != NULL) {
    return state->forward_sink(event, state->forward_user);
  }
  return 0;
}

static cpkt_sus_result
cpkt_sus_capture_prompt_tokens(struct whisper_context *context,
                               whisper_token **tokens, size_t *count,
                               size_t *capacity) {
  whisper_token *grown;
  int segment_count;
  int token_count;
  int i;
  int j;

  if (context == NULL || tokens == NULL || count == NULL || capacity == NULL) {
    return CPKT_SUS_ERR_ARG;
  }

  *count = 0U;
  segment_count = whisper_full_n_segments(context);
  for (i = 0; i < segment_count; ++i) {
    token_count = whisper_full_n_tokens(context, i);
    if (token_count < 0) {
      return CPKT_SUS_ERR_UPSTREAM;
    }
    if ((size_t)token_count > ((size_t)-1) - *count) {
      return CPKT_SUS_ERR_ALLOC;
    }
    if (*count + (size_t)token_count > *capacity) {
      size_t next_capacity;

      next_capacity = *capacity == 0U ? 64U : *capacity;
      while (next_capacity < *count + (size_t)token_count) {
        if (next_capacity > ((size_t)-1) / 2U) {
          return CPKT_SUS_ERR_ALLOC;
        }
        next_capacity *= 2U;
      }
      grown =
          (whisper_token *)realloc(*tokens, sizeof(**tokens) * next_capacity);
      if (grown == NULL) {
        return CPKT_SUS_ERR_ALLOC;
      }
      *tokens = grown;
      *capacity = next_capacity;
    }
    for (j = 0; j < token_count; ++j) {
      (*tokens)[*count] = whisper_full_get_token_id(context, i, j);
      ++*count;
    }
  }
  return CPKT_SUS_OK;
}

struct cpkt_sus_vox_transcribe_state {
  struct cpkt_sus_transcriber_impl *impl;
  struct cpkt_sus_model_impl *model_impl;
  const cpkt_sus_segmented_config *config;
  whisper_token *prompt_tokens;
  size_t prompt_count;
  size_t prompt_capacity;
  unsigned long segment_count;
  long last_t0;
  long last_t1;
  int final_emitted;
  int use_prompt;
  cpkt_sus_result result;
};

static cpkt_sus_result
cpkt_sus_read_vox_segment_pcm(cpkt_audio_vox_segment *segment,
                              float **samples_out,
                              unsigned long *sample_count_out) {
  float *samples;
  size_t offset;

  if (samples_out != NULL) {
    *samples_out = NULL;
  }
  if (sample_count_out != NULL) {
    *sample_count_out = 0UL;
  }
  if (segment == NULL || segment->read_f32_mono_16k == NULL ||
      samples_out == NULL || sample_count_out == NULL ||
      segment->frame_count > (size_t)INT_MAX ||
      segment->frame_count > ((size_t)-1) / sizeof(float)) {
    return CPKT_SUS_ERR_ARG;
  }

  samples = (float *)malloc(sizeof(float) * segment->frame_count);
  if (samples == NULL && segment->frame_count != 0U) {
    return CPKT_SUS_ERR_ALLOC;
  }

  offset = 0U;
  while (offset < segment->frame_count) {
    cpkt_audio_result audio_result;
    size_t frames_read;

    frames_read = 0U;
    audio_result = segment->read_f32_mono_16k(
        segment, samples + offset, segment->frame_count - offset, &frames_read);
    if (audio_result != CPKT_AUDIO_OK && audio_result != CPKT_AUDIO_AT_END) {
      free(samples);
      return CPKT_SUS_ERR_IO;
    }
    offset += frames_read;
    if (audio_result == CPKT_AUDIO_AT_END) {
      break;
    }
    if (frames_read == 0U) {
      free(samples);
      return CPKT_SUS_ERR_IO;
    }
  }
  if (offset != segment->frame_count) {
    free(samples);
    return CPKT_SUS_ERR_IO;
  }

  *samples_out = samples;
  *sample_count_out = (unsigned long)segment->frame_count;
  return CPKT_SUS_OK;
}

static int cpkt_sus_vox_segment_sink(cpkt_audio_vox_segment *segment,
                                     void *user) {
  struct cpkt_sus_vox_transcribe_state *state;
  struct cpkt_sus_transcriber_impl *impl;
  struct whisper_full_params params;
  float *samples;
  char *text;
  unsigned long sample_count;
  const char *language;
  int full_result;

  state = (struct cpkt_sus_vox_transcribe_state *)user;
  if (state == NULL || segment == NULL || state->impl == NULL ||
      state->model_impl == NULL || state->model_impl->context == NULL) {
    return 1;
  }
  if (state->result != CPKT_SUS_OK) {
    return 1;
  }

  samples = NULL;
  sample_count = 0UL;
  state->result =
      cpkt_sus_read_vox_segment_pcm(segment, &samples, &sample_count);
  if (state->result != CPKT_SUS_OK) {
    return 1;
  }

  impl = state->impl;
  impl->aborted = 0;
  impl->callback_error = 0;
  params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
  if (impl->config.threads > 0) {
    params.n_threads = impl->config.threads;
  }
  params.translate = impl->config.translate ? true : false;
  params.no_timestamps = true;
  params.print_progress = false;
  params.print_realtime = false;
  params.print_timestamps = false;
  params.initial_prompt = impl->config.initial_prompt;
  if (state->use_prompt && state->prompt_count > 0U) {
    if (state->prompt_count > (size_t)INT_MAX) {
      free(samples);
      state->result = CPKT_SUS_ERR_ARG;
      return 1;
    }
    params.prompt_tokens = state->prompt_tokens;
    params.prompt_n_tokens = (int)state->prompt_count;
  }
  if (state->config != NULL && state->config->max_tokens != 0UL &&
      !cpkt_sus_ul_to_int(state->config->max_tokens, &params.max_tokens)) {
    free(samples);
    state->result = CPKT_SUS_ERR_ARG;
    return 1;
  }
  if (state->config != NULL && state->config->audio_ctx != 0UL &&
      !cpkt_sus_ul_to_int(state->config->audio_ctx, &params.audio_ctx)) {
    free(samples);
    state->result = CPKT_SUS_ERR_ARG;
    return 1;
  }

  language = impl->config.language;
  if (language == NULL || language[0] == '\0' ||
      strcmp(language, "auto") == 0) {
    params.language = NULL;
  } else {
    params.language = language;
  }

  if (impl->config.progress_sink != NULL) {
    params.progress_callback = cpkt_sus_whisper_progress_callback;
    params.progress_callback_user_data = impl;
  }
  if (impl->config.abort != NULL || impl->config.progress_sink != NULL) {
    params.abort_callback = cpkt_sus_whisper_abort_callback;
    params.abort_callback_user_data = impl;
  }

  cpkt_sus_log_ensure_installed();
  full_result = whisper_full(state->model_impl->context, params, samples,
                             (int)sample_count);
  free(samples);
  if (impl->callback_error) {
    state->result = CPKT_SUS_ERR_CALLBACK;
    return 1;
  }
  if (impl->aborted) {
    state->result = CPKT_SUS_ABORTED;
    return 1;
  }
  if (full_result != 0) {
    state->result = CPKT_SUS_ERR_UPSTREAM;
    return 1;
  }

  text = NULL;
  state->result =
      cpkt_sus_build_segmented_text(&text, state->model_impl->context);
  if (state->result != CPKT_SUS_OK) {
    return 1;
  }
  state->result =
      cpkt_sus_emit_segmented_event(impl, state->config, text, strlen(text),
                                   state->segment_count, segment->t0,
                                   segment->t1, segment->is_final);
  free(text);
  if (state->result != CPKT_SUS_OK) {
    return 1;
  }
  state->last_t0 = segment->t0;
  state->last_t1 = segment->t1;
  if (segment->is_final) {
    state->final_emitted = 1;
  }
  ++state->segment_count;

  if (state->use_prompt) {
    state->result = cpkt_sus_capture_prompt_tokens(
        state->model_impl->context, &state->prompt_tokens, &state->prompt_count,
        &state->prompt_capacity);
    if (state->result != CPKT_SUS_OK) {
      return 1;
    }
  }
  if (impl->config.progress_sink != NULL &&
      impl->config.progress_sink(100, impl->config.progress_user) != 0) {
    state->result = CPKT_SUS_ERR_CALLBACK;
    return 1;
  }
  return 0;
}

static cpkt_sus_result
cpkt_sus_transcriber_transcribe_audio_decoder_segmented_impl(
    cpkt_sus_transcriber *self, cpkt_audio_decoder *decoder,
    const cpkt_sus_segmented_config *config) {
  struct cpkt_sus_transcriber_impl *impl;
  struct cpkt_sus_model_impl *model_impl;
  struct cpkt_sus_vox_transcribe_state state;
  cpkt_audio_vox_config vox_config;
  cpkt_audio_vox *vox;
  float *read_buffer;
  unsigned long read_frames;
  unsigned long length_ms;
  unsigned long keep_ms;
  size_t frames_read;
  int at_end;
  cpkt_sus_result sus_result;

  if (self == NULL || self->impl == NULL || decoder == NULL ||
      decoder->read_f32_mono_16k == NULL) {
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

  read_frames = config != NULL && config->read_frames != 0UL
                    ? config->read_frames
                    : 4096UL;
  if (config != NULL && config->length_ms != 0UL) {
    length_ms = config->length_ms;
  } else if (config != NULL &&
             config->mode == CPKT_SUS_SEGMENT_MODE_CONTINUOUS) {
    length_ms = 7000UL;
  } else {
    length_ms = 0UL;
  }
  keep_ms = config != NULL && config->keep_ms != 0UL ? config->keep_ms : 1500UL;
  if (read_frames == 0UL || read_frames > ((unsigned long)-1) / sizeof(float)) {
    return CPKT_SUS_ERR_ARG;
  }

  read_buffer = (float *)malloc(sizeof(float) * (size_t)read_frames);
  if (read_buffer == NULL) {
    return CPKT_SUS_ERR_ALLOC;
  }

  cpkt_sus_transcriber_reset_revised_text(impl);
  memset(&state, 0, sizeof(state));
  state.impl = impl;
  state.model_impl = model_impl;
  state.config = config;
  state.use_prompt = (config == NULL || config->keep_context >= 0) ? 1 : 0;
  state.result = CPKT_SUS_OK;

  memset(&vox_config, 0, sizeof(vox_config));
  vox_config.threshold = config != NULL && config->vox_threshold != 0.0f
                             ? config->vox_threshold
                             : 0.001f;
  vox_config.release_silence_ms = keep_ms;
  vox_config.max_segment_ms = length_ms;
  vox_config.min_segment_ms = 100UL;
  vox_config.prebuffer_ms = config != NULL ? config->prebuffer_ms : 0UL;
  vox_config.memory_spool_bytes =
      config != NULL ? config->memory_spool_bytes : 0UL;
  vox_config.max_spool_bytes = config != NULL ? config->max_spool_bytes : 0UL;
  vox_config.segment_sink = cpkt_sus_vox_segment_sink;
  vox_config.segment_user = &state;

  vox = NULL;
  sus_result = cpkt_audio_vox_open(&vox, &vox_config) == CPKT_AUDIO_OK
                   ? CPKT_SUS_OK
                   : CPKT_SUS_ERR_ARG;
  if (sus_result != CPKT_SUS_OK) {
    goto cleanup;
  }

  at_end = 0;
  while (!at_end) {
    cpkt_audio_result audio_result;
    cpkt_audio_result decoder_result;

    frames_read = 0U;
    decoder_result = decoder->read_f32_mono_16k(
        decoder, read_buffer, (size_t)read_frames, &frames_read);
    if (decoder_result != CPKT_AUDIO_OK &&
        decoder_result != CPKT_AUDIO_AT_END) {
      sus_result = CPKT_SUS_ERR_IO;
      goto cleanup;
    }
    if (frames_read > 0U) {
      audio_result = vox->push_f32_mono_16k(vox, read_buffer, frames_read);
      if (audio_result != CPKT_AUDIO_OK) {
        sus_result =
            state.result != CPKT_SUS_OK ? state.result : CPKT_SUS_ERR_IO;
        goto cleanup;
      }
    }
    if (decoder_result == CPKT_AUDIO_AT_END || frames_read == 0U) {
      at_end = 1;
    }
  }

  if (vox->flush(vox) != CPKT_AUDIO_OK) {
    sus_result = state.result != CPKT_SUS_OK ? state.result : CPKT_SUS_ERR_IO;
    goto cleanup;
  }
  if (!state.final_emitted) {
    sus_result = cpkt_sus_emit_segmented_event(
        impl, config, NULL, 0U,
        state.segment_count == 0UL ? 0UL : state.segment_count - 1UL,
        state.last_t0, state.last_t1, 1);
    if (sus_result != CPKT_SUS_OK) {
      goto cleanup;
    }
  }

cleanup:
  free(state.prompt_tokens);
  if (vox != NULL) {
    vox->destroy(vox);
  }
  free(read_buffer);
  return sus_result;
}

static cpkt_sus_result cpkt_sus_transcriber_transcribe_audio_vox_segment_impl(
    cpkt_sus_transcriber *self, cpkt_audio_vox_segment *segment,
    const cpkt_sus_segmented_config *config) {
  struct cpkt_sus_transcriber_impl *impl;
  struct cpkt_sus_model_impl *model_impl;
  struct cpkt_sus_vox_transcribe_state state;
  cpkt_sus_result result;

  if (self == NULL || self->impl == NULL || segment == NULL ||
      segment->read_f32_mono_16k == NULL) {
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

  memset(&state, 0, sizeof(state));
  state.impl = impl;
  state.model_impl = model_impl;
  state.config = config;
  state.prompt_tokens = impl->prompt_tokens;
  state.prompt_count = impl->prompt_count;
  state.prompt_capacity = impl->prompt_capacity;
  state.segment_count = impl->segmented_segment_count;
  state.use_prompt = (config == NULL || config->keep_context >= 0) ? 1 : 0;
  state.result = CPKT_SUS_OK;

  impl->prompt_tokens = NULL;
  impl->prompt_count = 0U;
  impl->prompt_capacity = 0U;

  if (state.segment_count == 0UL) {
    cpkt_sus_transcriber_reset_revised_text(impl);
  }

  if (cpkt_sus_vox_segment_sink(segment, &state) == 0) {
    result = state.result;
  } else {
    result = state.result != CPKT_SUS_OK ? state.result : CPKT_SUS_ERR_CALLBACK;
  }
  impl->prompt_tokens = state.prompt_tokens;
  impl->prompt_count = state.prompt_count;
  impl->prompt_capacity = state.prompt_capacity;
  impl->segmented_segment_count = state.segment_count;
  return result;
}

static cpkt_sus_result
cpkt_sus_transcriber_transcribe_audio_decoder_segmented_text_impl(
    cpkt_sus_transcriber *self, cpkt_audio_decoder *decoder,
    const cpkt_sus_segmented_config *config, char **text_out) {
  struct cpkt_sus_segmented_text_state state;
  cpkt_sus_segmented_config wrapped_config;
  cpkt_sus_result result;

  if (text_out != NULL) {
    *text_out = NULL;
  }
  if (text_out == NULL) {
    return CPKT_SUS_ERR_ARG;
  }

  memset(&state, 0, sizeof(state));
  state.result = CPKT_SUS_OK;
  memset(&wrapped_config, 0, sizeof(wrapped_config));
  if (config != NULL) {
    wrapped_config = *config;
  }
  state.forward_sink = wrapped_config.segmented_sink;
  state.forward_user = wrapped_config.segmented_user;
  wrapped_config.segmented_sink = cpkt_sus_segmented_text_sink;
  wrapped_config.segmented_user = &state;

  result = cpkt_sus_transcriber_transcribe_audio_decoder_segmented_impl(
      self, decoder, &wrapped_config);
  if (result == CPKT_SUS_ERR_CALLBACK && state.result != CPKT_SUS_OK) {
    result = state.result;
  }
  if (result != CPKT_SUS_OK) {
    free(state.text);
    return result;
  }
  if (state.text == NULL) {
    state.text = (char *)malloc(1U);
    if (state.text == NULL) {
      return CPKT_SUS_ERR_ALLOC;
    }
    state.text[0] = '\0';
  }
  *text_out = state.text;
  return CPKT_SUS_OK;
}

static cpkt_sus_result
cpkt_sus_transcriber_revised_text_impl(cpkt_sus_transcriber *self,
                                       char **text_out) {
  struct cpkt_sus_transcriber_impl *impl;
  const char *text;
  size_t length;

  if (text_out != NULL) {
    *text_out = NULL;
  }
  if (text_out == NULL || self == NULL || self->impl == NULL) {
    return CPKT_SUS_ERR_ARG;
  }
  impl = (struct cpkt_sus_transcriber_impl *)self->impl;
  text = impl->revised_text != NULL ? impl->revised_text : "";
  length = impl->revised_text != NULL ? impl->revised_length : 0U;
  *text_out = cpkt_sus_strdup_range(text, length);
  return *text_out != NULL ? CPKT_SUS_OK : CPKT_SUS_ERR_ALLOC;
}

static void cpkt_sus_transcriber_destroy_impl(cpkt_sus_transcriber *self) {
  struct cpkt_sus_transcriber_impl *impl;

  if (self == NULL) {
    return;
  }
  impl = (struct cpkt_sus_transcriber_impl *)self->impl;
  if (impl != NULL) {
    free(impl->revised_text);
    free(impl->prompt_tokens);
  }
  free(self->impl);
  free(self);
}

static cpkt_sus_result
cpkt_sus_reset_transcript_spacing_impl(cpkt_sus *self) {
  struct cpkt_sus_model_impl *impl;

  if (self == NULL || self->impl == NULL) {
    return CPKT_SUS_ERR_ARG;
  }

  impl = (struct cpkt_sus_model_impl *)self->impl;
  impl->segmented_initial_space_seen = 0;
  return CPKT_SUS_OK;
}

static cpkt_sus_result cpkt_sus_create_transcriber_impl(
    cpkt_sus *self, cpkt_sus_transcriber **out,
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
  transcriber->transcribe_audio_decoder_segmented =
      cpkt_sus_transcriber_transcribe_audio_decoder_segmented_impl;
  transcriber->transcribe_audio_vox_segment =
      cpkt_sus_transcriber_transcribe_audio_vox_segment_impl;
  transcriber->transcribe_audio_decoder_segmented_text =
      cpkt_sus_transcriber_transcribe_audio_decoder_segmented_text_impl;
  transcriber->revised_text = cpkt_sus_transcriber_revised_text_impl;
  transcriber->destroy = cpkt_sus_transcriber_destroy_impl;
  *out = transcriber;
  return CPKT_SUS_OK;
}

/** Initializes path-open config with documented defaults. */
void cpkt_sus_config_default(cpkt_sus_config *config) {
  if (config != NULL) {
    memset(config, 0, sizeof(*config));
  }
}

/** Initializes cache-open config with documented defaults. */
void cpkt_sus_cache_config_default(cpkt_sus_cache_config *config) {
  if (config != NULL) {
    memset(config, 0, sizeof(*config));
  }
}

/** Initializes transcriber config with documented defaults. */
void cpkt_sus_transcriber_config_default(cpkt_sus_transcriber_config *config) {
  if (config != NULL) {
    memset(config, 0, sizeof(*config));
  }
}

/** Initializes segmented transcription config with documented defaults. */
void cpkt_sus_segmented_config_default(cpkt_sus_segmented_config *config) {
  if (config != NULL) {
    memset(config, 0, sizeof(*config));
  }
}

/** Opens a receiver-shell speech facade instance from a model path. */
cpkt_sus_result cpkt_sus_open_path(cpkt_sus **out,
                                   const cpkt_sus_config *config) {
  cpkt_sus *model;
  struct cpkt_sus_model_impl *impl;
  struct whisper_context_params params;

  if (out != NULL) {
    *out = NULL;
  }
  if (out == NULL || config == NULL || config->model_path == NULL ||
      config->model_path[0] == '\0') {
    return CPKT_SUS_ERR_ARG;
  }

  model = (cpkt_sus *)calloc(1, sizeof(*model));
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

  cpkt_sus_log_ensure_installed();
  impl->context =
      whisper_init_from_file_with_params(config->model_path, params);
  if (impl->context == NULL) {
    free(impl);
    free(model);
    return CPKT_SUS_ERR_MODEL;
  }
  impl->cpu_only = config->cpu_only ? 1 : 0;
  impl->preserve_initial_space_after_first_transcriber =
      config->preserve_initial_space_after_first_transcriber ? 1 : 0;

  model->impl = impl;
  model->info = cpkt_sus_info_impl;
  model->create_transcriber = cpkt_sus_create_transcriber_impl;
  model->reset_transcript_spacing =
      cpkt_sus_reset_transcript_spacing_impl;
  model->destroy = cpkt_sus_destroy_impl;
  *out = model;
  return CPKT_SUS_OK;
}

/** Opens a cache-backed instance through the curated local resolver. */
cpkt_sus_result cpkt_sus_open_cached(cpkt_sus **out,
                                     const cpkt_sus_cache_config *config) {
  const struct cpkt_sus_catalog_entry *entry;
  cpkt_sus_result result;
  char *cache_dir;
  char *model_path;

  if (out != NULL) {
    *out = NULL;
  }
  if (out == NULL || config == NULL) {
    return CPKT_SUS_ERR_ARG;
  }

  if (config->sha256 != NULL && config->sha256[0] != '\0' &&
      !cpkt_sus_is_sha256_hex(config->sha256)) {
    return CPKT_SUS_ERR_ARG;
  }

  entry = cpkt_sus_find_catalog_entry(config->model);
  if (entry == NULL) {
    return CPKT_SUS_ERR_LOOKUP;
  }

  cache_dir = NULL;
  model_path = NULL;
  result = cpkt_sus_cache_dir_from_config(&cache_dir, config);
  if (result != CPKT_SUS_OK) {
    return result;
  }
  result = cpkt_sus_join2(&model_path, cache_dir, entry->filename);
  if (result != CPKT_SUS_OK) {
    free(cache_dir);
    return result;
  }
  result = cpkt_sus_emit_cache_status(
      config, CPKT_SUS_CACHE_STATUS_LOOKUP, entry, model_path, NULL);
  if (result != CPKT_SUS_OK) {
    free(cache_dir);
    free(model_path);
    return result;
  }

  if (!cpkt_sus_file_exists(model_path)) {
    result = cpkt_sus_emit_cache_status(
        config, CPKT_SUS_CACHE_STATUS_MISS, entry, model_path, NULL);
    if (result != CPKT_SUS_OK) {
      free(cache_dir);
      free(model_path);
      return result;
    }
    result = cpkt_sus_fetch_cached_file(model_path, cache_dir, entry, config);
    if (result != CPKT_SUS_OK) {
      free(cache_dir);
      free(model_path);
      return result;
    }
  } else {
    result = cpkt_sus_emit_cache_status(
        config, CPKT_SUS_CACHE_STATUS_HIT, entry, model_path, NULL);
    if (result != CPKT_SUS_OK) {
      free(cache_dir);
      free(model_path);
      return result;
    }
    result =
        cpkt_sus_open_validated_cached_file(out, model_path, entry, config);
    if ((result == CPKT_SUS_ERR_CHECKSUM || result == CPKT_SUS_ERR_MODEL) &&
        !config->offline) {
      result = cpkt_sus_fetch_cached_file(model_path, cache_dir, entry, config);
      if (result != CPKT_SUS_OK) {
        free(cache_dir);
        free(model_path);
        return result;
      }
    } else if (result != CPKT_SUS_OK) {
      free(cache_dir);
      free(model_path);
      return result;
    }
  }
  free(cache_dir);

  if (*out == NULL) {
    result =
        cpkt_sus_open_validated_cached_file(out, model_path, entry, config);
  }
  free(model_path);
  return result;
}

/** Creates a transcriber bound to an opened instance. */
cpkt_sus_result cpkt_sus_create_transcriber(
    cpkt_sus *sus, cpkt_sus_transcriber **out,
    const cpkt_sus_transcriber_config *config) {
  if (out != NULL) {
    *out = NULL;
  }
  if (sus == NULL || sus->create_transcriber == NULL) {
    return CPKT_SUS_ERR_ARG;
  }
  return sus->create_transcriber(sus, out, config);
}

/** Resets instance-level segmented transcript spacing state. */
cpkt_sus_result cpkt_sus_reset_transcript_spacing(cpkt_sus *sus) {
  if (sus == NULL || sus->reset_transcript_spacing == NULL) {
    return CPKT_SUS_ERR_ARG;
  }
  return sus->reset_transcript_spacing(sus);
}

/** Releases strings allocated by materialized transcription helpers. */
void cpkt_sus_string_free(char *text) { free(text); }

/** Returns the number of entries in the curated cached-model catalog. */
unsigned long cpkt_sus_model_catalog_count(void) {
  return cpkt_sus_catalog_count_internal();
}

/** Copies one curated cached-model catalog entry by index. */
cpkt_sus_result cpkt_sus_model_catalog_entry(unsigned long index,
                                             cpkt_sus_model_entry *entry) {
  if (entry != NULL) {
    memset(entry, 0, sizeof(*entry));
  }
  if (entry == NULL) {
    return CPKT_SUS_ERR_ARG;
  }
  if (index >= cpkt_sus_catalog_count_internal()) {
    return CPKT_SUS_ERR_LOOKUP;
  }
  cpkt_sus_copy_catalog_entry(entry, &cpkt_sus_catalog[index]);
  return CPKT_SUS_OK;
}

/** Copies a curated cached-model catalog entry by model name. */
cpkt_sus_result cpkt_sus_model_catalog_find(const char *name,
                                            cpkt_sus_model_entry *entry) {
  const struct cpkt_sus_catalog_entry *found;

  if (entry != NULL) {
    memset(entry, 0, sizeof(*entry));
  }
  if (entry == NULL) {
    return CPKT_SUS_ERR_ARG;
  }
  found = cpkt_sus_find_catalog_entry(name);
  if (found == NULL) {
    return CPKT_SUS_ERR_LOOKUP;
  }
  cpkt_sus_copy_catalog_entry(entry, found);
  return CPKT_SUS_OK;
}

/** Copies the default curated cached-model catalog entry. */
cpkt_sus_result cpkt_sus_model_catalog_default(cpkt_sus_model_entry *entry) {
  return cpkt_sus_model_catalog_find(NULL, entry);
}

/** Returns the linked backend version string. */
const char *cpkt_sus_backend_version(void) { return whisper_version(); }

/** Returns the linked backend system information string. */
const char *cpkt_sus_backend_system_info(void) {
  return whisper_print_system_info();
}

/** Returns the compiled backend capability list. */
const char *cpkt_sus_backend_capabilities(void) {
  return CPKT_SUS_BACKEND_CAPABILITIES;
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
  case CPKT_SUS_ERR_LOOKUP:
    return "model lookup failed";
  case CPKT_SUS_ERR_IO:
    return "I/O error";
  case CPKT_SUS_ERR_CHECKSUM:
    return "checksum mismatch";
  case CPKT_SUS_ERR_NETWORK:
    return "network error";
  case CPKT_SUS_ABORTED:
    return "transcription aborted";
  default:
    return "unknown result";
  }
}
