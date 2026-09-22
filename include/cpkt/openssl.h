#ifndef CPKT_OPENSSL_H
#define CPKT_OPENSSL_H

/*
 * Complete C89-compatible import surface for the OpenSSL headers bundled by
 * c.pkt.systems.  OpenSSL's legacy SHA-512 state declares long long despite
 * otherwise accepting strict C89; contain that upstream diagnostic here so a
 * consumer's own source remains warning-clean under -std=c89 -pedantic-errors.
 * Typed cpkt_openssl_* adapters will provide C89 word values for APIs that
 * accept or return native 64-bit values.
 */
#if defined(__GNUC__) || defined(__clang__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wlong-long"
#endif

#if defined(_WIN32) && defined(CPKT_OPENSSL_BUILDING_SHARED)
# define CPKT_OPENSSL_API __declspec(dllexport)
#elif defined(_WIN32) && !defined(CPKT_OPENSSL_STATIC)
# define CPKT_OPENSSL_API __declspec(dllimport)
#elif defined(__GNUC__) || defined(__clang__)
# define CPKT_OPENSSL_API __attribute__((visibility("default")))
#else
# define CPKT_OPENSSL_API
#endif

#include <openssl/aes.h>
#include <openssl/asn1.h>
#include <openssl/asn1err.h>
#include <openssl/asn1t.h>
#include <openssl/async.h>
#include <openssl/asyncerr.h>
#include <openssl/bio.h>
#include <openssl/bioerr.h>
#include <openssl/blowfish.h>
#include <openssl/bn.h>
#include <openssl/bnerr.h>
#include <openssl/buffer.h>
#include <openssl/buffererr.h>
#include <openssl/byteorder.h>
#include <openssl/camellia.h>
#include <openssl/cast.h>
#include <openssl/cmac.h>
#include <openssl/cmp.h>
#include <openssl/cmp_util.h>
#include <openssl/cmperr.h>
#include <openssl/cms.h>
#include <openssl/cmserr.h>
#include <openssl/comp.h>
#include <openssl/comperr.h>
#include <openssl/conf.h>
#include <openssl/conf_api.h>
#include <openssl/conferr.h>
#include <openssl/configuration.h>
#include <openssl/conftypes.h>
#include <openssl/core.h>
#include <openssl/core_dispatch.h>
#include <openssl/core_names.h>
#include <openssl/core_object.h>
#include <openssl/crmf.h>
#include <openssl/crmferr.h>
#include <openssl/crypto.h>
#include <openssl/cryptoerr.h>
#include <openssl/cryptoerr_legacy.h>
#include <openssl/ct.h>
#include <openssl/cterr.h>
#include <openssl/decoder.h>
#include <openssl/decodererr.h>
#include <openssl/des.h>
#include <openssl/dh.h>
#include <openssl/dherr.h>
#include <openssl/dsa.h>
#include <openssl/dsaerr.h>
#include <openssl/dtls1.h>
#include <openssl/e_os2.h>
#include <openssl/e_ostime.h>
#include <openssl/ebcdic.h>
#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/ecdsa.h>
#include <openssl/ecerr.h>
#include <openssl/encoder.h>
#include <openssl/encodererr.h>
#include <openssl/engine.h>
#include <openssl/engineerr.h>
#include <openssl/err.h>
#include <openssl/ess.h>
#include <openssl/esserr.h>
#include <openssl/evp.h>
#include <openssl/evperr.h>
#include <openssl/fips_names.h>
#include <openssl/fipskey.h>
#include <openssl/hmac.h>
#include <openssl/hpke.h>
#include <openssl/http.h>
#include <openssl/httperr.h>
#include <openssl/idea.h>
#include <openssl/indicator.h>
#include <openssl/kdf.h>
#include <openssl/kdferr.h>
#include <openssl/lhash.h>
#include <openssl/macros.h>
#include <openssl/md2.h>
#include <openssl/md4.h>
#include <openssl/md5.h>
#include <openssl/mdc2.h>
#include <openssl/ml_kem.h>
#include <openssl/modes.h>
#include <openssl/obj_mac.h>
#include <openssl/objects.h>
#include <openssl/objectserr.h>
#include <openssl/ocsp.h>
#include <openssl/ocsperr.h>
#include <openssl/opensslconf.h>
#include <openssl/opensslv.h>
#include <openssl/ossl_typ.h>
#include <openssl/param_build.h>
#include <openssl/params.h>
#include <openssl/pem.h>
#include <openssl/pem2.h>
#include <openssl/pemerr.h>
#include <openssl/pkcs12.h>
#include <openssl/pkcs12err.h>
#include <openssl/pkcs7.h>
#include <openssl/pkcs7err.h>
#include <openssl/prov_ssl.h>
#include <openssl/proverr.h>
#include <openssl/provider.h>
#include <openssl/quic.h>
#include <openssl/rand.h>
#include <openssl/randerr.h>
#include <openssl/rc2.h>
#include <openssl/rc4.h>
#include <openssl/rc5.h>
#include <openssl/ripemd.h>
#include <openssl/rsa.h>
#include <openssl/rsaerr.h>
#include <openssl/safestack.h>
#include <openssl/seed.h>
#include <openssl/self_test.h>
#include <openssl/sha.h>
#include <openssl/srp.h>
#include <openssl/srtp.h>
#include <openssl/ssl.h>
#include <openssl/ssl2.h>
#include <openssl/ssl3.h>
#include <openssl/sslerr.h>
#include <openssl/sslerr_legacy.h>
#include <openssl/stack.h>
#include <openssl/store.h>
#include <openssl/storeerr.h>
#include <openssl/symhacks.h>
#include <openssl/thread.h>
#include <openssl/tls1.h>
#include <openssl/trace.h>
#include <openssl/ts.h>
#include <openssl/tserr.h>
#include <openssl/txt_db.h>
#include <openssl/types.h>
#include <openssl/ui.h>
#include <openssl/uierr.h>
#include <openssl/whrlpool.h>
#include <openssl/x509.h>
#include <openssl/x509_acert.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509err.h>
#include <openssl/x509v3.h>
#include <openssl/x509v3err.h>

