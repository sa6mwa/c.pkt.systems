/* Dispatch ownership during native node creation. No creation algorithm or
 * callback context is replaced: the native function still owns all node work.
 */
static void cpkt_nodes_commit(cpkt_nodes_entry *entry);
static UA_StatusCode cpkt_creation_begin(cpkt_opcua_server *server,
                                         const UA_NodeId *id, int existing,
                                         cpkt_creation *creation) {
  cpkt_nodes_entry *previous;
  memset(creation, 0, sizeof(*creation));
  creation->owner = cpkt_nodes_owner_new(server);
  if (!creation->owner)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  creation->entry = (cpkt_nodes_entry *)UA_calloc(1, sizeof(*creation->entry));
  if (!creation->entry)
    return UA_STATUSCODE_BADOUTOFMEMORY;
  creation->entry->owner = creation->owner;
  creation->entry->references = 1;
  if (existing) {
    previous = cpkt_nodes_acquire(server->server, id);
    if (previous) {
      creation->entry->notifications = previous->notifications;
      creation->entry->lifecycle = previous->lifecycle;
      creation->entry->source = previous->source;
      creation->entry->method = previous->method;
      cpkt_nodes_release(previous);
    }
  }
  creation->id = id;
  creation->previous = creation->owner->creations;
  creation->owner->creations = creation;
  return 0;
}
static void cpkt_creation_end(cpkt_creation *creation, UA_NodeId *id) {
  void *context;
  if (!creation->entry)
    return;
  creation->owner->creations = creation->previous;
  /* Native failure can leave a callback-source node alive. Keep its dispatch
   * record in that case; do not silently change upstream rollback behavior.
   * Move the native ID, avoiding any post-creation metadata allocation. */
  if (!creation->superseded && !UA_NodeId_isNull(id) &&
      !UA_Server_getNodeContext(creation->owner->owner->server, *id,
                                &context)) {
    creation->entry->node = *id;
    UA_NodeId_init(id);
    cpkt_nodes_commit(creation->entry);
  } else
    cpkt_nodes_release(creation->entry);
  creation->entry = NULL;
}
static void cpkt_creation_rollback(cpkt_opcua_server *server,
                                   const UA_NodeId *id, int method) {
  if (method) {
    UA_BrowseDescription description;
    UA_BrowseResult result;
    size_t i;
    UA_BrowseDescription_init(&description);
    description.nodeId = *id;
    description.referenceTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY);
    description.browseDirection = UA_BROWSEDIRECTION_FORWARD;
    description.resultMask = UA_BROWSERESULTMASK_BROWSENAME;
    result = UA_Server_browse(server->server, 0, &description);
    /* Native method creation also creates its argument properties. Snapshot
     * their IDs before deleting them; do not mutate a live native iterator. */
    for (i = 0; i < result.referencesSize; ++i) {
      const UA_ReferenceDescription *reference = &result.references[i];
      const UA_String *name = &reference->browseName.name;
      if (!reference->browseName.namespaceIndex &&
          ((name->length == 14 && !memcmp(name->data, "InputArguments", 14)) ||
           (name->length == 15 && !memcmp(name->data, "OutputArguments", 15))))
        (void)UA_Server_deleteNode(server->server, reference->nodeId.nodeId,
                                   UA_TRUE);
    }
    UA_BrowseResult_clear(&result);
  }
  (void)UA_Server_deleteNode(server->server, *id, UA_TRUE);
}
