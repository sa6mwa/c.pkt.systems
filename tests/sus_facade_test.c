#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cmocka.h>

#include <cpkt/sus.h>

static int cpkt_test_file_exists(const char *path) {
  struct stat st;

  return stat(path, &st) == 0 ? 1 : 0;
}

static int cpkt_test_file_equals(const char *path, const char *expected) {
  char buffer[128];
  FILE *file;
  size_t expected_len;
  size_t read_len;

  if (path == NULL || expected == NULL) {
    return 0;
  }
  expected_len = strlen(expected);
  if (expected_len + 1U > sizeof(buffer)) {
    return 0;
  }
  file = fopen(path, "rb");
  if (file == NULL) {
    return 0;
  }
  read_len = fread(buffer, 1U, sizeof(buffer), file);
  if (fclose(file) != 0) {
    return 0;
  }
  return read_len == expected_len && memcmp(buffer, expected, expected_len) == 0
             ? 1
             : 0;
}

static int cpkt_test_dir_entry_count(const char *path, unsigned long *out) {
  DIR *dir;
  struct dirent *entry;
  unsigned long count;

  if (out != NULL) {
    *out = 0UL;
  }
  if (path == NULL || out == NULL) {
    return -1;
  }
  dir = opendir(path);
  if (dir == NULL) {
    return -1;
  }
  count = 0UL;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
      ++count;
    }
  }
  if (closedir(dir) != 0) {
    return -1;
  }
  *out = count;
  return 0;
}

struct cpkt_test_log_capture {
  unsigned long count;
  int last_level;
  const char *last_component;
  const char *last_message;
};

struct cpkt_test_cache_status_capture {
  unsigned long count;
  int phases[16];
  char last_model[128];
  char last_cache_path[512];
  char last_source_url[512];
  int fail_on_phase;
};

struct cpkt_test_env_guard {
  char *xdg_cache_home;
  char *home;
  int had_xdg_cache_home;
  int had_home;
};

static char *cpkt_test_strdup(const char *value) {
  char *copy;
  size_t len;

  if (value == NULL) {
    return NULL;
  }
  len = strlen(value);
  copy = (char *)malloc(len + 1U);
  if (copy == NULL) {
    return NULL;
  }
  memcpy(copy, value, len + 1U);
  return copy;
}

static void cpkt_test_env_guard_save(struct cpkt_test_env_guard *guard) {
  const char *value;

  memset(guard, 0, sizeof(*guard));
  value = getenv("XDG_CACHE_HOME");
  if (value != NULL) {
    guard->xdg_cache_home = cpkt_test_strdup(value);
    assert_non_null(guard->xdg_cache_home);
    guard->had_xdg_cache_home = 1;
  }
  value = getenv("HOME");
  if (value != NULL) {
    guard->home = cpkt_test_strdup(value);
    assert_non_null(guard->home);
    guard->had_home = 1;
  }
}

static void cpkt_test_env_guard_restore(struct cpkt_test_env_guard *guard) {
  if (guard->had_xdg_cache_home) {
    assert_int_equal(setenv("XDG_CACHE_HOME", guard->xdg_cache_home, 1), 0);
  } else {
    assert_int_equal(unsetenv("XDG_CACHE_HOME"), 0);
  }
  if (guard->had_home) {
    assert_int_equal(setenv("HOME", guard->home, 1), 0);
  } else {
    assert_int_equal(unsetenv("HOME"), 0);
  }
  free(guard->xdg_cache_home);
  free(guard->home);
  memset(guard, 0, sizeof(*guard));
}

static void cpkt_test_log_sink(const cpkt_sus_log_event *event, void *user) {
  struct cpkt_test_log_capture *capture;

  capture = (struct cpkt_test_log_capture *)user;
  assert_non_null(capture);
  assert_non_null(event);
  assert_non_null(event->component);
  assert_non_null(event->message);
  assert_true(event->level >= CPKT_SUS_LOG_NONE);
  assert_true(event->level != CPKT_SUS_LOG_CONT);
  assert_true(event->level <= CPKT_SUS_LOG_ERROR);
  ++capture->count;
  capture->last_level = event->level;
  capture->last_component = event->component;
  capture->last_message = event->message;
}