#if defined(__GNUC__) || defined(__clang__)
# pragma GCC diagnostic pop
#endif

/* Exact unsigned 64-bit bits represented without a C99 scalar. The byte
 * storage is exactly eight bytes so it can back upstream pointer APIs. */
/** C89 OpenSSL facade declaration. */
typedef struct cpkt_openssl_u64 {
  unsigned char bytes[8];
} cpkt_openssl_u64;

/* Exact signed 64-bit two's-complement bits in exactly eight bytes. */
/** C89 OpenSSL facade declaration. */
typedef struct cpkt_openssl_i64 {
  unsigned char bytes[8];
} cpkt_openssl_i64;

/** C89 OpenSSL facade declaration. */
CPKT_OPENSSL_API cpkt_openssl_u64 cpkt_openssl_u64_make(
    unsigned long high, unsigned long low);
/** Returns the most-significant 32-bit word of an exact unsigned value. */
CPKT_OPENSSL_API unsigned long cpkt_openssl_u64_high_word(
    cpkt_openssl_u64 value);
/** Returns the least-significant 32-bit word of an exact unsigned value. */
CPKT_OPENSSL_API unsigned long cpkt_openssl_u64_low_word(
    cpkt_openssl_u64 value);
/** C89 OpenSSL facade declaration. */
CPKT_OPENSSL_API int cpkt_openssl_u64_equal(
    cpkt_openssl_u64 left, cpkt_openssl_u64 right);
/** C89 OpenSSL facade declaration. */
CPKT_OPENSSL_API int cpkt_openssl_u64_is_zero(cpkt_openssl_u64 value);
/** Creates exact signed 64-bit two's-complement bits from two 32-bit words. */
CPKT_OPENSSL_API cpkt_openssl_i64 cpkt_openssl_i64_make(
    unsigned long high, unsigned long low);
/** Returns the most-significant two's-complement word of a signed value. */
CPKT_OPENSSL_API unsigned long cpkt_openssl_i64_high_word(
    cpkt_openssl_i64 value);
