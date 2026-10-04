#include <cpkt/opcua.h>
#include <cpkt/opcua_types.h>

#include <string.h>

struct cpkt_strict_browse_seen {
  int object_seen;
  int child_seen;
};

static int cpkt_strict_browse(const cpkt_opcua_browse_entry *entry,
                              void *user) {
  struct cpkt_strict_browse_seen *seen;

  seen = (struct cpkt_strict_browse_seen *)user;
  if (entry == 0 || seen == 0) {
    return 1;
  }
  if (entry->browse_name != 0 &&
      strcmp(entry->browse_name, "strictObject") == 0) {
    seen->object_seen = 1;
  }
  if (entry->browse_name != 0 &&
      strcmp(entry->browse_name, "strictChild") == 0) {
    seen->child_seen = 1;
  }
  return 0;
}

static cpkt_opcua_result cpkt_strict_method(const cpkt_opcua_value *inputs,
                                            size_t input_count,
                                            cpkt_opcua_value *output,
                                            void *user) {
  long factor;

  if (inputs == 0 || input_count != 1 || output == 0 || user == 0 ||
      inputs[0].type != CPKT_OPCUA_VALUE_INTEGER) {
    return CPKT_OPCUA_ERR_ARG;
  }
  factor = *(long *)user;
  cpkt_opcua_value_integer(output, inputs[0].integer_value * factor);
  return CPKT_OPCUA_OK;
}

static void
cpkt_strict_data_change(cpkt_opcua_subscription_id subscription_id,
                        cpkt_opcua_monitored_item_id monitored_item_id,
                        const cpkt_opcua_value *value, cpkt_opcua_status status,
                        void *user) {
  (void)subscription_id;
  (void)monitored_item_id;
  (void)value;
  (void)status;
  (void)user;
}

