#include <cpkt/openssl.h>

#include <limits.h>
#include <stdint.h>
#include <string.h>

typedef char cpkt_openssl_u64_is_eight_bytes[
    sizeof(uint64_t) == 8 ? 1 : -1];
typedef char cpkt_openssl_i64_is_eight_bytes[
    sizeof(int64_t) == 8 ? 1 : -1];
typedef char cpkt_openssl_octet_is_eight_bits[
    CHAR_BIT == 8 ? 1 : -1];
typedef char cpkt_openssl_public_u64_is_eight_bytes[
    sizeof(cpkt_openssl_u64) == 8 ? 1 : -1];
typedef char cpkt_openssl_public_i64_is_eight_bytes[
    sizeof(cpkt_openssl_i64) == 8 ? 1 : -1];

static uint64_t cpkt_openssl_native_u64(cpkt_openssl_u64 value) {
  uint64_t native;

  memcpy(&native, value.bytes, sizeof(native));
  return native;
}

static cpkt_openssl_u64 cpkt_openssl_public_u64(uint64_t value) {
  cpkt_openssl_u64 public_value;

  memcpy(public_value.bytes, &value, sizeof(value));
  return public_value;
}

static int64_t cpkt_openssl_native_i64(cpkt_openssl_i64 value) {
  uint64_t bits;
  int64_t native;

  memcpy(&bits, value.bytes, sizeof(bits));
  memcpy(&native, &bits, sizeof(native));
  return native;
}

static cpkt_openssl_i64 cpkt_openssl_public_i64(int64_t value) {
  uint64_t bits;
  cpkt_openssl_u64 public_bits;
  cpkt_openssl_i64 public_value;

  memcpy(&bits, &value, sizeof(bits));
  public_bits = cpkt_openssl_public_u64(bits);
  memcpy(public_value.bytes, public_bits.bytes, sizeof(public_value.bytes));
  return public_value;
}

/** Implements the documented public C89 OpenSSL facade operation cpkt_openssl_u64_make. */
cpkt_openssl_u64 cpkt_openssl_u64_make(unsigned long high, unsigned long low) {
  uint64_t native;

  native = (uint64_t) (high & 0xffffffffUL);
  native <<= 32;
  native |= (uint64_t) (low & 0xffffffffUL);
  return cpkt_openssl_public_u64(native);
}

/** Implements the documented public C89 OpenSSL facade operation cpkt_openssl_u64_high_word. */
unsigned long cpkt_openssl_u64_high_word(cpkt_openssl_u64 value) {
  return (unsigned long) (cpkt_openssl_native_u64(value) >> 32);
}

/** Implements the documented public C89 OpenSSL facade operation cpkt_openssl_u64_low_word. */
unsigned long cpkt_openssl_u64_low_word(cpkt_openssl_u64 value) {
  return (unsigned long) (cpkt_openssl_native_u64(value) & 0xffffffffUL);
}

/** Implements the documented public C89 OpenSSL facade operation cpkt_openssl_u64_equal. */
int cpkt_openssl_u64_equal(cpkt_openssl_u64 left, cpkt_openssl_u64 right) {
  return memcmp(left.bytes, right.bytes, sizeof(left.bytes)) == 0;
}

/** Implements the documented public C89 OpenSSL facade operation cpkt_openssl_u64_is_zero. */
int cpkt_openssl_u64_is_zero(cpkt_openssl_u64 value) {
  return cpkt_openssl_u64_high_word(value) == 0UL &&
         cpkt_openssl_u64_low_word(value) == 0UL;
}

/** Implements the documented public C89 OpenSSL facade operation cpkt_openssl_i64_make. */
cpkt_openssl_i64 cpkt_openssl_i64_make(unsigned long high, unsigned long low) {
  cpkt_openssl_u64 unsigned_value;
  cpkt_openssl_i64 value;

  unsigned_value = cpkt_openssl_u64_make(high, low);
  memcpy(value.bytes, unsigned_value.bytes, sizeof(value.bytes));
  return value;
}

/** Implements the documented public C89 OpenSSL facade operation cpkt_openssl_i64_high_word. */
unsigned long cpkt_openssl_i64_high_word(cpkt_openssl_i64 value) {
  cpkt_openssl_u64 unsigned_value;

  memcpy(unsigned_value.bytes, value.bytes, sizeof(unsigned_value.bytes));
  return cpkt_openssl_u64_high_word(unsigned_value);
}

/** Implements the documented public C89 OpenSSL facade operation cpkt_openssl_i64_low_word. */
unsigned long cpkt_openssl_i64_low_word(cpkt_openssl_i64 value) {
  cpkt_openssl_u64 unsigned_value;

  memcpy(unsigned_value.bytes, value.bytes, sizeof(unsigned_value.bytes));
  return cpkt_openssl_u64_low_word(unsigned_value);
}

