#include <cpkt/openssl.h>

#include <limits.h>
#include <stdlib.h>
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
typedef char cpkt_openssl_uintptr_fits_public_value[
    sizeof(uintptr_t) <= sizeof(cpkt_openssl_u64) ? 1 : -1];

struct cpkt_openssl_param_i64 {
  OSSL_PARAM native;
  int64_t value;
};

struct cpkt_openssl_param_u64 {
  OSSL_PARAM native;
  uint64_t value;
};

struct cpkt_openssl_sha512_context {
  SHA512_CTX native;
};

struct cpkt_openssl_atomic_u64 {
  uint64_t native;
};

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

static size_t cpkt_openssl_native_length(size_t public_length,
                                         size_t public_size,
                                         size_t native_size) {
  return public_length == public_size ? native_size : public_length;
}

static int cpkt_openssl_native_uintptr(cpkt_openssl_u64 value,
                                       uintptr_t *native_out) {
  uint64_t native_value;

  if (native_out == NULL ||
      (sizeof(uintptr_t) < sizeof(native_value) &&
       cpkt_openssl_u64_high_word(value) != 0UL)) {
    return 0;
  }
  native_value = cpkt_openssl_native_u64(value);
  *native_out = (uintptr_t) native_value;
  return 1;
}

static int cpkt_openssl_copy_poll_descriptor_to_native(
    BIO_POLL_DESCRIPTOR *native_descriptor,
    const cpkt_openssl_poll_descriptor *public_descriptor) {
  uintptr_t custom_uintptr;

  if (native_descriptor == NULL || public_descriptor == NULL ||
      (public_descriptor->type & ~0xffffffffUL) != 0UL) {
    return 0;
  }
  memset(native_descriptor, 0, sizeof(*native_descriptor));
  native_descriptor->type = (uint32_t) public_descriptor->type;
  switch (public_descriptor->value_kind) {
    case CPKT_OPENSSL_POLL_VALUE_NONE:
      return 1;
    case CPKT_OPENSSL_POLL_VALUE_FD:
      native_descriptor->value.fd = public_descriptor->file_descriptor;
      return 1;
    case CPKT_OPENSSL_POLL_VALUE_CUSTOM_POINTER:
      native_descriptor->value.custom = public_descriptor->custom_pointer;
      return 1;
    case CPKT_OPENSSL_POLL_VALUE_CUSTOM_UINTPTR:
      if (!cpkt_openssl_native_uintptr(public_descriptor->custom_uintptr,
                                       &custom_uintptr)) {
        return 0;
      }
      native_descriptor->value.custom_ui = custom_uintptr;
      return 1;
    case CPKT_OPENSSL_POLL_VALUE_SSL:
      native_descriptor->value.ssl = public_descriptor->ssl;
      return 1;
    default:
      return 0;
  }
}

static void cpkt_openssl_copy_poll_descriptor_from_native(
    cpkt_openssl_poll_descriptor *public_descriptor,
    const BIO_POLL_DESCRIPTOR *native_descriptor) {
  memset(public_descriptor, 0, sizeof(*public_descriptor));
  public_descriptor->type = (unsigned long) native_descriptor->type;
  if (native_descriptor->type == BIO_POLL_DESCRIPTOR_TYPE_SOCK_FD) {
    public_descriptor->value_kind = CPKT_OPENSSL_POLL_VALUE_FD;
    public_descriptor->file_descriptor = native_descriptor->value.fd;
  } else if (native_descriptor->type == BIO_POLL_DESCRIPTOR_TYPE_SSL) {
    public_descriptor->value_kind = CPKT_OPENSSL_POLL_VALUE_SSL;
    public_descriptor->ssl = native_descriptor->value.ssl;
  } else if (native_descriptor->type != BIO_POLL_DESCRIPTOR_TYPE_NONE) {
    public_descriptor->value_kind = CPKT_OPENSSL_POLL_VALUE_CUSTOM_POINTER;
    public_descriptor->custom_pointer = native_descriptor->value.custom;
  }
}

