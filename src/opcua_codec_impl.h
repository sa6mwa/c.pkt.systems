/* Private synchronous public-codec conversions. No wire codec is implemented
 * here: all binary, JSON and XML operations delegate to open62541. */
#ifndef CPKT_OPCUA_CODEC_IMPL_H
#define CPKT_OPCUA_CODEC_IMPL_H

typedef struct {
  UA_NamespaceMapping mapping;
  UA_String *uris;
  UA_DataTypeArray *types;
  const cpkt_opcua_DataTypeArray *public_types;
  cpkt_type_scope scope;
} cpkt_codec_views;

static void cpkt_codec_views_clear(cpkt_codec_views *views) {
  UA_DataTypeArray *next;
  cpkt_type_scope_leave(&views->scope);
  UA_free(views->mapping.namespaceUris);
  UA_free(views->uris);
  while (views->types) {
    next = views->types->next;
    /* Descriptors borrow their internals from the public descriptor registry.
     */
    UA_free(views->types->types);
    UA_free(views->types);
    views->types = next;
  }
  memset(views, 0, sizeof(*views));
}

static UA_StatusCode cpkt_codec_views_enter(cpkt_codec_views *views) {
  return cpkt_type_scope_enter(&views->scope, views->types,
                               views->public_types);
}

static UA_StatusCode
cpkt_codec_custom_types(const cpkt_opcua_DataTypeArray *source,
                        cpkt_codec_views *views) {
  const cpkt_opcua_DataTypeArray *slow = source, *fast = source;
  UA_DataTypeArray **tail = &views->types, *item;
  const cpkt_opcua_Type *type;
  size_t i;
  views->public_types = source;
  while (fast && fast->next) {
    slow = slow->next;
    fast = fast->next->next;
    if (slow == fast)
      return UA_STATUSCODE_BADINVALIDARGUMENT;
  }
  for (; source; source = source->next) {
    if ((source->typesSize &&
         (!source->types ||
          source->types == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL)) ||
        source->typesSize > (size_t)-1 / sizeof(UA_DataType))
      return UA_STATUSCODE_BADINVALIDARGUMENT;
    item = (UA_DataTypeArray *)UA_calloc(1, sizeof(*item));
    if (!item)
      return UA_STATUSCODE_BADOUTOFMEMORY;
    *tail = item;
    tail = &item->next;
    if (source->typesSize) {
      item->types =
          (UA_DataType *)UA_calloc(source->typesSize, sizeof(UA_DataType));
      if (!item->types)
        return UA_STATUSCODE_BADOUTOFMEMORY;
      item->typesSize = source->typesSize;
      for (i = 0; i < source->typesSize; ++i) {
        type = cpkt_valid_type(source->types[i]);
        if (!type)
          return UA_STATUSCODE_BADINVALIDARGUMENT;
        item->types[i] = *type->native;
      }
    }
  }
  return 0;
}

static UA_StatusCode
cpkt_codec_EncodeBinary_view(const cpkt_opcua_EncodeBinaryOptions *source,
                             UA_EncodeBinaryOptions *destination,
                             cpkt_codec_views *views) {
  UA_StatusCode status;
  memset(destination, 0, sizeof(*destination));
  if (!source)
    return 0;
  status = cpkt_mapping_view(source->namespaceMapping, &views->mapping);
  if (source->namespaceMapping)
    destination->namespaceMapping = &views->mapping;
  return status;
}

static UA_StatusCode
cpkt_codec_DecodeBinary_view(const cpkt_opcua_DecodeBinaryOptions *source,
                             UA_DecodeBinaryOptions *destination,
                             cpkt_codec_views *views) {
  UA_StatusCode status;
  memset(destination, 0, sizeof(*destination));
  if (!source)
    return 0;
  status = cpkt_mapping_view(source->namespaceMapping, &views->mapping);
  if (source->namespaceMapping)
    destination->namespaceMapping = &views->mapping;
  if (!status)
    status = cpkt_codec_custom_types(source->customTypes, views);
  destination->customTypes = views->types;
  destination->callocContext = source->callocContext;
  destination->calloc = source->calloc;
  destination->decodedLength = source->decodedLength;
  return status;
}

