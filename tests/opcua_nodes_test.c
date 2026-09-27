#include "opcua_callbacks_test.h"
#include "opcua_types_peer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void check(int ok, const char *expression, int line) {
  if (!ok) {
    fprintf(stderr, "node callback line %d: %s\n", line, expression);
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
  int marker, replaced, direct, replace, action, replace_global, replace_type,
      reject_child;
  cpkt_opcua_StatusCode return_status;
  unsigned int calls[8], errors;
};
static struct state *states[2];
static struct state *state_of(cpkt_opcua_server *server) {
  size_t i;
  for (i = 0; i < 2; ++i)
    if (states[i] && states[i]->server == server)
      return states[i];
  CHECK(0);
  return NULL;
}
static cpkt_opcua_NodeId number(unsigned int id) {
  cpkt_opcua_NodeId node;
  cpkt_opcua_NodeId_init(&node);
  node.namespaceIndex = 1;
  node.identifier.numeric = id;
  return node;
}
static void ids(struct state *s, const cpkt_opcua_NodeId *session,
                void *session_context, const cpkt_opcua_NodeId *node) {
  CHECK(node && session);
  if (s->direct) {
    CHECK(session->namespaceIndex == 2 &&
          session->identifierType == CPKT_OPCUA_NODEIDTYPE_STRING &&
          session->identifier.string.length == 7 &&
          !memcmp(session->identifier.string.data, "session", 7));
    CHECK(session_context == &s->marker);
  }
}
static void notification(cpkt_opcua_server *server,
                         const cpkt_opcua_NodeId *session,
                         void *session_context, const cpkt_opcua_NodeId *node,
                         void *context, const cpkt_opcua_NumericRange *range,
                         const cpkt_opcua_DataValue *value, int write) {
  struct state *s = state_of(server);
  cpkt_opcua_ValueSourceNotifications empty;
  cpkt_opcua_Int64 *integer;
  ids(s, session, session_context, node);
  CHECK(context == &s->marker && node->identifier.numeric == 7000);
  CHECK(value && value->hasValue &&
        value->value.type == cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64));
  integer = value->value.data;
  CHECK(integer && integer->high32 == 0x80000000U && !integer->low32);
  if (s->direct) {
    CHECK(value->hasSourceTimestamp &&
          value->sourceTimestamp.high32 == 0x7fffffffU &&
          value->sourceTimestamp.low32 == 0xffffffffU);
    CHECK(value->hasServerTimestamp &&
          value->serverTimestamp.high32 == 0x80000000U &&
          !value->serverTimestamp.low32);
    CHECK(value->hasStatus &&
          value->status == CPKT_OPCUA_STATUSCODE_GOODCLAMPED);
    CHECK(value->hasSourcePicoseconds && value->sourcePicoseconds == 321 &&
          value->hasServerPicoseconds && value->serverPicoseconds == 654);
    if (!write)
      CHECK(range && range->dimensionsSize == 1 &&
            range->dimensions[0].min == 1 && range->dimensions[0].max == 1);
    else
      CHECK(!range);
  }
  ++s->calls[write];
  if (s->replace) {
    s->replace = 0;
    memset(&empty, 0, sizeof(empty));
    CHECK(!cpkt_opcua_server_setVariableNode_internalValueSource_typed(
        server, *node, NULL, &empty));
    CHECK(integer->high32 == 0x80000000U);
  }
}
static void on_read(cpkt_opcua_server *server, const cpkt_opcua_NodeId *session,
                    void *session_context, const cpkt_opcua_NodeId *node,
                    void *context, const cpkt_opcua_NumericRange *range,
                    const cpkt_opcua_DataValue *value) {
  notification(server, session, session_context, node, context, range, value,
               0);
}
static void on_write(cpkt_opcua_server *server,
                     const cpkt_opcua_NodeId *session, void *session_context,
                     const cpkt_opcua_NodeId *node, void *context,
                     const cpkt_opcua_NumericRange *range,
                     const cpkt_opcua_DataValue *value) {
  notification(server, session, session_context, node, context, range, value,
               1);
}
static cpkt_opcua_StatusCode
global_constructor(cpkt_opcua_server *server, const cpkt_opcua_NodeId *session,
                   void *session_context, const cpkt_opcua_NodeId *node,
                   void **context) {
  struct state *s = state_of(server);
  ids(s, session, session_context, node);
  ++s->calls[2];
  if (s->direct)
    CHECK(*context == &s->marker);
  *context = &s->replaced;
  if (s->replace_global) {
    s->replace_global = 0;
    CHECK(!cpkt_opcua_server_set_global_node_lifecycle(server, NULL));
  }
  return s->return_status;
}
static void global_destructor(cpkt_opcua_server *server,
                              const cpkt_opcua_NodeId *session,
                              void *session_context,
                              const cpkt_opcua_NodeId *node, void *context) {
  struct state *s = state_of(server);
  ids(s, session, session_context, node);
  ++s->calls[3];
  if (s->direct)
    CHECK(context == &s->marker);
}
static cpkt_opcua_Boolean optional_child(cpkt_opcua_server *server,
                                         const cpkt_opcua_NodeId *session,
                                         void *session_context,
                                         const cpkt_opcua_NodeId *source,
                                         const cpkt_opcua_NodeId *parent,
                                         const cpkt_opcua_NodeId *reference) {
  struct state *s = state_of(server);
  ids(s, session, session_context, source);
  CHECK(parent && reference);
  ++s->calls[4];
  return s->reject_child ? 0 : 1;
}
static cpkt_opcua_StatusCode
child_id(cpkt_opcua_server *server, const cpkt_opcua_NodeId *session,
         void *session_context, const cpkt_opcua_NodeId *source,
         const cpkt_opcua_NodeId *parent, const cpkt_opcua_NodeId *reference,
         cpkt_opcua_NodeId *target) {
  struct state *s = state_of(server);
  ids(s, session, session_context, source);
  CHECK(parent && reference && target);
  ++s->calls[5];
  CHECK(target->namespaceIndex == 1);
  target->identifierType = CPKT_OPCUA_NODEIDTYPE_STRING;
  target->identifier.string = cpkt_opcua_String_fromChars("child");
  return target->identifier.string.data ? 0
                                        : CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY;
}
static cpkt_opcua_StatusCode
type_constructor(cpkt_opcua_server *server, const cpkt_opcua_NodeId *session,
                 void *session_context, const cpkt_opcua_NodeId *type,
                 void *type_context, const cpkt_opcua_NodeId *node,
                 void **context) {
  struct state *s = state_of(server);
  ids(s, session, session_context, node);
  CHECK(type && type->identifier.numeric == 7001 && type_context == &s->marker);
  ++s->calls[6];
  if (s->direct)
    CHECK(*context == &s->marker);
  *context = &s->replaced;
  if (s->replace_type) {
    cpkt_opcua_NodeTypeLifecycle empty;
    s->replace_type = 0;
    memset(&empty, 0, sizeof(empty));
    CHECK(!cpkt_opcua_server_setNodeTypeLifecycle_typed(server, *type, empty));
  }
  return s->return_status;
}
static void type_destructor(cpkt_opcua_server *server,
                            const cpkt_opcua_NodeId *session,
                            void *session_context,
                            const cpkt_opcua_NodeId *type, void *type_context,
                            const cpkt_opcua_NodeId *node, void **context) {
  struct state *s = state_of(server);
  ids(s, session, session_context, node);
  CHECK(type && type->identifier.numeric == 7001 && type_context == &s->marker);
  ++s->calls[7];
  if (s->direct)
    CHECK(*context == &s->marker);
  *context = &s->replaced;
}
static void logger(const cpkt_opcua_log_record *record, void *context) {
  struct state *s = context;
  if (record->level == CPKT_OPCUA_LOG_ERROR && record->message &&
      strstr(record->message, "C89 "))
    ++s->errors;
}
static cpkt_opcua_status peer(void *native, void *context) {
  struct state *s = context;
  return cpkt_types_peer_nodes(native, s->action, &s->marker);
}
static unsigned int drive(struct state *s, int action) {
  s->action = action;
  return cpkt_opcua_server_native(s->server, peer, s) == CPKT_OPCUA_OK ? 0 : 1;
}
static cpkt_opcua_status capture(void *native, void *slot) {
  *(void **)slot = native;
  return 0;
}
void cpkt_types_test_nodes(void) {
  struct state state[2];
  cpkt_opcua_ValueSourceNotifications notifications;
  cpkt_opcua_NodeTypeLifecycle type;
  cpkt_opcua_GlobalNodeLifecycle global;
  cpkt_opcua_log_config logging;
  cpkt_opcua_DataValue value;
  cpkt_opcua_Int64 integer;
  void *context;
  size_t i;
  unsigned int before;
  memset(state, 0, sizeof(state));
  memset(&logging, 0, sizeof(logging));
  logging.fn = logger;
  notifications.onRead = on_read;
  notifications.onWrite = on_write;
  type.constructor = type_constructor;
  type.destructor = type_destructor;
  global.constructor = global_constructor;
  global.destructor = global_destructor;
  global.createOptionalChild = optional_child;
  global.generateChildNodeId = child_id;
  for (i = 0; i < 2; ++i) {
    states[i] = &state[i];
    logging.user = &state[i];
    CHECK(cpkt_opcua_server_new_with_logger(&state[i].server, 0, &logging) ==
          CPKT_OPCUA_OK);
    CHECK(!drive(&state[i], 0));
    state[i].direct = 1;
    CHECK(!cpkt_opcua_server_setVariableNode_internalValueSource_typed(
        state[i].server, number(7000), NULL, &notifications));
    CHECK(!cpkt_opcua_server_setNodeTypeLifecycle_typed(state[i].server,
                                                        number(7001), type));
    CHECK(
        !cpkt_opcua_server_set_global_node_lifecycle(state[i].server, &global));
  }
  for (i = 0; i < 2; ++i) {
    size_t j;
    for (j = 1; j <= 8; ++j)
      CHECK(!drive(&state[i], (int)j));
    for (j = 0; j < 8; ++j)
      CHECK(state[i].calls[j] == 1);
    CHECK(!cpkt_opcua_server_getNodeContext_typed(state[i].server, number(7000),
                                                  &context) &&
          context == &state[i].marker);
    state[i].direct = 0;
    CHECK(!drive(&state[i], 9) && !drive(&state[i], 10));
    state[i].direct = 1;
  }
  {
    void *native[2];
    unsigned int reads[2], writes[2];
    for (i = 0; i < 2; ++i) {
      state[i].direct = 0;
      reads[i] = state[i].calls[0];
      writes[i] = state[i].calls[1];
      CHECK(cpkt_opcua_server_native(state[i].server, capture, &native[i]) ==
            CPKT_OPCUA_OK);
    }
    CHECK(!cpkt_types_peer_nodes_parallel(native[0], native[1]));
    for (i = 0; i < 2; ++i) {
      CHECK(state[i].calls[0] == reads[i] + 8 &&
            state[i].calls[1] == writes[i] + 8);
      state[i].direct = 1;
    }
  }
  state[0].return_status = CPKT_OPCUA_STATUSCODE_BADNOTFOUND;
  before = state[0].errors;
  CHECK(drive(&state[0], 3) && drive(&state[0], 7));
  CHECK(state[0].errors == before);
  state[0].return_status = 0;
  state[0].reject_child = 1;
  CHECK(drive(&state[0], 5));
  state[0].reject_child = 0;
  state[0].replace_global = 1;
  CHECK(!drive(&state[0], 3));
  CHECK(drive(&state[0], 3));
  CHECK(!cpkt_opcua_server_set_global_node_lifecycle(state[0].server, &global));
  state[0].replace_type = 1;
  CHECK(!drive(&state[0], 7));
  CHECK(drive(&state[0], 7));
  CHECK(!cpkt_opcua_server_setNodeTypeLifecycle_typed(state[0].server,
                                                      number(7001), type));
  state[0].replace = 1;
  before = state[0].calls[0];
  CHECK(!drive(&state[0], 1));
  CHECK(state[0].calls[0] == before + 1);
  CHECK(!drive(&state[0], 1) && state[0].calls[0] == before + 1);
  CHECK(!cpkt_opcua_server_setVariableNode_internalValueSource_typed(
      state[0].server, number(7000), NULL, &notifications));
  CHECK(cpkt_opcua_server_setVariableNode_internalValueSource_typed(
            state[0].server, number(7999), NULL, &notifications) != 0);
  CHECK(cpkt_opcua_server_setNodeTypeLifecycle_typed(state[0].server,
                                                     number(7999), type) != 0);
  CHECK(cpkt_opcua_server_setNodeTypeLifecycle_typed(state[0].server,
                                                     number(7000), type) != 0);
  CHECK(!drive(&state[0], 1));
  integer.high32 = 0x80000000U;
  integer.low32 = 0;
  cpkt_opcua_DataValue_init(&value);
  value.hasValue = 1;
  cpkt_opcua_Variant_setScalar(&value.value, &integer,
                               cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64));
  value.value.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
  CHECK(!cpkt_opcua_server_setVariableNode_internalValueSource_typed(
      state[0].server, number(7000), &value, NULL));
  before = state[0].calls[0];
  CHECK(!drive(&state[0], 1) && state[0].calls[0] == before);
  CHECK(!cpkt_opcua_server_setVariableNode_internalValueSource_typed(
      state[0].server, number(7000), NULL, &notifications));
  state[0].direct = 0;
  CHECK(!drive(&state[0], 11));
  CHECK(state[0].calls[2] >= 2 && state[0].calls[6] >= 2);
  CHECK(!drive(&state[0], 12));
  CHECK(state[0].calls[3] >= 2 && state[0].calls[7] >= 2);
  state[0].direct = 1;
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  {
    size_t round, mode;
    cpkt_opcua_StatusCode status;
    int injected;
    for (mode = 0; mode < 3; ++mode) {
      for (round = 0; round < 32; ++round) {
        cpkt_types_fail_after(round);
        if (mode == 0)
          status = cpkt_opcua_server_setVariableNode_internalValueSource_typed(
              state[0].server, number(7000), &value, &notifications);
        else if (mode == 1)
          status = cpkt_opcua_server_setNodeTypeLifecycle_typed(
              state[0].server, number(7001), type);
        else {
          state[0].action = 6;
          status = cpkt_opcua_server_native(state[0].server, peer, &state[0]);
        }
        injected = cpkt_types_fail_stop();
        if (!injected) {
          CHECK(!status);
          break;
        }
        CHECK(status != 0);
        CHECK(!drive(&state[0], 1));
      }
      CHECK(round > 0 && round < 32);
    }
    {
      cpkt_opcua_server *unused;
      cpkt_opcua_log_config silent;
      memset(&silent, 0, sizeof(silent));
      CHECK(cpkt_opcua_server_new_with_logger(&unused, 0, &silent) ==
            CPKT_OPCUA_OK);
      cpkt_types_fail_after(0);
      CHECK(cpkt_opcua_server_set_global_node_lifecycle(unused, &global) ==
            CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY);
      CHECK(cpkt_types_fail_stop());
      CHECK(!cpkt_opcua_server_set_global_node_lifecycle(unused, NULL));
      cpkt_opcua_server_free(unused);
    }
    before = state[0].calls[0];
    cpkt_types_fail_after(0);
    CHECK(!drive(&state[0], 1));
    CHECK(cpkt_types_fail_stop());
    CHECK(state[0].calls[0] == before && state[0].errors > 0);
    before = state[0].calls[7];
    cpkt_types_fail_after(0);
    CHECK(!drive(&state[0], 8));
    CHECK(!cpkt_types_fail_stop() && state[0].calls[7] == before + 1);
  }
#endif
  memset(&type, 0, sizeof(type));
  CHECK(!cpkt_opcua_server_setNodeTypeLifecycle_typed(state[0].server,
                                                      number(7001), type));
  CHECK(!cpkt_opcua_server_set_global_node_lifecycle(state[0].server, NULL));
  CHECK(!drive(&state[0], 13));
  CHECK(cpkt_opcua_server_startup(state[1].server, NULL) == CPKT_OPCUA_OK);
  CHECK(cpkt_opcua_server_set_global_node_lifecycle(state[1].server, &global) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  for (i = 0; i < 2; ++i) {
    state[i].direct = 0;
    cpkt_opcua_server_free(state[i].server);
    states[i] = NULL;
  }
}
