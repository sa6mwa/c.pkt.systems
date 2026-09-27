#include "opcua_internal.h"
#include "opcua_types_internal.h"
#include <limits.h>
#include <open62541/client_highlevel_async.h>
#include <open62541/client_subscriptions.h>
#include <open62541/util.h>
#include <string.h>

typedef struct cpkt_async_base cpkt_async_base;
struct cpkt_async_base {
  cpkt_opcua_client *client;
  void *user;
  const cpkt_opcua_Type *type;
  void (*deliver)(cpkt_async_base *, UA_UInt32, UA_StatusCode, const void *);
  UA_StatusCode (*invoke)(UA_Client *, const void *, cpkt_async_base *,
                          UA_UInt32 *);
};
static void cpkt_async_response(UA_Client *, void *, UA_UInt32, void *);
static UA_StatusCode cpkt_typed_async(const void *, const cpkt_opcua_Type *,
                                      cpkt_async_base *, cpkt_opcua_UInt32 *);

typedef char cpkt_word_is_32[(sizeof(cpkt_opcua_UInt32) == 4 &&
                              UINT_MAX == 0xffffffffU && CHAR_BIT == 8)
                                 ? 1
                                 : -1];
typedef char cpkt_short_is_16[(sizeof(cpkt_opcua_UInt16) == 2) ? 1 : -1];
typedef char cpkt_guid_layout[(sizeof(cpkt_opcua_Guid) == sizeof(UA_Guid) &&
                               offsetof(cpkt_opcua_Guid, data4) ==
                                   offsetof(UA_Guid, data4))
                                  ? 1
                                  : -1];

static UA_StatusCode cpkt_convert(const void *, void *, const cpkt_opcua_Type *,
                                  int, unsigned int);
static UA_StatusCode
cpkt_typed_service(cpkt_opcua_client *, const void *, void *,
                   const cpkt_opcua_Type *, const cpkt_opcua_Type *,
                   void (*)(UA_Client *, const void *, void *));
#include "opcua_types_metadata.inc"