/** Returns the least-significant two's-complement word of a signed value. */
CPKT_OPENSSL_API unsigned long cpkt_openssl_i64_low_word(
    cpkt_openssl_i64 value);
/** Compares exact signed 64-bit two's-complement bits. */
CPKT_OPENSSL_API int cpkt_openssl_i64_equal(
    cpkt_openssl_i64 left, cpkt_openssl_i64 right);

/** C89 adapter for OPENSSL_init_crypto. */
CPKT_OPENSSL_API int cpkt_openssl_OPENSSL_init_crypto(
    cpkt_openssl_u64 options, const OPENSSL_INIT_SETTINGS *settings);
/** C89 adapter for OPENSSL_init_ssl. */
CPKT_OPENSSL_API int cpkt_openssl_OPENSSL_init_ssl(
    cpkt_openssl_u64 options, const OPENSSL_INIT_SETTINGS *settings);
/** C89 adapter for SSL_CTX_get_options. */
CPKT_OPENSSL_API cpkt_openssl_u64 cpkt_openssl_SSL_CTX_get_options(
    const SSL_CTX *context);
/** C89 adapter for SSL_get_options. */
CPKT_OPENSSL_API cpkt_openssl_u64 cpkt_openssl_SSL_get_options(const SSL *ssl);
/** C89 adapter for SSL_CTX_set_options. */
CPKT_OPENSSL_API cpkt_openssl_u64 cpkt_openssl_SSL_CTX_set_options(
    SSL_CTX *context, cpkt_openssl_u64 options);
/** C89 adapter for SSL_set_options. */
CPKT_OPENSSL_API cpkt_openssl_u64 cpkt_openssl_SSL_set_options(
    SSL *ssl, cpkt_openssl_u64 options);
/** C89 adapter for SSL_CTX_clear_options. */
CPKT_OPENSSL_API cpkt_openssl_u64 cpkt_openssl_SSL_CTX_clear_options(
    SSL_CTX *context, cpkt_openssl_u64 options);
/** C89 adapter for SSL_clear_options. */
CPKT_OPENSSL_API cpkt_openssl_u64 cpkt_openssl_SSL_clear_options(
    SSL *ssl, cpkt_openssl_u64 options);
/** C89 adapter for SSL_get_handshake_rtt. */
CPKT_OPENSSL_API int cpkt_openssl_SSL_get_handshake_rtt(
    const SSL *ssl, cpkt_openssl_u64 *rtt_out);

/** C89 adapter for ASN1_ENUMERATED_get_int64. */
CPKT_OPENSSL_API int cpkt_openssl_ASN1_ENUMERATED_get_int64(
    cpkt_openssl_i64 *value_out, const ASN1_ENUMERATED *enumerated);
/** C89 adapter for ASN1_ENUMERATED_set_int64. */
CPKT_OPENSSL_API int cpkt_openssl_ASN1_ENUMERATED_set_int64(
    ASN1_ENUMERATED *enumerated, cpkt_openssl_i64 value);
/** C89 adapter for ASN1_INTEGER_get_int64. */
CPKT_OPENSSL_API int cpkt_openssl_ASN1_INTEGER_get_int64(
    cpkt_openssl_i64 *value_out, const ASN1_INTEGER *integer);
/** C89 adapter for ASN1_INTEGER_get_uint64. */
CPKT_OPENSSL_API int cpkt_openssl_ASN1_INTEGER_get_uint64(
    cpkt_openssl_u64 *value_out, const ASN1_INTEGER *integer);
/** C89 adapter for ASN1_INTEGER_set_int64. */
CPKT_OPENSSL_API int cpkt_openssl_ASN1_INTEGER_set_int64(
    ASN1_INTEGER *integer, cpkt_openssl_i64 value);
/** C89 adapter for ASN1_INTEGER_set_uint64. */
CPKT_OPENSSL_API int cpkt_openssl_ASN1_INTEGER_set_uint64(
    ASN1_INTEGER *integer, cpkt_openssl_u64 value);
/** C89 adapter for CT_POLICY_EVAL_CTX_get_time. */
CPKT_OPENSSL_API cpkt_openssl_u64 cpkt_openssl_CT_POLICY_EVAL_CTX_get_time(
    const CT_POLICY_EVAL_CTX *context);
