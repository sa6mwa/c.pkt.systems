#include "opcua_callbacks_test.h"
#include "opcua_types_peer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void check(int ok, const char *expression, int line) {
  if (!ok) {
    fprintf(stderr, "producer line %d: %s\n", line, expression);
    abort();
  }
}
#define CHECK(e) check(!!(e), #e, __LINE__)
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
void cpkt_types_fail_after(size_t count);
int cpkt_types_fail_stop(void);
#endif
struct state {
  struct cpkt_async_peer native;
  cpkt_opcua_server *server;
  int deferred, direct, replace, replace_method, borrow, borrow_method,
      fail_borrow, fail_async, partial_read, filter_source;
  cpkt_opcua_StatusCode callback_status;
  cpkt_opcua_DataValue *read;
  const cpkt_opcua_DataValue *write;
  cpkt_opcua_Variant *call;
  cpkt_opcua_Variant *zero[2];
  int zero_results;
  cpkt_opcua_history_value *stored;
  int callbacks[3], results[3], cancellations, replacement_calls;
  cpkt_opcua_StatusCode result_status[3];
};
static struct state *current;
static cpkt_opcua_NodeId node(unsigned int id) {
  cpkt_opcua_NodeId value;
  cpkt_opcua_NodeId_init(&value);
  value.namespaceIndex = 1;
  value.identifier.numeric = id;
  return value;
}
static cpkt_opcua_Int64 minimum(void) {
  cpkt_opcua_Int64 value;
  value.high32 = 0x80000000U;
  value.low32 = 0;
  return value;
}
static cpkt_opcua_Int64 maximum(void) {
  cpkt_opcua_Int64 value;
  value.high32 = 0x7fffffffU;
  value.low32 = 0xffffffffU;
  return value;
}
static void metadata(struct state *s, cpkt_opcua_server *server,
                     const cpkt_opcua_NodeId *session, void *session_context,
                     const cpkt_opcua_NodeId *id, unsigned int number,
                     const cpkt_opcua_NumericRange *range) {
  CHECK(server == s->server && id && id->namespaceIndex == 1 &&
        id->identifier.numeric == number);
  if (s->direct) {
    CHECK(session && session->namespaceIndex == 2 &&
          session->identifierType == CPKT_OPCUA_NODEIDTYPE_STRING);
    CHECK(session->identifier.string.length == 7 &&
          !memcmp(session->identifier.string.data, "session", 7));
    CHECK(session_context == s);
    if (number == 6200)
      CHECK(range && range->dimensionsSize == 2 &&
            range->dimensions[0].min == 1 && range->dimensions[1].max == 4);
  }
}
static cpkt_opcua_StatusCode fill_read(cpkt_opcua_DataValue *value) {
  cpkt_opcua_Int64 number = minimum();
  cpkt_opcua_StatusCode status;
  cpkt_opcua_Variant_clear(&value->value);
  status = cpkt_opcua_Variant_setScalarCopy(
      &value->value, &number, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64));
  value->hasValue = 1;
  value->hasStatus = 1;
  value->status = CPKT_OPCUA_STATUSCODE_UNCERTAINSUBNORMAL;
  value->hasSourceTimestamp = 1;
  value->sourceTimestamp = maximum();
  value->hasSourcePicoseconds = 1;
  value->sourcePicoseconds = 123;
  return status;
}
static cpkt_opcua_StatusCode fill_call(cpkt_opcua_Variant *outputs) {
  cpkt_opcua_Int64 number = minimum();
  cpkt_opcua_StatusCode status;
  cpkt_opcua_Variant_clear(&outputs[0]);
  cpkt_opcua_Variant_clear(&outputs[1]);
  status = cpkt_opcua_Variant_setScalarCopy(
      &outputs[0], &number, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64));
  number = maximum();
  if (!status)
    status = cpkt_opcua_Variant_setScalarCopy(
        &outputs[1], &number, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64));
  return status;
}
static cpkt_opcua_StatusCode
replacement_read(cpkt_opcua_server *server, const cpkt_opcua_NodeId *session,
                 void *session_context, const cpkt_opcua_NodeId *id,
                 void *context, cpkt_opcua_Boolean timestamp,
                 const cpkt_opcua_NumericRange *range,
                 cpkt_opcua_DataValue *value);
