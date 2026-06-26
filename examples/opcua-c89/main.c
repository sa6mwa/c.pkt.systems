#include <cpkt/opcua.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CPKT_OPCUA_OBJECTS_FOLDER 85UL

struct browse_state {
  const char *expected_name;
  int matched;
};

static cpkt_opcua_result multiply_method(
    const cpkt_opcua_value *inputs,
    size_t input_count,
    cpkt_opcua_value *output,
    void *user) {
  long factor;

  if (inputs == NULL || input_count != 1 || output == NULL || user == NULL ||
      inputs[0].type != CPKT_OPCUA_VALUE_INTEGER) {
    return CPKT_OPCUA_ERR_ARG;
  }
  factor = *(long *)user;
  cpkt_opcua_value_integer(output, inputs[0].integer_value * factor);
  return CPKT_OPCUA_OK;
}

static int browse_for_name(const cpkt_opcua_browse_entry *entry, void *user) {
  struct browse_state *state;

  state = (struct browse_state *)user;
  if (entry == NULL || state == NULL) {
    return 1;
  }
  if (entry->browse_name != NULL && strcmp(entry->browse_name, state->expected_name) == 0) {
    state->matched = 1;
  }
  return 0;
}

static int expect_ok(cpkt_opcua_result result, const char *operation) {
  if (result != CPKT_OPCUA_OK) {
    fprintf(stderr, "%s failed: %s\n", operation, cpkt_opcua_result_string(result));
    return 1;
  }
  return 0;
}

int main(void) {
  cpkt_opcua_server *server;
  cpkt_opcua_client *client;
  cpkt_opcua_node_id objects_folder;
  cpkt_opcua_node_id value_node;
  cpkt_opcua_node_id object_node;
  cpkt_opcua_node_id child_node;
  cpkt_opcua_node_id method_node;
  cpkt_opcua_value value;
  cpkt_opcua_value out;
  cpkt_opcua_status status;
  struct browse_state state;
  char endpoint[128];
  size_t required;
  int method_input_types[1];
  long method_factor;
  int failed;

  server = NULL;
  client = NULL;
  objects_folder = cpkt_opcua_node_id_numeric(0, CPKT_OPCUA_OBJECTS_FOLDER);
  value_node = cpkt_opcua_node_id_numeric(1, 7101);
  object_node = cpkt_opcua_node_id_numeric(1, 7102);
  child_node = cpkt_opcua_node_id_numeric(1, 7103);
  method_node = cpkt_opcua_node_id_numeric(1, 7104);
  required = 0;
  method_input_types[0] = CPKT_OPCUA_VALUE_INTEGER;
  method_factor = 2;
  failed = 0;

  if (expect_ok(cpkt_opcua_server_new(&server, 4840), "server new")) {
    return 1;
  }
  if (expect_ok(cpkt_opcua_client_new(&client), "client new")) {
    cpkt_opcua_server_free(server);
    return 1;
  }

  cpkt_opcua_value_integer(&value, 41);
  failed = expect_ok(
      cpkt_opcua_server_add_variable(server, value_node, "exampleValue", "Example Value", &value, &status),
      "add variable");

  if (!failed) {
    failed = expect_ok(
        cpkt_opcua_server_add_object(
            server,
            object_node,
            objects_folder,
            "exampleObject",
            "Example Object",
            &status),
        "add object");
  }

  if (!failed) {
    cpkt_opcua_value_string(&value, "ready", 5);
    failed = expect_ok(
        cpkt_opcua_server_add_variable_under(
            server,
            child_node,
            object_node,
            "exampleChild",
            "Example Child",
            &value,
            &status),
        "add child variable");
  }

  if (!failed) {
    failed = expect_ok(
        cpkt_opcua_server_add_method(
            server,
            method_node,
            object_node,
            "exampleMultiply",
            "Example Multiply",
            method_input_types,
            1,
            CPKT_OPCUA_VALUE_INTEGER,
            multiply_method,
            &method_factor,
            &status),
        "add method");
  }

  if (!failed) {
    cpkt_opcua_value_integer(&value, 42);
    failed = expect_ok(cpkt_opcua_server_write(server, value_node, &value, &status), "write variable");
  }

  if (!failed) {
    failed = expect_ok(
        cpkt_opcua_server_read(server, value_node, &out, NULL, 0, NULL, &status),
        "read variable");
    if (!failed && (out.type != CPKT_OPCUA_VALUE_INTEGER || out.integer_value != 42)) {
      fprintf(stderr, "read variable returned an unexpected value\n");
      failed = 1;
    }
  }

  if (!failed) {
    state.expected_name = "exampleObject";
    state.matched = 0;
    failed = expect_ok(
        cpkt_opcua_server_browse_children(server, objects_folder, browse_for_name, &state, &status),
        "browse objects");
    if (!failed && !state.matched) {
      fprintf(stderr, "browse did not find exampleObject\n");
      failed = 1;
    }
  }

  if (!failed) {
    state.expected_name = "exampleChild";
    state.matched = 0;
    failed = expect_ok(
        cpkt_opcua_server_browse_children(server, object_node, browse_for_name, &state, &status),
        "browse object");
    if (!failed && !state.matched) {
      fprintf(stderr, "browse did not find exampleChild\n");
      failed = 1;
    }
  }

  if (!failed) {
    failed = expect_ok(
        cpkt_opcua_server_endpoint_url(server, endpoint, sizeof(endpoint), &required),
        "endpoint url");
    if (!failed && required == 0) {
      fprintf(stderr, "endpoint url was empty\n");
      failed = 1;
    }
  }

  cpkt_opcua_client_free(client);
  cpkt_opcua_server_free(server);
  return failed ? 1 : 0;
}
