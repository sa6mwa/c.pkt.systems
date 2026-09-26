#include "opcua_callbacks_test.h"
#include "opcua_types_peer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void check(int ok, const char *expression, int line) {
  if (!ok) {
    fprintf(stderr, "history backend line %d: %s\n", line, expression);
    abort();
  }
}
#define CHECK(expression) check(!!(expression), #expression, __LINE__)
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
void cpkt_types_fail_after(size_t count);
int cpkt_types_fail_stop(void);
#endif
typedef struct {
  cpkt_opcua_server *server;
  cpkt_opcua_history_value *values[2];
  unsigned int calls[16], cleared;
  int invalid_count;
} history_state;
static history_state *state_of(cpkt_opcua_server *server, void *context,
                               const cpkt_opcua_NodeId *node) {
  history_state *state = (history_state *)context;
  CHECK(server == state->server && node && node->namespaceIndex == 1);
  return state;
}
static void destroy(cpkt_opcua_HistoryDataBackend *backend) {
  history_state *state = (history_state *)backend->context;
  ++state->cleared;
  cpkt_opcua_history_value_free(state->values[0]);
  cpkt_opcua_history_value_free(state->values[1]);
  state->values[0] = state->values[1] = NULL;
}
static cpkt_opcua_StatusCode set_value(cpkt_opcua_server *server, void *context,
                                       const cpkt_opcua_NodeId *session,
                                       void *session_context,
                                       const cpkt_opcua_NodeId *node,
                                       cpkt_opcua_Boolean historizing,
                                       const cpkt_opcua_DataValue *value) {
  history_state *state = state_of(server, context, node);
  (void)session;
  (void)session_context;
  (void)historizing;
  CHECK(value && value->hasValue &&
        value->value.type == cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64));
  CHECK(((const cpkt_opcua_Int64 *)value->value.data)->high32 == 0x80000000U);
  ++state->calls[0];
  return 0;
}
static size_t match(cpkt_opcua_server *server, void *context,
                    const cpkt_opcua_NodeId *session, void *session_context,
                    const cpkt_opcua_NodeId *node, cpkt_opcua_DateTime stamp,
                    cpkt_opcua_MatchStrategy strategy) {
  history_state *state = state_of(server, context, node);
  size_t i;
  (void)session;
  (void)session_context;
  ++state->calls[1];
  for (i = 0; i < 2; ++i) {
    unsigned int t = i ? 200U : 100U;
    if (stamp.high32)
      break;
    if ((strategy == CPKT_OPCUA_MATCH_EQUAL && stamp.low32 == t) ||
        (strategy == CPKT_OPCUA_MATCH_AFTER && t > stamp.low32) ||
        (strategy == CPKT_OPCUA_MATCH_EQUAL_OR_AFTER && t >= stamp.low32))
      return i;
  }
  if (strategy == CPKT_OPCUA_MATCH_BEFORE ||
      strategy == CPKT_OPCUA_MATCH_EQUAL_OR_BEFORE) {
    for (i = 2; i > 0; --i) {
      unsigned int t = i == 2 ? 200U : 100U;
      if (stamp.high32 ||
          (strategy == CPKT_OPCUA_MATCH_BEFORE ? t < stamp.low32
                                               : t <= stamp.low32))
        return i - 1;
    }
  }
  return 2;
}
static size_t end(cpkt_opcua_server *server, void *context,
                  const cpkt_opcua_NodeId *session, void *session_context,
                  const cpkt_opcua_NodeId *node) {
  history_state *state = state_of(server, context, node);
  (void)session;
  (void)session_context;
  ++state->calls[2];
  return 2;
}
static size_t first(cpkt_opcua_server *server, void *context,
                    const cpkt_opcua_NodeId *session, void *session_context,
                    const cpkt_opcua_NodeId *node) {
  history_state *state = state_of(server, context, node);
  (void)session;
  (void)session_context;
  ++state->calls[3];
  return 0;
}
static size_t last(cpkt_opcua_server *server, void *context,
                   const cpkt_opcua_NodeId *session, void *session_context,
                   const cpkt_opcua_NodeId *node) {
  history_state *state = state_of(server, context, node);
  (void)session;
  (void)session_context;
  ++state->calls[4];
  return 1;
}
static size_t size_of(cpkt_opcua_server *server, void *context,
                      const cpkt_opcua_NodeId *session, void *session_context,
                      const cpkt_opcua_NodeId *node, size_t start,
                      size_t finish) {
  history_state *state = state_of(server, context, node);
  (void)session;
  (void)session_context;
  ++state->calls[5];
  return start < 2 && finish < 2 && start <= finish ? finish - start + 1 : 0;
}
static const cpkt_opcua_history_value *
get(cpkt_opcua_server *server, void *context, const cpkt_opcua_NodeId *session,
    void *session_context, const cpkt_opcua_NodeId *node, size_t index) {
  history_state *state = state_of(server, context, node);
  (void)session;
  (void)session_context;
  CHECK(index < 2);
  ++state->calls[6];
  return state->values[index];
}
static cpkt_opcua_Boolean bounds(cpkt_opcua_server *server, void *context,
                                 const cpkt_opcua_NodeId *session,
                                 void *session_context,
                                 const cpkt_opcua_NodeId *node) {
  history_state *state = state_of(server, context, node);
  (void)session;
  (void)session_context;
  ++state->calls[7];
  return 1;
}
static cpkt_opcua_Boolean timestamps(cpkt_opcua_server *server, void *context,
                                     const cpkt_opcua_NodeId *session,
                                     void *session_context,
                                     const cpkt_opcua_NodeId *node,
                                     cpkt_opcua_TimestampsToReturn requested) {
  history_state *state = state_of(server, context, node);
  (void)session;
  (void)session_context;
  CHECK(requested == cpkt_opcua_TIMESTAMPSTORETURN_BOTH);
  ++state->calls[8];
  return 1;
}
static cpkt_opcua_StatusCode
copy(cpkt_opcua_server *server, void *context, const cpkt_opcua_NodeId *session,
     void *session_context, const cpkt_opcua_NodeId *node, size_t start,
     size_t finish, cpkt_opcua_Boolean reverse, size_t capacity,
     cpkt_opcua_NumericRange range, cpkt_opcua_Boolean release,
     const cpkt_opcua_ByteString *continuation, cpkt_opcua_ByteString *out,
     size_t *provided, cpkt_opcua_DataValue *values) {
  history_state *state = state_of(server, context, node);
  size_t count, i;
  (void)session;
  (void)session_context;
  (void)release;
  (void)continuation;
  (void)out;
  CHECK(start < 2 && finish < 2);
  CHECK(!range.dimensionsSize ||
        (range.dimensionsSize == 1 && range.dimensions[0].min == 0 &&
         range.dimensions[0].max == 1));
  ++state->calls[9];
  count = reverse ? start - finish + 1 : finish - start + 1;
  if (count > capacity)
    count = capacity;
  for (i = 0; i < count; ++i) {
    cpkt_opcua_StatusCode status = cpkt_opcua_history_value_get(
        state->values[reverse ? start - i : start + i], &values[i]);
    if (status)
      return status;
  }
  *provided = state->invalid_count ? capacity + 1 : count;
  return 0;
}
static cpkt_opcua_StatusCode insert(cpkt_opcua_server *server, void *context,
                                    const cpkt_opcua_NodeId *session,
                                    void *session_context,
                                    const cpkt_opcua_NodeId *node,
                                    const cpkt_opcua_DataValue *value) {
  history_state *state = state_of(server, context, node);
  (void)session;
  (void)session_context;
  CHECK(value && value->hasValue &&
        ((const cpkt_opcua_Int64 *)value->value.data)->high32 == 0x80000000U);
  ++state->calls[10];
  return 0;
}
static cpkt_opcua_StatusCode replace(cpkt_opcua_server *server, void *context,
                                     const cpkt_opcua_NodeId *session,
                                     void *session_context,
                                     const cpkt_opcua_NodeId *node,
                                     const cpkt_opcua_DataValue *value) {
  history_state *state = state_of(server, context, node);
  ++state->calls[11];
  return insert(server, context, session, session_context, node, value);
}
static cpkt_opcua_StatusCode update(cpkt_opcua_server *server, void *context,
                                    const cpkt_opcua_NodeId *session,
                                    void *session_context,
                                    const cpkt_opcua_NodeId *node,
                                    const cpkt_opcua_DataValue *value) {
  history_state *state = state_of(server, context, node);
  ++state->calls[12];
  return insert(server, context, session, session_context, node, value);
}
static cpkt_opcua_StatusCode
remove_values(cpkt_opcua_server *server, void *context,
              const cpkt_opcua_NodeId *session, void *session_context,
              const cpkt_opcua_NodeId *node, cpkt_opcua_DateTime start,
              cpkt_opcua_DateTime finish) {
  history_state *state = state_of(server, context, node);
  (void)session;
  (void)session_context;
  CHECK(start.high32 == 0x80000000U && start.low32 == 0 &&
        finish.high32 == 0x7fffffffU && finish.low32 == 0xffffffffU);
  ++state->calls[13];
  return 0;
}
static cpkt_opcua_StatusCode
high_level(cpkt_opcua_server *server, const cpkt_opcua_NodeId *session,
           void *session_context, const cpkt_opcua_HistoryDataBackend *backend,
           cpkt_opcua_DateTime start, cpkt_opcua_DateTime finish,
           const cpkt_opcua_NodeId *node, size_t capacity,
           cpkt_opcua_UInt32 requested, cpkt_opcua_Boolean return_bounds,
           cpkt_opcua_TimestampsToReturn requested_timestamps,
           cpkt_opcua_NumericRange range, cpkt_opcua_Boolean release,
           const cpkt_opcua_ByteString *continuation,
           cpkt_opcua_ByteString *out, cpkt_opcua_HistoryData *result) {
  history_state *state = state_of(server, backend->context, node);
  (void)session;
  (void)session_context;
  (void)requested;
  (void)return_bounds;
  (void)release;
  CHECK(start.high32 == 0x80000000U && finish.low32 == 200 && capacity == 10);
  CHECK(requested_timestamps == cpkt_opcua_TIMESTAMPSTORETURN_BOTH &&
        range.dimensionsSize == 1);
  CHECK(continuation && continuation->length == 5 &&
        memcmp(continuation->data, "token", 5) == 0);
  result->dataValues = (cpkt_opcua_DataValue *)cpkt_opcua_array_new(
      2, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_DATAVALUE));
  if (!result->dataValues)
    return CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY;
  result->dataValuesSize = 2;
  if (cpkt_opcua_history_value_get(state->values[0], &result->dataValues[0]) ||
      cpkt_opcua_history_value_get(state->values[1], &result->dataValues[1]))
    return CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY;
  out->data = (cpkt_opcua_Byte *)cpkt_opcua_array_new(
      4, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_BYTE));
  if (!out->data)
    return CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY;
  out->length = 4;
  memcpy(out->data, "next", 4);
  ++state->calls[14];
  return 0;
}
static void initialize(history_state *state,
                       cpkt_opcua_HistorizingNodeIdSettings *settings) {
  cpkt_opcua_DataValue value;
  cpkt_opcua_Int64 number;
  size_t i;
  memset(settings, 0, sizeof(*settings));
  settings->historizingBackend.context = state;
  settings->historizingBackend.deleteMembers = destroy;
  settings->historizingBackend.serverSetHistoryData = set_value;
  settings->historizingBackend.getDateTimeMatch = match;
  settings->historizingBackend.getEnd = end;
  settings->historizingBackend.firstIndex = first;
  settings->historizingBackend.lastIndex = last;
  settings->historizingBackend.resultSize = size_of;
  settings->historizingBackend.getDataValue = get;
  settings->historizingBackend.boundSupported = bounds;
  settings->historizingBackend.timestampsToReturnSupported = timestamps;
  settings->historizingBackend.copyDataValues = copy;
  settings->historizingBackend.insertDataValue = insert;
  settings->historizingBackend.replaceDataValue = replace;
  settings->historizingBackend.updateDataValue = update;
  settings->historizingBackend.removeDataValue = remove_values;
  settings->maxHistoryDataResponseSize = 10;
  settings->historizingUpdateStrategy =
      CPKT_OPCUA_HISTORIZINGUPDATESTRATEGY_VALUESET;
  for (i = 0; i < 2; ++i) {
    cpkt_opcua_DataValue_init(&value);
    number.high32 = i ? 0x7fffffffU : 0x80000000U;
    number.low32 = i ? 0xffffffffU : 0;
    value.hasValue = value.hasSourceTimestamp = 1;
    value.sourceTimestamp.low32 = i ? 200U : 100U;
    value.value.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
    value.value.data = &number;
    value.value.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
    CHECK(cpkt_opcua_history_value_new(&value, &state->values[i]) == 0);
    number.high32 = number.low32 = 0;
  }
}
static cpkt_opcua_status native_engine(void *native, void *user) {
  return cpkt_types_peer_history_backend(native, *(int *)user);
}
static cpkt_opcua_status native_poll_node(void *native, void *user) {
  (void)user;
  return cpkt_types_peer_history_poll_node(native);
}
static void polling(void) {
  history_state state;
  cpkt_opcua_HistorizingNodeIdSettings settings;
  cpkt_opcua_NodeId node;
  memset(&state, 0, sizeof(state));
  CHECK(cpkt_opcua_server_new(&state.server, 0) == CPKT_OPCUA_OK);
  cpkt_opcua_NodeId_init(&node);
  node.namespaceIndex = 1;
  node.identifier.numeric = 6102;
  CHECK(cpkt_opcua_server_history_start_poll(state.server, &node) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_server_history_stop_poll(state.server, &node) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_server_set_default_history_database(state.server, 1) == 0);
  CHECK(cpkt_opcua_server_history_start_poll(state.server, &node) ==
        CPKT_OPCUA_STATUSCODE_BADNODEIDUNKNOWN);
  initialize(&state, &settings);
  settings.historizingUpdateStrategy =
      CPKT_OPCUA_HISTORIZINGUPDATESTRATEGY_POLL;
  settings.pollingInterval = 100;
  CHECK(cpkt_opcua_server_register_history_backend(state.server, &node,
                                                   &settings) == 0);
  CHECK(cpkt_opcua_server_native(state.server, native_poll_node, NULL) ==
        CPKT_OPCUA_OK);
  CHECK(cpkt_opcua_server_history_start_poll(state.server, &node) == 0);
  CHECK(cpkt_opcua_server_history_start_poll(state.server, &node) ==
        CPKT_OPCUA_STATUSCODE_BADMONITOREDITEMIDINVALID);
  CHECK(cpkt_opcua_server_history_stop_poll(state.server, &node) == 0);
  CHECK(cpkt_opcua_server_history_stop_poll(state.server, &node) ==
        CPKT_OPCUA_STATUSCODE_BADMONITOREDITEMIDINVALID);
  CHECK(cpkt_opcua_server_history_start_poll(state.server, &node) == 0);
  node.identifier.numeric = 6103;
  CHECK(cpkt_opcua_server_register_history_backend(state.server, &node,
                                                   &settings) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_server_set_default_history_database(state.server, 1) == 0);
  CHECK(state.cleared == 1);
  initialize(&state, &settings);
  settings.historizingUpdateStrategy =
      CPKT_OPCUA_HISTORIZINGUPDATESTRATEGY_POLL;
  settings.pollingInterval = 100;
  node.identifier.numeric = 6102;
  CHECK(cpkt_opcua_server_register_history_backend(state.server, &node,
                                                   &settings) == 0);
  CHECK(cpkt_opcua_server_history_start_poll(state.server, &node) == 0);
  cpkt_opcua_server_free(state.server);
  CHECK(state.cleared == 2);
}
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
static cpkt_opcua_status native_allocation_engine(void *native, void *user) {
  cpkt_opcua_status *status = (cpkt_opcua_status *)user;
  *status = cpkt_types_peer_history_backend(native, 2);
  return *status;
}
static void allocation_failures(void) {
  history_state state;
  cpkt_opcua_HistorizingNodeIdSettings settings;
  cpkt_opcua_NodeId node;
  cpkt_opcua_DataValue value, copied;
  cpkt_opcua_Int64 number;
  cpkt_opcua_history_value *stored;
  size_t i;
  memset(&state, 0, sizeof(state));
  CHECK(cpkt_opcua_server_new(&state.server, 0) == CPKT_OPCUA_OK);
  CHECK(cpkt_opcua_server_set_default_history_database(state.server, 1) == 0);
  initialize(&state, &settings);
  cpkt_opcua_NodeId_init(&node);
  node.namespaceIndex = 1;
  node.identifierType = CPKT_OPCUA_NODEIDTYPE_STRING;
  node.identifier.string.length = 6;
  node.identifier.string.data = (cpkt_opcua_Byte *)"stored";
  /* Includes bridge allocation, both NodeId copies, and later gather growth. */
  for (i = 0; i < 16; ++i) {
    cpkt_opcua_StatusCode status;
    int injected;
    cpkt_types_fail_after(i);
    status = cpkt_opcua_server_register_history_backend(state.server, &node,
                                                        &settings);
    injected = cpkt_types_fail_stop();
    if (!status) {
      CHECK(!injected);
      break;
    }
    CHECK(injected && status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY &&
          state.cleared == 0);
  }
  CHECK(i < 16);
  /* Duplicate registration keeps the original backend owned by the server. */
  CHECK(cpkt_opcua_server_register_history_backend(state.server, &node,
                                                   &settings) ==
        CPKT_OPCUA_STATUSCODE_BADNODEIDEXISTS);
  node.identifier.string.data = (cpkt_opcua_Byte *)"second";
  settings.historizingBackend.deleteMembers = NULL;
  for (i = 0; i < 16; ++i) {
    cpkt_opcua_StatusCode status;
    int injected;
    cpkt_types_fail_after(i);
    status = cpkt_opcua_server_register_history_backend(state.server, &node,
                                                        &settings);
    injected = cpkt_types_fail_stop();
    if (!status) {
      CHECK(!injected);
      break;
    }
    CHECK(injected && status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY &&
          state.cleared == 0);
  }
  CHECK(i < 16);
  cpkt_opcua_DataValue_init(&value);
  number.high32 = 0x12345678U;
  number.low32 = 0xfedcba98U;
  value.hasValue = 1;
  value.value.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
  value.value.data = &number;
  value.value.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
  for (i = 0; i < 16; ++i) {
    cpkt_opcua_StatusCode status;
    int injected;
    cpkt_types_fail_after(i);
    status = cpkt_opcua_history_value_new(&value, &stored);
    injected = cpkt_types_fail_stop();
    if (!status) {
      CHECK(!injected);
      cpkt_opcua_history_value_free(stored);
      break;
    }
    CHECK(injected && status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY &&
          !stored);
  }
  CHECK(i < 16);
  for (i = 0; i < 16; ++i) {
    cpkt_opcua_StatusCode status;
    int injected;
    cpkt_types_fail_after(i);
    status = cpkt_opcua_history_value_get(state.values[0], &copied);
    injected = cpkt_types_fail_stop();
    if (!status) {
      CHECK(!injected);
      cpkt_opcua_DataValue_clear(&copied);
      break;
    }
    CHECK(injected && status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY &&
          !copied.hasValue && !copied.value.data);
  }
  CHECK(i < 16);
  for (i = 0; i < 16; ++i) {
    cpkt_opcua_StatusCode status;
    int injected;
    cpkt_types_fail_after(i);
    status = cpkt_opcua_history_value_set(state.values[0], &value);
    injected = cpkt_types_fail_stop();
    CHECK(cpkt_opcua_history_value_get(state.values[0], &copied) == 0);
    if (!status) {
      CHECK(!injected &&
            ((cpkt_opcua_Int64 *)copied.value.data)->high32 == number.high32 &&
            ((cpkt_opcua_Int64 *)copied.value.data)->low32 == number.low32);
      cpkt_opcua_DataValue_clear(&copied);
      break;
    }
    CHECK(injected && status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY &&
          ((cpkt_opcua_Int64 *)copied.value.data)->high32 == 0x80000000U);
    cpkt_opcua_DataValue_clear(&copied);
  }
  CHECK(i < 16);
  cpkt_opcua_server_free(state.server);
  CHECK(state.cleared == 1);
}
#endif
void cpkt_types_test_history_backend(void) {
  history_state low, high;
  cpkt_opcua_HistorizingNodeIdSettings settings;
  cpkt_opcua_NodeId node;
  cpkt_opcua_DataValue copy_value;
  cpkt_opcua_history_value *bad = (cpkt_opcua_history_value *)1;
  int mode = 0;
  size_t i;
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  allocation_failures();
#endif
  polling();
  memset(&low, 0, sizeof(low));
  memset(&high, 0, sizeof(high));
  CHECK(cpkt_opcua_history_value_new(NULL, &bad) ==
            CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT &&
        !bad);
  CHECK(cpkt_opcua_history_value_new(NULL, NULL) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_history_value_set(NULL, NULL) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_history_value_get(NULL, &copy_value) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  cpkt_opcua_history_value_free(NULL);
  CHECK(cpkt_opcua_server_new(&low.server, 0) == CPKT_OPCUA_OK);
  high.server = low.server;
  CHECK(cpkt_opcua_server_set_default_history_database(low.server, 0) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_server_set_default_history_database(
            low.server, (size_t)-1) == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY);
  CHECK(cpkt_opcua_server_set_default_history_database(low.server, 1) == 0);
  initialize(&low, &settings);
  cpkt_opcua_NodeId_init(&node);
  node.namespaceIndex = 1;
  node.identifier.numeric = 6100;
  CHECK(cpkt_opcua_server_register_history_backend(low.server, &node,
                                                   &settings) == 0);
  CHECK(cpkt_opcua_server_register_history_backend(low.server, &node,
                                                   &settings) ==
        CPKT_OPCUA_STATUSCODE_BADNODEIDEXISTS);
  CHECK(low.cleared == 0);
  CHECK(cpkt_opcua_history_value_get(low.values[0], &copy_value) == 0);
  CHECK(((cpkt_opcua_Int64 *)copy_value.value.data)->high32 == 0x80000000U);
  ((cpkt_opcua_Int64 *)copy_value.value.data)->high32 = 0;
  cpkt_opcua_DataValue_clear(&copy_value);
  CHECK(cpkt_opcua_history_value_get(low.values[0], &copy_value) == 0);
  CHECK(((cpkt_opcua_Int64 *)copy_value.value.data)->high32 == 0x80000000U);
  cpkt_opcua_DataValue_clear(&copy_value);
  initialize(&high, &settings);
  node.identifier.numeric = 6101;
  settings.historizingBackend.getHistoryData = high_level;
  settings.historizingBackend.getDateTimeMatch = NULL;
  settings.historizingBackend.getEnd = NULL;
  settings.historizingBackend.firstIndex = NULL;
  settings.historizingBackend.lastIndex = NULL;
  settings.historizingBackend.resultSize = NULL;
  settings.historizingBackend.getDataValue = NULL;
  settings.historizingBackend.copyDataValues = NULL;
  CHECK(cpkt_opcua_server_register_history_backend(low.server, &node,
                                                   &settings) == 0);
  CHECK(cpkt_opcua_server_native(low.server, native_engine, &mode) ==
        CPKT_OPCUA_OK);
  for (i = 0; i < 14; ++i)
    CHECK(low.calls[i] > 0);
  CHECK(high.calls[14] == 1);
  low.invalid_count = 1;
  mode = 1;
  CHECK(cpkt_opcua_server_native(low.server, native_engine, &mode) ==
        CPKT_OPCUA_OK);
  low.invalid_count = 0;
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  mode = 2;
  for (i = 0; i < 128; ++i) {
    cpkt_opcua_result result;
    cpkt_opcua_status status;
    int injected;
    cpkt_types_fail_after(i);
    result =
        cpkt_opcua_server_native(low.server, native_allocation_engine, &status);
    injected = cpkt_types_fail_stop();
    if (!injected) {
      CHECK(result == CPKT_OPCUA_OK && !status);
      break;
    }
    /* Native attribute reads fail closed if their allocation fails. */
    CHECK(result == CPKT_OPCUA_ERR_UPSTREAM &&
          (status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY ||
           status == CPKT_OPCUA_STATUSCODE_BADUSERACCESSDENIED ||
           status == CPKT_OPCUA_STATUSCODE_BADHISTORYOPERATIONINVALID));
  }
  CHECK(i < 128);
  for (i = 0; i < 8; ++i) {
    cpkt_opcua_StatusCode status;
    int injected;
    cpkt_types_fail_after(i);
    status = cpkt_opcua_server_set_default_history_database(low.server, 1);
    injected = cpkt_types_fail_stop();
    if (!status) {
      CHECK(!injected && low.cleared == 1 && high.cleared == 1);
      break;
    }
    CHECK(injected && status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY);
    CHECK(low.cleared == 0 && high.cleared == 0);
  }
  CHECK(i < 8);
#else
  CHECK(cpkt_opcua_server_set_default_history_database(low.server, 1) == 0);
#endif
  CHECK(low.cleared == 1 && high.cleared == 1);
  cpkt_opcua_server_free(low.server);
  CHECK(low.cleared == 1 && high.cleared == 1);
}