static UA_StatusCode
cpkt_codec_EncodeJson_view(const cpkt_opcua_EncodeJsonOptions *source,
                           UA_EncodeJsonOptions *destination,
                           cpkt_codec_views *views) {
  UA_StatusCode status;
  memset(destination, 0, sizeof(*destination));
  if (!source)
    return 0;
  status = cpkt_mapping_view(source->namespaceMapping, &views->mapping);
  if (source->namespaceMapping)
    destination->namespaceMapping = &views->mapping;
  if (!status)
    status = cpkt_uri_views(source->serverUris, source->serverUrisSize,
                            &views->uris);
  destination->serverUris = views->uris;
  destination->serverUrisSize = source->serverUrisSize;
  destination->useReversible = source->useReversible;
  destination->prettyPrint = source->prettyPrint;
  destination->unquotedKeys = source->unquotedKeys;
  destination->stringNodeIds = source->stringNodeIds;
  return status;
}

static UA_StatusCode
cpkt_codec_DecodeJson_view(const cpkt_opcua_DecodeJsonOptions *source,
                           UA_DecodeJsonOptions *destination,
                           cpkt_codec_views *views) {
  UA_StatusCode status;
  memset(destination, 0, sizeof(*destination));
  if (!source)
    return 0;
  status = cpkt_mapping_view(source->namespaceMapping, &views->mapping);
  if (source->namespaceMapping)
    destination->namespaceMapping = &views->mapping;
  if (!status)
    status = cpkt_codec_custom_types(source->customTypes, views);
  destination->customTypes = views->types;
  if (!status)
    status = cpkt_uri_views(source->serverUris, source->serverUrisSize,
                            &views->uris);
  destination->serverUris = views->uris;
  destination->serverUrisSize = source->serverUrisSize;
  destination->decodedLength = source->decodedLength;
  return status;
}

static UA_StatusCode
cpkt_codec_EncodeXml_view(const cpkt_opcua_EncodeXmlOptions *source,
                          UA_EncodeXmlOptions *destination,
                          cpkt_codec_views *views) {
  UA_StatusCode status;
  memset(destination, 0, sizeof(*destination));
  if (!source)
    return 0;
  status = cpkt_mapping_view(source->namespaceMapping, &views->mapping);
  if (source->namespaceMapping)
    destination->namespaceMapping = &views->mapping;
  if (!status)
    status = cpkt_uri_views(source->serverUris, source->serverUrisSize,
                            &views->uris);
  destination->serverUris = views->uris;
  destination->serverUrisSize = source->serverUrisSize;
  return status;
}

static UA_StatusCode
cpkt_codec_DecodeXml_view(const cpkt_opcua_DecodeXmlOptions *source,
                          UA_DecodeXmlOptions *destination,
                          cpkt_codec_views *views) {
  UA_StatusCode status;
  memset(destination, 0, sizeof(*destination));
  if (!source)
    return 0;
  status = cpkt_mapping_view(source->namespaceMapping, &views->mapping);
  if (source->namespaceMapping)
    destination->namespaceMapping = &views->mapping;
  if (!status)
    status = cpkt_codec_custom_types(source->customTypes, views);
  destination->customTypes = views->types;
  if (!status)
    status = cpkt_uri_views(source->serverUris, source->serverUrisSize,
                            &views->uris);
  destination->serverUris = views->uris;
  destination->serverUrisSize = source->serverUrisSize;
  destination->unwrapped = source->unwrapped;
  return status;
}

/* Message conversions must precede the codec wrappers that call them. */
#include "opcua_message_metadata.inc"

#include "opcua_codec_metadata.inc"
#endif
