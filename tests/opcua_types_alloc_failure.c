#include <stddef.h>

void *__real_calloc(size_t count, size_t size);
void *__real_malloc(size_t size);
void *__real_realloc(void *ptr, size_t size);
static size_t countdown;
static int enabled;
static int failed;
void cpkt_types_fail_after(size_t count) {
  countdown = count;
  enabled = 1;
  failed = 0;
}
int cpkt_types_fail_stop(void) {
  enabled = 0;
  return failed;
}
static int fail_now(void) {
  if (!enabled)
    return 0;
  if (countdown) {
    --countdown;
    return 0;
  }
  enabled = 0;
  failed = 1;
  return 1;
}
void *__wrap_calloc(size_t count, size_t size) {
  return fail_now() ? NULL : __real_calloc(count, size);
}
void *__wrap_malloc(size_t size) {
  return fail_now() ? NULL : __real_malloc(size);
}
void *__wrap_realloc(void *ptr, size_t size) {
  return fail_now() ? NULL : __real_realloc(ptr, size);
}
