#include "opcua_callbacks_test.h"
#include "opcua_types_peer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void check(int ok, const char *expression, int line) {
  if (!ok) {
    fprintf(stderr, "creation line %d: %s\n", line, expression);
    abort();
  }
}
#define CHECK(e) check(!!(e), #e, __LINE__)
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
void cpkt_types_fail_after(size_t count);
int cpkt_types_fail_stop(void);
#endif
struct state {
  cpkt_opcua_server *server;
  cpkt_opcua_VariableAttributes variable;
  cpkt_opcua_MethodAttributes method;
  cpkt_opcua_Argument argument;
  int kind, replace, nested, reject, constructors, reads, methods, replacement,
      sequence;
  int fail_output;
  cpkt_opcua_NodeId nested_id;
};
static struct state *current;
static cpkt_opcua_NodeId node(unsigned short ns, unsigned int id) {
  cpkt_opcua_NodeId n;
  cpkt_opcua_NodeId_init(&n);
  n.namespaceIndex = ns;
  n.identifier.numeric = id;
  return n;
}
static cpkt_opcua_NodeId string_id(const char *text) {
  cpkt_opcua_NodeId n = node(1, 0);
  n.identifierType = CPKT_OPCUA_NODEIDTYPE_STRING;
  n.identifier.string.data = (cpkt_opcua_Byte *)text;
  n.identifier.string.length = strlen(text);
  return n;
}
static cpkt_opcua_QualifiedName named(const char *text) {
  cpkt_opcua_QualifiedName n;
  cpkt_opcua_QualifiedName_init(&n);
  n.namespaceIndex = 1;
  n.name.data = (cpkt_opcua_Byte *)text;
  n.name.length = strlen(text);
  return n;
}
static cpkt_opcua_QualifiedName name(void) { return named("creation"); }
static cpkt_opcua_Int64 minimum(void) {
  cpkt_opcua_Int64 n;
  n.high32 = 0x80000000U;
  n.low32 = 0;
  return n;
}
static cpkt_opcua_StatusCode
read_source(cpkt_opcua_server *server, const cpkt_opcua_NodeId *session,
            void *session_context, const cpkt_opcua_NodeId *id, void *context,
            cpkt_opcua_Boolean timestamp, const cpkt_opcua_NumericRange *range,
            cpkt_opcua_DataValue *value) {
  struct state *s = context;
  cpkt_opcua_Int64 n = minimum();
  (void)session;
  (void)session_context;
  (void)timestamp;
  (void)range;
  CHECK(s == current && server == s->server && id->namespaceIndex == 1);
  ++s->reads;
  value->hasValue = 1;
  return cpkt_opcua_Variant_setScalarCopy(
      &value->value, &n, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64));
}
static cpkt_opcua_StatusCode
replacement_read(cpkt_opcua_server *server, const cpkt_opcua_NodeId *session,
                 void *session_context, const cpkt_opcua_NodeId *id,
                 void *context, cpkt_opcua_Boolean timestamp,
                 const cpkt_opcua_NumericRange *range,
                 cpkt_opcua_DataValue *value) {
  struct state *s = context;
  ++s->replacement;
  return read_source(server, session, session_context, id, context, timestamp,
                     range, value);
}
static cpkt_opcua_StatusCode
method_source(cpkt_opcua_server *server, const cpkt_opcua_NodeId *session,
              void *session_context, const cpkt_opcua_NodeId *id, void *context,
              const cpkt_opcua_NodeId *object, void *object_context,
              size_t input_size, const cpkt_opcua_Variant *input,
              size_t output_size, cpkt_opcua_Variant *output) {
  struct state *s = context;
  (void)session;
  (void)session_context;
  (void)object_context;
  CHECK(s == current && server == s->server && id && object &&
        input_size == 1 && output_size == 1);
  ++s->methods;
  return cpkt_opcua_Variant_copy(input, output);
}
static cpkt_opcua_StatusCode
replacement_method(cpkt_opcua_server *server, const cpkt_opcua_NodeId *session,
                   void *session_context, const cpkt_opcua_NodeId *id,
                   void *context, const cpkt_opcua_NodeId *object,
                   void *object_context, size_t input_size,
                   const cpkt_opcua_Variant *input, size_t output_size,
                   cpkt_opcua_Variant *output) {
  struct state *s = context;
  ++s->replacement;
  return method_source(server, session, session_context, id, context, object,
                       object_context, input_size, input, output_size, output);
}
static cpkt_opcua_StatusCode invoke(struct state *s, cpkt_opcua_NodeId id) {
  cpkt_opcua_StatusCode status;
  const cpkt_opcua_Int64 *n;
  if (s->kind == 0 || s->kind == 4) {
    cpkt_opcua_DataValue value;
    cpkt_opcua_ReadValueId request;
    cpkt_opcua_DataValue_init(&value);
    cpkt_opcua_ReadValueId_init(&request);
    request.nodeId = id;
    request.attributeId = 13;
    status = cpkt_opcua_server_read_typed(
        s->server, &request, cpkt_opcua_TIMESTAMPSTORETURN_BOTH, &value);
    if (!status && value.hasStatus)
      status = value.status;
    if (!status) {
      CHECK(value.hasValue &&
            value.value.type == cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64));
      n = value.value.data;
      CHECK(n && n->high32 == 0x80000000U && !n->low32);
    }
    cpkt_opcua_DataValue_clear(&value);
  } else {
    cpkt_opcua_CallMethodRequest request;
    cpkt_opcua_CallMethodResult result;
    cpkt_opcua_MethodCallback callback;
    cpkt_opcua_Variant input;
    cpkt_opcua_Int64 number = minimum();
    cpkt_opcua_CallMethodRequest_init(&request);
    cpkt_opcua_CallMethodResult_init(&result);
    cpkt_opcua_Variant_init(&input);
    status =
        cpkt_opcua_server_getMethodNodeCallback_typed(s->server, id, &callback);
    if (!status)
      CHECK(callback == (s->replace ? replacement_method : method_source));
    if (!status)
      status = cpkt_opcua_Variant_setScalarCopy(
          &input, &number, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64));
    request.objectId = node(0, CPKT_OPCUA_NS0ID_OBJECTSFOLDER);
    request.methodId = id;
    request.inputArguments = &input;
    request.inputArgumentsSize = 1;
    if (!status)
      status = cpkt_opcua_server_call_typed(s->server, &request, &result);
    if (!status)
      status = result.statusCode;
    if (!status) {
      CHECK(result.outputArgumentsSize == 1 &&
            result.outputArguments[0].type ==
                cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64));
      n = result.outputArguments[0].data;
      CHECK(n && n->high32 == 0x80000000U && !n->low32);
    }
    cpkt_opcua_CallMethodResult_clear(&result);
    cpkt_opcua_Variant_clear(&input);
  }
  return status;
}
static cpkt_opcua_StatusCode create(struct state *s, int kind,
                                    cpkt_opcua_NodeId requested,
                                    cpkt_opcua_NodeId *out, int bad_parent,
                                    cpkt_opcua_NodeId *input_id,
                                    cpkt_opcua_NodeId *output_id) {
  cpkt_opcua_NodeId parent = node(0, CPKT_OPCUA_NS0ID_OBJECTSFOLDER);
  cpkt_opcua_StatusCode status;
  cpkt_opcua_CallbackValueSource source;
  cpkt_opcua_QualifiedName browse = name();
  char key[64];
  if (requested.identifierType == CPKT_OPCUA_NODEIDTYPE_STRING)
    browse.name = requested.identifier.string;
  else {
    sprintf(key, "creation-%d", ++s->sequence);
    browse.name.data = (cpkt_opcua_Byte *)key;
    browse.name.length = strlen(key);
  }
  if (bad_parent)
    parent = node(1, 9999999);
  memset(&source, 0, sizeof(source));
  source.read = read_source;
  if (kind == 0)
    return cpkt_opcua_server_addCallbackValueSourceVariableNode_typed(
        s->server, requested, parent, node(0, CPKT_OPCUA_NS0ID_ORGANIZES),
        browse, node(0, CPKT_OPCUA_NS0ID_BASEDATAVARIABLETYPE), s->variable,
        source, s, out);
  if (kind == 1)
    return cpkt_opcua_server_addMethodNode_typed(
        s->server, requested, parent, node(0, CPKT_OPCUA_NS0ID_HASCOMPONENT),
        browse, s->method, method_source, 1, &s->argument, 1, &s->argument, s,
        out);
  if (kind == 2)
    return cpkt_opcua_server_addMethodNodeEx_typed(
        s->server, requested, parent, node(0, CPKT_OPCUA_NS0ID_HASCOMPONENT),
        browse, s->method, method_source, 1, &s->argument, node(1, 0), input_id,
        1, &s->argument, node(1, 0), output_id, s, out);
  status = cpkt_opcua_server_addNode_begin_typed(
      s->server,
      kind == 3 ? cpkt_opcua_NODECLASS_METHOD : cpkt_opcua_NODECLASS_VARIABLE,
      requested, parent,
      node(0, kind == 3 ? CPKT_OPCUA_NS0ID_HASCOMPONENT
                        : CPKT_OPCUA_NS0ID_ORGANIZES),
      browse, node(0, kind == 3 ? 0 : CPKT_OPCUA_NS0ID_BASEDATAVARIABLETYPE),
      kind == 3 ? (const void *)&s->method : (const void *)&s->variable,
      cpkt_opcua_type_at(kind == 3 ? CPKT_OPCUA_TYPES_METHODATTRIBUTES
                                   : CPKT_OPCUA_TYPES_VARIABLEATTRIBUTES),
      s, out);
  if (!status && kind == 3)
    status = cpkt_opcua_server_addMethodNode_finish_typed(
        s->server, *out, method_source, 1, &s->argument, 1, &s->argument);
  else if (!status)
    status = cpkt_opcua_server_addNode_finish_typed(s->server, *out);
  return status;
}
static cpkt_opcua_StatusCode constructor(cpkt_opcua_server *server,
                                         const cpkt_opcua_NodeId *session,
                                         void *session_context,
                                         const cpkt_opcua_NodeId *id,
                                         void **context) {
  struct state *s = current;
  cpkt_opcua_StatusCode status;
  cpkt_opcua_CallbackValueSource source;
  (void)session;
  (void)session_context;
  CHECK(s && server == s->server);
  if (*context != s)
    return 0;
  ++s->constructors;
  if (s->reject)
    return CPKT_OPCUA_STATUSCODE_BADINTERNALERROR;
  if (s->replace) {
    if (s->kind == 0) {
      memset(&source, 0, sizeof(source));
      source.read = replacement_read;
      status = cpkt_opcua_server_setVariableNode_callbackValueSource_typed(
          server, *id, source);
    } else if (s->kind != 4)
      status = cpkt_opcua_server_setMethodNodeCallback_typed(
          server, *id, replacement_method);
    else
      status = 0;
    if (status)
      return status;
  }
  status = invoke(s, *id);
  if (!status && s->nested) {
    int kind = s->kind;
    s->nested = 0;
    s->kind = 0;
    status = create(s, 0, node(1, 0), &s->nested_id, 0, NULL, NULL);
    s->kind = kind;
  }
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  if (!status && s->fail_output)
    cpkt_types_fail_after(2);
#endif
  return status;
}
static void initialize(struct state *s) {
  cpkt_opcua_GlobalNodeLifecycle lifecycle;
  cpkt_opcua_Int64 n = minimum();
  memset(s, 0, sizeof(*s));
  current = s;
  CHECK(cpkt_opcua_server_new(&s->server, 0) == CPKT_OPCUA_OK);
  CHECK(!cpkt_opcua_VariableAttributes_default(&s->variable));
  CHECK(!cpkt_opcua_MethodAttributes_default(&s->method));
  s->variable.dataType = node(0, CPKT_OPCUA_NS0ID_INT64);
  s->variable.valueRank = -1;
  s->variable.accessLevel = 1;
  CHECK(!cpkt_opcua_Variant_setScalarCopy(
      &s->variable.value, &n, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64)));
  s->method.executable = 1;
  s->method.userExecutable = 1;
  cpkt_opcua_Argument_init(&s->argument);
  s->argument.dataType = node(0, CPKT_OPCUA_NS0ID_INT64);
  s->argument.valueRank = -1;
  memset(&lifecycle, 0, sizeof(lifecycle));
  lifecycle.constructor = constructor;
  CHECK(!cpkt_opcua_server_set_global_node_lifecycle(s->server, &lifecycle));
}
static void finish(struct state *s) {
  cpkt_opcua_server_free(s->server);
  cpkt_opcua_VariableAttributes_clear(&s->variable);
  cpkt_opcua_MethodAttributes_clear(&s->method);
  cpkt_opcua_Argument_clear(&s->argument);
  cpkt_opcua_NodeId_clear(&s->nested_id);
  current = NULL;
}
void cpkt_types_test_creation(void) {
  struct state s;
  cpkt_opcua_NodeId id, input_id, output_id;
  void *context;
  int kind, automatic, replace;
  initialize(&s);
  for (kind = 0; kind < 5; ++kind) {
    for (automatic = 0; automatic < 2; ++automatic) {
      for (replace = 0; replace < 2; ++replace) {
        int before = s.constructors;
        cpkt_opcua_NodeId_init(&id);
        cpkt_opcua_NodeId_init(&input_id);
        cpkt_opcua_NodeId_init(&output_id);
        s.kind = kind;
        s.replace = replace;
        s.nested = 1;
        s.replacement = 0;
        CHECK(!create(&s, kind,
                      node(1, automatic ? 0 : 8000 + kind * 20 + replace), &id,
                      0, &input_id, &output_id));
        CHECK(s.constructors >= before + 2 && id.namespaceIndex == 1 &&
              id.identifierType == CPKT_OPCUA_NODEIDTYPE_NUMERIC &&
              id.identifier.numeric);
        CHECK(!cpkt_opcua_server_getNodeContext_typed(s.server, id, &context) &&
              context == &s);
        CHECK(!invoke(&s, id));
        if (replace && kind != 4)
          CHECK(s.replacement > 0);
        if (kind == 2)
          CHECK(input_id.namespaceIndex == 1 && output_id.namespaceIndex == 1 &&
                !cpkt_opcua_NodeId_equal(&input_id, &output_id));
        CHECK(!cpkt_opcua_server_deleteNode_typed(s.server, id, 1));
        CHECK(!cpkt_opcua_server_deleteNode_typed(s.server, s.nested_id, 1));
        cpkt_opcua_NodeId_clear(&s.nested_id);
        cpkt_opcua_NodeId_clear(&id);
        cpkt_opcua_NodeId_clear(&input_id);
        cpkt_opcua_NodeId_clear(&output_id);
      }
    }
  }
  /* Optional IDs still allow the actual native constructors to use callbacks.
   */
  s.kind = 0;
  s.replace = 0;
  CHECK(!create(&s, 0, node(1, 0), NULL, 0, NULL, NULL));
  s.kind = 1;
  CHECK(!create(&s, 1, node(1, 8350), NULL, 0, NULL, NULL));
  CHECK(!cpkt_opcua_server_deleteNode_typed(s.server, node(1, 8350), 1));
  /* Extended fixed property IDs are the same public native arguments. */
  {
    cpkt_opcua_MethodCallback callback;
    CHECK(!cpkt_opcua_server_addMethodNodeEx_typed(
        s.server, string_id("fixedMethod"),
        node(0, CPKT_OPCUA_NS0ID_OBJECTSFOLDER),
        node(0, CPKT_OPCUA_NS0ID_HASCOMPONENT), named("fixedMethod"), s.method,
        method_source, 1, &s.argument, string_id("fixedInput"), &input_id, 1,
        &s.argument, string_id("fixedOutput"), &output_id, &s, &id));
    CHECK(id.identifierType == CPKT_OPCUA_NODEIDTYPE_STRING &&
          input_id.identifierType == CPKT_OPCUA_NODEIDTYPE_STRING &&
          output_id.identifierType == CPKT_OPCUA_NODEIDTYPE_STRING);
    CHECK(!cpkt_opcua_server_getMethodNodeCallback_typed(s.server, id,
                                                         &callback) &&
          callback == method_source);
    CHECK(!invoke(&s, id));
    cpkt_opcua_NodeId_clear(&id);
    cpkt_opcua_NodeId_clear(&input_id);
    cpkt_opcua_NodeId_clear(&output_id);
  }
  /* Native failure can leave an assigned source node behind. The full facade
   * retains its output ID and dispatch record, matching an independent peer. */
  for (kind = 0; kind < 2; ++kind) {
    struct cpkt_creation_observed native;
    cpkt_opcua_StatusCode status;
    cpkt_types_peer_creation_failure(kind, &native);
    s.kind = kind;
    status = create(&s, kind, string_id(kind ? "badMethod" : "badVariable"),
                    &id, 1, NULL, NULL);
    CHECK(status == native.status &&
          (!cpkt_opcua_NodeId_isNull(&id)) == native.assigned);
    CHECK((!cpkt_opcua_server_getNodeContext_typed(s.server, id, &context)) ==
          native.exists);
    if (native.exists) {
      CHECK(context == &s);
      if (!kind)
        CHECK(!invoke(&s, id));
      CHECK(!cpkt_opcua_server_deleteNode_typed(s.server, id, 1));
    }
    cpkt_opcua_NodeId_clear(&id);
  }
  /* Constructor rejection follows the native failure path without committing
   * an inaccessible callback record as a live node. */
  s.reject = 1;
  for (kind = 0; kind < 4; ++kind) {
    s.kind = kind;
    CHECK(create(&s, kind, node(1, 8300 + kind), &id, 0, NULL, NULL) ==
          CPKT_OPCUA_STATUSCODE_BADINTERNALERROR);
    CHECK(cpkt_opcua_server_getNodeContext_typed(s.server, id, &context) ==
          CPKT_OPCUA_STATUSCODE_BADNODEIDUNKNOWN);
    cpkt_opcua_NodeId_clear(&id);
  }
  s.reject = 0;
  CHECK(cpkt_opcua_server_addNode_begin_typed(
            s.server, cpkt_opcua_NODECLASS_VARIABLE, node(1, 0), node(0, 85),
            node(0, 35), name(), node(0, 63), NULL,
            cpkt_opcua_type_at(CPKT_OPCUA_TYPES_VARIABLEATTRIBUTES), NULL,
            &id) == CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_server_addNode_begin_typed(
            s.server, cpkt_opcua_NODECLASS_VARIABLE, node(1, 0), node(0, 85),
            node(0, 35), name(), node(0, 63), &s.variable, NULL, NULL,
            &id) == CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  /* Native copies the two fixed argument IDs after the constructor. Fail the
   * following C89 output conversion and require method/property rollback. */
  s.kind = 2;
  s.fail_output = 1;
  CHECK(cpkt_opcua_server_addMethodNodeEx_typed(
            s.server, string_id("rollbackMethod"), node(0, 85), node(0, 47),
            named("rollbackMethod"), s.method, method_source, 1, &s.argument,
            string_id("rollbackInput"), &input_id, 1, &s.argument,
            string_id("rollbackOutput"), &output_id, &s,
            &id) == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY);
  CHECK(cpkt_types_fail_stop());
  s.fail_output = 0;
  CHECK(cpkt_opcua_NodeId_isNull(&id) && cpkt_opcua_NodeId_isNull(&input_id) &&
        cpkt_opcua_NodeId_isNull(&output_id));
  CHECK(cpkt_opcua_server_getNodeContext_typed(
            s.server, string_id("rollbackMethod"), &context) ==
        CPKT_OPCUA_STATUSCODE_BADNODEIDUNKNOWN);
  CHECK(cpkt_opcua_server_getNodeContext_typed(
            s.server, string_id("rollbackInput"), &context) ==
        CPKT_OPCUA_STATUSCODE_BADNODEIDUNKNOWN);
  CHECK(cpkt_opcua_server_getNodeContext_typed(
            s.server, string_id("rollbackOutput"), &context) ==
        CPKT_OPCUA_STATUSCODE_BADNODEIDUNKNOWN);
  for (kind = 0; kind < 5; ++kind) {
    size_t position;
    s.kind = kind;
    for (position = 0; position < 512; ++position) {
      int failed;
      cpkt_opcua_StatusCode status;
      cpkt_opcua_NodeId requested;
      char key[64];
      sprintf(key, "allocation-%d-%lu", kind, (unsigned long)position);
      requested = string_id(key);
      cpkt_types_fail_after(position);
      status = create(&s, kind, requested, &id, 0, &input_id, &output_id);
      failed = cpkt_types_fail_stop();
      if (!status) {
        CHECK(!cpkt_opcua_server_getNodeContext_typed(s.server, id, &context) &&
              context == &s);
        CHECK(!invoke(&s, id));
      }
      (void)cpkt_opcua_server_deleteNode_typed(s.server, requested, 1);
      cpkt_opcua_NodeId_clear(&id);
      cpkt_opcua_NodeId_clear(&input_id);
      cpkt_opcua_NodeId_clear(&output_id);
      if (!failed) {
        CHECK(!status);
        break;
      }
    }
    CHECK(position < 512);
  }
#endif
  finish(&s);
}
