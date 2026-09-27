#ifndef CPKT_OPCUA_UTIL_H
#define CPKT_OPCUA_UTIL_H
#include <cpkt/opcua_types.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Native total ordering values. */
typedef enum {
  CPKT_OPCUA_ORDER_LESS = -1,
  CPKT_OPCUA_ORDER_EQ = 0,
  CPKT_OPCUA_ORDER_MORE = 1
} cpkt_opcua_Order;
/** Compare complete generated records through UA_order. Conversion/allocation
 * errors are returned separately; order is reset to EQ on failure and must not
 * be used unless the status is Good. Inputs remain borrowed and unchanged. */
cpkt_opcua_StatusCode cpkt_opcua_type_order(const void *a, const void *b,
                                            const cpkt_opcua_Type *type,
                                            cpkt_opcua_Order *order);
/** Resize an owned C89 array, preserving native growth/shrink semantics.
 * New elements are zero initialized; removed elements are cleared. Failure
 * preserves the pointer, length, and all elements. Shrinking to zero produces
 * the empty-array sentinel. Do not resize borrowed arrays or scalar Variants.
 */
cpkt_opcua_StatusCode cpkt_opcua_array_resize(void **array, size_t *length,
                                              size_t new_length,
                                              const cpkt_opcua_Type *type);
/** Move an independently owned element into an owned array. Success empties
 * the source; failure leaves both unchanged. Source must not alias the array.
 */
cpkt_opcua_StatusCode cpkt_opcua_array_append(void **array, size_t *length,
                                              void *element,
                                              const cpkt_opcua_Type *type);
/** Append a deep copy; source may be borrowed and remains unchanged, including
 * when it is an existing array element. Preserves upstream's 512-byte native
 * type-size limit (BadInternalError). Failure preserves the array. */
cpkt_opcua_StatusCode cpkt_opcua_array_appendCopy(void **array, size_t *length,
                                                  const void *element,
                                                  const cpkt_opcua_Type *type);
/** Native empty-value predicate; NULL input returns true. */
cpkt_opcua_Boolean cpkt_opcua_Variant_isEmpty(const cpkt_opcua_Variant *value);
/** Native scalar predicate, including NULL and empty-array distinctions. */
cpkt_opcua_Boolean cpkt_opcua_Variant_isScalar(const cpkt_opcua_Variant *value);
/** Native array predicate; an empty untyped Variant is not an array. */
cpkt_opcua_Boolean cpkt_opcua_Variant_isArray(const cpkt_opcua_Variant *value);
/** Native scalar/type identity predicate; descriptors are borrowed. */
cpkt_opcua_Boolean
cpkt_opcua_Variant_hasScalarType(const cpkt_opcua_Variant *value,
                                 const cpkt_opcua_Type *type);
/** Native array/type predicate. Upstream returns true for an empty Variant
 * queried with NULL type; this intentionally preserves that behavior. */
cpkt_opcua_Boolean
cpkt_opcua_Variant_hasArrayType(const cpkt_opcua_Variant *value,
                                const cpkt_opcua_Type *type);
/** Transfer scalar ownership into an empty Variant without copying. The value
 * must come from the facade allocator. Clear the previous Variant first; like
 * upstream this setter initializes, rather than clearing, its destination. */
void cpkt_opcua_Variant_setScalar(cpkt_opcua_Variant *value, void *scalar,
                                  const cpkt_opcua_Type *type);
/** Transfer array ownership into an empty Variant without copying. NULL and
 * empty-sentinel arrays retain their native distinction. Clear old data first.
 */
void cpkt_opcua_Variant_setArray(cpkt_opcua_Variant *value, void *array,
                                 size_t length, const cpkt_opcua_Type *type);
/** Deep-copy a borrowed scalar into an empty Variant. Failure leaves the
 * destination unchanged, matching upstream setScalarCopy. */
cpkt_opcua_StatusCode
cpkt_opcua_Variant_setScalarCopy(cpkt_opcua_Variant *value, const void *scalar,
                                 const cpkt_opcua_Type *type);
/** Deep-copy a borrowed array into an empty Variant. Failure leaves it empty,
 * matching upstream setArrayCopy. Clear previous data before calling. */
cpkt_opcua_StatusCode
cpkt_opcua_Variant_setArrayCopy(cpkt_opcua_Variant *value, const void *array,
                                size_t length, const cpkt_opcua_Type *type);