/** Implements the ownership and conversion contract in <cpkt/opcua_types.h>. */
const cpkt_opcua_Type *cpkt_opcua_type_at(size_t index) {
  return index < CPKT_OPCUA_TYPES_COUNT ? &cpkt_types[index] : NULL;
}
/** Implements the ownership and conversion contract in <cpkt/opcua_types.h>. */
const char *cpkt_opcua_type_name(const cpkt_opcua_Type *type) {
  return type ? type->name : NULL;
}
/** Implements the ownership and conversion contract in <cpkt/opcua_types.h>. */
size_t cpkt_opcua_type_size(const cpkt_opcua_Type *type) {
  return type ? type->size : 0;
}
/** Implements the ownership and conversion contract in <cpkt/opcua_types.h>. */
void cpkt_opcua_type_init(void *value, const cpkt_opcua_Type *type) {
  if (value && type)
    memset(value, 0, type->size);
}
/** Implements the ownership and conversion contract in <cpkt/opcua_types.h>. */
void *cpkt_opcua_type_new(const cpkt_opcua_Type *type) {
  return type ? UA_calloc(1, type->size) : NULL;
}
static const cpkt_opcua_Type *cpkt_from_native(const UA_DataType *type) {
  size_t i;
  for (i = 0; i < CPKT_OPCUA_TYPES_COUNT; ++i)
    if (cpkt_types[i].native == type)
      return &cpkt_types[i];
  return NULL;
}
static const cpkt_opcua_Type *cpkt_valid_type(const cpkt_opcua_Type *type) {
  size_t i;
  for (i = 0; i < CPKT_OPCUA_TYPES_COUNT; ++i)
    if (&cpkt_types[i] == type)
      return type;
  return NULL;
}
static void cpkt_clear_array(void *data, size_t length,
                             const cpkt_opcua_Type *type) {
  size_t i;
  if (!data || data == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL)
    return;
  for (i = 0; i < length; ++i)
    cpkt_opcua_type_clear((char *)data + i * type->size, type);
  UA_free(data);
}
/** Implements generated array allocation using the upstream allocator. */
void *cpkt_opcua_array_new(size_t length, const cpkt_opcua_Type *type) {
  if (!cpkt_valid_type(type) || length > (size_t)-1 / type->size)
    return NULL;
  return length ? UA_calloc(length, type->size)
                : CPKT_OPCUA_EMPTY_ARRAY_SENTINEL;
}
/** Implements generated array ownership in the public type contract. */
void cpkt_opcua_array_delete(void *array, size_t length,
                             const cpkt_opcua_Type *type) {
  if (cpkt_valid_type(type))
    cpkt_clear_array(array, length, type);
}
/** Implements full nested C89 array copies without changing the source. */
cpkt_opcua_StatusCode cpkt_opcua_array_copy(const void *source, size_t length,
                                            void **destination,
                                            const cpkt_opcua_Type *type) {
  size_t i;
  UA_StatusCode status;
  if (!destination)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  *destination = NULL;
  if (!cpkt_valid_type(type) || (!source && length) ||
      (source == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL && length))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!source)
    return 0;
  *destination = cpkt_opcua_array_new(length, type);
  if (!*destination)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  for (i = 0; i < length; ++i) {
    status = cpkt_opcua_type_copy((const char *)source + i * type->size,
                                  (char *)*destination + i * type->size, type);
    if (status) {
      cpkt_clear_array(*destination, length, type);
      *destination = NULL;
      return status;
    }
  }
  return 0;
}
static UA_StatusCode cpkt_array(const void *src, size_t length, void **dst,
                                const cpkt_opcua_Type *type, int to_native,
                                unsigned int depth) {
  size_t i, in_size, out_size;
  UA_StatusCode status;
  if (!src)
    return length ? UA_STATUSCODE_BADINVALIDARGUMENT : UA_STATUSCODE_GOOD;
  if (length == 0) {
    *dst = UA_EMPTY_ARRAY_SENTINEL;
    return UA_STATUSCODE_GOOD;
  }
  if (src == UA_EMPTY_ARRAY_SENTINEL)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  in_size = to_native ? type->size : type->native->memSize;
  out_size = to_native ? type->native->memSize : type->size;
  if (length > (size_t)-1 / out_size || length > (size_t)-1 / in_size)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  *dst = UA_calloc(length, out_size);
  if (!*dst)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  for (i = 0; i < length; ++i) {
    status =
        cpkt_convert((const char *)src + i * in_size,
                     (char *)*dst + i * out_size, type, to_native, depth + 1);
    if (status)
      return status;
  }
  return UA_STATUSCODE_GOOD;
}
static UA_StatusCode cpkt_string(const void *src, void *dst, int to_native,
                                 unsigned int depth) {
  const cpkt_opcua_String *c;
  const UA_String *u;
  size_t length;
  const void *data;
  void *out = NULL;
  UA_StatusCode status;
  if (to_native) {
    c = (const cpkt_opcua_String *)src;
    length = c->length;
    data = c->data;
  } else {
    u = (const UA_String *)src;
    length = u->length;
    data = u->data;
  }
  status = cpkt_array(data, length, &out, &cpkt_types[CPKT_OPCUA_TYPES_BYTE],
                      to_native, depth);
  if (to_native) {
    ((UA_String *)dst)->length = out ? length : 0;
    ((UA_String *)dst)->data = (UA_Byte *)out;
  } else {
    ((cpkt_opcua_String *)dst)->length = out ? length : 0;
    ((cpkt_opcua_String *)dst)->data = (cpkt_opcua_Byte *)out;
  }
  return status;
}
#define CONVERT_FIELD(C, U, field, type)                                       \
  do {                                                                         \
    status = cpkt_convert(                                                     \
        to_native ? (const void *)&(C)->field : (const void *)&(U)->field,     \
        to_native ? (void *)&(U)->field : (void *)&(C)->field,                 \
        &cpkt_types[CPKT_OPCUA_TYPES_##type], to_native, depth + 1);           \
    if (status)                                                                \
      return status;                                                           \
  } while (0)
#define COPY_FIELD(C, U, field)                                                \
  do {                                                                         \
    if (to_native)                                                             \
      (U)->field = (C)->field;                                                 \
    else                                                                       \
      (C)->field = (U)->field;                                                 \
  } while (0)

static UA_StatusCode cpkt_convert(const void *src, void *dst,
                                  const cpkt_opcua_Type *type, int to_native,
                                  unsigned int depth) {
  UA_StatusCode status = UA_STATUSCODE_GOOD;
  size_t i, length, in_offset, out_offset, selected = 0;
  unsigned int switch_field;
  const cpkt_opcua_type_member *m;
  const cpkt_opcua_Type *member_type;
  const void *data;
  void *out;
  if (depth > 128)
    return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;
  if (!src || !dst || !type)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  switch (type->kind) {
  case 0:
    if (to_native)
      *(UA_Boolean *)dst = (*(const cpkt_opcua_Boolean *)src != 0);
    else
      *(cpkt_opcua_Boolean *)dst = (*(const UA_Boolean *)src != 0);
    return 0;
  case 7:
  case 8:
  case 12: {
    UA_UInt64 bits;
    cpkt_opcua_UInt64 *c = (cpkt_opcua_UInt64 *)(to_native ? (void *)src : dst);
    if (to_native) {
      bits = ((UA_UInt64)c->high32 << 32) | c->low32;
      memcpy(dst, &bits, sizeof(bits));
    } else {
      memcpy(&bits, src, sizeof(bits));
      c->high32 = (cpkt_opcua_UInt32)(bits >> 32);
      c->low32 = (cpkt_opcua_UInt32)bits;
    }
    return 0;
  }
  case 11:
  case 14:
  case 15:
    return cpkt_string(src, dst, to_native, depth);
  case 16: {
    cpkt_opcua_NodeId *c = (cpkt_opcua_NodeId *)(to_native ? (void *)src : dst);
    UA_NodeId *u = (UA_NodeId *)(to_native ? dst : (void *)src);
    COPY_FIELD(c, u, namespaceIndex);
    if (to_native)
      u->identifierType = (enum UA_NodeIdType)c->identifierType;
    else
      c->identifierType = (cpkt_opcua_NodeIdType)u->identifierType;
    switch (u->identifierType) {
    case UA_NODEIDTYPE_NUMERIC:
      COPY_FIELD(c, u, identifier.numeric);
      break;
    case UA_NODEIDTYPE_GUID:
      CONVERT_FIELD(c, u, identifier.guid, GUID);
      break;
    case UA_NODEIDTYPE_STRING:
      CONVERT_FIELD(c, u, identifier.string, STRING);
      break;
    case UA_NODEIDTYPE_BYTESTRING:
      CONVERT_FIELD(c, u, identifier.byteString, BYTESTRING);
      break;
    default:
      return UA_STATUSCODE_BADINVALIDARGUMENT;
    }
    return 0;
  }
  case 17: {
    cpkt_opcua_ExpandedNodeId *c =
        (cpkt_opcua_ExpandedNodeId *)(to_native ? (void *)src : dst);
    UA_ExpandedNodeId *u = (UA_ExpandedNodeId *)(to_native ? dst : (void *)src);
    CONVERT_FIELD(c, u, nodeId, NODEID);
    CONVERT_FIELD(c, u, namespaceUri, STRING);
    COPY_FIELD(c, u, serverIndex);
    return 0;
  }
  case 19: {
    cpkt_opcua_QualifiedName *c =
        (cpkt_opcua_QualifiedName *)(to_native ? (void *)src : dst);
    UA_QualifiedName *u = (UA_QualifiedName *)(to_native ? dst : (void *)src);
    COPY_FIELD(c, u, namespaceIndex);
    CONVERT_FIELD(c, u, name, STRING);
    return 0;
  }
  case 20: {
    cpkt_opcua_LocalizedText *c =
        (cpkt_opcua_LocalizedText *)(to_native ? (void *)src : dst);
    UA_LocalizedText *u = (UA_LocalizedText *)(to_native ? dst : (void *)src);
    CONVERT_FIELD(c, u, locale, STRING);
    CONVERT_FIELD(c, u, text, STRING);
    return 0;
  }
  case 21: {
    cpkt_opcua_ExtensionObject *c =
        (cpkt_opcua_ExtensionObject *)(to_native ? (void *)src : dst);
    UA_ExtensionObject *u =
        (UA_ExtensionObject *)(to_native ? dst : (void *)src);
    int encoding = to_native ? (int)c->encoding : (int)u->encoding;
    if (encoding < 0 || encoding > 4)
      return UA_STATUSCODE_BADINVALIDARGUMENT;
    if (encoding < 3) {
      if (to_native)
        u->encoding = (UA_ExtensionObjectEncoding)encoding;
      else
        c->encoding = (cpkt_opcua_ExtensionObjectEncoding)encoding;
      CONVERT_FIELD(c, u, content.encoded.typeId, NODEID);
      CONVERT_FIELD(c, u, content.encoded.body, BYTESTRING);
      return 0;
    }
    member_type = to_native ? cpkt_valid_type(c->content.decoded.type)
                            : cpkt_from_native(u->content.decoded.type);
    if (!member_type)
      return UA_STATUSCODE_BADTYPEMISMATCH;
    out = UA_calloc(1, to_native ? member_type->native->memSize
                                 : member_type->size);
    if (!out)
      return UA_STATUSCODE_BADOUTOFMEMORY;
    if (to_native) {
      u->encoding = UA_EXTENSIONOBJECT_DECODED;
      u->content.decoded.type = member_type->native;
      u->content.decoded.data = out;
      data = c->content.decoded.data;
    } else {
      c->encoding = CPKT_OPCUA_EXTENSIONOBJECT_DECODED;
      c->content.decoded.type = member_type;
      c->content.decoded.data = out;
      data = u->content.decoded.data;
    }
    return cpkt_convert(data, out, member_type, to_native, depth + 1);
  }
  case 22: {
    cpkt_opcua_DataValue *c =
        (cpkt_opcua_DataValue *)(to_native ? (void *)src : dst);
    UA_DataValue *u = (UA_DataValue *)(to_native ? dst : (void *)src);
    CONVERT_FIELD(c, u, value, VARIANT);
    CONVERT_FIELD(c, u, sourceTimestamp, DATETIME);
    CONVERT_FIELD(c, u, serverTimestamp, DATETIME);
    COPY_FIELD(c, u, sourcePicoseconds);
    COPY_FIELD(c, u, serverPicoseconds);
    COPY_FIELD(c, u, status);
    COPY_FIELD(c, u, hasValue);
    COPY_FIELD(c, u, hasStatus);
    COPY_FIELD(c, u, hasSourceTimestamp);
    COPY_FIELD(c, u, hasServerTimestamp);
    COPY_FIELD(c, u, hasSourcePicoseconds);
    COPY_FIELD(c, u, hasServerPicoseconds);
    return 0;
  }
  case 23: {
    cpkt_opcua_Variant *c =
        (cpkt_opcua_Variant *)(to_native ? (void *)src : dst);
    UA_Variant *u = (UA_Variant *)(to_native ? dst : (void *)src);
    if (to_native ? c->type == NULL : u->type == NULL) {
      if (to_native ? c->data != NULL : u->data != NULL)
        return UA_STATUSCODE_BADINVALIDARGUMENT;
      return 0;
    }
    member_type =
        to_native ? cpkt_valid_type(c->type) : cpkt_from_native(u->type);
    if (!member_type)
      return UA_STATUSCODE_BADTYPEMISMATCH;
    length = to_native ? c->arrayLength : u->arrayLength;
    data = to_native ? c->data : u->data;
    if (to_native) {
      u->type = member_type->native;
      u->arrayLength = length;
    } else {
      c->type = member_type;
      c->arrayLength = length;
    }
    if (!length && data && data != UA_EMPTY_ARRAY_SENTINEL)
      length = 1;
    out = NULL;
    status = cpkt_array(data, length, &out, member_type, to_native, depth);
    if (to_native) {
      u->data = out;
      if (!out)
        u->arrayLength = 0;
    } else {
      c->data = out;
      if (!out)
        c->arrayLength = 0;
    }
    if (status)
      return status;
    length = to_native ? c->arrayDimensionsSize : u->arrayDimensionsSize;
    data = to_native ? (const void *)c->arrayDimensions
                     : (const void *)u->arrayDimensions;
    out = NULL;
    status = cpkt_array(data, length, &out,
                        &cpkt_types[CPKT_OPCUA_TYPES_UINT32], to_native, depth);
    if (to_native) {
      u->arrayDimensionsSize = out ? length : 0;
      u->arrayDimensions = (UA_UInt32 *)out;
    } else {
      c->arrayDimensionsSize = out ? length : 0;
      c->arrayDimensions = (cpkt_opcua_UInt32 *)out;
    }
    return status;
  }
  case 24: {
    cpkt_opcua_DiagnosticInfo *c =
        (cpkt_opcua_DiagnosticInfo *)(to_native ? (void *)src : dst);
    UA_DiagnosticInfo *u = (UA_DiagnosticInfo *)(to_native ? dst : (void *)src);
    COPY_FIELD(c, u, hasSymbolicId);
    COPY_FIELD(c, u, hasNamespaceUri);
    COPY_FIELD(c, u, hasLocalizedText);
    COPY_FIELD(c, u, hasLocale);
    COPY_FIELD(c, u, hasAdditionalInfo);
    COPY_FIELD(c, u, hasInnerStatusCode);
    COPY_FIELD(c, u, hasInnerDiagnosticInfo);
    COPY_FIELD(c, u, symbolicId);
    COPY_FIELD(c, u, namespaceUri);
    COPY_FIELD(c, u, localizedText);
    COPY_FIELD(c, u, locale);
    COPY_FIELD(c, u, innerStatusCode);
    CONVERT_FIELD(c, u, additionalInfo, STRING);
    data = to_native ? (const void *)c->innerDiagnosticInfo
                     : (const void *)u->innerDiagnosticInfo;
    if (!data)
      return 0;
    member_type = &cpkt_types[CPKT_OPCUA_TYPES_DIAGNOSTICINFO];
    out = UA_calloc(1, to_native ? sizeof(UA_DiagnosticInfo)
                                 : sizeof(cpkt_opcua_DiagnosticInfo));
    if (!out)
      return UA_STATUSCODE_BADOUTOFMEMORY;
    if (to_native)
      u->innerDiagnosticInfo = (UA_DiagnosticInfo *)out;
    else
      c->innerDiagnosticInfo = (cpkt_opcua_DiagnosticInfo *)out;
    return cpkt_convert(data, out, member_type, to_native, depth + 1);
  }
  case 29:
    memcpy(&switch_field, src, sizeof(switch_field));
    if (switch_field > type->members_size)
      return UA_STATUSCODE_BADINVALIDARGUMENT;
    memcpy(dst, &switch_field, sizeof(switch_field));
    if (!switch_field)
      return 0;
    selected = switch_field - 1;
    /* FALLTHROUGH */
  case 27:
    for (i = 0; i < type->members_size; ++i) {
      if (type->kind == 29 && i != selected)
        continue;
      m = &type->members[i];
      member_type = &cpkt_types[m->type_index];
      in_offset = to_native ? m->c_offset : m->ua_offset;
      out_offset = to_native ? m->ua_offset : m->c_offset;
      if (m->is_array || m->is_optional) {
        memcpy(&data, (const char *)src + in_offset, sizeof(data));
        out = NULL;
        if (m->is_array) {
          memcpy(&length,
                 (const char *)src +
                     (to_native ? m->c_size_offset : m->ua_size_offset),
                 sizeof(length));
          status =
              cpkt_array(data, length, &out, member_type, to_native, depth);
          if (!out)
            length = 0;
          memcpy((char *)dst +
                     (to_native ? m->ua_size_offset : m->c_size_offset),
                 &length, sizeof(length));
        } else if (data) {
          out = UA_calloc(1, to_native ? member_type->native->memSize
                                       : member_type->size);
          status =
              out ? cpkt_convert(data, out, member_type, to_native, depth + 1)
                  : UA_STATUSCODE_BADOUTOFMEMORY;
        }
        memcpy((char *)dst + out_offset, &out, sizeof(out));
      } else
        status = cpkt_convert((const char *)src + in_offset,
                              (char *)dst + out_offset, member_type, to_native,
                              depth + 1);
      if (status)
        return status;
    }
    return 0;
  default:
    if (type->size != type->native->memSize)
      return UA_STATUSCODE_BADTYPEMISMATCH;
    memcpy(dst, src, type->size);
    return 0;
  }
}

/** Implements the ownership and conversion contract in <cpkt/opcua_types.h>. */
void cpkt_opcua_type_clear(void *value, const cpkt_opcua_Type *type) {
  size_t i, length, selected = 0;
  unsigned int switch_field;
  void *data;
  const cpkt_opcua_type_member *m;
  if (!value || !type)
    return;
  switch (type->kind) {
  case 11:
  case 14:
  case 15:
    UA_free(((cpkt_opcua_String *)value)->data == UA_EMPTY_ARRAY_SENTINEL
                ? NULL
                : ((cpkt_opcua_String *)value)->data);
    break;
  case 16: {
    cpkt_opcua_NodeId *v = (cpkt_opcua_NodeId *)value;
    if (v->identifierType == CPKT_OPCUA_NODEIDTYPE_STRING ||
        v->identifierType == CPKT_OPCUA_NODEIDTYPE_BYTESTRING)
      cpkt_opcua_type_clear(&v->identifier.string,
                            &cpkt_types[CPKT_OPCUA_TYPES_STRING]);
    break;
  }
  case 17: {
    cpkt_opcua_ExpandedNodeId *v = (cpkt_opcua_ExpandedNodeId *)value;
    cpkt_opcua_type_clear(&v->nodeId, &cpkt_types[CPKT_OPCUA_TYPES_NODEID]);
    cpkt_opcua_type_clear(&v->namespaceUri,
                          &cpkt_types[CPKT_OPCUA_TYPES_STRING]);
    break;
  }
  case 19:
    cpkt_opcua_type_clear(&((cpkt_opcua_QualifiedName *)value)->name,
                          &cpkt_types[CPKT_OPCUA_TYPES_STRING]);
    break;
  case 20: {
    cpkt_opcua_LocalizedText *v = (cpkt_opcua_LocalizedText *)value;
    cpkt_opcua_type_clear(&v->locale, &cpkt_types[CPKT_OPCUA_TYPES_STRING]);
    cpkt_opcua_type_clear(&v->text, &cpkt_types[CPKT_OPCUA_TYPES_STRING]);
    break;
  }
  case 21: {
    cpkt_opcua_ExtensionObject *v = (cpkt_opcua_ExtensionObject *)value;
    if (v->encoding < CPKT_OPCUA_EXTENSIONOBJECT_DECODED) {
      cpkt_opcua_type_clear(&v->content.encoded.typeId,
                            &cpkt_types[CPKT_OPCUA_TYPES_NODEID]);
      cpkt_opcua_type_clear(&v->content.encoded.body,
                            &cpkt_types[CPKT_OPCUA_TYPES_BYTESTRING]);
    } else if (v->encoding == CPKT_OPCUA_EXTENSIONOBJECT_DECODED &&
               cpkt_valid_type(v->content.decoded.type))
      cpkt_opcua_type_delete(v->content.decoded.data, v->content.decoded.type);
    break;
  }
  case 22:
    cpkt_opcua_type_clear(&((cpkt_opcua_DataValue *)value)->value,
                          &cpkt_types[CPKT_OPCUA_TYPES_VARIANT]);
    break;
  case 23: {
    cpkt_opcua_Variant *v = (cpkt_opcua_Variant *)value;
    if (v->storageType == CPKT_OPCUA_VARIANT_DATA_NODELETE)
      break;
    length = v->arrayLength;
    if (!length && v->data && v->data != UA_EMPTY_ARRAY_SENTINEL)
      length = 1;
    if (cpkt_valid_type(v->type))
      cpkt_clear_array(v->data, length, v->type);
    cpkt_clear_array(v->arrayDimensions, v->arrayDimensionsSize,
                     &cpkt_types[CPKT_OPCUA_TYPES_UINT32]);
    break;
  }
  case 24: {
    cpkt_opcua_DiagnosticInfo *v = (cpkt_opcua_DiagnosticInfo *)value;
    cpkt_opcua_type_clear(&v->additionalInfo,
                          &cpkt_types[CPKT_OPCUA_TYPES_STRING]);
    cpkt_opcua_type_delete(v->innerDiagnosticInfo, type);
    break;
  }
  case 29:
    memcpy(&switch_field, value, sizeof(switch_field));
    if (!switch_field || switch_field > type->members_size)
      break;
    selected = switch_field - 1;
    /* FALLTHROUGH */
  case 27:
    for (i = 0; i < type->members_size; ++i) {
      if (type->kind == 29 && i != selected)
        continue;
      m = &type->members[i];
      if (m->is_array || m->is_optional) {
        memcpy(&data, (char *)value + m->c_offset, sizeof(data));
        if (m->is_array) {
          memcpy(&length, (char *)value + m->c_size_offset, sizeof(length));
          cpkt_clear_array(data, length, &cpkt_types[m->type_index]);
        } else
          cpkt_opcua_type_delete(data, &cpkt_types[m->type_index]);
      } else
        cpkt_opcua_type_clear((char *)value + m->c_offset,
                              &cpkt_types[m->type_index]);
    }
    break;
  default:
    break;
  }
  memset(value, 0, type->size);
}
/** Implements the ownership and conversion contract in <cpkt/opcua_types.h>. */
void cpkt_opcua_type_delete(void *value, const cpkt_opcua_Type *type) {
  if (!value)
    return;
  cpkt_opcua_type_clear(value, type);
  UA_free(value);
}
/** Implements the ownership and conversion contract in <cpkt/opcua_types.h>. */
cpkt_opcua_StatusCode cpkt_opcua_type_copy(const void *src, void *dst,
                                           const cpkt_opcua_Type *type) {
  void *native;
  UA_StatusCode status;
  if (!src || !dst || !type || src == dst)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memset(dst, 0, type->size);
  native = UA_new(type->native);
  if (!native)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  status = cpkt_convert(src, native, type, 1, 0);
  if (!status)
    status = cpkt_convert(native, dst, type, 0, 0);
  UA_delete(native, type->native);
  if (status)
    cpkt_opcua_type_clear(dst, type);
  return status;
}
/** Implements the ownership and conversion contract in <cpkt/opcua_types.h>. */
cpkt_opcua_Boolean cpkt_opcua_type_equal(const void *a, const void *b,
                                         const cpkt_opcua_Type *type) {
  cpkt_opcua_Order order;
  return cpkt_opcua_type_order(a, b, type, &order) == 0 &&
         order == CPKT_OPCUA_ORDER_EQ;
}
/** Implements the ownership and conversion contract in <cpkt/opcua_types.h>. */
cpkt_opcua_StatusCode
cpkt_opcua_type_encode_binary(const void *src, const cpkt_opcua_Type *type,
                              cpkt_opcua_ByteString *out) {
  void *native;
  UA_ByteString bytes = UA_BYTESTRING_NULL;
  UA_StatusCode status;
  if (!src || !type || !out)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memset(out, 0, sizeof(*out));
  native = UA_new(type->native);
  if (!native)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  status = cpkt_convert(src, native, type, 1, 0);
  if (!status)
    status = UA_encodeBinary(native, type->native, &bytes, NULL);
  if (!status) {
    out->length = bytes.length;
    out->data = bytes.data;
  } else
    UA_ByteString_clear(&bytes);
  UA_delete(native, type->native);
  return status;
}
/** Implements the ownership and conversion contract in <cpkt/opcua_types.h>. */
cpkt_opcua_StatusCode
cpkt_opcua_type_decode_binary(const cpkt_opcua_ByteString *src, void *dst,
                              const cpkt_opcua_Type *type) {
  void *native;
  UA_ByteString bytes;
  UA_StatusCode status;
  if (!src || !dst || !type || (!src->data && src->length))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memset(dst, 0, type->size);
  bytes.length = src->length;
  bytes.data = src->data;
  native = UA_new(type->native);
  if (!native)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  status = UA_decodeBinary(&bytes, native, type->native, NULL);
  if (!status)
    status = cpkt_convert(native, dst, type, 0, 0);
  UA_delete(native, type->native);
  if (status)
    cpkt_opcua_type_clear(dst, type);
  return status;
}
typedef struct {
  void *request, *response;
  const UA_DataType *request_type, *response_type;
  void (*invoke)(UA_Client *, const void *, void *);
} cpkt_service_call;
static cpkt_opcua_status cpkt_service_native(void *client, void *userdata) {
  cpkt_service_call *call = (cpkt_service_call *)userdata;
  call->invoke((UA_Client *)client, call->request, call->response);
  return 0;
}
static UA_StatusCode
cpkt_typed_service(cpkt_opcua_client *client, const void *request,
                   void *response, const cpkt_opcua_Type *req_type,
                   const cpkt_opcua_Type *resp_type,
                   void (*invoke)(UA_Client *, const void *, void *)) {
  cpkt_service_call call;
  UA_StatusCode status;
  if (!client || !request || !response)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memset(response, 0, resp_type->size);
  call.invoke = invoke;
  call.request_type = req_type->native;
  call.response_type = resp_type->native;
  call.request = UA_new(call.request_type);
  call.response = UA_new(call.response_type);
  status = (!call.request || !call.response)
               ? UA_STATUSCODE_BADOUTOFMEMORY
               : cpkt_convert(request, call.request, req_type, 1, 0);
  if (!status && cpkt_opcua_client_native(client, cpkt_service_native, &call) !=
                     CPKT_OPCUA_OK)
    status = UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!status)
    status = cpkt_convert(call.response, response, resp_type, 0, 0);
  if (call.request)
    UA_delete(call.request, call.request_type);
  if (call.response)
    UA_delete(call.response, call.response_type);
  if (status)
    cpkt_opcua_type_clear(response, resp_type);
  return status;
}

typedef struct {
  cpkt_async_base *context;
  const void *request;
  const UA_DataType *request_type;
  UA_UInt32 request_id;
  UA_StatusCode status;
} cpkt_async_call;
static void cpkt_async_response(UA_Client *client, void *userdata, UA_UInt32 id,
                                void *native_response) {
  cpkt_async_base *context = (cpkt_async_base *)userdata;
  void *response;
  UA_StatusCode status;
  (void)client;
  response = cpkt_opcua_type_new(context->type);
  status = response
               ? cpkt_convert(native_response, response, context->type, 0, 0)
               : UA_STATUSCODE_BADOUTOFMEMORY;
  if (status) {
    cpkt_opcua_type_delete(response, context->type);
    response = NULL;
  }
  context->deliver(context, id, status, response);
  cpkt_opcua_type_delete(response, context->type);
  UA_free(context);
}
static cpkt_opcua_status cpkt_async_native(void *client, void *userdata) {
  cpkt_async_call *call = (cpkt_async_call *)userdata;
  if (call->context->invoke)
    call->status = call->context->invoke((UA_Client *)client, call->request,
                                         call->context, &call->request_id);
  else
    call->status = __UA_Client_AsyncService(
        (UA_Client *)client, call->request, call->request_type,
        cpkt_async_response, call->context->type->native, call->context,
        &call->request_id);
  return 0;
}
static UA_StatusCode cpkt_typed_async(const void *request,
                                      const cpkt_opcua_Type *type,
                                      cpkt_async_base *context,
                                      cpkt_opcua_UInt32 *request_id) {
  cpkt_async_call call;
  void *native;
  UA_StatusCode status;
  if (request_id)
    *request_id = 0;
  memset(&call, 0, sizeof(call));
  native = UA_new(type->native);
  status = native ? cpkt_convert(request, native, type, 1, 0)
                  : UA_STATUSCODE_BADOUTOFMEMORY;
  call.request = native;
  call.request_type = type->native;
  call.context = context;
  if (!status && cpkt_opcua_client_native(context->client, cpkt_async_native,
                                          &call) != CPKT_OPCUA_OK)
    status = UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!status)
    status = call.status;
  if (native)
    UA_delete(native, type->native);
  if (status)
    UA_free(context);
  else if (request_id)
    *request_id = call.request_id;
  return status;
}
typedef struct {
  cpkt_async_base base;
  cpkt_opcua_async_service_fn fn;
} cpkt_generic_async;
static void cpkt_generic_deliver(cpkt_async_base *base, UA_UInt32 id,
                                 UA_StatusCode status, const void *response) {
  cpkt_generic_async *context = (cpkt_generic_async *)base;
  context->fn(base->client, base->user, id, status, response, base->type);
}
/** Implements the asynchronous ownership contract in the public header. */
cpkt_opcua_StatusCode cpkt_opcua_client_service_async(
    cpkt_opcua_client *client, const void *request,
    const cpkt_opcua_Type *request_type, const cpkt_opcua_Type *response_type,
    cpkt_opcua_async_service_fn fn, void *user, cpkt_opcua_UInt32 *request_id) {
  size_t i;
  cpkt_generic_async *context;
  if (request_id)
    *request_id = 0;
  if (!client || !request || !fn || !cpkt_valid_type(request_type) ||
      !cpkt_valid_type(response_type))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  for (i = 0; i < sizeof(cpkt_service_pairs) / sizeof(cpkt_service_pairs[0]);
       ++i)
    if (&cpkt_types[cpkt_service_pairs[i][0]] == request_type &&
        &cpkt_types[cpkt_service_pairs[i][1]] == response_type)
      break;
  if (i == sizeof(cpkt_service_pairs) / sizeof(cpkt_service_pairs[0]))
    return UA_STATUSCODE_BADTYPEMISMATCH;
  context = (cpkt_generic_async *)UA_calloc(1, sizeof(*context));
  if (!context)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  context->base.client = client;
  context->base.user = user;
  context->base.type = response_type;
  context->base.deliver = cpkt_generic_deliver;
  context->fn = fn;
  return cpkt_typed_async(request, request_type, &context->base, request_id);
}
typedef struct {
  UA_UInt32 id, count;
  unsigned int operation;
  UA_StatusCode status;
} cpkt_cancel_call;
static cpkt_opcua_status cpkt_cancel_native(void *client, void *userdata) {
  cpkt_cancel_call *call = (cpkt_cancel_call *)userdata;
  if (call->operation == 0)
    call->status = UA_Client_cancelByRequestId((UA_Client *)client, call->id,
                                               &call->count);
  else if (call->operation == 1)
    call->status = UA_Client_cancelByRequestHandle((UA_Client *)client,
                                                   call->id, &call->count);
  else
    call->status = UA_Client_renewSecureChannel((UA_Client *)client);
  return 0;
}
static UA_StatusCode cpkt_cancel(cpkt_opcua_client *client, UA_UInt32 id,
                                 unsigned int operation,
                                 cpkt_opcua_UInt32 *count) {
  cpkt_cancel_call call;
  memset(&call, 0, sizeof(call));
  if (count)
    *count = 0;
  if (!client)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  call.id = id;
  call.operation = operation;
  if (cpkt_opcua_client_native(client, cpkt_cancel_native, &call) !=
      CPKT_OPCUA_OK)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (count)
    *count = call.count;
  return call.status;
}
/** Implements upstream cancellation without changing callback ownership. */
cpkt_opcua_StatusCode cpkt_opcua_client_cancel_by_request_id(
    cpkt_opcua_client *client, cpkt_opcua_UInt32 id, cpkt_opcua_UInt32 *count) {
  return cpkt_cancel(client, id, 0, count);
}
/** Implements upstream cancellation without changing callback ownership. */
cpkt_opcua_StatusCode cpkt_opcua_client_cancel_by_request_handle(
    cpkt_opcua_client *client, cpkt_opcua_UInt32 id, cpkt_opcua_UInt32 *count) {
  return cpkt_cancel(client, id, 1, count);
}
/** Implements upstream asynchronous SecureChannel renewal. */
cpkt_opcua_StatusCode
cpkt_opcua_client_renew_secure_channel(cpkt_opcua_client *client) {
  return cpkt_cancel(client, 0, 2, NULL);
}