/** C89 adapter for CT_POLICY_EVAL_CTX_set_time. */
CPKT_OPENSSL_API void cpkt_openssl_CT_POLICY_EVAL_CTX_set_time(
    CT_POLICY_EVAL_CTX *context, cpkt_openssl_u64 value);
/** C89 adapter for OSSL_get_max_threads. */
CPKT_OPENSSL_API cpkt_openssl_u64 cpkt_openssl_OSSL_get_max_threads(
    OSSL_LIB_CTX *library_context);
/** C89 adapter for OSSL_set_max_threads. */
CPKT_OPENSSL_API int cpkt_openssl_OSSL_set_max_threads(
    OSSL_LIB_CTX *library_context, cpkt_openssl_u64 value);
/** C89 adapter for OSSL_sleep. */
CPKT_OPENSSL_API void cpkt_openssl_OSSL_sleep(cpkt_openssl_u64 milliseconds);
/** C89 adapter for SCT_get_timestamp. */
CPKT_OPENSSL_API cpkt_openssl_u64 cpkt_openssl_SCT_get_timestamp(
    const SCT *sct);
/** C89 adapter for SCT_new_from_base64. */
CPKT_OPENSSL_API SCT *cpkt_openssl_SCT_new_from_base64(
    unsigned char version, const char *log_id, ct_log_entry_type_t entry_type,
    cpkt_openssl_u64 timestamp, const char *extensions, const char *signature);
/** C89 adapter for SCT_set_timestamp. */
CPKT_OPENSSL_API void cpkt_openssl_SCT_set_timestamp(
    SCT *sct, cpkt_openssl_u64 timestamp);
/** C89 adapter for EVP_PBE_scrypt. */
CPKT_OPENSSL_API int cpkt_openssl_EVP_PBE_scrypt(
    const char *password, size_t password_length, const unsigned char *salt,
    size_t salt_length, cpkt_openssl_u64 work_factor,
    cpkt_openssl_u64 block_size, cpkt_openssl_u64 parallelization,
    cpkt_openssl_u64 maximum_memory, unsigned char *key, size_t key_length);
/** C89 adapter for EVP_PBE_scrypt_ex. */
CPKT_OPENSSL_API int cpkt_openssl_EVP_PBE_scrypt_ex(
    const char *password, size_t password_length, const unsigned char *salt,
    size_t salt_length, cpkt_openssl_u64 work_factor,
    cpkt_openssl_u64 block_size, cpkt_openssl_u64 parallelization,
    cpkt_openssl_u64 maximum_memory, unsigned char *key, size_t key_length,
    OSSL_LIB_CTX *library_context, const char *property_query);
/** C89 adapter for EVP_PKEY_CTX_ctrl_uint64. */
CPKT_OPENSSL_API int cpkt_openssl_EVP_PKEY_CTX_ctrl_uint64(
    EVP_PKEY_CTX *context, int key_type, int operation, int command,
    cpkt_openssl_u64 value);
/** C89 adapter for EVP_PKEY_CTX_set_scrypt_N. */
CPKT_OPENSSL_API int cpkt_openssl_EVP_PKEY_CTX_set_scrypt_N(
    EVP_PKEY_CTX *context, cpkt_openssl_u64 work_factor);
/** C89 adapter for EVP_PKEY_CTX_set_scrypt_maxmem_bytes. */
CPKT_OPENSSL_API int cpkt_openssl_EVP_PKEY_CTX_set_scrypt_maxmem_bytes(
    EVP_PKEY_CTX *context, cpkt_openssl_u64 maximum_memory);
/** C89 adapter for EVP_PKEY_CTX_set_scrypt_p. */
CPKT_OPENSSL_API int cpkt_openssl_EVP_PKEY_CTX_set_scrypt_p(
    EVP_PKEY_CTX *context, cpkt_openssl_u64 parallelization);
/** C89 adapter for EVP_PKEY_CTX_set_scrypt_r. */
CPKT_OPENSSL_API int cpkt_openssl_EVP_PKEY_CTX_set_scrypt_r(
    EVP_PKEY_CTX *context, cpkt_openssl_u64 block_size);