static cpkt_opcua_StatusCode
read_source(cpkt_opcua_server *server, const cpkt_opcua_NodeId *session,
            void *session_context, const cpkt_opcua_NodeId *id, void *context,
            cpkt_opcua_Boolean timestamp, const cpkt_opcua_NumericRange *range,
            cpkt_opcua_DataValue *value) {
  struct state *s = context;
  cpkt_opcua_CallbackValueSource source;
  CHECK(s == current && (timestamp || s->filter_source));
  metadata(s, server, session, session_context, id, 6200, range);
  ++s->callbacks[0];
  CHECK(cpkt_opcua_server_setAsyncReadResult_typed(server, value) ==
        CPKT_OPCUA_STATUSCODE_BADNOTFOUND);
  if (s->replace) {
    s->replace = 0;
    memset(&source, 0, sizeof(source));
    source.read = replacement_read;
    CHECK(cpkt_opcua_server_setVariableNode_callbackValueSource_typed(
              server, *id, source) == 0);
  }
  if (s->borrow) {
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
    if (s->fail_borrow)
      cpkt_types_fail_after(0);
#endif
    return cpkt_opcua_server_valueSourceBorrow_typed(server, value, s->stored);
  }
  if (s->callback_status)
    return s->callback_status;
  if (s->deferred) {
    s->read = value;
    if (s->partial_read) {
      cpkt_opcua_StatusCode status = fill_read(value);
      if (status)
        return status;
      value->hasServerTimestamp = 1;
      value->serverTimestamp = minimum();
    }
    return CPKT_OPCUA_STATUSCODE_GOODCOMPLETESASYNCHRONOUSLY;
  }
  return fill_read(value);
}
static cpkt_opcua_StatusCode
replacement_read(cpkt_opcua_server *server, const cpkt_opcua_NodeId *session,
                 void *session_context, const cpkt_opcua_NodeId *id,
                 void *context, cpkt_opcua_Boolean timestamp,
                 const cpkt_opcua_NumericRange *range,
                 cpkt_opcua_DataValue *value) {
  struct state *s = context;
  ++s->replacement_calls;
  return read_source(server, session, session_context, id, context, timestamp,
                     range, value);
}
static cpkt_opcua_StatusCode
write_source(cpkt_opcua_server *server, const cpkt_opcua_NodeId *session,
             void *session_context, const cpkt_opcua_NodeId *id, void *context,
             const cpkt_opcua_NumericRange *range,
             const cpkt_opcua_DataValue *value) {
  struct state *s = context;
  const cpkt_opcua_Int64 *number;
  CHECK(s == current);
  metadata(s, server, session, session_context, id, 6200, range);
  CHECK(value && value->hasValue &&
        value->value.type == cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64));
  number = (const cpkt_opcua_Int64 *)value->value.data;
  CHECK(number && number->high32 == 0x80000000U && !number->low32);
  if (s->direct)
    CHECK(value->hasStatus &&
          value->status == CPKT_OPCUA_STATUSCODE_UNCERTAINSUBNORMAL &&
          value->hasSourceTimestamp &&
          value->sourceTimestamp.high32 == 0x7fffffffU &&
          value->sourceTimestamp.low32 == 0xffffffffU &&
          value->sourcePicoseconds == 123);
  ++s->callbacks[1];
  CHECK(cpkt_opcua_server_setAsyncWriteResult_typed(server, value, 0) ==
        CPKT_OPCUA_STATUSCODE_BADNOTFOUND);
  if (s->callback_status)
    return s->callback_status;
  if (s->deferred) {
    s->write = value;
    return CPKT_OPCUA_STATUSCODE_GOODCOMPLETESASYNCHRONOUSLY;
  }
  return 0;
}
static cpkt_opcua_StatusCode
method_source(cpkt_opcua_server *server, const cpkt_opcua_NodeId *session,
              void *session_context, const cpkt_opcua_NodeId *method,
              void *method_context, const cpkt_opcua_NodeId *object,
              void *object_context, size_t input_count,
              const cpkt_opcua_Variant *inputs, size_t output_count,
              cpkt_opcua_Variant *outputs) {
  struct state *s = method_context;
  CHECK(s == current && output_count == 2 && outputs);
  metadata(s, server, session, session_context, method, 6201, NULL);
  CHECK(object && object->namespaceIndex == 0 &&
        object->identifier.numeric == CPKT_OPCUA_NS0ID_OBJECTSFOLDER);
  if (s->direct) {
    const cpkt_opcua_Int64 *number;
    const cpkt_opcua_String *string;
    CHECK(object_context == s && input_count == 2);
    CHECK(inputs[0].type == cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64) &&
          inputs[1].type == cpkt_opcua_type_at(CPKT_OPCUA_TYPES_STRING));
    number = (const cpkt_opcua_Int64 *)inputs[0].data;
    string = (const cpkt_opcua_String *)inputs[1].data;
    CHECK(number->high32 == 0x80000000U && !number->low32 &&
          string->length == 3 && !memcmp(string->data, "a\0b", 3));
  } else
    CHECK(input_count == 0);
  ++s->callbacks[2];
  if (s->replace_method) {
    s->replace_method = 0;
    CHECK(cpkt_opcua_server_setMethodNodeCallback_typed(server, *method,
                                                        NULL) == 0);
  }
  CHECK(cpkt_opcua_server_setAsyncCallMethodResult_typed(server, outputs, 0) ==
        CPKT_OPCUA_STATUSCODE_BADNOTFOUND);
  if (s->callback_status)
    return s->callback_status;
  if (s->deferred) {
    s->call = outputs;
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
    if (s->fail_async)
      cpkt_types_fail_after(0);
#endif
    return CPKT_OPCUA_STATUSCODE_GOODCOMPLETESASYNCHRONOUSLY;
  }
  if (s->borrow_method) {
    cpkt_opcua_Int64 number = maximum();
    cpkt_opcua_StatusCode status = cpkt_opcua_server_methodResultBorrow_typed(
        server, outputs, 0, s->stored);
    if (status)
      return status;
    CHECK(cpkt_opcua_server_methodResultBorrow_typed(server, outputs, 2,
                                                     s->stored) ==
          CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
    return cpkt_opcua_Variant_setScalarCopy(
        &outputs[1], &number, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64));
  }
  return fill_call(outputs);
}
static void cancelled(cpkt_opcua_server *server, const void *out) {
  struct state *s = current;
  CHECK(server == s->server);
  ++s->cancellations;
  /* Destruction already controls native cancellation; this must be a no-op
   * during shutdown, rather than recursively operating on a half-deleted queue.
   */
  if (s->result_status[0] == CPKT_OPCUA_STATUSCODE_BADSHUTDOWN)
    cpkt_opcua_server_cancelAsync_typed(server, s,
                                        CPKT_OPCUA_STATUSCODE_BADTIMEOUT, 1);
  if (out == s->read) {
    CHECK(cpkt_opcua_server_setAsyncReadResult_typed(server, s->read) ==
          CPKT_OPCUA_STATUSCODE_BADNOTFOUND);
    s->read = NULL;
  } else if (out == s->write) {
    CHECK(cpkt_opcua_server_setAsyncWriteResult_typed(server, s->write, 0) ==
          CPKT_OPCUA_STATUSCODE_BADNOTFOUND);
    s->write = NULL;
  } else {
    CHECK(out == s->call);
    CHECK(cpkt_opcua_server_setAsyncCallMethodResult_typed(
              server, s->call, 0) == CPKT_OPCUA_STATUSCODE_BADNOTFOUND);
    s->call = NULL;
  }
}
static void read_result(cpkt_opcua_server *server, void *context,
                        cpkt_opcua_StatusCode conversion,
                        const cpkt_opcua_DataValue *value) {
  struct state *s = context;
  CHECK(server == s->server && !conversion && value);
  ++s->results[0];
  s->result_status[0] = value->hasStatus ? value->status : 0;
  if (value->hasValue) {
    const cpkt_opcua_Int64 *number = value->value.data;
    CHECK(value->value.type == cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64) &&
          number->high32 == 0x80000000U && !number->low32);
  }
}
static void write_result(cpkt_opcua_server *server, void *context,
                         cpkt_opcua_StatusCode status) {
  struct state *s = context;
  CHECK(server == s->server);
  ++s->results[1];
  s->result_status[1] = status;
}
static void call_result(cpkt_opcua_server *server, void *context,
                        cpkt_opcua_StatusCode conversion,
                        const cpkt_opcua_CallMethodResult *result) {
  struct state *s = context;
  CHECK(server == s->server && !conversion && result);
  ++s->results[2];
  s->result_status[2] = result->statusCode;
  if (!result->statusCode) {
    const cpkt_opcua_Int64 *first, *second;
    CHECK(result->outputArgumentsSize == 2);
    first = result->outputArguments[0].data;
    second = result->outputArguments[1].data;
    CHECK(first && second && first->high32 == 0x80000000U && !first->low32 &&
          second->high32 == 0x7fffffffU && second->low32 == 0xffffffffU);
  }
}
static cpkt_opcua_status install(void *native, void *context) {
  return cpkt_types_peer_async_install(native, context);
}
struct direct_call {
  struct state *state;
  int kind;
  struct cpkt_producer_observed observed;
  cpkt_opcua_StatusCode status;
};
static cpkt_opcua_status direct(void *native, void *context) {
  struct direct_call *call = context;
  call->status = cpkt_types_peer_producer_direct(native, call->kind,
                                                 call->state, &call->observed);
  return call->status;
}