static int cpkt_test_cache_status_sink(
    const cpkt_sus_cache_status_event *event, void *user) {
  struct cpkt_test_cache_status_capture *capture;

  capture = (struct cpkt_test_cache_status_capture *)user;
  assert_non_null(capture);
  assert_non_null(event);
  assert_non_null(event->model);
  if (capture->count < 16UL) {
    capture->phases[capture->count] = event->phase;
  }
  ++capture->count;
  if (event->model != NULL) {
    assert_true(snprintf(capture->last_model, sizeof(capture->last_model),
                         "%s", event->model) <
                (int)sizeof(capture->last_model));
  }
  if (event->cache_path != NULL) {
    assert_true(snprintf(capture->last_cache_path,
                         sizeof(capture->last_cache_path), "%s",
                         event->cache_path) <
                (int)sizeof(capture->last_cache_path));
  }
  if (event->source_url != NULL) {
    assert_true(snprintf(capture->last_source_url,
                         sizeof(capture->last_source_url), "%s",
                         event->source_url) <
                (int)sizeof(capture->last_source_url));
  }
  return event->phase == capture->fail_on_phase ? 1 : 0;
}

static void test_backend_metadata(void **state) {
  (void)state;

  assert_non_null(cpkt_sus_backend_version());
  assert_true(cpkt_sus_backend_version()[0] != '\0');
  assert_non_null(cpkt_sus_backend_system_info());
  assert_true(cpkt_sus_backend_system_info()[0] != '\0');
  assert_string_equal(cpkt_sus_backend_capabilities(), "cpu");
  assert_string_equal(cpkt_sus_facade_version(), "0");
}

static void test_log_callback_receives_backend_events(void **state) {
  struct cpkt_test_log_capture capture;
  cpkt_sus *sus;
  cpkt_sus_config config;

  (void)state;
  memset(&capture, 0, sizeof(capture));
  cpkt_sus_config_default(&config);
  config.model_path = "cpkt-sus-missing-model.gguf";
  config.cpu_only = 1;

  cpkt_sus_log_set(cpkt_test_log_sink, &capture);
  sus = (cpkt_sus *)1;
  assert_int_equal(cpkt_sus_open_path(&sus, &config), CPKT_SUS_ERR_MODEL);
  assert_null(sus);
  assert_true(capture.count > 0UL);
  assert_string_equal(capture.last_component, "whisper");
  assert_true(capture.last_level >= CPKT_SUS_LOG_NONE);
  assert_true(capture.last_level <= CPKT_SUS_LOG_ERROR);
  assert_non_null(capture.last_message);
  cpkt_sus_log_set(NULL, NULL);
}

static void test_open_model_rejects_invalid_arguments(void **state) {
  cpkt_sus_model *model;
  cpkt_sus *sus;
  cpkt_sus_config config;

  (void)state;
  cpkt_sus_config_default(&config);

  sus = (cpkt_sus *)1;
  assert_int_equal(cpkt_sus_open_path(NULL, &config), CPKT_SUS_ERR_ARG);
  assert_int_equal(cpkt_sus_open_path(&sus, NULL), CPKT_SUS_ERR_ARG);
  assert_null(sus);

  model = (cpkt_sus_model *)1;
  assert_int_equal(cpkt_sus_model_open_path(&model, NULL), CPKT_SUS_ERR_ARG);
  assert_null(model);

  sus = (cpkt_sus *)1;
  assert_int_equal(cpkt_sus_open_path(&sus, &config), CPKT_SUS_ERR_ARG);
  assert_null(sus);

  model = (cpkt_sus_model *)1;
  assert_int_equal(cpkt_sus_model_open_path(&model, &config), CPKT_SUS_ERR_ARG);
  assert_null(model);

  config.model_path = "";
  sus = (cpkt_sus *)1;
  assert_int_equal(cpkt_sus_open_path(&sus, &config), CPKT_SUS_ERR_ARG);
  assert_null(sus);

  model = (cpkt_sus_model *)1;
  assert_int_equal(cpkt_sus_model_open_path(&model, &config), CPKT_SUS_ERR_ARG);
  assert_null(model);
}

