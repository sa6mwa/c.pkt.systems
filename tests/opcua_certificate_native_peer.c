#include "opcua_types_peer.h"
#include <open62541/plugin/certificategroup.h>
#include <open62541/util.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "certificate peer line %d: %s\n", __LINE__, #x);         \
      abort();                                                                 \
    }                                                                          \
  } while (0)
unsigned int cpkt_types_peer_certificate(const void *data, size_t length,
                                         const void *other, size_t other_length,
                                         unsigned int operation,
                                         unsigned int *high, unsigned int *low,
                                         size_t *size) {
  UA_ByteString certificate = {length, (UA_Byte *)data};
  UA_ByteString second = {other_length, (UA_Byte *)other};
  UA_String text = UA_STRING_NULL;
  UA_DateTime date = 0;
  UA_StatusCode status;
  unsigned char thumb[40];
  switch (operation) {
  case 0:
    return UA_CertificateUtils_verifyApplicationUri(&certificate, &second);
  case 1:
    status = UA_CertificateUtils_getExpirationDate(&certificate, &date);
    if (!status) {
      *high = (UA_UInt32)((UA_UInt64)date >> 32);
      *low = (UA_UInt32)date;
    }
    return status;
  case 2:
    status = UA_CertificateUtils_getSubjectName(&certificate, &text);
    if (!status && other)
      CHECK(text.length == other_length &&
            !memcmp(text.data, other, other_length));
    UA_String_clear(&text);
    return status;
  case 3:
    text.length = sizeof(thumb);
    text.data = thumb;
    status = UA_CertificateUtils_getThumbprint(&certificate, &text);
    if (!status)
      CHECK(other_length == sizeof(thumb) &&
            !memcmp(thumb, other, sizeof(thumb)));
    return status;
  case 4:
    return UA_CertificateUtils_getKeySize(&certificate, size);
  case 5:
    return UA_CertificateUtils_checkCA(&certificate);
  case 6:
    return UA_CertificateUtils_checkKeyPair(&certificate, &second);
  case 7:
    return UA_CertificateUtils_comparePublicKeys(&certificate, &second);
  default:
    abort();
  }
}
unsigned int cpkt_types_peer_decrypted_key(const void *data, size_t length,
                                           const void *password,
                                           size_t password_length,
                                           const void *expected,
                                           size_t expected_length) {
  UA_ByteString key = {length, (UA_Byte *)data};
  UA_ByteString secret = {password_length, (UA_Byte *)password};
  UA_ByteString output = UA_BYTESTRING_NULL;
  UA_StatusCode status =
      UA_CertificateUtils_decryptPrivateKey(key, secret, &output);
  if (!status)
    CHECK(output.length == expected_length &&
          !memcmp(output.data, expected, expected_length));
  UA_ByteString_clear(&output);
  return status;
}
/* Exercise native public trust helpers independently of facade conversion. */
unsigned int cpkt_types_peer_trust(unsigned int operation, unsigned int mask,
                                   size_t *sizes, unsigned int *specified) {
  UA_TrustListDataType source, destination;
  UA_TrustListDataType_init(&source);
  UA_TrustListDataType_init(&destination);
  UA_ByteString input[] = {UA_STRING("keep"), UA_STRING("remove")};
  UA_ByteString removal = UA_STRING("remove");
  source.specifiedLists = UA_TRUSTLISTMASKS_ALL;
  source.trustedCertificatesSize = source.issuerCertificatesSize = 2;
  source.trustedCrlsSize = source.issuerCrlsSize = 2;
  source.trustedCertificates = source.issuerCertificates = input;
  source.trustedCrls = source.issuerCrls = input;
  CHECK(!UA_TrustListDataType_copy(&source, &destination));
  source.specifiedLists = mask;
  if (operation == 0)
    CHECK(!UA_TrustListDataType_add(&source, &destination));
  else if (operation == 1)
    CHECK(!UA_TrustListDataType_set(&source, &destination));
  else if (operation == 2) {
    source.trustedCertificates = source.issuerCertificates = &removal;
    source.trustedCrls = source.issuerCrls = &removal;
    source.trustedCertificatesSize = source.issuerCertificatesSize = 1;
    source.trustedCrlsSize = source.issuerCrlsSize = 1;
    CHECK(!UA_TrustListDataType_remove(&source, &destination));
  } else if (operation == 3)
    CHECK(!UA_TrustListDataType_set(&destination, &destination));
  else
    abort();
  sizes[0] = destination.trustedCertificatesSize;
  sizes[1] = destination.trustedCrlsSize;
  sizes[2] = destination.issuerCertificatesSize;
  sizes[3] = destination.issuerCrlsSize;
  *specified = destination.specifiedLists;
  UA_UInt32 total = UA_TrustListDataType_getSize(&destination);
  UA_TrustListDataType_clear(&destination);
  return total;
}