/** Transfer owned decoded payload into an empty ExtensionObject. Clear previous
 * contents first. The payload must come from the facade allocator. */
void cpkt_opcua_ExtensionObject_setValue(cpkt_opcua_ExtensionObject *value,
                                         void *payload,
                                         const cpkt_opcua_Type *type);
/** Borrow a decoded payload; clear will not free it. Caller keeps payload and
 * descriptor alive for every use of the ExtensionObject. Clear old data first.
 */
void cpkt_opcua_ExtensionObject_setValueNoDelete(
    cpkt_opcua_ExtensionObject *value, void *payload,
    const cpkt_opcua_Type *type);
/** Deep-copy into an empty ExtensionObject; failure leaves it empty. */
cpkt_opcua_StatusCode
cpkt_opcua_ExtensionObject_setValueCopy(cpkt_opcua_ExtensionObject *value,
                                        const void *payload,
                                        const cpkt_opcua_Type *type);
/** Test decoded encoding and descriptor identity without allocation. */
cpkt_opcua_Boolean cpkt_opcua_ExtensionObject_hasDecodedType(
    const cpkt_opcua_ExtensionObject *value, const cpkt_opcua_Type *type);
/** Native status severity predicate. */
cpkt_opcua_Boolean cpkt_opcua_StatusCode_isGood(cpkt_opcua_StatusCode code);
/** Native uncertain severity predicate. */
cpkt_opcua_Boolean
cpkt_opcua_StatusCode_isUncertain(cpkt_opcua_StatusCode code);
/** Native bad severity predicate. */
cpkt_opcua_Boolean cpkt_opcua_StatusCode_isBad(cpkt_opcua_StatusCode code);
/** Compare native top 16 status bits, ignoring info bits. */
cpkt_opcua_Boolean cpkt_opcua_StatusCode_equalTop(cpkt_opcua_StatusCode a,
                                                  cpkt_opcua_StatusCode b);
/** Borrow the upstream static status name; never free it. */
const char *cpkt_opcua_StatusCode_name(cpkt_opcua_StatusCode code);
/** Allocate a native-owned copy, without a trailing NUL. NULL input or
 * allocation failure returns a null String, as upstream. Clear to release. */
cpkt_opcua_String cpkt_opcua_String_fromChars(const char *text);
/** Native emptiness predicate (zero length OR NULL data). */
cpkt_opcua_Boolean cpkt_opcua_String_isEmpty(const cpkt_opcua_String *value);
/** Append using native realloc, retaining its status and ownership. Destination
 * must own its buffer; source must not alias that buffer. An error preserves
 * destination. Guards length overflow before invoking native code. */
cpkt_opcua_StatusCode cpkt_opcua_String_append(cpkt_opcua_String *value,
                                               cpkt_opcua_String suffix);
/** Allocate a zeroed buffer into an empty ByteString. Zero length produces the
 * empty sentinel; allocation failure produces a null ByteString. */
cpkt_opcua_StatusCode
cpkt_opcua_ByteString_allocBuffer(cpkt_opcua_ByteString *value, size_t length);
/** Native base64 codec. Input is borrowed; output must be empty and owns its
 * buffer on success. NULL input, NULL data, and zero length all encode as an
 * empty null String, preserving native behavior. */
cpkt_opcua_StatusCode
cpkt_opcua_ByteString_toBase64(const cpkt_opcua_ByteString *value,
                               cpkt_opcua_String *output);
/** Decode base64 into an empty owned ByteString. Input is borrowed. */
cpkt_opcua_StatusCode
cpkt_opcua_ByteString_fromBase64(cpkt_opcua_ByteString *output,
                                 const cpkt_opcua_String *value);
/** Zero writable bytes through the native secure-memory helper; no allocation
 * or ownership transfer. NULL input is harmless. */
void cpkt_opcua_ByteString_memZero(cpkt_opcua_ByteString *value);
/** Hash borrowed raw bytes through the native implementation. NULL is valid
 * only for length zero; invalid input returns zero. */
cpkt_opcua_UInt32 cpkt_opcua_ByteString_hash(cpkt_opcua_UInt32 initial,
                                             const cpkt_opcua_Byte *bytes,
                                             size_t length);
#ifdef __cplusplus
}
#endif
#endif
