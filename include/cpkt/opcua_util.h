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
/** Empty map constant; never clear/delete this const object. */
extern const cpkt_opcua_KeyValueMap cpkt_opcua_KEYVALUEMAP_NULL;
/** Allocate an empty owned map; NULL means allocation failure. */
cpkt_opcua_KeyValueMap *cpkt_opcua_KeyValueMap_new(void);
/** Clear owned entries and reset fields. Shallow value payloads remain
 * caller-owned. Do not clear borrowed notification maps. NULL is harmless. */
void cpkt_opcua_KeyValueMap_clear(cpkt_opcua_KeyValueMap *map);
/** Clear and free a map returned by new; NULL is harmless. */
void cpkt_opcua_KeyValueMap_delete(cpkt_opcua_KeyValueMap *map);
/** Native empty predicate; NULL is an empty map. No allocation. */
cpkt_opcua_Boolean
cpkt_opcua_KeyValueMap_isEmpty(const cpkt_opcua_KeyValueMap *map);
/** Native linear lookup with namespace and byte-exact key comparison. */
cpkt_opcua_Boolean
cpkt_opcua_KeyValueMap_contains(const cpkt_opcua_KeyValueMap *map,
                                cpkt_opcua_QualifiedName key);
/** Borrow the actual C89 Variant in the map; NULL if absent. No
 * conversion/copy. Insertion can reallocate the array; removal, replacement and
 * clear invalidate affected views. Do not clear a borrowed result. */
const cpkt_opcua_Variant *
cpkt_opcua_KeyValueMap_get(const cpkt_opcua_KeyValueMap *map,
                           cpkt_opcua_QualifiedName key);
/** Borrow the scalar payload; NULL for absent, array or different type. */
const void *cpkt_opcua_KeyValueMap_getScalar(const cpkt_opcua_KeyValueMap *map,
                                             cpkt_opcua_QualifiedName key,
                                             const cpkt_opcua_Type *type);
/** Upsert a deep copy of key/value; failure preserves the old map. The source
 * may be an existing map value. Caller retains source ownership. */
cpkt_opcua_StatusCode
cpkt_opcua_KeyValueMap_set(cpkt_opcua_KeyValueMap *map,
                           cpkt_opcua_QualifiedName key,
                           const cpkt_opcua_Variant *value);
/** Upsert a copied key and borrowed Variant payload, forcing DATA_NODELETE as
 * the native implementation does. Caller retains payload/dimensions ownership
 * and keeps them alive through use. Value must not alias the map's own slots.
 */
cpkt_opcua_StatusCode
cpkt_opcua_KeyValueMap_setShallow(cpkt_opcua_KeyValueMap *map,
                                  cpkt_opcua_QualifiedName key,
                                  cpkt_opcua_Variant *value);
/** Deep-copy a borrowed scalar into the map; source remains caller-owned. */
cpkt_opcua_StatusCode cpkt_opcua_KeyValueMap_setScalar(
    cpkt_opcua_KeyValueMap *map, cpkt_opcua_QualifiedName key,
    const void *value, const cpkt_opcua_Type *type);
/** Borrow scalar storage with native DATA_NODELETE semantics; keep it alive. */
cpkt_opcua_StatusCode cpkt_opcua_KeyValueMap_setScalarShallow(
    cpkt_opcua_KeyValueMap *map, cpkt_opcua_QualifiedName key, void *value,
    const cpkt_opcua_Type *type);
/** Remove and clear an entry, moving the last entry into its slot. Missing key
 * returns BadNotFound. Native shrink allocation failure still removes the key
 * and returns Good; the surviving array remains valid. */
cpkt_opcua_StatusCode
cpkt_opcua_KeyValueMap_remove(cpkt_opcua_KeyValueMap *map,
                              cpkt_opcua_QualifiedName key);
/** Deep-copy into an empty owned destination. NULL source is empty. */
cpkt_opcua_StatusCode
cpkt_opcua_KeyValueMap_copy(const cpkt_opcua_KeyValueMap *src,
                            cpkt_opcua_KeyValueMap *dst);
