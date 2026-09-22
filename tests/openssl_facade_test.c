#include <cpkt/openssl.h>

int main(void) {
  ASN1_ENUMERATED *enumerated;
  ASN1_INTEGER *integer;
  CT_POLICY_EVAL_CTX *policy_context;
  BIO *bio;
  OSSL_LIB_CTX *library_context;
  OSSL_PARAM *built_parameters;
  OSSL_PARAM *native_parameter;
  OSSL_PARAM_BLD *parameter_builder;
  SSL_CTX *context;
  SCT *sct;
  X509_ALGOR *algorithm;
  unsigned char derived_key[16];
  unsigned char iv[16];
  unsigned char salt[4];
  char read_buffer[4];
  cpkt_openssl_i64 signed_value;
  cpkt_openssl_param_i64 *signed_parameter;
  cpkt_openssl_param_u64 *unsigned_parameter;
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
  bio = BIO_new(BIO_s_mem());
  if (bio == 0 || BIO_write(bio, "abc", 3) != 3 ||
      cpkt_openssl_u64_low_word(cpkt_openssl_BIO_number_written(bio)) != 3UL ||
      BIO_read(bio, read_buffer, 3) != 3 ||
      cpkt_openssl_u64_low_word(cpkt_openssl_BIO_number_read(bio)) != 3UL) {
    BIO_free(bio);
    return 3;
  }
  BIO_free(bio);
  signed_value = cpkt_openssl_i64_make(0xffffffffUL, 0xfffffffeUL);
  if (cpkt_openssl_i64_high_word(signed_value) != 0xffffffffUL ||
      cpkt_openssl_i64_low_word(signed_value) != 0xfffffffeUL ||
      !cpkt_openssl_i64_equal(
          signed_value, cpkt_openssl_i64_make(0xffffffffUL, 0xfffffffeUL))) {
    return 4;
  }
  signed_parameter = 0;
  unsigned_parameter = 0;
  if (cpkt_openssl_OSSL_PARAM_construct_int64(
          &signed_parameter, "signed", signed_value) != 1 ||
      cpkt_openssl_OSSL_PARAM_construct_uint64(
          &unsigned_parameter, "unsigned", options) != 1 ||
      !cpkt_openssl_i64_equal(
          cpkt_openssl_param_i64_value(signed_parameter), signed_value) ||
      !cpkt_openssl_u64_equal(
          cpkt_openssl_param_u64_value(unsigned_parameter), options)) {
    cpkt_openssl_param_i64_free(signed_parameter);
    cpkt_openssl_param_u64_free(unsigned_parameter);
    return 5;
  }
  native_parameter = cpkt_openssl_param_i64_native(signed_parameter);
  if (cpkt_openssl_OSSL_PARAM_set_int64(
          native_parameter, cpkt_openssl_i64_make(0UL, 7UL)) != 1 ||
      cpkt_openssl_OSSL_PARAM_get_int64(&native_parameter[0], &signed_value) != 1 ||
      !cpkt_openssl_i64_equal(signed_value, cpkt_openssl_i64_make(0UL, 7UL))) {
    cpkt_openssl_param_i64_free(signed_parameter);
    cpkt_openssl_param_u64_free(unsigned_parameter);
    return 6;
  }
  native_parameter = cpkt_openssl_param_u64_native(unsigned_parameter);
  if (cpkt_openssl_OSSL_PARAM_set_uint64(
          native_parameter, cpkt_openssl_u64_make(0UL, 9UL)) != 1 ||
      cpkt_openssl_OSSL_PARAM_get_uint64(&native_parameter[0], &returned) != 1 ||
      !cpkt_openssl_u64_equal(returned, cpkt_openssl_u64_make(0UL, 9UL))) {
    cpkt_openssl_param_i64_free(signed_parameter);
    cpkt_openssl_param_u64_free(unsigned_parameter);
    return 7;
  }
  parameter_builder = OSSL_PARAM_BLD_new();
  if (parameter_builder == 0 ||
      cpkt_openssl_OSSL_PARAM_BLD_push_int64(
          parameter_builder, "signed", cpkt_openssl_i64_make(0UL, 11UL)) != 1 ||
      cpkt_openssl_OSSL_PARAM_BLD_push_uint64(
          parameter_builder, "unsigned", cpkt_openssl_u64_make(0UL, 12UL)) != 1) {
    OSSL_PARAM_BLD_free(parameter_builder);
    cpkt_openssl_param_i64_free(signed_parameter);
    cpkt_openssl_param_u64_free(unsigned_parameter);
    return 8;
  }
  built_parameters = OSSL_PARAM_BLD_to_param(parameter_builder);
  OSSL_PARAM_BLD_free(parameter_builder);
  if (built_parameters == 0 ||
      cpkt_openssl_OSSL_PARAM_get_int64(
          OSSL_PARAM_locate(built_parameters, "signed"), &signed_value) != 1 ||
      cpkt_openssl_OSSL_PARAM_get_uint64(
          OSSL_PARAM_locate(built_parameters, "unsigned"), &returned) != 1 ||
      !cpkt_openssl_i64_equal(signed_value, cpkt_openssl_i64_make(0UL, 11UL)) ||
      !cpkt_openssl_u64_equal(returned, cpkt_openssl_u64_make(0UL, 12UL))) {
    OSSL_PARAM_free(built_parameters);
    cpkt_openssl_param_i64_free(signed_parameter);
    cpkt_openssl_param_u64_free(unsigned_parameter);
    return 9;
  }
  OSSL_PARAM_free(built_parameters);
  cpkt_openssl_param_i64_free(signed_parameter);
  cpkt_openssl_param_u64_free(unsigned_parameter);
  signed_value = cpkt_openssl_i64_make(0xffffffffUL, 0xfffffffeUL);
  integer = ASN1_INTEGER_new();
  enumerated = ASN1_ENUMERATED_new();
  if (integer == 0 || enumerated == 0) {
    ASN1_INTEGER_free(integer);
    ASN1_ENUMERATED_free(enumerated);
    return 10;
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
    return 11;
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
    return 12;
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
    return 13;
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
    return 14;
  }
  algorithm = cpkt_openssl_PKCS5_pbe2_set_scrypt(
      EVP_aes_128_cbc(), salt, (int) sizeof(salt), iv,
      cpkt_openssl_u64_make(0UL, 16UL), cpkt_openssl_u64_make(0UL, 1UL),
      cpkt_openssl_u64_make(0UL, 1UL));
  if (algorithm == 0) {
    return 15;
  }
  X509_ALGOR_free(algorithm);
  if (cpkt_openssl_OPENSSL_init_ssl(cpkt_openssl_u64_make(0UL, 0UL), 0) != 1) {
    return 16;
  }
  context = SSL_CTX_new(TLS_method());
  if (context == 0) {
    return 17;
  }
  returned = cpkt_openssl_SSL_CTX_set_options(context, options);
  if ((cpkt_openssl_u64_low_word(returned) & 0x4000UL) == 0UL) {
    SSL_CTX_free(context);
    return 18;
  }
  returned = cpkt_openssl_SSL_CTX_clear_options(context, options);
  SSL_CTX_free(context);
  return (cpkt_openssl_u64_low_word(returned) & 0x4000UL) == 0UL ? 0 : 19;
}
