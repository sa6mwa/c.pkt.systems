#include <cpkt/sasl.h>

#include <string.h>

int main(void) {
  const char *implementation;
  const char *version;
  const char *message;
  const char *mechanisms;
  cpkt_sasl *client;
  cpkt_sasl_security_properties properties;
  unsigned long mechanisms_length;
  int mechanisms_count;
  int major;
  int minor;
  int step;
  int patch;
  int status;

  implementation = 0;
  version = 0;
  major = 0;
  minor = 0;
  step = 0;
  patch = 0;
  cpkt_sasl_version(&implementation, &version, &major, &minor, &step, &patch);
  if (implementation == 0 || version == 0 || major < 2)
    return 1;
  message = cpkt_sasl_error_string(CPKT_SASL_BADPARAM, 0, 0);
  if (message == 0 || strlen(message) == 0U)
    return 2;
  if (cpkt_sasl_set_path(CPKT_SASL_PATH_PLUGIN,
                         "/cpkt-no-external-sasl-plugins") != CPKT_SASL_OK)
    return 10;
  if (cpkt_sasl_client_initialize(0) != CPKT_SASL_OK)
    return 3;
  status = CPKT_SASL_FAIL;
  client =
      cpkt_sasl_client_new("imap", "mail.example.test", 0, 0, 0, 0, &status);
  if (client == 0 || status != CPKT_SASL_OK)
    return 4;
  mechanisms = 0;
  mechanisms_length = 0;
  mechanisms_count = 0;
  if (client->list_mechanisms(client, 0, 0, " ", 0, &mechanisms,
                              &mechanisms_length,
                              &mechanisms_count) != CPKT_SASL_OK ||
      mechanisms == 0 || strstr(mechanisms, "GSSAPI") == 0 ||
      mechanisms_count < 1 || mechanisms_length == 0) {
    client->close(client);
    return 11;
  }
  if (client->server_start(client, "PLAIN", 0, 0, 0, 0) != CPKT_SASL_BADPARAM) {
    client->close(client);
    return 5;
  }
  memset(&properties, 0, sizeof(properties));
  properties.minimum_ssf = 0;
  properties.maximum_ssf = 256;
  if (client->set_security_properties(client, &properties) != CPKT_SASL_OK) {
    client->close(client);
    return 6;
  }
  if (client->set_external_ssf(client, 0) != CPKT_SASL_OK) {
    client->close(client);
    return 7;
  }
  if (client->error_detail(client) == 0) {
    client->close(client);
    return 8;
  }
  client->close(client);
  if (cpkt_sasl_client_finish() != CPKT_SASL_OK)
    return 9;
  return 0;
}