static void test_open_model_reports_load_failure(void **state) {
  cpkt_sus_model *model;
  cpkt_sus_config config;
  cpkt_sus *sus;

  (void)state;
  cpkt_sus_config_default(&config);
  config.model_path = "cpkt-sus-missing-model.gguf";
  config.cpu_only = 1;

  sus = (cpkt_sus *)1;
  assert_int_equal(cpkt_sus_open_path(&sus, &config), CPKT_SUS_ERR_MODEL);
  assert_null(sus);

  model = (cpkt_sus_model *)1;
  assert_int_equal(cpkt_sus_model_open_path(&model, &config),
                   CPKT_SUS_ERR_MODEL);
  assert_null(model);
}

static void test_model_helpers_reject_invalid_arguments(void **state) {
  cpkt_sus_transcriber *transcriber;

  (void)state;

  transcriber = (cpkt_sus_transcriber *)1;
  assert_int_equal(cpkt_sus_create_transcriber(NULL, &transcriber, NULL),
                   CPKT_SUS_ERR_ARG);
  assert_null(transcriber);
  assert_int_equal(cpkt_sus_reset_transcript_spacing(NULL), CPKT_SUS_ERR_ARG);

  cpkt_sus_string_free(NULL);
}

static void test_config_default_helpers(void **state) {
  cpkt_sus_config config;
  cpkt_sus_cache_config cache_config;
  cpkt_sus_transcriber_config transcriber_config;
  cpkt_sus_segmented_config segmented_config;

  (void)state;

  memset(&config, 0xff, sizeof(config));
  memset(&cache_config, 0xff, sizeof(cache_config));
  memset(&transcriber_config, 0xff, sizeof(transcriber_config));
  memset(&segmented_config, 0xff, sizeof(segmented_config));

  cpkt_sus_config_default(&config);
  cpkt_sus_cache_config_default(&cache_config);
  cpkt_sus_transcriber_config_default(&transcriber_config);
  cpkt_sus_segmented_config_default(&segmented_config);

  assert_null(config.model_path);
  assert_int_equal(config.cpu_only, 0);
  assert_null(cache_config.model);
  assert_null(cache_config.status_sink);
  assert_null(transcriber_config.language);
  assert_null(transcriber_config.progress_sink);
  assert_int_equal(segmented_config.mode, CPKT_SUS_SEGMENT_MODE_SIMPLEX);
  assert_int_equal(segmented_config.keep_context, 0);

  cpkt_sus_config_default(NULL);
  cpkt_sus_cache_config_default(NULL);
  cpkt_sus_transcriber_config_default(NULL);
  cpkt_sus_segmented_config_default(NULL);
}

