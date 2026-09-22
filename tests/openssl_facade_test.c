#include <cpkt/openssl.h>

int main(void) {
  ASN1_ENUMERATED *enumerated;
  ASN1_INTEGER *integer;
  CT_POLICY_EVAL_CTX *policy_context;
  OSSL_LIB_CTX *library_context;
  SSL_CTX *context;
  SCT *sct;
  X509_ALGOR *algorithm;
  unsigned char derived_key[16];
  unsigned char iv[16];
  unsigned char salt[4];
  cpkt_openssl_i64 signed_value;
  cpkt_openssl_u64 options;
  cpkt_openssl_u64 returned;

  options = cpkt_openssl_u64_make(0UL, 0x4000UL);
  if (cpkt_openssl_u64_high_word(options) != 0UL ||
      cpkt_openssl_u64_low_word(options) != 0x4000UL ||
      cpkt_openssl_u64_is_zero(options) ||
      !cpkt_openssl_u64_equal(options, cpkt_openssl_u64_make(0UL, 0x4000UL))) {
    return 1;
  }
  if (!cpkt_openssl_u64_is_zero(cpkt_openssl_u64_make(0UL, 0UL))) {
    return 2;
  }
  signed_value = cpkt_openssl_i64_make(0xffffffffUL, 0xfffffffeUL);
  if (cpkt_openssl_i64_high_word(signed_value) != 0xffffffffUL ||
      cpkt_openssl_i64_low_word(signed_value) != 0xfffffffeUL ||
      !cpkt_openssl_i64_equal(
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
  policy_context = CT_POLICY_EVAL_CTX_new();
  sct = SCT_new();
  library_context = OSSL_LIB_CTX_new();
  if (policy_context == 0 || sct == 0 || library_context == 0) {
    CT_POLICY_EVAL_CTX_free(policy_context);
    SCT_free(sct);
    OSSL_LIB_CTX_free(library_context);
    return 6;
  }
  cpkt_openssl_CT_POLICY_EVAL_CTX_set_time(policy_context, options);
  cpkt_openssl_SCT_set_timestamp(sct, options);
  if (!cpkt_openssl_u64_equal(
          cpkt_openssl_CT_POLICY_EVAL_CTX_get_time(policy_context), options) ||
      !cpkt_openssl_u64_equal(cpkt_openssl_SCT_get_timestamp(sct), options) ||
      cpkt_openssl_OSSL_set_max_threads(
          library_context, cpkt_openssl_u64_make(0UL, 1UL)) != 1 ||
      cpkt_openssl_u64_is_zero(
          cpkt_openssl_OSSL_get_max_threads(library_context))) {
    CT_POLICY_EVAL_CTX_free(policy_context);
    SCT_free(sct);
    OSSL_LIB_CTX_free(library_context);
    return 7;
  }
  cpkt_openssl_OSSL_sleep(cpkt_openssl_u64_make(0UL, 0UL));
  CT_POLICY_EVAL_CTX_free(policy_context);
  SCT_free(sct);
  OSSL_LIB_CTX_free(library_context);
  salt[0] = 1U;
  salt[1] = 2U;
  salt[2] = 3U;
  salt[3] = 4U;
  if (cpkt_openssl_EVP_PBE_scrypt(
          "password", 8U, salt, sizeof(salt), cpkt_openssl_u64_make(0UL, 16UL),
          cpkt_openssl_u64_make(0UL, 1UL), cpkt_openssl_u64_make(0UL, 1UL),
          cpkt_openssl_u64_make(0UL, 1048576UL), derived_key,
          sizeof(derived_key)) != 1) {
    return 8;
  }
  algorithm = cpkt_openssl_PKCS5_pbe2_set_scrypt(
      EVP_aes_128_cbc(), salt, (int) sizeof(salt), iv,
      cpkt_openssl_u64_make(0UL, 16UL), cpkt_openssl_u64_make(0UL, 1UL),
      cpkt_openssl_u64_make(0UL, 1UL));
  if (algorithm == 0) {
    return 9;
  }
  X509_ALGOR_free(algorithm);
  if (cpkt_openssl_OPENSSL_init_ssl(cpkt_openssl_u64_make(0UL, 0UL), 0) != 1) {
    return 10;
  }
  context = SSL_CTX_new(TLS_method());
  if (context == 0) {
    return 11;
  }
  returned = cpkt_openssl_SSL_CTX_set_options(context, options);
  if ((cpkt_openssl_u64_low_word(returned) & 0x4000UL) == 0UL) {
    SSL_CTX_free(context);
    return 12;
  }
  returned = cpkt_openssl_SSL_CTX_clear_options(context, options);
  SSL_CTX_free(context);
  return (cpkt_openssl_u64_low_word(returned) & 0x4000UL) == 0UL ? 0 : 13;
}
