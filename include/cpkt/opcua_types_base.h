#ifndef CPKT_OPCUA_TYPES_BASE_H
#define CPKT_OPCUA_TYPES_BASE_H

#include <cpkt/opcua.h>

#ifdef __cplusplus
extern "C" {
#endif

/** C89 schema types. Names and fields follow open62541's public model.
 * Integers use fixed widths on every supported target. Int64/UInt64/DateTime
 * carry the exact two's-complement bit pattern, most significant word first.
 * These types are NOT layout compatible with native UA types. */
/** Boolean stored as a C89 byte; conversion uses zero / nonzero truth. */
typedef unsigned char cpkt_opcua_Boolean;
/** Signed 8-bit integer. */
typedef signed char cpkt_opcua_SByte;
/** Unsigned 8-bit integer. */
typedef unsigned char cpkt_opcua_Byte;
/** Signed 16-bit integer. */
typedef short cpkt_opcua_Int16;
/** Unsigned 16-bit integer. */
typedef unsigned short cpkt_opcua_UInt16;
/** Signed 32-bit integer. */
typedef int cpkt_opcua_Int32;
/** Unsigned 32-bit integer. */
typedef unsigned int cpkt_opcua_UInt32;
/** Unsigned 64-bit bit pattern as two 32-bit words, high word first. */
typedef struct {
  cpkt_opcua_UInt32 high32;
  cpkt_opcua_UInt32 low32;
} cpkt_opcua_UInt64;
/** Signed 64-bit two's-complement bit pattern; high word is unsigned. */
typedef cpkt_opcua_UInt64 cpkt_opcua_Int64;
/** OPC UA timestamp bit pattern, using the signed 64-bit representation. */
typedef cpkt_opcua_Int64 cpkt_opcua_DateTime;
/** Native 32-bit floating point value. */
typedef float cpkt_opcua_Float;
/** Native 64-bit floating point value. */
typedef double cpkt_opcua_Double;
/** Exact 32-bit upstream status code. */
typedef cpkt_opcua_UInt32 cpkt_opcua_StatusCode;
/** Sized byte string; NULL differs from EMPTY_ARRAY_SENTINEL at length zero. */
typedef struct {
  size_t length;
  cpkt_opcua_Byte *data;
} cpkt_opcua_String;
/** Byte string with the same length and ownership rules as String. */
typedef cpkt_opcua_String cpkt_opcua_ByteString;
/** XML payload with the same length and ownership rules as String. */
typedef cpkt_opcua_String cpkt_opcua_XmlElement;
/** GUID fields in the upstream public order. */
typedef struct {
  cpkt_opcua_UInt32 data1;
  cpkt_opcua_UInt16 data2, data3;
  cpkt_opcua_Byte data4[8];
} cpkt_opcua_Guid;
/** Discriminator for the NodeId identifier union. */
typedef enum {
  CPKT_OPCUA_NODEIDTYPE_NUMERIC = 0,
  CPKT_OPCUA_NODEIDTYPE_STRING = 3,
  CPKT_OPCUA_NODEIDTYPE_GUID = 4,
  CPKT_OPCUA_NODEIDTYPE_BYTESTRING = 5
} cpkt_opcua_NodeIdType;
/** Node identifier; only the member selected by identifierType is active. */
typedef struct {
  cpkt_opcua_UInt16 namespaceIndex;
  cpkt_opcua_NodeIdType identifierType;
  union {
    cpkt_opcua_UInt32 numeric;
    cpkt_opcua_String string;
    cpkt_opcua_Guid guid;
    cpkt_opcua_ByteString byteString;
  } identifier;
} cpkt_opcua_NodeId;
/** Node identifier plus explicit namespace URI and server index. */
typedef struct {
  cpkt_opcua_NodeId nodeId;
  cpkt_opcua_String namespaceUri;
  cpkt_opcua_UInt32 serverIndex;
} cpkt_opcua_ExpandedNodeId;
/** Name plus namespace index. */
typedef struct {
  cpkt_opcua_UInt16 namespaceIndex;
  cpkt_opcua_String name;
} cpkt_opcua_QualifiedName;
/** Sized locale and text strings. */
typedef struct {
  cpkt_opcua_String locale, text;
} cpkt_opcua_LocalizedText;

/** Opaque TYPE descriptor; payloads remain ordinary, generated C89 structs. */
/** Public schema type. */
typedef struct cpkt_opcua_Type cpkt_opcua_Type;
#define CPKT_OPCUA_EMPTY_ARRAY_SENTINEL ((void *)1)
/** DATA owns payload and dimensions; NODELETE borrows both. */
typedef enum {
  CPKT_OPCUA_VARIANT_DATA,
  CPKT_OPCUA_VARIANT_DATA_NODELETE
} cpkt_opcua_VariantStorageType;
/** Typed scalar or array. Preserve upstream NULL / empty-array / scalar rules.
 */
typedef struct {
  const cpkt_opcua_Type *type;
  cpkt_opcua_VariantStorageType storageType;
  size_t arrayLength;
  void *data;
  size_t arrayDimensionsSize;
  cpkt_opcua_UInt32 *arrayDimensions;
} cpkt_opcua_Variant;
/** Encoded or decoded payload; DECODED_NODELETE borrows payload. */
typedef enum {
  CPKT_OPCUA_EXTENSIONOBJECT_ENCODED_NOBODY = 0,
  CPKT_OPCUA_EXTENSIONOBJECT_ENCODED_BYTESTRING = 1,
  CPKT_OPCUA_EXTENSIONOBJECT_ENCODED_XML = 2,
  CPKT_OPCUA_EXTENSIONOBJECT_DECODED = 3,
  CPKT_OPCUA_EXTENSIONOBJECT_DECODED_NODELETE = 4
} cpkt_opcua_ExtensionObjectEncoding;
/** Typed decoded payload or uninterpreted binary/XML body. */
typedef struct {
  cpkt_opcua_ExtensionObjectEncoding encoding;
  union {
    struct {
      cpkt_opcua_NodeId typeId;
      cpkt_opcua_ByteString body;
    } encoded;
    struct {
      const cpkt_opcua_Type *type;
      void *data;
    } decoded;
  } content;
} cpkt_opcua_ExtensionObject;
/** Value, status, timestamps, picoseconds and all presence flags. */
typedef struct {
  cpkt_opcua_Variant value;
  cpkt_opcua_DateTime sourceTimestamp, serverTimestamp;
  cpkt_opcua_UInt16 sourcePicoseconds, serverPicoseconds;
  cpkt_opcua_StatusCode status;
  cpkt_opcua_Boolean hasValue, hasStatus, hasSourceTimestamp,
      hasServerTimestamp;
  cpkt_opcua_Boolean hasSourcePicoseconds, hasServerPicoseconds;
} cpkt_opcua_DataValue;
/** Recursive diagnostics; the innerDiagnosticInfo pointer is optional. */
typedef struct cpkt_opcua_DiagnosticInfo {
  cpkt_opcua_Boolean hasSymbolicId, hasNamespaceUri, hasLocalizedText,
      hasLocale;
  cpkt_opcua_Boolean hasAdditionalInfo, hasInnerStatusCode,
      hasInnerDiagnosticInfo;
  cpkt_opcua_Int32 symbolicId, namespaceUri, localizedText, locale;
  cpkt_opcua_String additionalInfo;
  cpkt_opcua_StatusCode innerStatusCode;
  struct cpkt_opcua_DiagnosticInfo *innerDiagnosticInfo;
} cpkt_opcua_DiagnosticInfo;

/** Return a generated descriptor, or NULL for an out-of-range index. */
const cpkt_opcua_Type *cpkt_opcua_type_at(size_t index);
/** Descriptor name and C89 object size; NULL yields NULL / zero. */
const char *cpkt_opcua_type_name(const cpkt_opcua_Type *type);
/** Return the C89 allocation size, or zero for NULL. */
size_t cpkt_opcua_type_size(const cpkt_opcua_Type *type);
/** Initialize/new produce empty values. Copy/decode/service destinations must
 * be empty; clear them before reuse. Copies own all nested allocations,
 * including formerly borrowed Variant/ExtensionObject data. Clear honors the
 * NODELETE flags of caller-built values. Buffers are freed by these helpers,
 * never by a host allocator. Borrowed arrays/strings must not be cleared. */
void cpkt_opcua_type_init(void *value, const cpkt_opcua_Type *type);
/** Allocate an empty owned value; NULL indicates allocation failure. */
void *cpkt_opcua_type_new(const cpkt_opcua_Type *type);
/** Free owned nested allocations and reset the value to empty. */
void cpkt_opcua_type_clear(void *value, const cpkt_opcua_Type *type);
/** Clear an owned value and free its top-level allocation; NULL is harmless. */
void cpkt_opcua_type_delete(void *value, const cpkt_opcua_Type *type);
/** Deep-copy into an empty distinct destination; failure leaves it empty. */
cpkt_opcua_StatusCode cpkt_opcua_type_copy(const void *src, void *dst,
                                           const cpkt_opcua_Type *type);
/** Compare through upstream UA_order; invalid values or allocation failure
 * return false. */
cpkt_opcua_Boolean cpkt_opcua_type_equal(const void *a, const void *b,
                                         const cpkt_opcua_Type *type);
/** Use upstream binary encoding without changing its wire format. The output
 * buffer/destination must be empty and owns allocations on success; it stays
 * empty on failure. These are materialized message codecs, like upstream. */
cpkt_opcua_StatusCode cpkt_opcua_type_encode_binary(const void *src,
                                                    const cpkt_opcua_Type *type,
                                                    cpkt_opcua_ByteString *out);
/** Decode with the upstream codec into an empty owned destination; failure
 * leaves it empty. */
cpkt_opcua_StatusCode
cpkt_opcua_type_decode_binary(const cpkt_opcua_ByteString *src, void *dst,
                              const cpkt_opcua_Type *type);

#ifdef __cplusplus
}
#endif
#endif