static void test_model_catalog_queries(void **state) {
  cpkt_sus_model_entry entry;
  cpkt_sus_model_entry other;
  unsigned long count;
  unsigned long i;
  unsigned long j;

  (void)state;

  count = cpkt_sus_model_catalog_count();
  assert_true(count >= 40UL);

  memset(&entry, 0xff, sizeof(entry));
  assert_int_equal(cpkt_sus_model_catalog_entry(0, &entry), CPKT_SUS_OK);
  assert_non_null(entry.name);
  assert_non_null(entry.provider);
  assert_non_null(entry.source_url);
  assert_non_null(entry.filename);
  assert_non_null(entry.sha256);
  assert_int_equal(strlen(entry.sha256), 64);

  memset(&entry, 0, sizeof(entry));
  assert_int_equal(cpkt_sus_model_catalog_default(&entry), CPKT_SUS_OK);
  assert_string_equal(entry.name, "small");
  assert_string_equal(entry.provider, "ggerganov/whisper.cpp");
  assert_string_equal(entry.filename, "ggml-small.bin");
  assert_string_equal(entry.quantization, "f16");
  assert_true(entry.is_default != 0);
  assert_string_equal(
      entry.sha256,
      "1be3a9b2063867b937e64e2ec7483364a79917e157fa98c5d94b5c1fffea987b");

  memset(&entry, 0, sizeof(entry));
  assert_int_equal(cpkt_sus_model_catalog_find("", &entry), CPKT_SUS_OK);
  assert_string_equal(entry.name, "small");

  memset(&entry, 0, sizeof(entry));
  assert_int_equal(cpkt_sus_model_catalog_find("kb-whisper-small", &entry),
                   CPKT_SUS_OK);
  assert_string_equal(entry.provider, "KBLab/kb-whisper-small");
  assert_string_equal(entry.filename, "ggml-small.sv.bin");
  assert_string_equal(entry.license, "Apache-2.0");

  memset(&entry, 0, sizeof(entry));
  assert_int_equal(cpkt_sus_model_catalog_find("small.sv", &entry),
                   CPKT_SUS_OK);
  assert_string_equal(entry.provider, "KBLab/kb-whisper-small");
  assert_string_equal(entry.filename, "ggml-small.sv.bin");
  assert_string_equal(entry.quantization, "f16");

  memset(&entry, 0, sizeof(entry));
  assert_int_equal(cpkt_sus_model_catalog_find("large-v3-turbo:q5_0", &entry),
                   CPKT_SUS_OK);
  assert_string_equal(entry.filename, "ggml-large-v3-turbo-q5_0.bin");
  assert_string_equal(entry.quantization, "q5_0");

  memset(&entry, 0xff, sizeof(entry));
  assert_int_equal(cpkt_sus_model_catalog_find("not-a-model", &entry),
                   CPKT_SUS_ERR_LOOKUP);
  assert_null(entry.name);

  memset(&entry, 0xff, sizeof(entry));
  assert_int_equal(cpkt_sus_model_catalog_entry(count, &entry),
                   CPKT_SUS_ERR_LOOKUP);
  assert_null(entry.name);

  assert_int_equal(cpkt_sus_model_catalog_entry(0, NULL), CPKT_SUS_ERR_ARG);
  assert_int_equal(cpkt_sus_model_catalog_default(NULL), CPKT_SUS_ERR_ARG);

  for (i = 0UL; i < count; ++i) {
    assert_int_equal(cpkt_sus_model_catalog_entry(i, &entry), CPKT_SUS_OK);
    for (j = i + 1UL; j < count; ++j) {
      assert_int_equal(cpkt_sus_model_catalog_entry(j, &other), CPKT_SUS_OK);
      if (strcmp(entry.filename, other.filename) == 0) {
        assert_string_equal(entry.source_url, other.source_url);
        assert_string_equal(entry.sha256, other.sha256);
      }
    }
  }
}

static void test_cached_open_contract(void **state) {
  cpkt_sus_cache_config config;
  cpkt_sus_model *model;
  FILE *file;

  (void)state;
  memset(&config, 0, sizeof(config));
  config.cache_dir = "cpkt-sus-test-cache";
  config.offline = 1;
  (void)mkdir(config.cache_dir, 0700);
  (void)remove("cpkt-sus-test-cache/ggml-small.bin");

  model = (cpkt_sus_model *)1;
  assert_int_equal(cpkt_sus_model_open_cached(NULL, &config), CPKT_SUS_ERR_ARG);

  model = (cpkt_sus_model *)1;
  assert_int_equal(cpkt_sus_model_open_cached(&model, NULL), CPKT_SUS_ERR_ARG);
  assert_null(model);

  model = (cpkt_sus_model *)1;
  assert_int_equal(cpkt_sus_model_open_cached(&model, &config),
                   CPKT_SUS_ERR_IO);
  assert_null(model);

  config.model = "";
  model = (cpkt_sus_model *)1;
  assert_int_equal(cpkt_sus_model_open_cached(&model, &config),
                   CPKT_SUS_ERR_IO);
  assert_null(model);

  config.model = "not-a-cpkt-model";
  model = (cpkt_sus_model *)1;
  assert_int_equal(cpkt_sus_model_open_cached(&model, &config),
                   CPKT_SUS_ERR_LOOKUP);
  assert_null(model);

  config.model = "small";
  config.sha256 = "not-a-sha";
  model = (cpkt_sus_model *)1;
  assert_int_equal(cpkt_sus_model_open_cached(&model, &config),
                   CPKT_SUS_ERR_ARG);
  assert_null(model);

  config.sha256 = NULL;
  file = fopen("cpkt-sus-test-cache/ggml-small.bin", "wb");
  assert_non_null(file);
  assert_int_equal(fwrite("not a whisper model", 1, 19, file), 19);
  assert_int_equal(fclose(file), 0);

  model = (cpkt_sus_model *)1;
  assert_int_equal(cpkt_sus_model_open_cached(&model, &config),
                   CPKT_SUS_ERR_CHECKSUM);
  assert_null(model);

  config.sha256 =
      "88e9294cb41d862d3e2670fb9894c3e46f74fefdd18eadd8f47ee4611406487a";
  model = (cpkt_sus_model *)1;
  assert_int_equal(cpkt_sus_model_open_cached(&model, &config),
                   CPKT_SUS_ERR_MODEL);
  assert_null(model);

  config.sha256 = NULL;
  config.insecure_no_checksum = 1;
  model = (cpkt_sus_model *)1;
  assert_int_equal(cpkt_sus_model_open_cached(&model, &config),
                   CPKT_SUS_ERR_MODEL);
  assert_null(model);

  (void)remove("cpkt-sus-test-cache/ggml-small.bin");
}

