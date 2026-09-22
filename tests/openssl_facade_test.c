#include <cpkt/openssl.h>

int main(void) {
  ASN1_ENUMERATED *enumerated;
  ASN1_INTEGER *integer;
  SSL_CTX *context;
  cpkt_openssl_i64 signed_value;
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
  signed_value = cpkt_openssl_i64_make(0xffffffffUL, 0xfffffffeUL);
  if (!cpkt_openssl_i64_equal(
          signed_value, cpkt_openssl_i64_make(0xffffffffUL, 0xfffffffeUL))) {
    return 3;
  }
  integer = ASN1_INTEGER_new();
  enumerated = ASN1_ENUMERATED_new();
  if (integer == 0 || enumerated == 0) {
    ASN1_INTEGER_free(integer);
    ASN1_ENUMERATED_free(enumerated);
    return 4;
  }
  if (cpkt_openssl_ASN1_INTEGER_set_int64(integer, signed_value) != 1 ||
      cpkt_openssl_ASN1_INTEGER_get_int64(&signed_value, integer) != 1 ||
      !cpkt_openssl_i64_equal(
          signed_value, cpkt_openssl_i64_make(0xffffffffUL, 0xfffffffeUL)) ||
      cpkt_openssl_ASN1_INTEGER_set_uint64(integer, options) != 1 ||
      cpkt_openssl_ASN1_INTEGER_get_uint64(&returned, integer) != 1 ||
      !cpkt_openssl_u64_equal(returned, options) ||
      cpkt_openssl_ASN1_ENUMERATED_set_int64(enumerated, signed_value) != 1 ||
      cpkt_openssl_ASN1_ENUMERATED_get_int64(&signed_value, enumerated) != 1 ||
      !cpkt_openssl_i64_equal(
          signed_value, cpkt_openssl_i64_make(0xffffffffUL, 0xfffffffeUL))) {
    ASN1_INTEGER_free(integer);
    ASN1_ENUMERATED_free(enumerated);
    return 5;
  }
  ASN1_INTEGER_free(integer);
  ASN1_ENUMERATED_free(enumerated);
  if (cpkt_openssl_OPENSSL_init_ssl(cpkt_openssl_u64_make(0UL, 0UL), 0) != 1) {
    return 6;
  }
  context = SSL_CTX_new(TLS_method());
  if (context == 0) {
    return 7;
  }
  returned = cpkt_openssl_SSL_CTX_set_options(context, options);
  if ((returned.low & 0x4000UL) == 0UL) {
    SSL_CTX_free(context);
    return 8;
  }
  returned = cpkt_openssl_SSL_CTX_clear_options(context, options);
  SSL_CTX_free(context);
  return (returned.low & 0x4000UL) == 0UL ? 0 : 9;
}