/** Deep-copy right-hand entries into left, upserting matches. On any allocation
 * failure both maps remain untouched. NULL right is empty; self-merge is valid.
 */
cpkt_opcua_StatusCode
cpkt_opcua_KeyValueMap_merge(cpkt_opcua_KeyValueMap *lhs,
                             const cpkt_opcua_KeyValueMap *rhs);
/** Public namespace mapping fields. URI/index arrays may be borrowed for use;
 * clear/delete require independently owned facade-allocated arrays and strings.
 * URI lookup marshals String records, borrowing their bytes; allocation failure
 * is reported without changing caller storage. */
typedef struct {
  cpkt_opcua_String *namespaceUris;
  size_t namespaceUrisSize;
  cpkt_opcua_UInt16 *local2remote;
  size_t local2remoteSize;
  cpkt_opcua_UInt16 *remote2local;
  size_t remote2localSize;
} cpkt_opcua_NamespaceMapping;
/** Native calendar fields, including fractional seconds and signed year. */
typedef struct {
  cpkt_opcua_UInt16 nanoSec, microSec, milliSec, sec, min, hour, day, month;
  cpkt_opcua_Int16 year;
} cpkt_opcua_DateTimeStruct;
/** Initialize without freeing previous storage. */
void cpkt_opcua_NamespaceMapping_init(cpkt_opcua_NamespaceMapping *mapping);
/** Allocate an empty mapping; NULL indicates allocation failure. */
cpkt_opcua_NamespaceMapping *cpkt_opcua_NamespaceMapping_new(void);
/** Release owned arrays/URI bytes and reset all fields; NULL is harmless. */
void cpkt_opcua_NamespaceMapping_clear(cpkt_opcua_NamespaceMapping *mapping);
/** Clear and release a mapping created with new; NULL is harmless. */
void cpkt_opcua_NamespaceMapping_delete(cpkt_opcua_NamespaceMapping *mapping);
/** Native index translation; unknown indices return UINT16_MAX - index.
 * NULL mapping is treated as an empty mapping. No allocation. */
cpkt_opcua_UInt16 cpkt_opcua_NamespaceMapping_local2Remote(
    const cpkt_opcua_NamespaceMapping *mapping, cpkt_opcua_UInt16 index);
/** Native reverse translation with the same unknown-index convention. */
cpkt_opcua_UInt16 cpkt_opcua_NamespaceMapping_remote2Local(
    const cpkt_opcua_NamespaceMapping *mapping, cpkt_opcua_UInt16 index);
/** Borrow mapping and URI during lookup; failure leaves index unchanged. */
cpkt_opcua_StatusCode cpkt_opcua_NamespaceMapping_uri2Index(
    const cpkt_opcua_NamespaceMapping *mapping, cpkt_opcua_String uri,
    cpkt_opcua_UInt16 *index);
/** Return a borrowed URI, valid only while its mapping bytes remain alive.
 * Never clear this borrowed result. Failure leaves the output unchanged. */
cpkt_opcua_StatusCode cpkt_opcua_NamespaceMapping_index2Uri(
    const cpkt_opcua_NamespaceMapping *mapping, cpkt_opcua_UInt16 index,
    cpkt_opcua_String *uri);
/** Native parsing into an empty destination. Result retains native failure
 * state; clear after use, including failed parses. Text is borrowed. */
cpkt_opcua_StatusCode cpkt_opcua_NodeId_parse(cpkt_opcua_NodeId *value,
                                              cpkt_opcua_String text);
/** Extended native parsing with a borrowed namespace mapping.
 * Unknown-URI behavior, escaping, and errors remain native. */
cpkt_opcua_StatusCode
cpkt_opcua_NodeId_parseEx(cpkt_opcua_NodeId *value, cpkt_opcua_String text,
                          const cpkt_opcua_NamespaceMapping *mapping);
