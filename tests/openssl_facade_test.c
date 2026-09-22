#include <cpkt/openssl.h>

int main(void) {
  SSL_CTX *context;
  cpkt_openssl_u64 options;
  cpkt_openssl_u64 returned;

  options = cpkt_openssl_u64_make(0UL, 0x4000UL);
  if (options.high != 0UL || options.low != 0x4000UL ||
      cpkt_openssl_u64_is_zero(options) ||
      !cpkt_openssl_u64_equal(options, cpkt_openssl_u64_make(0UL, 0x4000UL))) {
    return 1;
  }
  if (!cpkt_openssl_u64_is_zero(cpkt_openssl_u64_make(0UL, 0UL))) {
    return 2;
  }
  if (cpkt_openssl_OPENSSL_init_ssl(cpkt_openssl_u64_make(0UL, 0UL), 0) != 1) {
    return 3;
  }
  context = SSL_CTX_new(TLS_method());
  if (context == 0) {
    return 4;
  }
  returned = cpkt_openssl_SSL_CTX_set_options(context, options);
  if ((returned.low & 0x4000UL) == 0UL) {
    SSL_CTX_free(context);
    return 5;
  }
  returned = cpkt_openssl_SSL_CTX_clear_options(context, options);
  SSL_CTX_free(context);
  return (returned.low & 0x4000UL) == 0UL ? 0 : 6;
}
