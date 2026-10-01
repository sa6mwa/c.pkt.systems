#include <cpkt/openssl.h>

#include <stdio.h>
#include <stdlib.h>

enum creation_mode { CREATE_SUCCESS, CREATE_REJECT, CREATE_EX_DATA_FAILURE };

static cpkt_openssl_bio_method *method;
static cpkt_openssl_bio *nested;
static OSSL_LIB_CTX *library_context;
static enum creation_mode mode;
static int creation_depth;
static int create_calls;
static int destroy_calls;
static int nested_on_destroy;
static int fail_next_allocation;
static int allocation_failures;

static void check(int condition, const char *message) {
  if (!condition) {
    fprintf(stderr, "BIO lifecycle: %s (mode=%d)\n", message, (int)mode);
    exit(EXIT_FAILURE);
  }
}

/* OpenSSL's public allocator hooks inject the first ex-data publication
 * allocation failure after native creation. Facade allocations remain real. */
static void *test_malloc(size_t size, const char *file, int line) {
  (void)file;
  (void)line;
  if (fail_next_allocation) {
    fail_next_allocation = 0;
    ++allocation_failures;
    return NULL;
  }
  return malloc(size);
}

static void *test_realloc(void *pointer, size_t size, const char *file,
                          int line) {
  (void)file;
  (void)line;
  if (fail_next_allocation) {
    fail_next_allocation = 0;
    ++allocation_failures;
    return NULL;
  }
  return realloc(pointer, size);
}

static void test_free(void *pointer, const char *file, int line) {
  (void)file;
  (void)line;
  free(pointer);
}

static int batch_callback(void *context, BIO *bio,
                          cpkt_openssl_bio_message *messages, size_t stride,
                          size_t count, cpkt_openssl_u64 flags,
                          size_t *processed) {
  (void)context;
  (void)bio;
  (void)messages;
  (void)stride;
  (void)flags;
  *processed = count;
  return 1;
}

static void check_method_access(void) {
  check(cpkt_openssl_BIO_meth_get_sendmmsg(method) == batch_callback,
        "send callback inaccessible during native callback");
  check(cpkt_openssl_BIO_meth_get_recvmmsg(method) == batch_callback,
        "receive callback inaccessible during native callback");
  check(cpkt_openssl_BIO_meth_set_sendmmsg(method, batch_callback) == 1,
        "send callback registration failed during native callback");
  check(cpkt_openssl_BIO_meth_set_recvmmsg(method, batch_callback) == 1,
        "receive callback registration failed during native callback");
  check(cpkt_openssl_BIO_meth_close(method) == 0,
        "method freed while native construction/destruction was active");
}

static int create_callback(BIO *bio) {
  ++create_calls;
  ++creation_depth;
  check_method_access();
  BIO_set_init(bio, 1);
  if (creation_depth == 1) {
    nested = cpkt_openssl_BIO_new_ex(library_context, method, NULL);
    check(nested != NULL, "nested creation failed");
    --creation_depth;
    if (mode == CREATE_REJECT)
      return 0;
    if (mode == CREATE_EX_DATA_FAILURE)
      fail_next_allocation = 1;
  } else {
    --creation_depth;
  }
  return 1;
}

static int destroy_callback(BIO *bio) {
  cpkt_openssl_bio *temporary;
  (void)bio;
  ++destroy_calls;
  check_method_access();
  if (nested_on_destroy) {
    nested_on_destroy = 0;
    /* Avoid creating another retained child from this nested constructor. */
    ++creation_depth;
    temporary = cpkt_openssl_BIO_new(method, NULL);
    --creation_depth;
    check(temporary != NULL, "nested creation during destruction failed");
    check(cpkt_openssl_BIO_close(temporary) == 1, "nested destruction failed");
  }
  return 1;
}

static void run_case(enum creation_mode selected_mode, int explicit_context) {
  cpkt_openssl_bio *outer;
  BIO *retained;
  BIO_METHOD *native_method;
  mode = selected_mode;
  create_calls = 0;
  destroy_calls = 0;
  creation_depth = 0;
  allocation_failures = 0;
  method = cpkt_openssl_BIO_meth_new(BIO_TYPE_NONE, "reentrant-create");
  check(method != NULL, "method allocation failed");
  native_method = (BIO_METHOD *)cpkt_openssl_BIO_meth_native(method);
  check(BIO_meth_set_create(native_method, create_callback) == 1 &&
            BIO_meth_set_destroy(native_method, destroy_callback) == 1 &&
            cpkt_openssl_BIO_meth_set_sendmmsg(method, batch_callback) == 1 &&
            cpkt_openssl_BIO_meth_set_recvmmsg(method, batch_callback) == 1,
        "method configuration failed");
  outer = explicit_context
              ? cpkt_openssl_BIO_new_ex(library_context, method, NULL)
              : cpkt_openssl_BIO_new(method, NULL);
  check(create_calls == 2, "unexpected native create count");
  check((outer != NULL) == (mode == CREATE_SUCCESS),
        "unexpected constructor result");
  check(allocation_failures == (mode == CREATE_EX_DATA_FAILURE),
        "ex-data allocation failure was not exercised");
  check(destroy_calls == (mode == CREATE_EX_DATA_FAILURE),
        "failed creation used the wrong native cleanup path");
  check(cpkt_openssl_BIO_meth_close(method) == 0,
        "failed constructor released the retained child's method pin");
  if (outer != NULL) {
    /* Retained native ownership must continue to pin the method after close. */
    retained = cpkt_openssl_BIO_native(outer);
    check(BIO_up_ref(retained) == 1, "native up-ref failed");
    check(cpkt_openssl_BIO_close(outer) == 1, "outer close failed");
    check(destroy_calls == 0, "outer close destroyed a retained native BIO");
    check(cpkt_openssl_BIO_close(nested) == 1, "child close failed");
    check(cpkt_openssl_BIO_meth_close(method) == 0,
          "retained native reference lost its method pin");
    nested_on_destroy = 1;
    check(BIO_free(retained) == 1, "final native close failed");
    check(create_calls == 3 && destroy_calls == 3,
          "reentrant native destruction was not exercised");
  } else {
    check(cpkt_openssl_BIO_close(nested) == 1, "child close failed");
  }
  check(cpkt_openssl_BIO_meth_close(method) == 1,
        "constructor failure or destruction leaked a method pin");
  ERR_clear_error();
}

int main(void) {
  int explicit_context;
  check(CRYPTO_set_mem_functions(test_malloc, test_realloc, test_free) == 1,
        "could not install allocator hooks");
  library_context = OSSL_LIB_CTX_new();
  check(library_context != NULL, "library context allocation failed");
  for (explicit_context = 0; explicit_context < 2; ++explicit_context) {
    run_case(CREATE_SUCCESS, explicit_context);
    run_case(CREATE_REJECT, explicit_context);
    run_case(CREATE_EX_DATA_FAILURE, explicit_context);
  }
  OSSL_LIB_CTX_free(library_context);
  return EXIT_SUCCESS;
}
