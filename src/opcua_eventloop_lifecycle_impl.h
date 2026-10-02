/* Native method hooks retain facade metadata through native-origin lifecycle
 * calls, including configuration destruction. They delegate to the original
 * architecture methods and never invoke callbacks under the identity mutex. */
#ifndef CPKT_OPCUA_EVENTLOOP_LIFECYCLE_IMPL_H
#define CPKT_OPCUA_EVENTLOOP_LIFECYCLE_IMPL_H
static pthread_mutex_t cpkt_loop_mutex = PTHREAD_MUTEX_INITIALIZER;
static cpkt_opcua_EventLoop *cpkt_loop_registry;
static cpkt_opcua_EventSource *cpkt_loop_source_registry;

static cpkt_opcua_EventLoop *cpkt_loop_native_find(UA_EventLoop *native) {
  cpkt_opcua_EventLoop *loop;
  (void)pthread_mutex_lock(&cpkt_loop_mutex);
  for (loop = cpkt_loop_registry; loop; loop = loop->registry_next)
    if (loop->native == native)
      break;
  (void)pthread_mutex_unlock(&cpkt_loop_mutex);
  return loop;
}
static cpkt_opcua_EventSource *
cpkt_loop_source_native_find(UA_EventSource *native) {
  cpkt_opcua_EventSource *source;
  (void)pthread_mutex_lock(&cpkt_loop_mutex);
  for (source = cpkt_loop_source_registry; source;
       source = source->registry_next)
    if (source->native == native)
      break;
  (void)pthread_mutex_unlock(&cpkt_loop_mutex);
  return source;
}
static void cpkt_loop_source_unpublish(cpkt_opcua_EventSource *source) {
  cpkt_opcua_EventSource **slot;
  (void)pthread_mutex_lock(&cpkt_loop_mutex);
  for (slot = &cpkt_loop_source_registry; *slot; slot = &(*slot)->registry_next)
    if (*slot == source) {
      *slot = source->registry_next;
      break;
    }
  (void)pthread_mutex_unlock(&cpkt_loop_mutex);
}
static void cpkt_loop_unpublish(cpkt_opcua_EventLoop *loop) {
  cpkt_opcua_EventLoop **slot;
  (void)pthread_mutex_lock(&cpkt_loop_mutex);
  for (slot = &cpkt_loop_registry; *slot; slot = &(*slot)->registry_next)
    if (*slot == loop) {
      *slot = loop->registry_next;
      break;
    }
  (void)pthread_mutex_unlock(&cpkt_loop_mutex);
}

