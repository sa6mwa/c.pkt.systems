#include <cpkt/cmocka.h>
#include <errno.h>
#include <string.h>

static int check_scalar(unsigned long value, unsigned long data) {
  return value == data;
}
static int check_words(CMockaValueData value, CMockaValueData data) {
  return value.words.high == data.words.high &&
         value.words.low == data.words.low;
}
static void parameter_receiver(long signed_value, unsigned long value,
                               double number, const char *text,
                               const void *memory) {
  check_expected_int(parameter_receiver, signed_value);
  check_expected_uint(parameter_receiver, value);
  check_expected_double(parameter_receiver, number);
  check_expected_ptr(parameter_receiver, text);
  check_expected_ptr(parameter_receiver, memory);
}
static void setter(long *number, unsigned long *positive, double *real,
                   void **pointer) {
  *number = mock_parameter_int(setter, number);
  *positive = mock_parameter_uint(setter, positive);
  *real = mock_parameter_double(setter, real);
  *pointer = mock_parameter_ptr(setter, pointer);
  mock_errno(setter);
}
static void surface(void **state) {
  long number;
  unsigned long positive;
  double real;
  void *pointer;
  void *allocation;
  unsigned long set[] = {3, 7};
  long signed_set[] = {-3, -7};
  double real_set[] = {2.5, 7.5};
  CpktCmockaWords wide = cpkt_cmocka_words(0xffffffffUL, 0x87654321UL);
  CMockaValueData observed;
  const char bytes[] = "bytes";
  (void)state;
  expect_int_in_set(parameter_receiver, signed_value, signed_set);
  expect_uint_in_set(parameter_receiver, value, set);
  expect_double(parameter_receiver, number, 2.5, 0.01);
  expect_string(parameter_receiver, text, "text");
  expect_memory(parameter_receiver, memory, bytes, sizeof(bytes));
  parameter_receiver(-7, 7, 2.5, "text", bytes);
  expect_int_in_range(parameter_receiver, signed_value, -8, -2);
  expect_check(parameter_receiver, value, check_scalar, 3);
  expect_double(parameter_receiver, number, 7.5, 0.01);
  expect_not_string(parameter_receiver, text, "other");
  expect_not_memory(parameter_receiver, memory, "other", sizeof(bytes));
  parameter_receiver(-3, 3, 7.5, "text", bytes);
  will_set_parameter_int(setter, number, -9);
  will_set_parameter_uint(setter, positive, 9);
  will_set_parameter_double(setter, real, 2.5);
  will_set_parameter_ptr(setter, pointer, &number);
  will_set_errno(setter, EINVAL);
  setter(&number, &positive, &real, &pointer);
  assert_int_equal(number, -9);
  assert_uint_equal(positive, 9);
  assert_double_equal(real, 2.5, 0.01);
  assert_ptr_equal(pointer, &number);
  assert_int_equal(errno, EINVAL);
  cpkt_cmocka_expect_check_data("wide", "argument", __FILE__, __LINE__,
                                check_words, cpkt_cmocka_value_words(wide),
                                NULL, 1);
  cpkt_cmocka_check_expected("wide", "argument", __FILE__, __LINE__,
                             cpkt_cmocka_value_words(wide));
  cpkt_cmocka_will_return("wide", __FILE__, __LINE__, NULL,
                          cpkt_cmocka_value_words(wide), 1);
  observed = cpkt_cmocka_mock("wide", __FILE__, __LINE__, NULL);
  cpkt_cmocka_assert_uint_equal_words(observed.words, wide, __FILE__, __LINE__);
  cpkt_cmocka_assert_int_equal_words(observed.words, wide, __FILE__, __LINE__);
  assert_int_in_range(-3, -9, 0);
  assert_uint_in_range(7, 3, 9);
  assert_int_not_in_range(1, -9, 0);
  assert_uint_not_in_range(1, 3, 9);
  assert_float_in_set(2.5, real_set, 2, 0.01);
  assert_float_not_in_set(4.5, real_set, 2, 0.01);
  assert_memory_equal(bytes, "bytes", sizeof(bytes));
  assert_memory_not_equal(bytes, "other", sizeof(bytes));
  assert_non_null_msg(bytes, "bytes present");
  assert_null_msg(NULL, "null present");
  allocation = test_malloc(8);
  allocation = test_realloc(allocation, 16);
  test_free(allocation);
  allocation = test_calloc(2, 8);
  test_free(allocation);
}
int main(void) {
  const struct CMUnitTest tests[] = {cmocka_unit_test(surface)};
  return cmocka_run_group_tests_name("complete-c89-surface", tests, NULL, NULL);
}