/** Native printing. Empty output allocates an owned string; otherwise data
 * is writable caller storage and length is capacity. Native length/error
 * changes are preserved. Do not clear a stack/borrowed output buffer. */
cpkt_opcua_StatusCode cpkt_opcua_NodeId_print(const cpkt_opcua_NodeId *value,
                                              cpkt_opcua_String *output);
/** Native extended printing with a borrowed namespace mapping. */
cpkt_opcua_StatusCode
cpkt_opcua_NodeId_printEx(const cpkt_opcua_NodeId *value,
                          cpkt_opcua_String *output,
                          const cpkt_opcua_NamespaceMapping *mapping);
/** Native hash without allocation; NULL value is treated as initialized. */
cpkt_opcua_UInt32 cpkt_opcua_NodeId_hash(const cpkt_opcua_NodeId *value);
/** Native isNull predicate without allocation; NULL is initialized. */
cpkt_opcua_Boolean cpkt_opcua_NodeId_isNull(const cpkt_opcua_NodeId *value);
/** Native total order without cloning identifier bytes or allocating. */
cpkt_opcua_Order cpkt_opcua_NodeId_order(const cpkt_opcua_NodeId *a,
                                         const cpkt_opcua_NodeId *b);
/** Native parsing into an empty destination. Result retains native failure
 * state; clear after use, including failed parses. Text is borrowed. */
cpkt_opcua_StatusCode
cpkt_opcua_ExpandedNodeId_parse(cpkt_opcua_ExpandedNodeId *value,
                                cpkt_opcua_String text);
/** Extended native parsing with borrowed namespace/server URI records.
 * Unknown-URI behavior, escaping, and errors remain native. */
cpkt_opcua_StatusCode cpkt_opcua_ExpandedNodeId_parseEx(
    cpkt_opcua_ExpandedNodeId *value, cpkt_opcua_String text,
    const cpkt_opcua_NamespaceMapping *mapping, size_t server_uris_size,
    const cpkt_opcua_String *server_uris);
/** Native printing. Empty output allocates an owned string; otherwise data
 * is writable caller storage and length is capacity. Native length/error
 * changes are preserved. Do not clear a stack/borrowed output buffer. */
cpkt_opcua_StatusCode
cpkt_opcua_ExpandedNodeId_print(const cpkt_opcua_ExpandedNodeId *value,
                                cpkt_opcua_String *output);
/** Native extended printing with borrowed mapping/server URIs. */
cpkt_opcua_StatusCode cpkt_opcua_ExpandedNodeId_printEx(
    const cpkt_opcua_ExpandedNodeId *value, cpkt_opcua_String *output,
    const cpkt_opcua_NamespaceMapping *mapping, size_t server_uris_size,
    const cpkt_opcua_String *server_uris);
/** Native hash without allocation; NULL value is treated as initialized. */
cpkt_opcua_UInt32
cpkt_opcua_ExpandedNodeId_hash(const cpkt_opcua_ExpandedNodeId *value);
/** Native isLocal predicate without allocation; NULL is initialized. */
cpkt_opcua_Boolean
cpkt_opcua_ExpandedNodeId_isLocal(const cpkt_opcua_ExpandedNodeId *value);
/** Native total order without cloning identifier bytes or allocating. */
cpkt_opcua_Order
cpkt_opcua_ExpandedNodeId_order(const cpkt_opcua_ExpandedNodeId *a,
                                const cpkt_opcua_ExpandedNodeId *b);
/** Native parsing into an empty destination. Result retains native failure
 * state; clear after use, including failed parses. Text is borrowed. */
cpkt_opcua_StatusCode
cpkt_opcua_QualifiedName_parse(cpkt_opcua_QualifiedName *value,
                               cpkt_opcua_String text);
/** Extended native parsing with a borrowed namespace mapping.
 * Unknown-URI behavior, escaping, and errors remain native. */
cpkt_opcua_StatusCode
cpkt_opcua_QualifiedName_parseEx(cpkt_opcua_QualifiedName *value,
                                 cpkt_opcua_String text,
                                 const cpkt_opcua_NamespaceMapping *mapping);
