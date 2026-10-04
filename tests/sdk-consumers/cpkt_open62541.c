#include <open62541/client.h>
#include <open62541/server.h>

int main(void) {
  UA_Client *client = UA_Client_new();
  UA_Server *server;

  if (client == 0) {
    return 1;
  }
  UA_Client_delete(client);

  server = UA_Server_new();
  if (server == 0) {
    return 2;
  }
  UA_Server_delete(server);
  return 0;
}