static int cpkt_openssl_copy_poll_items_to_native(
    SSL_POLL_ITEM *native_items, const cpkt_openssl_ssl_poll_item *items,
    size_t item_stride, size_t item_count) {
  size_t index;

  for (index = 0; index < item_count; ++index) {
    const cpkt_openssl_ssl_poll_item *public_item;

    public_item = (const cpkt_openssl_ssl_poll_item *) (
        (const unsigned char *) items + index * item_stride);
    if (!cpkt_openssl_copy_poll_descriptor_to_native(
            &native_items[index].desc, &public_item->descriptor)) {
      return 0;
    }
    native_items[index].events = cpkt_openssl_native_u64(public_item->events);
    native_items[index].revents = cpkt_openssl_native_u64(
        public_item->returned_events);
  }
  return 1;
}

static void cpkt_openssl_copy_poll_items_from_native(
    cpkt_openssl_ssl_poll_item *items, size_t item_stride,
    const SSL_POLL_ITEM *native_items, size_t item_count) {
  size_t index;

  for (index = 0; index < item_count; ++index) {
    cpkt_openssl_ssl_poll_item *public_item;

    public_item = (cpkt_openssl_ssl_poll_item *) (
        (unsigned char *) items + index * item_stride);
    cpkt_openssl_copy_poll_descriptor_from_native(&public_item->descriptor,
                                                  &native_items[index].desc);
    public_item->events = cpkt_openssl_public_u64(native_items[index].events);
    public_item->returned_events = cpkt_openssl_public_u64(
        native_items[index].revents);
  }
}

static void cpkt_openssl_copy_bio_messages_to_native(
    BIO_MSG *native_messages, const cpkt_openssl_bio_message *messages,
    size_t message_stride, size_t message_count) {
  size_t index;

  for (index = 0; index < message_count; ++index) {
    const cpkt_openssl_bio_message *public_message;

    public_message = (const cpkt_openssl_bio_message *) (
        (const unsigned char *) messages + index * message_stride);
    native_messages[index].data = public_message->data;
    native_messages[index].data_len = public_message->data_length;
    native_messages[index].peer = public_message->peer;
    native_messages[index].local = public_message->local;
    native_messages[index].flags = cpkt_openssl_native_u64(
        public_message->flags);
  }
}