/** C89 adapter for PKCS5_pbe2_set_scrypt. */
CPKT_OPENSSL_API X509_ALGOR *cpkt_openssl_PKCS5_pbe2_set_scrypt(
    const EVP_CIPHER *cipher, const unsigned char *salt, int salt_length,
    unsigned char *iv, cpkt_openssl_u64 work_factor,
    cpkt_openssl_u64 block_size, cpkt_openssl_u64 parallelization);
/** C89 adapter for SSL_CTX_get_domain_flags. */
CPKT_OPENSSL_API int cpkt_openssl_SSL_CTX_get_domain_flags(
    const SSL_CTX *context, cpkt_openssl_u64 *flags_out);
/** C89 adapter for SSL_CTX_set_domain_flags. */
CPKT_OPENSSL_API int cpkt_openssl_SSL_CTX_set_domain_flags(
    SSL_CTX *context, cpkt_openssl_u64 flags);
/** C89 adapter for SSL_get_domain_flags. */
CPKT_OPENSSL_API int cpkt_openssl_SSL_get_domain_flags(
    const SSL *ssl, cpkt_openssl_u64 *flags_out);
/** C89 adapter for SSL_accept_connection. */
CPKT_OPENSSL_API SSL *cpkt_openssl_SSL_accept_connection(
    SSL *ssl, cpkt_openssl_u64 domain_flags);
/** C89 adapter for SSL_accept_stream. */
CPKT_OPENSSL_API SSL *cpkt_openssl_SSL_accept_stream(
    SSL *ssl, cpkt_openssl_u64 stream_id);
/** C89 adapter for SSL_get_stream_id. */
CPKT_OPENSSL_API cpkt_openssl_u64 cpkt_openssl_SSL_get_stream_id(SSL *ssl);
/** C89 adapter for SSL_get_stream_read_error_code. */
CPKT_OPENSSL_API int cpkt_openssl_SSL_get_stream_read_error_code(
    SSL *ssl, cpkt_openssl_u64 *error_code_out);
/** C89 adapter for SSL_get_stream_write_error_code. */
CPKT_OPENSSL_API int cpkt_openssl_SSL_get_stream_write_error_code(
    SSL *ssl, cpkt_openssl_u64 *error_code_out);
/** C89 adapter for SSL_new_domain. */
CPKT_OPENSSL_API SSL *cpkt_openssl_SSL_new_domain(
    SSL_CTX *context, cpkt_openssl_u64 domain_flags);
/** C89 adapter for SSL_new_from_listener. */
CPKT_OPENSSL_API SSL *cpkt_openssl_SSL_new_from_listener(
    SSL *listener, cpkt_openssl_u64 stream_id);
/** C89 adapter for SSL_new_listener. */
CPKT_OPENSSL_API SSL *cpkt_openssl_SSL_new_listener(
    SSL_CTX *context, cpkt_openssl_u64 domain_flags);
/** C89 adapter for SSL_new_listener_from. */
CPKT_OPENSSL_API SSL *cpkt_openssl_SSL_new_listener_from(
    SSL *ssl, cpkt_openssl_u64 domain_flags);
/** C89 adapter for SSL_new_stream. */
CPKT_OPENSSL_API SSL *cpkt_openssl_SSL_new_stream(
    SSL *ssl, cpkt_openssl_u64 stream_id);
/** C89 adapter for SSL_set_incoming_stream_policy. */
CPKT_OPENSSL_API int cpkt_openssl_SSL_set_incoming_stream_policy(
    SSL *ssl, int policy, cpkt_openssl_u64 application_error_code);
/** C89 adapter for SSL_stream_conclude. */
CPKT_OPENSSL_API int cpkt_openssl_SSL_stream_conclude(
    SSL *ssl, cpkt_openssl_u64 application_error_code);
/** C89 adapter for SSL_write_ex2. */
CPKT_OPENSSL_API int cpkt_openssl_SSL_write_ex2(
    SSL *ssl, const void *buffer, size_t buffer_length,
    cpkt_openssl_u64 flags, size_t *written_out);

#endif
