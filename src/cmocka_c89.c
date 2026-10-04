/* Private C99 boundary. Public C89 values never impersonate native ABI types.
 */
#include <cmocka.h>
#include <cpkt/cmocka_types.h>
#include <stdlib.h>
#include <string.h>

static __thread const char *cpkt_cmocka_file;
static __thread int cpkt_cmocka_line;

static uintmax_t cpkt_words_native(CpktCmockaWords value) {
  return ((uintmax_t)(value.high & 0xffffffffUL) << 32) |
         (uintmax_t)(value.low & 0xffffffffUL);
}
/** Construct an exact 64-bit two's-complement bit pattern from two C89 words.
 */
CpktCmockaWords cpkt_cmocka_words(unsigned long high, unsigned long low) {
  CpktCmockaWords value;
  value.high = high & 0xffffffffUL;
  value.low = low & 0xffffffffUL;
  return value;
}
static CpktCMockaValueData cpkt_value_public(CMockaValueData value) {
  CpktCMockaValueData result;
  memset(&result, 0, sizeof(result));
  result.kind = 5;
  result.int_val = (long)value.int_val;
  result.uint_val = (unsigned long)value.uint_val;
  result.float_val = value.float_val;
  result.real_val = value.real_val;
  result.ptr = value.ptr;
  result.const_ptr = value.const_ptr;
  result.func = value.func;
  result.words = cpkt_cmocka_words((unsigned long)(value.uint_val >> 32),
                                   (unsigned long)value.uint_val);
  return result;
}
static CMockaValueData cpkt_value_native(CpktCMockaValueData value) {
  CMockaValueData result = {0};
  switch (value.kind) {
  case 0:
    result.int_val = value.int_val;
    break;
  case 1:
    result.uint_val = value.uint_val;
    break;
  case 2:
    result.const_ptr = value.const_ptr;
    break;
  case 3:
    result.float_val = value.float_val;
    break;
  case 4:
    result.real_val = value.real_val;
    break;
  case 5:
    result.uint_val = cpkt_words_native(value.words);
    break;
  default:
    abort();
  }
  return result;
}
/** Construct a signed C89 scalar. Use value_words for full 64-bit values. */
CpktCMockaValueData cpkt_cmocka_value_int(long value) {
  CpktCMockaValueData result;
  memset(&result, 0, sizeof(result));
  result.kind = 0;
  result.int_val = value;
  return result;
}
/** Construct an unsigned C89 scalar. */
CpktCMockaValueData cpkt_cmocka_value_uint(unsigned long value) {
  CpktCMockaValueData result;
  memset(&result, 0, sizeof(result));
  result.kind = 1;
  result.uint_val = value;
  return result;
}
/** Construct a borrowed pointer mock. */
CpktCMockaValueData cpkt_cmocka_value_ptr(const void *value) {
  CpktCMockaValueData result;
  memset(&result, 0, sizeof(result));
  result.kind = 2;
  result.const_ptr = value;
  return result;
}
/** Construct a native single-precision mock. */
CpktCMockaValueData cpkt_cmocka_value_float(float value) {
  CpktCMockaValueData result;
  memset(&result, 0, sizeof(result));
  result.kind = 3;
  result.float_val = value;
  return result;
}
/** Construct a native double-precision mock. */
CpktCMockaValueData cpkt_cmocka_value_double(double value) {
  CpktCMockaValueData result;
  memset(&result, 0, sizeof(result));
  result.kind = 4;
  result.real_val = value;
  return result;
}
/** Construct a full-width mock without C99 integer types. */
CpktCMockaValueData cpkt_cmocka_value_words(CpktCmockaWords value) {
  CpktCMockaValueData result;
  memset(&result, 0, sizeof(result));
  result.kind = 5;
  result.words = value;
  return result;
}
/** Check the explicitly named native return queue. */
int cpkt_cmocka_has_mock(const char *function) {
  return _has_mock(function) ? 1 : 0;
}
/** Empty legacy unit callback. */
void cpkt_cmocka_unit_test_dummy(void **state) { (void)state; }
/** Save caller location for the C89 variadic-function failure entrypoint. */
void cpkt_cmocka_location(const char *file, int line) {
  cpkt_cmocka_file = file;
  cpkt_cmocka_line = line;
}
/** Preserve native printing and longjmp failure semantics at caller location.
 */
void cpkt_cmocka_fail_msg(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vprint_error(format, args);
  va_end(args);
  _fail(cpkt_cmocka_file, cpkt_cmocka_line);
}

/* Native owns and frees the whole event allocation, including adapter context,
 * on success, failure and repeated/always expectations. */
struct CpktCheckContext {
  CheckParameterEventData native;
  CpktCheckParameterValueData callback;
  CpktCheckParameterValue old_callback;
  CpktCMockaValueData data;
};
static int cpkt_check_callback(CMockaValueData value, CMockaValueData data) {
  const struct CpktCheckContext *context = data.const_ptr;
  if (context->callback)
    return context->callback(cpkt_value_public(value), context->data);
  return context->old_callback((unsigned long)value.uint_val,
                               context->data.uint_val);
}
/** Queue a typed C89 callback through the native event/diagnostic machinery. */
void cpkt_cmocka_expect_check_data(const char *function, const char *parameter,
                                   const char *file, int line,
                                   CpktCheckParameterValueData callback,
                                   CpktCMockaValueData data,
                                   CpktCheckParameterEventData *event,
                                   int count) {
  struct CpktCheckContext *context = malloc(sizeof(*context));
  CMockaValueData native = {0};
  if (!context)
    _fail(file, line);
  context->callback = callback;
  context->old_callback = NULL;
  context->data = data;
  /* A supplied facade event transfers heap ownership, as the native API does.
   */
  free(event);
  native.ptr = context;
  _expect_check_data(function, parameter, file, line, cpkt_check_callback,
                     native, &context->native, count);
}
/** Queue a legacy scalar callback without an incompatible function-pointer
 * cast. */
void cpkt_cmocka_expect_check(const char *function, const char *parameter,
                              const char *file, int line,
                              CpktCheckParameterValue callback,
                              unsigned long data,
                              CpktCheckParameterEvent *event, int count) {
  struct CpktCheckContext *context = malloc(sizeof(*context));
  CMockaValueData native = {0};
  if (!context)
    _fail(file, line);
  context->callback = NULL;
  context->old_callback = callback;
  context->data = cpkt_cmocka_value_uint(data);
  free(event);
  native.ptr = context;
  _expect_check_data(function, parameter, file, line, cpkt_check_callback,
                     native, &context->native, count);
}
/** Configure native diagnostic output through the fixed-width private boundary.
 */
void cpkt_cmocka_set_message_output(unsigned long output) {
  cmocka_set_message_output((uint32_t)output);
}
#include <cpkt/cmocka_bridge.inc>