static void cpkt_openssl_copy_bio_messages_from_native(
    cpkt_openssl_bio_message *messages, size_t message_stride,
    const BIO_MSG *native_messages, size_t message_count) {
  size_t index;

  for (index = 0; index < message_count; ++index) {
    cpkt_openssl_bio_message *public_message;

    public_message = (cpkt_openssl_bio_message *) (
        (unsigned char *) messages + index * message_stride);
    public_message->data = native_messages[index].data;
    public_message->data_length = native_messages[index].data_len;
    public_message->peer = native_messages[index].peer;
    public_message->local = native_messages[index].local;
    public_message->flags = cpkt_openssl_public_u64(native_messages[index].flags);
  }
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

/** Implements the documented public C89 adapter cpkt_openssl_BIO_number_read. */
cpkt_openssl_u64 cpkt_openssl_BIO_number_read(BIO *bio) {
  return cpkt_openssl_public_u64(BIO_number_read(bio));
}

/** Implements the documented public C89 adapter cpkt_openssl_BIO_number_written. */
cpkt_openssl_u64 cpkt_openssl_BIO_number_written(BIO *bio) {
  return cpkt_openssl_public_u64(BIO_number_written(bio));
}

/** Implements the documented public C89 adapter cpkt_openssl_OSSL_HPKE_CTX_get_seq. */
int cpkt_openssl_OSSL_HPKE_CTX_get_seq(
    OSSL_HPKE_CTX *context, cpkt_openssl_u64 *sequence_out) {
  uint64_t native_sequence;
  int result;

  native_sequence = 0;
  result = OSSL_HPKE_CTX_get_seq(
      context, sequence_out == NULL ? NULL : &native_sequence);
  if (result != 0 && sequence_out != NULL) {
    *sequence_out = cpkt_openssl_public_u64(native_sequence);
  }
  return result;
}

/** Implements the documented public C89 adapter cpkt_openssl_OSSL_HPKE_CTX_set_seq. */
int cpkt_openssl_OSSL_HPKE_CTX_set_seq(
    OSSL_HPKE_CTX *context, cpkt_openssl_u64 sequence) {
  return OSSL_HPKE_CTX_set_seq(context, cpkt_openssl_native_u64(sequence));
}

/** Implements the documented public C89 adapter cpkt_openssl_OSSL_PARAM_BLD_push_int64. */
int cpkt_openssl_OSSL_PARAM_BLD_push_int64(
    OSSL_PARAM_BLD *builder, const char *key, cpkt_openssl_i64 value) {
  return OSSL_PARAM_BLD_push_int64(
      builder, key, cpkt_openssl_native_i64(value));
}

/** Implements the documented public C89 adapter cpkt_openssl_OSSL_PARAM_BLD_push_uint64. */
int cpkt_openssl_OSSL_PARAM_BLD_push_uint64(
    OSSL_PARAM_BLD *builder, const char *key, cpkt_openssl_u64 value) {
  return OSSL_PARAM_BLD_push_uint64(
      builder, key, cpkt_openssl_native_u64(value));
}

/** Implements the documented public C89 adapter cpkt_openssl_OSSL_PARAM_construct_int64. */
int cpkt_openssl_OSSL_PARAM_construct_int64(
    cpkt_openssl_param_i64 **parameter_out, const char *key,
    cpkt_openssl_i64 value) {
  cpkt_openssl_param_i64 *parameter;

  if (parameter_out == NULL) {
    return 0;
  }
  *parameter_out = NULL;
  parameter = (cpkt_openssl_param_i64 *) malloc(sizeof(*parameter));
  if (parameter == NULL) {
    return 0;
  }
  parameter->value = cpkt_openssl_native_i64(value);
  parameter->native = OSSL_PARAM_construct_int64(key, &parameter->value);
  *parameter_out = parameter;
  return 1;
}

/** Implements the documented public C89 adapter cpkt_openssl_OSSL_PARAM_construct_uint64. */
int cpkt_openssl_OSSL_PARAM_construct_uint64(
    cpkt_openssl_param_u64 **parameter_out, const char *key,
    cpkt_openssl_u64 value) {
  cpkt_openssl_param_u64 *parameter;

  if (parameter_out == NULL) {
    return 0;
  }
  *parameter_out = NULL;
  parameter = (cpkt_openssl_param_u64 *) malloc(sizeof(*parameter));
  if (parameter == NULL) {
    return 0;
  }
  parameter->value = cpkt_openssl_native_u64(value);
  parameter->native = OSSL_PARAM_construct_uint64(key, &parameter->value);
  *parameter_out = parameter;
  return 1;
}

/** Implements the documented public C89 facade operation cpkt_openssl_param_i64_native. */
OSSL_PARAM *cpkt_openssl_param_i64_native(cpkt_openssl_param_i64 *parameter) {
  return parameter == NULL ? NULL : &parameter->native;
}

/** Implements the documented public C89 facade operation cpkt_openssl_param_i64_value. */
cpkt_openssl_i64 cpkt_openssl_param_i64_value(
    const cpkt_openssl_param_i64 *parameter) {
  return parameter == NULL ? cpkt_openssl_i64_make(0UL, 0UL) :
         cpkt_openssl_public_i64(parameter->value);
}

/** Implements the documented public C89 facade operation cpkt_openssl_param_i64_set_value. */
void cpkt_openssl_param_i64_set_value(
    cpkt_openssl_param_i64 *parameter, cpkt_openssl_i64 value) {
  if (parameter != NULL) {
    parameter->value = cpkt_openssl_native_i64(value);
  }
}

/** Implements the documented public C89 facade operation cpkt_openssl_param_i64_free. */
void cpkt_openssl_param_i64_free(cpkt_openssl_param_i64 *parameter) {
  free(parameter);
}

/** Implements the documented public C89 facade operation cpkt_openssl_param_u64_native. */
OSSL_PARAM *cpkt_openssl_param_u64_native(cpkt_openssl_param_u64 *parameter) {
  return parameter == NULL ? NULL : &parameter->native;
}

/** Implements the documented public C89 facade operation cpkt_openssl_param_u64_value. */
cpkt_openssl_u64 cpkt_openssl_param_u64_value(
    const cpkt_openssl_param_u64 *parameter) {
  return parameter == NULL ? cpkt_openssl_u64_make(0UL, 0UL) :
         cpkt_openssl_public_u64(parameter->value);
}

/** Implements the documented public C89 facade operation cpkt_openssl_param_u64_set_value. */
void cpkt_openssl_param_u64_set_value(
    cpkt_openssl_param_u64 *parameter, cpkt_openssl_u64 value) {
  if (parameter != NULL) {
    parameter->value = cpkt_openssl_native_u64(value);
  }
}

/** Implements the documented public C89 facade operation cpkt_openssl_param_u64_free. */
void cpkt_openssl_param_u64_free(cpkt_openssl_param_u64 *parameter) {
  free(parameter);
}

/** Implements the documented public C89 adapter cpkt_openssl_OSSL_PARAM_get_int64. */
int cpkt_openssl_OSSL_PARAM_get_int64(
    const OSSL_PARAM *parameter, cpkt_openssl_i64 *value_out) {
  int64_t native_value;
  int result;

  native_value = 0;
  result = OSSL_PARAM_get_int64(
      parameter, value_out == NULL ? NULL : &native_value);
  if (result != 0 && value_out != NULL) {
    *value_out = cpkt_openssl_public_i64(native_value);
  }
  return result;
}

/** Implements the documented public C89 adapter cpkt_openssl_OSSL_PARAM_get_uint64. */
int cpkt_openssl_OSSL_PARAM_get_uint64(
    const OSSL_PARAM *parameter, cpkt_openssl_u64 *value_out) {
  uint64_t native_value;
  int result;

  native_value = 0;
  result = OSSL_PARAM_get_uint64(
      parameter, value_out == NULL ? NULL : &native_value);
  if (result != 0 && value_out != NULL) {
    *value_out = cpkt_openssl_public_u64(native_value);
  }
  return result;
}

/** Implements the documented public C89 adapter cpkt_openssl_OSSL_PARAM_set_int64. */
int cpkt_openssl_OSSL_PARAM_set_int64(
    OSSL_PARAM *parameter, cpkt_openssl_i64 value) {
  return OSSL_PARAM_set_int64(parameter, cpkt_openssl_native_i64(value));
}

/** Implements the documented public C89 adapter cpkt_openssl_OSSL_PARAM_set_uint64. */
int cpkt_openssl_OSSL_PARAM_set_uint64(
    OSSL_PARAM *parameter, cpkt_openssl_u64 value) {
  return OSSL_PARAM_set_uint64(parameter, cpkt_openssl_native_u64(value));
}

/** Implements the documented public C89 facade operation cpkt_openssl_SHA512_CTX_new. */
cpkt_openssl_sha512_context *cpkt_openssl_SHA512_CTX_new(void) {
  return (cpkt_openssl_sha512_context *) calloc(1, sizeof(
      cpkt_openssl_sha512_context));
}

/** Implements the documented public C89 facade operation cpkt_openssl_SHA512_CTX_free. */
void cpkt_openssl_SHA512_CTX_free(cpkt_openssl_sha512_context *context) {
  free(context);
}

/** Implements the documented public C89 adapter cpkt_openssl_SHA384_Init. */
int cpkt_openssl_SHA384_Init(cpkt_openssl_sha512_context *context) {
  return context == NULL ? 0 : SHA384_Init(&context->native);
}

/** Implements the documented public C89 adapter cpkt_openssl_SHA384_Update. */
int cpkt_openssl_SHA384_Update(
    cpkt_openssl_sha512_context *context, const void *data, size_t length) {
  return context == NULL ? 0 : SHA384_Update(&context->native, data, length);
}

/** Implements the documented public C89 adapter cpkt_openssl_SHA384_Final. */
int cpkt_openssl_SHA384_Final(
    unsigned char *digest, cpkt_openssl_sha512_context *context) {
  return context == NULL ? 0 : SHA384_Final(digest, &context->native);
}

/** Implements the documented public C89 adapter cpkt_openssl_SHA512_Init. */
int cpkt_openssl_SHA512_Init(cpkt_openssl_sha512_context *context) {
  return context == NULL ? 0 : SHA512_Init(&context->native);
}

/** Implements the documented public C89 adapter cpkt_openssl_SHA512_Update. */
int cpkt_openssl_SHA512_Update(
    cpkt_openssl_sha512_context *context, const void *data, size_t length) {
  return context == NULL ? 0 : SHA512_Update(&context->native, data, length);
}

/** Implements the documented public C89 adapter cpkt_openssl_SHA512_Final. */
int cpkt_openssl_SHA512_Final(
    unsigned char *digest, cpkt_openssl_sha512_context *context) {
  return context == NULL ? 0 : SHA512_Final(digest, &context->native);
}

/** Implements the documented public C89 adapter cpkt_openssl_SHA512_Transform. */
void cpkt_openssl_SHA512_Transform(
    cpkt_openssl_sha512_context *context, const unsigned char *block) {
  if (context != NULL) {
    SHA512_Transform(&context->native, block);
  }
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_get_value_uint. */
int cpkt_openssl_SSL_get_value_uint(
    SSL *ssl, unsigned long value_class, unsigned long value_id,
    cpkt_openssl_u64 *value_out) {
  uint64_t native_value;
  int result;

  if (value_out != NULL) {
    *value_out = cpkt_openssl_u64_make(0UL, 0UL);
  }
  if (value_class > UINT32_MAX || value_id > UINT32_MAX) {
    return 0;
  }
  native_value = 0;
  result = SSL_get_value_uint(ssl, (uint32_t) value_class,
                               (uint32_t) value_id,
                               value_out == NULL ? NULL : &native_value);
  if (result != 0 && value_out != NULL) {
    *value_out = cpkt_openssl_public_u64(native_value);
  }
  return result;
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_set_value_uint. */
int cpkt_openssl_SSL_set_value_uint(
    SSL *ssl, unsigned long value_class, unsigned long value_id,
    cpkt_openssl_u64 value) {
  if (value_class > UINT32_MAX || value_id > UINT32_MAX) {
    return 0;
  }
  return SSL_set_value_uint(ssl, (uint32_t) value_class,
                            (uint32_t) value_id,
                            cpkt_openssl_native_u64(value));
}

/** Implements the documented public C89 facade operation cpkt_openssl_atomic_u64_new. */
cpkt_openssl_atomic_u64 *cpkt_openssl_atomic_u64_new(
    cpkt_openssl_u64 initial_value) {
  cpkt_openssl_atomic_u64 *value;

  value = (cpkt_openssl_atomic_u64 *) malloc(sizeof(*value));
  if (value != NULL) {
    value->native = cpkt_openssl_native_u64(initial_value);
  }
  return value;
}

/** Implements the documented public C89 facade operation cpkt_openssl_atomic_u64_free. */
void cpkt_openssl_atomic_u64_free(cpkt_openssl_atomic_u64 *value) {
  free(value);
}

/** Implements the documented public C89 adapter cpkt_openssl_CRYPTO_atomic_add64. */
int cpkt_openssl_CRYPTO_atomic_add64(
    cpkt_openssl_atomic_u64 *value, cpkt_openssl_u64 amount,
    cpkt_openssl_u64 *result_out, CRYPTO_RWLOCK *lock) {
  uint64_t result;
  int status;

  if (value == NULL) {
    return 0;
  }
  result = 0;
  status = CRYPTO_atomic_add64(&value->native, cpkt_openssl_native_u64(amount),
                               result_out == NULL ? NULL : &result, lock);
  if (status != 0 && result_out != NULL) {
    *result_out = cpkt_openssl_public_u64(result);
  }
  return status;
}

/** Implements the documented public C89 adapter cpkt_openssl_CRYPTO_atomic_and. */
int cpkt_openssl_CRYPTO_atomic_and(
    cpkt_openssl_atomic_u64 *value, cpkt_openssl_u64 mask,
    cpkt_openssl_u64 *result_out, CRYPTO_RWLOCK *lock) {
  uint64_t result;
  int status;

  if (value == NULL) {
    return 0;
  }
  result = 0;
  status = CRYPTO_atomic_and(&value->native, cpkt_openssl_native_u64(mask),
                             result_out == NULL ? NULL : &result, lock);
  if (status != 0 && result_out != NULL) {
    *result_out = cpkt_openssl_public_u64(result);
  }
  return status;
}

/** Implements the documented public C89 adapter cpkt_openssl_CRYPTO_atomic_load. */
int cpkt_openssl_CRYPTO_atomic_load(
    cpkt_openssl_atomic_u64 *value, cpkt_openssl_u64 *result_out,
    CRYPTO_RWLOCK *lock) {
  uint64_t result;
  int status;

  if (value == NULL) {
    return 0;
  }
  result = 0;
  status = CRYPTO_atomic_load(&value->native,
                              result_out == NULL ? NULL : &result, lock);
  if (status != 0 && result_out != NULL) {
    *result_out = cpkt_openssl_public_u64(result);
  }
  return status;
}

/** Implements the documented public C89 adapter cpkt_openssl_CRYPTO_atomic_or. */
int cpkt_openssl_CRYPTO_atomic_or(
    cpkt_openssl_atomic_u64 *value, cpkt_openssl_u64 mask,
    cpkt_openssl_u64 *result_out, CRYPTO_RWLOCK *lock) {
  uint64_t result;
  int status;

  if (value == NULL) {
    return 0;
  }
  result = 0;
  status = CRYPTO_atomic_or(&value->native, cpkt_openssl_native_u64(mask),
                            result_out == NULL ? NULL : &result, lock);
  if (status != 0 && result_out != NULL) {
    *result_out = cpkt_openssl_public_u64(result);
  }
  return status;
}

/** Implements the documented public C89 adapter cpkt_openssl_CRYPTO_atomic_store. */
int cpkt_openssl_CRYPTO_atomic_store(
    cpkt_openssl_atomic_u64 *value, cpkt_openssl_u64 replacement,
    CRYPTO_RWLOCK *lock) {
  if (value == NULL) {
    return 0;
  }
  return CRYPTO_atomic_store(&value->native,
                             cpkt_openssl_native_u64(replacement), lock);
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_shutdown_ex. */
int cpkt_openssl_SSL_shutdown_ex(
    SSL *ssl, cpkt_openssl_u64 flags,
    const cpkt_openssl_ssl_shutdown_args *arguments, size_t arguments_length) {
  SSL_SHUTDOWN_EX_ARGS native_arguments;

  if (arguments == NULL) {
    return SSL_shutdown_ex(ssl, cpkt_openssl_native_u64(flags), NULL,
                           arguments_length);
  }
  native_arguments.quic_error_code = cpkt_openssl_native_u64(
      arguments->quic_error_code);
  native_arguments.quic_reason = arguments->quic_reason;
  return SSL_shutdown_ex(ssl, cpkt_openssl_native_u64(flags),
                         &native_arguments,
                         cpkt_openssl_native_length(
                             arguments_length, sizeof(*arguments),
                             sizeof(native_arguments)));
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_stream_reset. */
int cpkt_openssl_SSL_stream_reset(
    SSL *ssl, const cpkt_openssl_ssl_stream_reset_args *arguments,
    size_t arguments_length) {
  SSL_STREAM_RESET_ARGS native_arguments;

  if (arguments == NULL) {
    return SSL_stream_reset(ssl, NULL, arguments_length);
  }
  native_arguments.quic_error_code = cpkt_openssl_native_u64(
      arguments->quic_error_code);
  return SSL_stream_reset(ssl, &native_arguments,
                          cpkt_openssl_native_length(
                              arguments_length, sizeof(*arguments),
                              sizeof(native_arguments)));
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_get_conn_close_info. */
int cpkt_openssl_SSL_get_conn_close_info(
    SSL *ssl, cpkt_openssl_ssl_conn_close_info *information_out,
    size_t information_length) {
  SSL_CONN_CLOSE_INFO native_information;
  int result;

  if (information_out == NULL) {
    return SSL_get_conn_close_info(ssl, NULL, information_length);
  }
  memset(&native_information, 0, sizeof(native_information));
  result = SSL_get_conn_close_info(ssl, &native_information,
                                   cpkt_openssl_native_length(
                                       information_length, sizeof(*information_out),
                                       sizeof(native_information)));
  if (result != 0) {
    information_out->error_code = cpkt_openssl_public_u64(
        native_information.error_code);
    information_out->frame_type = cpkt_openssl_public_u64(
        native_information.frame_type);
    information_out->reason = native_information.reason;
    information_out->reason_length = native_information.reason_len;
    information_out->flags = (unsigned long) native_information.flags;
  }
  return result;
}

/** Implements the documented public C89 adapter cpkt_openssl_BIO_recvmmsg. */
int cpkt_openssl_BIO_recvmmsg(
    BIO *bio, cpkt_openssl_bio_message *messages, size_t message_stride,
    size_t message_count, cpkt_openssl_u64 flags, size_t *processed_out) {
  BIO_MSG *native_messages;
  int result;

  if (messages == NULL || message_stride < sizeof(*messages) ||
      (message_count != 0 && message_count > SIZE_MAX / sizeof(*native_messages))) {
    return 0;
  }
  native_messages = (BIO_MSG *) calloc(message_count, sizeof(*native_messages));
  if (native_messages == NULL && message_count != 0) {
    return 0;
  }
  cpkt_openssl_copy_bio_messages_to_native(
      native_messages, messages, message_stride, message_count);
  result = BIO_recvmmsg(bio, native_messages, sizeof(*native_messages),
                        message_count, cpkt_openssl_native_u64(flags),
                        processed_out);
  cpkt_openssl_copy_bio_messages_from_native(
      messages, message_stride, native_messages, message_count);
  free(native_messages);
  return result;
}

/** Implements the documented public C89 adapter cpkt_openssl_BIO_sendmmsg. */
int cpkt_openssl_BIO_sendmmsg(
    BIO *bio, cpkt_openssl_bio_message *messages, size_t message_stride,
    size_t message_count, cpkt_openssl_u64 flags, size_t *processed_out) {
  BIO_MSG *native_messages;
  int result;

  if (messages == NULL || message_stride < sizeof(*messages) ||
      (message_count != 0 && message_count > SIZE_MAX / sizeof(*native_messages))) {
    return 0;
  }
  native_messages = (BIO_MSG *) calloc(message_count, sizeof(*native_messages));
  if (native_messages == NULL && message_count != 0) {
    return 0;
  }
  cpkt_openssl_copy_bio_messages_to_native(
      native_messages, messages, message_stride, message_count);
  result = BIO_sendmmsg(bio, native_messages, sizeof(*native_messages),
                        message_count, cpkt_openssl_native_u64(flags),
                        processed_out);
  cpkt_openssl_copy_bio_messages_from_native(
      messages, message_stride, native_messages, message_count);
  free(native_messages);
  return result;
}

/** Implements the documented public C89 adapter cpkt_openssl_BIO_get_rpoll_descriptor. */
int cpkt_openssl_BIO_get_rpoll_descriptor(
    BIO *bio, cpkt_openssl_poll_descriptor *descriptor_out) {
  BIO_POLL_DESCRIPTOR native_descriptor;
  int result;

  if (descriptor_out == NULL) {
    return 0;
  }
  result = BIO_get_rpoll_descriptor(bio, &native_descriptor);
  if (result != 0) {
    cpkt_openssl_copy_poll_descriptor_from_native(descriptor_out,
                                                  &native_descriptor);
  }
  return result;
}

/** Implements the documented public C89 adapter cpkt_openssl_BIO_get_wpoll_descriptor. */
int cpkt_openssl_BIO_get_wpoll_descriptor(
    BIO *bio, cpkt_openssl_poll_descriptor *descriptor_out) {
  BIO_POLL_DESCRIPTOR native_descriptor;
  int result;

  if (descriptor_out == NULL) {
    return 0;
  }
  result = BIO_get_wpoll_descriptor(bio, &native_descriptor);
  if (result != 0) {
    cpkt_openssl_copy_poll_descriptor_from_native(descriptor_out,
                                                  &native_descriptor);
  }
  return result;
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_get_rpoll_descriptor. */
int cpkt_openssl_SSL_get_rpoll_descriptor(
    SSL *ssl, cpkt_openssl_poll_descriptor *descriptor_out) {
  BIO_POLL_DESCRIPTOR native_descriptor;
  int result;

  if (descriptor_out == NULL) {
    return 0;
  }
  result = SSL_get_rpoll_descriptor(ssl, &native_descriptor);
  if (result != 0) {
    cpkt_openssl_copy_poll_descriptor_from_native(descriptor_out,
                                                  &native_descriptor);
  }
  return result;
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_get_wpoll_descriptor. */
int cpkt_openssl_SSL_get_wpoll_descriptor(
    SSL *ssl, cpkt_openssl_poll_descriptor *descriptor_out) {
  BIO_POLL_DESCRIPTOR native_descriptor;
  int result;

  if (descriptor_out == NULL) {
    return 0;
  }
  result = SSL_get_wpoll_descriptor(ssl, &native_descriptor);
  if (result != 0) {
    cpkt_openssl_copy_poll_descriptor_from_native(descriptor_out,
                                                  &native_descriptor);
  }
  return result;
}

/** Implements the documented public C89 helper cpkt_openssl_SSL_as_poll_descriptor. */
void cpkt_openssl_SSL_as_poll_descriptor(
    SSL *ssl, cpkt_openssl_poll_descriptor *descriptor_out) {
  if (descriptor_out == NULL) {
    return;
  }
  memset(descriptor_out, 0, sizeof(*descriptor_out));
  descriptor_out->type = BIO_POLL_DESCRIPTOR_TYPE_SSL;
  descriptor_out->value_kind = CPKT_OPENSSL_POLL_VALUE_SSL;
  descriptor_out->ssl = ssl;
}

/** Implements the documented public C89 adapter cpkt_openssl_SSL_poll. */
int cpkt_openssl_SSL_poll(
    cpkt_openssl_ssl_poll_item *items, size_t item_count, size_t item_stride,
    const struct timeval *timeout, cpkt_openssl_u64 flags,
    size_t *result_count_out) {
  SSL_POLL_ITEM *native_items;
  int result;

  if (items == NULL || item_stride < sizeof(*items) ||
      (item_count != 0 && item_count > SIZE_MAX / sizeof(*native_items))) {
    return 0;
  }
  native_items = (SSL_POLL_ITEM *) calloc(item_count, sizeof(*native_items));
  if (native_items == NULL && item_count != 0) {
    return 0;
  }
  if (!cpkt_openssl_copy_poll_items_to_native(native_items, items,
                                              item_stride, item_count)) {
    free(native_items);
    return 0;
  }
  result = SSL_poll(native_items, item_count, sizeof(*native_items), timeout,
                    cpkt_openssl_native_u64(flags), result_count_out);
  cpkt_openssl_copy_poll_items_from_native(items, item_stride, native_items,
                                           item_count);
  free(native_items);
  return result;
}
