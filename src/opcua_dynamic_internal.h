/* Descriptor registry. Values and configurations borrow descriptors; their
 * owners must outlive every borrowed descriptor/value. Registry locks protect
 * lookup and publication only, never user callbacks or native operations. */
#ifndef CPKT_OPCUA_DYNAMIC_INTERNAL_H
#define CPKT_OPCUA_DYNAMIC_INTERNAL_H
#include <pthread.h>
typedef struct cpkt_dynamic_types cpkt_dynamic_types;
struct cpkt_dynamic_types {
  cpkt_dynamic_types *next;
  UA_DataTypeArray *native;
  const UA_DataType **aliases;
  void *borrowed_owner;
  cpkt_opcua_Type *types;
  cpkt_opcua_type_member **members;
  unsigned char *state;
  size_t count;
  cpkt_opcua_DataTypeArray *public_arrays;
  int registered;
};
static cpkt_dynamic_types *cpkt_dynamic_registry;
static pthread_mutex_t cpkt_dynamic_mutex = PTHREAD_MUTEX_INITIALIZER;

static int cpkt_native_type_equal(const UA_DataType *a, const UA_DataType *b,
                                  unsigned int depth) {
  size_t i;
  const UA_DataTypeMember *am, *bm;
  if (a == b)
    return 1;
  if (!a || !b || depth > 128 || a->memSize != b->memSize ||
      a->typeKind != b->typeKind || a->pointerFree != b->pointerFree ||
      a->overlayable != b->overlayable || a->membersSize != b->membersSize ||
      !UA_NodeId_equal(&a->typeId, &b->typeId) ||
      !UA_NodeId_equal(&a->binaryEncodingId, &b->binaryEncodingId) ||
      !UA_NodeId_equal(&a->xmlEncodingId, &b->xmlEncodingId))
    return 0;
  for (i = 0; i < a->membersSize; ++i) {
    if (!a->members || !b->members)
      return 0;
    am = &a->members[i];
    bm = &b->members[i];
    if (am->padding != bm->padding || am->isArray != bm->isArray ||
        am->isOptional != bm->isOptional)
      return 0;
    if (a->typeKind == UA_DATATYPEKIND_ENUM) {
      /* For enumerations this public slot stores an integer value, not a type.
       */
      if (am->memberType != bm->memberType)
        return 0;
    } else if (am->memberType == a && bm->memberType == b) {
      continue;
    } else if (!cpkt_native_type_equal(am->memberType, bm->memberType,
                                       depth + 1))
      return 0;
  }
  return 1;
}
/* Codec descriptor arrays contain shallow native copies. Resolve those copies
 * to the caller's descriptors within this thread and synchronous call only.
 * Nested codec calls restore the outer scope; equivalent live owners never
 * participate in this exact address mapping. */
typedef struct cpkt_type_scope cpkt_type_scope;
struct cpkt_type_scope {
  cpkt_type_scope *previous;
  const UA_DataTypeArray *native;
  const cpkt_opcua_DataTypeArray *public_types;
  int active;
};
static pthread_key_t cpkt_type_scope_key;
static pthread_once_t cpkt_type_scope_once = PTHREAD_ONCE_INIT;
static int cpkt_type_scope_key_status;
static void cpkt_type_scope_key_init(void) {
  cpkt_type_scope_key_status = pthread_key_create(&cpkt_type_scope_key, NULL);
}
static UA_StatusCode
cpkt_type_scope_enter(cpkt_type_scope *scope, const UA_DataTypeArray *native,
                      const cpkt_opcua_DataTypeArray *types) {
  if (!native || !types)
    return 0;
  if (pthread_once(&cpkt_type_scope_once, cpkt_type_scope_key_init) ||
      cpkt_type_scope_key_status)
    return UA_STATUSCODE_BADINTERNALERROR;
  scope->native = native;
  scope->public_types = types;
  scope->previous = (cpkt_type_scope *)pthread_getspecific(cpkt_type_scope_key);
  if (pthread_setspecific(cpkt_type_scope_key, scope))
    return UA_STATUSCODE_BADINTERNALERROR;
  scope->active = 1;
  return 0;
}
static void cpkt_type_scope_leave(cpkt_type_scope *scope) {
  if (scope->active) {
    (void)pthread_setspecific(cpkt_type_scope_key, scope->previous);
    scope->active = 0;
  }
}
static const cpkt_opcua_Type *cpkt_type_scope_find(const UA_DataType *type) {
  cpkt_type_scope *scope;
  const UA_DataTypeArray *native;
  const cpkt_opcua_DataTypeArray *types;
  size_t i;
  if (pthread_once(&cpkt_type_scope_once, cpkt_type_scope_key_init) ||
      cpkt_type_scope_key_status)
    return NULL;
  for (scope = (cpkt_type_scope *)pthread_getspecific(cpkt_type_scope_key);
       scope; scope = scope->previous) {
    for (native = scope->native, types = scope->public_types; native && types;
         native = native->next, types = types->next) {
      for (i = 0; i < native->typesSize && i < types->typesSize; ++i)
        if (type == &native->types[i])
          return types->types[i];
    }
  }
  return NULL;
}
static const cpkt_opcua_Type *cpkt_from_native(const UA_DataType *native) {
  cpkt_dynamic_types *group;
  const cpkt_opcua_Type *result = NULL;
  size_t i;
  if (!native)
    return NULL;
  result = cpkt_type_scope_find(native);
  if (result)
    return result;
  for (i = 0; i < CPKT_OPCUA_TYPES_COUNT; ++i)
    if (cpkt_types[i].native == native)
      return &cpkt_types[i];
  (void)pthread_mutex_lock(&cpkt_dynamic_mutex);
  /* Preserve the actual descriptor owner before comparing equivalent copies. */
  for (group = cpkt_dynamic_registry; group && !result; group = group->next)
    for (i = 0; i < group->count; ++i)
      if (group->state[i] == 2 &&
          (group->types[i].native == native ||
           (group->aliases && group->aliases[i] == native))) {
        result = &group->types[i];
        break;
      }
  if (!result)
    for (i = 0; i < CPKT_OPCUA_TYPES_COUNT; ++i)
      if (cpkt_native_type_equal(cpkt_types[i].native, native, 0)) {
        result = &cpkt_types[i];
        break;
      }
  for (group = cpkt_dynamic_registry; group && !result; group = group->next)
    for (i = 0; i < group->count; ++i)
      if (group->state[i] == 2 &&
          cpkt_native_type_equal(group->types[i].native, native, 0)) {
        result = &group->types[i];
        break;
      }
  (void)pthread_mutex_unlock(&cpkt_dynamic_mutex);
  return result;
}
static const cpkt_opcua_Type *cpkt_valid_type(const cpkt_opcua_Type *type) {
  cpkt_dynamic_types *group;
  const cpkt_opcua_Type *result = NULL;
  size_t i;
  for (i = 0; i < CPKT_OPCUA_TYPES_COUNT; ++i)
    if (&cpkt_types[i] == type)
      return type;
  (void)pthread_mutex_lock(&cpkt_dynamic_mutex);
  for (group = cpkt_dynamic_registry; group && !result; group = group->next)
    for (i = 0; i < group->count; ++i)
      if (&group->types[i] == type && group->state[i] == 2) {
        result = type;
        break;
      }
  (void)pthread_mutex_unlock(&cpkt_dynamic_mutex);
  return result;
}
#endif
