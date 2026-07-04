#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
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

static void test_backend_metadata(void **state) {
  (void)state;

  assert_non_null(cpkt_sus_backend_version());
  assert_true(cpkt_sus_backend_version()[0] != '\0');
  assert_non_null(cpkt_sus_backend_system_info());
  assert_true(cpkt_sus_backend_system_info()[0] != '\0');
  assert_string_equal(cpkt_sus_backend_capabilities(), "cpu");
  assert_string_equal(cpkt_sus_facade_version(), "0");
}

static void test_open_model_rejects_invalid_arguments(void **state) {
  cpkt_sus_model *model;
  cpkt_sus *sus;
  cpkt_sus_config config;

  (void)state;
  memset(&config, 0, sizeof(config));

  sus = (cpkt_sus *)1;
  assert_int_equal(cpkt_sus_open_model(NULL, &config), CPKT_SUS_ERR_ARG);
  assert_int_equal(cpkt_sus_open_model(&sus, NULL), CPKT_SUS_ERR_ARG);
  assert_null(sus);

  model = (cpkt_sus_model *)1;
  assert_int_equal(cpkt_sus_model_open_path(&model, NULL), CPKT_SUS_ERR_ARG);
  assert_null(model);

  sus = (cpkt_sus *)1;
  assert_int_equal(cpkt_sus_open_model(&sus, &config), CPKT_SUS_ERR_ARG);
  assert_null(sus);

  model = (cpkt_sus_model *)1;
  assert_int_equal(cpkt_sus_model_open_path(&model, &config), CPKT_SUS_ERR_ARG);
  assert_null(model);

  config.model_path = "";
  sus = (cpkt_sus *)1;
  assert_int_equal(cpkt_sus_open_model(&sus, &config), CPKT_SUS_ERR_ARG);
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
  memset(&config, 0, sizeof(config));
  config.model_path = "cpkt-sus-missing-model.gguf";
  config.cpu_only = 1;

  sus = (cpkt_sus *)1;
  assert_int_equal(cpkt_sus_open_model(&sus, &config), CPKT_SUS_ERR_MODEL);
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
  assert_int_equal(cpkt_sus_model_create_transcriber(NULL, &transcriber, NULL),
                   CPKT_SUS_ERR_ARG);
  assert_null(transcriber);

  cpkt_sus_string_free(NULL);
}

static void test_model_catalog_queries(void **state) {
  cpkt_sus_model_entry entry;
  unsigned long count;

  (void)state;

  count = cpkt_sus_model_catalog_count();
  assert_true(count >= 30UL);

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
  assert_string_equal(entry.filename, "ggml-model.bin");
  assert_string_equal(entry.license, "Apache-2.0");

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

static void test_cached_open_downloads_to_temp_before_rename(void **state) {
  cpkt_sus_cache_config config;
  cpkt_sus_model *model;
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
  config.model = "small";
  config.cache_dir = "cpkt-sus-fetch-cache/nested";
  config.source_url = source_url;
  config.sha256 =
      "88e9294cb41d862d3e2670fb9894c3e46f74fefdd18eadd8f47ee4611406487a";
  config.cpu_only = 1;

  model = (cpkt_sus_model *)1;
  assert_int_equal(cpkt_sus_model_open_cached(&model, &config),
                   CPKT_SUS_ERR_MODEL);
  assert_null(model);
  assert_false(
      cpkt_test_file_exists("cpkt-sus-fetch-cache/nested/ggml-small.bin"));

  (void)remove("cpkt-sus-source-model.bin");
  (void)remove("cpkt-sus-fetch-cache/nested/ggml-small.bin");
  (void)rmdir("cpkt-sus-fetch-cache/nested");
  (void)rmdir("cpkt-sus-fetch-cache");
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
      cmocka_unit_test(test_open_model_rejects_invalid_arguments),
      cmocka_unit_test(test_open_model_reports_load_failure),
      cmocka_unit_test(test_model_helpers_reject_invalid_arguments),
      cmocka_unit_test(test_model_catalog_queries),
      cmocka_unit_test(test_cached_open_contract),
      cmocka_unit_test(test_cached_open_downloads_to_temp_before_rename),
      cmocka_unit_test(test_cached_open_retries_invalid_existing_cache),
      cmocka_unit_test(test_result_strings),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
