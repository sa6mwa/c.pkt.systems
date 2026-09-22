#include <cpkt/openssl.h>

#include <stdint.h>
#include <string.h>

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

static int64_t cpkt_openssl_native_i64(cpkt_openssl_i64 value) {
  uint64_t bits;
  int64_t native;

  bits = cpkt_openssl_native_u64(
      cpkt_openssl_u64_make(value.high, value.low));
  memcpy(&native, &bits, sizeof(native));
  return native;
}

static cpkt_openssl_i64 cpkt_openssl_public_i64(int64_t value) {
  uint64_t bits;
  cpkt_openssl_u64 public_bits;
  cpkt_openssl_i64 public_value;

  memcpy(&bits, &value, sizeof(bits));
  public_bits = cpkt_openssl_public_u64(bits);
  public_value.high = public_bits.high;
  public_value.low = public_bits.low;
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

/** Implements the documented public C89 OpenSSL facade operation cpkt_openssl_i64_make. */
cpkt_openssl_i64 cpkt_openssl_i64_make(unsigned long high, unsigned long low) {
  cpkt_openssl_i64 value;

  value.high = high & 0xffffffffUL;
  value.low = low & 0xffffffffUL;
  return value;
}

/** Implements the documented public C89 OpenSSL facade operation cpkt_openssl_i64_equal. */
int cpkt_openssl_i64_equal(cpkt_openssl_i64 left, cpkt_openssl_i64 right) {
  return left.high == right.high && left.low == right.low;
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

/** Implements the documented public C89 adapter cpkt_openssl_ASN1_ENUMERATED_get_int64. */
int cpkt_openssl_ASN1_ENUMERATED_get_int64(
    cpkt_openssl_i64 *value_out, const ASN1_ENUMERATED *enumerated) {
  int64_t native_value;
  int result;

  native_value = 0;
  result = ASN1_ENUMERATED_get_int64(
      value_out == NULL ? NULL : &native_value, enumerated);
  if (result != 0 && value_out != NULL) {
    *value_out = cpkt_openssl_public_i64(native_value);
  }
  return result;
}

/** Implements the documented public C89 adapter cpkt_openssl_ASN1_ENUMERATED_set_int64. */
int cpkt_openssl_ASN1_ENUMERATED_set_int64(
    ASN1_ENUMERATED *enumerated, cpkt_openssl_i64 value) {
  return ASN1_ENUMERATED_set_int64(
      enumerated, cpkt_openssl_native_i64(value));
}

/** Implements the documented public C89 adapter cpkt_openssl_ASN1_INTEGER_get_int64. */
int cpkt_openssl_ASN1_INTEGER_get_int64(
    cpkt_openssl_i64 *value_out, const ASN1_INTEGER *integer) {
  int64_t native_value;
  int result;

  native_value = 0;
  result = ASN1_INTEGER_get_int64(value_out == NULL ? NULL : &native_value,
                                  integer);
  if (result != 0 && value_out != NULL) {
    *value_out = cpkt_openssl_public_i64(native_value);
  }
  return result;
}

/** Implements the documented public C89 adapter cpkt_openssl_ASN1_INTEGER_get_uint64. */
int cpkt_openssl_ASN1_INTEGER_get_uint64(
    cpkt_openssl_u64 *value_out, const ASN1_INTEGER *integer) {
  uint64_t native_value;
  int result;

  native_value = 0;
  result = ASN1_INTEGER_get_uint64(value_out == NULL ? NULL : &native_value,
                                   integer);
  if (result != 0 && value_out != NULL) {
    *value_out = cpkt_openssl_public_u64(native_value);
  }
  return result;
}

/** Implements the documented public C89 adapter cpkt_openssl_ASN1_INTEGER_set_int64. */
int cpkt_openssl_ASN1_INTEGER_set_int64(
    ASN1_INTEGER *integer, cpkt_openssl_i64 value) {
  return ASN1_INTEGER_set_int64(integer, cpkt_openssl_native_i64(value));
}

/** Implements the documented public C89 adapter cpkt_openssl_ASN1_INTEGER_set_uint64. */
int cpkt_openssl_ASN1_INTEGER_set_uint64(
    ASN1_INTEGER *integer, cpkt_openssl_u64 value) {
  return ASN1_INTEGER_set_uint64(integer, cpkt_openssl_native_u64(value));
}

/** Implements the documented public C89 adapter cpkt_openssl_CT_POLICY_EVAL_CTX_get_time. */
cpkt_openssl_u64 cpkt_openssl_CT_POLICY_EVAL_CTX_get_time(
    const CT_POLICY_EVAL_CTX *context) {
  return cpkt_openssl_public_u64(CT_POLICY_EVAL_CTX_get_time(context));
}

/** Implements the documented public C89 adapter cpkt_openssl_CT_POLICY_EVAL_CTX_set_time. */
void cpkt_openssl_CT_POLICY_EVAL_CTX_set_time(
    CT_POLICY_EVAL_CTX *context, cpkt_openssl_u64 value) {
  CT_POLICY_EVAL_CTX_set_time(context, cpkt_openssl_native_u64(value));
}

/** Implements the documented public C89 adapter cpkt_openssl_OSSL_get_max_threads. */
cpkt_openssl_u64 cpkt_openssl_OSSL_get_max_threads(
    OSSL_LIB_CTX *library_context) {
  return cpkt_openssl_public_u64(OSSL_get_max_threads(library_context));
}

/** Implements the documented public C89 adapter cpkt_openssl_OSSL_set_max_threads. */
int cpkt_openssl_OSSL_set_max_threads(
    OSSL_LIB_CTX *library_context, cpkt_openssl_u64 value) {
  return OSSL_set_max_threads(
      library_context, cpkt_openssl_native_u64(value));
}

/** Implements the documented public C89 adapter cpkt_openssl_OSSL_sleep. */
void cpkt_openssl_OSSL_sleep(cpkt_openssl_u64 milliseconds) {
  OSSL_sleep(cpkt_openssl_native_u64(milliseconds));
}

/** Implements the documented public C89 adapter cpkt_openssl_SCT_get_timestamp. */
cpkt_openssl_u64 cpkt_openssl_SCT_get_timestamp(const SCT *sct) {
  return cpkt_openssl_public_u64(SCT_get_timestamp(sct));
}

/** Implements the documented public C89 adapter cpkt_openssl_SCT_new_from_base64. */
SCT *cpkt_openssl_SCT_new_from_base64(
    unsigned char version, const char *log_id, ct_log_entry_type_t entry_type,
    cpkt_openssl_u64 timestamp, const char *extensions, const char *signature) {
  return SCT_new_from_base64(version, log_id, entry_type,
                             cpkt_openssl_native_u64(timestamp), extensions,
                             signature);
}

/** Implements the documented public C89 adapter cpkt_openssl_SCT_set_timestamp. */
void cpkt_openssl_SCT_set_timestamp(SCT *sct, cpkt_openssl_u64 timestamp) {
  SCT_set_timestamp(sct, cpkt_openssl_native_u64(timestamp));
}