static void test_cached_open_default_cache_dir_precedence(void **state) {
  cpkt_sus_cache_config config;
  cpkt_sus_model *model;
  struct cpkt_test_cache_status_capture status;
  struct cpkt_test_env_guard env;

  (void)state;
  cpkt_test_env_guard_save(&env);

  memset(&config, 0, sizeof(config));
  memset(&status, 0, sizeof(status));
  config.model = "tiny";
  config.offline = 1;
  config.status_sink = cpkt_test_cache_status_sink;
  config.status_user = &status;

  assert_int_equal(setenv("XDG_CACHE_HOME", "cpkt-sus-xdg-cache", 1), 0);
  assert_int_equal(setenv("HOME", "cpkt-sus-home", 1), 0);
  model = (cpkt_sus_model *)1;
  assert_int_equal(cpkt_sus_model_open_cached(&model, &config),
                   CPKT_SUS_ERR_IO);
  assert_null(model);
  assert_int_equal(status.count, 2);
  assert_string_equal(
      status.last_cache_path,
      "cpkt-sus-xdg-cache/cpkt/susurro/models/ggml-tiny.bin");

  memset(&status, 0, sizeof(status));
  assert_int_equal(unsetenv("XDG_CACHE_HOME"), 0);
  model = (cpkt_sus_model *)1;
  assert_int_equal(cpkt_sus_model_open_cached(&model, &config),
                   CPKT_SUS_ERR_IO);
  assert_null(model);
  assert_int_equal(status.count, 2);
  assert_string_equal(
      status.last_cache_path,
      "cpkt-sus-home/.cache/cpkt/susurro/models/ggml-tiny.bin");

  memset(&status, 0, sizeof(status));
  assert_int_equal(unsetenv("HOME"), 0);
  model = (cpkt_sus_model *)1;
  assert_int_equal(cpkt_sus_model_open_cached(&model, &config),
                   CPKT_SUS_ERR_IO);
  assert_null(model);
  assert_int_equal(status.count, 0);

  cpkt_test_env_guard_restore(&env);
}

