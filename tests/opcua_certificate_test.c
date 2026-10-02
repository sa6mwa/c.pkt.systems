#include "opcua_callbacks_test.h"
#include "opcua_types_peer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void check(int ok, const char *expression, int line) {
  if (!ok) {
    fprintf(stderr, "certificate facade line %d: %s\n", line, expression);
    abort();
  }
}
#define CHECK(x) check(!!(x), #x, __LINE__)
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
void cpkt_types_fail_after(size_t);
int cpkt_types_fail_stop(void);
#endif
/* Synthetic encrypted EC test key, generated for this test; password is public.
 */
static const char encrypted_key[] =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIH0MF8GCSqGSIb3DQEFDTBSMDEGCSqGSIb3DQEFDDAkBBDYeWeqf68GeSqkM0ui\n"
    "LV8FAgIIADAMBggqhkiG9w0CCQUAMB0GCWCGSAFlAwQBKgQQawq3d6qx8CK59qxZ\n"
    "LWgASASBkKhhyNnibyB+RHQfCWvEUMX5A7b4QjWc0rVccRxHk/sO2dtMuOjX8EiV\n"
    "JJ1oBv1B5PCkrfrC1uA327/lfIg2+Qiao8aGiDZfE03iSVF3+NVsh86p/xaRxSvb\n"
    "Bylw9cd880rXyZrTRGUif9PZ7fjA7iNSq3oQm+JMB3gl6TvfJkKpv9Gfzdq8pi3C\n"
    "e8UM2YmU1g==\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";
