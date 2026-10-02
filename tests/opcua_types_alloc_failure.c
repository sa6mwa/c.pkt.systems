#include <pthread.h>
#include <stddef.h>

void *__real_calloc(size_t count, size_t size);
void *__real_malloc(size_t size);
void *__real_realloc(void *ptr, size_t size);
static size_t countdown;
static int enabled;
static int failed;
static pthread_t owner;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
void cpkt_types_fail_after(size_t count) {
  pthread_mutex_lock(&lock);
  owner = pthread_self();
  countdown = count;
  enabled = 1;
  failed = 0;
  pthread_mutex_unlock(&lock);
}
int cpkt_types_fail_stop(void) {
  int result;
  pthread_mutex_lock(&lock);
  enabled = 0;
  result = failed;
  pthread_mutex_unlock(&lock);
  return result;
}
static int fail_now(void) {
  int result = 0;
  pthread_mutex_lock(&lock);
  /* Native peers run on other threads. Their allocations must neither consume
   * nor race the caller's deterministic conversion failure position. */
  if (!enabled || !pthread_equal(owner, pthread_self())) {
    pthread_mutex_unlock(&lock);
    return 0;
  }
  if (countdown)
    --countdown;
  else {
    enabled = 0;
    failed = result = 1;
  }
  pthread_mutex_unlock(&lock);
  return result;
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