static void test_cached_open_downloads_to_temp_before_rename(void **state) {
  cpkt_sus_cache_config config;
  cpkt_sus_model *model;
  struct cpkt_test_cache_status_capture status;
  char cwd[4096];
  char source_url[4608];
  FILE *file;

  (void)state;
  assert_non_null(getcwd(cwd, sizeof(cwd)));
  assert_true(snprintf(source_url, sizeof(source_url),
                       "file://%s/cpkt-sus-source-model.bin",
                       cwd) < (int)sizeof(source_url));

  (void)remove("cpkt-sus-source-model.bin");
  (void)remove("cpkt-sus-fetch-cache/nested/ggml-small.bin");
  (void)rmdir("cpkt-sus-fetch-cache/nested");
  (void)rmdir("cpkt-sus-fetch-cache");

  file = fopen("cpkt-sus-source-model.bin", "wb");
  assert_non_null(file);
  assert_int_equal(fwrite("not a whisper model", 1, 19, file), 19);
  assert_int_equal(fclose(file), 0);

  memset(&config, 0, sizeof(config));
  memset(&status, 0, sizeof(status));
  config.model = "small";
  config.cache_dir = "cpkt-sus-fetch-cache/nested";
  config.source_url = source_url;
  config.sha256 =
      "88e9294cb41d862d3e2670fb9894c3e46f74fefdd18eadd8f47ee4611406487a";
  config.cpu_only = 1;
  config.status_sink = cpkt_test_cache_status_sink;
  config.status_user = &status;

  model = (cpkt_sus_model *)1;
  assert_int_equal(cpkt_sus_model_open_cached(&model, &config),
                   CPKT_SUS_ERR_MODEL);
  assert_null(model);
  assert_int_equal(status.count, 5);
  assert_int_equal(status.phases[0], CPKT_SUS_CACHE_STATUS_LOOKUP);
  assert_int_equal(status.phases[1], CPKT_SUS_CACHE_STATUS_MISS);
  assert_int_equal(status.phases[2], CPKT_SUS_CACHE_STATUS_DOWNLOAD_BEGIN);
  assert_int_equal(status.phases[3], CPKT_SUS_CACHE_STATUS_DOWNLOAD_COMPLETE);
  assert_int_equal(status.phases[4], CPKT_SUS_CACHE_STATUS_VERIFY_BEGIN);
  assert_string_equal(status.last_model, "small");
  assert_string_equal(status.last_cache_path,
                      "cpkt-sus-fetch-cache/nested/ggml-small.bin");
  assert_false(
      cpkt_test_file_exists("cpkt-sus-fetch-cache/nested/ggml-small.bin"));

  (void)remove("cpkt-sus-source-model.bin");
  (void)remove("cpkt-sus-fetch-cache/nested/ggml-small.bin");
  (void)rmdir("cpkt-sus-fetch-cache/nested");
  (void)rmdir("cpkt-sus-fetch-cache");
}

static void test_cached_open_cleans_up_failed_download(void **state) {
  cpkt_sus_cache_config config;
  cpkt_sus_model *model;
  struct cpkt_test_cache_status_capture status;
  char cwd[4096];
  char source_url[4608];
  unsigned long entry_count;

  (void)state;
  assert_non_null(getcwd(cwd, sizeof(cwd)));
  assert_true(snprintf(source_url, sizeof(source_url),
                       "file://%s/cpkt-sus-missing-source-model.bin",
                       cwd) < (int)sizeof(source_url));

  (void)remove("cpkt-sus-download-fail-cache/ggml-small.bin");
  (void)rmdir("cpkt-sus-download-fail-cache");

  memset(&config, 0, sizeof(config));
  memset(&status, 0, sizeof(status));
  config.model = "small";
  config.cache_dir = "cpkt-sus-download-fail-cache";
  config.source_url = source_url;
  config.cpu_only = 1;
  config.status_sink = cpkt_test_cache_status_sink;
  config.status_user = &status;

  model = (cpkt_sus_model *)1;
  assert_int_equal(cpkt_sus_model_open_cached(&model, &config),
                   CPKT_SUS_ERR_NETWORK);
  assert_null(model);
  assert_int_equal(status.count, 3);
  assert_int_equal(status.phases[0], CPKT_SUS_CACHE_STATUS_LOOKUP);
  assert_int_equal(status.phases[1], CPKT_SUS_CACHE_STATUS_MISS);
  assert_int_equal(status.phases[2], CPKT_SUS_CACHE_STATUS_DOWNLOAD_BEGIN);
  assert_string_equal(status.last_model, "small");
  assert_string_equal(status.last_cache_path,
                      "cpkt-sus-download-fail-cache/ggml-small.bin");
  assert_string_equal(status.last_source_url, source_url);
  assert_false(
      cpkt_test_file_exists("cpkt-sus-download-fail-cache/ggml-small.bin"));
  assert_int_equal(
      cpkt_test_dir_entry_count("cpkt-sus-download-fail-cache", &entry_count),
      0);
  assert_int_equal(entry_count, 0);

  (void)remove("cpkt-sus-download-fail-cache/ggml-small.bin");
  (void)rmdir("cpkt-sus-download-fail-cache");
}