static cpkt_opcua_status install_zero(void *native, void *context) {
  return cpkt_types_peer_producer_zero_methods(native, context);
}
static cpkt_opcua_StatusCode
zero_method(cpkt_opcua_server *server, const cpkt_opcua_NodeId *session,
            void *session_context, const cpkt_opcua_NodeId *method,
            void *method_context, const cpkt_opcua_NodeId *object,
            void *object_context, size_t input_count,
            const cpkt_opcua_Variant *inputs, size_t output_count,
            cpkt_opcua_Variant *outputs) {
  struct state *s = method_context;
  unsigned int index = method->identifier.numeric - 6202;
  (void)session;
  (void)session_context;
  (void)object;
  (void)object_context;
  (void)inputs;
  CHECK(server == s->server && index < 2 && !input_count && !output_count &&
        outputs);
  s->zero[index] = outputs;
  return CPKT_OPCUA_STATUSCODE_GOODCOMPLETESASYNCHRONOUSLY;
}
static void zero_result(cpkt_opcua_server *server, void *context,
                        cpkt_opcua_StatusCode conversion,
                        const cpkt_opcua_CallMethodResult *result) {
  struct state *s = context;

  CHECK(server == s->server && !conversion && result && !result->statusCode &&
        !result->outputArgumentsSize);
  ++s->zero_results;
}
static void submit(struct state *s, int kind) {
  cpkt_opcua_ReadValueId read;
  cpkt_opcua_WriteValue write;
  cpkt_opcua_CallMethodRequest call;
  cpkt_opcua_Int64 number = minimum();
  if (kind == 0) {
    cpkt_opcua_ReadValueId_init(&read);
    read.nodeId = node(6200);
    read.attributeId = 13 /* Native AttributeId Value. */;
    CHECK(cpkt_opcua_server_read_async_typed(s->server, &read,
                                             cpkt_opcua_TIMESTAMPSTORETURN_BOTH,
                                             read_result, s, 0) == 0);
  } else if (kind == 1) {
    cpkt_opcua_WriteValue_init(&write);
    write.nodeId = node(6200);
    write.attributeId = 13 /* Native AttributeId Value. */;
    write.value.hasValue = 1;
    write.value.value.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
    write.value.value.data = &number;
    write.value.value.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
    CHECK(cpkt_opcua_server_write_async_typed(s->server, &write, write_result,
                                              s, 0) == 0);
  } else {
    cpkt_opcua_CallMethodRequest_init(&call);
    call.methodId = node(6201);
    call.objectId.identifier.numeric = CPKT_OPCUA_NS0ID_OBJECTSFOLDER;
    CHECK(cpkt_opcua_server_call_async_typed(s->server, &call, call_result, s,
                                             0) == 0);
  }
}
static void iterate(struct state *s) {
  unsigned short delay;
  CHECK(cpkt_opcua_server_iterate(s->server, 0, &delay) == CPKT_OPCUA_OK);
}
static void install_typed(struct state *s) {
  cpkt_opcua_CallbackValueSource source;
  source.read = read_source;
  source.write = write_source;
  CHECK(cpkt_opcua_server_setVariableNode_callbackValueSource_typed(
            s->server, node(6200), source) == 0);
  CHECK(cpkt_opcua_server_setMethodNodeCallback_typed(s->server, node(6201),
                                                      method_source) == 0);
}
void cpkt_types_test_producers(void) {
  struct state s;
  struct direct_call operation;
  cpkt_opcua_MethodCallback method;
  cpkt_opcua_CallbackValueSource source;
  cpkt_opcua_DataValue stored;
  int kind, initial;
  memset(&s, 0, sizeof(s));
  current = &s;
  CHECK(cpkt_opcua_server_new(&s.server, 0) == CPKT_OPCUA_OK);
  CHECK(cpkt_opcua_server_native(s.server, install, &s) == CPKT_OPCUA_OK);
  CHECK(cpkt_opcua_server_getMethodNodeCallback_typed(s.server, node(6201),
                                                      &method) ==
            CPKT_OPCUA_STATUSCODE_BADNOTSUPPORTED &&
        method == NULL);
  install_typed(&s);
  CHECK(cpkt_opcua_server_set_async_operation_cancel_callback_typed(
            s.server, cancelled) == 0);
  CHECK(cpkt_opcua_server_getMethodNodeCallback_typed(s.server, node(6201),
                                                      &method) == 0 &&
        method == method_source);
  s.direct = 1;
  memset(&operation, 0, sizeof(operation));
  operation.state = &s;
  for (kind = 0; kind < 3; ++kind) {
    operation.kind = kind;
    CHECK(cpkt_opcua_server_native(s.server, direct, &operation) ==
          CPKT_OPCUA_OK);
    if (kind != 1)
      CHECK(operation.observed.high[0] == 0x80000000U &&
            !operation.observed.low[0]);
    if (kind == 2)
      CHECK(operation.observed.high[1] == 0x7fffffffU &&
            operation.observed.low[1] == 0xffffffffU);
  }
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  for (kind = 0; kind < 3; ++kind) {
    size_t countdown;
    for (countdown = 0; countdown < 100; ++countdown) {
      cpkt_opcua_result result;
      int failed;
      operation.kind = kind;
      cpkt_types_fail_after(countdown);
      result = cpkt_opcua_server_native(s.server, direct, &operation);
      failed = cpkt_types_fail_stop();
      if (failed)
        CHECK(result == CPKT_OPCUA_ERR_UPSTREAM &&
              operation.status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY);
      else {
        CHECK(result == CPKT_OPCUA_OK);
        break;
      }
    }
    CHECK(countdown > 0 && countdown < 100);
  }
  for (kind = 0; kind < 2; ++kind) {
    size_t countdown;
    for (countdown = 0; countdown < 100; ++countdown) {
      cpkt_opcua_StatusCode status;
      int failed;
      source.read = replacement_read;
      source.write = write_source;
      cpkt_types_fail_after(countdown);
      status =
          kind ? cpkt_opcua_server_setMethodNodeCallback_typed(s.server,
                                                               node(6201), NULL)
               : cpkt_opcua_server_setVariableNode_callbackValueSource_typed(
                     s.server, node(6200), source);
      failed = cpkt_types_fail_stop();
      if (failed) {
        int replacements = s.replacement_calls;
        CHECK(status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY);
        CHECK(cpkt_opcua_server_getMethodNodeCallback_typed(
                  s.server, node(6201), &method) == 0 &&
              method == method_source);
        operation.kind = 0;
        CHECK(cpkt_opcua_server_native(s.server, direct, &operation) ==
                  CPKT_OPCUA_OK &&
              s.replacement_calls == replacements);
      } else {
        CHECK(status == 0);
        install_typed(&s);
        break;
      }
    }
    CHECK(countdown > 0 && countdown < 100);
  }
#endif
  s.replace = 1;
  operation.kind = 0;
  CHECK(cpkt_opcua_server_native(s.server, direct, &operation) ==
        CPKT_OPCUA_OK);
  CHECK(cpkt_opcua_server_native(s.server, direct, &operation) ==
            CPKT_OPCUA_OK &&
        s.replacement_calls == 1);
  install_typed(&s);
  s.callback_status = CPKT_OPCUA_STATUSCODE_BADNOTFOUND;
  for (kind = 0; kind < 3; ++kind) {
    operation.kind = kind;
    CHECK(cpkt_opcua_server_native(s.server, direct, &operation) ==
          CPKT_OPCUA_ERR_UPSTREAM);
  }
  s.callback_status = 0;
  s.replace_method = 1;
  operation.kind = 2;
  CHECK(cpkt_opcua_server_native(s.server, direct, &operation) ==
        CPKT_OPCUA_OK);
  CHECK(cpkt_opcua_server_getMethodNodeCallback_typed(s.server, node(6201),
                                                      &method) == 0 &&
        !method);
  install_typed(&s);
  cpkt_opcua_DataValue_init(&stored);
  CHECK(fill_read(&stored) == 0);
  CHECK(cpkt_opcua_history_value_new(&stored, &s.stored) == 0);
  cpkt_opcua_DataValue_clear(&stored);
  s.borrow = 1;
  operation.kind = 0;
  CHECK(cpkt_opcua_server_native(s.server, direct, &operation) ==
            CPKT_OPCUA_OK &&
        operation.observed.borrowed);
  {
    const void *address = operation.observed.address;
    CHECK(cpkt_opcua_server_native(s.server, direct, &operation) ==
              CPKT_OPCUA_OK &&
          operation.observed.address == address);
  }
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  s.fail_borrow = 1;
  CHECK(cpkt_opcua_server_native(s.server, direct, &operation) ==
        CPKT_OPCUA_OK);
  CHECK(!cpkt_types_fail_stop());
  s.fail_borrow = 0;
#endif
  s.borrow = 0;
  s.borrow_method = 1;
  operation.kind = 2;
  CHECK(cpkt_opcua_server_native(s.server, direct, &operation) ==
            CPKT_OPCUA_OK &&
        operation.observed.borrowed);
  {
    const void *address = operation.observed.address;
    CHECK(cpkt_opcua_server_native(s.server, direct, &operation) ==
              CPKT_OPCUA_OK &&
          operation.observed.address == address);
  }
  s.borrow_method = 0;
  s.direct = 0;
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  s.deferred = 1;
  s.fail_async = 1;
  submit(&s, 2);
  CHECK(cpkt_types_fail_stop()); /* initial staging failed after the producer
                                    returned async */
  CHECK(s.call == NULL && s.cancellations == 1 && s.results[2] == 1 &&
        s.result_status[2] == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY);
  s.results[2] = 0;
  s.cancellations = 0;
  s.deferred = 0;
  s.fail_async = 0;
#endif
  for (kind = 0; kind < 3; ++kind) {
    submit(&s, kind);
    CHECK(s.results[kind] == 1);
  }
  s.deferred = 1;
  for (kind = 0; kind < 3; ++kind)
    submit(&s, kind);
  CHECK(s.read && s.write && s.call && s.results[0] == 1 && s.results[1] == 1 &&
        s.results[2] == 1);
  CHECK(cpkt_opcua_server_setAsyncReadResult_typed(
            s.server, (cpkt_opcua_DataValue *)s.write) ==
        CPKT_OPCUA_STATUSCODE_BADNOTFOUND);
  CHECK(cpkt_opcua_server_setAsyncWriteResult_typed(s.server, s.read, 0) ==
        CPKT_OPCUA_STATUSCODE_BADNOTFOUND);
  CHECK(cpkt_opcua_server_setAsyncReadResult_typed(s.server, NULL) ==
        CPKT_OPCUA_STATUSCODE_BADNOTFOUND);
  CHECK(s.read->hasSourceTimestamp && s.read->hasServerTimestamp);
  CHECK(fill_read(s.read) == 0 && fill_call(s.call) == 0);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  for (kind = 0; kind < 2; ++kind) {
    size_t countdown;
    for (countdown = 0; countdown < 100; ++countdown) {
      cpkt_opcua_StatusCode status;
      int failed;
      cpkt_types_fail_after(countdown);
      status =
          kind ? cpkt_opcua_server_setAsyncCallMethodResult_typed(s.server,
                                                                  s.call, 0)
               : cpkt_opcua_server_setAsyncReadResult_typed(s.server, s.read);
      failed = cpkt_types_fail_stop();
      if (failed)
        CHECK(status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY &&
              s.results[kind ? 2 : 0] == 1);
      else {
        CHECK(status == 0);
        break;
      }
    }
    CHECK(countdown > 0 && countdown < 100);
  }
#else
  CHECK(cpkt_opcua_server_setAsyncReadResult_typed(s.server, s.read) == 0);
  CHECK(cpkt_opcua_server_setAsyncCallMethodResult_typed(s.server, s.call, 0) ==
        0);
#endif
  s.read = NULL;
  s.call = NULL;
  CHECK(cpkt_opcua_server_setAsyncWriteResult_typed(
            s.server, s.write, CPKT_OPCUA_STATUSCODE_BADNOTWRITABLE) == 0);
  s.write = NULL;
  CHECK(s.results[0] == 1 && s.results[1] == 1 && s.results[2] == 1);
  iterate(&s);
  CHECK(s.results[0] == 2 && s.results[1] == 2 && s.results[2] == 2);
  CHECK(s.result_status[0] == CPKT_OPCUA_STATUSCODE_UNCERTAINSUBNORMAL &&
        s.result_status[1] == CPKT_OPCUA_STATUSCODE_BADNOTWRITABLE);
  for (kind = 0; kind < 3; ++kind)
    submit(&s, kind);
  initial = s.native.cancelled;
  cpkt_opcua_server_cancelAsync_typed(s.server, &s,
                                      CPKT_OPCUA_STATUSCODE_BADTIMEOUT, 1);
  CHECK(!s.read && !s.write && !s.call && s.cancellations == 3 &&
        s.native.cancelled == initial + 3);
  for (kind = 0; kind < 3; ++kind)
    CHECK(s.results[kind] == 3 &&
          s.result_status[kind] == CPKT_OPCUA_STATUSCODE_BADTIMEOUT);
  submit(&s, 0);
  CHECK(cpkt_opcua_server_valueSourceBorrow_typed(s.server, s.read, s.stored) ==
        0);
  CHECK(cpkt_opcua_server_setAsyncReadResult_typed(s.server, s.read) == 0);
  s.read = NULL;
  iterate(&s);
  CHECK(s.results[0] == 4 &&
        s.result_status[0] == CPKT_OPCUA_STATUSCODE_UNCERTAINSUBNORMAL);
  CHECK(cpkt_opcua_server_set_async_operation_cancel_callback_typed(s.server,
                                                                    NULL) == 0);
  submit(&s, 0);
  cpkt_opcua_server_cancelAsync_typed(s.server, &s,
                                      CPKT_OPCUA_STATUSCODE_BADTIMEOUT, 1);
  CHECK(s.results[0] == 5 && s.cancellations == 3);
  s.read = NULL;
  CHECK(cpkt_opcua_server_set_async_operation_cancel_callback_typed(
            s.server, cancelled) == 0);
  CHECK(cpkt_opcua_server_setMethodNodeCallback_typed(s.server, node(6201),
                                                      NULL) == 0);
  CHECK(cpkt_opcua_server_getMethodNodeCallback_typed(s.server, node(6201),
                                                      &method) == 0 &&
        method == NULL);
  source.read = read_source;
  source.write = write_source;
  CHECK(cpkt_opcua_server_setVariableNode_callbackValueSource_typed(
            s.server, node(99999), source) ==
        CPKT_OPCUA_STATUSCODE_BADNODEIDUNKNOWN);
  CHECK(cpkt_opcua_server_setMethodNodeCallback_typed(s.server, node(6200),
                                                      method_source) != 0);
  install_typed(&s);
  {
    cpkt_opcua_ReadValueId read;
    cpkt_opcua_ReadValueId_init(&read);
    read.nodeId = node(6200);
    read.attributeId = 13;
    s.partial_read = s.filter_source = 1;
    CHECK(cpkt_opcua_server_read_async_typed(
              s.server, &read, cpkt_opcua_TIMESTAMPSTORETURN_SERVER,
              read_result, &s, 0) == 0);
    CHECK(s.read && s.read->hasValue && s.read->hasServerTimestamp &&
          !s.read->hasSourceTimestamp && !s.read->hasSourcePicoseconds);
    CHECK(s.read->serverTimestamp.high32 != 0x80000000U);
    s.read->sourceTimestamp = maximum();
    s.read->hasSourceTimestamp = 1;
    CHECK(cpkt_opcua_server_refresh_async_producer_metadata(s.server) == 0 &&
          s.read->hasSourceTimestamp);
    CHECK(cpkt_opcua_server_setAsyncReadResult_typed(s.server, s.read) == 0);
    s.read = NULL;
    iterate(&s);
    CHECK(s.results[0] == 6 &&
          s.result_status[0] == CPKT_OPCUA_STATUSCODE_UNCERTAINSUBNORMAL);
    s.partial_read = s.filter_source = 0;
  }
  CHECK(cpkt_opcua_server_native(s.server, install_zero, &s) == CPKT_OPCUA_OK);
  {
    cpkt_opcua_CallMethodRequest call;
    for (kind = 0; kind < 2; ++kind) {
      CHECK(cpkt_opcua_server_setMethodNodeCallback_typed(
                s.server, node(6202 + kind), zero_method) == 0);
      cpkt_opcua_CallMethodRequest_init(&call);
      call.methodId = node(6202 + kind);
      call.objectId.identifier.numeric = CPKT_OPCUA_NS0ID_OBJECTSFOLDER;
      CHECK(cpkt_opcua_server_call_async_typed(s.server, &call, zero_result, &s,
                                               0) == 0);
    }
    CHECK(s.zero[0] && s.zero[1] && s.zero[0] != s.zero[1]);
    CHECK(cpkt_opcua_server_setAsyncCallMethodResult_typed(s.server, s.zero[1],
                                                           0) == 0);
    CHECK(cpkt_opcua_server_setAsyncCallMethodResult_typed(s.server, s.zero[0],
                                                           0) == 0);
    s.zero[0] = s.zero[1] = NULL;
    iterate(&s);
    CHECK(s.zero_results == 2);
  }
  for (kind = 0; kind < 3; ++kind)
    submit(&s, kind);
  /* Exercise cancellation reentry during destruction without changing the
   * native shutdown result. */
  s.result_status[0] = CPKT_OPCUA_STATUSCODE_BADSHUTDOWN;
  cpkt_opcua_server_free(s.server);
  CHECK(!s.read && !s.write && !s.call && s.cancellations == 6);
  for (kind = 0; kind < 3; ++kind)
    CHECK(s.results[kind] == (kind ? 4 : 7) &&
          s.result_status[kind] == CPKT_OPCUA_STATUSCODE_BADSHUTDOWN);
  cpkt_opcua_history_value_free(s.stored);
  current = NULL;
}
