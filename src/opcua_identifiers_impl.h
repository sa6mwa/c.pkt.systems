/* Native identifier views borrow bytes. Parsed results transfer ownership;
 * printers retain native allocated / caller-buffer behavior. */
static UA_NodeId cpkt_nodeid_view(const cpkt_opcua_NodeId *id) {
  UA_NodeId native;
  UA_NodeId_init(&native);
  if (!id)
    return native;
  native.namespaceIndex = id->namespaceIndex;
  native.identifierType = (enum UA_NodeIdType)id->identifierType;
  switch (id->identifierType) {
  case CPKT_OPCUA_NODEIDTYPE_NUMERIC:
    native.identifier.numeric = id->identifier.numeric;
    break;
  case CPKT_OPCUA_NODEIDTYPE_GUID:
    memcpy(&native.identifier.guid, &id->identifier.guid, sizeof(UA_Guid));
    break;
  case CPKT_OPCUA_NODEIDTYPE_STRING:
    native.identifier.string = cpkt_string_view(&id->identifier.string);
    break;
  case CPKT_OPCUA_NODEIDTYPE_BYTESTRING:
    native.identifier.byteString = cpkt_string_view(&id->identifier.byteString);
    break;
  default:
    break;
  }
  return native;
}
static cpkt_opcua_NodeId cpkt_nodeid_take(UA_NodeId native) {
  cpkt_opcua_NodeId id;
  cpkt_opcua_NodeId_init(&id);
  id.namespaceIndex = native.namespaceIndex;
  id.identifierType = (cpkt_opcua_NodeIdType)native.identifierType;
  switch (native.identifierType) {
  case UA_NODEIDTYPE_NUMERIC:
    id.identifier.numeric = native.identifier.numeric;
    break;
  case UA_NODEIDTYPE_GUID:
    memcpy(&id.identifier.guid, &native.identifier.guid, sizeof(UA_Guid));
    break;
  case UA_NODEIDTYPE_STRING:
    cpkt_string_take(&id.identifier.string, native.identifier.string);
    break;
  case UA_NODEIDTYPE_BYTESTRING:
    cpkt_string_take(&id.identifier.byteString, native.identifier.byteString);
    break;
  default:
    break;
  }
  return id;
}
static UA_ExpandedNodeId
cpkt_expanded_view(const cpkt_opcua_ExpandedNodeId *id) {
  UA_ExpandedNodeId native;
  UA_ExpandedNodeId_init(&native);
  if (id) {
    native.nodeId = cpkt_nodeid_view(&id->nodeId);
    native.namespaceUri = cpkt_string_view(&id->namespaceUri);
    native.serverIndex = id->serverIndex;
  }
  return native;
}
static cpkt_opcua_ExpandedNodeId cpkt_expanded_take(UA_ExpandedNodeId native) {
  cpkt_opcua_ExpandedNodeId id;
  id.nodeId = cpkt_nodeid_take(native.nodeId);
  cpkt_string_take(&id.namespaceUri, native.namespaceUri);
  id.serverIndex = native.serverIndex;
  return id;
}
static UA_QualifiedName
cpkt_qualified_view(const cpkt_opcua_QualifiedName *name) {
  UA_QualifiedName native;
  UA_QualifiedName_init(&native);
  if (name) {
    native.namespaceIndex = name->namespaceIndex;
    native.name = cpkt_string_view(&name->name);
  }
  return native;
}
static void cpkt_mapping_indices(const cpkt_opcua_NamespaceMapping *mapping,
                                 UA_NamespaceMapping *native) {
  memset(native, 0, sizeof(*native));
  if (mapping) {
    native->local2remote = mapping->local2remote;
    native->local2remoteSize = mapping->local2remoteSize;
    native->remote2local = mapping->remote2local;
    native->remote2localSize = mapping->remote2localSize;
  }
}
/* Only the String record array is marshalled; all URI bytes are borrowed. */
static UA_StatusCode
cpkt_mapping_view(const cpkt_opcua_NamespaceMapping *mapping,
                  UA_NamespaceMapping *native) {
  size_t i;
  cpkt_mapping_indices(mapping, native);
  if (!mapping)
    return 0;
  if (((!mapping->namespaceUris ||
        mapping->namespaceUris == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL) &&
       mapping->namespaceUrisSize) ||
      ((!mapping->local2remote ||
        mapping->local2remote == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL) &&
       mapping->local2remoteSize) ||
      ((!mapping->remote2local ||
        mapping->remote2local == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL) &&
       mapping->remote2localSize))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (mapping->namespaceUrisSize > (size_t)-1 / sizeof(UA_String))
    return UA_STATUSCODE_BADOUTOFMEMORY;
  if (!mapping->namespaceUrisSize)
    return 0;
  native->namespaceUris =
      (UA_String *)UA_calloc(mapping->namespaceUrisSize, sizeof(UA_String));
  if (!native->namespaceUris)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  native->namespaceUrisSize = mapping->namespaceUrisSize;
  for (i = 0; i < mapping->namespaceUrisSize; ++i) {
    if (!cpkt_string_valid(&mapping->namespaceUris[i])) {
      UA_free(native->namespaceUris);
      native->namespaceUris = NULL;
      native->namespaceUrisSize = 0;
      return UA_STATUSCODE_BADINVALIDARGUMENT;
    }
    native->namespaceUris[i] = cpkt_string_view(&mapping->namespaceUris[i]);
  }
  return 0;
}
static UA_StatusCode cpkt_uri_views(const cpkt_opcua_String *strings,
                                    size_t size, UA_String **native) {
  size_t i;
  *native = NULL;
  if (!size)
    return 0;
  if (!strings || strings == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (size > (size_t)-1 / sizeof(UA_String))
    return UA_STATUSCODE_BADOUTOFMEMORY;
  *native = (UA_String *)UA_calloc(size, sizeof(UA_String));
  if (!*native)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  for (i = 0; i < size; ++i) {
    if (!cpkt_string_valid(&strings[i])) {
      UA_free(*native);
      *native = NULL;
      return UA_STATUSCODE_BADINVALIDARGUMENT;
    }
    (*native)[i] = cpkt_string_view(&strings[i]);
  }
  return 0;
}
void cpkt_opcua_NamespaceMapping_init(cpkt_opcua_NamespaceMapping *mapping) {
  if (mapping)
    memset(mapping, 0, sizeof(*mapping));
}
cpkt_opcua_NamespaceMapping *cpkt_opcua_NamespaceMapping_new(void) {
  return (cpkt_opcua_NamespaceMapping *)UA_calloc(
      1, sizeof(cpkt_opcua_NamespaceMapping));
}
void cpkt_opcua_NamespaceMapping_clear(cpkt_opcua_NamespaceMapping *mapping) {
  if (!mapping)
    return;
  cpkt_opcua_array_delete(mapping->namespaceUris, mapping->namespaceUrisSize,
                          &cpkt_types[CPKT_OPCUA_TYPES_STRING]);
  cpkt_opcua_array_delete(mapping->local2remote, mapping->local2remoteSize,
                          &cpkt_types[CPKT_OPCUA_TYPES_UINT16]);
  cpkt_opcua_array_delete(mapping->remote2local, mapping->remote2localSize,
                          &cpkt_types[CPKT_OPCUA_TYPES_UINT16]);
  cpkt_opcua_NamespaceMapping_init(mapping);
}
void cpkt_opcua_NamespaceMapping_delete(cpkt_opcua_NamespaceMapping *mapping) {
  cpkt_opcua_NamespaceMapping_clear(mapping);
  UA_free(mapping);
}
cpkt_opcua_UInt16 cpkt_opcua_NamespaceMapping_local2Remote(
    const cpkt_opcua_NamespaceMapping *mapping, cpkt_opcua_UInt16 index) {
  UA_NamespaceMapping native;
  cpkt_mapping_indices(mapping, &native);
  return UA_NamespaceMapping_local2Remote(&native, index);
}
cpkt_opcua_UInt16 cpkt_opcua_NamespaceMapping_remote2Local(
    const cpkt_opcua_NamespaceMapping *mapping, cpkt_opcua_UInt16 index) {
  UA_NamespaceMapping native;
  cpkt_mapping_indices(mapping, &native);
  return UA_NamespaceMapping_remote2Local(&native, index);
}
cpkt_opcua_StatusCode cpkt_opcua_NamespaceMapping_uri2Index(
    const cpkt_opcua_NamespaceMapping *mapping, cpkt_opcua_String uri,
    cpkt_opcua_UInt16 *index) {
  UA_NamespaceMapping native;
  UA_StatusCode status;
  if (!mapping || !index || !cpkt_string_valid(&uri))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  status = cpkt_mapping_view(mapping, &native);
  if (!status)
    status =
        UA_NamespaceMapping_uri2Index(&native, cpkt_string_view(&uri), index);
  UA_free(native.namespaceUris);
  return status;
}
cpkt_opcua_StatusCode cpkt_opcua_NamespaceMapping_index2Uri(
    const cpkt_opcua_NamespaceMapping *mapping, cpkt_opcua_UInt16 index,
    cpkt_opcua_String *uri) {
  UA_NamespaceMapping native;
  UA_String result;
  UA_StatusCode status;
  if (!mapping || !uri)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  status = cpkt_mapping_view(mapping, &native);
  if (!status) {
    status = UA_NamespaceMapping_index2Uri(&native, index, &result);
    if (!status)
      cpkt_string_take(uri, result);
  }
  UA_free(native.namespaceUris);
  return status;
}
cpkt_opcua_StatusCode
cpkt_opcua_NodeId_parseEx(cpkt_opcua_NodeId *id, cpkt_opcua_String text,
                          const cpkt_opcua_NamespaceMapping *mapping) {
  UA_NamespaceMapping view;
  UA_NodeId native;
  UA_StatusCode status;
  if (!id || !cpkt_string_valid(&text))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  UA_NodeId_init(&native);
  status = cpkt_mapping_view(mapping, &view);
  if (!status)
    status = UA_NodeId_parseEx(&native, cpkt_string_view(&text),
                               mapping ? &view : NULL);
  *id = cpkt_nodeid_take(native);
  UA_free(view.namespaceUris);
  return status;
}
cpkt_opcua_StatusCode cpkt_opcua_NodeId_parse(cpkt_opcua_NodeId *id,
                                              cpkt_opcua_String text) {
  return cpkt_opcua_NodeId_parseEx(id, text, NULL);
}
cpkt_opcua_StatusCode
cpkt_opcua_NodeId_printEx(const cpkt_opcua_NodeId *id,
                          cpkt_opcua_String *output,
                          const cpkt_opcua_NamespaceMapping *mapping) {
  UA_NamespaceMapping view;
  UA_NodeId native;
  UA_String result;
  UA_StatusCode status;
  if (!id || !cpkt_string_valid(output))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  native = cpkt_nodeid_view(id);
  result = cpkt_string_view(output);
  status = cpkt_mapping_view(mapping, &view);
  if (!status)
    status = UA_NodeId_printEx(&native, &result, mapping ? &view : NULL);
  cpkt_string_take(output, result);
  UA_free(view.namespaceUris);
  return status;
}
cpkt_opcua_StatusCode cpkt_opcua_NodeId_print(const cpkt_opcua_NodeId *id,
                                              cpkt_opcua_String *output) {
  return cpkt_opcua_NodeId_printEx(id, output, NULL);
}
cpkt_opcua_StatusCode cpkt_opcua_ExpandedNodeId_parseEx(
    cpkt_opcua_ExpandedNodeId *id, cpkt_opcua_String text,
    const cpkt_opcua_NamespaceMapping *mapping, size_t server_uris_size,
    const cpkt_opcua_String *server_uris) {
  UA_NamespaceMapping view;
  UA_ExpandedNodeId native;
  UA_String *uris = NULL;
  UA_StatusCode status;
  if (!id || !cpkt_string_valid(&text))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  UA_ExpandedNodeId_init(&native);
  status = cpkt_mapping_view(mapping, &view);
  if (!status)
    status = cpkt_uri_views(server_uris, server_uris_size, &uris);
  if (!status)
    status = UA_ExpandedNodeId_parseEx(&native, cpkt_string_view(&text),
                                       mapping ? &view : NULL, server_uris_size,
                                       uris);
  *id = cpkt_expanded_take(native);
  UA_free(uris);
  UA_free(view.namespaceUris);
  return status;
}
cpkt_opcua_StatusCode
cpkt_opcua_ExpandedNodeId_parse(cpkt_opcua_ExpandedNodeId *id,
                                cpkt_opcua_String text) {
  return cpkt_opcua_ExpandedNodeId_parseEx(id, text, NULL, 0, NULL);
}
cpkt_opcua_StatusCode cpkt_opcua_ExpandedNodeId_printEx(
    const cpkt_opcua_ExpandedNodeId *id, cpkt_opcua_String *output,
    const cpkt_opcua_NamespaceMapping *mapping, size_t server_uris_size,
    const cpkt_opcua_String *server_uris) {
  UA_NamespaceMapping view;
  UA_ExpandedNodeId native;
  UA_String *uris = NULL, result;
  UA_StatusCode status;
  if (!id || !cpkt_string_valid(output))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  native = cpkt_expanded_view(id);
  result = cpkt_string_view(output);
  status = cpkt_mapping_view(mapping, &view);
  if (!status)
    status = cpkt_uri_views(server_uris, server_uris_size, &uris);
  if (!status)
    status = UA_ExpandedNodeId_printEx(&native, &result, mapping ? &view : NULL,
                                       server_uris_size, uris);
  cpkt_string_take(output, result);
  UA_free(uris);
  UA_free(view.namespaceUris);
  return status;
}
cpkt_opcua_StatusCode
cpkt_opcua_ExpandedNodeId_print(const cpkt_opcua_ExpandedNodeId *id,
                                cpkt_opcua_String *output) {
  return cpkt_opcua_ExpandedNodeId_printEx(id, output, NULL, 0, NULL);
}
cpkt_opcua_StatusCode
cpkt_opcua_QualifiedName_parseEx(cpkt_opcua_QualifiedName *name,
                                 cpkt_opcua_String text,
                                 const cpkt_opcua_NamespaceMapping *mapping) {
  UA_NamespaceMapping view;
  UA_QualifiedName native;
  UA_StatusCode status;
  if (!name || !cpkt_string_valid(&text))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  UA_QualifiedName_init(&native);
  status = cpkt_mapping_view(mapping, &view);
  if (!status)
    status = UA_QualifiedName_parseEx(&native, cpkt_string_view(&text),
                                      mapping ? &view : NULL);
  name->namespaceIndex = native.namespaceIndex;
  cpkt_string_take(&name->name, native.name);
  UA_free(view.namespaceUris);
  return status;
}
cpkt_opcua_StatusCode
cpkt_opcua_QualifiedName_parse(cpkt_opcua_QualifiedName *name,
                               cpkt_opcua_String text) {
  return cpkt_opcua_QualifiedName_parseEx(name, text, NULL);
}
cpkt_opcua_StatusCode
cpkt_opcua_QualifiedName_printEx(const cpkt_opcua_QualifiedName *name,
                                 cpkt_opcua_String *output,
                                 const cpkt_opcua_NamespaceMapping *mapping) {
  UA_NamespaceMapping view;
  UA_QualifiedName native;
  UA_String result;
  UA_StatusCode status;
  if (!name || !cpkt_string_valid(output))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  native = cpkt_qualified_view(name);
  result = cpkt_string_view(output);
  status = cpkt_mapping_view(mapping, &view);
  if (!status)
    status = UA_QualifiedName_printEx(&native, &result, mapping ? &view : NULL);
  cpkt_string_take(output, result);
  UA_free(view.namespaceUris);
  return status;
}
cpkt_opcua_StatusCode
cpkt_opcua_QualifiedName_print(const cpkt_opcua_QualifiedName *name,
                               cpkt_opcua_String *output) {
  return cpkt_opcua_QualifiedName_printEx(name, output, NULL);
}
cpkt_opcua_UInt32 cpkt_opcua_NodeId_hash(const cpkt_opcua_NodeId *id) {
  UA_NodeId native = cpkt_nodeid_view(id);
  return UA_NodeId_hash(&native);
}
cpkt_opcua_Boolean cpkt_opcua_NodeId_isNull(const cpkt_opcua_NodeId *id) {
  UA_NodeId native = cpkt_nodeid_view(id);
  return UA_NodeId_isNull(&native);
}
cpkt_opcua_Order cpkt_opcua_NodeId_order(const cpkt_opcua_NodeId *a,
                                         const cpkt_opcua_NodeId *b) {
  UA_NodeId na = cpkt_nodeid_view(a), nb = cpkt_nodeid_view(b);
  return (cpkt_opcua_Order)UA_NodeId_order(&na, &nb);
}
cpkt_opcua_UInt32
cpkt_opcua_ExpandedNodeId_hash(const cpkt_opcua_ExpandedNodeId *id) {
  UA_ExpandedNodeId native = cpkt_expanded_view(id);
  return UA_ExpandedNodeId_hash(&native);
}
cpkt_opcua_Boolean
cpkt_opcua_ExpandedNodeId_isLocal(const cpkt_opcua_ExpandedNodeId *id) {
  UA_ExpandedNodeId native = cpkt_expanded_view(id);
  return UA_ExpandedNodeId_isLocal(&native);
}
cpkt_opcua_Order
cpkt_opcua_ExpandedNodeId_order(const cpkt_opcua_ExpandedNodeId *a,
                                const cpkt_opcua_ExpandedNodeId *b) {
  UA_ExpandedNodeId na = cpkt_expanded_view(a), nb = cpkt_expanded_view(b);
  return (cpkt_opcua_Order)UA_ExpandedNodeId_order(&na, &nb);
}
cpkt_opcua_UInt32
cpkt_opcua_QualifiedName_hash(const cpkt_opcua_QualifiedName *name) {
  UA_QualifiedName native = cpkt_qualified_view(name);
  return UA_QualifiedName_hash(&native);
}
cpkt_opcua_Boolean
cpkt_opcua_QualifiedName_isNull(const cpkt_opcua_QualifiedName *name) {
  UA_QualifiedName native = cpkt_qualified_view(name);
  return UA_QualifiedName_isNull(&native);
}
cpkt_opcua_StatusCode cpkt_opcua_Guid_parse(cpkt_opcua_Guid *guid,
                                            cpkt_opcua_String text) {
  UA_Guid native = UA_GUID_NULL;
  UA_StatusCode status;
  if (!guid || !cpkt_string_valid(&text))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  status = UA_Guid_parse(&native, cpkt_string_view(&text));
  memcpy(guid, &native, sizeof(native));
  return status;
}
cpkt_opcua_StatusCode cpkt_opcua_Guid_print(const cpkt_opcua_Guid *guid,
                                            cpkt_opcua_String *output) {
  UA_Guid native;
  UA_String result;
  UA_StatusCode status;
  if (!guid || !cpkt_string_valid(output))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memcpy(&native, guid, sizeof(native));
  result = cpkt_string_view(output);
  status = UA_Guid_print(&native, &result);
  cpkt_string_take(output, result);
  return status;
}
cpkt_opcua_Guid cpkt_opcua_Guid_random(void) {
  UA_Guid native = UA_Guid_random();
  cpkt_opcua_Guid result;
  memcpy(&result, &native, sizeof(native));
  return result;
}
static UA_DateTime cpkt_time_native(cpkt_opcua_DateTime time) {
  UA_UInt64 bits = ((UA_UInt64)time.high32 << 32) | time.low32;
  UA_DateTime native;
  memcpy(&native, &bits, sizeof(native));
  return native;
}
static cpkt_opcua_DateTime cpkt_time_take(UA_DateTime native) {
  UA_UInt64 bits;
  cpkt_opcua_DateTime time;
  memcpy(&bits, &native, sizeof(bits));
  time.high32 = (cpkt_opcua_UInt32)(bits >> 32);
  time.low32 = (cpkt_opcua_UInt32)bits;
  return time;
}
cpkt_opcua_DateTime cpkt_opcua_DateTime_now(void) {
  return cpkt_time_take(UA_DateTime_now());
}
cpkt_opcua_DateTime cpkt_opcua_DateTime_nowMonotonic(void) {
  return cpkt_time_take(UA_DateTime_nowMonotonic());
}
cpkt_opcua_Int64 cpkt_opcua_DateTime_localTimeUtcOffset(void) {
  return cpkt_time_take(UA_DateTime_localTimeUtcOffset());
}
cpkt_opcua_DateTime cpkt_opcua_DateTime_fromUnixTime(cpkt_opcua_Int64 seconds) {
  return cpkt_time_take(UA_DateTime_fromUnixTime(cpkt_time_native(seconds)));
}
cpkt_opcua_Int64 cpkt_opcua_DateTime_toUnixTime(cpkt_opcua_DateTime time) {
  return cpkt_time_take(UA_DateTime_toUnixTime(cpkt_time_native(time)));
}
cpkt_opcua_StatusCode cpkt_opcua_DateTime_parse(cpkt_opcua_DateTime *time,
                                                cpkt_opcua_String text) {
  UA_DateTime native = 0;
  UA_StatusCode status;
  if (!time || !cpkt_string_valid(&text))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  status = UA_DateTime_parse(&native, cpkt_string_view(&text));
  *time = cpkt_time_take(native);
  return status;
}
cpkt_opcua_DateTime cpkt_opcua_DATETIME(const char *text) {
  return cpkt_time_take(text ? UA_DATETIME(text) : 0);
}
cpkt_opcua_DateTimeStruct
cpkt_opcua_DateTime_toStruct(cpkt_opcua_DateTime time) {
  UA_DateTimeStruct native = UA_DateTime_toStruct(cpkt_time_native(time));
  cpkt_opcua_DateTimeStruct result;
  result.nanoSec = native.nanoSec;
  result.microSec = native.microSec;
  result.milliSec = native.milliSec;
  result.sec = native.sec;
  result.min = native.min;
  result.hour = native.hour;
  result.day = native.day;
  result.month = native.month;
  result.year = native.year;
  return result;
}
cpkt_opcua_DateTime
cpkt_opcua_DateTime_fromStruct(cpkt_opcua_DateTimeStruct time) {
  UA_DateTimeStruct native;
  native.nanoSec = time.nanoSec;
  native.microSec = time.microSec;
  native.milliSec = time.milliSec;
  native.sec = time.sec;
  native.min = time.min;
  native.hour = time.hour;
  native.day = time.day;
  native.month = time.month;
  native.year = time.year;
  return cpkt_time_take(UA_DateTime_fromStruct(native));
}

void cpkt_opcua_random_seed(cpkt_opcua_UInt64 seed) {
  UA_random_seed(((UA_UInt64)seed.high32 << 32) | seed.low32);
}
void cpkt_opcua_random_seed_deterministic(cpkt_opcua_UInt64 seed) {
  UA_random_seed_deterministic(((UA_UInt64)seed.high32 << 32) | seed.low32);
}
cpkt_opcua_UInt32 cpkt_opcua_UInt32_random(void) { return UA_UInt32_random(); }