int main(void) {
  static const unsigned char json_config[] =
      "{ applicationDescription: { applicationUri: "
      "\"urn:cpkt:package:opcua-json\" } }";
  cpkt_opcua_server *server;
  cpkt_opcua_GlobalNodeLifecycle global_lifecycle;
  cpkt_opcua_NodeTypeLifecycle type_lifecycle;
  cpkt_opcua_ValueSourceNotifications notifications;
  cpkt_opcua_NodeId missing_node;
  cpkt_opcua_node_id node_id;
  cpkt_opcua_node_id object_id;
  cpkt_opcua_node_id child_id;
  cpkt_opcua_node_id method_id;
  cpkt_opcua_value value;
  cpkt_opcua_value out;
  cpkt_opcua_status status;
  cpkt_opcua_subscription_id subscription_id;
  cpkt_opcua_monitored_item_id monitored_item_id;
  struct cpkt_strict_browse_seen browse_seen;
  char endpoint[64];
  size_t required;
  int method_input_types[1];
  long method_factor;

  if (cpkt_opcua_open62541_version() == 0 || cpkt_opcua_facade_version() == 0) {
    return 1;
  }
  if (cpkt_opcua_server_new_from_json(&server, json_config,
                                      sizeof(json_config) - 1,
                                      &status) != CPKT_OPCUA_OK) {
    return 2;
  }
  if (cpkt_opcua_server_set_endpoint(server, "127.0.0.1", 4840) !=
      CPKT_OPCUA_OK) {
    cpkt_opcua_server_free(server);
    return 16;
  }
  memset(&global_lifecycle, 0, sizeof(global_lifecycle));
  memset(&type_lifecycle, 0, sizeof(type_lifecycle));
  memset(&notifications, 0, sizeof(notifications));
  cpkt_opcua_NodeId_init(&missing_node);
  missing_node.namespaceIndex = 1;
  missing_node.identifier.numeric = 99999;
  if (cpkt_opcua_server_set_global_node_lifecycle(server, &global_lifecycle) !=
          0 ||
      cpkt_opcua_server_set_global_node_lifecycle(server, 0) != 0 ||
      cpkt_opcua_server_setVariableNode_internalValueSource_typed(
          server, missing_node, 0, &notifications) !=
          CPKT_OPCUA_STATUSCODE_BADNODEIDUNKNOWN ||
      cpkt_opcua_server_setNodeTypeLifecycle_typed(server, missing_node,
                                                   type_lifecycle) !=
          CPKT_OPCUA_STATUSCODE_BADNODEIDUNKNOWN) {
    cpkt_opcua_server_free(server);
    return 51;
  }
  node_id = cpkt_opcua_node_id_numeric(1, 7001);
  object_id = cpkt_opcua_node_id_numeric(1, 7002);
  child_id = cpkt_opcua_node_id_numeric(1, 7003);
  method_id = cpkt_opcua_node_id_numeric(1, 7004);
  method_input_types[0] = CPKT_OPCUA_VALUE_INTEGER;
  method_factor = 2;
  cpkt_opcua_value_integer(&value, 11);
  if (cpkt_opcua_server_add_variable(server, node_id, "strictValue",
                                     "Strict Value", &value,
                                     &status) != CPKT_OPCUA_OK) {
    cpkt_opcua_server_free(server);
    return 3;
  }
  if (cpkt_opcua_server_add_object(
          server, object_id, cpkt_opcua_node_id_numeric(0, 85), "strictObject",
          "Strict Object", &status) != CPKT_OPCUA_OK) {
    cpkt_opcua_server_free(server);
    return 4;
  }
  if (cpkt_opcua_server_add_variable_under(server, child_id, object_id,
                                           "strictChild", "Strict Child",
                                           &value, &status) != CPKT_OPCUA_OK) {
    cpkt_opcua_server_free(server);
    return 5;
  }
  if (cpkt_opcua_server_add_method(server, method_id, object_id, "strictMethod",
                                   "Strict Method", method_input_types, 1,
                                   CPKT_OPCUA_VALUE_INTEGER, cpkt_strict_method,
                                   &method_factor, &status) != CPKT_OPCUA_OK) {
    cpkt_opcua_server_free(server);
    return 6;
  }
  browse_seen.object_seen = 0;
  browse_seen.child_seen = 0;
  if (cpkt_opcua_server_browse_children(
          server, cpkt_opcua_node_id_numeric(0, 85), cpkt_strict_browse,
          &browse_seen, &status) != CPKT_OPCUA_OK ||
      browse_seen.object_seen == 0) {
    cpkt_opcua_server_free(server);
    return 7;
  }
  browse_seen.object_seen = 0;
  browse_seen.child_seen = 0;
  if (cpkt_opcua_server_browse_children(server, object_id, cpkt_strict_browse,
                                        &browse_seen,
                                        &status) != CPKT_OPCUA_OK ||
      browse_seen.child_seen == 0) {
    cpkt_opcua_server_free(server);
    return 8;
  }
  cpkt_opcua_value_integer(&value, 12);
  if (cpkt_opcua_server_write(server, node_id, &value, &status) !=
      CPKT_OPCUA_OK) {
    cpkt_opcua_server_free(server);
    return 9;
  }
  if (cpkt_opcua_server_read(server, node_id, &out, 0, 0, 0, &status) !=
      CPKT_OPCUA_OK) {
    cpkt_opcua_server_free(server);
    return 10;
  }
  if (out.type != CPKT_OPCUA_VALUE_INTEGER || out.integer_value != 12) {
    cpkt_opcua_server_free(server);
    return 11;
  }
  if (cpkt_opcua_server_endpoint_url(server, endpoint, sizeof(endpoint),
                                     &required) != CPKT_OPCUA_OK) {
    cpkt_opcua_server_free(server);
    return 12;
  }
  if (required == 0 || strstr(endpoint, "opc.tcp://127.0.0.1:") != endpoint) {
    cpkt_opcua_server_free(server);
    return 13;
  }
  if (cpkt_opcua_client_create_subscription(0, 1.0, &subscription_id,
                                            &status) != CPKT_OPCUA_ERR_ARG) {
    cpkt_opcua_server_free(server);
    return 14;
  }
  if (cpkt_opcua_client_monitor_value(
          0, 1, node_id, 1.0, cpkt_strict_data_change, 0, &monitored_item_id,
          &status) != CPKT_OPCUA_ERR_ARG) {
    cpkt_opcua_server_free(server);
    return 15;
  }
  cpkt_opcua_server_free(server);
  return 0;
}
