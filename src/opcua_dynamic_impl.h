/* Runtime type metadata uses the same conversion engine as generated types.
 * Native owns schema interpretation; this layer derives public C89 offsets. */
#ifndef CPKT_OPCUA_DYNAMIC_IMPL_H
#define CPKT_OPCUA_DYNAMIC_IMPL_H
struct cpkt_dynamic_array_alignment {
  char prefix;
  size_t length;
};
struct cpkt_dynamic_pointer_alignment {
  char prefix;
  void *pointer;
};
struct cpkt_dynamic_array_layout {
  size_t length;
  void *pointer;
};
typedef char
    cpkt_dynamic_array_contiguous[(offsetof(struct cpkt_dynamic_array_layout,
                                            pointer) == sizeof(size_t) &&
                                   sizeof(struct cpkt_dynamic_array_layout) ==
                                       sizeof(size_t) + sizeof(void *))
                                      ? 1
                                      : -1];

static void cpkt_dynamic_native_delete(UA_DataTypeArray *array) {
  UA_DataTypeArray *next;
  size_t i;
  while (array) {
    next = array->next;
    if (array->cleanup) {
      for (i = 0; i < array->typesSize; ++i)
        UA_DataType_clear(&array->types[i]);
      UA_free(array->types);
      UA_free(array);
    }
    array = next;
  }
}
static void cpkt_dynamic_delete(cpkt_dynamic_types *group) {
  cpkt_dynamic_types **slot;
  cpkt_opcua_DataTypeArray *array, *next;
  size_t i;
  if (!group)
    return;
  if (group->registered) {
    (void)pthread_mutex_lock(&cpkt_dynamic_mutex);
    for (slot = &cpkt_dynamic_registry; *slot; slot = &(*slot)->next)
      if (*slot == group) {
        *slot = group->next;
        break;
      }
    (void)pthread_mutex_unlock(&cpkt_dynamic_mutex);
  }
  for (i = 0; group->members && i < group->count; ++i)
    UA_free(group->members[i]);
  for (array = group->public_arrays; array; array = next) {
    next = array->next;
    UA_free((void *)array->types);
    UA_free(array);
  }
  cpkt_dynamic_native_delete(group->native);
  UA_free(group->aliases);
  UA_free(group->members);
  UA_free(group->types);
  UA_free(group->state);
  UA_free(group);
}
static const cpkt_opcua_Type *cpkt_dynamic_member(cpkt_dynamic_types *group,
                                                  const UA_DataType *native) {
  size_t i;
  /* First preserve exact identity in a graph (including recursive members). */
  for (i = 0; i < group->count; ++i)
    if (group->types[i].native == native ||
        (group->aliases && group->aliases[i] == native))
      return &group->types[i];
  return cpkt_from_native(native);
}
static UA_StatusCode cpkt_dynamic_layout(cpkt_dynamic_types *group,
                                         size_t index, unsigned int depth) {
  cpkt_opcua_Type *type = &group->types[index];
  const UA_DataType *native = type->native;
  const UA_DataTypeMember *member;
  const cpkt_opcua_Type *child;
  cpkt_opcua_type_member *out;
  size_t i, nativeOffset = 0, nativeSize, offset, size, alignment, maxAlignment;
  UA_StatusCode status;
  if (group->state[index] == 2)
    return 0;
  if (group->state[index] || depth > 128)
    return UA_STATUSCODE_BADTYPEDEFINITIONINVALID;
  group->state[index] = 1;
  type->kind = native->typeKind;
  type->name = native->typeName;
  if (native->typeKind <= UA_DATATYPEKIND_DIAGNOSTICINFO ||
      native->typeKind == UA_DATATYPEKIND_ENUM) {
    i = native->typeKind == UA_DATATYPEKIND_ENUM ? CPKT_OPCUA_TYPES_INT32
                                                 : native->typeKind;
    type->size = cpkt_types[i].size;
    type->alignment = cpkt_types[i].alignment;
    if (native->memSize != cpkt_types[i].native->memSize)
      return UA_STATUSCODE_BADTYPEDEFINITIONINVALID;
    group->state[index] = 2;
    return 0;
  }
  if (native->typeKind != UA_DATATYPEKIND_STRUCTURE &&
      native->typeKind != UA_DATATYPEKIND_OPTSTRUCT &&
      native->typeKind != UA_DATATYPEKIND_UNION)
    return UA_STATUSCODE_BADNOTIMPLEMENTED;
  if (native->membersSize && !native->members)
    return UA_STATUSCODE_BADTYPEDEFINITIONINVALID;
  group->members[index] = (cpkt_opcua_type_member *)UA_calloc(
      native->membersSize, sizeof(cpkt_opcua_type_member));
  if (native->membersSize && !group->members[index])
    return UA_STATUSCODE_BADOUTOFMEMORY;
  type->members = group->members[index];
  type->members_size = native->membersSize;
  offset =
      native->typeKind == UA_DATATYPEKIND_UNION ? sizeof(cpkt_opcua_UInt32) : 0;
  maxAlignment = native->typeKind == UA_DATATYPEKIND_UNION
                     ? cpkt_types[CPKT_OPCUA_TYPES_UINT32].alignment
                     : 1;
  for (i = 0; i < native->membersSize; ++i) {
    member = &native->members[i];
    out = &group->members[index][i];
    child = cpkt_dynamic_member(group, member->memberType);
    if (!child)
      return UA_STATUSCODE_BADTYPEMISMATCH;
    out->type = child;
    out->is_array = member->isArray;
    out->is_optional = member->isOptional;
    if (member->isArray) {
      size = sizeof(size_t) + sizeof(void *);
      nativeSize = size;
      alignment = offsetof(struct cpkt_dynamic_array_alignment, length);
      if (alignment < offsetof(struct cpkt_dynamic_pointer_alignment, pointer))
        alignment = offsetof(struct cpkt_dynamic_pointer_alignment, pointer);
    } else if (member->isOptional) {
      size = nativeSize = sizeof(void *);
      alignment = offsetof(struct cpkt_dynamic_pointer_alignment, pointer);
    } else {
      if (child->dynamic_owner == group) {
        status = cpkt_dynamic_layout(group, (size_t)(child - group->types),
                                     depth + 1);
        if (status)
          return status;
      }
      size = child->size;
      nativeSize = child->native->memSize;
      alignment = child->alignment;
    }
    if (!alignment || !size)
      return UA_STATUSCODE_BADTYPEDEFINITIONINVALID;
    if (alignment > maxAlignment)
      maxAlignment = alignment;
    if (native->typeKind == UA_DATATYPEKIND_UNION) {
      nativeOffset = member->padding;
      out->c_size_offset = sizeof(cpkt_opcua_UInt32);
    } else {
      if (nativeOffset > (size_t)-1 - member->padding)
        return UA_STATUSCODE_BADTYPEDEFINITIONINVALID;
      nativeOffset += member->padding;
      out->c_size_offset = offset;
    }
    out->c_size_offset +=
        (alignment - out->c_size_offset % alignment) % alignment;
    out->c_offset = out->c_size_offset + (member->isArray ? sizeof(size_t) : 0);
    out->ua_size_offset = nativeOffset;
    out->ua_offset = nativeOffset + (member->isArray ? sizeof(size_t) : 0);
    if (nativeOffset > native->memSize ||
        nativeSize > native->memSize - nativeOffset ||
        out->c_size_offset > (size_t)-1 - size)
      return UA_STATUSCODE_BADTYPEDEFINITIONINVALID;
    if (native->typeKind == UA_DATATYPEKIND_UNION) {
      if (out->c_size_offset + size > offset)
        offset = out->c_size_offset + size;
    } else {
      offset = out->c_size_offset + size;
      nativeOffset += nativeSize;
    }
  }
  if (offset > (size_t)-1 - maxAlignment)
    return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;
  type->size = offset + (maxAlignment - offset % maxAlignment) % maxAlignment;
  /* A C structure cannot have size zero, even for an empty native definition.
   */
  if (!type->size)
    type->size = 1;
  type->alignment = maxAlignment;
  group->state[index] = 2;
  return 0;
}

