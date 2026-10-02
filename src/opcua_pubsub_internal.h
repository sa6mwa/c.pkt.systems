/* Private public-record dispatch; no native private types. */
typedef struct cpkt_pubsub_entry cpkt_pubsub_entry;
typedef UA_StatusCode (*cpkt_pubsub_state_fn)(cpkt_opcua_server *,
                                              cpkt_opcua_NodeId, void *,
                                              cpkt_opcua_PubSubState *,
                                              cpkt_opcua_PubSubState);
typedef struct cpkt_pubsub_owner cpkt_pubsub_owner;
struct cpkt_pubsub_entry {
  cpkt_pubsub_entry *next, *scoped_previous;
  cpkt_pubsub_owner *owner;
  UA_NodeId id;
  UA_PubSubComponentType type;
  cpkt_pubsub_state_fn state;
  UA_PubSubSecurityPolicy *policy;
  cpkt_opcua_PubSubSecurityPolicy *borrowed_policy;
  int scoped, committed, updating;
};
struct cpkt_pubsub_owner {
  cpkt_opcua_server *server;
  cpkt_pubsub_entry *entries, *scope;
  UA_PubSubConfiguration original;
  cpkt_opcua_PubSubConfiguration callbacks;
  unsigned int depth;
};
static cpkt_pubsub_entry *cpkt_pubsub_find(cpkt_opcua_server *,
                                           const UA_NodeId *);
static UA_StatusCode cpkt_pubsub_state(UA_Server *, const UA_NodeId, void *,
                                       UA_PubSubState *, UA_PubSubState);
static UA_StatusCode cpkt_pubsub_stage(cpkt_opcua_server *,
                                       UA_PubSubComponentType,
                                       const UA_NodeId *, cpkt_pubsub_state_fn,
                                       cpkt_pubsub_entry **);
static void cpkt_pubsub_finish(cpkt_pubsub_entry *, int, const UA_NodeId *);
static UA_StatusCode cpkt_pubsub_policy(cpkt_pubsub_entry *,
                                        cpkt_opcua_PubSubSecurityPolicy *,
                                        UA_PubSubSecurityPolicy **);
static UA_StatusCode cpkt_ps_publisher_native(const cpkt_opcua_PublisherId *,
                                              UA_PublisherId *);
static UA_StatusCode cpkt_ps_publisher_public(const UA_PublisherId *,
                                              cpkt_opcua_PublisherId *);
