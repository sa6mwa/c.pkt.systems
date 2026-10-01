#include "opcua_callbacks_test.h"
#include "opcua_types_peer.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void check(int ok, const char *text, int line) {
  if (!ok) {
    fprintf(stderr, "core client line %d: %s\n", line, text);
    abort();
  }
}
#define CHECK(x) check(!!(x), #x, __LINE__)
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
void cpkt_types_fail_after(size_t count);
int cpkt_types_fail_stop(void);
#endif
static cpkt_opcua_String string(const char *text) {
  cpkt_opcua_String result;
  result.data = (cpkt_opcua_Byte *)text;
  result.length = strlen(text);
  return result;
}
static cpkt_opcua_status native_state(void *client, void *user) {
  cpkt_types_peer_state(client, user);
  return 0;
}
struct transfer_config {
  void *donor;
  const char *url;
};
static cpkt_opcua_status copy_session_endpoint(void *client, void *user) {
  struct transfer_config *config = user;
  return cpkt_types_peer_session_endpoint(client, config->donor, config->url);
}
static cpkt_opcua_status retain_native_client(void *client, void *user) {
  struct transfer_config *config = user;
  config->donor = client;
  return 0;
}
static void state_parity(cpkt_opcua_client *client) {
  unsigned int native[3];
  cpkt_opcua_SecureChannelState channel;
  cpkt_opcua_SessionState session;
  cpkt_opcua_StatusCode status;
  CHECK(cpkt_opcua_client_native(client, native_state, native) ==
        CPKT_OPCUA_OK);
  CHECK(!cpkt_opcua_client_getState_typed(client, &channel, &session, &status));
  CHECK((unsigned int)channel == native[0] &&
        (unsigned int)session == native[1] && status == native[2]);
  CHECK(!cpkt_opcua_client_getState_typed(client, NULL, NULL, NULL));
}
static void wait_state(cpkt_opcua_client *client,
                       cpkt_opcua_SecureChannelState expected_channel,
                       cpkt_opcua_SessionState expected_session) {
  size_t i;
  cpkt_opcua_SecureChannelState channel;
  cpkt_opcua_SessionState session;
  cpkt_opcua_StatusCode status;
  for (i = 0; i < 400; ++i) {
    CHECK(
        !cpkt_opcua_client_getState_typed(client, &channel, &session, &status));
    CHECK(!status || (expected_session == CPKT_OPCUA_SESSIONSTATE_CLOSED &&
                      status == CPKT_OPCUA_STATUSCODE_BADCONNECTIONCLOSED));
    if (channel == expected_channel && session == expected_session)
      break;
    status = cpkt_opcua_client_run_iterate_typed(client, 10);
    CHECK(!status || (expected_session == CPKT_OPCUA_SESSIONSTATE_CLOSED &&
                      status == CPKT_OPCUA_STATUSCODE_BADCONNECTIONCLOSED));
  }
  CHECK(i < 400);
  state_parity(client);
}
struct discovery {
  const char *url;
  int kind;
  unsigned char *bytes;
  size_t length, count;
  cpkt_opcua_StatusCode status;
};
static cpkt_opcua_status native_discovery(void *client, void *user) {
  struct discovery *result = user;
  result->status = cpkt_types_peer_discovery(client, result->url, result->kind,
                                             &result->bytes, &result->length,
                                             &result->count);
  return 0;
}
static void discovery(cpkt_opcua_client *client, const char *url) {
  struct discovery native;
  cpkt_opcua_String missing = string("missing-uri"), locale = string("en");
  cpkt_opcua_EndpointDescription *endpoints;
  cpkt_opcua_ApplicationDescription *servers;
  cpkt_opcua_ServerOnNetwork *network;
  cpkt_opcua_Variant value;
  cpkt_opcua_ByteString bytes;
  cpkt_opcua_StatusCode status;
  size_t count, i;
  int kind;
  for (kind = 0; kind < 5; ++kind) {
    memset(&native, 0, sizeof(native));
    native.url = url;
    native.kind = kind;
    CHECK(cpkt_opcua_client_native(client, native_discovery, &native) ==
          CPKT_OPCUA_OK);
    count = 0;
    endpoints = NULL;
    servers = NULL;
    network = NULL;
    cpkt_opcua_Variant_init(&value);
    if (kind == 0) {
      status =
          cpkt_opcua_client_getEndpoints_typed(client, url, &count, &endpoints);
      value.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_ENDPOINTDESCRIPTION);
      value.data = endpoints;
      if (!status) {
        CHECK(count);
        for (i = 0; i < count; ++i) {
          CHECK(endpoints[i].endpointUrl.length &&
                endpoints[i].server.applicationUri.length &&
                endpoints[i].server.productUri.length &&
                endpoints[i].server.applicationName.text.length &&
                endpoints[i].server.discoveryUrlsSize &&
                endpoints[i].securityPolicyUri.length &&
                endpoints[i].transportProfileUri.length &&
                endpoints[i].userIdentityTokensSize);
        }
      }
    } else if (kind == 1 || kind == 3) {
      status = cpkt_opcua_client_findServers_typed(
          client, url, kind == 3 ? 1 : 0, kind == 3 ? &missing : NULL,
          kind == 3 ? 1 : 0, kind == 3 ? &locale : NULL, &count, &servers);
      value.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_APPLICATIONDESCRIPTION);
      value.data = servers;
      CHECK(status || (kind == 3 ? count == 0 : count > 0));
    } else {
      status = cpkt_opcua_client_findServersOnNetwork_typed(
          client, url, kind == 4 ? 1 : 0, kind == 4 ? 1 : 0, kind == 4 ? 1 : 0,
          kind == 4 ? &missing : NULL, &count, &network);
      value.type = cpkt_opcua_type_at(CPKT_OPCUA_TYPES_SERVERONNETWORK);
      value.data = network;
    }
    CHECK(status == native.status && count == native.count);
    if (!status) {
      value.arrayLength = count;
      value.storageType = CPKT_OPCUA_VARIANT_DATA_NODELETE;
      cpkt_opcua_ByteString_init(&bytes);
      CHECK(!cpkt_opcua_type_encode_binary(
          &value, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_VARIANT), &bytes));
      CHECK(bytes.length == native.length &&
            !memcmp(bytes.data, native.bytes, bytes.length));
      cpkt_opcua_ByteString_clear(&bytes);
    }
    cpkt_types_native_free(native.bytes);
    cpkt_opcua_array_delete(value.data, count, value.type);
  }
  CHECK(cpkt_opcua_client_findServers_typed(client, url, (size_t)-1, &missing,
                                            0, NULL, &count, &servers) ==
        CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY);
  CHECK(!count && !servers);
  CHECK(cpkt_opcua_client_findServersOnNetwork_typed(
            client, url, 0, 0, (size_t)-1, &missing, &count, &network) ==
        CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY);
  CHECK(!count && !network);
  CHECK(cpkt_opcua_client_getEndpoints_typed(client, url, NULL, &endpoints) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  for (i = 0; i < 128; ++i) {
    int injected;
    cpkt_types_peer_client_fail_conversion(8, i);
    status =
        cpkt_opcua_client_getEndpoints_typed(client, url, &count, &endpoints);
    injected = cpkt_types_fail_stop();
    if (injected) {
      CHECK(status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY && !count &&
            !endpoints);
      continue;
    }
    CHECK(!status && count && endpoints);
    cpkt_opcua_array_delete(
        endpoints, count,
        cpkt_opcua_type_at(CPKT_OPCUA_TYPES_ENDPOINTDESCRIPTION));
    break;
  }
  CHECK(i > 0 && i < 128);
#endif
}
static void namespaces(cpkt_opcua_client *client) {
  cpkt_opcua_String uri, output;
  cpkt_opcua_UInt16 index = 777, again = 999;
  cpkt_opcua_StatusCode status;
  static const unsigned char text[] = {'t', 'y', 'p', 'e', 'd', 0, 'n', 's'};
  CHECK(cpkt_opcua_client_getNamespaceIndex_typed(client, string("missing-uri"),
                                                  &index) ==
        CPKT_OPCUA_STATUSCODE_BADNOTFOUND);
  CHECK(index == 777);
  CHECK(!cpkt_opcua_client_getNamespaceUri_typed(client, 0, &output));
  CHECK(output.length == strlen("http://opcfoundation.org/UA/") &&
        !memcmp(output.data, "http://opcfoundation.org/UA/", output.length));
  cpkt_opcua_String_clear(&output);
  uri.data = (cpkt_opcua_Byte *)text;
  uri.length = sizeof(text);
  CHECK(!cpkt_opcua_client_addNamespace_typed(client, uri, &index));
  CHECK(index > 0);
  CHECK(!cpkt_opcua_client_addNamespace_typed(client, uri, &again) &&
        again == index);
  CHECK(!cpkt_opcua_client_getNamespaceIndex_typed(client, uri, &again) &&
        again == index);
  CHECK(!cpkt_opcua_client_getNamespaceUri_typed(client, index, &output));
  CHECK(output.length == sizeof(text) && output.data != uri.data &&
        !memcmp(output.data, text, sizeof(text)));
  cpkt_opcua_String_clear(&output);
  CHECK(cpkt_opcua_client_getNamespaceUri_typed(client, 65535, &output) ==
            CPKT_OPCUA_STATUSCODE_BADNOTFOUND &&
        !output.data && !output.length);
  CHECK(cpkt_opcua_client_addNamespace_typed(client, uri, NULL) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  {
    size_t i;
    for (i = 0; i < 8; ++i) {
      int injected;
      cpkt_types_peer_client_fail_conversion(9, i);
      status = cpkt_opcua_client_getNamespaceUri_typed(client, index, &output);
      injected = cpkt_types_fail_stop();
      if (injected) {
        CHECK(status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY && !output.data &&
              !output.length);
        continue;
      }
      CHECK(!status && output.length == sizeof(text));
      cpkt_opcua_String_clear(&output);
      break;
    }
    CHECK(i > 0 && i < 8);
  }
#else
  (void)status;
#endif
}
struct timer {
  cpkt_opcua_client *client;
  cpkt_opcua_UInt64 id, other;
  unsigned int calls;
  int mode;
};
static void timer(cpkt_opcua_client *client, void *user) {
  struct timer *state = user;
  CHECK(state && client == state->client);
  ++state->calls;
  if (state->mode == 1 && state->calls == 2)
    cpkt_opcua_client_removeCallback_typed(client, state->id);
  if (state->mode == 2) {
    if (state->calls == 1)
      CHECK(!cpkt_opcua_client_changeRepeatedCallbackInterval_typed(
          client, state->id, 1));
    else
      cpkt_opcua_client_removeCallback_typed(client, state->id);
  }
  if (state->mode == 3)
    cpkt_opcua_client_removeCallback_typed(client, state->other);
  if (state->mode == 4) {
    cpkt_opcua_client_removeCallback_typed(client, state->id);
    CHECK(!raise(SIGINT));
  }
}
struct external {
  void *loop;
  unsigned int independent;
};
static cpkt_opcua_status external_loop(void *client, void *user) {
  struct external *state = user;
  state->loop = cpkt_types_peer_external_loop(client, &state->independent);
  return 0;
}
static void timers(const char *url) {
  cpkt_opcua_client *client;
  struct timer state, other;
  cpkt_opcua_DateTime date = {0, 0};
  cpkt_opcua_UInt64 wrong;
  size_t i;
  unsigned int native_calls;
  struct external external;
  CHECK(cpkt_opcua_client_new(&client) == CPKT_OPCUA_OK);
  memset(&state, 0, sizeof(state));
  state.client = client;
  memset(&other, 0, sizeof(other));
  other.client = client;
  CHECK(cpkt_opcua_client_addTimedCallback_typed(NULL, timer, &state, date,
                                                 &state.id) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(!state.id.high32 && !state.id.low32);
  CHECK(cpkt_opcua_client_addTimedCallback_typed(client, NULL, &state, date,
                                                 &state.id) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  CHECK(cpkt_opcua_client_addRepeatedCallback_typed(client, timer, &state, 0,
                                                    &state.id) != 0);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
  for (i = 0; i < 2; ++i) {
    cpkt_types_fail_after(i);
    CHECK(cpkt_opcua_client_addTimedCallback_typed(client, timer, &state, date,
                                                   &state.id) ==
          CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY);
    CHECK(cpkt_types_fail_stop() && !state.id.high32 && !state.id.low32);
  }
#endif
  CHECK(!cpkt_opcua_client_addTimedCallback_typed(client, timer, &state, date,
                                                  &state.id));
  wrong = state.id;
  wrong.high32 = 0x80000000U;
  CHECK(cpkt_opcua_client_changeRepeatedCallbackInterval_typed(client, wrong,
                                                               1) != 0);
  cpkt_opcua_client_removeCallback_typed(client, wrong);
  CHECK(!cpkt_opcua_client_run_iterate_typed(client, 0) && state.calls == 1);
  CHECK(!cpkt_opcua_client_run_iterate_typed(client, 0) && state.calls == 1);
  state.calls = 0;
  state.mode = 1;
  CHECK(!cpkt_opcua_client_addRepeatedCallback_typed(client, timer, &state, 1,
                                                     &state.id));
  CHECK(!cpkt_opcua_client_changeRepeatedCallbackInterval_typed(client,
                                                                state.id, 2));
  for (i = 0; i < 100 && state.calls < 2; ++i)
    CHECK(!cpkt_opcua_client_run_iterate_typed(client, 2));
  CHECK(state.calls == 2);
  for (i = 0; i < 3; ++i)
    CHECK(!cpkt_opcua_client_run_iterate_typed(client, 2));
  CHECK(state.calls == 2);
  state.calls = 0;
  state.mode = 2;
  CHECK(!cpkt_opcua_client_addTimedCallback_typed(client, timer, &state, date,
                                                  &state.id));
  for (i = 0; i < 100 && state.calls < 2; ++i)
    CHECK(!cpkt_opcua_client_run_iterate_typed(client, 2));
  CHECK(state.calls == 2);
  state.calls = 0;
  state.mode = 3;
  CHECK(!cpkt_opcua_client_addTimedCallback_typed(client, timer, &state, date,
                                                  &state.id));
  /* Equal native deadlines have no insertion-order guarantee. */
  date.low32 = 1;
  CHECK(!cpkt_opcua_client_addTimedCallback_typed(client, timer, &other, date,
                                                  &state.other));
  date.low32 = 0;
  CHECK(!cpkt_opcua_client_run_iterate_typed(client, 0) && state.calls == 1 &&
        !other.calls);
  state.calls = 0;
  state.mode = 0;
  CHECK(!cpkt_opcua_client_addTimedCallback_typed(client, timer, &state, date,
                                                  NULL));
  CHECK(!cpkt_opcua_client_run_iterate_typed(client, 0) && state.calls == 1);
  CHECK(!cpkt_opcua_client_connect_typed(client, url));
#ifdef __APPLE__
  /* The already-running peer owns the stock self-pipe singleton. The facade
   * must preserve the native error when this client has no interrupt source. */
  CHECK(cpkt_opcua_client_runUntilInterrupt_typed(client) ==
        CPKT_OPCUA_STATUSCODE_BADINTERNALERROR);
#endif
  state.calls = 0;
  state.mode = 0;
  CHECK(!cpkt_opcua_client_addTimedCallback_typed(client, timer, &state, date,
                                                  NULL));
  cpkt_opcua_client_free(client);
  CHECK(!cpkt_types_peer_delete_timer(url, &native_calls));
  CHECK(state.calls == native_calls);
  CHECK(cpkt_opcua_client_new(&client) == CPKT_OPCUA_OK);
  memset(&external, 0, sizeof(external));
  CHECK(cpkt_opcua_client_native(client, external_loop, &external) ==
        CPKT_OPCUA_OK);
  memset(&state, 0, sizeof(state));
  state.client = client;
  date.high32 = 0x7fffffffU;
  date.low32 = 0xffffffffU;
  CHECK(!cpkt_opcua_client_addTimedCallback_typed(client, timer, &state, date,
                                                  NULL));
  CHECK(!cpkt_opcua_client_addRepeatedCallback_typed(client, timer, &state, 1,
                                                     NULL));
  cpkt_opcua_client_free(client);
  native_calls = state.calls;
  cpkt_types_peer_external_loop_free(external.loop);
  CHECK(state.calls == native_calls && external.independent == 1);
  cpkt_opcua_client_removeCallback_typed(NULL, wrong);
}
void cpkt_types_test_core_client_interrupt(void) {
  cpkt_opcua_client *client;
  struct timer state;
  void *peer;
  unsigned short port;
  char endpoint[64];
  size_t i;
  /* Allocate the client first: non-epoll POSIX permits one stock interrupt
   * manager per process. The peer's ordinary iterate loop needs no manager. */
  CHECK(cpkt_opcua_client_new(&client) == CPKT_OPCUA_OK);
  peer = cpkt_types_peer_start(&port);
  CHECK(peer);
  sprintf(endpoint, "opc.tcp://127.0.0.1:%u", (unsigned int)port);
  CHECK(!cpkt_opcua_client_connect_typed(client, endpoint));
  memset(&state, 0, sizeof(state));
  state.client = client;
  state.mode = 4;
  for (i = 0; i < 2; ++i) {
    state.calls = 0;
    CHECK(!cpkt_opcua_client_addRepeatedCallback_typed(client, timer, &state, 1,
                                                       &state.id));
    CHECK(!cpkt_opcua_client_runUntilInterrupt_typed(client) &&
          state.calls == 1);
  }
  cpkt_opcua_client_free(client);
  cpkt_types_peer_stop(peer);
}
static void sessions(const char *url) {
  struct transfer_config config;
  cpkt_opcua_client *donor, *recipient;
  cpkt_opcua_NodeId token, node;
  cpkt_opcua_ByteString nonce;
  cpkt_opcua_Variant value;
  cpkt_opcua_StatusCode status;
  size_t i;
  int asynchronous;
  config.url = url;
  for (asynchronous = 0; asynchronous < 2; ++asynchronous) {
    CHECK(cpkt_opcua_client_new(&donor) == CPKT_OPCUA_OK);
    CHECK(cpkt_opcua_client_new(&recipient) == CPKT_OPCUA_OK);
    CHECK(cpkt_opcua_client_getSessionAuthenticationToken_typed(
              donor, &token, &nonce) == CPKT_OPCUA_STATUSCODE_BADSESSIONCLOSED);
    CHECK(!token.identifier.numeric && !nonce.length && !nonce.data);
    CHECK(!cpkt_opcua_client_connect_typed(donor, url));
    CHECK(!cpkt_opcua_client_getSessionAuthenticationToken_typed(donor, &token,
                                                                 &nonce));
    CHECK(token.identifierType == CPKT_OPCUA_NODEIDTYPE_GUID && nonce.length);
    CHECK(!cpkt_opcua_client_activateCurrentSession_typed(donor));
    CHECK(!cpkt_opcua_client_activateCurrentSessionAsync_typed(donor));
    wait_state(donor, CPKT_OPCUA_SECURECHANNELSTATE_OPEN,
               CPKT_OPCUA_SESSIONSTATE_ACTIVATED);
    cpkt_opcua_NodeId_clear(&token);
    cpkt_opcua_ByteString_clear(&nonce);
    CHECK(!cpkt_opcua_client_getSessionAuthenticationToken_typed(donor, &token,
                                                                 &nonce));
    /* SecureChannel-only async connect does not discover an endpoint. Native
     * activation requires the original endpoint's user-token policies. Prepare
     * that public configuration explicitly, without adding discovery to the
     * facade's activateSession wrapper. Both clients use anonymous identity. */
    CHECK(cpkt_opcua_client_native(donor, retain_native_client, &config) ==
          CPKT_OPCUA_OK);
    CHECK(cpkt_opcua_client_native(recipient, copy_session_endpoint, &config) ==
          CPKT_OPCUA_OK);
    if (asynchronous) {
      CHECK(!cpkt_opcua_client_disconnectSecureChannelAsync_typed(donor));
      CHECK(!cpkt_opcua_client_connectSecureChannelAsync_typed(recipient, url));
    } else {
      CHECK(!cpkt_opcua_client_disconnectSecureChannel_typed(donor));
      CHECK(!cpkt_opcua_client_connectSecureChannel_typed(recipient, url));
    }
    wait_state(recipient, CPKT_OPCUA_SECURECHANNELSTATE_OPEN,
               CPKT_OPCUA_SESSIONSTATE_CLOSED);
    if (asynchronous)
      CHECK(!cpkt_opcua_client_activateSessionAsync_typed(recipient, token,
                                                          nonce));
    else
      CHECK(!cpkt_opcua_client_activateSession_typed(recipient, token, nonce));
    /* Native switchSession has copied the credentials before returning. */
    cpkt_opcua_NodeId_clear(&token);
    cpkt_opcua_ByteString_clear(&nonce);
    wait_state(recipient, CPKT_OPCUA_SECURECHANNELSTATE_OPEN,
               CPKT_OPCUA_SESSIONSTATE_ACTIVATED);
    cpkt_opcua_NodeId_init(&node);
    node.namespaceIndex = 1;
    node.identifier.numeric = 6001;
    CHECK(
        !cpkt_opcua_client_readValueAttribute_typed(recipient, node, &value) &&
        value.arrayLength == 4);
    cpkt_opcua_Variant_clear(&value);
#ifdef CPKT_OPCUA_TYPES_ALLOC_FAILURE
    for (i = 0; i < 8; ++i) {
      int injected;
      cpkt_types_peer_client_fail_conversion(7, i);
      status = cpkt_opcua_client_getSessionAuthenticationToken_typed(
          recipient, &token, &nonce);
      injected = cpkt_types_fail_stop();
      if (injected) {
        CHECK(status == CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY &&
              !token.identifier.numeric && !nonce.length && !nonce.data);
        continue;
      }
      CHECK(!status && nonce.length);
      cpkt_opcua_NodeId_clear(&token);
      cpkt_opcua_ByteString_clear(&nonce);
      break;
    }
    CHECK(i > 0 && i < 8);
#else
    (void)i;
    (void)status;
#endif
    CHECK(cpkt_opcua_client_getSessionAuthenticationToken_typed(recipient, NULL,
                                                                &nonce) ==
          CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
    CHECK(cpkt_opcua_client_activateSession_typed(recipient, node, nonce) ==
          CPKT_OPCUA_STATUSCODE_BADINTERNALERROR);
    if (asynchronous) {
      CHECK(!cpkt_opcua_client_disconnectAsync_typed(recipient));
      wait_state(recipient, CPKT_OPCUA_SECURECHANNELSTATE_CLOSED,
                 CPKT_OPCUA_SESSIONSTATE_CLOSED);
    } else
      CHECK(!cpkt_opcua_client_disconnect_typed(recipient));
    cpkt_opcua_client_free(donor);
    cpkt_opcua_client_free(recipient);
  }
}
void cpkt_types_test_core_client(cpkt_opcua_client *connected, void *peer,
                                 const char *url) {
  cpkt_opcua_client *client;
  cpkt_opcua_SecureChannelState channel;
  cpkt_opcua_SessionState session;
  cpkt_opcua_String host;
  unsigned short port;
  cpkt_opcua_StatusCode status;
  CHECK(cpkt_opcua_client_getState_typed(NULL, &channel, &session, &status) ==
        CPKT_OPCUA_STATUSCODE_BADINVALIDARGUMENT);
  state_parity(connected);
  namespaces(connected);
  discovery(connected, url);
  CHECK(cpkt_opcua_client_new(&client) == CPKT_OPCUA_OK);
  state_parity(client);
  CHECK(!cpkt_opcua_client_connectAsync_typed(client, url));
  wait_state(client, CPKT_OPCUA_SECURECHANNELSTATE_OPEN,
             CPKT_OPCUA_SESSIONSTATE_ACTIVATED);
  CHECK(!cpkt_opcua_client_disconnect_typed(client));
  CHECK(!cpkt_opcua_client_connect_typed(client, NULL));
  wait_state(client, CPKT_OPCUA_SECURECHANNELSTATE_OPEN,
             CPKT_OPCUA_SESSIONSTATE_ACTIVATED);
  cpkt_opcua_client_free(client);
  CHECK(cpkt_opcua_client_new(&client) == CPKT_OPCUA_OK);
  status = cpkt_types_peer_username_status(url);
  CHECK(status && cpkt_opcua_client_connectUsername_typed(
                      client, url, "user", "password") == status);
  cpkt_opcua_client_free(client);
  sessions(url);
  timers(url);
  CHECK(cpkt_opcua_client_new(&client) == CPKT_OPCUA_OK);
  host = string("127.0.0.1");
  port = cpkt_types_peer_port();
  CHECK(cpkt_opcua_client_startListeningForReverseConnect_typed(
            client, &host, (size_t)-1, port) ==
        CPKT_OPCUA_STATUSCODE_BADOUTOFMEMORY);
  CHECK(!cpkt_opcua_client_startListeningForReverseConnect_typed(client, &host,
                                                                 1, port));
  CHECK(!cpkt_opcua_client_run_iterate_typed(client, 0));
  CHECK(!cpkt_types_peer_reverse(peer, port, 0));
  wait_state(client, CPKT_OPCUA_SECURECHANNELSTATE_OPEN,
             CPKT_OPCUA_SESSIONSTATE_ACTIVATED);
  CHECK(!cpkt_types_peer_reverse(peer, port, 1));
  cpkt_opcua_client_free(client);
}
