/* Native producer output identities and C89 representations. This list owns
 * conversion storage only; open62541 owns scheduling, timeouts and queues. */
typedef struct cpkt_producer cpkt_producer;
struct cpkt_producer {
  cpkt_producer *next;
  cpkt_nodes_owner *owner;
  const void *native;
  int kind, active, metadata_synced;
  cpkt_opcua_DataValue value;
  cpkt_opcua_Variant *outputs;
  size_t output_count;
  const cpkt_opcua_history_value **borrowed_outputs;
  const cpkt_opcua_history_value *borrowed;
};
/* Native read processing adds/filters timestamps after the producer returns.
 * Synchronize once at the outer C89 submission/iteration boundary, preserving
 * subsequent application edits to the pending result. No allocation or queue.
 */
static void cpkt_producer_refresh(cpkt_opcua_server *server) {
  cpkt_producer *op;
  if (!server->typed_nodes)
    return;
  for (op = server->typed_nodes->producers; op; op = op->next) {
    const UA_DataValue *native;
    if (op->kind || op->active || op->metadata_synced)
      continue;
    native = (const UA_DataValue *)op->native;
    (void)cpkt_convert(&native->sourceTimestamp, &op->value.sourceTimestamp,
                       &cpkt_types[CPKT_OPCUA_TYPES_DATETIME], 0, 0);
    (void)cpkt_convert(&native->serverTimestamp, &op->value.serverTimestamp,
                       &cpkt_types[CPKT_OPCUA_TYPES_DATETIME], 0, 0);
    op->value.hasSourceTimestamp = native->hasSourceTimestamp;
    op->value.hasServerTimestamp = native->hasServerTimestamp;
    op->value.hasSourcePicoseconds = native->hasSourcePicoseconds;
    op->value.hasServerPicoseconds = native->hasServerPicoseconds;
    op->value.sourcePicoseconds = native->sourcePicoseconds;
    op->value.serverPicoseconds = native->serverPicoseconds;
    op->metadata_synced = 1;
  }
}
cpkt_opcua_StatusCode
cpkt_opcua_server_refresh_async_producer_metadata(cpkt_opcua_server *server) {
  if (!server || !server->server || server->destroying)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  cpkt_producer_refresh(server);
  return 0;
}
static const void *cpkt_producer_address(const cpkt_producer *op) {
  return op->kind == 2 ? (const void *)op->outputs : (const void *)&op->value;
}
static void cpkt_producer_free(cpkt_producer *op) {
  cpkt_opcua_DataValue_clear(&op->value);
  if (op->outputs)
    cpkt_opcua_array_delete(op->outputs, op->output_count + 1,
                            &cpkt_types[CPKT_OPCUA_TYPES_VARIANT]);
  UA_free(op->borrowed_outputs);
  UA_free(op);
}
static void cpkt_producer_unlink(cpkt_producer *op) {
  cpkt_producer **slot;
  for (slot = &op->owner->producers; *slot; slot = &(*slot)->next)
    if (*slot == op) {
      *slot = op->next;
      return;
    }
}
static void cpkt_producer_cleanup(cpkt_nodes_owner *owner) {
  cpkt_producer *op;
  /* Native deletion has already issued its cancellation notifications. Do not
   * invent another queue or a second notification for unqueued native calls. */
  while ((op = owner->producers) != NULL) {
    cpkt_producer_unlink(op);
    cpkt_producer_free(op);
  }
}
static void cpkt_producer_cancel(UA_Server *server, const void *output) {
  cpkt_nodes_owner *owner = cpkt_nodes_find_owner(server);
  cpkt_producer *op;
  void (*callback)(cpkt_opcua_server *, const void *);
  void (*native_callback)(UA_Server *, const void *);
  if (!owner)
    return;
  for (op = owner->producers; op; op = op->next)
    if (op->native == output)
      break;
  callback = owner->cancel;
  native_callback = owner->native_cancel;
  if (op)
    cpkt_producer_unlink(op);
  /* Completion from cancellation is rejected: the identity is already gone.
   * Keep the C89 object alive through both hooks and reentrant new submissions.
   */
  if (callback)
    callback(owner->owner, op ? cpkt_producer_address(op) : output);
  if (native_callback)
    native_callback(server, output);
  if (op)
    cpkt_producer_free(op);
}
static void cpkt_producer_install_cancel(cpkt_nodes_owner *owner) {
  UA_ServerConfig *config = UA_Server_getConfig(owner->owner->server);
  if (config->asyncOperationCancelCallback != cpkt_producer_cancel) {
    owner->native_cancel = config->asyncOperationCancelCallback;
    config->asyncOperationCancelCallback = cpkt_producer_cancel;
  }
}
static cpkt_producer *cpkt_producer_new(cpkt_nodes_owner *owner,
                                        const void *native, int kind) {
  cpkt_producer *op = (cpkt_producer *)UA_calloc(1, sizeof(*op));
  if (!op)
    return NULL;
  op->owner = owner;
  op->native = native;
  op->kind = kind;
  op->active = 1;
  op->next = owner->producers;
  owner->producers = op;
  owner->producer_cleanup = cpkt_producer_cleanup;
  owner->owner->typed_producers_refresh = cpkt_producer_refresh;
  cpkt_producer_install_cancel(owner);
  return op;
}
static cpkt_producer *cpkt_producer_find(cpkt_opcua_server *server,
                                         const void *value, int kind,
                                         int allow_active) {
  cpkt_producer *op;
  if (!server || !server->server || server->destroying || !value ||
      !server->typed_nodes)
    return NULL;
  for (op = server->typed_nodes->producers; op; op = op->next)
    if (op->kind == kind && (allow_active || !op->active) &&
        cpkt_producer_address(op) == value)
      return op;
  return NULL;
}
static UA_StatusCode cpkt_producer_read_value(cpkt_producer *op,
                                              UA_DataValue *out) {
  if (op->borrowed) {
    *out = *cpkt_history_native_const(op->borrowed);
    out->value.storageType = UA_VARIANT_DATA_NODELETE;
    return 0;
  }
  return cpkt_convert(&op->value, out, &cpkt_types[CPKT_OPCUA_TYPES_DATAVALUE],
                      1, 0);
}
static UA_StatusCode
cpkt_producer_read(UA_Server *server, const UA_NodeId *session,
                   void *session_context, const UA_NodeId *node,
                   void *node_context, UA_Boolean timestamp,
                   const UA_NumericRange *range, UA_DataValue *value) {
  cpkt_nodes_entry *entry = cpkt_nodes_acquire(server, node);
  cpkt_producer *op;
  cpkt_opcua_CallbackValueSource source;
  cpkt_opcua_NodeId c_session, c_node;
  cpkt_opcua_NumericRange c_range;
  UA_DataValue staged;
  UA_StatusCode status;
  if (!entry || !entry->source.read) {
    if (entry)
      cpkt_nodes_release(entry);
    return UA_STATUSCODE_BADINTERNALERROR;
  }
  source = entry->source;
  op = cpkt_producer_new(entry->owner, value, 0);
  if (!op) {
    cpkt_nodes_release(entry);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  memset(&c_range, 0, sizeof(c_range));
  UA_DataValue_init(&staged);
  if (session)
    cpkt_hb_borrow_node(session, &c_session);
  if (node)
    cpkt_hb_borrow_node(node, &c_node);
  status = range ? cpkt_nodes_range(range, &c_range) : 0;
  if (!status)
    status = cpkt_convert(value, &op->value,
                          &cpkt_types[CPKT_OPCUA_TYPES_DATAVALUE], 0, 0);
  if (!status) {
    ++entry->owner->owner->typed_config_depth;
    status = source.read(entry->owner->owner, session ? &c_session : NULL,
                         session_context, node ? &c_node : NULL, node_context,
                         timestamp, range ? &c_range : NULL, &op->value);
    --entry->owner->owner->typed_config_depth;
  }
  {
    UA_StatusCode returned = status;
    if (!status || status == UA_STATUSCODE_GOODCOMPLETESASYNCHRONOUSLY) {
      status = cpkt_producer_read_value(op, &staged);
      if (!status) {
        UA_DataValue_clear(value);
        *value = staged;
        UA_DataValue_init(&staged);
        status = returned;
      } else if (returned == UA_STATUSCODE_GOODCOMPLETESASYNCHRONOUSLY) {
        cpkt_producer_cancel(server, value);
        op = NULL;
      }
    }
  }

  UA_DataValue_clear(&staged);
  UA_free(c_range.dimensions);
  cpkt_nodes_release(entry);
  if (op) {
    if (status == UA_STATUSCODE_GOODCOMPLETESASYNCHRONOUSLY)
      op->active = 0;
    else {
      cpkt_producer_unlink(op);
      cpkt_producer_free(op);
    }
  }
  return status;
}
static UA_StatusCode
cpkt_producer_write(UA_Server *server, const UA_NodeId *session,
                    void *session_context, const UA_NodeId *node,
                    void *node_context, const UA_NumericRange *range,
                    const UA_DataValue *value) {
  cpkt_nodes_entry *entry = cpkt_nodes_acquire(server, node);
  cpkt_producer *op;
  cpkt_opcua_CallbackValueSource source;
  cpkt_opcua_NodeId c_session, c_node;
  cpkt_opcua_NumericRange c_range;
  UA_StatusCode status;
  if (!entry || !entry->source.write) {
    if (entry)
      cpkt_nodes_release(entry);
    return UA_STATUSCODE_BADINTERNALERROR;
  }
  source = entry->source;
  op = cpkt_producer_new(entry->owner, value, 1);
  if (!op) {
    cpkt_nodes_release(entry);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  memset(&c_range, 0, sizeof(c_range));
  if (session)
    cpkt_hb_borrow_node(session, &c_session);
  if (node)
    cpkt_hb_borrow_node(node, &c_node);
  status = range ? cpkt_nodes_range(range, &c_range) : 0;
  if (!status)
    status = cpkt_convert(value, &op->value,
                          &cpkt_types[CPKT_OPCUA_TYPES_DATAVALUE], 0, 0);
  if (!status) {
    ++entry->owner->owner->typed_config_depth;
    status = source.write(entry->owner->owner, session ? &c_session : NULL,
                          session_context, node ? &c_node : NULL, node_context,
                          range ? &c_range : NULL, &op->value);
    --entry->owner->owner->typed_config_depth;
  }
  UA_free(c_range.dimensions);
  cpkt_nodes_release(entry);
  if (op) {
    if (status == UA_STATUSCODE_GOODCOMPLETESASYNCHRONOUSLY)
      op->active = 0;
    else {
      cpkt_producer_unlink(op);
      cpkt_producer_free(op);
    }
  }
  return status;
}
static UA_StatusCode cpkt_producer_outputs(cpkt_producer *op,
                                           UA_Variant *outputs) {
  UA_Variant *staged;
  size_t i;
  UA_StatusCode status = 0;
  staged = (UA_Variant *)UA_Array_new(
      op->output_count, cpkt_types[CPKT_OPCUA_TYPES_VARIANT].native);
  if (!staged)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  for (i = 0; !status && i < op->output_count; ++i) {
    if (op->borrowed_outputs && op->borrowed_outputs[i]) {
      staged[i] = cpkt_history_native_const(op->borrowed_outputs[i])->value;
      staged[i].storageType = UA_VARIANT_DATA_NODELETE;
    } else
      status = cpkt_convert(&op->outputs[i], &staged[i],
                            &cpkt_types[CPKT_OPCUA_TYPES_VARIANT], 1, 0);
  }
  if (!status)
    for (i = 0; i < op->output_count; ++i) {
      UA_Variant_clear(&outputs[i]);
      outputs[i] = staged[i];
      UA_Variant_init(&staged[i]);
    }
  if (staged)
    UA_Array_delete(staged, op->output_count,
                    cpkt_types[CPKT_OPCUA_TYPES_VARIANT].native);
  return status;
}

static UA_StatusCode cpkt_producer_method(
    UA_Server *server, const UA_NodeId *session, void *session_context,
    const UA_NodeId *method, void *method_context, const UA_NodeId *object,
    void *object_context, size_t input_size, const UA_Variant *inputs,
    size_t output_size, UA_Variant *outputs) {
  cpkt_nodes_entry *entry = cpkt_nodes_acquire(server, method);
  cpkt_producer *op;
  cpkt_opcua_MethodCallback callback;
  cpkt_opcua_NodeId c_session, c_method, c_object;
  void *c_inputs = NULL;
  size_t i;
  UA_StatusCode status;
  if (!entry || !entry->method) {
    if (entry)
      cpkt_nodes_release(entry);
    return UA_STATUSCODE_BADINTERNALERROR;
  }
  if (output_size == (size_t)-1) {
    cpkt_nodes_release(entry);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  callback = entry->method;
  op = cpkt_producer_new(entry->owner, outputs, 2);
  if (!op) {
    cpkt_nodes_release(entry);
    return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  op->output_count = output_size;
  op->outputs = (cpkt_opcua_Variant *)cpkt_opcua_array_new(
      output_size + 1, &cpkt_types[CPKT_OPCUA_TYPES_VARIANT]);
  if (session)
    cpkt_hb_borrow_node(session, &c_session);
  if (method)
    cpkt_hb_borrow_node(method, &c_method);
  if (object)
    cpkt_hb_borrow_node(object, &c_object);
  status = op->outputs ? 0 : UA_STATUSCODE_BADOUTOFMEMORY;
  for (i = 0; !status && i < output_size; ++i)
    status = cpkt_convert(&outputs[i], &op->outputs[i],
                          &cpkt_types[CPKT_OPCUA_TYPES_VARIANT], 0, 0);
  if (!status)
    status = cpkt_array(inputs, input_size, &c_inputs,
                        &cpkt_types[CPKT_OPCUA_TYPES_VARIANT], 0, 0);
  if (!status) {
    ++entry->owner->owner->typed_config_depth;
    status = callback(entry->owner->owner, session ? &c_session : NULL,
                      session_context, method ? &c_method : NULL,
                      method_context, object ? &c_object : NULL, object_context,
                      input_size, (const cpkt_opcua_Variant *)c_inputs,
                      output_size, op->outputs);
    --entry->owner->owner->typed_config_depth;
  }
  {
    UA_StatusCode returned = status;
    if (!status || status == UA_STATUSCODE_GOODCOMPLETESASYNCHRONOUSLY) {
      status = cpkt_producer_outputs(op, outputs);
      if (!status)
        status = returned;
      else if (returned == UA_STATUSCODE_GOODCOMPLETESASYNCHRONOUSLY) {
        cpkt_producer_cancel(server, outputs);
        op = NULL;
      }
    }
  }
  cpkt_clear_array(c_inputs, c_inputs ? input_size : 0,
                   &cpkt_types[CPKT_OPCUA_TYPES_VARIANT]);
  cpkt_nodes_release(entry);
  if (op) {
    if (status == UA_STATUSCODE_GOODCOMPLETESASYNCHRONOUSLY)
      op->active = 0;
    else {
      cpkt_producer_unlink(op);
      cpkt_producer_free(op);
    }
  }
  return status;
}
cpkt_opcua_StatusCode
cpkt_opcua_server_setAsyncReadResult_typed(cpkt_opcua_server *server,
                                           cpkt_opcua_DataValue *value) {
  cpkt_producer *op = cpkt_producer_find(server, value, 0, 0);
  UA_DataValue staged;
  UA_StatusCode status;
  if (!op)
    return UA_STATUSCODE_BADNOTFOUND;
  UA_DataValue_init(&staged);
  status = cpkt_producer_read_value(op, &staged);
  if (!status) {
    UA_DataValue *native = (UA_DataValue *)op->native;
    UA_DataValue previous = *native;
    *native = staged;
    UA_DataValue_init(&staged);
    status = UA_Server_setAsyncReadResult(server->server, native);
    if (status) {
      staged = *native;
      *native = previous;
    } else
      UA_DataValue_clear(&previous);
  }
  UA_DataValue_clear(&staged);
  if (!status) {
    cpkt_producer_unlink(op);
    cpkt_producer_free(op);
  }
  return status;
}
cpkt_opcua_StatusCode
cpkt_opcua_server_setAsyncWriteResult_typed(cpkt_opcua_server *server,
                                            const cpkt_opcua_DataValue *value,
                                            cpkt_opcua_StatusCode result) {
  cpkt_producer *op = cpkt_producer_find(server, value, 1, 0);
  UA_StatusCode status;
  if (!op)
    return UA_STATUSCODE_BADNOTFOUND;
  status = UA_Server_setAsyncWriteResult(
      server->server, (const UA_DataValue *)op->native, result);
  if (!status) {
    cpkt_producer_unlink(op);
    cpkt_producer_free(op);
  }
  return status;
}
cpkt_opcua_StatusCode
cpkt_opcua_server_setAsyncCallMethodResult_typed(cpkt_opcua_server *server,
                                                 cpkt_opcua_Variant *output,
                                                 cpkt_opcua_StatusCode result) {
  cpkt_producer *op = cpkt_producer_find(server, output, 2, 0);
  UA_StatusCode status;
  if (!op)
    return UA_STATUSCODE_BADNOTFOUND;
  status = cpkt_producer_outputs(op, (UA_Variant *)op->native);
  if (!status)
    status = UA_Server_setAsyncCallMethodResult(
        server->server, (UA_Variant *)op->native, result);
  if (!status) {
    cpkt_producer_unlink(op);
    cpkt_producer_free(op);
  }
  return status;
}
cpkt_opcua_StatusCode cpkt_opcua_server_valueSourceBorrow_typed(
    cpkt_opcua_server *server, cpkt_opcua_DataValue *value,
    const cpkt_opcua_history_value *stored) {
  cpkt_producer *op = cpkt_producer_find(server, value, 0, 1);
  if (!op)
    return UA_STATUSCODE_BADNOTFOUND;
  op->borrowed = stored;
  return 0;
}
cpkt_opcua_StatusCode
cpkt_opcua_server_set_async_operation_cancel_callback_typed(
    cpkt_opcua_server *server,
    void (*callback)(cpkt_opcua_server *, const void *)) {
  cpkt_nodes_owner *owner;
  if (!server || !server->server || server->destroying || server->started)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  owner = cpkt_nodes_owner_new(server);
  if (!owner)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  owner->cancel = callback;
  cpkt_producer_install_cancel(owner);
  return 0;
}

cpkt_opcua_StatusCode cpkt_opcua_server_methodResultBorrow_typed(
    cpkt_opcua_server *server, cpkt_opcua_Variant *outputs, size_t index,
    const cpkt_opcua_history_value *stored) {
  cpkt_producer *op = cpkt_producer_find(server, outputs, 2, 1);
  if (!op)
    return UA_STATUSCODE_BADNOTFOUND;
  if (index >= op->output_count)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!stored && !op->borrowed_outputs)
    return 0;
  if (!op->borrowed_outputs) {
    op->borrowed_outputs = (const cpkt_opcua_history_value **)UA_calloc(
        op->output_count, sizeof(*op->borrowed_outputs));
    if (!op->borrowed_outputs)
      return UA_STATUSCODE_BADOUTOFMEMORY;
  }
  op->borrowed_outputs[index] = stored;
  return 0;
}