/** Implements the documented public C89 OpenSSL facade operation cpkt_openssl_i64_equal. */
int cpkt_openssl_i64_equal(cpkt_openssl_i64 left, cpkt_openssl_i64 right) {
  return memcmp(left.bytes, right.bytes, sizeof(left.bytes)) == 0;
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

/** Implements the documented public C89 adapter cpkt_openssl_EVP_PBE_scrypt. */
int cpkt_openssl_EVP_PBE_scrypt(
    const char *password, size_t password_length, const unsigned char *salt,
    size_t salt_length, cpkt_openssl_u64 work_factor,
    cpkt_openssl_u64 block_size, cpkt_openssl_u64 parallelization,
    cpkt_openssl_u64 maximum_memory, unsigned char *key, size_t key_length) {
  return EVP_PBE_scrypt(password, password_length, salt, salt_length,
                        cpkt_openssl_native_u64(work_factor),
                        cpkt_openssl_native_u64(block_size),
                        cpkt_openssl_native_u64(parallelization),
                        cpkt_openssl_native_u64(maximum_memory), key,
                        key_length);
}

/** Implements the documented public C89 adapter cpkt_openssl_EVP_PBE_scrypt_ex. */
int cpkt_openssl_EVP_PBE_scrypt_ex(
    const char *password, size_t password_length, const unsigned char *salt,
    size_t salt_length, cpkt_openssl_u64 work_factor,
    cpkt_openssl_u64 block_size, cpkt_openssl_u64 parallelization,
    cpkt_openssl_u64 maximum_memory, unsigned char *key, size_t key_length,
    OSSL_LIB_CTX *library_context, const char *property_query) {
  return EVP_PBE_scrypt_ex(password, password_length, salt, salt_length,
                           cpkt_openssl_native_u64(work_factor),
                           cpkt_openssl_native_u64(block_size),
                           cpkt_openssl_native_u64(parallelization),
                           cpkt_openssl_native_u64(maximum_memory), key,
                           key_length, library_context, property_query);
}

/** Implements the documented public C89 adapter cpkt_openssl_EVP_PKEY_CTX_ctrl_uint64. */
int cpkt_openssl_EVP_PKEY_CTX_ctrl_uint64(
    EVP_PKEY_CTX *context, int key_type, int operation, int command,
    cpkt_openssl_u64 value) {
  return EVP_PKEY_CTX_ctrl_uint64(context, key_type, operation, command,
                                  cpkt_openssl_native_u64(value));
}

/** Implements the documented public C89 adapter cpkt_openssl_EVP_PKEY_CTX_set_scrypt_N. */
int cpkt_openssl_EVP_PKEY_CTX_set_scrypt_N(
    EVP_PKEY_CTX *context, cpkt_openssl_u64 work_factor) {
  return EVP_PKEY_CTX_set_scrypt_N(
      context, cpkt_openssl_native_u64(work_factor));
}

/** Implements the documented public C89 adapter cpkt_openssl_EVP_PKEY_CTX_set_scrypt_maxmem_bytes. */
int cpkt_openssl_EVP_PKEY_CTX_set_scrypt_maxmem_bytes(
    EVP_PKEY_CTX *context, cpkt_openssl_u64 maximum_memory) {
  return EVP_PKEY_CTX_set_scrypt_maxmem_bytes(
      context, cpkt_openssl_native_u64(maximum_memory));
}

/** Implements the documented public C89 adapter cpkt_openssl_EVP_PKEY_CTX_set_scrypt_p. */
int cpkt_openssl_EVP_PKEY_CTX_set_scrypt_p(
    EVP_PKEY_CTX *context, cpkt_openssl_u64 parallelization) {
  return EVP_PKEY_CTX_set_scrypt_p(
      context, cpkt_openssl_native_u64(parallelization));
}

/** Implements the documented public C89 adapter cpkt_openssl_EVP_PKEY_CTX_set_scrypt_r. */
int cpkt_openssl_EVP_PKEY_CTX_set_scrypt_r(
    EVP_PKEY_CTX *context, cpkt_openssl_u64 block_size) {
  return EVP_PKEY_CTX_set_scrypt_r(
      context, cpkt_openssl_native_u64(block_size));
}

/** Implements the documented public C89 adapter cpkt_openssl_PKCS5_pbe2_set_scrypt. */
X509_ALGOR *cpkt_openssl_PKCS5_pbe2_set_scrypt(
    const EVP_CIPHER *cipher, const unsigned char *salt, int salt_length,
    unsigned char *iv, cpkt_openssl_u64 work_factor,
    cpkt_openssl_u64 block_size, cpkt_openssl_u64 parallelization) {
  return PKCS5_pbe2_set_scrypt(cipher, salt, salt_length, iv,
                               cpkt_openssl_native_u64(work_factor),
                               cpkt_openssl_native_u64(block_size),
                               cpkt_openssl_native_u64(parallelization));
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_CTX_get_domain_flags. */
int cpkt_openssl_SSL_CTX_get_domain_flags(
    const SSL_CTX *context, cpkt_openssl_u64 *flags_out) {
  uint64_t native_flags;
  int result;

  native_flags = 0;
  result = SSL_CTX_get_domain_flags(
      context, flags_out == NULL ? NULL : &native_flags);
  if (result != 0 && flags_out != NULL) {
    *flags_out = cpkt_openssl_public_u64(native_flags);
  }
  return result;
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_CTX_set_domain_flags. */
int cpkt_openssl_SSL_CTX_set_domain_flags(
    SSL_CTX *context, cpkt_openssl_u64 flags) {
  return SSL_CTX_set_domain_flags(context, cpkt_openssl_native_u64(flags));
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_get_domain_flags. */
int cpkt_openssl_SSL_get_domain_flags(
    const SSL *ssl, cpkt_openssl_u64 *flags_out) {
  uint64_t native_flags;
  int result;

  native_flags = 0;
  result = SSL_get_domain_flags(ssl, flags_out == NULL ? NULL : &native_flags);
  if (result != 0 && flags_out != NULL) {
    *flags_out = cpkt_openssl_public_u64(native_flags);
  }
  return result;
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_accept_connection. */
SSL *cpkt_openssl_SSL_accept_connection(
    SSL *ssl, cpkt_openssl_u64 domain_flags) {
  return SSL_accept_connection(ssl, cpkt_openssl_native_u64(domain_flags));
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_accept_stream. */
SSL *cpkt_openssl_SSL_accept_stream(SSL *ssl, cpkt_openssl_u64 stream_id) {
  return SSL_accept_stream(ssl, cpkt_openssl_native_u64(stream_id));
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_get_stream_id. */
cpkt_openssl_u64 cpkt_openssl_SSL_get_stream_id(SSL *ssl) {
  return cpkt_openssl_public_u64(SSL_get_stream_id(ssl));
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_get_stream_read_error_code. */
int cpkt_openssl_SSL_get_stream_read_error_code(
    SSL *ssl, cpkt_openssl_u64 *error_code_out) {
  uint64_t native_error_code;
  int result;

  native_error_code = 0;
  result = SSL_get_stream_read_error_code(
      ssl, error_code_out == NULL ? NULL : &native_error_code);
  if (result != 0 && error_code_out != NULL) {
    *error_code_out = cpkt_openssl_public_u64(native_error_code);
  }
  return result;
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_get_stream_write_error_code. */
int cpkt_openssl_SSL_get_stream_write_error_code(
    SSL *ssl, cpkt_openssl_u64 *error_code_out) {
  uint64_t native_error_code;
  int result;

  native_error_code = 0;
  result = SSL_get_stream_write_error_code(
      ssl, error_code_out == NULL ? NULL : &native_error_code);
  if (result != 0 && error_code_out != NULL) {
    *error_code_out = cpkt_openssl_public_u64(native_error_code);
  }
  return result;
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_new_domain. */
SSL *cpkt_openssl_SSL_new_domain(
    SSL_CTX *context, cpkt_openssl_u64 domain_flags) {
  return SSL_new_domain(context, cpkt_openssl_native_u64(domain_flags));
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_new_from_listener. */
SSL *cpkt_openssl_SSL_new_from_listener(
    SSL *listener, cpkt_openssl_u64 stream_id) {
  return SSL_new_from_listener(listener, cpkt_openssl_native_u64(stream_id));
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_new_listener. */
SSL *cpkt_openssl_SSL_new_listener(
    SSL_CTX *context, cpkt_openssl_u64 domain_flags) {
  return SSL_new_listener(context, cpkt_openssl_native_u64(domain_flags));
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_new_listener_from. */
SSL *cpkt_openssl_SSL_new_listener_from(
    SSL *ssl, cpkt_openssl_u64 domain_flags) {
  return SSL_new_listener_from(ssl, cpkt_openssl_native_u64(domain_flags));
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_new_stream. */
SSL *cpkt_openssl_SSL_new_stream(SSL *ssl, cpkt_openssl_u64 stream_id) {
  return SSL_new_stream(ssl, cpkt_openssl_native_u64(stream_id));
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_set_incoming_stream_policy. */
int cpkt_openssl_SSL_set_incoming_stream_policy(
    SSL *ssl, int policy, cpkt_openssl_u64 application_error_code) {
  return SSL_set_incoming_stream_policy(
      ssl, policy, cpkt_openssl_native_u64(application_error_code));
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_stream_conclude. */
int cpkt_openssl_SSL_stream_conclude(
    SSL *ssl, cpkt_openssl_u64 application_error_code) {
  return SSL_stream_conclude(
      ssl, cpkt_openssl_native_u64(application_error_code));
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_write_ex2. */
int cpkt_openssl_SSL_write_ex2(
    SSL *ssl, const void *buffer, size_t buffer_length,
    cpkt_openssl_u64 flags, size_t *written_out) {
  return SSL_write_ex2(ssl, buffer, buffer_length,
                       cpkt_openssl_native_u64(flags), written_out);
}
