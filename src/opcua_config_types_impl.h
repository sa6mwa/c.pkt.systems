/* Persistent public descriptor conversions, with native cleanup flags.
 * Only public metadata is copied; no application/backend state is cloned. */
#ifndef CPKT_OPCUA_CONFIG_TYPES_IMPL_H
#define CPKT_OPCUA_CONFIG_TYPES_IMPL_H
struct cpkt_cfg_type_allocation {
  cpkt_cfg_type_allocation *next;
  UA_DataTypeArray *array;
  UA_DataType *types;
  int cleanup;
};
struct cpkt_cfg_type_snapshot {
  cpkt_cfg_type_snapshot *next;
  cpkt_dynamic_types *group;
};
static void cpkt_cfg_type_allocations_clear(cpkt_cfg_type_allocation **head) {
  cpkt_cfg_type_allocation *item;
  while (*head) {
    item = *head;
    *head = item->next;
    /* Native already cleared the cleanup=true allocations. Do not read them. */
    if (!item->cleanup) {
      UA_free(item->types);
      UA_free(item->array);
    }
    UA_free(item);
  }
}
static void cpkt_cfg_type_metadata_clear(cpkt_cfg_type_allocation **allocations,
                                         cpkt_cfg_type_snapshot **snapshot) {
  cpkt_cfg_type_snapshot *item;
  while (*snapshot) {
    item = *snapshot;
    *snapshot = item->next;
    cpkt_dynamic_delete(item->group);
    UA_free(item);
  }
  cpkt_cfg_type_allocations_clear(allocations);
}
static UA_StatusCode
cpkt_cfg_types_stage(const cpkt_opcua_DataTypeArray *source,
                     UA_DataTypeArray **output,
                     cpkt_cfg_type_allocation **allocations) {
  cpkt_codec_views views;
  cpkt_cfg_type_allocation *item, **tail = allocations;
  const cpkt_opcua_DataTypeArray *public_array, *target_array;
  UA_DataTypeArray *native, *target;
  size_t i, j, member;
  UA_StatusCode status;
  *output = NULL;
  *allocations = NULL;
  memset(&views, 0, sizeof(views));
  status = cpkt_codec_custom_types(source, &views);
  if (status) {
    cpkt_codec_views_clear(&views);
    return status;
  }
  /* Reserve teardown records before changing shallow codec metadata. */
  for (native = views.types, public_array = source; native;
       native = native->next, public_array = public_array->next) {
    item = (cpkt_cfg_type_allocation *)UA_calloc(1, sizeof(*item));
    if (!item) {
      cpkt_codec_views_clear(&views);
      while (*allocations) {
        item = *allocations;
        *allocations = item->next;
        UA_free(item);
      }
      return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    item->array = native;
    item->types = native->types;
    *tail = item;
    tail = &item->next;
  }
  for (native = views.types, public_array = source, item = *allocations;
       native && !status; native = native->next,
      public_array = public_array->next, item = item->next) {
    if (!public_array->cleanup)
      continue;
    native->cleanup = true;
    item->cleanup = 1;
    if (native->typesSize)
      memset(native->types, 0, native->typesSize * sizeof(*native->types));
    for (i = 0; !status && i < native->typesSize; ++i)
      status =
          UA_DataType_copy(public_array->types[i]->native, &native->types[i]);
  }
  /* Owned copies reference the corresponding persistent configuration graph,
   * including self-recursion. Borrowed member tables are never modified. */
  for (native = views.types; native && !status; native = native->next) {
    if (!native->cleanup)
      continue;
    for (i = 0; i < native->typesSize; ++i) {
      if (native->types[i].typeKind == UA_DATATYPEKIND_ENUM)
        continue;
      for (member = 0; member < native->types[i].membersSize; ++member) {
        const UA_DataType *referenced =
            native->types[i].members[member].memberType;
        int found = 0;
        for (target = views.types, target_array = source; target && !found;
             target = target->next, target_array = target_array->next)
          for (j = 0; j < target->typesSize; ++j)
            if (referenced == target_array->types[j]->native) {
              native->types[i].members[member].memberType = &target->types[j];
              found = 1;
              break;
            }
      }
    }
  }
  if (status) {
    cpkt_dynamic_native_delete(views.types);
    cpkt_cfg_type_allocations_clear(allocations);
    return status;
  }
  *output = views.types;
  return 0;
}
static UA_StatusCode
cpkt_cfg_types_snapshot(UA_DataTypeArray *head, void *owner,
                        cpkt_cfg_type_snapshot **snapshot,
                        const cpkt_opcua_DataTypeArray **output) {
  UA_DataTypeArray *source, *copies = NULL, **tail = &copies, *copy;
  const UA_DataType **aliases;
  cpkt_dynamic_types *group;
  cpkt_cfg_type_snapshot *item;
  cpkt_opcua_DataTypeArray *public_array;
  size_t count = 0, offset = 0, i;
  UA_StatusCode status = 0;
  *output = NULL;
  if (!head)
    return 0;
  for (source = head; source; source = source->next) {
    if (source->typesSize > (size_t)-1 - count)
      return UA_STATUSCODE_BADOUTOFMEMORY;
    count += source->typesSize;
  }
  aliases = (const UA_DataType **)cpkt_conversion_calloc(NULL, count,
                                                         sizeof(*aliases));
  if (count && !aliases)
    return UA_STATUSCODE_BADOUTOFMEMORY;
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
                                                        sizeof(*copy->types));
    if (source->typesSize && !copy->types) {
      status = UA_STATUSCODE_BADOUTOFMEMORY;
      break;
    }
    copy->typesSize = source->typesSize;
    for (i = 0; !status && i < source->typesSize; ++i) {
      aliases[offset++] = &source->types[i];
      status = UA_DataType_copy(&source->types[i], &copy->types[i]);
    }
  }
  if (status) {
    cpkt_dynamic_native_delete(copies);
    UA_free(aliases);
    return status;
  }
  status = cpkt_dynamic_import(copies, 1, aliases, &group);
  if (status)
    return status;
  group->borrowed_owner = owner;
  for (source = head, public_array = group->public_arrays; source;
       source = source->next, public_array = public_array->next)
    public_array->cleanup = source->cleanup;
  item = (cpkt_cfg_type_snapshot *)UA_calloc(1, sizeof(*item));
  if (!item) {
    cpkt_dynamic_delete(group);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  item->group = group;
  item->next = *snapshot;
  *snapshot = item;
  *output = group->public_arrays;
  return 0;
}
#endif