typedef struct {
  const UA_ReadValueId *read;
  const UA_WriteValue *write;
  UA_DataValue response;
  UA_TimestampsToReturn timestamps;
  UA_StatusCode status;
} cpkt_server_call;
static cpkt_opcua_status cpkt_server_typed_native(void *server,
                                                  void *userdata) {
  cpkt_server_call *call = (cpkt_server_call *)userdata;
  if (call->read)
    call->response =
        UA_Server_read((UA_Server *)server, call->read, call->timestamps);
  else
    call->status = UA_Server_write((UA_Server *)server, call->write);
  return 0;
}
/** Implements the ownership and conversion contract in <cpkt/opcua_types.h>. */
cpkt_opcua_StatusCode cpkt_opcua_server_read_typed(
    cpkt_opcua_server *server, const cpkt_opcua_ReadValueId *request,
    cpkt_opcua_TimestampsToReturn timestamps, cpkt_opcua_DataValue *response) {
  UA_ReadValueId native;
  cpkt_server_call call;
  UA_StatusCode status;
  if (!server || !request || !response)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memset(response, 0, sizeof(*response));
  memset(&call, 0, sizeof(call));
  UA_ReadValueId_init(&native);
  status = cpkt_convert(request, &native,
                        &cpkt_types[CPKT_OPCUA_TYPES_READVALUEID], 1, 0);
  call.read = &native;
  call.timestamps = (UA_TimestampsToReturn)timestamps;
  if (!status && cpkt_opcua_server_native(server, cpkt_server_typed_native,
                                          &call) != CPKT_OPCUA_OK)
    status = UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!status)
    status = cpkt_convert(&call.response, response,
                          &cpkt_types[CPKT_OPCUA_TYPES_DATAVALUE], 0, 0);
  UA_ReadValueId_clear(&native);
  UA_DataValue_clear(&call.response);
  if (status)
    cpkt_opcua_DataValue_clear(response);
  return status;
}
/** Implements the ownership and conversion contract in <cpkt/opcua_types.h>. */
cpkt_opcua_StatusCode
cpkt_opcua_server_write_typed(cpkt_opcua_server *server,
                              const cpkt_opcua_WriteValue *request) {
  UA_WriteValue native;
  cpkt_server_call call;
  UA_StatusCode status;
  if (!server || !request)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memset(&call, 0, sizeof(call));
  UA_WriteValue_init(&native);
  status = cpkt_convert(request, &native,
                        &cpkt_types[CPKT_OPCUA_TYPES_WRITEVALUE], 1, 0);
  call.write = &native;
  if (!status && cpkt_opcua_server_native(server, cpkt_server_typed_native,
                                          &call) != CPKT_OPCUA_OK)
    status = UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!status)
    status = call.status;
  UA_WriteValue_clear(&native);
  return status;
}

#include "opcua_callbacks_impl.h"
#include "opcua_plugins_impl.h"

#include "opcua_util_impl.h"
