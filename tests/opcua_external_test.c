#include "opcua_callbacks_test.h"
#include "opcua_types_peer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void check(int ok, const char *expression, int line) {
  if (!ok) {
    fprintf(stderr, "external value line %d: %s\n", line, expression);
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
  cpkt_opcua_external_value *holder;
  cpkt_opcua_history_value *alternate;
  int reads, writes, switch_on_read, disable;
};
static cpkt_opcua_NodeId node(unsigned short ns, unsigned int id) {
  cpkt_opcua_NodeId result;
  cpkt_opcua_NodeId_init(&result);
  result.namespaceIndex = ns;
  result.identifier.numeric = id;
  return result;
}
static void notify(cpkt_opcua_server *server, const cpkt_opcua_NodeId *session,
                   void *session_context, const cpkt_opcua_NodeId *id,
                   void *context, const cpkt_opcua_NumericRange *range,
                   const cpkt_opcua_DataValue *value, int write) {
  struct state *s = context;
  cpkt_opcua_Int64 first;
  (void)session_context;
  CHECK(s && s->server == server && session && id && id->namespaceIndex == 1 &&
        value && value->hasValue);
  CHECK(value->value.type == cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64));
  CHECK(value->value.data);
  first = *(cpkt_opcua_Int64 *)value->value.data;
  if (range)
    CHECK(range->dimensionsSize == 1 && range->dimensions[0].min == 1 &&
          range->dimensions[0].max == 1);
  if (write)
    ++s->writes;
  else
    ++s->reads;
  if (!write && s->switch_on_read) {
    s->switch_on_read = 0;
    CHECK(!cpkt_opcua_external_value_set(s->holder, s->alternate));
  }
  if (s->disable) {
    s->disable = 0;
    CHECK(!cpkt_opcua_server_setVariableNode_externalValueSource_typed(
        server, *id, s->holder, NULL));
  }
  /* Replacing registration/selection cannot invalidate the in-flight copy. */
  CHECK(((cpkt_opcua_Int64 *)value->value.data)->high32 == first.high32 &&
        ((cpkt_opcua_Int64 *)value->value.data)->low32 == first.low32);
}
static void on_read(cpkt_opcua_server *server, const cpkt_opcua_NodeId *session,
                    void *session_context, const cpkt_opcua_NodeId *id,
                    void *context, const cpkt_opcua_NumericRange *range,
                    const cpkt_opcua_DataValue *value) {
  notify(server, session, session_context, id, context, range, value, 0);
}
static void on_write(cpkt_opcua_server *server,
                     const cpkt_opcua_NodeId *session, void *session_context,
                     const cpkt_opcua_NodeId *id, void *context,
                     const cpkt_opcua_NumericRange *range,
                     const cpkt_opcua_DataValue *value) {
  notify(server, session, session_context, id, context, range, value, 1);
}
struct peer_args {
  unsigned int id;
  struct cpkt_external_observed *out;
};
static cpkt_opcua_status peer(void *native, void *context) {
  struct peer_args *args = context;
  return cpkt_types_peer_external(native, args->id, args->out);
}
static void observe(struct state *s, struct cpkt_external_observed *out) {
  struct peer_args args;
  args.id = 9000;
  args.out = out;
  CHECK(!cpkt_opcua_server_native(s->server, peer, &args));
}
static void read_value(struct state *s, const char *range, unsigned int high,
                       unsigned int low) {
  cpkt_opcua_ReadValueId request;
  cpkt_opcua_DataValue result;
  cpkt_opcua_Int64 *integer;
  cpkt_opcua_ReadValueId_init(&request);
  request.nodeId = node(1, 9000);
  request.attributeId = 13; /* Native Value attribute ID. */
  if (range) {
    request.indexRange.data = (cpkt_opcua_Byte *)range;
    request.indexRange.length = strlen(range);
  }
  CHECK(!cpkt_opcua_server_read_typed(
      s->server, &request, cpkt_opcua_TIMESTAMPSTORETURN_NEITHER, &result));
  if (!result.hasValue || (result.hasStatus && result.status))
    fprintf(stderr, "external read %d/write %d range %s status %08lx\n",
            s->reads, s->writes, range ? range : "none",
            (unsigned long)result.status);
  CHECK(result.hasValue && (!result.hasStatus || !result.status));
  CHECK(result.value.type == cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64) &&
        result.value.arrayLength == (range ? 1 : 2));
  integer = result.value.data;
  CHECK(integer->high32 == high && integer->low32 == low);
  cpkt_opcua_DataValue_clear(&result);
}
void cpkt_types_test_external(void) {
  struct state s[2];
  cpkt_opcua_ValueSourceNotifications notifications;
  cpkt_opcua_VariableAttributes attr;
  cpkt_opcua_QualifiedName name;
  cpkt_opcua_DataValue value, copy;
  cpkt_opcua_history_value *stored[2];
  cpkt_opcua_external_value *failed;
  cpkt_opcua_Int64 integers[2];
  cpkt_opcua_UInt32 dimension = 2;
  struct cpkt_external_observed before, after, other;
  cpkt_opcua_StatusCode status;
  size_t i;
  int count;
  memset(s, 0, sizeof(s));
  cpkt_opcua_DataValue_init(&value);
  integers[0].high32 = 0x80000000U;
  integers[0].low32 = 0;
  integers[1].high32 = 0x7fffffffU;
  integers[1].low32 = 0xffffffffU;
  value.hasValue = 1;
  value.value.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
  value.value.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
  value.value.data = integers;
  value.value.arrayLength = 2;
  CHECK(!cpkt_opcua_history_value_new(&value, &stored[0]));
  integers[0].low32 = 1;
  integers[1].low32 = 0xfffffffeU;
  CHECK(!cpkt_opcua_history_value_new(&value, &stored[1]));
  CHECK(!cpkt_opcua_external_value_new(stored[0], &s[0].holder));
  s[1].holder = s[0].holder;
  CHECK(cpkt_opcua_external_value_get(s[0].holder) == stored[0]);
  CHECK(!cpkt_opcua_external_value_get(NULL));
  failed = s[0].holder;
  CHECK(cpkt_opcua_external_value_new(NULL, &failed) ==
            CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT &&
        !failed);
  CHECK(cpkt_opcua_external_value_new(stored[0], NULL) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_external_value_set(s[0].holder, NULL) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_external_value_set(NULL, stored[0]) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  notifications.onRead = on_read;
  notifications.onWrite = on_write;
  for (i = 0; i < 2; ++i) {
    CHECK(!cpkt_opcua_server_new(&s[i].server, 0));
    CHECK(!cpkt_opcua_VariableAttributes_default(&attr));
    attr.dataType = node(0, CPKT_OPCUA_NS0ID_INT64);
    attr.valueRank = 1;
    attr.arrayDimensionsSize = 1;
    attr.arrayDimensions = &dimension;
    attr.value = value.value;
    attr.accessLevel = 3; /* Native READ | WRITE mask. */
    cpkt_opcua_QualifiedName_init(&name);
    name.namespaceIndex = 1;
    name.name.data = (cpkt_opcua_Byte *)"external";
    name.name.length = 8;
    CHECK(!cpkt_opcua_server_addVariableNode_typed(
        s[i].server, node(1, 9000), node(0, CPKT_OPCUA_NS0ID_OBJECTSFOLDER),
        node(0, CPKT_OPCUA_NS0ID_ORGANIZES), name,
        node(0, CPKT_OPCUA_NS0ID_BASEDATAVARIABLETYPE), attr, &s[i], NULL));
    /* Only these fields borrow stack memory; default-owned strings still clear.
     */
    memset(&attr.value, 0, sizeof(attr.value));
    attr.arrayDimensions = NULL;
    attr.arrayDimensionsSize = 0;
    cpkt_opcua_VariableAttributes_clear(&attr);
    CHECK(!cpkt_opcua_server_setVariableNode_externalValueSource_typed(
        s[i].server, node(1, 9000), s[i].holder, &notifications));
    read_value(&s[i], NULL, 0x80000000U, 0);
    CHECK(s[i].reads == 1);
  }
  observe(&s[0], &before);
  observe(&s[1], &other);
  CHECK(before.slot == other.slot && before.storage == other.storage &&
        before.data == other.data);
  /* Native reloads a selection changed inside onRead, across both servers. */
  s[0].alternate = stored[1];
  s[0].switch_on_read = 1;
  read_value(&s[0], NULL, 0x80000000U, 1);
  observe(&s[0], &after);
  CHECK(before.slot == after.slot && before.storage != after.storage &&
        before.data != after.data);
  read_value(&s[1], "1", 0x7fffffffU, 0xfffffffeU);
  CHECK(!cpkt_opcua_external_value_set(s[0].holder, stored[0]));
  observe(&s[0], &after);
  CHECK(before.slot == after.slot && before.storage == after.storage &&
        before.data == after.data);
  /* Native writes update the selected persistent object, not a bridge cache. */
  CHECK(!cpkt_opcua_server_writeValue_typed(s[0].server, node(1, 9000),
                                            value.value));
  CHECK(s[0].writes == 1);
  CHECK(!cpkt_opcua_history_value_get(stored[0], &copy));
  CHECK(((cpkt_opcua_Int64 *)copy.value.data)[0].low32 == 1 &&
        ((cpkt_opcua_Int64 *)copy.value.data)[1].low32 == 0xfffffffeU);
  cpkt_opcua_DataValue_clear(&copy);
  read_value(&s[1], NULL, 0x80000000U, 1);
  observe(&s[0], &after);
  CHECK(before.storage == after.storage);
  {
    cpkt_opcua_WriteValue request;
    cpkt_opcua_Int64 replacement;
    cpkt_opcua_WriteValue_init(&request);
    request.nodeId = node(1, 9000);
    request.attributeId = 13;
    request.indexRange.data = (cpkt_opcua_Byte *)"1";
    request.indexRange.length = 1;
    replacement.high32 = 0xffffffffU;
    replacement.low32 = 0xffffffffU;
    request.value.hasValue = 1;
    request.value.value.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_INT64);
    request.value.value.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
    request.value.value.arrayLength = 1;
    request.value.value.data = &replacement;
    CHECK(!cpkt_opcua_server_write_typed(s[0].server, &request));
    CHECK(s[0].writes == 2);
    read_value(&s[1], "1", 0xffffffffU, 0xffffffffU);
    CHECK(!cpkt_opcua_history_value_get(stored[0], &copy));
    CHECK(((cpkt_opcua_Int64 *)copy.value.data)[0].low32 == 1 &&
          ((cpkt_opcua_Int64 *)copy.value.data)[1].high32 == 0xffffffffU &&
          ((cpkt_opcua_Int64 *)copy.value.data)[1].low32 == 0xffffffffU);
    cpkt_opcua_DataValue_clear(&copy);
  }
  /* Mutating quiescent persistent storage also stays visible through the slot.
   * The storage address is stable even though replacing its payload allocates.
   */
  CHECK(!cpkt_opcua_history_value_set(stored[0], &value));
  observe(&s[0], &after);
  CHECK(before.slot == after.slot && before.storage == after.storage);
  read_value(&s[1], "1", 0x7fffffffU, 0xfffffffeU);
  CHECK(cpkt_opcua_server_setVariableNode_externalValueSource_typed(
            s[0].server, node(1, 9999), s[0].holder, NULL) ==
        CPKT_OPCUA_STATUSCODE_BADNODEIDUNKNOWN);
  CHECK(cpkt_opcua_server_setVariableNode_externalValueSource_typed(
            s[0].server, node(0, CPKT_OPCUA_NS0ID_OBJECTSFOLDER), s[0].holder,
            NULL) == CPKT_OPCUA_STATUSCODE_BADNODECLASSINVALID);
  CHECK(cpkt_opcua_server_setVariableNode_externalValueSource_typed(
            s[0].server, node(1, 9000), NULL, NULL) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  cpkt_types_fail_after(0);
  status = cpkt_opcua_external_value_new(stored[0], &failed);
  CHECK(cpkt_types_fail_stop());
  CHECK(status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY && !failed);
  cpkt_types_fail_after(0);
  CHECK(!cpkt_opcua_external_value_set(s[0].holder, stored[0]));
  CHECK(!cpkt_types_fail_stop());
  for (i = 0; i < 32; ++i) {
    cpkt_types_fail_after(i);
    status = cpkt_opcua_server_setVariableNode_externalValueSource_typed(
        s[0].server, node(1, 9000), s[0].holder, &notifications);
    count = cpkt_types_fail_stop();
    if (!count) {
      CHECK(!status);
      break;
    }
    CHECK(status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY);
    count = s[0].reads;
    read_value(&s[0], NULL, 0x80000000U, 1);
    CHECK(s[0].reads == count + 1);
    observe(&s[0], &after);
    CHECK(before.slot == after.slot && before.storage == after.storage);
  }
  CHECK(i > 0 && i < 32);
#else
  (void)status;
#endif
  /* Replace notification registration during its callback. */
  s[0].disable = 1;
  read_value(&s[0], NULL, 0x80000000U, 1);
  count = s[0].reads;
  read_value(&s[0], NULL, 0x80000000U, 1);
  CHECK(s[0].reads == count);
  /* Detach first server, destroy second. Holder does not own selected storage.
   */
  CHECK(!cpkt_opcua_server_setVariableNode_internalValueSource_typed(
      s[0].server, node(1, 9000), &value, NULL));
  cpkt_opcua_server_free(s[1].server);
  cpkt_opcua_external_value_free(s[0].holder);
  cpkt_opcua_external_value_free(NULL);
  CHECK(!cpkt_opcua_history_value_get(stored[0], &copy));
  cpkt_opcua_DataValue_clear(&copy);
  cpkt_opcua_history_value_free(stored[0]);
  cpkt_opcua_history_value_free(stored[1]);
  read_value(&s[0], NULL, 0x80000000U, 1);
  cpkt_opcua_server_free(s[0].server);
}