typedef struct {
  cpkt_opcua_server *servers;
} cpkt_loop_frame;
static void cpkt_loop_frame_end(cpkt_loop_frame *frame) {
  cpkt_opcua_server *server;
  while (frame->servers) {
    server = frame->servers;
    frame->servers = server->typed_loop_frame_next;
    server->typed_loop_frame_next = NULL;
    if (server->typed_loop_pubsub_held && server->typed_pubsub_finish)
      server->typed_pubsub_finish(server);
    server->typed_loop_pubsub_held = 0;
    --server->typed_loop_depth;
  }
}
static UA_StatusCode cpkt_loop_frame_begin(UA_EventLoop *native,
                                           cpkt_loop_frame *frame,
                                           int synchronize) {
  cpkt_nodes_owner *owner;
  cpkt_opcua_server *server;
  UA_StatusCode status = 0;
  frame->servers = NULL;
  /* Pin all existing owners before callbacks. Their opaque handles supply an
   * intrusive snapshot without allocating during void stop or teardown. A
   * nested frame borrows the enclosing pins and policy leases. Per-loop/server
   * operations are serialized; registry insertion and identity lookup alone
   * are protected by the mutex. Never invoke native/user callbacks under it. */
  (void)pthread_mutex_lock(&cpkt_nodes_mutex);
  for (owner = cpkt_nodes_owners; owner; owner = owner->next) {
    server = owner->owner;
    if (owner->event_loop == native && !owner->deleting &&
        !server->typed_loop_depth) {
      ++server->typed_loop_depth;
      server->typed_loop_frame_next = frame->servers;
      frame->servers = server;
    }
  }
  (void)pthread_mutex_unlock(&cpkt_nodes_mutex);
  for (server = frame->servers; server;
       server = server->typed_loop_frame_next) {
    if (synchronize) {
      status = server->typed_pubsub_prepare
                   ? server->typed_pubsub_prepare(server)
                   : 0;
      if (status) {
        cpkt_loop_frame_end(frame);
        return status;
      }
      server->typed_loop_pubsub_held = server->typed_pubsub_prepare != NULL;
    } else if (server->typed_pubsub_retain) {
      server->typed_pubsub_retain(server);
      server->typed_loop_pubsub_held = 1;
    }
  }
  return 0;
}
static UA_StatusCode cpkt_loop_hook_start(UA_EventLoop *native) {
  cpkt_opcua_EventLoop *loop = cpkt_loop_native_find(native);
  cpkt_loop_frame frame;
  UA_StatusCode status;
  if (!loop || loop->freeing)
    return UA_STATUSCODE_BADINVALIDSTATE;
  status = cpkt_loop_frame_begin(native, &frame, 1);
  if (status)
    return status;
  ++loop->depth;
  status = loop->native_start(native);
  --loop->depth;
  cpkt_loop_frame_end(&frame);
  return status;
}
static UA_StatusCode cpkt_loop_hook_run(UA_EventLoop *native,
                                        UA_UInt32 timeout) {
  cpkt_opcua_EventLoop *loop = cpkt_loop_native_find(native);
  cpkt_loop_frame frame;
  UA_StatusCode status;
  if (!loop || loop->depth || loop->freeing)
    return UA_STATUSCODE_BADINVALIDSTATE;
  status = cpkt_loop_frame_begin(native, &frame, 1);
  if (status)
    return status;
  ++loop->depth;
  status = loop->native_run(native, timeout);
  --loop->depth;
  cpkt_loop_frame_end(&frame);
  return status;
}
static void cpkt_loop_hook_stop(UA_EventLoop *native) {
  cpkt_opcua_EventLoop *loop = cpkt_loop_native_find(native);
  cpkt_loop_frame frame;
  if (!loop || loop->freeing)
    return;
  (void)cpkt_loop_frame_begin(native, &frame, 0);
  ++loop->depth;
  loop->native_stop(native);
  --loop->depth;
  cpkt_loop_frame_end(&frame);
}
static UA_StatusCode cpkt_loop_hook_source_free(UA_EventSource *native) {
  cpkt_opcua_EventSource *source = cpkt_loop_source_native_find(native);
  cpkt_opcua_EventLoop *loop;
  UA_StatusCode status;
  if (!source || source->depth ||
      (source->loop && native->state != UA_EVENTSOURCESTATE_FRESH))
    return UA_STATUSCODE_BADINVALIDSTATE;
  loop = source->loop;
  ++source->depth;
  if (loop)
    ++loop->depth;
  status = source->native_free(native);
  if (loop)
    --loop->depth;
  --source->depth;
  if (!status) {
    cpkt_loop_source_detach(source);
    cpkt_loop_source_unpublish(source);
    cpkt_loop_source_metadata_clear(source);
  }
  return status;
}
static UA_StatusCode cpkt_loop_hook_free(UA_EventLoop *native) {
  cpkt_opcua_EventLoop *loop = cpkt_loop_native_find(native);
  UA_StatusCode status;
  cpkt_loop_frame frame;
  cpkt_nodes_owner *owner;
  if (!loop || loop->depth || loop->freeing)
    return UA_STATUSCODE_BADINVALIDSTATE;
  (void)cpkt_loop_frame_begin(native, &frame, 0);
  loop->freeing = 1;
  ++loop->depth;
  status = loop->native_free(native);
  --loop->depth;
  if (!status && loop->configuration_slot &&
      *loop->configuration_slot == native)
    *loop->configuration_slot = NULL;
  if (!status) {
    (void)pthread_mutex_lock(&cpkt_nodes_mutex);
    for (owner = cpkt_nodes_owners; owner; owner = owner->next)
      if (owner->event_loop == native)
        owner->event_loop = NULL;
    (void)pthread_mutex_unlock(&cpkt_nodes_mutex);
  }
  cpkt_loop_frame_end(&frame);
  if (status) {
    loop->freeing = 0;
    return status;
  }
  /* Valid native backends released their owned sources via the source hooks.
   * Never dereference native roots after native free. */
  while (loop->sources) {
    cpkt_opcua_EventSource *source = loop->sources;
    cpkt_loop_source_detach(source);
  }
  cpkt_loop_unpublish(loop);
  UA_free(loop);
  return 0;
}
static UA_StatusCode cpkt_loop_hook_register(UA_EventLoop *native,
                                             UA_EventSource *es) {
  cpkt_opcua_EventLoop *loop = cpkt_loop_native_find(native);
  cpkt_opcua_EventSource *source;
  UA_StatusCode status;
  if (!loop || loop->freeing)
    return UA_STATUSCODE_BADINVALIDSTATE;
  source = cpkt_loop_source_native_find(es);
  if (source && source->loop && source->loop != loop)
    return UA_STATUSCODE_BADINVALIDSTATE;
  source = cpkt_loop_source_adopt(loop, es);
  if (!source)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  ++loop->depth;
  ++source->depth;
  status = loop->native_register(native, es);
  --source->depth;
  --loop->depth;
  if (es->eventLoop != native || es->state == UA_EVENTSOURCESTATE_FRESH)
    cpkt_loop_source_detach(source);
  return status;
}
static UA_StatusCode cpkt_loop_hook_deregister(UA_EventLoop *native,
                                               UA_EventSource *es) {
  cpkt_opcua_EventLoop *loop = cpkt_loop_native_find(native);
  cpkt_opcua_EventSource *source = cpkt_loop_source_native_find(es);
  UA_StatusCode status;
  if (!loop || !source || source->loop != loop || source->depth)
    return UA_STATUSCODE_BADINVALIDSTATE;
  ++loop->depth;
  ++source->depth;
  status = loop->native_deregister(native, es);
  --source->depth;
  --loop->depth;
  if (!status)
    cpkt_loop_source_detach(source);
  return status;
}
static void cpkt_loop_source_publish(cpkt_opcua_EventSource *source) {
  source->native_free = source->native->free;
  source->native->free = cpkt_loop_hook_source_free;
  (void)pthread_mutex_lock(&cpkt_loop_mutex);
  source->registry_next = cpkt_loop_source_registry;
  cpkt_loop_source_registry = source;
  (void)pthread_mutex_unlock(&cpkt_loop_mutex);
}
static void cpkt_loop_publish(cpkt_opcua_EventLoop *loop) {
  loop->native_start = loop->native->start;
  loop->native_stop = loop->native->stop;
  loop->native_free = loop->native->free;
  loop->native_run = loop->native->run;
  loop->native_register = loop->native->registerEventSource;
  loop->native_deregister = loop->native->deregisterEventSource;
  loop->native->start = cpkt_loop_hook_start;
  loop->native->stop = cpkt_loop_hook_stop;
  loop->native->free = cpkt_loop_hook_free;
  loop->native->run = cpkt_loop_hook_run;
  loop->native->registerEventSource = cpkt_loop_hook_register;
  loop->native->deregisterEventSource = cpkt_loop_hook_deregister;
  (void)pthread_mutex_lock(&cpkt_loop_mutex);
  loop->registry_next = cpkt_loop_registry;
  cpkt_loop_registry = loop;
  (void)pthread_mutex_unlock(&cpkt_loop_mutex);
}
static cpkt_opcua_EventLoop *cpkt_loop_native_adopt(UA_EventLoop *native) {
  cpkt_opcua_EventLoop *loop;
  UA_EventSource *es;
  if (!native)
    return NULL;
  loop = cpkt_loop_native_find(native);
  if (loop)
    return loop;
  loop = (cpkt_opcua_EventLoop *)UA_calloc(1, sizeof(*loop));
  if (!loop)
    return NULL;
  loop->native = native;
  for (es = native->eventSources; es; es = es->next)
    if (!cpkt_loop_source_adopt(loop, es)) {
      while (loop->sources)
        cpkt_loop_source_detach(loop->sources);
      UA_free(loop);
      return NULL;
    }
  cpkt_loop_publish(loop);
  return loop;
}
cpkt_opcua_StatusCode
cpkt_opcua_client_get_event_loop_typed(cpkt_opcua_client *client,
                                       cpkt_opcua_EventLoop **loop) {
  UA_EventLoop *native;
  if (!loop)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  *loop = NULL;
  if (!client || !client->client)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  client->typed_eventloop_forget_config = cpkt_loop_forget_config;
  native = UA_Client_getConfig(client->client)->eventLoop;
  if (!native)
    return 0;
  *loop = cpkt_loop_native_adopt(native);
  if (*loop && !UA_Client_getConfig(client->client)->externalEventLoop)
    (*loop)->configuration_slot =
        &UA_Client_getConfig(client->client)->eventLoop;
  return *loop ? 0 : UA_STATUSCODE_BADOUTOFMEMORY;
}
cpkt_opcua_StatusCode
cpkt_opcua_server_get_event_loop_typed(cpkt_opcua_server *server,
                                       cpkt_opcua_EventLoop **loop) {
  UA_EventLoop *native;
  if (!loop)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  *loop = NULL;
  if (!server || !server->server || server->destroying)
    return UA_STATUSCODE_BADINVALIDARGUMENT;
  if (!cpkt_nodes_owner_new(server))
    return UA_STATUSCODE_BADOUTOFMEMORY;
  cpkt_nodes_refresh_eventloop(server);
  server->typed_eventloop_forget_config = cpkt_loop_forget_config;
  native = UA_Server_getConfig(server->server)->eventLoop;
  if (!native)
    return 0;
  *loop = cpkt_loop_native_adopt(native);
  if (*loop && !UA_Server_getConfig(server->server)->externalEventLoop)
    (*loop)->configuration_slot =
        &UA_Server_getConfig(server->server)->eventLoop;
  return *loop ? 0 : UA_STATUSCODE_BADOUTOFMEMORY;
}
#endif