static unsigned int logs;
static void certificate_log(const cpkt_opcua_log_record *record, void *user) {
  CHECK(user == &logs && record->message && record->message_length);
  ++logs;
}
static void set_string(cpkt_opcua_KeyValueMap *map, const char *key,
                       const char *data) {
  cpkt_opcua_String value = cpkt_opcua_STRING((char *)data);
  CHECK(!cpkt_opcua_KeyValueMap_setScalar(
      map, cpkt_opcua_QUALIFIEDNAME(0, (char *)key), &value,
      cpkt_opcua_type_at(CPKT_OPCUA_TYPES_STRING)));
}
static void inspect(cpkt_opcua_ByteString *certificate,
                    cpkt_opcua_ByteString *key) {
  cpkt_opcua_String uri = cpkt_opcua_STRING("urn:cpkt:certificate");
  cpkt_opcua_String subject;
  cpkt_opcua_String thumb;
  cpkt_opcua_DateTime expiry = {0, 0};
  cpkt_opcua_ByteString decrypted;
  cpkt_opcua_ByteString password = cpkt_opcua_BYTESTRING("");
  cpkt_opcua_StatusCode status;
  unsigned int high = 0, low = 0;
  size_t native_size = 0, size = 0;
  unsigned char guarded[42];
  CHECK(!cpkt_opcua_CertificateUtils_verifyApplicationUri(certificate, &uri));
  CHECK(!cpkt_types_peer_certificate(certificate->data, certificate->length,
                                     uri.data, uri.length, 0, NULL, NULL,
                                     NULL));
  uri = cpkt_opcua_STRING("urn:wrong");
  status = cpkt_opcua_CertificateUtils_verifyApplicationUri(certificate, &uri);
  CHECK(status && status == cpkt_types_peer_certificate(
                                certificate->data, certificate->length,
                                uri.data, uri.length, 0, NULL, NULL, NULL));
  CHECK(!cpkt_opcua_CertificateUtils_getExpirationDate(certificate, &expiry));
  CHECK(!cpkt_types_peer_certificate(certificate->data, certificate->length,
                                     NULL, 0, 1, &high, &low, NULL));
  CHECK(expiry.high32 == high && expiry.low32 == low && high);
  cpkt_opcua_String_init(&subject);
  CHECK(!cpkt_opcua_CertificateUtils_getSubjectName(certificate, &subject));
  CHECK(!cpkt_types_peer_certificate(certificate->data, certificate->length,
                                     subject.data, subject.length, 2, NULL,
                                     NULL, NULL));
  cpkt_opcua_String_clear(&subject);
  memset(guarded, 0xa5, sizeof(guarded));
  thumb.length = 40;
  thumb.data = guarded + 1;
  CHECK(!cpkt_opcua_CertificateUtils_getThumbprint(certificate, &thumb));
  CHECK(thumb.data == guarded + 1 && thumb.length == 40 && guarded[0] == 0xa5 &&
        guarded[41] == 0xa5);
  CHECK(!cpkt_types_peer_certificate(certificate->data, certificate->length,
                                     thumb.data, thumb.length, 3, NULL, NULL,
                                     NULL));
  thumb.length = 39;
  CHECK(cpkt_opcua_CertificateUtils_getThumbprint(certificate, &thumb) ==
        CPKT_OPCUA_STATUSCODE_BADINTERNALERROR);
  CHECK(!cpkt_opcua_CertificateUtils_getKeySize(certificate, &size));
  CHECK(!cpkt_types_peer_certificate(certificate->data, certificate->length,
                                     NULL, 0, 4, NULL, NULL, &native_size) &&
        size == native_size);
  status = cpkt_opcua_CertificateUtils_checkCA(certificate);
  CHECK(status == cpkt_types_peer_certificate(certificate->data,
                                              certificate->length, NULL, 0, 5,
                                              NULL, NULL, NULL));
  CHECK(!cpkt_opcua_CertificateUtils_checkKeyPair(certificate, key));
  CHECK(!cpkt_types_peer_certificate(certificate->data, certificate->length,
                                     key->data, key->length, 6, NULL, NULL,
                                     NULL));
  CHECK(
      !cpkt_opcua_CertificateUtils_comparePublicKeys(certificate, certificate));
  CHECK(!cpkt_types_peer_certificate(certificate->data, certificate->length,
                                     certificate->data, certificate->length, 7,
                                     NULL, NULL, NULL));
  cpkt_opcua_ByteString_init(&decrypted);
  CHECK(!cpkt_opcua_CertificateUtils_decryptPrivateKey(*key, password,
                                                       &decrypted));
  CHECK(!cpkt_opcua_CertificateUtils_checkKeyPair(certificate, &decrypted));
  CHECK(!cpkt_types_peer_decrypted_key(key->data, key->length, password.data,
                                       password.length, decrypted.data,
                                       decrypted.length));
  cpkt_opcua_ByteString_clear(&decrypted);
}
static void certificate_groups(cpkt_opcua_ByteString *certificate) {
  cpkt_opcua_CertificateGroup group;
  cpkt_opcua_NodeId id = cpkt_opcua_NODEID_NUMERIC(0, 14156);
  cpkt_opcua_TrustListDataType trust, readback;
  cpkt_opcua_ByteString *rejected = NULL;
  cpkt_opcua_ByteString invalid = cpkt_opcua_BYTESTRING("invalid certificate");
  size_t count = 0;
  memset(&group, 0, sizeof(group));
  memset(&trust, 0, sizeof(trust));
  memset(&readback, 0, sizeof(readback));
  CHECK(cpkt_opcua_CertificateGroup_AcceptAll(&group) == 0);
  CHECK(group.verifyCertificate(&group, &invalid) == 0);
  group.clear(&group);
  CHECK(group.context == NULL && group.clear == NULL);
  trust.specifiedLists = cpkt_opcua_TRUSTLISTMASKS_ALL;
  trust.trustedCertificatesSize = 1;
  trust.trustedCertificates = certificate;
  CHECK(cpkt_opcua_CertificateGroup_Memorystore(&group, &id, &trust, NULL,
                                                NULL) == 0);
  CHECK(cpkt_opcua_NodeId_equal(&group.certificateGroupId, &id));
  CHECK(group.getTrustList(&group, &readback) == 0 &&
        readback.trustedCertificatesSize == 1);
  CHECK(cpkt_opcua_ByteString_equal(&readback.trustedCertificates[0],
                                    certificate) &&
        readback.trustedCertificates[0].data != certificate->data);
  cpkt_opcua_TrustListDataType_clear(&readback);
  CHECK(group.verifyCertificate(&group, certificate) == 0);
  CHECK(group.verifyCertificate(&group, &invalid) != 0);
  CHECK(group.getRejectedList(&group, &rejected, &count) == 0);
  cpkt_opcua_array_delete(rejected, count,
                          cpkt_opcua_type_at(CPKT_OPCUA_TYPES_BYTESTRING));
  CHECK(group.removeFromTrustList(&group, &trust) == 0);
  CHECK(group.getTrustList(&group, &readback) == 0 &&
        readback.trustedCertificatesSize == 0);
  cpkt_opcua_TrustListDataType_clear(&readback);
  CHECK(group.addToTrustList(&group, &trust) == 0);
  CHECK(group.setTrustList(&group, &trust) == 0);
  CHECK(group.verifyCertificate(&group, certificate) == 0);
  group.clear(&group);
}
static void stock_policy(cpkt_opcua_ByteString certificate,
                         cpkt_opcua_ByteString key, int ec) {
  cpkt_opcua_SecurityPolicy policy;
  cpkt_opcua_ByteString thumb, nonce;
  cpkt_opcua_StatusCode status;
  size_t kind;
  typedef cpkt_opcua_StatusCode (*factory)(
      cpkt_opcua_SecurityPolicy *, const cpkt_opcua_ByteString,
      const cpkt_opcua_ByteString, const cpkt_opcua_log_config *);
  static const factory factories[] = {
      cpkt_opcua_SecurityPolicy_Basic128Rsa15,
      cpkt_opcua_SecurityPolicy_Basic256,
      cpkt_opcua_SecurityPolicy_Basic256Sha256,
      cpkt_opcua_SecurityPolicy_Aes128Sha256RsaOaep,
      cpkt_opcua_SecurityPolicy_Aes256Sha256RsaPss};
  for (kind = 0; kind < (ec ? 1 : sizeof(factories) / sizeof(factories[0]));
       ++kind) {
    memset(&policy, 0, sizeof(policy));
    memset(&thumb, 0, sizeof(thumb));
    memset(&nonce, 0, sizeof(nonce));
    status = ec ? cpkt_opcua_SecurityPolicy_EccNistP256(
                      &policy, cpkt_opcua_APPLICATIONTYPE_SERVER, certificate,
                      key, NULL)
                : factories[kind](&policy, certificate, key, NULL);
    CHECK(status == 0 && policy.clear != NULL && policy.policyUri.length > 0);
    CHECK(cpkt_opcua_ByteString_equal(&policy.localCertificate, &certificate) &&
          policy.localCertificate.data != certificate.data);
    CHECK(cpkt_opcua_ByteString_allocBuffer(&thumb, 20) == 0);
    CHECK(policy.makeCertThumbprint(&policy, &certificate, &thumb) == 0);
    CHECK(policy.compareCertThumbprint(&policy, &thumb) == 0);
    CHECK(cpkt_opcua_SecurityPolicy_refresh(&policy) == 0);
    CHECK(cpkt_opcua_ByteString_allocBuffer(&nonce, policy.nonceLength) == 0);
    memset(nonce.data, 0, nonce.length);
    CHECK(policy.generateNonce(&policy, NULL, &nonce) == 0);
    cpkt_opcua_ByteString_clear(&nonce);
    cpkt_opcua_ByteString_clear(&thumb);
    policy.clear(&policy);
    CHECK(policy.policyContext == NULL && policy.clear == NULL);
  }
}
static void pubsub_crypto(void) {
  cpkt_opcua_PubSubSecurityPolicy policy;
  memset(&policy, 0, sizeof(policy));
  /* Upstream implements these stock factories only with mbedTLS. The bundle
   * uses OpenSSL; callback-based custom policies remain fully available. */
  CHECK(cpkt_opcua_PubSubSecurityPolicy_Aes128Ctr(&policy, NULL) ==
        CPKT_OPCUA_STATUSCODE_BADNOTSUPPORTED);
  CHECK(policy.policyContext == NULL && policy.clear == NULL);
  CHECK(cpkt_opcua_PubSubSecurityPolicy_Aes256Ctr(&policy, NULL) ==
        CPKT_OPCUA_STATUSCODE_BADNOTSUPPORTED);
  CHECK(policy.policyContext == NULL && policy.clear == NULL);
}
static void trust_lists(void) {
  cpkt_opcua_TrustListDataType source, dst, removal, addition;
  cpkt_opcua_ByteString values[24], erase = cpkt_opcua_BYTESTRING("remove");
  cpkt_opcua_ByteString extra = cpkt_opcua_BYTESTRING("new");
  size_t sizes[4], position, i;
  unsigned int specified, total, operation, mask;
  cpkt_opcua_StatusCode status;
  int injected;
  for (i = 0; i < 24; ++i)
    values[i] = cpkt_opcua_BYTESTRING(i % 2 ? "remove" : "keep");
  cpkt_opcua_TrustListDataType_init(&source);
  source.specifiedLists = cpkt_opcua_TRUSTLISTMASKS_ALL;
  source.trustedCertificates = source.trustedCrls = source.issuerCertificates =
      source.issuerCrls = values;
  source.trustedCertificatesSize = source.trustedCrlsSize =
      source.issuerCertificatesSize = source.issuerCrlsSize = 24;
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  cpkt_types_fail_after(0);
#endif
  CHECK(cpkt_opcua_TrustListDataType_contains(&source, &erase,
                                              cpkt_opcua_TRUSTLISTMASKS_ALL));
  CHECK(!cpkt_opcua_TrustListDataType_contains(&source, &erase, 0));
  CHECK(cpkt_opcua_TrustListDataType_getSize(&source) == 24 * 5 * 4);
  values[23] = extra;
  CHECK(cpkt_opcua_TrustListDataType_contains(
      &source, &extra, cpkt_opcua_TRUSTLISTMASKS_ISSUERCRLS));
  CHECK(!cpkt_opcua_TrustListDataType_contains(&source, &extra, 0));
  values[23] = cpkt_opcua_BYTESTRING("remove");
  extra = cpkt_opcua_BYTESTRING("absent");
  CHECK(!cpkt_opcua_TrustListDataType_contains(&source, &extra,
                                               cpkt_opcua_TRUSTLISTMASKS_ALL));
  extra = cpkt_opcua_BYTESTRING("new");
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  CHECK(!cpkt_types_fail_stop());
#endif
  source.trustedCertificatesSize = source.trustedCrlsSize =
      source.issuerCertificatesSize = source.issuerCrlsSize = 2;
  removal = source;
  removal.trustedCertificates = removal.trustedCrls =
      removal.issuerCertificates = removal.issuerCrls = &erase;
  removal.trustedCertificatesSize = removal.trustedCrlsSize =
      removal.issuerCertificatesSize = removal.issuerCrlsSize = 1;
  addition = removal;
  addition.trustedCertificates = addition.trustedCrls =
      addition.issuerCertificates = addition.issuerCrls = &extra;
  for (operation = 0; operation < 4; ++operation)
    for (mask = 0; mask <= 15; ++mask) {
      CHECK(!cpkt_opcua_TrustListDataType_copy(&source, &dst));
      source.specifiedLists = mask;
      if (operation == 0)
        CHECK(!cpkt_opcua_TrustListDataType_add(&source, &dst));
      else if (operation == 1)
        CHECK(!cpkt_opcua_TrustListDataType_set(&source, &dst));
      else if (operation == 2)
        CHECK(!cpkt_opcua_TrustListDataType_remove(&removal, &dst));
      else
        CHECK(!cpkt_opcua_TrustListDataType_set(&dst, &dst));
      total = cpkt_types_peer_trust(operation, mask, sizes, &specified);
      CHECK(dst.trustedCertificatesSize == sizes[0] &&
            dst.trustedCrlsSize == sizes[1]);
      CHECK(dst.issuerCertificatesSize == sizes[2] &&
            dst.issuerCrlsSize == sizes[3]);
      CHECK(dst.specifiedLists == specified &&
            cpkt_opcua_TrustListDataType_getSize(&dst) == total);
      cpkt_opcua_TrustListDataType_clear(&dst);
      source.specifiedLists = cpkt_opcua_TRUSTLISTMASKS_ALL;
    }
  for (operation = 0; operation < 3; ++operation) {
    for (position = 0; position < 256; ++position) {
      CHECK(!cpkt_opcua_TrustListDataType_copy(&source, &dst));
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
      cpkt_types_fail_after(position);
#endif
      if (operation == 0)
        status = cpkt_opcua_TrustListDataType_add(&addition, &dst);
      else if (operation == 1)
        status = cpkt_opcua_TrustListDataType_set(&removal, &dst);
      else
        status = cpkt_opcua_TrustListDataType_remove(&removal, &dst);
      injected = 0;
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
      injected = cpkt_types_fail_stop();
#endif
      CHECK(injected ? status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY
                     : !status);
      CHECK(dst.trustedCertificatesSize <= 3 && dst.trustedCrlsSize <= 3 &&
            dst.issuerCertificatesSize <= 3 && dst.issuerCrlsSize <= 3);
      if (operation == 0) {
        CHECK(dst.trustedCertificatesSize >= 2 && dst.trustedCrlsSize >= 2 &&
              dst.issuerCertificatesSize >= 2 && dst.issuerCrlsSize >= 2);
        CHECK(dst.trustedCertificates[0].length == 4 &&
              !memcmp(dst.trustedCertificates[0].data, "keep", 4));
        CHECK(dst.trustedCertificates[1].length == 6 &&
              !memcmp(dst.trustedCertificates[1].data, "remove", 6));
        if (!status)
          CHECK(dst.trustedCertificatesSize == 3 && dst.trustedCrlsSize == 3 &&
                dst.issuerCertificatesSize == 3 && dst.issuerCrlsSize == 3);
      }
      cpkt_opcua_TrustListDataType_clear(&dst);
      if (!injected)
        break;
    }
    CHECK(position < 256);
  }
  CHECK(cpkt_opcua_TrustListDataType_contains(NULL, &erase, 15) == 0);
  CHECK(cpkt_opcua_TrustListDataType_set(NULL, NULL) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
}
void cpkt_types_test_certificates(void) {
  cpkt_opcua_KeyValueMap params = cpkt_opcua_KEYVALUEMAP_NULL;
  cpkt_opcua_String subject = cpkt_opcua_STRING("CN=cpkt certificates");
  cpkt_opcua_String names[2];
  cpkt_opcua_ByteString key, certificate, decoded, input, password;
  cpkt_opcua_log_config logger;
  cpkt_opcua_StatusCode status;
  /* Synthetic local fixtures exercise both encodings and policy factories,
   * not RSA prime-search performance or secure-channel key-strength policy. */
  cpkt_opcua_UInt16 days = 3, bits = 1024;
  cpkt_opcua_DateTime date = {0x12345678U, 0xabcdef01U};
  size_t i, position, generated_bits;
  int injected;
  unsigned int encoding;
  names[0] = cpkt_opcua_STRING("DNS:localhost");
  names[1] = cpkt_opcua_STRING("URI:urn:cpkt:certificate");
  CHECK(!cpkt_opcua_KeyValueMap_setScalar(
      &params, cpkt_opcua_QUALIFIEDNAME(0, "expires-in-days"), &days,
      cpkt_opcua_type_at(CPKT_OPCUA_TYPES_UINT16)));
  CHECK(!cpkt_opcua_KeyValueMap_setScalar(
      &params, cpkt_opcua_QUALIFIEDNAME(0, "key-size-bits"), &bits,
      cpkt_opcua_type_at(CPKT_OPCUA_TYPES_UINT16)));
  memset(&logger, 0, sizeof(logger));
  logger.fn = certificate_log;
  logger.user = &logs;
  for (i = 0; i < 2; ++i) {
    set_string(&params, "key-type", i ? "EC" : "RSA");
    for (encoding = 0; encoding < 2; ++encoding) {
      cpkt_opcua_ByteString_init(&key);
      cpkt_opcua_ByteString_init(&certificate);
      CHECK(!cpkt_opcua_CreateCertificate(
          &logger, &subject, 1, names, 2,
          (cpkt_opcua_CertificateFormat)encoding, &params, &key, &certificate));
      CHECK(key.length && certificate.length);
      if (!i) {
        CHECK(!cpkt_opcua_CertificateUtils_getKeySize(&certificate,
                                                      &generated_bits));
        CHECK(generated_bits == bits);
      }
      inspect(&certificate, &key);
      if (encoding == 0) {
        certificate_groups(&certificate);
        stock_policy(certificate, key, i != 0);
      }
      if (i == 1 && encoding == 0) {
        for (position = 0; position < 4096; ++position) {
          cpkt_opcua_String result;
          cpkt_opcua_StatusCode native_status;
          cpkt_opcua_String_init(&result);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
          cpkt_types_fail_after(position);
#endif
          status =
              cpkt_opcua_CertificateUtils_getSubjectName(&certificate, &result);
          injected = 0;
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
          injected = cpkt_types_fail_stop();
#endif
          cpkt_opcua_String_clear(&result);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
          cpkt_types_fail_after(position);
#endif
          native_status =
              cpkt_types_peer_certificate(certificate.data, certificate.length,
                                          NULL, 0, 2, NULL, NULL, NULL);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
          CHECK(cpkt_types_fail_stop() == injected);
#endif
          CHECK(status == native_status);
          if (!injected) {
            CHECK(!status);
            break;
          }
        }
        CHECK(position < 4096);
      }
      cpkt_opcua_ByteString_clear(&key);
      cpkt_opcua_ByteString_clear(&certificate);
    }
  }
  input = cpkt_opcua_BYTESTRING("invalid");
  CHECK(cpkt_opcua_CertificateUtils_getExpirationDate(&input, &date) &&
        date.high32 == 0x12345678U && date.low32 == 0xabcdef01U);
  password = cpkt_opcua_BYTESTRING("cpkt-fixture");
  input.length = sizeof(encrypted_key) - 1;
  input.data = (unsigned char *)encrypted_key;
  cpkt_opcua_ByteString_init(&decoded);
  CHECK(!cpkt_opcua_CertificateUtils_decryptPrivateKey(input, password,
                                                       &decoded));
  CHECK(!cpkt_types_peer_decrypted_key(input.data, input.length, password.data,
                                       password.length, decoded.data,
                                       decoded.length));
  cpkt_opcua_ByteString_clear(&decoded);
  password = cpkt_opcua_BYTESTRING("wrong");
  status =
      cpkt_opcua_CertificateUtils_decryptPrivateKey(input, password, &decoded);
  CHECK(status == CPKT_OPCUA_STATUSCODE_BADSECURITYCHECKSFAILED);
  CHECK(status == cpkt_types_peer_decrypted_key(input.data, input.length,
                                                password.data, password.length,
                                                NULL, 0));
  cpkt_opcua_ByteString_clear(&decoded);
  set_string(&params, "ecc-curve", "not-a-curve");
  logs = 0;
  CHECK(cpkt_opcua_CreateCertificate(&logger, &subject, 1, names, 2,
                                     CPKT_OPCUA_CERTIFICATEFORMAT_DER, &params,
                                     &key, &certificate));
  CHECK(logs);
  cpkt_opcua_ByteString_clear(&key);
  cpkt_opcua_ByteString_clear(&certificate);
  cpkt_opcua_KeyValueMap_clear(&params);
  trust_lists();
  pubsub_crypto();
}