/** Native printing. Empty output allocates an owned string; otherwise data
 * is writable caller storage and length is capacity. Native length/error
 * changes are preserved. Do not clear a stack/borrowed output buffer. */
cpkt_opcua_StatusCode
cpkt_opcua_QualifiedName_print(const cpkt_opcua_QualifiedName *value,
                               cpkt_opcua_String *output);
/** Native extended printing with a borrowed namespace mapping. */
cpkt_opcua_StatusCode
cpkt_opcua_QualifiedName_printEx(const cpkt_opcua_QualifiedName *value,
                                 cpkt_opcua_String *output,
                                 const cpkt_opcua_NamespaceMapping *mapping);
/** Native hash without allocation; NULL value is treated as initialized. */
cpkt_opcua_UInt32
cpkt_opcua_QualifiedName_hash(const cpkt_opcua_QualifiedName *value);
/** Native isNull predicate without allocation; NULL is initialized. */
cpkt_opcua_Boolean
cpkt_opcua_QualifiedName_isNull(const cpkt_opcua_QualifiedName *value);
/** Native GUID parser. Input bytes are borrowed; output receives native state.
 */
cpkt_opcua_StatusCode cpkt_opcua_Guid_parse(cpkt_opcua_Guid *guid,
                                            cpkt_opcua_String text);
/** Native GUID print with the same allocated/caller-buffer contract as NodeId.
 */
cpkt_opcua_StatusCode cpkt_opcua_Guid_print(const cpkt_opcua_Guid *guid,
                                            cpkt_opcua_String *output);
/** Generate through the configured native random facility. */
cpkt_opcua_Guid cpkt_opcua_Guid_random(void);
/** Native wall-clock timestamp, preserving both 32-bit words. */
cpkt_opcua_DateTime cpkt_opcua_DateTime_now(void);
/** Native monotonic timestamp, intended only for measuring durations. */
cpkt_opcua_DateTime cpkt_opcua_DateTime_nowMonotonic(void);
/** Native local UTC offset in 100ns units, as a signed 64-bit bit pattern. */
cpkt_opcua_Int64 cpkt_opcua_DateTime_localTimeUtcOffset(void);
/** Native Unix seconds to OPC UA timestamp conversion. Caller must keep input
 * in upstream's representable arithmetic range; no new saturation policy. */
cpkt_opcua_DateTime cpkt_opcua_DateTime_fromUnixTime(cpkt_opcua_Int64 seconds);
/** Native timestamp to signed Unix seconds, with native rounding/range rules.
 */
cpkt_opcua_Int64 cpkt_opcua_DateTime_toUnixTime(cpkt_opcua_DateTime time);
/** Parse through the native date parser; output receives native parser state.
 */
cpkt_opcua_StatusCode cpkt_opcua_DateTime_parse(cpkt_opcua_DateTime *time,
                                                cpkt_opcua_String text);
/** Native shorthand parser; invalid text or NULL returns zero. */
cpkt_opcua_DateTime cpkt_opcua_DATETIME(const char *text);
/** Extract native UTC calendar fields and fractional seconds. */
cpkt_opcua_DateTimeStruct
cpkt_opcua_DateTime_toStruct(cpkt_opcua_DateTime time);
/** Native calendar conversion, retaining upstream normalization/range rules. */
cpkt_opcua_DateTime
cpkt_opcua_DateTime_fromStruct(cpkt_opcua_DateTimeStruct time);
/** Seed the native per-thread RNG using all 64 seed bits and native time. */
void cpkt_opcua_random_seed(cpkt_opcua_UInt64 seed);
/** Seed the native per-thread RNG deterministically, retaining all 64 bits. */
void cpkt_opcua_random_seed_deterministic(cpkt_opcua_UInt64 seed);
/** Native random UInt32; not a cryptographic entropy source. */
cpkt_opcua_UInt32 cpkt_opcua_UInt32_random(void);
#ifdef __cplusplus
}
#endif
#endif
