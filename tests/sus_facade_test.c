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

static void test_backend_metadata(void **state) {
  (void)state;

  assert_non_null(cpkt_sus_backend_version());
  assert_true(cpkt_sus_backend_version()[0] != '\0');
  assert_non_null(cpkt_sus_backend_system_info());
  assert_true(cpkt_sus_backend_system_info()[0] != '\0');
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

static void test_result_strings(void **state) {
  (void)state;

  assert_int_equal(cpkt_sus_result_string(CPKT_SUS_OK)[0], 'o');
  assert_int_equal(cpkt_sus_result_string(CPKT_SUS_ERR_CALLBACK)[0], 'c');
  assert_int_equal(cpkt_sus_result_string(CPKT_SUS_ERR_LOOKUP)[0], 'm');
  assert_int_equal(cpkt_sus_result_string(CPKT_SUS_ERR_IO)[0], 'I');
  assert_int_equal(cpkt_sus_result_string(CPKT_SUS_ERR_CHECKSUM)[0], 'c');
  assert_int_equal(cpkt_sus_result_string(CPKT_SUS_ERR_NETWORK)[0], 'n');
  assert_int_equal(cpkt_sus_result_string((cpkt_sus_result)999)[0], 'u');
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_backend_metadata),
      cmocka_unit_test(test_open_model_rejects_invalid_arguments),
      cmocka_unit_test(test_open_model_reports_load_failure),
      cmocka_unit_test(test_model_helpers_reject_invalid_arguments),
      cmocka_unit_test(test_cached_open_contract),
      cmocka_unit_test(test_cached_open_downloads_to_temp_before_rename),
      cmocka_unit_test(test_result_strings),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