static void test_cached_open_status_callback_can_abort(void **state) {
  cpkt_sus_cache_config config;
  cpkt_sus_model *model;
  struct cpkt_test_cache_status_capture status;

  (void)state;
  (void)remove("cpkt-sus-abort-cache/ggml-small.bin");
  (void)rmdir("cpkt-sus-abort-cache");

  memset(&config, 0, sizeof(config));
  memset(&status, 0, sizeof(status));
  config.model = "small";
  config.cache_dir = "cpkt-sus-abort-cache";
  config.cpu_only = 1;
  config.status_sink = cpkt_test_cache_status_sink;
  config.status_user = &status;
  status.fail_on_phase = CPKT_SUS_CACHE_STATUS_MISS;

  model = (cpkt_sus_model *)1;
  assert_int_equal(cpkt_sus_model_open_cached(&model, &config),
                   CPKT_SUS_ERR_CALLBACK);
  assert_null(model);
  assert_int_equal(status.count, 2);
  assert_int_equal(status.phases[0], CPKT_SUS_CACHE_STATUS_LOOKUP);
  assert_int_equal(status.phases[1], CPKT_SUS_CACHE_STATUS_MISS);
  assert_false(cpkt_test_file_exists("cpkt-sus-abort-cache/ggml-small.bin"));

  (void)remove("cpkt-sus-abort-cache/ggml-small.bin");
  (void)rmdir("cpkt-sus-abort-cache");
}

static void test_cached_open_download_begin_abort_cleans_temp(void **state) {
  cpkt_sus_cache_config config;
  cpkt_sus_model *model;
  struct cpkt_test_cache_status_capture status;
  char cwd[4096];
  char source_url[4608];
  unsigned long entry_count;

  (void)state;
  assert_non_null(getcwd(cwd, sizeof(cwd)));
  assert_true(snprintf(source_url, sizeof(source_url),
                       "file://%s/cpkt-sus-unused-source-model.bin",
                       cwd) < (int)sizeof(source_url));

  (void)remove("cpkt-sus-download-abort-cache/ggml-small.bin");
  (void)rmdir("cpkt-sus-download-abort-cache");

  memset(&config, 0, sizeof(config));
  memset(&status, 0, sizeof(status));
  config.model = "small";
  config.cache_dir = "cpkt-sus-download-abort-cache";
  config.source_url = source_url;
  config.cpu_only = 1;
  config.status_sink = cpkt_test_cache_status_sink;
  config.status_user = &status;
  status.fail_on_phase = CPKT_SUS_CACHE_STATUS_DOWNLOAD_BEGIN;

  model = (cpkt_sus_model *)1;
  assert_int_equal(cpkt_sus_model_open_cached(&model, &config),
                   CPKT_SUS_ERR_CALLBACK);
  assert_null(model);
  assert_int_equal(status.count, 3);
  assert_int_equal(status.phases[0], CPKT_SUS_CACHE_STATUS_LOOKUP);
  assert_int_equal(status.phases[1], CPKT_SUS_CACHE_STATUS_MISS);
  assert_int_equal(status.phases[2], CPKT_SUS_CACHE_STATUS_DOWNLOAD_BEGIN);
  assert_string_equal(status.last_model, "small");
  assert_string_equal(status.last_cache_path,
                      "cpkt-sus-download-abort-cache/ggml-small.bin");
  assert_string_equal(status.last_source_url, source_url);
  assert_false(
      cpkt_test_file_exists("cpkt-sus-download-abort-cache/ggml-small.bin"));
  assert_int_equal(cpkt_test_dir_entry_count("cpkt-sus-download-abort-cache",
                                             &entry_count),
                   0);
  assert_int_equal(entry_count, 0);

  (void)remove("cpkt-sus-download-abort-cache/ggml-small.bin");
  (void)rmdir("cpkt-sus-download-abort-cache");
}

