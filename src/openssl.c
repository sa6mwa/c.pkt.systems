#include <cpkt/openssl.h>

#include <stdint.h>

static uint64_t cpkt_openssl_native_u64(cpkt_openssl_u64 value) {
  uint64_t native;

  native = (uint64_t) (value.high & 0xffffffffUL);
  native <<= 32;
  native |= (uint64_t) (value.low & 0xffffffffUL);
  return native;
}

static cpkt_openssl_u64 cpkt_openssl_public_u64(uint64_t value) {
  cpkt_openssl_u64 public_value;

  public_value.high = (unsigned long) (value >> 32);
  public_value.low = (unsigned long) (value & 0xffffffffUL);
  return public_value;
}

/** Implements the documented public C89 OpenSSL facade operation cpkt_openssl_u64_make. */
cpkt_openssl_u64 cpkt_openssl_u64_make(unsigned long high, unsigned long low) {
  cpkt_openssl_u64 value;

  value.high = high & 0xffffffffUL;
  value.low = low & 0xffffffffUL;
  return value;
}

/** Implements the documented public C89 OpenSSL facade operation cpkt_openssl_u64_equal. */
int cpkt_openssl_u64_equal(cpkt_openssl_u64 left, cpkt_openssl_u64 right) {
  return left.high == right.high && left.low == right.low;
}

/** Implements the documented public C89 OpenSSL facade operation cpkt_openssl_u64_is_zero. */
int cpkt_openssl_u64_is_zero(cpkt_openssl_u64 value) {
  return value.high == 0UL && value.low == 0UL;
}

/** Implements the documented public C89 adapter cpkt_openssl_OPENSSL_init_crypto. */
int cpkt_openssl_OPENSSL_init_crypto(
    cpkt_openssl_u64 options, const OPENSSL_INIT_SETTINGS *settings) {
  return OPENSSL_init_crypto(cpkt_openssl_native_u64(options), settings);
}

/** Implements the documented public C89 adapter cpkt_openssl_OPENSSL_init_ssl. */
int cpkt_openssl_OPENSSL_init_ssl(
    cpkt_openssl_u64 options, const OPENSSL_INIT_SETTINGS *settings) {
  return OPENSSL_init_ssl(cpkt_openssl_native_u64(options), settings);
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_CTX_get_options. */
cpkt_openssl_u64 cpkt_openssl_SSL_CTX_get_options(const SSL_CTX *context) {
  return cpkt_openssl_public_u64(SSL_CTX_get_options(context));
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_get_options. */
cpkt_openssl_u64 cpkt_openssl_SSL_get_options(const SSL *ssl) {
  return cpkt_openssl_public_u64(SSL_get_options(ssl));
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_CTX_set_options. */
cpkt_openssl_u64 cpkt_openssl_SSL_CTX_set_options(
    SSL_CTX *context, cpkt_openssl_u64 options) {
  return cpkt_openssl_public_u64(
      SSL_CTX_set_options(context, cpkt_openssl_native_u64(options)));
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_set_options. */
cpkt_openssl_u64 cpkt_openssl_SSL_set_options(
    SSL *ssl, cpkt_openssl_u64 options) {
  return cpkt_openssl_public_u64(
      SSL_set_options(ssl, cpkt_openssl_native_u64(options)));
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_CTX_clear_options. */
cpkt_openssl_u64 cpkt_openssl_SSL_CTX_clear_options(
    SSL_CTX *context, cpkt_openssl_u64 options) {
  return cpkt_openssl_public_u64(
      SSL_CTX_clear_options(context, cpkt_openssl_native_u64(options)));
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_clear_options. */
cpkt_openssl_u64 cpkt_openssl_SSL_clear_options(
    SSL *ssl, cpkt_openssl_u64 options) {
  return cpkt_openssl_public_u64(
      SSL_clear_options(ssl, cpkt_openssl_native_u64(options)));
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_get_handshake_rtt. */
int cpkt_openssl_SSL_get_handshake_rtt(
    const SSL *ssl, cpkt_openssl_u64 *rtt_out) {
  uint64_t native_rtt;
  int result;

  native_rtt = 0;
  result = SSL_get_handshake_rtt(ssl, rtt_out == NULL ? NULL : &native_rtt);
  if (result != 0 && rtt_out != NULL) {
    *rtt_out = cpkt_openssl_public_u64(native_rtt);
  }
  return result;
}