/* Takes ownership of native on every path. When exposeArray is false the
 * single resulting descriptor is individually owned. */
static UA_StatusCode cpkt_dynamic_import(UA_DataTypeArray *native,
                                         int exposeArray,
                                         const UA_DataType **aliases,
                                         cpkt_dynamic_types **destination) {
  cpkt_dynamic_types *group;
  UA_DataTypeArray *array;
  cpkt_opcua_DataTypeArray **tail, *publicArray;
  const cpkt_opcua_Type **publicTypes;
  const cpkt_opcua_Type *child;
  size_t count = 0, i, j, index = 0;
  UA_StatusCode status = 0;
  *destination = NULL;
  for (array = native; array; array = array->next) {
    if (count > (size_t)-1 - array->typesSize) {
      UA_free(aliases);
      cpkt_dynamic_native_delete(native);
      return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    count += array->typesSize;
  }
  group = (cpkt_dynamic_types *)UA_calloc(1, sizeof(*group));
  if (!group) {
    UA_free(aliases);
    cpkt_dynamic_native_delete(native);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  group->native = native;
  group->aliases = aliases;
  group->count = count;
  group->types = (cpkt_opcua_Type *)cpkt_conversion_calloc(
      NULL, count, sizeof(cpkt_opcua_Type));
  group->members = (cpkt_opcua_type_member **)cpkt_conversion_calloc(
      NULL, count, sizeof(*group->members));
  group->state = (unsigned char *)UA_calloc(count, 1);
  if (count && (!group->types || !group->members || !group->state)) {
    cpkt_dynamic_delete(group);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  tail = &group->public_arrays;
  for (array = native; array && !status; array = array->next) {
    for (i = 0; i < array->typesSize; ++i) {
      group->types[index + i].native = &array->types[i];
      group->types[index + i].dynamic_owner = group;
    }
    if (exposeArray) {
      publicArray =
          (cpkt_opcua_DataTypeArray *)UA_calloc(1, sizeof(*publicArray));
      if (!publicArray) {
        status = UA_STATUSCODE_BADOUTOFMEMORY;
        break;
      }
      *tail = publicArray;
      tail = &publicArray->next;
      publicArray->cleanup = array->cleanup;
      publicTypes = (const cpkt_opcua_Type **)cpkt_conversion_calloc(
          NULL, array->typesSize, sizeof(*publicTypes));
      publicArray->types = publicTypes;
      if (array->typesSize && !publicTypes) {
        status = UA_STATUSCODE_BADOUTOFMEMORY;
        break;
      }
      publicArray->typesSize = array->typesSize;
      for (i = 0; i < array->typesSize; ++i)
        publicTypes[i] = &group->types[index + i];
    }
    index += array->typesSize;
  }
  /* Normalize external member pointers before temporary codec descriptor views
   * are freed. References within this native graph retain their exact identity.
   */
  for (i = 0; !status && i < count; ++i) {
    UA_DataType *type = (UA_DataType *)group->types[i].native;
    if (type->typeKind == UA_DATATYPEKIND_ENUM)
      continue;
    for (j = 0; j < type->membersSize; ++j) {
      child = cpkt_dynamic_member(group, type->members[j].memberType);
      if (!child) {
        status = UA_STATUSCODE_BADTYPEMISMATCH;
        break;
      }
      type->members[j].memberType = child->native;
    }
  }
  for (i = 0; !status && i < count; ++i)
    status = cpkt_dynamic_layout(group, i, 0);
  if (status) {
    cpkt_dynamic_delete(group);
    return status;
  }
  (void)pthread_mutex_lock(&cpkt_dynamic_mutex);
  group->next = cpkt_dynamic_registry;
  cpkt_dynamic_registry = group;
  group->registered = 1;
  (void)pthread_mutex_unlock(&cpkt_dynamic_mutex);
  *destination = group;
  return 0;
}

static UA_DataTypeArray *cpkt_dynamic_single_new(void) {
  UA_DataTypeArray *array = (UA_DataTypeArray *)UA_calloc(1, sizeof(*array));
  if (!array)
    return NULL;
  array->types = (UA_DataType *)UA_calloc(1, sizeof(UA_DataType));
  if (!array->types) {
    UA_free(array);
    return NULL;
  }
  array->typesSize = 1;
  array->cleanup = true;
  return array;
}

cpkt_opcua_StatusCode cpkt_opcua_DataType_fromDescription(
    cpkt_opcua_Type **destination,
    const cpkt_opcua_ExtensionObject *description,
    const cpkt_opcua_DataTypeArray *customTypes) {
  UA_ExtensionObject nativeDescription;
  UA_DataTypeArray *native;
  cpkt_codec_views views;
  cpkt_dynamic_types *group;
  UA_StatusCode status;
  if (!destination || !description)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  *destination = NULL;
  native = cpkt_dynamic_single_new();
  if (!native)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  memset(&views, 0, sizeof(views));
  memset(&nativeDescription, 0, sizeof(nativeDescription));
  status = cpkt_codec_custom_types(customTypes, &views);
  if (!status)
    status = cpkt_convert(description, &nativeDescription,
                          &cpkt_types[CPKT_OPCUA_TYPES_EXTENSIONOBJECT], 1, 0);
  if (!status)
    status = UA_DataType_fromDescription(native->types, &nativeDescription,
                                         views.types);
  if (!status) {
    status = cpkt_dynamic_import(native, 0, NULL, &group);
    native = NULL;
    if (!status)
      *destination = &group->types[0];
  }
  cpkt_dynamic_native_delete(native);
  UA_ExtensionObject_clear(&nativeDescription);
  cpkt_codec_views_clear(&views);
  return status;
}
cpkt_opcua_StatusCode cpkt_opcua_DataType_copy(const cpkt_opcua_Type *source,
                                               cpkt_opcua_Type **destination) {
  UA_DataTypeArray *native;
  cpkt_dynamic_types *group;
  UA_StatusCode status;
  if (!destination || !cpkt_valid_type(source))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  *destination = NULL;
  native = cpkt_dynamic_single_new();
  if (!native)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  status = UA_DataType_copy(source->native, native->types);
  if (status) {
    cpkt_dynamic_native_delete(native);
    return status;
  }
  status = cpkt_dynamic_import(native, 0, NULL, &group);
  if (!status)
    *destination = &group->types[0];
  return status;
}
cpkt_opcua_StatusCode
cpkt_opcua_DataType_toDescription(const cpkt_opcua_Type *type,
                                  cpkt_opcua_ExtensionObject *description) {
  UA_ExtensionObject native;
  UA_StatusCode status, converted;
  if (!description || !cpkt_valid_type(type))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memset(description, 0, sizeof(*description));
  memset(&native, 0, sizeof(native));
  status = UA_DataType_toDescription(type->native, &native);
  converted = cpkt_convert(&native, description,
                           &cpkt_types[CPKT_OPCUA_TYPES_EXTENSIONOBJECT], 0, 0);
  UA_ExtensionObject_clear(&native);
  if (converted)
    cpkt_opcua_ExtensionObject_clear(description);
  return converted ? converted : status;
}

static cpkt_dynamic_types *
cpkt_dynamic_individual(const cpkt_opcua_Type *type) {
  cpkt_dynamic_types *group, *result = NULL;
  (void)pthread_mutex_lock(&cpkt_dynamic_mutex);
  for (group = cpkt_dynamic_registry; group; group = group->next)
    if (group->count == 1 && group->types == type && !group->public_arrays) {
      result = group;
      break;
    }
  (void)pthread_mutex_unlock(&cpkt_dynamic_mutex);
  return result;
}
cpkt_opcua_StatusCode cpkt_opcua_DataType_clear(cpkt_opcua_Type *type) {
  cpkt_dynamic_types *group = cpkt_dynamic_individual(type);
  if (!group)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  (void)pthread_mutex_lock(&cpkt_dynamic_mutex);
  group->state[0] = 0;
  (void)pthread_mutex_unlock(&cpkt_dynamic_mutex);
  UA_DataType_clear(group->native->types);
  UA_free(group->members[0]);
  group->members[0] = NULL;
  memset(group->types, 0, sizeof(cpkt_opcua_Type));
  return 0;
}
cpkt_opcua_StatusCode cpkt_opcua_DataType_delete(cpkt_opcua_Type *type) {
  cpkt_dynamic_types *group;
  if (!type)
    return 0;
  group = cpkt_dynamic_individual(type);
  if (!group)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  cpkt_dynamic_delete(group);
  return 0;
}
cpkt_opcua_StatusCode
cpkt_opcua_DataTypeArray_delete(cpkt_opcua_DataTypeArray *array) {
  cpkt_dynamic_types *group, *result = NULL;
  if (!array)
    return 0;
  (void)pthread_mutex_lock(&cpkt_dynamic_mutex);
  for (group = cpkt_dynamic_registry; group; group = group->next)
    if (group->public_arrays == array) {
      result = group;
      break;
    }
  (void)pthread_mutex_unlock(&cpkt_dynamic_mutex);
  if (!result || result->borrowed_owner)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  cpkt_dynamic_delete(result);
  return 0;
}

cpkt_opcua_StatusCode
cpkt_opcua_DataType_getInfo(const cpkt_opcua_Type *type,
                            cpkt_opcua_DataTypeInfo *info) {
  const UA_DataType *native;
  if (!info || !cpkt_valid_type(type))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  native = type->native;
  memset(info, 0, sizeof(*info));
  info->typeName = native->typeName;
  cpkt_hb_borrow_node(&native->typeId, &info->typeId);
  cpkt_hb_borrow_node(&native->binaryEncodingId, &info->binaryEncodingId);
  cpkt_hb_borrow_node(&native->xmlEncodingId, &info->xmlEncodingId);
  info->memSize = native->memSize;
  info->publicSize = type->size;
  info->publicAlignment = type->alignment;
  info->typeKind = (cpkt_opcua_DataTypeKind)native->typeKind;
  info->pointerFree = native->pointerFree;
  info->overlayable = native->overlayable;
  info->membersSize = native->membersSize;
  return 0;
}
cpkt_opcua_StatusCode
cpkt_opcua_DataType_getMember(const cpkt_opcua_Type *type, size_t index,
                              cpkt_opcua_DataTypeMemberInfo *info) {
  const UA_DataTypeMember *native;
  const cpkt_opcua_type_member *member;
  UA_Int64 enumValue;
  if (!info || !cpkt_valid_type(type) || index >= type->native->membersSize)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memset(info, 0, sizeof(*info));
  native = &type->native->members[index];
  info->memberName = native->memberName;
  info->padding = native->padding;
  info->isArray = native->isArray;
  info->isOptional = native->isOptional;
  if (type->kind == UA_DATATYPEKIND_ENUM) {
    enumValue = (UA_Int32)(uintptr_t)native->memberType;
    return cpkt_convert(&enumValue, &info->enumValue,
                        &cpkt_types[CPKT_OPCUA_TYPES_INT64], 0, 0);
  }
  if (index >= type->members_size)
    return UA_STATUSCODE_BADTYPEMISMATCH;
  member = &type->members[index];
  info->memberType = cpkt_member_type(member);
  info->offset = member->c_offset;
  info->lengthOffset = member->c_size_offset;
  return 0;
}
cpkt_opcua_Boolean cpkt_opcua_DataType_getStructMember(
    const cpkt_opcua_Type *type, const char *name, size_t *offset,
    const cpkt_opcua_Type **memberType, cpkt_opcua_Boolean *isArray) {
  size_t i;
  if (!cpkt_valid_type(type) || !name ||
      (type->kind != UA_DATATYPEKIND_STRUCTURE &&
       type->kind != UA_DATATYPEKIND_OPTSTRUCT))
    return 0;
  for (i = 0; i < type->members_size; ++i) {
    if (type->native->members[i].memberName &&
        strcmp(name, type->native->members[i].memberName) == 0) {
      if (offset)
        *offset = type->members[i].is_array ? type->members[i].c_size_offset
                                            : type->members[i].c_offset;
      if (memberType)
        *memberType = cpkt_member_type(&type->members[i]);
      if (isArray)
        *isArray = type->members[i].is_array != 0;
      return 1;
    }
  }
  return 0;
}

const cpkt_opcua_Type *cpkt_opcua_findDataType(const cpkt_opcua_NodeId *id) {
  UA_NodeId native;
  if (!id)
    return NULL;
  native = cpkt_nodeid_view(id);
  return cpkt_from_native(UA_findDataType(&native));
}
const cpkt_opcua_Type *
cpkt_opcua_findDataTypeByName(const cpkt_opcua_QualifiedName *name) {
  UA_QualifiedName native;
  if (!name || !cpkt_string_valid(&name->name))
    return NULL;
  native.namespaceIndex = name->namespaceIndex;
  native.name = cpkt_string_view(&name->name);
  return cpkt_from_native(UA_findDataTypeByName(&native));
}
const cpkt_opcua_Type *
cpkt_opcua_findDataTypeWithCustom(const cpkt_opcua_NodeId *id,
                                  const cpkt_opcua_DataTypeArray *types) {
  UA_NodeId native;
  cpkt_codec_views views;
  const UA_DataType *found;
  UA_DataTypeArray *array;
  const cpkt_opcua_DataTypeArray *public_array;
  const cpkt_opcua_Type *result = NULL;
  size_t i;
  if (!id)
    return NULL;
  memset(&views, 0, sizeof(views));
  native = cpkt_nodeid_view(id);
  if (!cpkt_codec_custom_types(types, &views)) {
    found = UA_findDataTypeWithCustom(&native, views.types);
    /* Map the native-selected descriptor in this call's array first. Equal
     * descriptors in another live snapshot must not change its identity. */
    for (array = views.types, public_array = types; array && !result;
         array = array->next, public_array = public_array->next)
      for (i = 0; i < array->typesSize; ++i)
        if (found == &array->types[i]) {
          result = public_array->types[i];
          break;
        }
    if (!result)
      result = cpkt_from_native(found);
  }
  cpkt_codec_views_clear(&views);
  return result;
}
cpkt_opcua_StatusCode cpkt_opcua_client_getRemoteDataTypes_typed(
    cpkt_opcua_client *client, size_t count, const cpkt_opcua_NodeId *nodes,
    cpkt_opcua_DataTypeArray **types) {
  UA_NodeId *nativeNodes = NULL;
  UA_DataTypeArray *nativeTypes = NULL;
  cpkt_dynamic_types *group;
  UA_StatusCode status;
  if (!client || !client->client || !types)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  *types = NULL;
  status = cpkt_array(nodes, count, (void **)&nativeNodes,
                      &cpkt_types[CPKT_OPCUA_TYPES_NODEID], 1, 0);
  if (!status)
    status = UA_Client_getRemoteDataTypes(client->client, count, nativeNodes,
                                          &nativeTypes);
  if (!status && nativeTypes) {
    status = cpkt_dynamic_import(nativeTypes, 1, NULL, &group);
    nativeTypes = NULL;
    if (!status)
      *types = group->public_arrays;
  }
  cpkt_dynamic_native_delete(nativeTypes);
  if (nativeNodes)
    UA_Array_delete(nativeNodes, count,
                    cpkt_types[CPKT_OPCUA_TYPES_NODEID].native);
  return status;
}
typedef struct cpkt_dynamic_server_snapshot cpkt_dynamic_server_snapshot;
struct cpkt_dynamic_server_snapshot {
  cpkt_dynamic_server_snapshot *next;
  cpkt_dynamic_types *group;
};
typedef struct {
  cpkt_dynamic_server_snapshot *snapshots;
  const UA_DataTypeArray *nativeHead;
  size_t count;
} cpkt_dynamic_server;
static void cpkt_dynamic_server_cleanup(cpkt_nodes_owner *owner) {
  cpkt_dynamic_server *state = owner->dynamic_types;
  cpkt_dynamic_server_snapshot *snapshot, *next;
  if (!state)
    return;
  for (snapshot = state->snapshots; snapshot; snapshot = next) {
    next = snapshot->next;
    cpkt_dynamic_delete(snapshot->group);
    UA_free(snapshot);
  }
  UA_free(state);
  owner->dynamic_types = NULL;
}

static UA_StatusCode
cpkt_dynamic_server_sync(cpkt_opcua_server *server,
                         const cpkt_opcua_DataTypeArray **types) {
  const UA_DataTypeArray *head = UA_Server_getDataTypes(server->server),
                         *source;
  cpkt_nodes_owner *owner;
  cpkt_dynamic_server *state;
  cpkt_dynamic_server_snapshot *snapshot;
  cpkt_dynamic_types *group;
  UA_DataTypeArray *copies = NULL, **tail = &copies, *copy;
  cpkt_opcua_DataTypeArray *publicArray;
  const UA_DataType **aliases;
  size_t count = 0, index = 0, i;
  UA_StatusCode status = 0;
  if (types)
    *types = NULL;
  for (source = head; source; source = source->next) {
    if (count > (size_t)-1 - source->typesSize)
      return UA_STATUSCODE_BADOUTOFMEMORY;
    count += source->typesSize;
  }
  if (!head)
    return 0;
  owner = cpkt_nodes_owner_new(server);
  if (!owner)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  state = owner->dynamic_types;
  if (!state) {
    state = (cpkt_dynamic_server *)UA_calloc(1, sizeof(*state));
    if (!state)
      return UA_STATUSCODE_BADOUTOFMEMORY;
    owner->dynamic_types = state;
    owner->dynamic_types_cleanup = cpkt_dynamic_server_cleanup;
  }
  if (state->snapshots && state->nativeHead == head && state->count == count) {
    if (types)
      *types = state->snapshots->group->public_arrays;
    return 0;
  }
  snapshot = (cpkt_dynamic_server_snapshot *)UA_calloc(1, sizeof(*snapshot));
  aliases = (const UA_DataType **)cpkt_conversion_calloc(NULL, count,
                                                         sizeof(*aliases));
  if (!snapshot || (count && !aliases)) {
    UA_free(snapshot);
    UA_free(aliases);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  for (source = head; source && !status; source = source->next) {
    copy = (UA_DataTypeArray *)UA_calloc(1, sizeof(*copy));
    if (!copy) {
      status = UA_STATUSCODE_BADOUTOFMEMORY;
      break;
    }
    *tail = copy;
    tail = &copy->next;
    copy->cleanup = true;
    copy->types = (UA_DataType *)cpkt_conversion_calloc(NULL, source->typesSize,
                                                        sizeof(UA_DataType));
    if (source->typesSize && !copy->types) {
      status = UA_STATUSCODE_BADOUTOFMEMORY;
      break;
    }
    copy->typesSize = source->typesSize;
    for (i = 0; !status && i < source->typesSize; ++i) {
      aliases[index++] = &source->types[i];
      status = UA_DataType_copy(&source->types[i], &copy->types[i]);
    }
  }
  if (status) {
    cpkt_dynamic_native_delete(copies);
    UA_free(aliases);
    UA_free(snapshot);
    return status;
  }
  status = cpkt_dynamic_import(copies, 1, aliases, &group);
  if (status) {
    UA_free(snapshot);
    return status;
  }
  group->borrowed_owner = owner;
  /* The view preserves the actual native configuration ownership flags. */
  for (source = head, publicArray = group->public_arrays; source;
       source = source->next, publicArray = publicArray->next)
    publicArray->cleanup = source->cleanup;
  snapshot->group = group;
  snapshot->next = state->snapshots;
  state->snapshots = snapshot;
  state->nativeHead = head;
  state->count = count;
  if (types)
    *types = group->public_arrays;
  return 0;
}
cpkt_opcua_StatusCode
cpkt_opcua_server_getDataTypes_typed(cpkt_opcua_server *server,
                                     const cpkt_opcua_DataTypeArray **types) {
  if (!server || !server->server || !types)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  return cpkt_dynamic_server_sync(server, types);
}
const cpkt_opcua_Type *
cpkt_opcua_server_findDataType_typed(cpkt_opcua_server *server,
                                     const cpkt_opcua_NodeId *id) {
  UA_NodeId native;
  if (!server || !server->server || !id)
    return NULL;
  if (cpkt_dynamic_server_sync(server, NULL))
    return NULL;
  native = cpkt_nodeid_view(id);
  return cpkt_from_native(UA_Server_findDataType(server->server, &native));
}
cpkt_opcua_StatusCode
cpkt_opcua_server_addDataType_typed(cpkt_opcua_server *server,
                                    cpkt_opcua_NodeId parent,
                                    const cpkt_opcua_Type *type) {
  UA_NodeId nativeParent;
  if (!server || !server->server || !cpkt_valid_type(type))
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  nativeParent = cpkt_nodeid_view(&parent);
  {
    UA_StatusCode status =
        UA_Server_addDataType(server->server, nativeParent, type->native);
    return status ? status : cpkt_dynamic_server_sync(server, NULL);
  }
}
cpkt_opcua_StatusCode cpkt_opcua_server_addDataTypeFromDescription_typed(
    cpkt_opcua_server *server, const cpkt_opcua_ExtensionObject *description) {
  UA_ExtensionObject native;
  UA_StatusCode status;
  if (!server || !server->server || !description)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  memset(&native, 0, sizeof(native));
  status = cpkt_convert(description, &native,
                        &cpkt_types[CPKT_OPCUA_TYPES_EXTENSIONOBJECT], 1, 0);
  if (!status)
    status = UA_Server_addDataTypeFromDescription(server->server, &native);
  UA_ExtensionObject_clear(&native);
  if (!status)
    status = cpkt_dynamic_server_sync(server, NULL);
  return status;
}
#endif