static void test_cached_open_retries_invalid_existing_cache(void **state) {
  cpkt_sus_cache_config config;
  cpkt_sus_model *model;
  char cwd[4096];
  char source_url[4608];
  FILE *file;

  (void)state;
  assert_non_null(getcwd(cwd, sizeof(cwd)));
  assert_true(snprintf(source_url, sizeof(source_url),
                       "file://%s/cpkt-sus-replacement-model.bin",
                       cwd) < (int)sizeof(source_url));

  (void)mkdir("cpkt-sus-retry-cache", 0700);
  (void)remove("cpkt-sus-retry-cache/ggml-small.bin");
  (void)remove("cpkt-sus-replacement-model.bin");

  file = fopen("cpkt-sus-retry-cache/ggml-small.bin", "wb");
  assert_non_null(file);
  assert_int_equal(fwrite("stale corrupt cache", 1, 19, file), 19);
  assert_int_equal(fclose(file), 0);

  file = fopen("cpkt-sus-replacement-model.bin", "wb");
  assert_non_null(file);
  assert_int_equal(fwrite("not a whisper model", 1, 19, file), 19);
  assert_int_equal(fclose(file), 0);

  memset(&config, 0, sizeof(config));
  config.model = "small";
  config.cache_dir = "cpkt-sus-retry-cache";
  config.source_url = source_url;
  config.sha256 =
      "88e9294cb41d862d3e2670fb9894c3e46f74fefdd18eadd8f47ee4611406487a";
  config.cpu_only = 1;

  model = (cpkt_sus_model *)1;
  assert_int_equal(cpkt_sus_model_open_cached(&model, &config),
                   CPKT_SUS_ERR_MODEL);
  assert_null(model);
  assert_true(cpkt_test_file_equals("cpkt-sus-retry-cache/ggml-small.bin",
                                    "stale corrupt cache"));

  (void)remove("cpkt-sus-retry-cache/ggml-small.bin");
  (void)remove("cpkt-sus-replacement-model.bin");
  (void)rmdir("cpkt-sus-retry-cache");
}

static void test_result_strings(void **state) {
  (void)state;

  assert_int_equal(cpkt_sus_result_string(CPKT_SUS_OK)[0], 'o');
  assert_int_equal(cpkt_sus_result_string(CPKT_SUS_ERR_CALLBACK)[0], 'c');
  assert_int_equal(cpkt_sus_result_string(CPKT_SUS_ERR_LOOKUP)[0], 'm');
  assert_int_equal(cpkt_sus_result_string(CPKT_SUS_ERR_IO)[0], 'I');
  assert_int_equal(cpkt_sus_result_string(CPKT_SUS_ERR_CHECKSUM)[0], 'c');
  assert_int_equal(cpkt_sus_result_string(CPKT_SUS_ERR_NETWORK)[0], 'n');
  assert_int_equal(cpkt_sus_result_string(CPKT_SUS_ABORTED)[0], 't');
  assert_int_equal(cpkt_sus_result_string((cpkt_sus_result)999)[0], 'u');
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_backend_metadata),
      cmocka_unit_test(test_log_callback_receives_backend_events),
      cmocka_unit_test(test_open_model_rejects_invalid_arguments),
      cmocka_unit_test(test_open_model_reports_load_failure),
      cmocka_unit_test(test_model_helpers_reject_invalid_arguments),
      cmocka_unit_test(test_config_default_helpers),
      cmocka_unit_test(test_model_catalog_queries),
      cmocka_unit_test(test_cached_open_contract),
      cmocka_unit_test(test_cached_open_default_cache_dir_precedence),
      cmocka_unit_test(test_cached_open_downloads_to_temp_before_rename),
      cmocka_unit_test(test_cached_open_cleans_up_failed_download),
      cmocka_unit_test(test_cached_open_status_callback_can_abort),
      cmocka_unit_test(test_cached_open_download_begin_abort_cleans_temp),
      cmocka_unit_test(test_cached_open_retries_invalid_existing_cache),
      cmocka_unit_test(test_result_strings),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
