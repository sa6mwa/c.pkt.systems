#include <cpkt/libssh2.h>

#include <assert.h>
#include <stdlib.h>

static void *test_alloc(size_t count, void **abstract) {
  (void)abstract;
  return malloc(count);
}

static void test_free(void *pointer, void **abstract) {
  (void)abstract;
  free(pointer);
}

static void *test_realloc(void *pointer, size_t count, void **abstract) {
  (void)abstract;
  return realloc(pointer, count);
}

int main(void) {
  cpkt_libssh2_session *session;
  cpkt_libssh2_u64 offset;

  offset.high = 1UL;
  offset.low = 0UL;
  assert(cpkt_libssh2_init(0) == 0);
  session =
      cpkt_libssh2_session_init_ex(test_alloc, test_free, test_realloc, NULL);
  assert(session != NULL);
  assert(cpkt_libssh2_session_free(session) == 0);
  cpkt_libssh2_exit();
  return offset.high != 1UL || offset.low != 0UL;
}
